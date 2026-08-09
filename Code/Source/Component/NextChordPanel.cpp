#include "Component/NextChordPanel.h"

#include <cmath>

#include "Theory/ChordDatabase.h"
#include "Theory/NextChordAiGenerator.h"
#include "Theory/NextChordGenerator.h"

namespace component
{

// -----------------------------------------------------------------------------
// Row
// -----------------------------------------------------------------------------

NextChordPanel::Row::Row(const std::string& identifier, NextChordPanel& owner,
                         theory::NextChordCandidate candidate, int rowIndex):
    Component(identifier),
    _owner(owner),
    _candidate(std::move(candidate)),
    _rowIndex(rowIndex),
    _playButton(identifier + "-play", nui::Icons::getPlay())
{
    addAndMakeVisible(_playButton);
    _playButton.addOnClickListener(this);
    _playButton.setIconSize(12.f);
    _playButton.setHelpText(juce::translate("next_chord_play_tooltip").toStdString());
    setMouseCursor(juce::MouseCursor::DraggingHandCursor);
}

NextChordPanel::Row::~Row()
{
    _playButton.removeListener(this);
}

void NextChordPanel::Row::setCandidate(theory::NextChordCandidate candidate)
{
    _candidate = std::move(candidate);
    repaint();
}

void NextChordPanel::Row::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if ((_rowIndex % 2) == 0)
    {
        g.setColour(nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce().withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.toFloat().reduced(0.0f, 1.0f), 4.0f);
    }

    // Leave room for the play button on the left.
    auto content = bounds.withTrimmedLeft(kPlayButtonSize + 6);

    // Right side: Fit + Tension meters (independent axes).
    auto metersArea = content.removeFromRight(96);
    auto nameArea = content.removeFromLeft(juce::jmin(72, content.getWidth() / 4));
    auto reasonArea = content;

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    g.setColour(nui::Theme::newColor(nui::Theme::TEXT).asJuce());
    g.drawText(_candidate.chord.readableName, nameArea.reduced(4, 0), juce::Justification::centredLeft);

    g.setColour(nui::Theme::newColor(nui::Theme::DISABLED).asJuce());
    g.drawText(_candidate.reasonLabel, reasonArea.reduced(4, 0), juce::Justification::centredLeft);

    auto fitCol = metersArea.removeFromLeft(metersArea.getWidth() / 2);
    auto tenCol = metersArea;

    const auto paintMeter = [&g](juce::Rectangle<int> area, int percent, const juce::String& label,
                                 juce::Colour fillColour)
    {
        auto top = area.removeFromTop(area.getHeight() / 2);
        g.setColour(nui::Theme::newColor(nui::Theme::DISABLED).asJuce());
        g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));
        g.drawText(label + " " + juce::String(percent), top.reduced(2, 0), juce::Justification::centredLeft);

        auto bar = area.reduced(4, 3).toFloat();
        g.setColour(nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce());
        g.fillRoundedRectangle(bar, 2.0f);
        const float fill = bar.getWidth() * (static_cast<float>(percent) / 100.0f);
        g.setColour(fillColour);
        g.fillRoundedRectangle(bar.withWidth(fill), 2.0f);
    };

    paintMeter(fitCol, _candidate.fitPercent, "F",
               nui::Theme::newColor(nui::Theme::ACCENT).asJuce());
    paintMeter(tenCol, _candidate.tensionPercent, "T",
               nui::Theme::newColor(nui::Theme::SECONDARY_ACCENT).asJuce());
}

void NextChordPanel::Row::resized()
{
    Component::resized();
    _playButton.setBounds(2, (getHeight() - kPlayButtonSize) / 2, kPlayButtonSize, kPlayButtonSize);
}

void NextChordPanel::Row::mouseDown(const juce::MouseEvent& event)
{
    _dragGestureStarted = false;
    _mouseDownOnPlay = _playButton.getBounds().contains(event.getPosition());
}

void NextChordPanel::Row::mouseDrag(const juce::MouseEvent& event)
{
    if (_dragGestureStarted || _mouseDownOnPlay)
        return;

    if (static_cast<float>(event.getDistanceFromDragStart()) < kDragStartThreshold)
        return;

    _dragGestureStarted = true;

    if (_owner._onCandidateDragStarted)
        _owner._onCandidateDragStarted(_candidate);
}

void NextChordPanel::Row::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (_dragGestureStarted)
    {
        _dragGestureStarted = false;
        return;
    }

    if (_mouseDownOnPlay)
        return;

    // Click on the row body → make this the new "current" chord.
    if (_owner._onCandidateChosen)
        _owner._onCandidateChosen(_candidate);
}

void NextChordPanel::Row::onButtonClick(const std::string& componentID)
{
    juce::ignoreUnused(componentID);

    if (_owner._onCandidatePreview)
        _owner._onCandidatePreview(_candidate);
}

// -----------------------------------------------------------------------------
// ListContent
// -----------------------------------------------------------------------------

NextChordPanel::ListContent::ListContent(NextChordPanel& owner):
    _owner(owner)
{
}

void NextChordPanel::ListContent::rebuildRows()
{
    _rows.clear();
    removeAllChildren();

    _rows.reserve(_owner._candidates.size());
    for (std::size_t i = 0; i < _owner._candidates.size(); ++i)
    {
        auto row = std::make_unique<Row>(
            "next-chord-row-" + std::to_string(i),
            _owner,
            _owner._candidates[i],
            static_cast<int>(i));
        addAndMakeVisible(*row);
        _rows.push_back(std::move(row));
    }

    const int height = juce::jmax(kRowHeight, static_cast<int>(_rows.size()) * kRowHeight);
    setSize(juce::jmax(1, getParentWidth() > 0 ? getParentWidth() : getWidth()), height);
    resized();
}

void NextChordPanel::ListContent::resized()
{
    auto bounds = getLocalBounds();
    for (auto& row : _rows)
        row->setBounds(bounds.removeFromTop(kRowHeight));
}

// -----------------------------------------------------------------------------
// NextChordPanel
// -----------------------------------------------------------------------------

NextChordPanel::NextChordPanel(const std::string& identifier):
    Component(identifier)
{
    addAndMakeVisible(_title);
    addAndMakeVisible(_modeButton);
    addAndMakeVisible(_currentLabel);
    addAndMakeVisible(_dramaLabel);
    addAndMakeVisible(_dramaLowLabel);
    addAndMakeVisible(_dramaHighLabel);
    addAndMakeVisible(_dramaSlider);
    addAndMakeVisible(_viewport);

    _title.setFontSize(nui::Theme::LABEL);
    _currentLabel.setFontSize(nui::Theme::SMALL);
    _dramaLabel.setFontSize(nui::Theme::SMALL);
    _dramaLowLabel.setFontSize(nui::Theme::SMALL);
    _dramaHighLabel.setFontSize(nui::Theme::SMALL);

    _modeButton.setFontSize(nui::Theme::SMALL);
    _modeButton.addOnClickListener(this);
    if (!theory::NextChordAiGenerator::isAvailable())
    {
        _modeButton.setEnabled(false);
        _modeButton.setHelpText(theory::NextChordAiGenerator::unavailableReason());
    }

    _dramaSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    _dramaSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    _dramaSlider.setRange(0.0, 1.0, 0.01);
    _dramaSlider.setValue(static_cast<double>(_drama01), juce::dontSendNotification);
    _dramaSlider.setMouseDragSensitivity(160);
    _dramaSlider.setColour(juce::Slider::trackColourId,
                           nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce());
    _dramaSlider.setColour(juce::Slider::thumbColourId,
                           nui::Theme::newColor(nui::Theme::SECONDARY_ACCENT).asJuce());
    _dramaSlider.setColour(juce::Slider::backgroundColourId,
                           nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce().withAlpha(0.45f));
    _dramaSlider.setTooltip(juce::translate("next_chord_drama_tooltip"));
    _dramaSlider.addListener(this);

    _viewport.setViewedComponent(&_listContent, false);
    _viewport.setScrollBarsShown(true, false);
    _viewport.setScrollBarThickness(kScrollbarThickness);
    _viewport.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId,
        nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));

    nui::Component::displayBackground(nui::Theme::SECONDARY_BACKGROUND, nui::Theme::getBorderRadius());
    syncLabels();
    syncModeButton();
}

NextChordPanel::~NextChordPanel()
{
    _modeButton.removeListener(this);
    _dramaSlider.removeListener(this);
}

void NextChordPanel::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _key = key;
    _scale = scale;
    regenerate();
}

void NextChordPanel::setCurrentChord(const theory::Chord& chord)
{
    _currentChord = chord;
    regenerate();
}

void NextChordPanel::setSequenceContext(theory::SequenceContext sequence)
{
    sequence.trim();
    _sequence = std::move(sequence);
    regenerate();
}

void NextChordPanel::setCurrentChord(const theory::Chord& chord, theory::SequenceContext sequence)
{
    sequence.trim();
    _sequence = std::move(sequence);
    _currentChord = chord;
    regenerate();
}

void NextChordPanel::setDrama01(float drama01)
{
    drama01 = juce::jlimit(0.0f, 1.0f, drama01);
    if (std::abs(drama01 - _drama01) < 0.0001f)
        return;

    _drama01 = drama01;
    _dramaSlider.setValue(static_cast<double>(_drama01), juce::dontSendNotification);
    regenerate();
}

void NextChordPanel::sliderValueChanged(juce::Slider* slider)
{
    if (slider != &_dramaSlider)
        return;

    const float next = static_cast<float>(_dramaSlider.getValue());
    if (std::abs(next - _drama01) < 0.0001f)
        return;

    _drama01 = next;
    regenerate();
}

void NextChordPanel::syncLabels()
{
    _title.setText(juce::translate("next_chord_panel_title").toStdString());
    _dramaLabel.setText(juce::translate("next_chord_drama_label").toStdString());
    _dramaLowLabel.setText(juce::translate("next_chord_drama_smooth").toStdString());
    _dramaHighLabel.setText(juce::translate("next_chord_drama_wild").toStdString());
    _dramaSlider.setTooltip(juce::translate("next_chord_drama_tooltip"));
    syncModeButton();
}

void NextChordPanel::syncModeButton()
{
    if (_mode == SuggestionMode::Ai)
    {
        _modeButton.setButtonText(juce::translate("next_chord_mode_ai").toStdString());
        _modeButton.setIsSelected(true);
        _modeButton.setHelpText(juce::translate("next_chord_mode_ai_tooltip").toStdString());
    }
    else
    {
        _modeButton.setButtonText(juce::translate("next_chord_mode_theory").toStdString());
        _modeButton.setIsSelected(false);
        _modeButton.setHelpText(juce::translate("next_chord_mode_theory_tooltip").toStdString());
    }
}

void NextChordPanel::setSuggestionMode(SuggestionMode mode)
{
    if (mode == SuggestionMode::Ai && !theory::NextChordAiGenerator::isAvailable())
        mode = SuggestionMode::Theory;
    if (mode == _mode)
        return;
    _mode = mode;
    syncModeButton();
    regenerate();
}

void NextChordPanel::onButtonClick(const std::string& componentID)
{
    if (componentID != "next-chord-mode")
        return;

    setSuggestionMode(_mode == SuggestionMode::Theory ? SuggestionMode::Ai : SuggestionMode::Theory);
}

void NextChordPanel::clear()
{
    _currentChord.reset();
    _sequence = {};
    _candidates.clear();
    _currentLabel.setText("");
    rebuildList();
    repaint();
}

void NextChordPanel::regenerate()
{
    _candidates.clear();

    if (!_currentChord)
    {
        _currentLabel.setText(juce::translate("next_chord_empty_hint").toStdString());
        rebuildList();
        repaint();
        return;
    }

    juce::String currentText = juce::translate("next_chord_current_prefix") + " " + _currentChord->readableName;
    if (!_sequence.empty())
        currentText += "  ·  " + juce::translate("next_chord_history_suffix")
            .replace("%n", juce::String(_sequence.size()));
    _currentLabel.setText(currentText.toStdString());

    const auto& keyScale = theory::ChordDatabase::getInstance().get(_key, _scale);
    if (_mode == SuggestionMode::Ai && theory::NextChordAiGenerator::isAvailable())
    {
        _candidates = theory::NextChordAiGenerator::generate(*_currentChord, keyScale, _drama01, _sequence);
        if (_candidates.empty())
        {
            // Tokenisation or empty model output — fall back to theory with a hint.
            _candidates = theory::NextChordGenerator::generate(*_currentChord, keyScale, _drama01, _sequence);
            _currentLabel.setText(
                (currentText + "  ·  " + juce::translate("next_chord_ai_fallback_hint")).toStdString());
        }
    }
    else
    {
        _candidates = theory::NextChordGenerator::generate(*_currentChord, keyScale, _drama01, _sequence);
    }

    rebuildList();
    _viewport.setViewPosition(0, 0);
    repaint();
}

void NextChordPanel::rebuildList()
{
    _listContent.rebuildRows();
}

void NextChordPanel::resized()
{
    Component::resized();
    auto bounds = getLocalBounds().reduced(kPadding);

    auto titleRow = bounds.removeFromTop(18);
    _modeButton.setBounds(titleRow.removeFromRight(kModeButtonWidth));
    titleRow.removeFromRight(6);
    _title.setBounds(titleRow);

    bounds.removeFromTop(4);
    _currentLabel.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(4);

    auto dramaRow = bounds.removeFromTop(kDramaRowHeight);
    _dramaLabel.setBounds(dramaRow.removeFromLeft(48));
    _dramaLowLabel.setBounds(dramaRow.removeFromLeft(46));
    _dramaHighLabel.setBounds(dramaRow.removeFromRight(36));
    _dramaSlider.setBounds(dramaRow.reduced(4, 2));

    bounds.removeFromTop(6);
    _viewport.setBounds(bounds);

    const int contentWidth = juce::jmax(1, _viewport.getMaximumVisibleWidth());
    _listContent.setSize(contentWidth, juce::jmax(kRowHeight, static_cast<int>(_candidates.size()) * kRowHeight));
    _listContent.resized();
}

void NextChordPanel::paint(juce::Graphics& g)
{
    Component::paint(g);
}

}
