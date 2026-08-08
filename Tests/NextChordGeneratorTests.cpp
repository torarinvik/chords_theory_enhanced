#include <catch2/catch_test_macros.hpp>

#include <set>

#include "Theory/ChordDatabase.h"
#include "Theory/NextChordGenerator.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

using theory::ChordDatabase;
using theory::ChordType;
using theory::Key;
using theory::NextChordGenerator;
using theory::NextChordScorer;
using theory::Scale;
using theory::TriadLibrary;
using theory::TriadQuality;

namespace
{
    theory::Chord makeTriad(const std::string& symbol, std::initializer_list<const char*> noteNames)
    {
        theory::Chord chord;
        chord.symbol = symbol;
        chord.readableName = symbol;
        chord.type = ChordType::Triad;
        for (const char* name : noteNames)
            chord.notes.push_back(theory::NoteName { name, name, 0 });
        return chord;
    }

    std::set<int> pitchClasses(const theory::Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }
}

TEST_CASE("TriadLibrary: catalogues exactly 48 triads (12 roots × 4 qualities)", "[NextChord][TriadLibrary]")
{
    const auto all = TriadLibrary::allTriads(Key::C);
    REQUIRE(all.size() == TriadLibrary::kNumTriads);
    REQUIRE(all.size() == 48);

    for (const auto& triad : all)
    {
        CHECK(triad.type == ChordType::Triad);
        CHECK(triad.notes.size() == 3);
    }

    // Named catalogue is 48, but augmented triads cycle every major third
    // (Caug == Eaug == G#aug as pitch classes) → 12+12+12+4 = 40 unique sonorities.
    std::set<std::set<int>> uniqueSets;
    for (const auto& triad : all)
        uniqueSets.insert(pitchClasses(triad));
    CHECK(uniqueSets.size() == 40);
}

TEST_CASE("TriadLibrary: C major/minor/dim/aug intervals are correct", "[NextChord][TriadLibrary]")
{
    const auto cMaj = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    CHECK(pitchClasses(cMaj) == std::set<int>({ 0, 4, 7 }));
    CHECK(cMaj.symbol == "C");

    const auto cMin = TriadLibrary::makeTriad(0, TriadQuality::Minor, Key::C);
    CHECK(pitchClasses(cMin) == std::set<int>({ 0, 3, 7 }));
    CHECK(cMin.symbol == "Cm");

    const auto cDim = TriadLibrary::makeTriad(0, TriadQuality::Diminished, Key::C);
    CHECK(pitchClasses(cDim) == std::set<int>({ 0, 3, 6 }));
    CHECK(cDim.symbol == "Cdim");

    const auto cAug = TriadLibrary::makeTriad(0, TriadQuality::Augmented, Key::C);
    CHECK(pitchClasses(cAug) == std::set<int>({ 0, 4, 8 }));
    CHECK(cAug.symbol == "Caug");
}

TEST_CASE("NextChordGenerator: from C major yields every other unique triad harmony, sorted by tension", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto current = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);

    const auto candidates = NextChordGenerator::generate(current, keyScale);
    // 40 unique triad sonorities − current C major.
    REQUIRE(candidates.size() == 39);

    for (const auto& candidate : candidates)
    {
        CHECK(candidate.chord.type == ChordType::Triad);
        CHECK(pitchClasses(candidate.chord) != pitchClasses(current));
    }

    for (std::size_t i = 1; i < candidates.size(); ++i)
        CHECK(candidates[i - 1].tensionPercent <= candidates[i].tensionPercent);

    // Diatonic Am (vi) should appear and carry Degree::VI.
    const auto am = std::find_if(candidates.begin(), candidates.end(),
        [](const theory::NextChordCandidate& c)
        {
            return pitchClasses(c.chord) == std::set<int>({ 9, 0, 4 });
        });
    REQUIRE(am != candidates.end());
    REQUIRE(am->degree.has_value());
    CHECK(*am->degree == theory::Degree::VI);
}

TEST_CASE("NextChordScorer: Am shares more common tones with C than F# maj, and scores smoother", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto cMajor = makeTriad("C", { "C", "E", "G" });
    const auto aMinor = makeTriad("Am", { "A", "C", "E" });
    const auto fSharp = makeTriad("F#", { "F#", "A#", "C#" });

    CHECK(NextChordScorer::commonToneCount(cMajor, aMinor) == 2);
    CHECK(NextChordScorer::commonToneCount(cMajor, fSharp) == 0);

    theory::NextChordCandidate amCandidate { .chord = aMinor };
    theory::NextChordCandidate fsCandidate { .chord = fSharp };
    NextChordScorer::score(cMajor, keyScale, amCandidate);
    NextChordScorer::score(cMajor, keyScale, fsCandidate);

    CHECK(amCandidate.tensionPercent < fsCandidate.tensionPercent);
}

TEST_CASE("NextChordScorer: root motion of a fifth is closer than a chromatic root step", "[NextChord]")
{
    CHECK(NextChordScorer::pitchClassDistance(0, 7) == 5); // C → G
    CHECK(NextChordScorer::pitchClassDistance(0, 1) == 1); // C → C#
}
