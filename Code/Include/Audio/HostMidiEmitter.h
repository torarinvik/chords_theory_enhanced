#pragma once

#include <mutex>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// UI-thread → audio-thread bridge that injects note-on/off into the host-bound MidiBuffer so a
// DAW can route this plugin's MIDI output to another track. Separate from ChordSynthEngine's
// preview path (which uses MidiKeyboardState for the internal synth only).
//
// Contract:
//  - playChord() / allNotesOff() message thread only
//  - renderNextBlock() audio thread only
class HostMidiEmitter
{
public:
    void prepare(double sampleRate);

    // Message thread: schedule a one-shot chord for host MIDI out (note-ons at next block,
    // note-offs after durationMs). Replaces any still-sounding previous schedule.
    void playChord(const std::vector<int>& midiNotes, int durationMs = 1000);

    // Message thread: silence anything currently scheduled/sounding on the host MIDI out.
    void allNotesOff();

    // Audio thread: append host MIDI events into midiOut for this block.
    void renderNextBlock(juce::MidiBuffer& midiOut, int numSamples);

private:
    static constexpr int kMidiChannel = 1;
    static constexpr int kMaxNotes = 16;
    static constexpr juce::uint8 kVelocity = 100;

    double _sampleRate = 44100.0;

    std::mutex _mutex;
    bool _hasPending = false;
    bool _pendingAllNotesOff = false;
    int _pendingCount = 0;
    int _pendingNotes[kMaxNotes] {};
    int _pendingDurationSamples = 0;

    int _activeNotes[kMaxNotes] {};
    int _activeCount = 0;
    int _samplesUntilNoteOff = 0;
    bool _noteOffPending = false;
};

}
