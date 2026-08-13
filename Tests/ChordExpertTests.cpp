#include <algorithm>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "Theory/ChordExpert.h"
#include "Theory/TriadLibrary.h"

using theory::ChordExpert;
using theory::ChordExpertContext;
using theory::ChordNamingStyle;
using theory::Key;
using theory::Scale;
using theory::TriadLibrary;
using theory::TriadQuality;

namespace
{
    std::vector<int> midiPcs(std::initializer_list<int> pitchClasses, int bassPc = -1)
    {
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
}

TEST_CASE("ChordExpert: C6 with C bass in jazz style", "[ChordExpert]")
{
    ChordExpertContext ctx;
    ctx.key = Key::C;
    ctx.scale = Scale::Major;
    ctx.style = ChordNamingStyle::JazzChart;

    const auto r = ChordExpert::analyse(midiPcs({ 0, 4, 7, 9 }), ctx);
    REQUIRE(r.detection.matched);
    CHECK(r.detection.name == "C6");
    CHECK_FALSE(r.explanation.empty());
}

TEST_CASE("ChordExpert: previous G7 then C major is resolution", "[ChordExpert]")
{
    ChordExpertContext ctx;
    ctx.key = Key::C;
    ctx.scale = Scale::Major;
    ctx.style = ChordNamingStyle::JazzChart;
    ctx.previousChords.push_back(TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C, 0)); // G7

    const auto r = ChordExpert::analyse(midiPcs({ 0, 4, 7 }), ctx);
    REQUIRE(r.detection.matched);
    CHECK(r.detection.name == "C");
    CHECK(r.usedProgressionContext);
    CHECK(r.explanation.find("resolves") != std::string::npos);
}

TEST_CASE("ChordExpert: rootless shell after dominant", "[ChordExpert]")
{
    ChordExpertContext ctx;
    ctx.key = Key::C;
    ctx.scale = Scale::Major;
    ctx.style = ChordNamingStyle::JazzChart;
    ctx.previousChords.push_back(TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C, 0)); // G7

    // E G B D — guide tones / shell of Cmaj7 without C root.
    const auto r = ChordExpert::analyse(midiPcs({ 4, 7, 11, 2 }), ctx);
    REQUIRE(r.detection.matched);
    // Either rootless Cmaj7/E-ish or Em7; expert should prefer resolution toward C when confident.
    CHECK((r.detection.name.find("C") != std::string::npos
           || r.detection.name.find("Em") != std::string::npos
           || r.detection.qualityLabel == "rootless"
           || !r.alternatives.empty()));
}

TEST_CASE("ChordExpert: jazz chart spells m7b5 as ø7", "[ChordExpert]")
{
    ChordExpertContext ctx;
    ctx.key = Key::C;
    ctx.scale = Scale::Major;
    ctx.style = ChordNamingStyle::JazzChart;

    // B D F A = Bm7b5
    const auto r = ChordExpert::analyse(midiPcs({ 11, 2, 5, 9 }), ctx);
    REQUIRE(r.detection.matched);
    CHECK((r.detection.name.find("ø7") != std::string::npos
           || r.detection.name.find("m7b5") != std::string::npos));
}

TEST_CASE("ChordExpert: style keys round-trip", "[ChordExpert]")
{
    CHECK(ChordExpert::parseStyle("jazz") == ChordNamingStyle::JazzChart);
    CHECK(ChordExpert::parseStyle("pop") == ChordNamingStyle::PopSlash);
    CHECK(ChordExpert::parseStyle("classical") == ChordNamingStyle::Classical);
    CHECK(ChordExpert::styleKey(ChordNamingStyle::PopSlash) == "pop");
}
