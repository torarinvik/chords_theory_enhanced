#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Theory/ChordDetector.h"
#include "Theory/TriadLibrary.h"

using theory::ChordDetector;
using theory::Key;
using theory::TriadLibrary;
using theory::TriadQuality;

namespace
{
    std::vector<int> midiPcs(std::initializer_list<int> pitchClasses, int bassPc = -1)
    {
        // Place each pitch class as a MIDI note near the middle register.
        // If bassPc is set, that PC is voiced as the lowest note.
        std::vector<int> ordered(pitchClasses);
        if (bassPc >= 0)
        {
            ordered.erase(std::remove(ordered.begin(), ordered.end(), bassPc), ordered.end());
            ordered.insert(ordered.begin(), bassPc);
        }

        std::vector<int> notes;
        int prev = 40;
        for (const int pc : ordered)
        {
            int midi = ((pc % 12) + 12) % 12;
            while (midi <= prev)
                midi += 12;
            notes.push_back(midi);
            prev = midi;
        }
        return notes;
    }

    std::vector<int> midiFor(const theory::Chord& chord)
    {
        std::vector<int> notes;
        int prev = 40;
        for (const auto& n : chord.notes)
        {
            int midi = n.getPitchClass();
            while (midi <= prev)
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
    const auto d = ChordDetector::detectFromMidiNotes({ 60 }, Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C");
}

TEST_CASE("ChordDetector: C major triad", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C");
    CHECK(d.hasLibraryQuality);
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
    const auto chord = TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7");
}

TEST_CASE("ChordDetector: flat spelling in flat keys", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::Bb, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::Bb);
    REQUIRE(d.matched);
    CHECK(d.name == "Bb");
}

TEST_CASE("ChordDetector: prefers seventh over triad when 7th is present", "[ChordDetector]")
{
    const auto chord = TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C, 0);
    const auto d = ChordDetector::detectFromMidiNotes(midiFor(chord), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cmaj7");
    CHECK(d.quality == TriadQuality::Major7);
}

TEST_CASE("ChordDetector: dim7", "[ChordDetector]")
{
    // C Eb Gb Bbb(=A)
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 3, 6, 9 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cdim7");
    CHECK(d.qualityLabel == "dim7");
}

TEST_CASE("ChordDetector: minor 9", "[ChordDetector]")
{
    // Am9 = A C E G B
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 9, 0, 4, 7, 11 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Am9");
}

TEST_CASE("ChordDetector: dominant 9", "[ChordDetector]")
{
    // G9 = G B D F A
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 7, 11, 2, 5, 9 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G9");
}

TEST_CASE("ChordDetector: maj9", "[ChordDetector]")
{
    // Cmaj9 = C E G B D
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 7, 11, 2 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cmaj9");
}

TEST_CASE("ChordDetector: sixth and minor sixth", "[ChordDetector]")
{
    // C6 = C E G A
    auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 7, 9 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C6");

    // Am6 = A C E F#
    d = ChordDetector::detectFromMidiNotes(midiPcs({ 9, 0, 4, 6 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Am6");
}

TEST_CASE("ChordDetector: add9", "[ChordDetector]")
{
    // Cadd9 = C E G D
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 7, 2 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cadd9");
}

TEST_CASE("ChordDetector: 7sus4", "[ChordDetector]")
{
    // G7sus4 = G C D F
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 7, 0, 2, 5 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7sus4");
}

TEST_CASE("ChordDetector: 7b9", "[ChordDetector]")
{
    // G7b9 = G B D F Ab
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 7, 11, 2, 5, 8 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7b9");
}

TEST_CASE("ChordDetector: 7#9", "[ChordDetector]")
{
    // E7#9 = E G# B D G  (Hendrix)
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 4, 8, 11, 2, 7 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "E7#9");
}

TEST_CASE("ChordDetector: m(maj7)", "[ChordDetector]")
{
    // Cm(maj7) = C Eb G B
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 3, 7, 11 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cm(maj7)");
}

TEST_CASE("ChordDetector: shell voicing without fifth still names seventh", "[ChordDetector]")
{
    // Cmaj7 shell: C E B (no G)
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 11 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cmaj7");
}

TEST_CASE("ChordDetector: power chord and sus", "[ChordDetector]")
{
    auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 7 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C5");

    d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 5, 7 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Csus4");

    d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 2, 7 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Csus2");
}

TEST_CASE("ChordDetector: 13 prefers extension over plain 7", "[ChordDetector]")
{
    // C13 tones: C E Bb D A (5th optional omitted)
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 10, 2, 9 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "C13");
}

TEST_CASE("ChordDetector: slash bass on extension", "[ChordDetector]")
{
    // Am7/G = A C E G with G in bass
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 9, 0, 4, 7 }, 7), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Am7/G");
}

TEST_CASE("ChordDetector: sus forbids thirds", "[ChordDetector]")
{
    // C E F G has both 3 and sus4 tone — not a pure sus4; prefer Cadd11-ish or C with extra.
    const auto withThird = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 5, 7 }), Key::C);
    REQUIRE(withThird.matched);
    CHECK(withThird.name != "Csus4");
    // Pure sus4
    const auto pure = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 5, 7 }), Key::C);
    REQUIRE(pure.matched);
    CHECK(pure.name == "Csus4");
}

TEST_CASE("ChordDetector: maj vs min mutual exclusion", "[ChordDetector]")
{
    const auto maj = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 7 }), Key::C);
    REQUIRE(maj.matched);
    CHECK(maj.name == "C");

    const auto min = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 3, 7 }), Key::C);
    REQUIRE(min.matched);
    CHECK(min.name == "Cm");
}

TEST_CASE("ChordDetector: maj9#11 and 7#5", "[ChordDetector]")
{
    // Cmaj9#11 = C E G B D F#
    auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 4, 7, 11, 2, 6 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cmaj9#11");

    // G7#5 = G B D# F
    d = ChordDetector::detectFromMidiNotes(midiPcs({ 7, 11, 3, 5 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7#5");
}

TEST_CASE("ChordDetector: m(maj9)", "[ChordDetector]")
{
    // Cm(maj9) = C Eb G B D
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 3, 7, 11, 2 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cm(maj9)");
}

TEST_CASE("ChordDetector: rootless maj7 shell still detects", "[ChordDetector]")
{
    // E G B D without C → often heard as rootless Cmaj9 / Em7. Prefer Em7 with E bass.
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 4, 7, 11, 2 }), Key::C);
    REQUIRE(d.matched);
    // Em7 (root held as E) is the rooted reading.
    CHECK(d.name == "Em7");
}

TEST_CASE("ChordDetector: 7sus2", "[ChordDetector]")
{
    // G7sus2 = G A D F
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 7, 9, 2, 5 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "G7sus2");
}

TEST_CASE("ChordDetector: dim(maj7)", "[ChordDetector]")
{
    // Cdim(maj7) = C Eb Gb B
    const auto d = ChordDetector::detectFromMidiNotes(midiPcs({ 0, 3, 6, 11 }), Key::C);
    REQUIRE(d.matched);
    CHECK(d.name == "Cdim(maj7)");
}

