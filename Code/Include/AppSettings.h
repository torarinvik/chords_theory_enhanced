#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>
#include <nierika_dsp/nierika_dsp.h>

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
