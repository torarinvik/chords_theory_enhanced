#pragma once

#include <nierika_dsp/nierika_dsp.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_extra/juce_gui_extra.h>

namespace component
{

// Clickable colour chip used by VisualSettings.
class ColourSwatch : public juce::Component
{
public:
    explicit ColourSwatch(const std::string& componentId);
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
    enum class ColourTarget
    {
        NoteText,
        ScaleHighlight,
        ChordHighlight,
        MidiInputHighlight,
    };

    void onSelectionChanged(const std::string& componentID, int selectedIndex) override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void openColourPicker(ColourTarget target, ColourSwatch& swatch, juce::Colour current);

    nelement::Text _title { "visual-settings-title", "", juce::translate("visual_settings_title").toStdString() };

    nelement::Text _themeLabel { "settings-theme-label", "", juce::translate("visual_settings_theme_label").toStdString() };
    nelement::TwoWaySwitch _themeSwitch { "settings-theme-toggle", juce::translate("visual_settings_dark_theme").toStdString(), juce::translate("visual_settings_light_theme").toStdString() };

    nelement::Text _noteTextColourLabel { "settings-note-text-colour-label", "", juce::translate("visual_settings_note_text_colour_label").toStdString() };
    ColourSwatch _noteTextColourSwatch { "settings-note-text-colour-swatch" };

    nelement::Text _chordHighlightColourLabel { "settings-chord-highlight-colour-label", "", juce::translate("visual_settings_chord_highlight_colour_label").toStdString() };
    ColourSwatch _chordHighlightColourSwatch { "settings-chord-highlight-colour-swatch" };

    nelement::Text _scaleHighlightColourLabel { "settings-scale-highlight-colour-label", "", juce::translate("visual_settings_scale_highlight_colour_label").toStdString() };
    ColourSwatch _scaleHighlightColourSwatch { "settings-scale-highlight-colour-swatch" };

    nelement::Text _midiInputHighlightColourLabel { "settings-midi-input-highlight-colour-label", "", juce::translate("visual_settings_midi_input_highlight_colour_label").toStdString() };
    ColourSwatch _midiInputHighlightColourSwatch { "settings-midi-input-highlight-colour-swatch" };

    ColourTarget _activeColourTarget = ColourTarget::NoteText;
    juce::Component::SafePointer<juce::ColourSelector> _activeColourSelector;

    nlayout::GridLayout<nui::Component> _layout { *this };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualSettings)
};

}
