#include "Component/AudioSettings.h"

#include "AppLocalisation.h"
#include "AppSettings.h"

namespace component
{

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

    AppLocalisation::getChangeBroadcaster().addChangeListener(this);
    AppSettings::getChangeBroadcaster().addChangeListener(this);

    _layout.setGap(8.f);
    _layout.setDisplayGrid(false);
    _layout.init({ 1, 1 }, { 1, 4 });

    _layout.setFixedRowHeight(0, 32.f);
    _layout.setFixedRowHeight(1, 36.f);

    _layout.addComponent(_title, 0, 0, 2, 1);
    _layout.addComponent(_muteLabel, 1, 0, 1, 1);
    _layout.addComponent(_muteToggle, 1, 1, 1, 1, 10);

    _layout.setBottomBorder(_title.getComponentID().toStdString(), nui::Theme::newColor(nui::Theme::ThemeColor::BORDER).asJuce());
}

AudioSettings::~AudioSettings()
{
    _muteToggle.removeListener(this);
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
        return;
    }

    if (source != &AppLocalisation::getChangeBroadcaster())
        return;

    _title.setText(juce::translate("audio_settings_title").toStdString());
    _muteLabel.setText(juce::translate("audio_settings_mute_label").toStdString());
    _muteLabel.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());
    _muteToggle.setHelpText(juce::translate("audio_settings_mute_tooltip").toStdString());
    repaint();
}

}
