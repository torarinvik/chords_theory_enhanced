#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>

namespace component
{

// Settings card section: mute the plugin's internal audio output.
class AudioSettings : public nui::Component,
                      public nelement::ToggleSwitch::OnValueChangedListener
{
public:
    explicit AudioSettings(const std::string& identifier);
    ~AudioSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void onToggleValueChanged(const std::string& componentID, bool isOn) override;

    nelement::Text _title { "audio-settings-title", "", juce::translate("audio_settings_title").toStdString() };
    nelement::Text _muteLabel { "audio-settings-mute-label", "", juce::translate("audio_settings_mute_label").toStdString() };
    nelement::ToggleSwitch _muteToggle { "audio-settings-mute-toggle" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioSettings)
};

}
