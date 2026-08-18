#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Audio/InputMidiNoteTracker.h"
#include "Audio/ProgressionPlayer.h"
#include "Component/MidiEditor.h"
#include "Theory/ChordDatabase.h"
#include "Theory/MidiEditorState.h"
#include "Theory/NoteConvertor.h"
#include "Theory/ProgressionSlot.h"
#include "Theory/TriadLibrary.h"

using audio::InputMidiNoteTracker;
using audio::ProgressionPlayer;
using component::MidiEditor;
using theory::Chord;
using theory::ChordDatabase;
using theory::Degree;
using theory::Key;
using theory::MidiEditorState;
using theory::NoteConvertor;
using theory::ProgressionSlot;
using theory::Scale;
// theory::Key / Scale also used via theory:: namespace in attach-scale tests

namespace
{
    // Mirrors the fixed default-state constants declared in MidiEditor.cpp's anonymous namespace
    // (kGutterWidth, kRulerHeight, kDefaultPixelsPerBeat, kDefaultRowHeight, kInitialTopMidiNote,
    // kMaxMidiNote, kPianoKeyboardHeight) - a freshly constructed, unzoomed/unscrolled editor's
    // coordinate math is fully deterministic from these, same "hardcode the widget's known fixed
    // layout" precedent ProgressionSlotView's own tests once used (its {40, 28} for an 80x56
    // slot's centre). Kept in sync manually; if MidiEditor.cpp's own defaults ever change, update
    // these too.
    constexpr float kGutterWidth = 40.f;
    constexpr float kRulerHeight = 24.f;
    constexpr float kPixelsPerBeat = 80.f;
    constexpr float kRowHeight = 16.f;
    constexpr int kInitialTopMidiNote = 67;
    constexpr float kScrollbarThickness = 8.f;
    constexpr float kChordLaneHeight = 28.f;
    constexpr float kPianoKeyboardHeight = 80.f; // real bottom piano strip (see MidiEditor.cpp)
    constexpr double kBeatsPerBar = 4.0; // a dropped chord's default length/snap cell is a full bar

    float beatToX(double beat) { return kGutterWidth + static_cast<float>(beat) * kPixelsPerBeat; }
    float pitchToY(int midiNote) { return kRulerHeight + static_cast<float>(kInitialTopMidiNote - midiNote) * kRowHeight; }

    juce::MouseEvent makeMouseEvent(juce::Component& component, juce::Point<float> position)
    {
        return juce::MouseEvent(
            juce::Desktop::getInstance().getMainMouseSource(),
            position,
            juce::ModifierKeys(),
            0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
            &component, &component,
            juce::Time::getCurrentTime(),
            position,
            juce::Time::getCurrentTime(),
            1,
            false);
    }

    struct RecordingListener : public MidiEditor::Listener
    {
        int droppedCount = 0;
        double lastDroppedBeat = -1.0;
        int contentChangedCount = 0;
        int playbackStateChangedCount = 0;
        bool lastPlaybackState = false;
        int chordPreviewCount = 0;
        std::vector<int> lastPreviewNotes;
        theory::Chord lastPreviewChord;
        int lastPreviewBlockId = -1;

        void onChordFileDropped(double startBeat, const juce::String&) override
        {
            ++droppedCount;
            lastDroppedBeat = startBeat;
        }

        void onContentChanged() override { ++contentChangedCount; }

        void onPlaybackStateChanged(bool isPlaying) override
        {
            ++playbackStateChangedCount;
            lastPlaybackState = isPlaying;
        }

        void onChordBlockPreviewRequested(const std::vector<int>& midiNotes,
                                          const theory::Chord& chord,
                                          int blockId) override
        {
            ++chordPreviewCount;
            lastPreviewNotes = midiNotes;
            lastPreviewChord = chord;
            lastPreviewBlockId = blockId;
        }
    };

    const Chord& getTestChord()
    {
        return ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees.front().chords.front();
    }

    // getTestChord() always resolves to degree I - this just packages that fact for addChordAtBeat
    // calls that don't care about provenance beyond "a real, consistent slot".
    ProgressionSlot testSlot(const Chord& chord) { return { Degree::I, chord.popularityOrder }; }
}

TEST_CASE("MidiEditor::addChordAtBeat splits the chord into one note block per chord tone", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    const auto expectedNoteCount = static_cast<int>(NoteConvertor::voiceChordCloseToMiddleC(chord).size());

    editor.addChordAtBeat(0.0, chord, testSlot(chord));

    CHECK(editor.getNoteCount() == expectedNoteCount);
    CHECK(editor.getChordBlockCount() == 1);
    REQUIRE(editor.getChordBlockStartBeat(0).has_value());
    CHECK(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar));

    REQUIRE(editor.getChordBlockSlot(0).has_value());
    CHECK(*editor.getChordBlockSlot(0) == testSlot(chord));
}

TEST_CASE("MidiEditor: double-click on empty space adds a note, double-click on a note removes it", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const juce::Point<float> pos { 200.f, 200.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));
    REQUIRE(editor.getNoteCount() == 1);

    // Same point again - now lands on the note just created, so this removes it instead of adding
    // a second one (the resize-handle zone is only 7px wide, comfortably narrower than a 1-beat
    // note at the default zoom, so a click at its own creation point always lands in its body).
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));
    CHECK(editor.getNoteCount() == 0);
}

TEST_CASE("MidiEditor: dragging a note moves it in both time and pitch", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    // Well clear of both 7px-wide resize handle zones at the note's own default 1-beat length, so
    // this always lands in the note's body (a plain move), not either edge's resize zone.
    const juce::Point<float> start { beatToX(0.5f), pitchToY(originalPitch) + 1.f };
    const juce::Point<float> dragged { start.x + 2.f * kPixelsPerBeat, start.y - 3.f * kRowHeight }; // +2 beats, +3 semitones

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(2.0));
    CHECK(*editor.getNoteMidiPitch(0) == originalPitch + 3);
}

TEST_CASE("MidiEditor: dragging a note's right edge resizes it without moving its start", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    const auto originalStart = *editor.getNoteStartBeat(0);
    const auto originalLength = *editor.getNoteLengthBeats(0);
    const auto rightEdgeX = beatToX(originalStart + originalLength);
    const auto y = pitchToY(originalPitch) + 1.f;

    const juce::Point<float> start { rightEdgeX - 3.f, y }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x + kPixelsPerBeat, y }; // +1 beat of length

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(originalStart));
    CHECK(*editor.getNoteLengthBeats(0) == Catch::Approx(originalLength + 1.0));
}

TEST_CASE("MidiEditor: dragging a note's left edge resizes it without moving its end", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(kBeatsPerBar, getTestChord(), testSlot(getTestChord())); // starts at bar 2, leaving room to drag the start earlier
    REQUIRE(editor.getNoteCount() > 0);

    const auto originalPitch = *editor.getNoteMidiPitch(0);
    const auto originalStart = *editor.getNoteStartBeat(0);
    const auto originalEnd = originalStart + *editor.getNoteLengthBeats(0);
    const auto leftEdgeX = beatToX(originalStart);
    const auto y = pitchToY(originalPitch) + 1.f;

    const juce::Point<float> start { leftEdgeX + 3.f, y }; // inside the resize handle's 7px zone
    const juce::Point<float> dragged { start.x - 0.5f * kPixelsPerBeat, y }; // start half a beat earlier

    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(*editor.getNoteStartBeat(0) == Catch::Approx(originalStart - 0.5));
    CHECK(*editor.getNoteStartBeat(0) + *editor.getNoteLengthBeats(0) == Catch::Approx(originalEnd));
}

TEST_CASE("MidiEditor: a chord block's length tracks the longest of its remaining notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getNoteCount() > 0);
    REQUIRE(editor.getChordBlockCount() == 1);
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar));

    // Extend note 0's right edge by one extra beat - the chord block should grow to match, even
    // though ChordBlockData::lengthBeats itself is never mutated after creation (see
    // MidiEditor::effectiveChordBlockLength).
    const auto pitch = *editor.getNoteMidiPitch(0);
    const auto start = *editor.getNoteStartBeat(0);
    const auto rightEdgeX = beatToX(start + *editor.getNoteLengthBeats(0));
    const auto y = pitchToY(pitch) + 1.f;

    const juce::Point<float> dragStart { rightEdgeX - 3.f, y };
    const juce::Point<float> dragged { dragStart.x + kPixelsPerBeat, y };

    editor.mouseDown(makeMouseEvent(editor, dragStart));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(*editor.getNoteLengthBeats(0) == Catch::Approx(kBeatsPerBar + 1.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(kBeatsPerBar + 1.0));
}

TEST_CASE("MidiEditor: filesDropped fires onChordFileDropped with the drop's beat position", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    REQUIRE(editor.isInterestedInFileDrag({ "chord.mid" }));
    editor.filesDropped({ "chord.mid" }, static_cast<int>(beatToX(4.0)), 100);

    CHECK(listener.droppedCount == 1);
    CHECK(listener.lastDroppedBeat >= 0.0);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: a chord dropped on a fully-occupied beat replaces the existing block and its notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chordA = getTestChord();
    const auto& chordB = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees[1].chords.front();
    const auto expectedCountB = static_cast<int>(NoteConvertor::voiceChordCloseToMiddleC(chordB).size());

    editor.addChordAtBeat(0.0, chordA, { Degree::I, chordA.popularityOrder });
    editor.addChordAtBeat(0.0, chordB, { Degree::II, chordB.popularityOrder }); // same whole-bar cell, chordA fully occupies it

    CHECK(editor.getChordBlockCount() == 1);
    CHECK(editor.getNoteCount() == expectedCountB);
}

TEST_CASE("MidiEditor: a chord dropped on a partially-occupied beat takes only the free remainder", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getChordBlockCount() == 1);
    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));

    // There's no chord-block resize gesture (only move) - so the way to reach a genuine partial
    // overlap is to drag the existing block half a bar (2 beats) to the right, so it now straddles
    // bar cells [0,4) and [4,8), leaving the first half of [0,4) free for the next drop to claim.
    const auto chordLaneY = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight + 4.f;
    const juce::Point<float> start { beatToX(0.0) + 1.f, chordLaneY };
    const juce::Point<float> dragged { start.x + kPixelsPerBeat * 2.f, chordLaneY };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(2.0));

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // targets cell [0,4) - [2,4) of it is now occupied

    REQUIRE(editor.getChordBlockCount() == 2);
    CHECK(*editor.getChordBlockStartBeat(1) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(1) == Catch::Approx(2.0));
}

TEST_CASE("MidiEditor::clear empties both notes and chord blocks", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getNoteCount() > 0);
    REQUIRE(editor.getChordBlockCount() > 0);

    editor.clear();

    CHECK(editor.getNoteCount() == 0);
    CHECK(editor.getChordBlockCount() == 0);
}

TEST_CASE("MidiEditor: clicking a chord-lane label previews its notes without moving the block", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    const auto& chord = getTestChord();
    const auto expectedNotes = NoteConvertor::voiceChordCloseToMiddleC(chord);
    editor.addChordAtBeat(0.0, chord, testSlot(chord));
    const auto contentChangedAfterAdd = listener.contentChangedCount;

    const auto chordLaneY = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight + 4.f;
    const juce::Point<float> clickPos { beatToX(0.0) + 1.f, chordLaneY };
    editor.mouseDown(makeMouseEvent(editor, clickPos));
    editor.mouseUp(makeMouseEvent(editor, clickPos));

    REQUIRE(listener.chordPreviewCount == 1);
    REQUIRE(listener.lastPreviewNotes.size() == expectedNotes.size());
    for (std::size_t i = 0; i < expectedNotes.size(); ++i)
        CHECK(listener.lastPreviewNotes[i] == expectedNotes[i]);
    CHECK(listener.lastPreviewChord.readableName == chord.readableName);
    CHECK(listener.lastPreviewBlockId >= 0);

    // Pure click must not move the block or fire another content-changed.
    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));
    CHECK(listener.contentChangedCount == contentChangedAfterAdd);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: dragging a chord-lane label does not preview, it moves", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    const auto contentChangedAfterAdd = listener.contentChangedCount;

    const auto chordLaneY = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight + 4.f;
    const juce::Point<float> start { beatToX(0.0) + 1.f, chordLaneY };
    const juce::Point<float> dragged { start.x + kPixelsPerBeat * 2.f, chordLaneY };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    CHECK(listener.chordPreviewCount == 0);
    REQUIRE(*editor.getChordBlockStartBeat(0) == Catch::Approx(2.0));
    CHECK(listener.contentChangedCount == contentChangedAfterAdd + 1);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor::getState/restoreState round-trips notes, chord blocks, and their sourceSlot exactly", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    editor.addChordAtBeat(0.0, chord, testSlot(chord));
    editor.addChordAtBeat(kBeatsPerBar, chord, { Degree::V, chord.popularityOrder });

    const auto state = editor.getState();
    REQUIRE(state.notes.size() == static_cast<std::size_t>(editor.getNoteCount()));
    REQUIRE(state.chordBlocks.size() == 2);

    MidiEditor restored("test-midi-editor-restored");
    restored.setBounds(0, 0, 800, 400);
    restored.restoreState(state);

    CHECK(restored.getNoteCount() == editor.getNoteCount());
    CHECK(restored.getChordBlockCount() == editor.getChordBlockCount());
    CHECK(restored.getState() == state);

    REQUIRE(restored.getChordBlockSlot(0).has_value());
    CHECK(*restored.getChordBlockSlot(0) == testSlot(chord));
    REQUIRE(restored.getChordBlockSlot(1).has_value());
    CHECK(*restored.getChordBlockSlot(1) == ProgressionSlot { Degree::V, chord.popularityOrder });
}

TEST_CASE("MidiEditor::onContentChanged fires on add/move/resize/delete but not on a plain click", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    RecordingListener listener;
    editor.addListener(&listener);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    CHECK(listener.contentChangedCount == 1);

    // A completed drag (move the note) fires once more.
    const auto pitch = *editor.getNoteMidiPitch(0);
    const juce::Point<float> start { beatToX(0.5), pitchToY(pitch) + 1.f };
    const juce::Point<float> dragged { start.x + kPixelsPerBeat, start.y };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));
    CHECK(listener.contentChangedCount == 2);

    // A plain click (mouseDown immediately followed by mouseUp at the same position, no drag) on
    // empty space does not count as a mutation.
    const juce::Point<float> emptySpace { 700.f, 300.f };
    editor.mouseDown(makeMouseEvent(editor, emptySpace));
    editor.mouseUp(makeMouseEvent(editor, emptySpace));
    CHECK(listener.contentChangedCount == 2);

    // Double-click delete fires again.
    editor.mouseDoubleClick(makeMouseEvent(editor, dragged));
    CHECK(listener.contentChangedCount == 3);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: loop bounds auto-compute to the first/last note's bar boundaries", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // bar 0: [0,4)

    // A manually-added note past that, at beat 6 (bar 1's territory: [4,8)), 1 beat long by
    // default - the loop must stretch to cover it too. (Kept within the 800px-wide test bounds'
    // visible content area - mouseDoubleClick silently no-ops outside _contentArea.)
    const juce::Point<float> pos { beatToX(6.0), pitchToY(60) + 1.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, pos));

    CHECK(editor.getLoopStartBeat() == Catch::Approx(0.0));
    CHECK(editor.getLoopEndBeat() == Catch::Approx(8.0)); // ceil(7/4)*4
}

TEST_CASE("MidiEditor: a manually-resized loop no longer moves when content changes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // loop auto-tracks to [0,4)
    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(4.0));

    // Drag the loop's end handle (in the ruler band, y within [0,kRulerHeight]) one beat later.
    const auto handleX = beatToX(4.0);
    const juce::Point<float> start { handleX, 10.f };
    const juce::Point<float> dragged { handleX + kPixelsPerBeat, 10.f };
    editor.mouseDown(makeMouseEvent(editor, start));
    editor.mouseDrag(makeMouseEvent(editor, dragged));
    editor.mouseUp(makeMouseEvent(editor, dragged));

    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(5.0));

    // Adding a chord far enough out that auto-tracking (if it were still active) would expand the
    // loop to bar 4 ([12,16)) - the manual resize must have opted out of that.
    editor.addChordAtBeat(3.0 * kBeatsPerBar, getTestChord(), testSlot(getTestChord()));

    CHECK(editor.getLoopStartBeat() == Catch::Approx(0.0));
    CHECK(editor.getLoopEndBeat() == Catch::Approx(5.0));
}

TEST_CASE("MidiEditor::getPlayheadChordPitchClasses reflects notes under the loop-start playhead", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    // Empty editor → no highlights.
    auto empty = editor.getPlayheadChordPitchClasses();
    for (bool on : empty)
        CHECK_FALSE(on);

    const auto& chord = getTestChord(); // C major triad: C, E, G
    editor.addChordAtBeat(0.0, chord, testSlot(chord));

    // Stopped playhead sits at loop start (beat 0) → first chord's pitch classes.
    auto active = editor.getPlayheadChordPitchClasses();
    CHECK(active[0]);  // C
    CHECK(active[4]);  // E
    CHECK(active[7]);  // G
    for (int pc = 0; pc < 12; ++pc)
    {
        if (pc == 0 || pc == 4 || pc == 7)
            continue;
        CHECK_FALSE(active[static_cast<std::size_t>(pc)]);
    }

    // Second chord at bar 1 - still parked at loop start, so still the first chord.
    const auto& chordV = ChordDatabase::getInstance().get(Key::C, Scale::Major).degrees[4].chords.front();
    editor.addChordAtBeat(kBeatsPerBar, chordV, { Degree::V, chordV.popularityOrder });
    active = editor.getPlayheadChordPitchClasses();
    CHECK(active[0]);
    CHECK(active[4]);
    CHECK(active[7]);
}

TEST_CASE("MidiEditor: clicking the ruler seeks the playhead to that bar", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    editor.addChordAtBeat(kBeatsPerBar, getTestChord(), testSlot(getTestChord()));

    // Click in the ruler over beat ~5 → bar 1 start (beat 4).
    const juce::Point<float> rulerPos { beatToX(5.0), 10.f };
    editor.mouseDown(makeMouseEvent(editor, rulerPos));
    editor.mouseUp(makeMouseEvent(editor, rulerPos));

    CHECK(editor.getPlayheadBeat() == Catch::Approx(4.0));
    CHECK(player.getPlayheadBeat() == Catch::Approx(4.0));
}

TEST_CASE("MidiEditor::setBpm forwards to the progression player", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBpm(96.0);
    CHECK(editor.getBpm() == Catch::Approx(96.0));
    CHECK(player.getBpm() == Catch::Approx(96.0));
}

TEST_CASE("MidiEditor: attaching a scale to a chord block is round-tripped via getState/restoreState", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);
    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));

    editor.attachScaleToChordBlock(0, theory::Key::A, theory::Scale::Dorian);

    const auto state = editor.getState();
    REQUIRE(state.chordBlocks.size() == 1);
    CHECK(state.chordBlocks[0].hasAttachedScale);
    CHECK(state.chordBlocks[0].attachedScaleKey == theory::Key::A);
    CHECK(state.chordBlocks[0].attachedScale == theory::Scale::Dorian);

    MidiEditor restored("test-midi-editor-restored");
    restored.setBounds(0, 0, 800, 400);
    restored.restoreState(state);

    const auto again = restored.getState();
    REQUIRE(again.chordBlocks.size() == 1);
    CHECK(again.chordBlocks[0].hasAttachedScale);
    CHECK(again.chordBlocks[0].attachedScaleKey == theory::Key::A);
    CHECK(again.chordBlocks[0].attachedScale == theory::Scale::Dorian);

    // Scale tones under playhead at beat 0: A Dorian root is pitch class 9.
    const auto scalePcs = restored.getPlayheadScalePitchClasses();
    CHECK(scalePcs[9]); // A
}

TEST_CASE("MidiEditor: roman numeral appears only when a scale is attached", "[MidiEditor]")
{
    // C major triad as "C" — no roman until a scale is attached.
    const auto c = theory::TriadLibrary::makeTriad(0, theory::TriadQuality::Major, theory::Key::C, 0);
    MidiEditor editor("test-midi-editor-roman");
    editor.setBounds(0, 0, 800, 400);
    editor.setAnalysisKeyAndScale(theory::Key::C, theory::Scale::Major);
    editor.addChordAtBeat(0.0, c, testSlot(c));

    REQUIRE(editor.getChordBlockCount() == 1);
    const auto bare = editor.getChordBlockDisplayLabel(0);
    CHECK(bare.find(" - ") == std::string::npos);
    CHECK(bare.find("C") != std::string::npos);

    // Attach A minor: C is III in A natural minor.
    editor.attachScaleToChordBlock(0, theory::Key::A, theory::Scale::Minor);
    const auto withScale = editor.getChordBlockDisplayLabel(0);
    CHECK(withScale.find("C") != std::string::npos);
    CHECK(withScale.find("III") != std::string::npos);
    CHECK(withScale.find(" - ") != std::string::npos);

    editor.clearScaleFromChordBlock(0);
    CHECK(editor.getChordBlockDisplayLabel(0).find(" - ") == std::string::npos);
}

TEST_CASE("MidiEditor: tryParseScaleDragDescription parses scale payload", "[MidiEditor]")
{
    theory::Key key = theory::Key::C;
    theory::Scale scale = theory::Scale::Major;
    CHECK(MidiEditor::tryParseScaleDragDescription("chordsTheoryScale|Db|Dorian", key, scale));
    CHECK(key == theory::Key::Db);
    CHECK(scale == theory::Scale::Dorian);
    CHECK_FALSE(MidiEditor::tryParseScaleDragDescription("not-a-scale", key, scale));
}

TEST_CASE("MidiEditor::startPlayback on an empty editor is a no-op", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBounds(0, 0, 800, 400);

    REQUIRE(editor.getNoteCount() == 0);

    editor.startPlayback();

    CHECK_FALSE(editor.isPlaying());
    CHECK_FALSE(player.isPlaying());
}

TEST_CASE("MidiEditor::pausePlayback freezes transport without clearing the playhead", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor-pause", &player);
    editor.setBounds(0, 0, 800, 400);
    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));

    editor.startPlayback();
    REQUIRE(editor.isPlaying());

    player.seek(2.0);
    editor.pausePlayback();

    CHECK_FALSE(editor.isPlaying());
    CHECK(editor.getPlayheadBeat() == Catch::Approx(2.0));

    editor.startPlayback();
    CHECK(editor.isPlaying());
    CHECK(editor.getPlayheadBeat() == Catch::Approx(2.0));
}

TEST_CASE("MidiEditor: record captures MIDI input notes and multi-note slices as chords", "[MidiEditor]")
{
    ProgressionPlayer player;
    InputMidiNoteTracker tracker;
    MidiEditor editor("test-midi-editor-record", &player, &tracker);
    editor.setBounds(0, 0, 800, 400);
    editor.setAnalysisKeyAndScale(theory::Key::C, theory::Scale::Major);
    editor.setBpm(120.0);

    editor.startRecording();
    REQUIRE(editor.isRecording());
    REQUIRE(editor.isPlaying());

    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();
    CHECK(editor.getNoteCount() == 0);

    player.seek(1.0);

    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();

    REQUIRE(editor.getNoteCount() == 3);
    REQUIRE(editor.getChordBlockCount() == 1);
    // Chip sits on the gesture span (playhead was 0 → 1), not forced to a full bar.
    CHECK(*editor.getChordBlockStartBeat(0) == Catch::Approx(0.0));
    CHECK(*editor.getChordBlockLengthBeats(0) == Catch::Approx(1.0).margin(0.01));
    CHECK(editor.getChordBlockDisplayLabel(0).find("C") != std::string::npos);

    editor.stopRecording();
    CHECK_FALSE(editor.isRecording());
}

TEST_CASE("MidiEditor: two recorded chords in the same bar do not stack", "[MidiEditor]")
{
    ProgressionPlayer player;
    InputMidiNoteTracker tracker;
    MidiEditor editor("test-midi-editor-record-overlap", &player, &tracker);
    editor.setBounds(0, 0, 800, 400);
    editor.setAnalysisKeyAndScale(theory::Key::C, theory::Scale::Major);

    editor.startRecording();

    // C major at beat 0 → 1
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 67, (juce::uint8) 100), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();
    player.seek(1.0);
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 67), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();

    // D minor at beat 1 → 2 (same bar as C if bar = 4 beats, both in bar 0)
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 65, (juce::uint8) 100), 0);
        buffer.addEvent(juce::MidiMessage::noteOn(1, 69, (juce::uint8) 100), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();
    player.seek(2.0);
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 65), 0);
        buffer.addEvent(juce::MidiMessage::noteOff(1, 69), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();

    REQUIRE(editor.getChordBlockCount() == 2);
    const auto s0 = *editor.getChordBlockStartBeat(0);
    const auto e0 = s0 + *editor.getChordBlockLengthBeats(0);
    const auto s1 = *editor.getChordBlockStartBeat(1);
    const auto e1 = s1 + *editor.getChordBlockLengthBeats(1);

    // Non-overlapping ranges (allow exact abutment).
    CHECK((e0 <= s1 + 1.0e-6 || e1 <= s0 + 1.0e-6));
    editor.stopRecording();
}

TEST_CASE("MidiEditor: single-note record does not create a chord block", "[MidiEditor]")
{
    ProgressionPlayer player;
    InputMidiNoteTracker tracker;
    MidiEditor editor("test-midi-editor-record-mono", &player, &tracker);
    editor.setBounds(0, 0, 800, 400);

    editor.startRecording();
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();
    player.seek(0.5);
    {
        juce::MidiBuffer buffer;
        buffer.addEvent(juce::MidiMessage::noteOff(1, 62), 0);
        tracker.processHostMidi(buffer);
    }
    editor.pollRecordingCapture();

    CHECK(editor.getNoteCount() == 1);
    CHECK(editor.getChordBlockCount() == 0);
    editor.stopRecording();
}

TEST_CASE("MidiEditor::onPlaybackStateChanged fires on start and stop", "[MidiEditor]")
{
    ProgressionPlayer player;
    MidiEditor editor("test-midi-editor", &player);
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));

    RecordingListener listener;
    editor.addListener(&listener);

    editor.startPlayback();
    CHECK(editor.isPlaying());
    CHECK(listener.playbackStateChangedCount == 1);
    CHECK(listener.lastPlaybackState == true);

    editor.stopPlayback();
    CHECK_FALSE(editor.isPlaying());
    CHECK(listener.playbackStateChangedCount == 2);
    CHECK(listener.lastPlaybackState == false);

    editor.removeListener(&listener);
}

TEST_CASE("MidiEditor: double-clicking the ruler zooms/scrolls so the loop exactly fills the visible width, without creating a note", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord())); // loop auto-tracks to [0,4)
    REQUIRE(editor.getLoopStartBeat() == Catch::Approx(0.0));
    REQUIRE(editor.getLoopEndBeat() == Catch::Approx(4.0));

    const auto noteCountBefore = editor.getNoteCount();

    // Double-clicking the ruler (y within [0,kRulerHeight]) must not fall through to the
    // empty-space "add a note" behavior.
    const juce::Point<float> rulerPos { 300.f, 10.f };
    editor.mouseDoubleClick(makeMouseEvent(editor, rulerPos));
    CHECK(editor.getNoteCount() == noteCountBefore);

    // The content area is 800 - kGutterWidth - kScrollbarThickness = 752px wide; zoomed to exactly
    // fit the 4-beat loop, that's 188px/beat. Double-clicking the middle of the chord block's own
    // bar (beat 2, now at x = kGutterWidth + 2*188 = 416) in the chord lane must hit and delete it
    // - at the old default zoom (80px/beat) that same screen x would land around beat 4.7, well
    // outside the block, so this only passes if the zoom genuinely took effect.
    constexpr float kExpectedPixelsPerBeat = (800.f - kGutterWidth - kScrollbarThickness) / 4.f;
    const auto chordLaneY = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight + 4.f;
    const juce::Point<float> midBarPos { kGutterWidth + 2.f * kExpectedPixelsPerBeat, chordLaneY };
    editor.mouseDoubleClick(makeMouseEvent(editor, midBarPos));

    CHECK(editor.getChordBlockCount() == 0);
    CHECK(editor.getNoteCount() == 0); // delete must remove the chord's notes too
}

TEST_CASE("MidiEditor: hover × on a chord-lane chip deletes the chord and its notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    const auto& chord = getTestChord();
    editor.addChordAtBeat(0.0, chord, testSlot(chord));
    REQUIRE(editor.getChordBlockCount() == 1);
    REQUIRE(editor.getNoteCount() > 0);

    // Chip: y = contentBottom+2 … +kChordLaneHeight-2; content bottom =
    // height - scrollbar - piano - chordLane. Delete disc is centred in the chip, inset from its
    // right edge (full-bar width ≈ 4*ppb - 4).
    const auto contentBottom = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight;
    const auto chipCentreY = contentBottom + kChordLaneHeight * 0.5f;
    const auto chipRight = beatToX(4.0) - 4.f; // length*ppb - 4, start at beat 0
    const auto deleteX = chipRight - 3.f - 7.f; // pad + half button size (matches MidiEditor.cpp)
    const juce::Point<float> deletePos { deleteX, chipCentreY };

    editor.mouseMove(makeMouseEvent(editor, deletePos));
    editor.mouseDown(makeMouseEvent(editor, deletePos));
    editor.mouseUp(makeMouseEvent(editor, deletePos));

    CHECK(editor.getChordBlockCount() == 0);
    CHECK(editor.getNoteCount() == 0);
}

TEST_CASE("MidiEditor: Delete/Backspace removes the selected chord and its notes", "[MidiEditor]")
{
    MidiEditor editor("test-midi-editor");
    editor.setBounds(0, 0, 800, 400);

    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    REQUIRE(editor.getChordBlockCount() == 1);
    const auto noteCount = editor.getNoteCount();
    REQUIRE(noteCount > 0);

    const auto chordLaneY = 400.f - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight + 4.f;
    const juce::Point<float> clickPos { beatToX(0.0) + 20.f, chordLaneY };
    editor.mouseDown(makeMouseEvent(editor, clickPos));
    editor.mouseUp(makeMouseEvent(editor, clickPos)); // select + preview

    REQUIRE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK(editor.getChordBlockCount() == 0);
    CHECK(editor.getNoteCount() == 0);

    // Nothing selected → Delete is a no-op (not consumed as "handled" only if no selection - we
    // return false when nothing to delete? Currently returns false only when not delete key.
    // After delete, further Delete should not crash.
    editor.addChordAtBeat(0.0, getTestChord(), testSlot(getTestChord()));
    // No selection after add - Delete should not remove it.
    CHECK_FALSE(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    CHECK(editor.getChordBlockCount() == 1);
}
