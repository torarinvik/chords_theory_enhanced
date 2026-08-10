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
