#include "Component/ProgressionEditor.h"

#include <algorithm>

#include "Component/SavePresetPrompt.h"
#include "Theory/ProgressionPresetFactory.h"
#include "Theory/ProgressionPresetLibrary.h"

namespace component
{

namespace
{
    constexpr float kHeaderRowHeight = 32.f;
}

ProgressionEditor::ProgressionEditor(const std::string& identifier, ChordResolver chordResolver, audio::ProgressionPlayer* progressionPlayer):
    Component(identifier),
    _chordResolver(std::move(chordResolver)),
    _presetPicker("progression-preset-picker-wrapper"),
    _savePresetButton("progression-save-preset-button", nui::Icons::getPlus()),
    _dragHandle("progression-drag-handle"),
    _midiEditor("progression-midi-editor", progressionPlayer)
{
    _presetPicker.addListener(this);

    _savePresetButton.setIconSize(16.f);
    _savePresetButton.addOnClickListener(this);

    _playButton.setIconSize(16.f);
    _playButton.addOnClickListener(this);

    _clearButton.setIconSize(14.f);
    _clearButton.addOnClickListener(this);
    _clearButton.setHelpText(juce::translate("progression_clear_tooltip").toStdString());

    _dragHandle.addListener(this);

    _presetsLabel.setText(juce::translate("progression_presets_label").toStdString());
    _presetsLabel.setFontSize(nui::Theme::LABEL);
    _presetsLabel.setJustificationType(juce::Justification::centredRight);

    _bpmLabel.setText(juce::translate("progression_bpm_label").toStdString());
    _bpmLabel.setFontSize(nui::Theme::LABEL);
    _bpmLabel.setJustificationType(juce::Justification::centredRight);

    _bpmSlider.setComponentID("progression-bpm-slider");
    _bpmSlider.setSliderStyle(juce::Slider::IncDecButtons);
    _bpmSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 44, 22);
    _bpmSlider.setRange(40.0, 240.0, 1.0);
    _bpmSlider.setValue(_midiEditor.getBpm(), juce::dontSendNotification);
    _bpmSlider.setTooltip(juce::translate("progression_bpm_tooltip"));
    _bpmSlider.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    _bpmSlider.onValueChange = [this]
    {
        _midiEditor.setBpm(_bpmSlider.getValue());
    };

    _midiEditor.addListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);

    // play | clear | gap | bpm label | bpm | gap | drag | flex | presets label | picker | save
    _layout.init({ 1, 1, 1 }, { 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1 });

    _layout.setFixedColumnWidth(0, 32.f);
    _layout.setFixedColumnWidth(1, 32.f);
    _layout.setFixedColumnWidth(2, 8.f);
    _layout.setFixedColumnWidth(3, 36.f);
    _layout.setFixedColumnWidth(4, 96.f);
    _layout.setFixedColumnWidth(5, 8.f);
    _layout.setFixedColumnWidth(6, 150.f);
    _layout.setFixedColumnWidth(8, 160.f);
    _layout.setFixedColumnWidth(9, 220.f);
    _layout.setFixedColumnWidth(10, 32.f);

    _layout.setFixedRowHeight(0, kHeaderRowHeight);
    _layout.setFixedRowHeight(1, 12.f);

    _layout.addComponent(_playButton, 0, 0, 1, 1);
    _layout.addComponent(_clearButton, 0, 1, 1, 1);
    _layout.addComponent(_bpmLabel, 0, 3, 1, 1);
    _layout.addComponent("progression-bpm-slider", _bpmSlider, 0, 4, 1, 1);
    _layout.addComponent(_dragHandle, 0, 6, 1, 1);
    _layout.addComponent(_presetsLabel, 0, 8, 1, 1);
    _layout.addComponent(_presetPicker, 0, 9, 1, 1);
    _layout.addComponent(_savePresetButton, 0, 10, 1, 1);
    _layout.addComponent(_midiEditor, 2, 0, 11, 1);
}

ProgressionEditor::~ProgressionEditor()
{
    _presetPicker.removeListener(this);
    _savePresetButton.removeListener(this);
    _playButton.removeListener(this);
    _clearButton.removeListener(this);
    _dragHandle.removeListener(this);
    _midiEditor.removeListener(this);
}

void ProgressionEditor::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void ProgressionEditor::resized()
{
    Component::resized();

    _layout.resized();
}

void ProgressionEditor::setScale(theory::Scale scale)
{
    _currentScale = scale;
    _presetPicker.refreshForScale(scale);
}

void ProgressionEditor::loadPreset(const theory::ProgressionPreset& preset)
{
    _midiEditor.clear();

    for (int i = 0; i < static_cast<int>(preset.slots.size()); ++i)
    {
        const auto& slot = preset.slots[static_cast<std::size_t>(i)];
        if (const auto* chord = _chordResolver ? _chordResolver(slot) : nullptr)
            _midiEditor.addChordAtBeat(static_cast<double>(i) * MidiEditor::kBeatsPerBar, *chord, slot);
    }
}

void ProgressionEditor::clearAll()
{
    _midiEditor.clear();
    // clear() stops playback but does not fire content-changed (restore/load paths own that).
    // User reset must still persist empty session state and refresh next-chord history.
    onContentChanged();
}

std::vector<theory::ProgressionSlot> ProgressionEditor::getPopulatedSlots() const
{
    struct IndexedSlot
    {
        double startBeat;
        theory::ProgressionSlot slot;
    };

    std::vector<IndexedSlot> indexed;
    indexed.reserve(static_cast<std::size_t>(_midiEditor.getChordBlockCount()));

    for (int i = 0; i < _midiEditor.getChordBlockCount(); ++i)
    {
        const auto startBeat = _midiEditor.getChordBlockStartBeat(i);
        const auto slot = _midiEditor.getChordBlockSlot(i);
        if (startBeat && slot)
            indexed.push_back({ *startBeat, *slot });
    }

    std::sort(indexed.begin(), indexed.end(), [](const auto& a, const auto& b) { return a.startBeat < b.startBeat; });

    std::vector<theory::ProgressionSlot> result;
    result.reserve(indexed.size());
    for (const auto& entry : indexed)
        result.push_back(entry.slot);

    return result;
}

void ProgressionEditor::addChordAtBeat(double startBeat, const theory::Chord& chord, const theory::ProgressionSlot& sourceSlot)
{
    _midiEditor.addChordAtBeat(startBeat, chord, sourceSlot);
}

void ProgressionEditor::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void ProgressionEditor::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void ProgressionEditor::onChordFileDropped(double startBeat, const juce::String& filePath)
{
    for (auto* listener : _listeners)
        listener->onChordFileDropped(startBeat, filePath); // pure bubble-up, never resolves itself
}

void ProgressionEditor::onContentChanged()
{
    for (auto* listener : _listeners)
        listener->onContentChanged();
}

void ProgressionEditor::onChordBlockPreviewRequested(const std::vector<int>& midiNotes)
{
    for (auto* listener : _listeners)
        listener->onChordBlockPreviewRequested(midiNotes);
}

void ProgressionEditor::onPlaybackStateChanged(bool isPlaying)
{
    // Not the click handler's job to set this directly - it needs to stay correct even when
    // playback stops "from underneath" (e.g. clear()/restoreState() via a preset load), not just
    // in response to this button's own click.
    _playButton.setIconBinary(isPlaying ? nui::Icons::getStop() : nui::Icons::getPlay());
}

void ProgressionEditor::onPresetSelected(const theory::ProgressionPreset& preset)
{
    loadPreset(preset);
}

void ProgressionEditor::onProgressionDragStarted()
{
    for (auto* listener : _listeners)
        listener->onProgressionDragStarted();
}

void ProgressionEditor::onButtonClick(const std::string& componentID)
{
    if (componentID == _playButton.getComponentID())
    {
        if (_midiEditor.isPlaying())
            _midiEditor.stopPlayback();
        else
            _midiEditor.startPlayback();
        return;
    }

    if (componentID == _clearButton.getComponentID())
    {
        clearAll();
        return;
    }

    if (componentID != _savePresetButton.getComponentID())
        return;

    const auto populatedSlots = getPopulatedSlots();
    if (populatedSlots.empty())
        return;

    // areaToPointTo must be relative to parentComponent (`this`) when parentComponent is non-null,
    // per juce::CallOutBox::launchAsynchronously's documented contract - _savePresetButton.getBounds()
    // is already in that space (it's this component's own direct child), so no conversion is needed.
    SavePresetPrompt::show(_savePresetButton.getBounds(), this,
        [this, populatedSlots](const std::string& name)
        {
            const auto preset = theory::ProgressionPresetFactory::createFromSlots(name, populatedSlots, _currentScale);
            theory::ProgressionPresetLibrary::getInstance().savePreset(preset);
            _presetPicker.refreshForScale(_currentScale);
        });
}

}
