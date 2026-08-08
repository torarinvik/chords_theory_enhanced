#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <set>

#include "Theory/ChordDatabase.h"
#include "Theory/NextChordGenerator.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

using theory::ChordDatabase;
using theory::ChordType;
using theory::Degree;
using theory::Key;
using theory::NextChordCandidate;
using theory::NextChordGenerator;
using theory::NextChordScorer;
using theory::Scale;
using theory::TriadLibrary;
using theory::TriadQuality;

namespace
{
    std::set<int> pitchClasses(const theory::Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }

    int tensionOf(const theory::Chord& from, const theory::Chord& to, const theory::KeyScaleData& keyScale,
                  std::optional<Degree> degree = std::nullopt,
                  float drama01 = NextChordScorer::kDefaultDrama)
    {
        NextChordCandidate candidate;
        candidate.chord = to;
        candidate.degree = degree;
        NextChordScorer::score(from, keyScale, candidate, drama01);
        return candidate.tensionPercent;
    }

    const NextChordCandidate* findByPcs(const std::vector<NextChordCandidate>& candidates, std::set<int> pcs)
    {
        for (const auto& c : candidates)
            if (pitchClasses(c.chord) == pcs)
                return &c;
        return nullptr;
    }
}

TEST_CASE("TriadLibrary: 11 qualities on 12 roots including sevenths", "[NextChord][TriadLibrary]")
{
    const auto all = TriadLibrary::allTriads(Key::C);
    REQUIRE(all.size() == TriadLibrary::kNumNamedChords);
    REQUIRE(all.size() == 132);

    int sus2 = 0, sus4 = 0, power = 0, sevenths = 0;
    for (const auto& c : all)
    {
        if (c.type == ChordType::Sus2)
            ++sus2;
        if (c.type == ChordType::Sus4)
            ++sus4;
        if (c.type == ChordType::Power)
            ++power;
        if (c.type == ChordType::Seventh)
            ++sevenths;
    }
    CHECK(sus2 == 12);
    CHECK(sus4 == 12);
    CHECK(power == 12);
    CHECK(sevenths == 48); // maj7 / m7 / 7 / m7b5 × 12

    // Pre-seventh unique sonorities: 64 (12×(maj+min+dim+power) + 4 aug + 12 sus after Csus2≡Gsus4).
    // Sevenths are all 4-note sets, so they never collide with 2/3-note triads/sus/power.
    // 4 seventh qualities × 12 roots = 48 additional unique sets → 112.
    std::set<std::set<int>> uniqueSets;
    for (const auto& c : all)
        uniqueSets.insert(pitchClasses(c));
    CHECK(uniqueSets.size() == 112);
}

TEST_CASE("TriadLibrary: C qualities have correct intervals", "[NextChord][TriadLibrary]")
{
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C)) == std::set<int>({ 0, 4, 7 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Minor, Key::C)) == std::set<int>({ 0, 3, 7 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Diminished, Key::C)) == std::set<int>({ 0, 3, 6 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Augmented, Key::C)) == std::set<int>({ 0, 4, 8 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Sus2, Key::C)) == std::set<int>({ 0, 2, 7 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Sus4, Key::C)) == std::set<int>({ 0, 5, 7 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Power, Key::C)) == std::set<int>({ 0, 7 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C)) == std::set<int>({ 0, 4, 7, 11 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Minor7, Key::C)) == std::set<int>({ 0, 3, 7, 10 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::Dominant7, Key::C)) == std::set<int>({ 0, 4, 7, 10 }));
    CHECK(pitchClasses(TriadLibrary::makeTriad(0, TriadQuality::HalfDim7, Key::C)) == std::set<int>({ 0, 3, 6, 10 }));

    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Sus4, Key::C).symbol == "Csus4");
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C).symbol == "Cmaj7");
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Dominant7, Key::C).symbol == "C7");
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::HalfDim7, Key::C).symbol == "Cm7b5");
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Sus2, Key::C).type == ChordType::Sus2);
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Power, Key::C).type == ChordType::Power);
    CHECK(TriadLibrary::makeTriad(0, TriadQuality::Minor7, Key::C).type == ChordType::Seventh);
}

TEST_CASE("NextChordGenerator: pool includes sevenths; drama reorders list", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto current = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);

    const auto candidates = NextChordGenerator::generate(current, keyScale, 0.0f);
    // 112 unique sonorities − C major.
    REQUIRE(candidates.size() == 111);

    // drama=0 → softest first (monotone non-decreasing tension).
    for (std::size_t i = 1; i < candidates.size(); ++i)
        CHECK(candidates[i - 1].tensionPercent <= candidates[i].tensionPercent);

    REQUIRE(findByPcs(candidates, { 0, 5, 7 }) != nullptr);       // Csus4
    REQUIRE(findByPcs(candidates, { 0, 2, 7 }) != nullptr);       // Csus2
    REQUIRE(findByPcs(candidates, { 0, 7 }) != nullptr);          // C5
    REQUIRE(findByPcs(candidates, { 0, 4, 7, 11 }) != nullptr);   // Cmaj7
    REQUIRE(findByPcs(candidates, { 0, 4, 7, 10 }) != nullptr);   // C7
    REQUIRE(findByPcs(candidates, { 2, 5, 9, 0 }) != nullptr);    // Dm7
    REQUIRE(findByPcs(candidates, { 7, 11, 2, 5 }) != nullptr);   // G7

    const auto* am = findByPcs(candidates, { 9, 0, 4 });
    REQUIRE(am != nullptr);
    REQUIRE(am->degree.has_value());
    CHECK(*am->degree == Degree::VI);

    // Csus4 appears in C major database under degree I — should get a degree label.
    const auto* csus4 = findByPcs(candidates, { 0, 5, 7 });
    REQUIRE(csus4 != nullptr);
    REQUIRE(csus4->degree.has_value());
    CHECK(*csus4->degree == Degree::I);

    // drama=1 → wildest first.
    const auto wild = NextChordGenerator::generate(current, keyScale, 1.0f);
    REQUIRE(wild.size() == candidates.size());
    for (std::size_t i = 1; i < wild.size(); ++i)
        CHECK(wild[i - 1].tensionPercent >= wild[i].tensionPercent);
}

TEST_CASE("NextChordScorer: Am softer than F#; diatonic primaries beat parallel minor", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C);
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto cm = TriadLibrary::makeTriad(0, TriadQuality::Minor, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);
    const auto csus4 = TriadLibrary::makeTriad(0, TriadQuality::Sus4, Key::C);
    const auto cmaj7 = TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C);

    CHECK(tensionOf(c, am, keyScale, Degree::VI) < tensionOf(c, fs, keyScale));
    CHECK(tensionOf(c, g, keyScale, Degree::V) < tensionOf(c, cm, keyScale));
    CHECK(tensionOf(c, dm, keyScale, Degree::II) < tensionOf(c, cm, keyScale));
    CHECK(tensionOf(c, f, keyScale, Degree::IV) < tensionOf(c, cm, keyScale));

    // Same-root sus is a mild colour change — softer than parallel minor, harder than staying on V.
    const int tSus = tensionOf(c, csus4, keyScale, Degree::I);
    const int tG = tensionOf(c, g, keyScale, Degree::V);
    const int tCm = tensionOf(c, cm, keyScale);
    CHECK(tSus < tCm);
    CHECK(tSus > tG - 15); // not wildly out of the soft band

    // C → Cmaj7 is a mild extension — softer than C → Cm.
    CHECK(tensionOf(c, cmaj7, keyScale, Degree::I) < tCm);
}

TEST_CASE("NextChordScorer: tritone root more remote than half-step root", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto db = TriadLibrary::makeTriad(1, TriadQuality::Major, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);
    CHECK(tensionOf(c, db, keyScale) < tensionOf(c, fs, keyScale));
}

TEST_CASE("NextChordScorer: detectTriadQuality covers sus, power, and sevenths", "[NextChord]")
{
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Sus2, Key::C)) == TriadQuality::Sus2);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Sus4, Key::C)) == TriadQuality::Sus4);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Power, Key::C)) == TriadQuality::Power);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C)) == TriadQuality::Major);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Major7, Key::C)) == TriadQuality::Major7);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Minor7, Key::C)) == TriadQuality::Minor7);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::Dominant7, Key::C)) == TriadQuality::Dominant7);
    CHECK(NextChordScorer::detectTriadQuality(TriadLibrary::makeTriad(0, TriadQuality::HalfDim7, Key::C)) == TriadQuality::HalfDim7);
}

TEST_CASE("NextChordScorer: circle of fifths distance", "[NextChord]")
{
    CHECK(NextChordScorer::circleOfFifthsDistance(0, 7) == 1); // C→G
    CHECK(NextChordScorer::circleOfFifthsDistance(0, 2) == 2); // C→D
    CHECK(NextChordScorer::circleOfFifthsDistance(0, 6) == 6); // C→F# (max)
}

TEST_CASE("NextChordScorer: scaleFamily maps modes", "[NextChord]")
{
    using Family = NextChordScorer::ScaleFamily;
    CHECK(NextChordScorer::scaleFamily(Scale::Major) == Family::Majorish);
    CHECK(NextChordScorer::scaleFamily(Scale::Lydian) == Family::Majorish);
    CHECK(NextChordScorer::scaleFamily(Scale::Minor) == Family::Minorish);
    CHECK(NextChordScorer::scaleFamily(Scale::HarmonicMinor) == Family::Minorish);
    CHECK(NextChordScorer::scaleFamily(Scale::Dorian) == Family::ModalSoft);
    CHECK(NextChordScorer::scaleFamily(Scale::Mixolydian) == Family::ModalSoft);
    CHECK(NextChordScorer::scaleFamily(Scale::Locrian) == Family::Diminishedish);
}

TEST_CASE("NextChordScorer: drama targets tension band for ranking", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    // Objective tension: V softer than remote F# (independent of drama target).
    REQUIRE(tensionOf(c, g, keyScale, Degree::V, 0.35f) < tensionOf(c, fs, keyScale, std::nullopt, 0.35f));

    const auto rankOf = [](const std::vector<NextChordCandidate>& list, std::set<int> pcs) -> int
    {
        for (std::size_t i = 0; i < list.size(); ++i)
            if (pitchClasses(list[i].chord) == pcs)
                return static_cast<int>(i);
        return -1;
    };

    const auto calm = NextChordGenerator::generate(c, keyScale, 0.0f);
    const auto wild = NextChordGenerator::generate(c, keyScale, 1.0f);

    const int calmFs = rankOf(calm, { 6, 10, 1 });
    const int wildFs = rankOf(wild, { 6, 10, 1 });
    const int calmG = rankOf(calm, { 7, 11, 2 });
    const int wildG = rankOf(wild, { 7, 11, 2 });
    REQUIRE(calmFs >= 0);
    REQUIRE(wildFs >= 0);
    REQUIRE(calmG >= 0);
    REQUIRE(wildG >= 0);

    // Smooth: G before F#. Wild: F# before G.
    CHECK(calmG < calmFs);
    CHECK(wildFs < wildG);
    CHECK(wildFs < calmFs);
}

TEST_CASE("NextChordScorer: minor scale prefers bVII over major's vii habits", "[NextChord]")
{
    const auto& major = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto& minor = ChordDatabase::getInstance().get(Key::A, Scale::Minor);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C); // C = III in A minor, I in C major
    const auto bb = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::C); // Bb = bVII of C / bII of A? 

    // In C major from C: Bb is chromatic (bVII rock colour).
    // In A minor from Am: G major is bVII — very common. Use Am as current in minor.
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::A);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::A); // bVII in A minor

    const int tBbInMajor = tensionOf(c, bb, major);
    const int tGInMinor = tensionOf(am, g, minor, Degree::VII);

    // Minor bVII should sit in a comfortable band (not extreme); major bVII is outside and harsher.
    CHECK(tGInMinor < tBbInMajor);
}
