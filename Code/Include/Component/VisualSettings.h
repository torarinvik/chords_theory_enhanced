#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace component
{

// Clickable colour chip used by VisualSettings for note-text colour.
class NoteTextColourSwatch : public juce::Component
{
public:
    explicit NoteTextColourSwatch(const std::string& componentId);
    void setColour(juce::Colour colour);
    [[nodiscard]] juce::Colour getColour() const { return _colour; }

    std::function<void()> onClick;

    void paint(juce::Graphics& g) override;
    void mouseUp(const juce::MouseEvent& event) override;

private:
    juce::Colour _colour { 0xFF6A6E76 };
};

class VisualSettings : public nui::Component,
                       public nelement::TwoWaySwitch::OnValueChangedListener
{
public:
    explicit VisualSettings(const std::string& identifier);
    ~VisualSettings() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void onSelectionChanged(const std::string& componentID, int selectedIndex) override;
    // Handles Theme, AppSettings, AppLocalisation, and the temporary ColourSelector call-out.
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void openNoteTextColourPicker();

    nelement::Text _title { "visual-settings-title", "", juce::translate("visual_settings_title").toStdString() };

    nelement::Text _themeLabel { "settings-theme-label", "", juce::translate("visual_settings_theme_label").toStdString() };
    nelement::TwoWaySwitch _themeSwitch { "settings-theme-toggle", juce::translate("visual_settings_dark_theme").toStdString(), juce::translate("visual_settings_light_theme").toStdString() };

    nelement::Text _noteTextColourLabel { "settings-note-text-colour-label", "", juce::translate("visual_settings_note_text_colour_label").toStdString() };
    NoteTextColourSwatch _noteTextColourSwatch { "settings-note-text-colour-swatch" };

    // Owned while the call-out is open; cleared when the selector is destroyed with the box.
    juce::Component::SafePointer<juce::ColourSelector> _activeColourSelector;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualSettings)
};

}
