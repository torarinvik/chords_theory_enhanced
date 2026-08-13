#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/InputMidiNoteTracker.h"
#include "Audio/ProgressionPlayer.h"
#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/MidiEditorState.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/Scale.h"

namespace component
{

// A piano-roll MIDI note editor: a scrollable/zoomable pitch grid (pitch-labeled gutter on the
// left, beat/bar ruler on top), a "chord lane" strip under the grid, and a horizontal key strip
// under that. The bottom strip is a real multi-octave piano (white keys + overlapping black keys,
// letter labels, Chordz-style); chord tones under the playhead highlight in PRIMARY blue, and any
// scale attached to that chord highlights in the user-configurable scale colour. Dropping a
// ChordCard's exported .mid file onto it (see AppLayout's in-flight-drag-map resolution) adds a
// labeled chord block to the lane and splits the chord into individually movable/resizable note
// blocks in the grid above, via addChordAtBeat(). Dragging a scale suggestion (internal DND
// "chordsTheoryScale|…") onto a chord chip attaches that scale to the chord. Owns its own
// in-memory note/chord-block state - getState()/restoreState() bridge that to/from
// theory::MidiEditorState.
//
// Hand-paints everything itself (no juce::Viewport) - there's no existing precedent in this
// codebase or nierika_dsp for a Viewport scrolling on both axes with a frozen gutter/ruler synced
// against it, and hand-painting sidesteps that entirely: the gutter/ruler are simply drawn last, at
// screen-fixed rects regardless of scroll offset. Two real juce::ScrollBars are still used (not
// wrapped in a Viewport) purely for their native thumb/drag/click-to-page UI, driving plain
// scroll-offset member state directly.
class MidiEditor : public nui::Component,
                    public juce::FileDragAndDropTarget,
                    public juce::DragAndDropTarget,
                    public juce::ScrollBar::Listener,
                    private juce::Timer
{
public:
    struct Listener
    {
        virtual ~Listener() = default;

        // Mirrors ProgressionSlotView::Listener::onFileDropped exactly, beat- not slot-indexed:
        // the owner resolves filePath via its in-flight drag map and calls addChordAtBeat() back
        // with the result - this class never resolves chord data itself.
        virtual void onChordFileDropped(double startBeat, const juce::String& filePath) = 0;

        // Fired after any mutation (add/move/resize/delete of a note or chord block) - once per
        // completed gesture, not continuously mid-drag. Default no-op so listeners that don't care
        // (there's only ever one method to implement otherwise) aren't forced to override it.
        virtual void onContentChanged() {}

        // Fired whenever playback starts/stops - both user-initiated (the play/pause button) and
        // internally forced (clear()/restoreState() stop playback first, since either can swap out
        // the very notes a playing loop was referencing). Default no-op, same convention as
        // onContentChanged.
        virtual void onPlaybackStateChanged(bool isPlaying) { juce::ignoreUnused(isPlaying); }

        // Fired when MIDI record arm turns on/off (user toggle or clear/restore).
        virtual void onRecordingStateChanged(bool isRecording) { juce::ignoreUnused(isRecording); }

        // Click (not drag) on a chord-lane label - midiNotes are the live piano-roll notes still
        // tagged with that block's id (what the user currently sees for that chord), sorted
        // ascending. Empty when every note was individually deleted. Default no-op.
        virtual void onChordBlockPreviewRequested(const std::vector<int>& midiNotes)
        {
            juce::ignoreUnused(midiNotes);
        }
    };

    // Exposed so callers spacing consecutive chords/presets one bar apart (see
    // ProgressionEditor::loadPreset) don't need to hardcode the bar length themselves.
    static constexpr double kBeatsPerBar = 4.0;

    // progressionPlayer / inputMidiNoteTracker are nullable (defaults null so test construction
    // with just an identifier works). Playback and live MIDI-input highlighting are no-ops when null.
    explicit MidiEditor(const std::string& identifier,
                        audio::ProgressionPlayer* progressionPlayer = nullptr,
                        audio::InputMidiNoteTracker* inputMidiNoteTracker = nullptr);
    ~MidiEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Session key/scale kept in sync with the key/scale pickers (used as fallback analysis context).
    // Chord-lane roman numerals only appear when a scale is attached to that block.
    void setAnalysisKeyAndScale(theory::Key key, theory::Scale scale);

    // Absolute name, or "Am - ii" when that block has an attached scale with a computable roman.
    [[nodiscard]] std::string getChordBlockDisplayLabel(int index) const;

    // Snaps to the whole-bar (4-beat) cell startBeat falls in, then splits chord into N note
    // blocks (via theory::NoteConvertor::voiceChordCloseToMiddleC) plus one chord-lane block
    // (chord.readableName), spanning whatever's actually free in that cell: the full bar if
    // empty, the remaining gap if another block partially overlaps it, or the full bar again
    // (replacing that block and the notes it originally created) if another block already fully
    // occupies it. A no-op if the cell has no free room at all. sourceSlot is frozen onto the new
    // chord block (see ChordBlockData::sourceSlot) - never re-resolved later.
    void addChordAtBeat(double startBeat, const theory::Chord& chord, const theory::ProgressionSlot& sourceSlot);

    // Resets to empty - used before loading a preset or restoring session state, both of which
    // wholesale-replace the current content rather than merge into it.
    void clear();

    [[nodiscard]] int getNoteCount() const { return static_cast<int>(_notes.size()); }
    [[nodiscard]] int getChordBlockCount() const { return static_cast<int>(_chordBlocks.size()); }
    [[nodiscard]] std::optional<int> getNoteMidiPitch(int index) const;
    [[nodiscard]] std::optional<double> getNoteStartBeat(int index) const;
    [[nodiscard]] std::optional<double> getNoteLengthBeats(int index) const;
    [[nodiscard]] std::optional<double> getChordBlockStartBeat(int index) const;
    [[nodiscard]] std::optional<double> getChordBlockLengthBeats(int index) const;
    [[nodiscard]] std::optional<theory::ProgressionSlot> getChordBlockSlot(int index) const;

    // Pure-data snapshot of the current notes/chord-blocks, and the inverse - used to bridge into
    // Theory::SessionState (DAW project persistence) and Theory::MidiExporter (exact-content drag
    // export) without either depending on this being a live nui::Component. restoreState replaces
    // the current content wholesale (bypassing addChordAtBeat's collision logic, which only makes
    // sense when resolving a *new* drop against *existing* content) and does not fire
    // Listener::onContentChanged (it's an inbound sync, not a user-initiated change).
    [[nodiscard]] theory::MidiEditorState getState() const;
    void restoreState(const theory::MidiEditorState& state);

    // No-op if there's no progressionPlayer. Empty content is allowed while recording (playhead
    // still advances). Loop bounds are auto-computed from content unless the user has manually
    // resized the loop this "editing session" - see the loop-region members below.
    void startPlayback();
    // Stops transport but keeps the playhead where it is (resume via startPlayback).
    void pausePlayback();
    void stopPlayback();
    [[nodiscard]] bool isPlaying() const;

    // MIDI input → piano roll. Arms record and starts transport if needed. Multi-note slices
    // become full-bar chord-lane blocks (same system as chord drops). Toggle off finalizes open notes.
    void startRecording();
    void stopRecording();
    [[nodiscard]] bool isRecording() const { return _isRecording; }

    // One capture poll (also driven by the editor timer while armed). Public so unit tests can
    // advance recording without a JUCE message-thread timer.
    void pollRecordingCapture();

    [[nodiscard]] double getLoopStartBeat() const { return _loopStartBeat; }
    [[nodiscard]] double getLoopEndBeat() const { return _loopEndBeat; }

    // Parks/moves the playhead (and live-seeks if currently playing). beat is snapped to the start
    // of its bar. No-op without a ProgressionPlayer.
    void seekPlayheadToBeat(double beat);
    [[nodiscard]] double getPlayheadBeat() const;

    // Sequencer tempo (BPM) for progression playback - forwarded to ProgressionPlayer.
    void setBpm(double bpm);
    [[nodiscard]] double getBpm() const;

    // Pitch classes (0-11) belonging to whatever is sounding under the UI playhead position -
    // live piano-roll notes active at that beat, or the chord block covering it when notes have
    // been deleted. Used by the bottom mini-piano highlight (and unit-tested directly).
    [[nodiscard]] std::array<bool, 12> getPlayheadChordPitchClasses() const;

    // Pitch classes of the scale attached to the chord under the playhead (empty if none).
    [[nodiscard]] std::array<bool, 12> getPlayheadScalePitchClasses() const;

    // Attach / clear a scale on a chord-lane block (e.g. after a scale-suggestion drop).
    void attachScaleToChordBlock(int chordBlockIndex, theory::Key key, theory::Scale scale);
    void clearScaleFromChordBlock(int chordBlockIndex);

    // Drag payload prefix for scale suggestions: "chordsTheoryScale|<keyJson>|<scaleJson>".
    static constexpr const char* kScaleDragPrefix = "chordsTheoryScale|";
    [[nodiscard]] static bool tryParseScaleDragDescription(const juce::var& description,
                                                           theory::Key& outKey,
                                                           theory::Scale& outScale);

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

    // Internal DND for scale suggestions dropped onto chord chips.
    bool isInterestedInDragSource(const SourceDetails& dragSourceDetails) override;
    void itemDragEnter(const SourceDetails& dragSourceDetails) override;
    void itemDragMove(const SourceDetails& dragSourceDetails) override;
    void itemDragExit(const SourceDetails& dragSourceDetails) override;
    void itemDropped(const SourceDetails& dragSourceDetails) override;

    void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify(const juce::MouseEvent&, float scaleFactor) override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    struct MidiNoteBlock
    {
        int midiNote = 60;
        double startBeat = 0.0;
        double lengthBeats = 1.0;
        int sourceChordId = -1; // which addChordAtBeat() call created this note; -1 = created
                                 // directly (double-click) or its origin chord block was replaced
    };

    struct ChordBlockData
    {
        int id = -1;             // stable id from a monotonic counter - exists only so a later
                                  // full-beat chord drop can find and remove this block's own
                                  // notes when replacing it; not a live editing link
        std::string label;       // frozen snapshot of Chord::readableName at drop time
        double startBeat = 0.0;
        double lengthBeats = 1.0;
        theory::ProgressionSlot sourceSlot; // frozen at drop time - degree + the resolved chord's
                                             // popularityOrder; never re-resolved live afterward
        theory::Chord frozenChord; // full harmony at drop time (for next-chord context)
        bool hasAttachedScale = false;
        theory::Key attachedScaleKey = theory::Key::C;
        theory::Scale attachedScale = theory::Scale::Major;
    };

    enum class DragMode { None, MoveNote, ResizeNoteStart, ResizeNoteEnd, MoveChordBlock, ResizeLoopStart, ResizeLoopEnd, SeekPlayhead };

    void timerCallback() override; // drag auto-scroll, playhead repaint, live MIDI-input highlight, record

    // MIDI record capture helpers (message thread).
    void finalizeOpenRecordNotes();
    void commitRecordedSliceIfChord();
    void ensureRecordingLoopCoversPlayhead();
    void notifyRecordingStateChanged();
    // Flush deferred content listeners once (after record stop / pause). Cheap no-op if clean.
    void flushRecordingContentChanged();

    // paint helpers
    void paintGridlines(juce::Graphics&) const;
    void paintChordLane(juce::Graphics&) const;
    // Absolute name only, unless the block has an attached scale — then "Am - ii" vs that scale.
    [[nodiscard]] std::string chordBlockDisplayName(const ChordBlockData& block) const;
    [[nodiscard]] theory::Chord harmonyForChordBlock(const ChordBlockData& block) const;
    void paintNotes(juce::Graphics&) const;
    void paintRuler(juce::Graphics&) const;
    void paintGutter(juce::Graphics&) const;
    void paintLoopRegion(juce::Graphics&) const;
    void paintPlayhead(juce::Graphics&) const;
    void paintPianoKeyboard(juce::Graphics&) const;

    // coordinate math
    [[nodiscard]] float beatToX(double beat) const noexcept;
    [[nodiscard]] double xToBeat(float x) const noexcept;
    [[nodiscard]] float pitchToY(int midiNote) const noexcept;
    [[nodiscard]] int yToPitch(float y) const noexcept;
    [[nodiscard]] static double snapBeat(double beat) noexcept;

    // Same beat the playhead line uses (player playhead when available, else loop start).
    [[nodiscard]] double getUiPlayheadBeat() const;

    // Snaps beat to the containing bar's start (whole bars of kBeatsPerBar).
    [[nodiscard]] static double snapBeatToBar(double beat) noexcept;

    void seekPlayheadFromPosition(juce::Point<float> position);

    // Chord-lane block covering beat, if any (used for scale attach + playhead scale lookup).
    [[nodiscard]] int findChordBlockIndexAtBeat(double beat) const;

    // hit-testing
    [[nodiscard]] int hitTestNote(juce::Point<float>) const;
    [[nodiscard]] int hitTestChordBlock(juce::Point<float>) const;
    [[nodiscard]] int hitTestChordDeleteButton(juce::Point<float>) const;
    [[nodiscard]] bool isInNoteResizeZone(int noteIndex, juce::Point<float>, bool leftEdge) const;
    [[nodiscard]] bool isInLoopHandleZone(juce::Point<float>, bool startHandle) const;

    // A chord-lane block visually/logically stays exactly as long as the longest of the notes that
    // still carry its id (falls back to its own stored lengthBeats if none remain, e.g. every note
    // it created was individually deleted) - block.lengthBeats itself is never mutated after
    // creation, this is computed fresh everywhere the block's effective length matters (painting,
    // hit-testing, collision detection against new drops).
    [[nodiscard]] double effectiveChordBlockLength(const ChordBlockData& block) const;

    // Chord-lane chip geometry (label pill + hover delete ×).
    [[nodiscard]] juce::Rectangle<float> getChordBlockBounds(const ChordBlockData& block) const;
    [[nodiscard]] juce::Rectangle<float> getChordDeleteButtonBounds(const ChordBlockData& block) const;

    // Removes the chord-lane block and every piano-roll note tagged with its id.
    void removeChordBlockAt(int index);

    // gestures
    void applyDragAt(juce::Point<float> position);
    void updateHoverState(juce::Point<float> position);

    void notifyContentChanged();

    // Recomputes _loopStartBeat/_loopEndBeat from the current note content (a no-op if
    // _loopManuallyAdjusted or there are no notes) - called from notifyContentChanged.
    void recomputeLoopBoundsFromContent();

    // scroll/zoom
    void refreshScrollRanges();
    void zoomHorizontal(float amount, juce::Point<float> anchor);
    void zoomVertical(float amount, juce::Point<float> anchor);
    void updateScrollBarVisibility();

    // Horizontally zooms/scrolls so the current loop region exactly fills the visible content
    // width - triggered by double-clicking the ruler.
    void zoomToLoop();

    std::vector<MidiNoteBlock> _notes;
    std::vector<ChordBlockData> _chordBlocks;
    int _nextChordBlockId = 0;
    std::vector<Listener*> _listeners;

    juce::ScrollBar _hScrollBar { false };
    juce::ScrollBar _vScrollBar { true };
    juce::Rectangle<float> _contentArea;

    double _scrollBeat = 0.0;
    float _scrollRow = 0.f; // set from kMaxMidiNote - kInitialTopMidiNote in the constructor
    float _pixelsPerBeat = 0.f; // set from kDefaultPixelsPerBeat in the constructor
    float _rowHeight = 0.f;     // set from kDefaultRowHeight in the constructor

    DragMode _dragMode = DragMode::None;
    int _draggedNoteIndex = -1;
    int _draggedChordIndex = -1;
    juce::Point<float> _dragStartMouse;
    juce::Point<float> _lastMousePosition;
    double _dragStartBeat = 0.0;
    double _dragStartLengthBeats = 0.0;
    int _dragStartMidiNote = 60;

    int _hoveredNoteIndex = -1;
    int _hoveredChordIndex = -1;
    bool _hoveredChordDeleteButton = false;
    int _selectedChordIndex = -1;
    bool _hoveredIsResizeZone = false;
    bool _hoveredResizeIsLeftEdge = false;
    bool _isHovering = false;

    audio::ProgressionPlayer* _progressionPlayer = nullptr;
    audio::InputMidiNoteTracker* _inputMidiNoteTracker = nullptr;
    std::uint32_t _lastInputMidiGeneration = 0;
    double _loopStartBeat = 0.0;
    double _loopEndBeat = kBeatsPerBar;
    bool _loopManuallyAdjusted = false;

    // Default key/scale for roman numerals on chord chips (block attached scale overrides).
    theory::Key _analysisKey = theory::Key::C;
    theory::Scale _analysisScale = theory::Scale::Major;

    // MIDI record arm + open note-ons (midi note → start beat). Slice note indices fill while a
    // continuous multi-note gesture is active; committed as a chord block when the gesture ends.
    bool _isRecording = false;
    bool _recordDirty = false; // true if notes were written since last session notify
    std::uint32_t _lastRecordCaptureGeneration = 0;
    std::unordered_map<int, double> _recordNoteOnBeats;
    std::vector<int> _recordSliceNoteIndices; // indices into _notes for the current gesture
    std::array<bool, 128> _recordPreviouslyHeld {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiEditor)
};

}
