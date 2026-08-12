#include <algorithm>

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include "Theory/ChordDatabase.h"
#include "Theory/NextScaleGenerator.h"

using theory::ChordDatabase;
using theory::Key;
using theory::NextScaleGenerator;
using theory::Scale;

TEST_CASE("NextScaleGenerator: parallel modes exclude the current scale", "[NextScale]")
{
    const auto suggestions = NextScaleGenerator::generate(
        Key::C, Scale::Major, std::nullopt, NextScaleGenerator::Pool::All);

    REQUIRE_FALSE(suggestions.empty());

    for (const auto& s : suggestions)
    {
        const bool isCurrent = s.key == Key::C && s.scale == Scale::Major;
        CHECK_FALSE(isCurrent);
    }

    // Current tonic parallels should dominate the list.
    const auto parallelCount = std::count_if(suggestions.begin(), suggestions.end(),
        [](const auto& s) { return s.key == Key::C; });
    CHECK(parallelCount >= 5);
}

TEST_CASE("NextScaleGenerator: relative minor of C major is suggested", "[NextScale]")
{
    const auto suggestions = NextScaleGenerator::generate(
        Key::C, Scale::Major, std::nullopt, NextScaleGenerator::Pool::Predicted);

    const auto hasAMinor = std::any_of(suggestions.begin(), suggestions.end(),
        [](const auto& s) { return s.key == Key::A && s.scale == Scale::Minor; });
    CHECK(hasAMinor);
}

TEST_CASE("NextScaleGenerator: query filters by scale name", "[NextScale]")
{
    const auto all = NextScaleGenerator::generate(
        Key::C, Scale::Major, std::nullopt, NextScaleGenerator::Pool::All, "");
    const auto dorianOnly = NextScaleGenerator::generate(
        Key::C, Scale::Major, std::nullopt, NextScaleGenerator::Pool::All, "dorian");

    REQUIRE(dorianOnly.size() < all.size());
    REQUIRE_FALSE(dorianOnly.empty());
    for (const auto& s : dorianOnly)
        CHECK(juce::String(s.label).containsIgnoreCase("dorian"));
}

TEST_CASE("NextScaleGenerator: chord context raises fit on containing scales", "[NextScale]")
{
    const auto& cMajor = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    REQUIRE_FALSE(cMajor.degrees.empty());
    REQUIRE_FALSE(cMajor.degrees.front().chords.empty());
    const auto& tonic = cMajor.degrees.front().chords.front(); // C major triad

    const auto suggestions = NextScaleGenerator::generate(
        Key::C, Scale::Dorian, tonic, NextScaleGenerator::Pool::All);

    auto it = std::find_if(suggestions.begin(), suggestions.end(),
        [](const auto& s) { return s.key == Key::C && s.scale == Scale::Major; });
    REQUIRE(it != suggestions.end());
    CHECK(it->fitPercent == 100);
}

TEST_CASE("NextScaleGenerator: changing current chord re-ranks by fit", "[NextScale]")
{
    const auto& cMajor = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    REQUIRE(cMajor.degrees.size() >= 5);

    const auto& cChord = cMajor.degrees.front().chords.front(); // I = C
    // V in C major is typically G (index 4 for I,II,III,IV,V)
    const auto& gChord = cMajor.degrees[4].chords.front();

    const auto forC = NextScaleGenerator::generate(
        Key::C, Scale::Major, cChord, NextScaleGenerator::Pool::All);
    const auto forG = NextScaleGenerator::generate(
        Key::C, Scale::Major, gChord, NextScaleGenerator::Pool::All);

    REQUIRE_FALSE(forC.empty());
    REQUIRE_FALSE(forG.empty());

    // G-rooted pool entries should appear / rank differently once G is current.
    const auto gMajorFitForC = [&]
    {
        auto it = std::find_if(forC.begin(), forC.end(),
            [](const auto& s) { return s.key == Key::G && s.scale == Scale::Major; });
        return it == forC.end() ? -1 : it->fitPercent;
    }();
    const auto gMajorFitForG = [&]
    {
        auto it = std::find_if(forG.begin(), forG.end(),
            [](const auto& s) { return s.key == Key::G && s.scale == Scale::Major; });
        return it == forG.end() ? -1 : it->fitPercent;
    }();

    REQUIRE(gMajorFitForG >= 0);
    // G major always fits G triad at 100%; with C as current, G major still fits C triad but the
    // presence of G-root scales (or their relative ranking) should differ vs C context.
    CHECK(gMajorFitForG == 100);

    // At least one "contains" reason should mention the active chord's readable name.
    const auto containsG = std::any_of(forG.begin(), forG.end(),
        [&](const auto& s)
        {
            return s.reason == theory::ScaleSuggestionReason::ContainsChord
                && s.reasonChordName == gChord.readableName;
        });
    CHECK(containsG);

    // Rankings should move with the chord: list shape, top pick, or fit of G major.
    const bool listsDiffer = forC.size() != forG.size()
        || forC.front().label != forG.front().label
        || std::abs(forC.front().score - forG.front().score) > 0.001f
        || gMajorFitForC != gMajorFitForG;
    CHECK(listsDiffer);
}
