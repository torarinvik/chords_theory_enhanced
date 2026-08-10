#pragma once

#include <nierika_dsp/nierika_dsp.h>

#include "Component/LanguageSettings.h"
#include "Component/VisualSettings.h"

namespace component
{

class SettingsWindow : public nlayout::Window,
                        public nelement::SVGButton::OnClickListener
{
public:
    SettingsWindow(const std::string& identifier, nlayout::WindowsManager& windowsManager);
    ~SettingsWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    void onButtonClick(const std::string& componentID) override;

private:
    void close();
    [[nodiscard]] juce::Rectangle<int> getCardBounds();
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    nlayout::WindowsManager& _windowsManager;

    nelement::Text _title { "settings-title", "", "Settings" };
    nelement::SVGButton _titleIcon { "settings-title-icon", nui::Icons::getGear() };
    nelement::SVGButton _closeButton { "settings-close-button", nui::Icons::getCross() };

    VisualSettings _visualSettings { "settings-visual" };
    LanguageSettings _languageSettings { "settings-language" };

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsWindow)
};

}
