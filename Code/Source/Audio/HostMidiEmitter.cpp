#include "Audio/HostMidiEmitter.h"

#include <cmath>

namespace audio
{

void HostMidiEmitter::prepare(double sampleRate)
{
    _sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
}

void HostMidiEmitter::playChord(const std::vector<int>& midiNotes, int durationMs)
{
    const int durationSamples = juce::jmax(1, static_cast<int>(std::round(
        (_sampleRate * static_cast<double>(juce::jmax(1, durationMs))) / 1000.0)));

    const std::lock_guard lock(_mutex);
    _pendingAllNotesOff = false;
    _pendingCount = 0;
    for (const int note : midiNotes)
    {
        if (_pendingCount >= kMaxNotes)
            break;
        if (note < 0 || note > 127)
            continue;
        _pendingNotes[_pendingCount++] = note;
    }
    _pendingDurationSamples = durationSamples;
    _hasPending = true;
}

void HostMidiEmitter::allNotesOff()
{
    const std::lock_guard lock(_mutex);
    _pendingAllNotesOff = true;
    _pendingCount = 0;
    _pendingDurationSamples = 0;
    _hasPending = true;
}

void HostMidiEmitter::renderNextBlock(juce::MidiBuffer& midiOut, int numSamples)
{
    bool hasPending = false;
    bool pendingAllOff = false;
    int pendingCount = 0;
    int pendingNotes[kMaxNotes] {};
    int pendingDuration = 0;

    {
        const std::lock_guard lock(_mutex);
        if (_hasPending)
        {
            hasPending = true;
            pendingAllOff = _pendingAllNotesOff;
            pendingCount = _pendingCount;
            for (int i = 0; i < pendingCount; ++i)
                pendingNotes[i] = _pendingNotes[i];
            pendingDuration = _pendingDurationSamples;
            _hasPending = false;
        }
    }

    if (hasPending)
    {
        for (int i = 0; i < _activeCount; ++i)
            midiOut.addEvent(juce::MidiMessage::noteOff(kMidiChannel, _activeNotes[i]), 0);

        _activeCount = 0;
        _noteOffPending = false;
        _samplesUntilNoteOff = 0;

        if (!pendingAllOff && pendingCount > 0)
        {
            for (int i = 0; i < pendingCount; ++i)
            {
                _activeNotes[i] = pendingNotes[i];
                midiOut.addEvent(juce::MidiMessage::noteOn(kMidiChannel, pendingNotes[i], kVelocity), 0);
            }
            _activeCount = pendingCount;
            _samplesUntilNoteOff = pendingDuration;
            _noteOffPending = true;
        }
    }

    if (!_noteOffPending || _activeCount == 0)
        return;

    if (_samplesUntilNoteOff <= numSamples)
    {
        const int offset = juce::jmax(0, _samplesUntilNoteOff - 1);
        for (int i = 0; i < _activeCount; ++i)
            midiOut.addEvent(juce::MidiMessage::noteOff(kMidiChannel, _activeNotes[i]), offset);

        _activeCount = 0;
        _noteOffPending = false;
        _samplesUntilNoteOff = 0;
    }
    else
    {
        _samplesUntilNoteOff -= numSamples;
    }
}

}
