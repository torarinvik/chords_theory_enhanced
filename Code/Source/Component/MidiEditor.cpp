#include "Component/MidiEditor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "AppSettings.h"
#include "Theory/ChordDatabase.h"
#include "Theory/Key.h"
#include "Theory/NoteConvertor.h"
#include "Theory/Scale.h"

namespace component
{

namespace
{
    constexpr int kMinMidiNote = 24;  // C1
    constexpr int kMaxMidiNote = 108; // C8 - voiceChordCloseToMiddleC always lands ~49-84,
                                       // comfortably inside this with headroom for manual edits
    constexpr int kNumPitchRows = kMaxMidiNote - kMinMidiNote + 1;
    constexpr int kInitialTopMidiNote = 67; // G4, matches the mockup's initial visible top row

    constexpr float kGutterWidth = 40.f;
    constexpr float kRulerHeight = 24.f;
    constexpr float kChordLaneHeight = 28.f;
    constexpr float kChordDeleteButtonSize = 14.f;
    constexpr float kChordDeleteButtonPad = 3.f;
    // Real horizontal piano under the chord lane - height chosen for readable black-key depth and
    // bottom letter labels at pro-plugin proportions.
    constexpr float kPianoKeyboardHeight = 80.f;
    constexpr float kScrollbarThickness = 8.f;

    // Fixed span for the bottom piano - independent of roll scroll/zoom. C2–B6 is ~5 octaves of
    // white keys, readable at typical editor widths.
    constexpr int kPianoMinMidiNote = 36; // C2
    constexpr int kPianoMaxMidiNote = 95; // B6 (inclusive)

    constexpr double kDefaultContentBars = 32.0;
    constexpr double kDefaultNoteLengthBeats = 1.0; // chords pack ~1 beat apart in the mockup, not
                                                     // one-bar-per-chord like MidiExporter's export
    constexpr double kSnapBeats = 0.25; // sixteenth notes at 4 beats/bar
    constexpr float kResizeHandleWidth = 7.f; // fixed screen pixels regardless of zoom

    constexpr float kRulerTickHeight = 16.f; // bottom-aligned tick mark height within the ruler row
    constexpr float kRulerTickLabelGap = 8.f; // horizontal gap between a tick and its beat/bar label

    constexpr float kMinPixelsPerBeat = 20.f, kMaxPixelsPerBeat = 400.f, kDefaultPixelsPerBeat = 80.f;
    constexpr float kMinRowHeight = 8.f, kMaxRowHeight = 40.f, kDefaultRowHeight = 16.f;
    constexpr float kZoomStepMultiplier = 1.08f; // multiplicative, not additive - additive steps
                                                  // feel wildly different at low vs. high zoom
    constexpr double kWheelScrollBeats = 2.0; // per full wheel notch
    constexpr float kWheelScrollRows = 3.0f;

    // Same threshold ChordCard uses: below this, mouse-up is a click (preview), not a drag-move.
    constexpr float kClickMaxDistance = 6.f;

    constexpr std::array<bool, 12> kIsBlackKey { false,true,false,true,false,false,true,false,true,false,true,false };
    // C-major spellings (sharps on black keys) - same vocabulary as a C-major keyboard diagram.
    const std::array<juce::String, 12> kNoteNames { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };

    // Settings note-text colour, auto-brightened on ebony keys when the pick is too dark to read.
    juce::Colour noteTextColourForKey(bool onBlackKey, bool isActive)
    {
        if (isActive)
            return juce::Colours::white.withAlpha(0.95f);

        auto colour = AppSettings::getInstance().getNoteTextColour();
        if (onBlackKey && colour.getBrightness() < 0.55f)
            colour = colour.withBrightness(0.78f);
        return colour;
    }
}

MidiEditor::MidiEditor(const std::string& identifier, audio::ProgressionPlayer* progressionPlayer):
    Component(identifier),
    _progressionPlayer(progressionPlayer)
{
    displayBackground(nui::Theme::ThemeColor::BACKGROUND, nui::Theme::getBorderRadius());
    displayBorder(nui::Theme::ThemeColor::BORDER, 1.f, nui::Theme::getBorderRadius(), 1.f);

    _scrollRow = static_cast<float>(kMaxMidiNote - kInitialTopMidiNote);
    _pixelsPerBeat = kDefaultPixelsPerBeat;
    _rowHeight = kDefaultRowHeight;

    addAndMakeVisible(_hScrollBar);
    addAndMakeVisible(_vScrollBar);
    _hScrollBar.addListener(this);
    _vScrollBar.addListener(this);

    // Matches VoicingSelector's own scrollbar exactly: same thickness/colour, and the same
    // auto-hide-until-hovering-and-actually-needed behaviour (see mouseEnter/mouseExit,
    // updateScrollBarVisibility).
    _hScrollBar.setColour(juce::ScrollBar::thumbColourId, nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));
    _vScrollBar.setColour(juce::ScrollBar::thumbColourId, nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));

    addMouseListener(this, true);
    setWantsKeyboardFocus(true);
    AppSettings::getChangeBroadcaster().addChangeListener(this);
}

MidiEditor::~MidiEditor()
{
    // Playback lives on ChordSynthEngine (owned by the audio processor, outliving this editor
    // Component) - without this, closing the editor while a progression is playing would leave it
    // looping forever with no way left to stop it.
    if (_progressionPlayer != nullptr)
        _progressionPlayer->stop();

    AppSettings::getChangeBroadcaster().removeChangeListener(this);
    _hScrollBar.removeListener(this);
    _vScrollBar.removeListener(this);
}

void MidiEditor::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &AppSettings::getChangeBroadcaster())
        repaint();
}

void MidiEditor::paint(juce::Graphics& g)
{
    Component::paint(g);

    {
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(_contentArea.toNearestInt());
        paintGridlines(g);
        paintNotes(g);
    }

    {
        const juce::Rectangle<float> chordLaneBounds(0.f, _contentArea.getBottom(),
            static_cast<float>(getWidth()), kChordLaneHeight);
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(chordLaneBounds.toNearestInt());
        paintChordLane(g);
    }

    {
        const juce::Rectangle<float> pianoBounds(0.f, _contentArea.getBottom() + kChordLaneHeight,
            static_cast<float>(getWidth()), kPianoKeyboardHeight);
        juce::Graphics::ScopedSaveState saved(g);
        g.reduceClipRegion(pianoBounds.toNearestInt());
        paintPianoKeyboard(g);
    }

    paintRuler(g);
    paintGutter(g);

    // Drawn last, fully on top (low-alpha fills so notes/gridlines/ruler stay legible underneath) -
    // the loop region's ruler-band highlight needs to sit over paintRuler's own opaque fill to be
    // visible at all, and the playhead is a transport cursor, conventionally always on top.
    paintLoopRegion(g);
    paintPlayhead(g);
}

void MidiEditor::resized()
{
    Component::resized();

    const auto bounds = getLocalBounds().toFloat();

    _hScrollBar.setBounds(juce::Rectangle<float>(kGutterWidth, bounds.getHeight() - kScrollbarThickness,
        bounds.getWidth() - kGutterWidth - kScrollbarThickness, kScrollbarThickness).toNearestInt());
    _vScrollBar.setBounds(juce::Rectangle<float>(bounds.getWidth() - kScrollbarThickness, kRulerHeight,
        kScrollbarThickness, bounds.getHeight() - kRulerHeight - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight).toNearestInt());

    // Bottom chrome (top → bottom): chord lane, mini-piano, horizontal scrollbar.
    const auto gridBottom = bounds.getHeight() - kScrollbarThickness - kPianoKeyboardHeight - kChordLaneHeight;
    _contentArea = juce::Rectangle<float>(kGutterWidth, kRulerHeight,
        juce::jmax(0.f, bounds.getWidth() - kGutterWidth - kScrollbarThickness),
        juce::jmax(0.f, gridBottom - kRulerHeight));

    refreshScrollRanges();
}

void MidiEditor::addChordAtBeat(double startBeat, const theory::Chord& chord, const theory::ProgressionSlot& sourceSlot)
{
    const auto barIndex = std::floor(juce::jmax(0.0, startBeat) / kBeatsPerBar);
    const auto cellStart = barIndex * kBeatsPerBar;
    const auto cellEnd = cellStart + kBeatsPerBar;

    int fullyCoveringIndex = -1;
    auto leftBound = cellStart;
    auto rightBound = cellEnd;

    for (int i = 0; i < static_cast<int>(_chordBlocks.size()); ++i)
    {
        const auto& block = _chordBlocks[static_cast<std::size_t>(i)];
        const auto blockEnd = block.startBeat + effectiveChordBlockLength(block);
        if (block.startBeat >= cellEnd || blockEnd <= cellStart)
            continue; // no overlap with the target cell at all

        if (block.startBeat <= cellStart && blockEnd >= cellEnd)
        {
            fullyCoveringIndex = i;
            break;
        }

        // Simplified two-sided model: a block touching the cell's left edge caps how far left the
        // new chord can start, one touching (or starting inside) the right side caps how far right
        // it can end. Doesn't handle a block sitting as an island fully inside the cell with free
        // space on both sides of it - an edge case only reachable via manual resizing, not from a
        // plain chord-after-chord drop sequence.
        if (block.startBeat <= cellStart)
            leftBound = juce::jmax(leftBound, blockEnd);
        else
            rightBound = juce::jmin(rightBound, block.startBeat);
    }

    double resolvedStart = cellStart;
    double resolvedLength = kBeatsPerBar;

    if (fullyCoveringIndex >= 0)
    {
        const auto removedId = _chordBlocks[static_cast<std::size_t>(fullyCoveringIndex)].id;
        _chordBlocks.erase(_chordBlocks.begin() + fullyCoveringIndex);
        _notes.erase(std::remove_if(_notes.begin(), _notes.end(),
            [removedId](const MidiNoteBlock& note) { return note.sourceChordId == removedId; }), _notes.end());
    }
    else
    {
        if (leftBound >= rightBound)
            return; // no free room left in this cell at all

        resolvedStart = leftBound;
        resolvedLength = rightBound - leftBound;
    }

    const auto chordId = _nextChordBlockId++;
    _chordBlocks.push_back({ chordId, chord.readableName, resolvedStart, resolvedLength, sourceSlot, chord });

    for (const auto midiNote : theory::NoteConvertor::voiceChordCloseToMiddleC(chord))
        _notes.push_back({ midiNote, resolvedStart, resolvedLength, chordId });

    refreshScrollRanges();
    repaint();
    notifyContentChanged();
}

void MidiEditor::clear()
{
    stopPlayback();

    _notes.clear();
    _chordBlocks.clear();
    _nextChordBlockId = 0;
    _selectedChordIndex = -1;
    _hoveredChordIndex = -1;
    _hoveredChordDeleteButton = false;
    _loopStartBeat = 0.0;
    _loopEndBeat = kBeatsPerBar;
    _loopManuallyAdjusted = false;
    refreshScrollRanges();
    repaint();
}

void MidiEditor::removeChordBlockAt(int index)
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return;

    const auto removedId = _chordBlocks[static_cast<std::size_t>(index)].id;
    _chordBlocks.erase(_chordBlocks.begin() + index);
    _notes.erase(std::remove_if(_notes.begin(), _notes.end(),
        [removedId](const MidiNoteBlock& note) { return note.sourceChordId == removedId; }), _notes.end());

    if (_selectedChordIndex == index)
        _selectedChordIndex = -1;
    else if (_selectedChordIndex > index)
        --_selectedChordIndex;

    if (_hoveredChordIndex == index)
    {
        _hoveredChordIndex = -1;
        _hoveredChordDeleteButton = false;
    }
    else if (_hoveredChordIndex > index)
        --_hoveredChordIndex;

    if (_draggedChordIndex == index)
        _draggedChordIndex = -1;
    else if (_draggedChordIndex > index)
        --_draggedChordIndex;

    refreshScrollRanges();
    repaint();
    notifyContentChanged();
}

std::optional<int> MidiEditor::getNoteMidiPitch(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].midiNote;
}

std::optional<double> MidiEditor::getNoteStartBeat(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].startBeat;
}

std::optional<double> MidiEditor::getNoteLengthBeats(int index) const
{
    if (index < 0 || index >= static_cast<int>(_notes.size()))
        return std::nullopt;
    return _notes[static_cast<std::size_t>(index)].lengthBeats;
}

std::optional<double> MidiEditor::getChordBlockStartBeat(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].startBeat;
}

std::optional<double> MidiEditor::getChordBlockLengthBeats(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return effectiveChordBlockLength(_chordBlocks[static_cast<std::size_t>(index)]);
}

std::optional<theory::ProgressionSlot> MidiEditor::getChordBlockSlot(int index) const
{
    if (index < 0 || index >= static_cast<int>(_chordBlocks.size()))
        return std::nullopt;
    return _chordBlocks[static_cast<std::size_t>(index)].sourceSlot;
}

theory::MidiEditorState MidiEditor::getState() const
{
    theory::MidiEditorState state;
    state.nextChordBlockId = _nextChordBlockId;

    state.notes.reserve(_notes.size());
    for (const auto& note : _notes)
        state.notes.push_back({ note.midiNote, note.startBeat, note.lengthBeats, note.sourceChordId });

    state.chordBlocks.reserve(_chordBlocks.size());
    for (const auto& block : _chordBlocks)
    {
        theory::MidiEditorChordBlockState out;
        out.id = block.id;
        out.label = block.label;
        out.startBeat = block.startBeat;
        out.lengthBeats = block.lengthBeats;
        out.sourceSlot = block.sourceSlot;
        out.frozenSymbol = block.frozenChord.symbol;
        out.frozenType = block.frozenChord.type;
        out.frozenNotes = block.frozenChord.notes;
        out.hasAttachedScale = block.hasAttachedScale;
        out.attachedScaleKey = block.attachedScaleKey;
        out.attachedScale = block.attachedScale;
        state.chordBlocks.push_back(std::move(out));
    }

    return state;
}

void MidiEditor::restoreState(const theory::MidiEditorState& state)
{
    stopPlayback();
    _loopManuallyAdjusted = false;
    _selectedChordIndex = -1;
    _hoveredChordIndex = -1;
    _hoveredChordDeleteButton = false;

    _notes.clear();
    _notes.reserve(state.notes.size());
    for (const auto& note : state.notes)
        _notes.push_back({ note.midiNote, note.startBeat, note.lengthBeats, note.sourceChordId });

    _chordBlocks.clear();
    _chordBlocks.reserve(state.chordBlocks.size());
    for (const auto& block : state.chordBlocks)
    {
        ChordBlockData data;
        data.id = block.id;
        data.label = block.label;
        data.startBeat = block.startBeat;
        data.lengthBeats = block.lengthBeats;
        data.sourceSlot = block.sourceSlot;
        data.frozenChord.symbol = block.frozenSymbol.empty() ? block.label : block.frozenSymbol;
        data.frozenChord.readableName = block.label;
        data.frozenChord.type = block.frozenType;
        data.frozenChord.popularityOrder = block.sourceSlot.popularityOrder;
        data.frozenChord.notes = block.frozenNotes;
        data.hasAttachedScale = block.hasAttachedScale;
        data.attachedScaleKey = block.attachedScaleKey;
        data.attachedScale = block.attachedScale;
        _chordBlocks.push_back(std::move(data));
    }

    _nextChordBlockId = state.nextChordBlockId;

    recomputeLoopBoundsFromContent();
    refreshScrollRanges();
    repaint();
}

void MidiEditor::startPlayback()
{
    if (_progressionPlayer == nullptr || _notes.empty() || _progressionPlayer->isPlaying())
        return;

    std::vector<audio::ScheduledNote> scheduledNotes;
    scheduledNotes.reserve(_notes.size());
    for (const auto& note : _notes)
        scheduledNotes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

    _progressionPlayer->setNotes(scheduledNotes);
    _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
    _progressionPlayer->play();

    if (!isTimerRunning())
        startTimerHz(45);

    repaint();

    for (auto* listener : _listeners)
        listener->onPlaybackStateChanged(true);
}

void MidiEditor::stopPlayback()
{
    if (_progressionPlayer == nullptr || !_progressionPlayer->isPlaying())
        return;

    _progressionPlayer->stop();

    if (_dragMode == DragMode::None)
        stopTimer();

    repaint();

    for (auto* listener : _listeners)
        listener->onPlaybackStateChanged(false);
}

bool MidiEditor::isPlaying() const
{
    return _progressionPlayer != nullptr && _progressionPlayer->isPlaying();
}

double MidiEditor::getUiPlayheadBeat() const
{
    if (_progressionPlayer != nullptr)
        return _progressionPlayer->getPlayheadBeat();
    return _loopStartBeat;
}

double MidiEditor::getPlayheadBeat() const
{
    return getUiPlayheadBeat();
}

double MidiEditor::snapBeatToBar(double beat) noexcept
{
    return std::floor(juce::jmax(0.0, beat) / kBeatsPerBar) * kBeatsPerBar;
}

void MidiEditor::seekPlayheadToBeat(double beat)
{
    if (_progressionPlayer == nullptr)
        return;

    _progressionPlayer->seek(snapBeatToBar(beat));
    repaint();
}

void MidiEditor::setBpm(double bpm)
{
    if (_progressionPlayer != nullptr)
        _progressionPlayer->setBpm(bpm);
}

double MidiEditor::getBpm() const
{
    return _progressionPlayer != nullptr ? _progressionPlayer->getBpm() : 120.0;
}

void MidiEditor::seekPlayheadFromPosition(juce::Point<float> position)
{
    if (_progressionPlayer == nullptr)
        return;

    // Ruler (and seek drags) map x → beat; ignore the gutter so a click there doesn't jump to 0.
    if (position.x < kGutterWidth)
        return;

    seekPlayheadToBeat(xToBeat(position.x));
}

std::array<bool, 12> MidiEditor::getPlayheadChordPitchClasses() const
{
    std::array<bool, 12> pitchClasses {};
    pitchClasses.fill(false);

    if (_notes.empty() && _chordBlocks.empty())
        return pitchClasses;

    const auto beat = getUiPlayheadBeat();

    // Prefer the live piano-roll notes still sounding at this beat - they already reflect edits
    // (deleted tones, re-pitched notes, resized lengths).
    auto foundActiveNotes = false;
    for (const auto& note : _notes)
    {
        if (beat >= note.startBeat && beat < note.startBeat + note.lengthBeats)
        {
            pitchClasses[static_cast<std::size_t>(juce::jlimit(0, 11, note.midiNote % 12))] = true;
            foundActiveNotes = true;
        }
    }
    if (foundActiveNotes)
        return pitchClasses;

    // Fall back to the chord block covering this beat (label still present, notes maybe deleted).
    const auto blockIndex = findChordBlockIndexAtBeat(beat);
    if (blockIndex >= 0)
    {
        for (const auto& tone : _chordBlocks[static_cast<std::size_t>(blockIndex)].frozenChord.notes)
            pitchClasses[static_cast<std::size_t>(juce::jlimit(0, 11, tone.getPitchClass()))] = true;
    }

    return pitchClasses;
}

std::array<bool, 12> MidiEditor::getPlayheadScalePitchClasses() const
{
    std::array<bool, 12> pitchClasses {};
    pitchClasses.fill(false);

    const auto blockIndex = findChordBlockIndexAtBeat(getUiPlayheadBeat());
    if (blockIndex < 0)
        return pitchClasses;

    const auto& block = _chordBlocks[static_cast<std::size_t>(blockIndex)];
    if (!block.hasAttachedScale)
        return pitchClasses;

    const auto& data = theory::ChordDatabase::getInstance().get(block.attachedScaleKey, block.attachedScale);
    for (const auto& note : data.scaleNotes)
        pitchClasses[static_cast<std::size_t>(juce::jlimit(0, 11, note.getPitchClass()))] = true;

    return pitchClasses;
}

int MidiEditor::findChordBlockIndexAtBeat(double beat) const
{
    for (int i = 0; i < static_cast<int>(_chordBlocks.size()); ++i)
    {
        const auto& block = _chordBlocks[static_cast<std::size_t>(i)];
        const auto length = effectiveChordBlockLength(block);
        if (beat >= block.startBeat && beat < block.startBeat + length)
            return i;
    }
    return -1;
}

void MidiEditor::attachScaleToChordBlock(int chordBlockIndex, theory::Key key, theory::Scale scale)
{
    if (chordBlockIndex < 0 || chordBlockIndex >= static_cast<int>(_chordBlocks.size()))
        return;

    auto& block = _chordBlocks[static_cast<std::size_t>(chordBlockIndex)];
    block.hasAttachedScale = true;
    block.attachedScaleKey = key;
    block.attachedScale = scale;
    repaint();
    notifyContentChanged();
}

void MidiEditor::clearScaleFromChordBlock(int chordBlockIndex)
{
    if (chordBlockIndex < 0 || chordBlockIndex >= static_cast<int>(_chordBlocks.size()))
        return;

    auto& block = _chordBlocks[static_cast<std::size_t>(chordBlockIndex)];
    if (!block.hasAttachedScale)
        return;

    block.hasAttachedScale = false;
    repaint();
    notifyContentChanged();
}

bool MidiEditor::tryParseScaleDragDescription(const juce::var& description, theory::Key& outKey, theory::Scale& outScale)
{
    const auto text = description.toString();
    if (!text.startsWith(kScaleDragPrefix))
        return false;

    const auto payload = text.fromFirstOccurrenceOf(kScaleDragPrefix, false, false);
    const auto keyText = payload.upToFirstOccurrenceOf("|", false, false);
    const auto scaleText = payload.fromFirstOccurrenceOf("|", false, false);
    if (keyText.isEmpty() || scaleText.isEmpty())
        return false;

    try
    {
        outKey = theory::parseKey(keyText.toStdString());
        outScale = theory::parseScale(scaleText.toStdString());
        return true;
    }
    catch (const std::invalid_argument&)
    {
        return false;
    }
}

void MidiEditor::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void MidiEditor::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void MidiEditor::notifyContentChanged()
{
    recomputeLoopBoundsFromContent();

    // Keep a playing loop in sync with live edits - audible on the next pass rather than stale.
    if (_progressionPlayer != nullptr && _progressionPlayer->isPlaying())
    {
        std::vector<audio::ScheduledNote> scheduledNotes;
        scheduledNotes.reserve(_notes.size());
        for (const auto& note : _notes)
            scheduledNotes.push_back({ note.midiNote, note.startBeat, note.lengthBeats });

        _progressionPlayer->setNotes(scheduledNotes);
        _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
    }

    for (auto* listener : _listeners)
        listener->onContentChanged();
}

void MidiEditor::recomputeLoopBoundsFromContent()
{
    if (_loopManuallyAdjusted || _notes.empty())
        return;

    auto minStart = std::numeric_limits<double>::max();
    auto maxEnd = 0.0;
    for (const auto& note : _notes)
    {
        minStart = juce::jmin(minStart, note.startBeat);
        maxEnd = juce::jmax(maxEnd, note.startBeat + note.lengthBeats);
    }

    _loopStartBeat = std::floor(minStart / kBeatsPerBar) * kBeatsPerBar;
    _loopEndBeat = std::ceil(maxEnd / kBeatsPerBar) * kBeatsPerBar;
}

bool MidiEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() == 1 && files[0].endsWithIgnoreCase(".mid");
}

void MidiEditor::fileDragEnter(const juce::StringArray&, int, int)
{
}

void MidiEditor::fileDragExit(const juce::StringArray&)
{
}

void MidiEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(y);

    if (files.isEmpty())
        return;

    const auto dropBeat = juce::jmax(0.0, static_cast<double>(xToBeat(static_cast<float>(x))));
    for (auto* listener : _listeners)
        listener->onChordFileDropped(dropBeat, files[0]);
}

bool MidiEditor::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    theory::Key key;
    theory::Scale scale;
    return tryParseScaleDragDescription(dragSourceDetails.description, key, scale);
}

void MidiEditor::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    itemDragMove(dragSourceDetails);
}

void MidiEditor::itemDragMove(const SourceDetails& dragSourceDetails)
{
    const auto pos = dragSourceDetails.localPosition.toFloat();
    const auto chordIndex = hitTestChordBlock(pos);
    if (chordIndex != _hoveredChordIndex)
    {
        _hoveredChordIndex = chordIndex;
        repaint();
    }
}

void MidiEditor::itemDragExit(const SourceDetails&)
{
    _hoveredChordIndex = -1;
    repaint();
}

void MidiEditor::itemDropped(const SourceDetails& dragSourceDetails)
{
    theory::Key key;
    theory::Scale scale;
    if (!tryParseScaleDragDescription(dragSourceDetails.description, key, scale))
        return;

    const auto pos = dragSourceDetails.localPosition.toFloat();
    auto chordIndex = hitTestChordBlock(pos);
    if (chordIndex < 0)
        chordIndex = findChordBlockIndexAtBeat(xToBeat(pos.x));

    if (chordIndex >= 0)
        attachScaleToChordBlock(chordIndex, key, scale);

    _hoveredChordIndex = -1;
    repaint();
}

void MidiEditor::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &_hScrollBar)
        _scrollBeat = juce::jmax(0.0, newRangeStart);
    else if (scrollBarThatHasMoved == &_vScrollBar)
        _scrollRow = static_cast<float>(juce::jmax(0.0, newRangeStart));

    repaint();
}

void MidiEditor::mouseEnter(const juce::MouseEvent&)
{
    _isHovering = true;
    updateScrollBarVisibility();
}

void MidiEditor::mouseExit(const juce::MouseEvent& event)
{
    // addMouseListener(this, true) means this also fires for every child's own boundary crossings
    // (e.g. moving from the content area onto a scrollbar) - only treat it as a real "left the
    // whole editor" exit if the pointer has actually left this component's own bounds.
    if (getLocalBounds().contains(event.getEventRelativeTo(this).getPosition()))
        return;

    _isHovering = false;
    updateScrollBarVisibility();
}

void MidiEditor::mouseMove(const juce::MouseEvent& event)
{
    updateHoverState(event.position);
}

void MidiEditor::mouseDown(const juce::MouseEvent& event)
{
    _dragStartMouse = event.position;
    _lastMousePosition = event.position;

    if (isInLoopHandleZone(event.position, true))
    {
        _dragMode = DragMode::ResizeLoopStart;
        _dragStartBeat = _loopStartBeat;
        startTimerHz(45);
        return;
    }
    if (isInLoopHandleZone(event.position, false))
    {
        _dragMode = DragMode::ResizeLoopEnd;
        _dragStartBeat = _loopEndBeat;
        startTimerHz(45);
        return;
    }

    // Click/scrub on the bar ruler (outside loop handles) moves the playhead to that bar.
    if (event.position.y >= 0.f && event.position.y <= kRulerHeight && event.position.x >= kGutterWidth)
    {
        _dragMode = DragMode::SeekPlayhead;
        seekPlayheadFromPosition(event.position);
        startTimerHz(45);
        return;
    }

    // Hover × on a chord-lane chip - one-click delete (block + its notes).
    const auto deleteChordIndex = hitTestChordDeleteButton(event.position);
    if (deleteChordIndex >= 0)
    {
        removeChordBlockAt(deleteChordIndex);
        _dragMode = DragMode::None;
        return;
    }

    const auto noteIndex = hitTestNote(event.position);
    if (noteIndex >= 0)
    {
        const auto& note = _notes[static_cast<std::size_t>(noteIndex)];
        _draggedNoteIndex = noteIndex;
        if (isInNoteResizeZone(noteIndex, event.position, true))
            _dragMode = DragMode::ResizeNoteStart;
        else if (isInNoteResizeZone(noteIndex, event.position, false))
            _dragMode = DragMode::ResizeNoteEnd;
        else
            _dragMode = DragMode::MoveNote;
        _dragStartBeat = note.startBeat;
        _dragStartLengthBeats = note.lengthBeats;
        _dragStartMidiNote = note.midiNote;
        startTimerHz(45);
        return;
    }

    const auto chordIndex = hitTestChordBlock(event.position);
    if (chordIndex >= 0)
    {
        const auto& block = _chordBlocks[static_cast<std::size_t>(chordIndex)];
        _draggedChordIndex = chordIndex;
        _selectedChordIndex = chordIndex;
        _dragMode = DragMode::MoveChordBlock;
        _dragStartBeat = block.startBeat;
        if (getPeer() != nullptr)
            grabKeyboardFocus();
        startTimerHz(45);
        repaint();
        return;
    }

    _selectedChordIndex = -1;
    _dragMode = DragMode::None;
    repaint();
}

void MidiEditor::mouseDrag(const juce::MouseEvent& event)
{
    _lastMousePosition = event.position;
    applyDragAt(event.position);
}

void MidiEditor::mouseUp(const juce::MouseEvent& event)
{
    const auto finishedDragMode = _dragMode;
    const auto finishedChordIndex = _draggedChordIndex;
    const auto originalChordStartBeat = _dragStartBeat;
    const auto didDrag = finishedDragMode != DragMode::None;
    const auto dragDistance = event.position.getDistanceFrom(_dragStartMouse);

    _dragMode = DragMode::None;
    _draggedNoteIndex = -1;
    _draggedChordIndex = -1;

    // The timer is now shared with playback repaint (see timerCallback) - only stop it here if
    // nothing else still needs it running.
    if (!isPlaying())
        stopTimer();

    refreshScrollRanges();
    repaint();

    if (finishedDragMode == DragMode::SeekPlayhead)
        return; // pure transport gesture - no content mutation

    if (finishedDragMode == DragMode::ResizeLoopStart || finishedDragMode == DragMode::ResizeLoopEnd)
    {
        _loopManuallyAdjusted = true;
        if (_progressionPlayer != nullptr)
            _progressionPlayer->setLoopBounds(_loopStartBeat, _loopEndBeat);
        return;
    }

    // Chord-lane label: click (no real drag) selects + previews; a real drag moves it.
    if (finishedDragMode == DragMode::MoveChordBlock
        && finishedChordIndex >= 0
        && finishedChordIndex < static_cast<int>(_chordBlocks.size()))
    {
        if (dragDistance < kClickMaxDistance)
        {
            // Undo any micro-nudge so a pure click never mutates the progression.
            _chordBlocks[static_cast<std::size_t>(finishedChordIndex)].startBeat = originalChordStartBeat;
            _selectedChordIndex = finishedChordIndex;
            if (getPeer() != nullptr)
                grabKeyboardFocus();

            const auto blockId = _chordBlocks[static_cast<std::size_t>(finishedChordIndex)].id;
            std::vector<int> midiNotes;
            midiNotes.reserve(4);
            for (const auto& note : _notes)
            {
                if (note.sourceChordId == blockId)
                    midiNotes.push_back(note.midiNote);
            }
            std::sort(midiNotes.begin(), midiNotes.end());

            for (auto* listener : _listeners)
                listener->onChordBlockPreviewRequested(midiNotes);
            repaint();
            return;
        }

        notifyContentChanged();
        return;
    }

    if (didDrag)
        notifyContentChanged();
}

void MidiEditor::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.position.y >= 0.f && event.position.y <= kRulerHeight)
    {
        zoomToLoop();
        return;
    }

    const auto noteIndex = hitTestNote(event.position);
    if (noteIndex >= 0)
    {
        _notes.erase(_notes.begin() + noteIndex);
        _hoveredNoteIndex = -1;
        refreshScrollRanges();
        repaint();
        notifyContentChanged();
        return;
    }

    const auto chordIndex = hitTestChordBlock(event.position);
    if (chordIndex >= 0)
    {
        // Double-click a chord chip removes the whole chord (lane + notes) - same as the hover ×
        // and Delete/Backspace on a selection.
        removeChordBlockAt(chordIndex);
        return;
    }

    if (!_contentArea.contains(event.position))
        return;

    _notes.push_back({ yToPitch(event.position.y), juce::jmax(0.0, snapBeat(xToBeat(event.position.x))), kDefaultNoteLengthBeats, -1 });
    refreshScrollRanges();
    repaint();
    notifyContentChanged();
}

bool MidiEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (_selectedChordIndex < 0)
            return false;

        removeChordBlockAt(_selectedChordIndex);
        return true;
    }

    return Component::keyPressed(key);
}

void MidiEditor::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (event.mods.isCommandDown() && event.mods.isShiftDown())
    {
        zoomVertical(std::pow(kZoomStepMultiplier, wheel.deltaY), event.position);
        return;
    }
    if (event.mods.isCommandDown())
    {
        zoomHorizontal(std::pow(kZoomStepMultiplier, wheel.deltaY), event.position);
        return;
    }

    if (event.mods.isShiftDown())
    {
        _scrollBeat = juce::jmax(0.0, _scrollBeat - static_cast<double>(wheel.deltaY) * kWheelScrollBeats);
    }
    else
    {
        _scrollRow = juce::jlimit(0.f, static_cast<float>(kNumPitchRows) - 1.f, _scrollRow - wheel.deltaY * kWheelScrollRows);
        if (wheel.deltaX != 0.f)
            _scrollBeat = juce::jmax(0.0, _scrollBeat - static_cast<double>(wheel.deltaX) * kWheelScrollBeats);
    }

    refreshScrollRanges();
    repaint();
}

void MidiEditor::mouseMagnify(const juce::MouseEvent& event, float scaleFactor)
{
    zoomHorizontal(scaleFactor, event.position);
}

void MidiEditor::timerCallback()
{
    // Auto-scroll while dragging near an edge - nudge the scroll offset, then re-apply the drag at
    // the same (now-stale-relative-to-content) mouse position so it keeps tracking it. mouseDrag
    // alone only fires on actual OS mouse-move events, never while the pointer holds still.
    constexpr float kEdgeMargin = 24.f;

    if (_dragMode == DragMode::None)
    {
        // No drag in progress - the timer can only still be running because playback is active
        // (see startPlayback/stopPlayback), so its only job here is to keep the playhead's visual
        // position in sync with the audio thread's actual position.
        if (isPlaying())
            repaint();
        return;
    }

    if (_lastMousePosition.x < _contentArea.getX() + kEdgeMargin)
        _scrollBeat = juce::jmax(0.0, _scrollBeat - 0.15);
    else if (_lastMousePosition.x > _contentArea.getRight() - kEdgeMargin)
        _scrollBeat += 0.15;

    if (_dragMode != DragMode::MoveChordBlock && _dragMode != DragMode::ResizeLoopStart && _dragMode != DragMode::ResizeLoopEnd)
    {
        if (_lastMousePosition.y < _contentArea.getY() + kEdgeMargin)
            _scrollRow = juce::jmax(0.f, _scrollRow - 0.3f);
        else if (_lastMousePosition.y > _contentArea.getBottom() - kEdgeMargin)
            _scrollRow = juce::jmin(static_cast<float>(kNumPitchRows) - 1.f, _scrollRow + 0.3f);
    }

    refreshScrollRanges();
    applyDragAt(_lastMousePosition);
}

void MidiEditor::paintGridlines(juce::Graphics& g) const
{
    const auto border = nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce();

    g.setColour(border.withAlpha(0.15f));
    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y < _contentArea.getY() - _rowHeight || y > _contentArea.getBottom())
            continue;

        g.drawHorizontalLine(static_cast<int>(y), _contentArea.getX(), _contentArea.getRight());
    }

    const auto firstBeat = juce::jmax(0, static_cast<int>(std::floor(xToBeat(_contentArea.getX()))));
    const auto lastBeat = static_cast<int>(std::ceil(xToBeat(_contentArea.getRight())));
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        const auto x = beatToX(static_cast<double>(beat));
        const auto isBarLine = beat % static_cast<int>(kBeatsPerBar) == 0;
        g.setColour(border.withAlpha(isBarLine ? 0.35f : 0.15f));
        g.drawVerticalLine(static_cast<int>(x), _contentArea.getY(), _contentArea.getBottom());
    }
}

void MidiEditor::paintChordLane(juce::Graphics& g) const
{
    const auto laneTop = _contentArea.getBottom();
    const auto laneWidth = static_cast<float>(getWidth());

    // Middle strip between the grid and the mini-piano - no outer-corner rounding (the piano owns
    // the widget's bottom silhouette now). Full width including under the gutter/scrollbar.
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());
    g.fillRect(juce::Rectangle<float>(0.f, laneTop, laneWidth, kChordLaneHeight));
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.drawRect(juce::Rectangle<float>(0.f, laneTop, laneWidth, kChordLaneHeight), 1.f);

    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    const auto textColour = nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce();
    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    for (int i = 0; i < static_cast<int>(_chordBlocks.size()); ++i)
    {
        const auto& block = _chordBlocks[static_cast<std::size_t>(i)];
        const auto bounds = getChordBlockBounds(block);
        if (bounds.getRight() < _contentArea.getX() || bounds.getX() > _contentArea.getRight())
            continue;

        const auto isSelected = i == _selectedChordIndex;
        const auto isHovered = i == _hoveredChordIndex;

        g.setColour(accent.withAlpha(isSelected ? 0.38f : (isHovered ? 0.28f : 0.2f)));
        g.fillRoundedRectangle(bounds, 4.f);
        g.setColour(accent);
        g.drawRoundedRectangle(bounds, 4.f, isSelected ? 2.f : 1.f);

        // Leave room on the right for the hover × so the label never sits under it.
        const auto showDelete = isHovered || isSelected;
        auto labelBounds = bounds;
        if (showDelete)
            labelBounds.removeFromRight(kChordDeleteButtonSize + kChordDeleteButtonPad * 2.f);

        if (block.hasAttachedScale)
        {
            // Chord name on the left, attached scale on the right (before the ×).
            auto nameArea = labelBounds.reduced(4.f, 0.f);
            auto scaleArea = nameArea.removeFromRight(juce::jmin(nameArea.getWidth() * 0.55f, 110.f));
            g.setColour(textColour);
            g.drawText(block.label, nameArea, juce::Justification::centredLeft, true);

            const auto scaleText = theory::getKeyLabel(block.attachedScaleKey) + " "
                + juce::translate(theory::getScaleTranslationKey(block.attachedScale)).toStdString();
            g.setColour(AppSettings::getInstance().getScaleHighlightColour());
            g.drawText(scaleText, scaleArea, juce::Justification::centredRight, true);
        }
        else
        {
            g.setColour(textColour);
            g.drawText(block.label, labelBounds.reduced(4.f, 0.f), juce::Justification::centred, true);
        }

        if (showDelete)
        {
            const auto deleteBounds = getChordDeleteButtonBounds(block);
            const auto deleteHot = isHovered && _hoveredChordDeleteButton;

            g.setColour(deleteHot ? accent.brighter(0.15f) : accent.withAlpha(0.55f));
            g.fillEllipse(deleteBounds);

            // Simple × in white - small but readable on the accent disc.
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            const auto cx = deleteBounds.getCentreX();
            const auto cy = deleteBounds.getCentreY();
            constexpr float arm = 3.2f;
            g.drawLine(cx - arm, cy - arm, cx + arm, cy + arm, 1.4f);
            g.drawLine(cx + arm, cy - arm, cx - arm, cy + arm, 1.4f);
        }
    }
}

void MidiEditor::paintNotes(juce::Graphics& g) const
{
    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();

    for (int i = 0; i < static_cast<int>(_notes.size()); ++i)
    {
        const auto& note = _notes[static_cast<std::size_t>(i)];
        auto bounds = juce::Rectangle<float>(beatToX(note.startBeat), pitchToY(note.midiNote) + 1.f,
            static_cast<float>(note.lengthBeats * static_cast<double>(_pixelsPerBeat)) - 2.f, _rowHeight - 2.f);

        if (bounds.getBottom() < _contentArea.getY() || bounds.getY() > _contentArea.getBottom()
            || bounds.getRight() < _contentArea.getX() || bounds.getX() > _contentArea.getRight())
            continue;

        g.setColour(accent.withAlpha(.6f));
        g.fillRoundedRectangle(bounds, 3.f);

        if (i == _hoveredNoteIndex && _hoveredIsResizeZone)
        {
            g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::EMPTY_SHADE).asJuce());
            g.fillRoundedRectangle(_hoveredResizeIsLeftEdge ? bounds.removeFromLeft(kResizeHandleWidth) : bounds.removeFromRight(kResizeHandleWidth), 3.f);
        }
    }
}

void MidiEditor::paintRuler(juce::Graphics& g) const
{
    const juce::Rectangle<float> rulerBounds(kGutterWidth, 0.f, static_cast<float>(getWidth()) - kGutterWidth, kRulerHeight);
    const auto radius = nui::Theme::getBorderRadius();

    // Spans the full width (not just rulerBounds, which starts past the gutter) so its top-left/
    // top-right corners land exactly on the whole widget's own outer corners - only the top two are
    // rounded here, matching the widget-level displayBackground/displayBorder radius (see the
    // constructor and paintChordLane's own bottom-rounded counterpart). This also covers what used
    // to be the gutter's own separate top-left corner patch, so paintGutter no longer draws one.
    juce::Path topBand;
    topBand.addRoundedRectangle(0.f, 0.f, static_cast<float>(getWidth()), kRulerHeight, radius, radius, true, true, false, false);

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce());
    g.fillPath(topBand);
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    g.strokePath(topBand, juce::PathStrokeType(1.f));

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    const auto firstBeat = juce::jmax(0, static_cast<int>(std::floor(xToBeat(rulerBounds.getX()))));
    const auto lastBeat = static_cast<int>(std::ceil(xToBeat(rulerBounds.getRight())));
    for (int beat = firstBeat; beat <= lastBeat; ++beat)
    {
        const auto x = beatToX(static_cast<double>(beat));
        if (x < rulerBounds.getX() || x > rulerBounds.getRight())
            continue;

        const auto bar = beat / static_cast<int>(kBeatsPerBar) + 1;
        const auto beatInBar = beat % static_cast<int>(kBeatsPerBar);
        const auto label = beatInBar == 0 ? juce::String(bar) : juce::String(bar) + "." + juce::String(beatInBar + 1);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
        g.drawText(label, juce::Rectangle<float>(x + kRulerTickLabelGap, 0.f, 60.f, kRulerHeight), juce::Justification::centredLeft, true);

        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        g.drawVerticalLine(static_cast<int>(x), kRulerHeight - kRulerTickHeight, kRulerHeight);
    }
}

void MidiEditor::paintGutter(juce::Graphics& g) const
{
    // The corner above the grid (behind the ruler's own left edge) is now covered by paintRuler's
    // own top-rounded band, which spans the full widget width - painting it again here would just
    // square off the widget's rounded top-left corner over top of that. Only the actual per-row key
    // cells below get the white/black key colouring, clipped strictly to the grid's own vertical
    // span so nothing bleeds into the ruler or chord lane rows.
    juce::Graphics::ScopedSaveState saved(g);
    g.reduceClipRegion(juce::Rectangle<float>(0.f, _contentArea.getY(), kGutterWidth, _contentArea.getHeight()).toNearestInt());

    const auto whiteKeyColour = juce::Colour(0xFFD9D9D9);
    const auto blackKeyColour = nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce();

    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        const auto isBlackKey = kIsBlackKey[static_cast<std::size_t>(midiNote % 12)];
        g.setColour(isBlackKey ? blackKeyColour : whiteKeyColour);
        g.fillRect(juce::Rectangle<float>(0.f, y, kGutterWidth, _rowHeight));
    }

    // Drawn as dedicated 1px lines rather than a per-row drawRect border - two adjacent rows each
    // drawing a float-stroked rect on their shared edge can anti-alias to a visibly thicker seam
    // whenever _rowHeight (zoom-adjustable) doesn't land on a whole pixel; drawHorizontalLine/
    // drawVerticalLine always draw exactly one physical pixel, and each boundary is only drawn once.
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        g.drawHorizontalLine(static_cast<int>(y), 0.f, kGutterWidth);
    }
    g.drawVerticalLine(0, _contentArea.getY(), _contentArea.getBottom());
    g.drawVerticalLine(static_cast<int>(kGutterWidth), _contentArea.getY(), _contentArea.getBottom());

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    for (int midiNote = kMinMidiNote; midiNote <= kMaxMidiNote; ++midiNote)
    {
        const auto pc = midiNote % 12;
        const auto isBlack = kIsBlackKey[static_cast<std::size_t>(pc)];
        const auto y = pitchToY(midiNote);
        if (y + _rowHeight < _contentArea.getY() || y > _contentArea.getBottom())
            continue;

        // White keys: C4-style. Black keys: C-major sharp names (C#, D#, …) so both match the
        // bottom piano's vocabulary.
        const auto label = isBlack
            ? kNoteNames[static_cast<std::size_t>(pc)]
            : kNoteNames[static_cast<std::size_t>(pc)] + juce::String(midiNote / 12 - 1);
        g.setColour(noteTextColourForKey(isBlack, false));
        g.drawText(label, juce::Rectangle<float>(2.f, y, kGutterWidth - 4.f, _rowHeight), juce::Justification::centred, true);
    }
}

void MidiEditor::paintLoopRegion(juce::Graphics& g) const
{
    const auto x0 = beatToX(_loopStartBeat);
    const auto x1 = beatToX(_loopEndBeat);
    const auto width = static_cast<float>(getWidth());
    if (x1 < kGutterWidth || x0 > width)
        return; // entirely scrolled off-screen

    const auto accent = nui::Theme::newColor(nui::Theme::ThemeColor::ACCENT).asJuce();
    const auto visibleX0 = juce::jmax(x0, kGutterWidth);
    const auto visibleX1 = juce::jmin(x1, width);

    // Low-alpha so the ruler/gridlines/notes underneath stay fully legible - the ruler-band strip
    // needs to sit over paintRuler's own opaque fill (this is drawn later, in the "on top" tier) to
    // be visible at all; the fainter content/chord-lane band just needs to read as "this is the
    // loop's extent" without competing with the notes for attention.
    g.setColour(accent.withAlpha(0.2f));
    g.fillRect(juce::Rectangle<float>(visibleX0, 0.f, visibleX1 - visibleX0, kRulerHeight));

    // Through the grid + chord lane only - leave the mini-piano un-tinted so chord highlights stay
    // readable (and the loop region doesn't read as "owned" by the keyboard strip).
    const auto loopFillBottom = _contentArea.getBottom() + kChordLaneHeight;
    g.setColour(accent.withAlpha(0.08f));
    g.fillRect(juce::Rectangle<float>(visibleX0, _contentArea.getY(), visibleX1 - visibleX0,
        loopFillBottom - _contentArea.getY()));

    constexpr float kHandleWidth = 4.f;
    g.setColour(accent);
    if (x0 >= kGutterWidth && x0 <= width)
        g.fillRect(juce::Rectangle<float>(x0 - kHandleWidth * 0.5f, 0.f, kHandleWidth, kRulerHeight));
    if (x1 >= kGutterWidth && x1 <= width)
        g.fillRect(juce::Rectangle<float>(x1 - kHandleWidth * 0.5f, 0.f, kHandleWidth, kRulerHeight));
}

void MidiEditor::paintPlayhead(juce::Graphics& g) const
{
    // Only once there's (or has been) real content to loop over - an untouched, contentless editor
    // has no meaningful playhead position to show.
    if (_notes.empty() && !_loopManuallyAdjusted)
        return;

    const auto x = beatToX(getUiPlayheadBeat());
    const auto width = static_cast<float>(getWidth());
    // Stop above the mini-piano so the transport cursor doesn't cut through the keyboard keys.
    const auto playheadBottom = _contentArea.getBottom() + kChordLaneHeight;
    if (x < kGutterWidth || x > width)
        return;

    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::TEXT).asJuce());
    g.drawVerticalLine(static_cast<int>(x), 0.f, playheadBottom);

    juce::Path flag;
    flag.addTriangle(x - 4.f, 0.f, x + 4.f, 0.f, x, 8.f);
    g.fillPath(flag);
}

void MidiEditor::paintPianoKeyboard(juce::Graphics& g) const
{
    const auto pianoTop = _contentArea.getBottom() + kChordLaneHeight;
    const auto pianoWidth = static_cast<float>(getWidth());
    const auto radius = nui::Theme::getBorderRadius();

    // Owns the widget's bottom-left/bottom-right rounded silhouette. Horizontal scrollbar sits in
    // the thin strip under this band.
    juce::Path pianoBand;
    pianoBand.addRoundedRectangle(0.f, pianoTop, pianoWidth, kPianoKeyboardHeight, radius, radius,
        false, false, true, true);

    // Clip everything to the rounded band so key fills don't square off the widget corners.
    juce::Graphics::ScopedSaveState clipToBand(g);
    g.reduceClipRegion(pianoBand);

    // Recessed bed behind the keys - reads as the keyboard frame on pro plugins.
    {
        juce::ColourGradient bed(juce::Colour(0xFF1A1D22), 0.f, pianoTop,
            juce::Colour(0xFF0E1014), 0.f, pianoTop + kPianoKeyboardHeight, false);
        g.setGradientFill(bed);
        g.fillPath(pianoBand);
    }

    // Soft inset shadow under the chord lane so the piano sits "below" the roll.
    {
        juce::ColourGradient topShade(juce::Colours::black.withAlpha(0.35f), 0.f, pianoTop,
            juce::Colours::transparentBlack, 0.f, pianoTop + 6.f, false);
        g.setGradientFill(topShade);
        g.fillRect(juce::Rectangle<float>(0.f, pianoTop, pianoWidth, 6.f));
    }

    auto whiteKeyCount = 0;
    for (int midi = kPianoMinMidiNote; midi <= kPianoMaxMidiNote; ++midi)
    {
        if (!kIsBlackKey[static_cast<std::size_t>(midi % 12)])
            ++whiteKeyCount;
    }
    if (whiteKeyCount <= 0)
    {
        g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        g.strokePath(pianoBand, juce::PathStrokeType(1.f));
        return;
    }

    // Small gutters between white keys so they read as separate physical keys, not one slab.
    constexpr float kWhiteKeyGap = 1.f;
    constexpr float kWhiteKeyBottomRadius = 2.5f;
    constexpr float kBlackKeyBottomRadius = 2.f;
    const auto whiteKeyWidth = pianoWidth / static_cast<float>(whiteKeyCount);
    const auto blackKeyWidth = whiteKeyWidth * 0.55f;
    const auto blackKeyHeight = kPianoKeyboardHeight * 0.60f;
    const auto chordPitchClasses = getPlayheadChordPitchClasses();
    const auto scalePitchClasses = getPlayheadScalePitchClasses();
    const auto chordColour = AppSettings::getInstance().getChordHighlightColour();
    const auto scaleColour = AppSettings::getInstance().getScaleHighlightColour();

    // Single membership → full-key wash. Two or more → keep neutral key + role dots (scale then chord).
    const auto paintKeyBodyFill = [&](juce::Path& keyPath, juce::Rectangle<float> bounds,
                                      bool isChord, bool isScale, bool isBlackKey)
    {
        const auto roleCount = (isChord ? 1 : 0) + (isScale ? 1 : 0);
        if (roleCount == 1)
        {
            const auto colour = isChord ? chordColour : scaleColour;
            juce::ColourGradient fill(colour.brighter(isBlackKey ? 0.35f : 0.28f), 0.f, bounds.getY(),
                colour.darker(isBlackKey ? 0.1f : 0.18f), 0.f, bounds.getBottom(), false);
            g.setGradientFill(fill);
            g.fillPath(keyPath);
            g.setColour(colour.withAlpha(isChord ? 0.4f : 0.3f));
            g.strokePath(keyPath, juce::PathStrokeType(isChord ? 2.2f : 1.6f));
            return;
        }

        // Neutral body (idle ivory / ebony) when multi-role or idle.
        if (isBlackKey)
        {
            juce::ColourGradient body(juce::Colour(0xFF3A3D44), 0.f, bounds.getY(),
                juce::Colour(0xFF0A0B0D), 0.f, bounds.getBottom(), false);
            body.addColour(0.18, juce::Colour(0xFF2A2C32));
            body.addColour(0.55, juce::Colour(0xFF141518));
            g.setGradientFill(body);
            g.fillPath(keyPath);
            g.setColour(juce::Colours::white.withAlpha(0.14f));
            g.fillRoundedRectangle(bounds.reduced(1.5f, 0.f).withHeight(3.5f)
                .withY(bounds.getY() + 1.f), 1.f);
            g.setColour(juce::Colours::white.withAlpha(0.08f));
            g.fillRect(juce::Rectangle<float>(bounds.getX() + 0.5f, bounds.getY() + 4.f, 1.f, bounds.getHeight() - 8.f));
            g.setColour(juce::Colours::black.withAlpha(0.5f));
            g.fillRect(juce::Rectangle<float>(bounds.getRight() - 1.5f, bounds.getY() + 4.f, 1.f, bounds.getHeight() - 8.f));
        }
        else
        {
            juce::ColourGradient body(juce::Colour(0xFFFAFAFC), 0.f, bounds.getY(),
                juce::Colour(0xFFE4E6EA), 0.f, bounds.getBottom(), false);
            body.addColour(0.72, juce::Colour(0xFFEFF0F3));
            g.setGradientFill(body);
            g.fillPath(keyPath);
            g.setColour(juce::Colours::white.withAlpha(0.55f));
            g.fillRect(juce::Rectangle<float>(bounds.getX(), bounds.getY() + 1.f, 1.f, bounds.getHeight() - 3.f));
            g.setColour(juce::Colours::black.withAlpha(0.08f));
            g.fillRect(juce::Rectangle<float>(bounds.getRight() - 1.f, bounds.getY() + 1.f, 1.f, bounds.getHeight() - 3.f));
            g.setColour(juce::Colours::black.withAlpha(0.06f));
            g.fillRect(juce::Rectangle<float>(bounds.getX() + 1.f, bounds.getBottom() - 4.f,
                bounds.getWidth() - 2.f, 3.f));
        }
    };

    const auto paintRoleMarkers = [&](juce::Rectangle<float> bounds, bool isChord, bool isScale, bool isBlackKey)
    {
        // Only when 2+ roles apply (single-role uses full-key colour instead).
        if (!(isChord && isScale))
            return;

        const auto diameter = isBlackKey ? 7.5f : 9.f;
        const auto gap = 2.5f;
        const auto totalH = diameter * 2.f + gap;
        // Stack above the note letter, centred horizontally.
        const auto cx = bounds.getCentreX();
        auto y = bounds.getBottom() - (isBlackKey ? 11.f : 18.f) - totalH;

        const auto drawDot = [&](juce::Colour colour)
        {
            const auto dot = juce::Rectangle<float>(cx - diameter * 0.5f, y, diameter, diameter);
            g.setColour(colour);
            g.fillEllipse(dot);
            g.setColour(juce::Colours::white.withAlpha(isBlackKey ? 0.55f : 0.35f));
            g.drawEllipse(dot, 0.9f);
            y += diameter + gap;
        };

        // Fixed order: scale (top) then chord (bottom).
        drawDot(scaleColour);
        drawDot(chordColour);
    };

    // --- White keys ----------------------------------------------------------
    auto whiteIndex = 0;
    for (int midi = kPianoMinMidiNote; midi <= kPianoMaxMidiNote; ++midi)
    {
        const auto pc = midi % 12;
        if (kIsBlackKey[static_cast<std::size_t>(pc)])
            continue;

        const auto x = static_cast<float>(whiteIndex) * whiteKeyWidth;
        const auto bounds = juce::Rectangle<float>(x + kWhiteKeyGap * 0.5f, pianoTop,
            juce::jmax(1.f, whiteKeyWidth - kWhiteKeyGap), kPianoKeyboardHeight - 1.f);
        const auto isChord = chordPitchClasses[static_cast<std::size_t>(pc)];
        const auto isScale = scalePitchClasses[static_cast<std::size_t>(pc)];
        const auto roleCount = (isChord ? 1 : 0) + (isScale ? 1 : 0);
        const auto singleFill = roleCount == 1;

        juce::Path keyPath;
        keyPath.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
            kWhiteKeyBottomRadius, kWhiteKeyBottomRadius, false, false, true, true);

        paintKeyBodyFill(keyPath, bounds, isChord, isScale, false);

        g.setColour(singleFill
            ? (isChord ? chordColour : scaleColour).darker(0.25f).withAlpha(0.9f)
            : juce::Colour(0xFF9A9DA3).withAlpha(0.85f));
        g.strokePath(keyPath, juce::PathStrokeType(0.8f));

        paintRoleMarkers(bounds, isChord, isScale, false);

        const auto label = kNoteNames[static_cast<std::size_t>(pc)];
        const auto labelBounds = bounds.withTrimmedTop(kPianoKeyboardHeight * 0.58f).reduced(1.f, 3.f);
        g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));
        // Multi-role keeps ivory body → normal text colour; single fill needs contrast text.
        const auto labelColour = noteTextColourForKey(false, singleFill);
        if (singleFill)
        {
            g.setColour(juce::Colours::black.withAlpha(0.25f));
            g.drawText(label, labelBounds.translated(0.f, 0.5f), juce::Justification::centredBottom, false);
        }
        g.setColour(labelColour);
        g.drawText(label, labelBounds, juce::Justification::centredBottom, false);

        ++whiteIndex;
    }

    // --- Black keys (drawn after whites so they sit on top) ------------------
    whiteIndex = 0;
    for (int midi = kPianoMinMidiNote; midi <= kPianoMaxMidiNote; ++midi)
    {
        const auto pc = midi % 12;
        if (kIsBlackKey[static_cast<std::size_t>(pc)])
        {
            const auto seamX = static_cast<float>(whiteIndex) * whiteKeyWidth;
            const auto bounds = juce::Rectangle<float>(seamX - blackKeyWidth * 0.5f, pianoTop,
                blackKeyWidth, blackKeyHeight);
            const auto isChord = chordPitchClasses[static_cast<std::size_t>(pc)];
            const auto isScale = scalePitchClasses[static_cast<std::size_t>(pc)];
            const auto roleCount = (isChord ? 1 : 0) + (isScale ? 1 : 0);
            const auto singleFill = roleCount == 1;

            {
                juce::Path shadow;
                const auto shadowBounds = bounds.translated(0.f, 1.5f).expanded(0.5f, 1.f);
                shadow.addRoundedRectangle(shadowBounds.getX(), shadowBounds.getY(),
                    shadowBounds.getWidth(), shadowBounds.getHeight(),
                    kBlackKeyBottomRadius, kBlackKeyBottomRadius, false, false, true, true);
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.fillPath(shadow);
            }

            juce::Path keyPath;
            keyPath.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                kBlackKeyBottomRadius, kBlackKeyBottomRadius, false, false, true, true);

            paintKeyBodyFill(keyPath, bounds, isChord, isScale, true);

            g.setColour(singleFill
                ? (isChord ? chordColour : scaleColour).darker(0.3f)
                : juce::Colours::black.withAlpha(0.9f));
            g.strokePath(keyPath, juce::PathStrokeType(0.9f));

            paintRoleMarkers(bounds, isChord, isScale, true);

            const auto label = kNoteNames[static_cast<std::size_t>(pc)];
            const auto labelBounds = bounds.withTrimmedTop(bounds.getHeight() * 0.42f).reduced(0.5f, 2.f);
            g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));
            const auto labelColour = noteTextColourForKey(true, singleFill);
            if (singleFill)
            {
                g.setColour(juce::Colours::black.withAlpha(0.3f));
                g.drawText(label, labelBounds.translated(0.f, 0.5f), juce::Justification::centredBottom, true);
            }
            g.setColour(labelColour);
            g.drawText(label, labelBounds, juce::Justification::centredBottom, true);
            continue;
        }
        ++whiteIndex;
    }

    // Outer frame on top of everything.
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.strokePath(pianoBand, juce::PathStrokeType(1.2f));
    g.setColour(nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce().withAlpha(0.7f));
    g.strokePath(pianoBand, juce::PathStrokeType(1.f));
}

double MidiEditor::effectiveChordBlockLength(const ChordBlockData& block) const
{
    auto maxLength = 0.0;
    auto foundAny = false;

    for (const auto& note : _notes)
    {
        if (note.sourceChordId != block.id)
            continue;

        foundAny = true;
        maxLength = juce::jmax(maxLength, note.lengthBeats);
    }

    return foundAny ? maxLength : block.lengthBeats;
}

float MidiEditor::beatToX(double beat) const noexcept
{
    return kGutterWidth + static_cast<float>((beat - _scrollBeat) * static_cast<double>(_pixelsPerBeat));
}

double MidiEditor::xToBeat(float x) const noexcept
{
    return _scrollBeat + static_cast<double>((x - kGutterWidth) / _pixelsPerBeat);
}

float MidiEditor::pitchToY(int midiNote) const noexcept
{
    const auto rowIndex = static_cast<float>(kMaxMidiNote - midiNote); // higher pitch = higher on screen
    return kRulerHeight + (rowIndex - _scrollRow) * _rowHeight;
}

int MidiEditor::yToPitch(float y) const noexcept
{
    const auto rowIndex = static_cast<int>(std::floor(_scrollRow + (y - kRulerHeight) / _rowHeight));
    return juce::jlimit(kMinMidiNote, kMaxMidiNote, kMaxMidiNote - rowIndex);
}

double MidiEditor::snapBeat(double beat) noexcept
{
    return juce::jmax(0.0, std::round(beat / kSnapBeats) * kSnapBeats);
}

int MidiEditor::hitTestNote(juce::Point<float> position) const
{
    for (int i = static_cast<int>(_notes.size()) - 1; i >= 0; --i)
    {
        const auto& note = _notes[static_cast<std::size_t>(i)];
        const juce::Rectangle<float> bounds(beatToX(note.startBeat), pitchToY(note.midiNote),
            static_cast<float>(note.lengthBeats * static_cast<double>(_pixelsPerBeat)), _rowHeight);
        if (bounds.contains(position))
            return i;
    }
    return -1;
}

int MidiEditor::hitTestChordBlock(juce::Point<float> position) const
{
    for (int i = static_cast<int>(_chordBlocks.size()) - 1; i >= 0; --i)
    {
        if (getChordBlockBounds(_chordBlocks[static_cast<std::size_t>(i)]).contains(position))
            return i;
    }
    return -1;
}

int MidiEditor::hitTestChordDeleteButton(juce::Point<float> position) const
{
    // Hit the × zone whenever the pointer is over it. The disc is only *painted* when the chip is
    // hovered/selected, so casual clicks on the right edge of a cold chip still move/select
    // (the × zone is a small disc inset from the edge). Hovering first is the intended path.
    for (int i = static_cast<int>(_chordBlocks.size()) - 1; i >= 0; --i)
    {
        if (getChordDeleteButtonBounds(_chordBlocks[static_cast<std::size_t>(i)]).contains(position))
            return i;
    }
    return -1;
}

juce::Rectangle<float> MidiEditor::getChordBlockBounds(const ChordBlockData& block) const
{
    const auto laneTop = _contentArea.getBottom();
    const auto length = effectiveChordBlockLength(block);
    return juce::Rectangle<float>(beatToX(block.startBeat), laneTop + 2.f,
        juce::jmax(1.f, static_cast<float>(length * static_cast<double>(_pixelsPerBeat)) - 4.f),
        kChordLaneHeight - 4.f);
}

juce::Rectangle<float> MidiEditor::getChordDeleteButtonBounds(const ChordBlockData& block) const
{
    const auto chip = getChordBlockBounds(block);
    return juce::Rectangle<float>(kChordDeleteButtonSize, kChordDeleteButtonSize)
        .withCentre({ chip.getRight() - kChordDeleteButtonPad - kChordDeleteButtonSize * 0.5f,
            chip.getCentreY() });
}

bool MidiEditor::isInNoteResizeZone(int noteIndex, juce::Point<float> position, bool leftEdge) const
{
    if (noteIndex < 0 || noteIndex >= static_cast<int>(_notes.size()))
        return false;

    const auto& note = _notes[static_cast<std::size_t>(noteIndex)];

    if (leftEdge)
    {
        const auto edge = beatToX(note.startBeat);
        return position.x >= edge && position.x <= edge + kResizeHandleWidth;
    }

    const auto edge = beatToX(note.startBeat + note.lengthBeats);
    return position.x >= edge - kResizeHandleWidth && position.x <= edge;
}

bool MidiEditor::isInLoopHandleZone(juce::Point<float> position, bool startHandle) const
{
    if (position.y < 0.f || position.y > kRulerHeight)
        return false;

    // Symmetric around the edge (unlike a note's resize zone, which extends only into its own
    // body) - a loop handle has no "inside" it belongs to more than the other side.
    const auto edge = beatToX(startHandle ? _loopStartBeat : _loopEndBeat);
    return position.x >= edge - kResizeHandleWidth && position.x <= edge + kResizeHandleWidth;
}

void MidiEditor::applyDragAt(juce::Point<float> position)
{
    if (_dragMode == DragMode::None)
        return;

    const auto snap = !juce::ModifierKeys::currentModifiers.isShiftDown();
    const auto deltaBeats = xToBeat(position.x) - xToBeat(_dragStartMouse.x);

    if (_dragMode == DragMode::MoveNote && _draggedNoteIndex >= 0)
    {
        auto& note = _notes[static_cast<std::size_t>(_draggedNoteIndex)];
        const auto rawStart = _dragStartBeat + deltaBeats;
        note.startBeat = juce::jmax(0.0, snap ? snapBeat(rawStart) : rawStart);
        note.midiNote = yToPitch(position.y);
        repaint();
    }
    else if (_dragMode == DragMode::ResizeNoteEnd && _draggedNoteIndex >= 0)
    {
        auto& note = _notes[static_cast<std::size_t>(_draggedNoteIndex)];
        const auto rawEnd = _dragStartBeat + _dragStartLengthBeats + deltaBeats;
        const auto snappedEnd = snap ? snapBeat(rawEnd) : rawEnd;
        note.lengthBeats = juce::jmax(kSnapBeats, snappedEnd - _dragStartBeat);
        repaint();
    }
    else if (_dragMode == DragMode::ResizeNoteStart && _draggedNoteIndex >= 0)
    {
        auto& note = _notes[static_cast<std::size_t>(_draggedNoteIndex)];
        const auto originalEnd = _dragStartBeat + _dragStartLengthBeats; // fixed - only the start edge moves
        const auto rawStart = _dragStartBeat + deltaBeats;
        const auto snappedStart = snap ? snapBeat(rawStart) : rawStart;
        const auto clampedStart = juce::jmin(snappedStart, originalEnd - kSnapBeats);
        note.startBeat = juce::jmax(0.0, clampedStart);
        note.lengthBeats = originalEnd - note.startBeat;
        repaint();
    }
    else if (_dragMode == DragMode::MoveChordBlock && _draggedChordIndex >= 0)
    {
        auto& block = _chordBlocks[static_cast<std::size_t>(_draggedChordIndex)];
        const auto rawStart = _dragStartBeat + deltaBeats;
        block.startBeat = juce::jmax(0.0, snap ? snapBeat(rawStart) : rawStart);
        repaint();
    }
    else if (_dragMode == DragMode::ResizeLoopStart)
    {
        const auto rawStart = _dragStartBeat + deltaBeats;
        const auto snappedStart = snap ? snapBeat(rawStart) : rawStart;
        _loopStartBeat = juce::jlimit(0.0, _loopEndBeat - kSnapBeats, snappedStart); // can't pass beat 0, or the loop end
        repaint();
    }
    else if (_dragMode == DragMode::ResizeLoopEnd)
    {
        const auto rawEnd = _dragStartBeat + deltaBeats;
        const auto snappedEnd = snap ? snapBeat(rawEnd) : rawEnd;
        _loopEndBeat = juce::jmax(_loopStartBeat + kSnapBeats, snappedEnd); // no upper bound
        repaint();
    }
    else if (_dragMode == DragMode::SeekPlayhead)
    {
        seekPlayheadFromPosition(position);
    }
}

void MidiEditor::updateHoverState(juce::Point<float> position)
{
    const auto noteIndex = hitTestNote(position);
    const auto isLeftResize = noteIndex >= 0 && isInNoteResizeZone(noteIndex, position, true);
    const auto isResize = noteIndex >= 0 && (isLeftResize || isInNoteResizeZone(noteIndex, position, false));
    const auto chordIndex = hitTestChordBlock(position);
    // Delete × is shown for hovered/selected chips - while the pointer is over a chip it counts
    // as hovered for geometry, so the button lights up as soon as you enter the chip.
    const auto overDelete = chordIndex >= 0
        && getChordDeleteButtonBounds(_chordBlocks[static_cast<std::size_t>(chordIndex)]).contains(position);

    if (noteIndex == _hoveredNoteIndex
        && isResize == _hoveredIsResizeZone
        && isLeftResize == _hoveredResizeIsLeftEdge
        && chordIndex == _hoveredChordIndex
        && overDelete == _hoveredChordDeleteButton)
        return;

    _hoveredNoteIndex = noteIndex;
    _hoveredIsResizeZone = isResize;
    _hoveredResizeIsLeftEdge = isLeftResize;
    _hoveredChordIndex = chordIndex;
    _hoveredChordDeleteButton = overDelete;

    if (overDelete)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    else if (noteIndex < 0 && chordIndex < 0)
        setMouseCursor(juce::MouseCursor::NormalCursor);
    else if (isResize)
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);

    repaint();
}

void MidiEditor::refreshScrollRanges()
{
    auto furthestBeat = kDefaultContentBars * kBeatsPerBar;
    for (const auto& note : _notes)
        furthestBeat = juce::jmax(furthestBeat, note.startBeat + note.lengthBeats);
    for (const auto& block : _chordBlocks)
        furthestBeat = juce::jmax(furthestBeat, block.startBeat + block.lengthBeats);
    const auto contentBeats = furthestBeat + 4.0 * kBeatsPerBar;

    const auto visibleBeats = _pixelsPerBeat > 0.f ? static_cast<double>(_contentArea.getWidth() / _pixelsPerBeat) : 0.0;
    _scrollBeat = juce::jlimit(0.0, juce::jmax(0.0, contentBeats - visibleBeats), _scrollBeat);
    _hScrollBar.setRangeLimits(0.0, contentBeats, juce::dontSendNotification);
    _hScrollBar.setCurrentRange(_scrollBeat, visibleBeats, juce::dontSendNotification);

    const auto visibleRows = _rowHeight > 0.f ? _contentArea.getHeight() / _rowHeight : 0.f;
    _scrollRow = juce::jlimit(0.f, juce::jmax(0.f, static_cast<float>(kNumPitchRows) - visibleRows), _scrollRow);
    _vScrollBar.setRangeLimits(0.0, static_cast<double>(kNumPitchRows), juce::dontSendNotification);
    _vScrollBar.setCurrentRange(static_cast<double>(_scrollRow), static_cast<double>(visibleRows), juce::dontSendNotification);

    updateScrollBarVisibility();
}

void MidiEditor::updateScrollBarVisibility()
{
    // Matches VoicingSelector::updateScrollBarVisibility exactly: hidden unless both hovering and
    // actually needed - re-evaluated here too (not just from mouseEnter/mouseExit) since zoom or
    // content growth/shrinkage can flip "needed" without any hover-state change at all.
    const auto needsHorizontalScroll = _hScrollBar.getCurrentRangeSize() < _hScrollBar.getMaximumRangeLimit() - _hScrollBar.getMinimumRangeLimit();
    const auto needsVerticalScroll = _vScrollBar.getCurrentRangeSize() < _vScrollBar.getMaximumRangeLimit() - _vScrollBar.getMinimumRangeLimit();

    _hScrollBar.setVisible(_isHovering && needsHorizontalScroll);
    _vScrollBar.setVisible(_isHovering && needsVerticalScroll);
}

void MidiEditor::zoomHorizontal(float factor, juce::Point<float> anchor)
{
    const auto anchorBeat = xToBeat(anchor.x);
    _pixelsPerBeat = juce::jlimit(kMinPixelsPerBeat, kMaxPixelsPerBeat, _pixelsPerBeat * factor);
    _scrollBeat = juce::jmax(0.0, anchorBeat - static_cast<double>((anchor.x - kGutterWidth) / _pixelsPerBeat));
    refreshScrollRanges();
    repaint();
}

void MidiEditor::zoomVertical(float factor, juce::Point<float> anchor)
{
    const auto anchorRow = _scrollRow + (anchor.y - kRulerHeight) / _rowHeight;
    _rowHeight = juce::jlimit(kMinRowHeight, kMaxRowHeight, _rowHeight * factor);
    _scrollRow = juce::jmax(0.f, anchorRow - (anchor.y - kRulerHeight) / _rowHeight);
    refreshScrollRanges();
    repaint();
}

void MidiEditor::zoomToLoop()
{
    const auto loopLengthBeats = _loopEndBeat - _loopStartBeat;
    if (loopLengthBeats <= 0.0 || _contentArea.getWidth() <= 0.f)
        return;

    _pixelsPerBeat = juce::jlimit(kMinPixelsPerBeat, kMaxPixelsPerBeat, _contentArea.getWidth() / static_cast<float>(loopLengthBeats));
    _scrollBeat = _loopStartBeat;

    refreshScrollRanges();
    repaint();
}

}
