#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <nierika_dsp/nierika_dsp.h>

#include "Theory/ChordExpert.h"

class AppSettings
{
public:
    explicit AppSettings(const juce::File& settingsFile);

    // Standalone-only: whether the app name is drawn centered over the native title bar (see
    // StandaloneApp.cpp) - has no effect on AU/VST3/AUv3, which don't have a title bar at all.
    [[nodiscard]] bool getShowStandaloneTitle() const;
    void setShowStandaloneTitle(bool show);

    [[nodiscard]] nui::Theme::Mode getThemeMode() const;
    void setThemeMode(nui::Theme::Mode mode);

    [[nodiscard]] std::string getLanguage() const;
    void setLanguage(const std::string& languageCode);

    // Colour used for piano / pitch note letter labels (white + black keys). Default is a mid grey
    // that reads on ivory keys; black keys auto-brighten dark choices for legibility when painting.
    [[nodiscard]] juce::Colour getNoteTextColour() const;
    void setNoteTextColour(juce::Colour colour);

    // Colour used to highlight scale tones on the progression piano (under the playhead's chord
    // scale). Distinct from chord highlight by default (soft green).
    [[nodiscard]] juce::Colour getScaleHighlightColour() const;
    void setScaleHighlightColour(juce::Colour colour);

    // Colour used to highlight chord tones on the progression piano. Defaults to theme-like blue.
    [[nodiscard]] juce::Colour getChordHighlightColour() const;
    void setChordHighlightColour(juce::Colour colour);

    // Colour used to fully light piano keys while those MIDI notes are held from host/controller
    // input. Defaults to a warm amber distinct from chord (blue) and scale (green) highlights.
    [[nodiscard]] juce::Colour getMidiInputHighlightColour() const;
    void setMidiInputHighlightColour(juce::Colour colour);

    // When true, the plugin's audio output is silenced (MIDI out / piano highlight still run).
    // Default is unmuted. Safe for the audio thread via isOutputMuted().
    [[nodiscard]] bool getMuted() const;
    void setMuted(bool muted);

    // Audio-thread-safe mute flag, kept in sync with getInstance()'s getMuted().
    [[nodiscard]] static bool isOutputMuted() noexcept;

    // Live chord naming dialect (jazz chart / pop slash / classical). Global, like mute.
    [[nodiscard]] theory::ChordNamingStyle getChordNamingStyle() const;
    void setChordNamingStyle(theory::ChordNamingStyle style);

    // Fired after any setter that should refresh UI (theme is also announced via nui::Theme).
    static juce::ChangeBroadcaster& getChangeBroadcaster();

    static AppSettings& getInstance();

    // Writable per-app support directory (~/Library/<CompanyName>/<AppName> on macOS) this app
    // uses for all local, writable state - shared by settings storage and any other per-app
    // writable files a downstream plugin might add.
    [[nodiscard]] static juce::File getAppSupportDirectory();

private:
    juce::InterProcessLock _processLock { "chords-theory-enhanced-settings" };
    juce::PropertiesFile _properties;
};
