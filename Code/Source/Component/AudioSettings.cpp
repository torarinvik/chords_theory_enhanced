#include "Component/AudioSettings.h"

#include "AppLocalisation.h"
#include "AppSettings.h"
#include "Theory/ChordExpert.h"

namespace component
{

namespace
{
    constexpr int kStyleJazzId = 1;
    constexpr int kStylePopId = 2;
    constexpr int kStyleClassicalId = 3;

    int styleToId(theory::ChordNamingStyle style)
    {
        switch (style)
        {
            case theory::ChordNamingStyle::PopSlash: return kStylePopId;
            case theory::ChordNamingStyle::Classical: return kStyleClassicalId;
            case theory::ChordNamingStyle::JazzChart:
            default: return kStyleJazzId;
        }
    }

    theory::ChordNamingStyle idToStyle(int id)
    {
        if (id == kStylePopId)
            return theory::ChordNamingStyle::PopSlash;
        if (id == kStyleClassicalId)
            return theory::ChordNamingStyle::Classical;
        return theory::ChordNamingStyle::JazzChart;
    }
}

AudioSettings::AudioSettings(const std::string& identifier):
    Component(identifier)
{
    _title.setFontSize(nui::Theme::SMALL);
    _title.setColor(nui::Theme::ThemeColor::DISABLED);
    _title.setJustificationType(juce::Justification::centredLeft);

    _muteLabel.setJustificationType(juce::Justification::centredLeft);
    _muteLabel.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());

    _muteToggle.setToggleState(AppSettings::getInstance().getMuted(), juce::dontSendNotification);
    _muteToggle.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());
    _muteToggle.setFixedWidth(36.f);
    _muteToggle.setFixedHeight(18.f);
    _muteToggle.setVerticalAlignment(nui::Component::CENTER);
    _muteToggle.setHorizontalAlignment(nui::Component::START);
    _muteToggle.addOnValueChangedListener(this);

    _namingLabel.setJustificationType(juce::Justification::centredLeft);
    _namingLabel.setHelpText(juce::translate("audio_settings_naming_style_tooltip").toStdString());

    _namingPicker.addItem(juce::translate("audio_settings_naming_jazz").toStdString(), kStyleJazzId);
    _namingPicker.addItem(juce::translate("audio_settings_naming_pop").toStdString(), kStylePopId);
    _namingPicker.addItem(juce::translate("audio_settings_naming_classical").toStdString(), kStyleClassicalId);
    _namingPicker.setSelectedId(styleToId(AppSettings::getInstance().getChordNamingStyle()), juce::dontSendNotification);
    _namingPicker.addOnValueChangedListener(this);
    _namingPicker.setSelectedInvertedTextColor(true);
    _namingPicker.setHeightType(nui::Theme::HeightType::THIN);

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    AppSettings::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);
    _layout.setFixedRowHeight(1, 36.f);
    _layout.setFixedRowHeight(2, 36.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_muteLabel, 1, 0, 1, 1);
    _layout.addComponent(_muteToggle, 1, 1, 1, 1, 10);
    _layout.addComponent(_namingLabel, 2, 0, 1, 1);
    _layout.addComponent(_namingPicker, 2, 1, 1, 1, 10);

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

AudioSettings::~AudioSettings()
{
    _muteToggle.removeListener(this);
    _namingPicker.removeListener(this);
    AppLocalisation::getChangeBroadcaster().removeChangeListener(this);
    AppSettings::getChangeBroadcaster().removeChangeListener(this);
}

void AudioSettings::paint(juce::Graphics& g)
{
    Component::paint(g);
    _layout.paint(g);
}

void AudioSettings::resized()
{
    Component::resized();
    _layout.resized();
}

void AudioSettings::onToggleValueChanged(const std::string& componentID, bool isOn)
{
    if (componentID != _muteToggle.getComponentID())
        return;

    AppSettings::getInstance().setMuted(isOn);
}

void AudioSettings::onSelectionChanged(const std::string& componentID, int selectedId)
{
    if (componentID != _namingPicker.getComponentID())
        return;

    AppSettings::getInstance().setChordNamingStyle(idToStyle(selectedId));
}

void AudioSettings::syncNamingStylePicker()
{
    _namingPicker.setSelectedId(styleToId(AppSettings::getInstance().getChordNamingStyle()), juce::dontSendNotification);
}

void AudioSettings::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    Component::changeListenerCallback(source);

    if (source == &nui::Theme::getChangeBroadcaster())
    {
        _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
        repaint();
        return;
    }

    if (source == &AppSettings::getChangeBroadcaster())
    {
        _muteToggle.setToggleState(AppSettings::getInstance().getMuted(), juce::dontSendNotification);
        syncNamingStylePicker();
        return;
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("audio_settings_title").toStdString());
    _muteLabel.setText(juce::translate("audio_settings_mute_label").toStdString());
    _muteLabel.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());
    _muteToggle.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());
    _namingLabel.setText(juce::translate("audio_settings_naming_style_label").toStdString());
    _namingLabel.setHelpText(juce::translate("audio_settings_naming_style_tooltip").toStdString());

    const auto selected = _namingPicker.getSelectedId();
    _namingPicker.clear(juce::dontSendNotification);
    _namingPicker.addItem(juce::translate("audio_settings_naming_jazz").toStdString(), kStyleJazzId);
    _namingPicker.addItem(juce::translate("audio_settings_naming_pop").toStdString(), kStylePopId);
    _namingPicker.addItem(juce::translate("audio_settings_naming_classical").toStdString(), kStyleClassicalId);
    _namingPicker.setSelectedId(selected > 0 ? selected : kStyleJazzId, juce::dontSendNotification);
    repaint();
}

}
