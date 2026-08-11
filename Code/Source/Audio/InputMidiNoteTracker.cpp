#include "Audio/InputMidiNoteTracker.h"

namespace audio
{

void InputMidiNoteTracker::processHostMidi(const juce::MidiBuffer& midiMessages)
{
    auto changed = false;

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();
        const auto note = message.getNoteNumber();
        if (note < 0 || note >= kNumMidiNotes)
            continue;

        if (message.isNoteOn(false)) // false = velocity 0 is not note-on
        {
            const auto wasHeld = _held[static_cast<std::size_t>(note)].exchange(true, std::memory_order_relaxed);
            changed = changed || !wasHeld;
        }
        else if (message.isNoteOff() || (message.isNoteOn() && message.getVelocity() == 0))
        {
            const auto wasHeld = _held[static_cast<std::size_t>(note)].exchange(false, std::memory_order_relaxed);
            changed = changed || wasHeld;
        }
        else if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            for (auto& flag : _held)
            {
                if (flag.exchange(false, std::memory_order_relaxed))
                    changed = true;
            }
        }
    }

    if (changed)
        _generation.fetch_add(1, std::memory_order_relaxed);
}

void InputMidiNoteTracker::clear()
{
    auto changed = false;
    for (auto& flag : _held)
    {
        if (flag.exchange(false, std::memory_order_relaxed))
            changed = true;
    }
    if (changed)
        _generation.fetch_add(1, std::memory_order_relaxed);
}

bool InputMidiNoteTracker::isNoteHeld(int midiNote) const noexcept
{
    if (midiNote < 0 || midiNote >= kNumMidiNotes)
        return false;
    return _held[static_cast<std::size_t>(midiNote)].load(std::memory_order_relaxed);
}

}
