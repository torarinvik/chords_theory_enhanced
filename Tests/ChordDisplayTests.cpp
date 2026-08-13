#include <catch2/catch_test_macros.hpp>

#include "Theory/ChordDisplay.h"
#include "Theory/TriadLibrary.h"

using theory::formatAbsoluteWithRoman;
using theory::Key;
using theory::romanForChordInKeyScale;
using theory::Scale;
using theory::TriadLibrary;
using theory::TriadQuality;

TEST_CASE("formatAbsoluteWithRoman: C major triad in C major is C · I", "[ChordDisplay]")
{
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(c, Key::C, Scale::Major);
    CHECK(label == "C · I");
    CHECK(romanForChordInKeyScale(c, Key::C, Scale::Major) == "I");
}

TEST_CASE("formatAbsoluteWithRoman: Am in C major is Am · vi", "[ChordDisplay]")
{
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(am, Key::C, Scale::Major);
    CHECK(label == "Am · vi");
}

TEST_CASE("formatAbsoluteWithRoman: F major as IV in C major", "[ChordDisplay]")
{
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C, 0);
    const auto label = formatAbsoluteWithRoman(f, Key::C, Scale::Major);
    CHECK(label.find("F") != std::string::npos);
    CHECK(label.find("IV") != std::string::npos);
    CHECK(label == "F · IV");
}

TEST_CASE("formatAbsoluteWithRoman: attached-scale style Dorian example", "[ChordDisplay]")
{
    // D minor triad analysed in A Dorian → iv (or similar minor degree).
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::A, 0);
    const auto label = formatAbsoluteWithRoman(dm, Key::A, Scale::Dorian, "Dm");
    CHECK(label.find("Dm") != std::string::npos);
    // Should include a roman when the root is diatonic in A Dorian.
    CHECK(label.find("·") != std::string::npos);
}

TEST_CASE("formatAbsoluteWithRoman: fallback name when chord notes empty", "[ChordDisplay]")
{
    theory::Chord empty;
    CHECK(formatAbsoluteWithRoman(empty, Key::C, Scale::Major, "X") == "X");
}
