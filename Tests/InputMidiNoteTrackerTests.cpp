#include <catch2/catch_test_macros.hpp>

#include "Audio/ChordSynthEngine.h"
#include "Audio/InputMidiNoteTracker.h"

using audio::InputMidiNoteTracker;

TEST_CASE("InputMidiNoteTracker starts with no notes held", "[InputMidiNoteTracker]")
{
    InputMidiNoteTracker tracker;
    CHECK_FALSE(tracker.isNoteHeld(60));
    CHECK_FALSE(tracker.isNoteHeld(0));
    CHECK_FALSE(tracker.isNoteHeld(127));
    CHECK_FALSE(tracker.isNoteHeld(-1));
    CHECK_FALSE(tracker.isNoteHeld(128));
    CHECK(tracker.getGeneration() == 0);
}

TEST_CASE("InputMidiNoteTracker tracks note on and off", "[InputMidiNoteTracker]")
{
    InputMidiNoteTracker tracker;
    juce::MidiBuffer midi;

    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 90), 10);
    tracker.processHostMidi(midi);

    CHECK(tracker.isNoteHeld(60));
    CHECK(tracker.isNoteHeld(64));
    CHECK_FALSE(tracker.isNoteHeld(67));
    const auto genAfterOn = tracker.getGeneration();
    CHECK(genAfterOn > 0);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
    tracker.processHostMidi(midi);

    CHECK_FALSE(tracker.isNoteHeld(60));
    CHECK(tracker.isNoteHeld(64));
    CHECK(tracker.getGeneration() > genAfterOn);
}

TEST_CASE("InputMidiNoteTracker treats velocity-0 note-on as note-off", "[InputMidiNoteTracker]")
{
    InputMidiNoteTracker tracker;
    juce::MidiBuffer midi;

    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 80), 0);
    tracker.processHostMidi(midi);
    REQUIRE(tracker.isNoteHeld(72));

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8) 0), 0);
    tracker.processHostMidi(midi);
    CHECK_FALSE(tracker.isNoteHeld(72));
}

TEST_CASE("InputMidiNoteTracker clear and all-notes-off release everything", "[InputMidiNoteTracker]")
{
    InputMidiNoteTracker tracker;
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 100), 0);
    midi.addEvent(juce::MidiMessage::noteOn(1, 55, (juce::uint8) 100), 0);
    tracker.processHostMidi(midi);
    REQUIRE(tracker.isNoteHeld(48));
    REQUIRE(tracker.isNoteHeld(55));

    const auto genBefore = tracker.getGeneration();
    tracker.clear();
    CHECK_FALSE(tracker.isNoteHeld(48));
    CHECK_FALSE(tracker.isNoteHeld(55));
    CHECK(tracker.getGeneration() > genBefore);

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    tracker.processHostMidi(midi);
    REQUIRE(tracker.isNoteHeld(60));

    midi.clear();
    midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    tracker.processHostMidi(midi);
    CHECK_FALSE(tracker.isNoteHeld(60));
}

TEST_CASE("InputMidiNoteTracker generation does not bump for redundant state", "[InputMidiNoteTracker]")
{
    InputMidiNoteTracker tracker;
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
    tracker.processHostMidi(midi);
    const auto gen = tracker.getGeneration();

    midi.clear();
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 80), 0); // already held
    tracker.processHostMidi(midi);
    CHECK(tracker.getGeneration() == gen);
}

TEST_CASE("ChordSynthEngine tracks host MIDI before progression inject", "[InputMidiNoteTracker][ChordSynthEngine]")
{
    audio::ChordSynthEngine engine;
    engine.prepare(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 61, (juce::uint8) 100), 0);

    engine.renderNextBlock(buffer, midi, 0, 512);

    CHECK(engine.getInputMidiNoteTracker().isNoteHeld(61));

    engine.reset();
    CHECK_FALSE(engine.getInputMidiNoteTracker().isNoteHeld(61));
}
