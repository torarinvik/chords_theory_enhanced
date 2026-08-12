#pragma once

#include <cstdint>
#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/InputMidiNoteTracker.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

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

    // Spelling + roman-numeral context from the key/scale pickers.
    void setSpellKey(theory::Key key);
    void setScale(theory::Scale scale);
    void setKeyAndScale(theory::Key key, theory::Scale scale);

    [[nodiscard]] std::string getDisplayedName() const { return _displayedName; }

private:
    void timerCallback() override;
    void refreshFromTracker(bool force);
    void syncEmptyLabel();

    audio::InputMidiNoteTracker* _tracker = nullptr;
    theory::Key _spellKey = theory::Key::C;
    theory::Scale _scale = theory::Scale::Major;
    std::uint32_t _lastGeneration = 0;
    std::string _displayedName;
    std::string _qualityHint;
    std::string _romanHint;
    std::string _alternateHint;
    std::string _emptyLabel;
    bool _hasChord = false;
    float _confidence = 0.f;
    // Hold the previous name briefly when a low-confidence flip arrives (reduces flicker).
    int _holdFrames = 0;
    std::string _heldName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveChordDisplay)
};

}
