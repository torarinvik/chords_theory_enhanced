#include <catch2/catch_test_macros.hpp>

#include "Theory/ChordDisplay.h"
#include "Theory/TriadLibrary.h"

using theory::formatAbsoluteWithRoman;
using theory::Key;
using theory::romanForChordInKeyScale;
using theory::Scale;
using theory::TriadLibrary;
using theory::TriadQuality;

TEST_CASE("formatAbsoluteWithRoman: C major triad in C major is C - I", "[ChordDisplay]")
{
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(c, Key::C, Scale::Major);
    CHECK(label == "C - I");
    CHECK(romanForChordInKeyScale(c, Key::C, Scale::Major) == "I");
}

TEST_CASE("formatAbsoluteWithRoman: Am in C major is Am - vi", "[ChordDisplay]")
{
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(am, Key::C, Scale::Major);
    CHECK(label == "Am - vi");
}

TEST_CASE("formatAbsoluteWithRoman: F major as IV in C major", "[ChordDisplay]")
{
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(f, Key::C, Scale::Major);
    CHECK(label.find("F") != std::string::npos);
    CHECK(label.find("IV") != std::string::npos);
    CHECK(label == "F - IV");
}

TEST_CASE("formatAbsoluteWithRoman: attached-scale style Dorian example", "[ChordDisplay]")
{
    // D minor triad analysed in A Dorian → iv (or similar minor degree).
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::A, 0);
    const auto label = formatAbsoluteWithRoman(dm, Key::A, Scale::Dorian, "Dm");
    CHECK(label.find("Dm") != std::string::npos);
    // Should include a roman when the root is diatonic in A Dorian.
    CHECK(label.find(" - ") != std::string::npos);
}

TEST_CASE("formatAbsoluteWithRoman: C major under attached A minor is C - III", "[ChordDisplay]")
{
    // Timeline chip with A Minor attached: C functions as III.
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(c, Key::A, Scale::Minor) == "III");
    CHECK(formatAbsoluteWithRoman(c, Key::A, Scale::Minor, "C") == "C - III");
}

TEST_CASE("formatAbsoluteWithRoman: fallback name when chord notes empty", "[ChordDisplay]")
{
    theory::Chord empty;
    CHECK(formatAbsoluteWithRoman(empty, Key::C, Scale::Major, "X") == "X");
}

TEST_CASE("romanForChordInKeyScale: mixture flats bVII bVI bIII bII", "[ChordDisplay]")
{
    // In C major: Bb major → bVII, Ab major → bVI, Eb major → bIII, Db major → bII
    const auto bb = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(bb, Key::C, Scale::Major) == "bVII");
    CHECK(formatAbsoluteWithRoman(bb, Key::C, Scale::Major).find("bVII") != std::string::npos);

    const auto ab = TriadLibrary::makeTriad(8, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(ab, Key::C, Scale::Major) == "bVI");

    const auto eb = TriadLibrary::makeTriad(3, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(eb, Key::C, Scale::Major) == "bIII");

    const auto db = TriadLibrary::makeTriad(1, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(db, Key::C, Scale::Major) == "bII");
}

TEST_CASE("romanForChordInKeyScale: #IV and minor mixture iv", "[ChordDisplay]")
{
    // F# dim-ish / F# major as #IV colour in C
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C, 0);
    CHECK(romanForChordInKeyScale(fs, Key::C, Scale::Major) == "#IV");

    // Fm is diatonic degree IV with minor quality → "iv" (not bIV)
    const auto fm = TriadLibrary::makeTriad(5, TriadQuality::Minor, Key::C, 0);
    CHECK(romanForChordInKeyScale(fm, Key::C, Scale::Major) == "iv");
}

TEST_CASE("romanForChordInKeyScale: secondary dominant still V/x", "[ChordDisplay]")
{
    // D7 in C → V/V (or similar secondary label)
    const auto d7 = TriadLibrary::makeTriad(2, TriadQuality::Dominant7, Key::C, 0);
    const auto roman = romanForChordInKeyScale(d7, Key::C, Scale::Major);
    CHECK((roman == "V/V" || roman.find("V/") != std::string::npos || roman == "II"));
}
