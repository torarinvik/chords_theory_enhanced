#pragma once

#include <array>
#include <atomic>
#include <cstdint>

#include <juce_audio_basics/juce_audio_basics.h>

namespace audio
{

// Tracks which MIDI notes are currently held from *host/input* MIDI (not progression or UI
// preview injects). Updated on the audio thread; polled from the message thread for piano
// highlighting. Generation counter bumps on every note-on/off so the UI can repaint only when
// something changed.
class InputMidiNoteTracker
{
public:
    // Audio thread only. Call on the host midi buffer *before* progression/preview notes are
    // injected into it, so only external input is tracked.
    void processHostMidi(const juce::MidiBuffer& midiMessages);

    // Message or audio thread. All notes off (e.g. reset / transport stop if desired).
    void clear();

    [[nodiscard]] bool isNoteHeld(int midiNote) const noexcept;
    [[nodiscard]] std::uint32_t getGeneration() const noexcept
    {
        return _generation.load(std::memory_order_relaxed);
    }

private:
    static constexpr int kNumMidiNotes = 128;

    std::array<std::atomic<bool>, kNumMidiNotes> _held {};
    std::atomic<std::uint32_t> _generation { 0 };
};

}
