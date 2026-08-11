#include "Component/KeyScaleSelector.h"

#include <algorithm>

#include "AppLocalisation.h"

namespace component
{

KeyScaleSelector::KeyScaleSelector(const std::string& identifier):
    Component(identifier)
{
    _searchInput.setPlaceholder(juce::translate("key_scale_selector_search_placeholder").toStdString());
    _searchInput.setHeightType(nui::Theme::HeightType::THIN);
    _searchInput.setRounded(true);
    _searchInput.addOnValueChangedListener(this);

    _searchModeSwitch.addOnValueChangedListener(this);
    _searchModeSwitch.setSelectedIndex(static_cast<int>(_searchMode), juce::dontSendNotification);
    _searchModeSwitch.setSelectedInvertedTextColor(true);
    _searchModeSwitch.setHeightType(nui::Theme::HeightType::THIN);
    _searchModeSwitch.setRounded(true);

    _searchAllLabel.setText(juce::translate("key_scale_selector_search_all_label").toStdString());
    _searchAllLabel.setFontSize(nui::Theme::LABEL);
    _searchAllLabel.setJustificationType(juce::Justification::centredRight);
    _searchAllLabel.setHelpText(juce::translate("key_scale_selector_search_all_tooltip").toStdString());

    _searchAllToggle.addOnValueChangedListener(this);
    _searchAllToggle.setToggleState(_searchScope == SearchScope::All, juce::dontSendNotification);
    _searchAllToggle.setHelpText(juce::translate("key_scale_selector_search_all_tooltip").toStdString());
    // ToggleSwitch paints into its full bounds; without a fixed size it fills the 60px header
    // row and looks oversized. Keep a compact ~2:1 pill, centred in the cell.
    _searchAllToggle.setFixedWidth(36.f);
    _searchAllToggle.setFixedHeight(18.f);
    _searchAllToggle.setVerticalAlignment(nui::Component::CENTER);
    _searchAllToggle.setHorizontalAlignment(nui::Component::CENTER);

    for (int i = 0; i < theory::kNumKeys; ++i)
        _keyPicker.addItem(theory::getKeyLabel(static_cast<theory::Key>(i)), i + 1);

    for (int i = 0; i < theory::kNumScales; ++i)
        _scalePicker.addItem(juce::translate(theory::getScaleTranslationKey(static_cast<theory::Scale>(i))), i + 1);

    _keyPicker.setSelectedId(static_cast<int>(_currentKey) + 1, juce::dontSendNotification);
    _scalePicker.setSelectedId(static_cast<int>(_currentScale) + 1, juce::dontSendNotification);

    _keyPicker.addOnValueChangedListener(this);
    _scalePicker.addOnValueChangedListener(this);

    _keyPicker.setHeightType(nui::Theme::HeightType::THIN);
    _scalePicker.setHeightType(nui::Theme::HeightType::THIN);

    _keyLabel.setText(juce::translate("key_scale_selector_key_label").toStdString());
    _scaleLabel.setText(juce::translate("key_scale_selector_scale_label").toStdString());
    _keyLabel.setFontSize(nui::Theme::LABEL);
    _scaleLabel.setFontSize(nui::Theme::LABEL);
    _keyLabel.setJustificationType(juce::Justification::centredRight);
    _scaleLabel.setJustificationType(juce::Justification::centredRight);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    // search | Chord/Scale | All label | All toggle | Key label | Key | Scale label | Scale
    _layout.init({ 1 }, { 4, 3, 1, 1, 1, 2, 1, 2 });

    _layout.setFixedColumnWidth(2, 28.f);
    _layout.setFixedColumnWidth(3, 44.f);
    _layout.setFixedColumnWidth(4, 36.f);
    _layout.setFixedColumnWidth(5, 72.f);
    _layout.setFixedColumnWidth(6, 48.f);
    _layout.setFixedColumnWidth(7, 140.f);

    _layout.addComponent(_searchInput, 0, 0, 1, 1);
    _layout.addComponent(_searchModeSwitch, 0, 1, 1, 1);
    _layout.addComponent(_searchAllLabel, 0, 2, 1, 1);
    _layout.addComponent(_searchAllToggle, 0, 3, 1, 1);
    _layout.addComponent(_keyLabel, 0, 4, 1, 1);
    _layout.addComponent(_keyPicker, 0, 5, 1, 1);
    _layout.addComponent(_scaleLabel, 0, 6, 1, 1);
    _layout.addComponent(_scalePicker, 0, 7, 1, 1);
}

KeyScaleSelector::~KeyScaleSelector()
{
    _searchInput.removeOnValueChangedListener(this);
    _searchModeSwitch.removeListener(this);
    _searchAllToggle.removeListener(this);
    _keyPicker.removeListener(this);
    _scalePicker.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
}

void KeyScaleSelector::paint(juce::Graphics& g)
{
    Component::paint(g);

    _layout.paint(g);
}

void KeyScaleSelector::resized()
{
    Component::resized();

    _layout.resized();
}

void KeyScaleSelector::setKeyAndScale(theory::Key key, theory::Scale scale)
{
    _currentKey = key;
    _currentScale = scale;

    _keyPicker.setSelectedId(static_cast<int>(_currentKey) + 1, juce::dontSendNotification);
    _scalePicker.setSelectedId(static_cast<int>(_currentScale) + 1, juce::dontSendNotification);
}

void KeyScaleSelector::addListener(Listener* listener)
{
    _listeners.push_back(listener);
}

void KeyScaleSelector::removeListener(Listener* listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void KeyScaleSelector::onSelectionChanged(const std::string& componentID, int selectedId)
{
    if (componentID == _searchModeSwitch.getComponentID())
    {
        _searchMode = selectedId == 1 ? SearchMode::Scale : SearchMode::Chord;
        notifySearchListeners();
        return;
    }

    juce::ignoreUnused(selectedId);

    _currentKey = static_cast<theory::Key>(_keyPicker.getSelectedId() - 1);
    _currentScale = static_cast<theory::Scale>(_scalePicker.getSelectedId() - 1);

    notifyKeyScaleListeners();
}

void KeyScaleSelector::onValueChanged(const std::string& componentID, const std::string& newValue)
{
    juce::ignoreUnused(componentID, newValue);
    notifySearchListeners();
}

void KeyScaleSelector::onToggleValueChanged(const std::string& componentID, bool isOn)
{
    if (componentID != _searchAllToggle.getComponentID())
        return;

    _searchScope = isOn ? SearchScope::All : SearchScope::Predicted;
    notifySearchListeners();
}

void KeyScaleSelector::notifyKeyScaleListeners()
{
    for (auto* listener : _listeners)
        listener->onKeyScaleChanged(_currentKey, _currentScale);
}

void KeyScaleSelector::notifySearchListeners()
{
    const auto query = _searchInput.getText();
    for (auto* listener : _listeners)
        listener->onSearchChanged(query, _searchMode, _searchScope);
}

void KeyScaleSelector::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    const auto selectedScale = _currentScale;

    _scalePicker.clear(juce::dontSendNotification);
    for (int i = 0; i < theory::kNumScales; ++i)
        _scalePicker.addItem(juce::translate(theory::getScaleTranslationKey(static_cast<theory::Scale>(i))), i + 1);
    _scalePicker.setSelectedId(static_cast<int>(selectedScale) + 1, juce::dontSendNotification);

    _keyLabel.setText(juce::translate("key_scale_selector_key_label").toStdString());
    _scaleLabel.setText(juce::translate("key_scale_selector_scale_label").toStdString());
    _searchInput.setPlaceholder(juce::translate("key_scale_selector_search_placeholder").toStdString());
    _searchModeSwitch.setLabels(
        juce::translate("key_scale_selector_search_chord").toStdString(),
        juce::translate("key_scale_selector_search_scale").toStdString());
    _searchAllLabel.setText(juce::translate("key_scale_selector_search_all_label").toStdString());
    _searchAllLabel.setHelpText(juce::translate("key_scale_selector_search_all_tooltip").toStdString());
    _searchAllToggle.setHelpText(juce::translate("key_scale_selector_search_all_tooltip").toStdString());

    repaint();
}

}
