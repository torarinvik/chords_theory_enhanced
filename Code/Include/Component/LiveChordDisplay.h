#pragma once

#include <cstdint>
#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/InputMidiNoteTracker.h"
#include "Theory/Key.h"

namespace component
{

// Shows the name of the chord currently held on host/controller MIDI input.
// Polls InputMidiNoteTracker on a timer (same cadence as piano input highlights).
class LiveChordDisplay : public nui::Component, private juce::Timer
{
public:
    // tracker may be null (tests / no audio) — display stays empty.
    explicit LiveChordDisplay(const std::string& identifier,
                              audio::InputMidiNoteTracker* inputMidiNoteTracker = nullptr);
    ~LiveChordDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // Spelling context for enharmonics (C# vs Db) — pass the current key picker selection.
    void setSpellKey(theory::Key key);

    [[nodiscard]] std::string getDisplayedName() const { return _displayedName; }

private:
    void timerCallback() override;
    void refreshFromTracker(bool force);
    void syncEmptyLabel();

    audio::InputMidiNoteTracker* _tracker = nullptr;
    theory::Key _spellKey = theory::Key::C;
    std::uint32_t _lastGeneration = 0;
    std::string _displayedName;
    std::string _emptyLabel;
    bool _hasChord = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveChordDisplay)
};

}
