#pragma once

#include <cstdint>
#include <string>

#include <nierika_dsp/nierika_dsp.h>

#include "Audio/InputMidiNoteTracker.h"
#include "Theory/ChordExpert.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace component
{

// Live chord expert readout: names held host MIDI using ChordExpert (detector + progression
// context + naming style). Shows roman/function, alternates, and a short explanation.
class LiveChordDisplay : public nui::Component, private juce::Timer
{
public:
    explicit LiveChordDisplay(const std::string& identifier,
                              audio::InputMidiNoteTracker* inputMidiNoteTracker = nullptr);
    ~LiveChordDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    void setSpellKey(theory::Key key);
    void setScale(theory::Scale scale);
    void setKeyAndScale(theory::Key key, theory::Scale scale);

    // Progression / audition history for context-aware naming (oldest → newest).
    void setExpertContext(theory::ChordExpertContext context);

    [[nodiscard]] std::string getDisplayedName() const { return _displayedName; }
    [[nodiscard]] std::string getExplanation() const { return _explanation; }

private:
    void timerCallback() override;
    void refreshFromTracker(bool force);
    void applyResult(const theory::ChordExpertResult& result);
    void syncEmptyLabel();
    void syncStyleFromSettings();

    audio::InputMidiNoteTracker* _tracker = nullptr;
    theory::ChordExpertContext _context;
    std::uint32_t _lastGeneration = 0;
    std::uint32_t _lastHarmonyId = 0;
    std::string _displayedName;
    std::string _qualityHint;
    std::string _romanHint;
    std::string _alternateHint;
    std::string _explanation;
    std::string _emptyLabel;
    bool _hasChord = false;
    float _confidence = 0.f;
    std::string _pendingName;
    int _pendingFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveChordDisplay)
};

}
