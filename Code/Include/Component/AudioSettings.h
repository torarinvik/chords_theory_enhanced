#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

namespace component
{

// Settings card: mute + live chord naming style (jazz / pop / classical expert dialect).
class AudioSettings : public nui::Component,
                      public nelement::ToggleSwitch::OnValueChangedListener,
                      public nelement::ComboBox::OnValueChangedListener
{
public:
    explicit AudioSettings(const std::string& identifier);
    ~AudioSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void onToggleValueChanged(const std::string& componentID, bool isOn) override;
    void onSelectionChanged(const std::string& componentID, int selectedId) override;
    void syncNamingStylePicker();

    nelement::Text _title { "audio-settings-title", "", juce::translate("audio_settings_title").toStdString() };
    nelement::Text _muteLabel { "audio-settings-mute-label", "", juce::translate("audio_settings_mute_label").toStdString() };
    nelement::ToggleSwitch _muteToggle { "audio-settings-mute-toggle" };

    nelement::Text _namingLabel { "audio-settings-naming-label", "", juce::translate("audio_settings_naming_style_label").toStdString() };
    nelement::ComboBox _namingPicker { "audio-settings-naming-picker" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettings)
};

}
