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

NextChordPanel::Row::Row(const std::string& identifier,
                         NextChordPanel& owner,
                         theory::NextChordCandidate candidate,
                         int rowIndex,
                         Column column):
    Component(identifier),
    _owner(owner),
    _candidate(std::move(candidate)),
    _rowIndex(rowIndex),
    _column(column),
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

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));

    if (_column == Column::Ai)
    {
        // Pure AI row: name + confidence (no Fit/Tension meters).
        auto confArea = content.removeFromRight(juce::jmin(72, content.getWidth() / 3));
        g.setColour(nui::Theme::newColor(nui::Theme::TEXT).asJuce());
        g.drawText(_candidate.chord.readableName, content.reduced(4, 0), juce::Justification::centredLeft);

        g.setColour(nui::Theme::newColor(nui::Theme::DISABLED).asJuce());
        g.drawText(_candidate.reasonLabel, confArea.reduced(2, 0), juce::Justification::centredRight);
        return;
    }

    // Theory row: name + reason + Fit/Tension meters.
    auto metersArea = content.removeFromRight(96);
    auto nameArea = content.removeFromLeft(juce::jmin(72, content.getWidth() / 4));
    auto reasonArea = content;

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

NextChordPanel::ListContent::ListContent(NextChordPanel& owner, Column column):
    _owner(owner),
    _column(column)
{
}

void NextChordPanel::ListContent::rebuildRows(const std::vector<theory::NextChordCandidate>& candidates)
{
    _rows.clear();
    removeAllChildren();

    const char* idPrefix = _column == Column::Ai ? "next-chord-ai-row-" : "next-chord-row-";

    _rows.reserve(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        auto row = std::make_unique<Row>(
            std::string(idPrefix) + std::to_string(i),
            _owner,
            candidates[i],
            static_cast<int>(i),
            _column);
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
    addAndMakeVisible(_currentLabel);
    addAndMakeVisible(_dramaLabel);
    addAndMakeVisible(_dramaLowLabel);
    addAndMakeVisible(_dramaHighLabel);
    addAndMakeVisible(_dramaSlider);
    addAndMakeVisible(_theoryColumnLabel);
    addAndMakeVisible(_aiColumnLabel);
    addAndMakeVisible(_aiEmptyHint);
    addAndMakeVisible(_theoryViewport);
    addAndMakeVisible(_aiViewport);

    _title.setFontSize(nui::Theme::LABEL);
    _currentLabel.setFontSize(nui::Theme::SMALL);
    _dramaLabel.setFontSize(nui::Theme::SMALL);
    _dramaLowLabel.setFontSize(nui::Theme::SMALL);
    _dramaHighLabel.setFontSize(nui::Theme::SMALL);
    _theoryColumnLabel.setFontSize(nui::Theme::SMALL);
    _aiColumnLabel.setFontSize(nui::Theme::SMALL);
    _aiEmptyHint.setFontSize(nui::Theme::SMALL);

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

    const auto setupViewport = [](juce::Viewport& viewport, juce::Component& content)
    {
        viewport.setViewedComponent(&content, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(kScrollbarThickness);
        viewport.getVerticalScrollBar().setColour(
            juce::ScrollBar::thumbColourId,
            nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));
    };
    setupViewport(_theoryViewport, _theoryListContent);
    setupViewport(_aiViewport, _aiListContent);

    nui::Component::displayBackground(nui::Theme::SECONDARY_BACKGROUND, nui::Theme::getBorderRadius());
    syncLabels();
}

NextChordPanel::~NextChordPanel()
{
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
    // Drama only re-ranks the theory column; AI is pure model output.
    if (!_currentChord)
    {
        repaint();
        return;
    }

    const auto& keyScale = theory::ChordDatabase::getInstance().get(_key, _scale);
    _theoryCandidates = theory::NextChordGenerator::generate(*_currentChord, keyScale, _drama01, _sequence);
    _theoryListContent.rebuildRows(_theoryCandidates);
    _theoryViewport.setViewPosition(0, 0);
    repaint();
}

void NextChordPanel::syncLabels()
{
    _title.setText(juce::translate("next_chord_panel_title").toStdString());
    _dramaLabel.setText(juce::translate("next_chord_drama_label").toStdString());
    _dramaLowLabel.setText(juce::translate("next_chord_drama_smooth").toStdString());
    _dramaHighLabel.setText(juce::translate("next_chord_drama_wild").toStdString());
    _dramaSlider.setTooltip(juce::translate("next_chord_drama_tooltip"));
    _theoryColumnLabel.setText(juce::translate("next_chord_mode_theory").toStdString());
    _aiColumnLabel.setText(juce::translate("next_chord_mode_ai").toStdString());
    _theoryColumnLabel.setTooltip(juce::translate("next_chord_mode_theory_tooltip").toStdString());
    _aiColumnLabel.setTooltip(juce::translate("next_chord_mode_ai_tooltip").toStdString());
}

void NextChordPanel::clear()
{
    _currentChord.reset();
    _sequence = {};
    _theoryCandidates.clear();
    _aiCandidates.clear();
    _currentLabel.setText("");
    _aiEmptyHint.setText("");
    rebuildLists();
    repaint();
}

void NextChordPanel::regenerate()
{
    _theoryCandidates.clear();
    _aiCandidates.clear();
    _aiEmptyHint.setText("");

    if (!_currentChord)
    {
        _currentLabel.setText(juce::translate("next_chord_empty_hint").toStdString());
        rebuildLists();
        repaint();
        return;
    }

    juce::String currentText = juce::translate("next_chord_current_prefix") + " " + _currentChord->readableName;
    if (!_sequence.empty())
        currentText += " - " + juce::translate("next_chord_history_suffix")
            .replace("%n", juce::String(_sequence.size()));
    _currentLabel.setText(currentText.toStdString());

    const auto& keyScale = theory::ChordDatabase::getInstance().get(_key, _scale);

    // Left: pure symbolic ranking (Drama applies here only).
    _theoryCandidates = theory::NextChordGenerator::generate(*_currentChord, keyScale, _drama01, _sequence);

    // Right: pure AI ranking (no theory blend, no Drama).
    if (theory::NextChordAiGenerator::isAvailable())
    {
        _aiCandidates = theory::NextChordAiGenerator::generate(*_currentChord, keyScale, _sequence);
        if (_aiCandidates.empty())
            _aiEmptyHint.setText(juce::translate("next_chord_ai_fallback_hint").toStdString());
    }
    else
    {
        const auto reason = theory::NextChordAiGenerator::unavailableReason();
        _aiEmptyHint.setText(reason.empty()
                                 ? juce::translate("next_chord_ai_fallback_hint").toStdString()
                                 : reason);
    }

    rebuildLists();
    _theoryViewport.setViewPosition(0, 0);
    _aiViewport.setViewPosition(0, 0);
    repaint();
}

void NextChordPanel::rebuildLists()
{
    _theoryListContent.rebuildRows(_theoryCandidates);
    _aiListContent.rebuildRows(_aiCandidates);
}

void NextChordPanel::resized()
{
    Component::resized();
    auto bounds = getLocalBounds().reduced(kPadding);

    _title.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(4);
    _currentLabel.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(4);

    auto dramaRow = bounds.removeFromTop(kDramaRowHeight);
    _dramaLabel.setBounds(dramaRow.removeFromLeft(48));
    _dramaLowLabel.setBounds(dramaRow.removeFromLeft(46));
    _dramaHighLabel.setBounds(dramaRow.removeFromRight(36));
    _dramaSlider.setBounds(dramaRow.reduced(4, 2));

    bounds.removeFromTop(6);

    // Two equal columns: Theory | AI
    auto columns = bounds;
    const int half = (columns.getWidth() - kColumnGap) / 2;
    auto theoryCol = columns.removeFromLeft(half);
    columns.removeFromLeft(kColumnGap);
    auto aiCol = columns;

    auto theoryHeader = theoryCol.removeFromTop(kColumnHeaderHeight);
    _theoryColumnLabel.setBounds(theoryHeader);

    auto aiHeader = aiCol.removeFromTop(kColumnHeaderHeight);
    _aiColumnLabel.setBounds(aiHeader);

    theoryCol.removeFromTop(2);
    aiCol.removeFromTop(2);

    _theoryViewport.setBounds(theoryCol);

    // AI empty hint overlays the AI viewport area when there are no AI rows.
    if (_aiCandidates.empty() && _currentChord.has_value())
    {
        _aiEmptyHint.setVisible(true);
        _aiEmptyHint.setBounds(aiCol);
        _aiViewport.setBounds(aiCol); // still laid out so it can show when populated later
        _aiViewport.setVisible(false);
    }
    else
    {
        _aiEmptyHint.setVisible(false);
        _aiEmptyHint.setBounds({});
        _aiViewport.setVisible(true);
        _aiViewport.setBounds(aiCol);
    }

    const int theoryWidth = juce::jmax(1, _theoryViewport.getMaximumVisibleWidth());
    _theoryListContent.setSize(
        theoryWidth,
        juce::jmax(kRowHeight, static_cast<int>(_theoryCandidates.size()) * kRowHeight));
    _theoryListContent.resized();

    const int aiWidth = juce::jmax(1, _aiViewport.getMaximumVisibleWidth());
    _aiListContent.setSize(
        aiWidth,
        juce::jmax(kRowHeight, static_cast<int>(_aiCandidates.size()) * kRowHeight));
    _aiListContent.resized();
}

void NextChordPanel::paint(juce::Graphics& g)
{
    Component::paint(g);
}

}
