#include "Component/ScaleSuggestionPanel.h"

#include "AppLocalisation.h"

namespace component
{

// -----------------------------------------------------------------------------
// Row
// -----------------------------------------------------------------------------

ScaleSuggestionPanel::Row::Row(const std::string& identifier,
                               ScaleSuggestionPanel& owner,
                               theory::ScaleSuggestion suggestion,
                               int rowIndex):
    Component(identifier),
    _owner(owner),
    _suggestion(std::move(suggestion)),
    _rowIndex(rowIndex)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void ScaleSuggestionPanel::Row::setSuggestion(theory::ScaleSuggestion suggestion)
{
    _suggestion = std::move(suggestion);
    repaint();
}

void ScaleSuggestionPanel::Row::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    if ((_rowIndex % 2) == 0)
    {
        g.setColour(nui::Theme::newColor(nui::Theme::BACKGROUND).asJuce().withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.toFloat().reduced(0.0f, 1.0f), 4.0f);
    }

    auto content = bounds.reduced(8, 0);
    auto fitArea = content.removeFromRight(juce::jmin(56, content.getWidth() / 4));
    auto nameArea = content.removeFromLeft(juce::jmin(140, content.getWidth() / 2));
    auto reasonArea = content;

    g.setFont(nui::Theme::newFont(nui::Theme::REGULAR, nui::Theme::SMALL));
    g.setColour(nui::Theme::newColor(nui::Theme::TEXT).asJuce());
    g.drawText(_suggestion.label, nameArea.reduced(2, 0), juce::Justification::centredLeft, true);

    g.setColour(nui::Theme::newColor(nui::Theme::DISABLED).asJuce());
    g.drawText(_suggestion.reasonLabel, reasonArea.reduced(4, 0), juce::Justification::centredLeft, true);

    g.setColour(nui::Theme::newColor(nui::Theme::ACCENT).asJuce());
    g.drawText("F " + juce::String(_suggestion.fitPercent), fitArea.reduced(2, 0),
        juce::Justification::centredRight, false);
}

void ScaleSuggestionPanel::Row::mouseUp(const juce::MouseEvent& event)
{
    if (!event.mouseWasClicked())
        return;

    if (_owner._onScaleChosen)
        _owner._onScaleChosen(_suggestion.key, _suggestion.scale);
}

// -----------------------------------------------------------------------------
// ListContent
// -----------------------------------------------------------------------------

ScaleSuggestionPanel::ListContent::ListContent(ScaleSuggestionPanel& owner):
    _owner(owner)
{
}

void ScaleSuggestionPanel::ListContent::rebuildRows(const std::vector<theory::ScaleSuggestion>& suggestions)
{
    _rows.clear();
    removeAllChildren();

    _rows.reserve(suggestions.size());
    for (std::size_t i = 0; i < suggestions.size(); ++i)
    {
        auto row = std::make_unique<Row>(
            "scale-suggestion-row-" + std::to_string(i),
            _owner,
            suggestions[i],
            static_cast<int>(i));
        addAndMakeVisible(*row);
        _rows.push_back(std::move(row));
    }

    const int height = juce::jmax(kRowHeight, static_cast<int>(_rows.size()) * kRowHeight);
    setSize(juce::jmax(1, getParentWidth() > 0 ? getParentWidth() : getWidth()), height);
    resized();
}

void ScaleSuggestionPanel::ListContent::resized()
{
    auto bounds = getLocalBounds();
    for (auto& row : _rows)
        row->setBounds(bounds.removeFromTop(kRowHeight));
}

// -----------------------------------------------------------------------------
// ScaleSuggestionPanel
// -----------------------------------------------------------------------------

ScaleSuggestionPanel::ScaleSuggestionPanel(const std::string& identifier):
    Component(identifier)
{
    addAndMakeVisible(_title);
    addAndMakeVisible(_subtitle);
    addAndMakeVisible(_emptyHint);
    addAndMakeVisible(_viewport);

    _title.setFontSize(nui::Theme::LABEL);
    _subtitle.setFontSize(nui::Theme::SMALL);
    _emptyHint.setFontSize(nui::Theme::SMALL);

    _viewport.setViewedComponent(&_listContent, false);
    _viewport.setScrollBarsShown(true, false);
    _viewport.setScrollBarThickness(kScrollbarThickness);
    _viewport.getVerticalScrollBar().setColour(
        juce::ScrollBar::thumbColourId,
        nui::Theme::newColor(nui::Theme::ThemeColor::BACKGROUND).asJuce().withAlpha(0.5f));

    nui::Component::displayBackground(nui::Theme::SECONDARY_BACKGROUND, nui::Theme::getBorderRadius());
    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    syncLabels();
    regenerate();
}

ScaleSuggestionPanel::~ScaleSuggestionPanel()
{
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void ScaleSuggestionPanel::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _key = key;
    _scale = scale;
    regenerate();
}

void ScaleSuggestionPanel::setCurrentChord(const std::optional<theory::Chord>& chord)
{
    _currentChord = chord;
    regenerate();
}

void ScaleSuggestionPanel::setSearchQuery(const std::string& query)
{
    if (_query == query)
        return;
    _query = query;
    regenerate();
}

void ScaleSuggestionPanel::setSearchScope(theory::NextScaleGenerator::Pool pool)
{
    if (_pool == pool)
        return;
    _pool = pool;
    regenerate();
}

void ScaleSuggestionPanel::syncLabels()
{
    _title.setText(juce::translate("scale_suggestion_panel_title").toStdString());
    _emptyHint.setText(juce::translate("scale_suggestion_empty").toStdString());
}

void ScaleSuggestionPanel::regenerate()
{
    _suggestions = theory::NextScaleGenerator::generate(
        _key, _scale, _currentChord, _pool, _query, 16);

    const auto currentLabel = juce::translate(theory::getScaleTranslationKey(_scale)).toStdString();
    _subtitle.setText(
        juce::translate("scale_suggestion_current_prefix").toStdString()
            + " " + theory::getKeyLabel(_key) + " " + currentLabel);

    _listContent.rebuildRows(_suggestions);
    _viewport.setViewPosition(0, 0);
    _emptyHint.setVisible(_suggestions.empty());
    repaint();
    resized();
}

void ScaleSuggestionPanel::paint(juce::Graphics& g)
{
    Component::paint(g);
}

void ScaleSuggestionPanel::resized()
{
    Component::resized();

    auto bounds = getLocalBounds().reduced(kPadding);
    _title.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(2);
    _subtitle.setBounds(bounds.removeFromTop(16));
    bounds.removeFromTop(4);

    if (_suggestions.empty())
    {
        _viewport.setBounds({});
        _emptyHint.setBounds(bounds);
    }
    else
    {
        _emptyHint.setBounds({});
        _viewport.setBounds(bounds);
        _listContent.setSize(_viewport.getWidth() - kScrollbarThickness,
            juce::jmax(kRowHeight, static_cast<int>(_suggestions.size()) * kRowHeight));
    }
}

void ScaleSuggestionPanel::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);
    if (source == &AppLocalisation::getChangeBroadcaster())
    {
        syncLabels();
        regenerate();
    }
}

}
