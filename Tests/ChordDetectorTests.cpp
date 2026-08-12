#include <catch2/catch_test_macros.hpp>

#include "Theory/ChordDetector.h"
#include "Theory/TriadLibrary.h"

using theory::ChordDetector;
using theory::Key;
using theory::TriadLibrary;
using theory::TriadQuality;

namespace
{
    std::vector<int> midiFor(const theory::Chord& chord, int bassOctaveMidi = 48)
    {
        // Place chord tones as MIDI notes starting near bassOctaveMidi, ascending.
        std::vector<int> notes;
        int prev = bassOctaveMidi - 1;
        for (const auto& n : chord.notes)
        {
            const int pc = n.getPitchClass();
            int midi = pc;
            while (midi <= prev)
                midi += 12;
            // Prefer mid-register.
            while (midi < 48)
                midi += 12;
            notes.push_back(midi);
            prev = midi;
        }
        return notes;
    }
}

TEST_CASE("ChordDetector: empty input is unmatched", "[ChordDetector]")
{
    const auto d = ChordDetector::detectFromMidiNotes({}, Key::C);
    CHECK_FALSE(d.matched);
    CHECK(d.name.empty());
}

TEST_CASE("ChordDetector: single note names the pitch class", "[ChordDetector]")
{
    const auto d = ChordDetector::detectFromMidiNotes({ 60 }, Key::C); // C4
    REQUIRE(d.matched);
    CHECK(d.name == "C");
}

TEST_CASE("ChordDetector: C major triad", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C");
    CHECK(d.quality == TriadQuality::Major);
}

TEST_CASE("ChordDetector: A minor seventh", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(9, TriadQuality::Minor7, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Am7");
    CHECK(d.quality == TriadQuality::Minor7);
}

TEST_CASE("ChordDetector: first inversion slash name", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 1); // C/E
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C/E");
}

TEST_CASE("ChordDetector: dominant 7", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C, 0); // G7
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7");
}

TEST_CASE("ChordDetector: flat spelling in flat keys", "[ChordDetector]")
{
    // Bb major triad voiced mid-range.
    const auto chord = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::Bb, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::Bb);
    REQUIRE(d.matched);
    CHECK(d.name == "Bb");
}

TEST_CASE("ChordDetector: prefers seventh over triad when 7th is present", "[ChordDetector]")
{
    // C E G B → Cmaj7, not C.
    const auto chord = TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cmaj7");
    CHECK(d.quality == TriadQuality::Major7);
}
