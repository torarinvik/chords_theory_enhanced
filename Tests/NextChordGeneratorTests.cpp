#include <catch2/catch_test_macros.hpp>

#include <optional>
#include <set>

#include "Theory/ChordDatabase.h"
#include "Theory/MidiEditorState.h"
#include "Theory/NextChordGenerator.h"
#include "Theory/NextChordScorer.h"
#include "Theory/NextChordSequenceContext.h"
#include "Theory/TriadLibrary.h"

using theory::ChordDatabase;
using theory::ChordType;
using theory::Degree;
using theory::Key;
using theory::MidiEditorChordBlockState;
using theory::MidiEditorNoteState;
using theory::MidiEditorState;
using theory::NextChordCandidate;
using theory::NextChordGenerator;
using theory::NextChordScorer;
using theory::Scale;
using theory::SequenceContext;
using theory::SequenceEvent;
using theory::TriadLibrary;
using theory::TriadQuality;
using theory::buildSequenceContext;

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
                  float drama01 = NextChordScorer::kDefaultDrama,
                  const SequenceContext& sequence = {})
    {
        NextChordCandidate candidate;
        candidate.chord = to;
        candidate.degree = degree;
        NextChordScorer::score(from, keyScale, candidate, drama01, sequence);
        return candidate.tensionPercent;
    }

    SequenceContext history(std::initializer_list<std::pair<theory::Chord, std::optional<Degree>>> events)
    {
        SequenceContext ctx;
        for (const auto& [chord, degree] : events)
            ctx.previous.push_back(SequenceEvent { chord, degree });
        ctx.trim();
        return ctx;
    }

    const NextChordCandidate* findByPcs(const std::vector<NextChordCandidate>& candidates, std::set<int> pcs)
    {
        for (const auto& c : candidates)
            if (pitchClasses(c.chord) == pcs)
                return &c;
        return nullptr;
    }
}

TEST_CASE("TriadLibrary: 11 qualities on 12 roots including sevenths and inversions", "[NextChord][TriadLibrary]")
{
    const auto all = TriadLibrary::allTriads(Key::C);
    REQUIRE(all.size() == TriadLibrary::kNumNamedChords);
    REQUIRE(all.size() == 432); // root positions + inversions

    int sus2 = 0, sus4 = 0, power = 0, sevenths = 0, slash = 0, rootPosition = 0;
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
        if (c.symbol.find('/') != std::string::npos)
            ++slash;
        else
            ++rootPosition;
    }
    // Each quality × inversions: sus2/sus4 are 3-note → 3 inv × 12 roots.
    CHECK(sus2 == 36);
    CHECK(sus4 == 36);
    CHECK(power == 24);     // 2-note × 12 roots
    CHECK(sevenths == 192); // 4 qualities × 4 inv × 12
    CHECK(rootPosition == TriadLibrary::kNumRootPositionChords);
    CHECK(slash == TriadLibrary::kNumNamedChords - TriadLibrary::kNumRootPositionChords);

    // Unique pitch-class sets are unchanged by inversions (same tones, different bass).
    // Pre-seventh unique sonorities: 64; + 48 seventh sets → 112.
    std::set<std::set<int>> uniqueSets;
    for (const auto& c : all)
        uniqueSets.insert(pitchClasses(c));
    CHECK(uniqueSets.size() == 112);
}

TEST_CASE("TriadLibrary: inversions are bass-first slash chords with root role preserved", "[NextChord][TriadLibrary]")
{
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto cOverE = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 1);
    const auto cOverG = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 2);

    CHECK(c.symbol == "C");
    CHECK(cOverE.symbol == "C/E");
    CHECK(cOverG.symbol == "C/G");

    REQUIRE(cOverE.notes.size() == 3);
    CHECK(cOverE.notes.front().rawNote == "E"); // bass
    CHECK(NextChordScorer::bassPitchClass(cOverE) == 4);
    CHECK(NextChordScorer::rootPitchClass(cOverE) == 0); // still C
    CHECK(pitchClasses(cOverE) == std::set<int>({ 0, 4, 7 }));

    const auto g7Third = TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C, 3);
    CHECK(g7Third.symbol == "G7/F"); // 7th in bass
    CHECK(NextChordScorer::rootPitchClass(g7Third) == 7);
    CHECK(NextChordScorer::bassPitchClass(g7Third) == 5);
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

TEST_CASE("NextChordGenerator: pool includes sevenths and inversions; drama reorders list", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto current = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);

    const auto candidates = NextChordGenerator::generate(current, keyScale, 0.0f);
    // Catalogue emits 432 named chords, but enharmonic equivalents collapse by (bass, pcs)
    // (e.g. C+ ≡ E+ ≡ G#+, Csus2 ≡ Gsus4). Unique voicings minus current C: 371.
    REQUIRE(candidates.size() == 371);

    // drama=0 → softest first (monotone non-decreasing rankingScore; display tension tracks it).
    for (std::size_t i = 1; i < candidates.size(); ++i)
        CHECK(candidates[i - 1].rankingScore <= candidates[i].rankingScore + 1.0e-5f);

    REQUIRE(findByPcs(candidates, { 0, 5, 7 }) != nullptr);       // Csus4
    REQUIRE(findByPcs(candidates, { 0, 2, 7 }) != nullptr);       // Csus2
    REQUIRE(findByPcs(candidates, { 0, 7 }) != nullptr);          // C5
    REQUIRE(findByPcs(candidates, { 0, 4, 7, 11 }) != nullptr);   // Cmaj7
    REQUIRE(findByPcs(candidates, { 0, 4, 7, 10 }) != nullptr);   // C7
    REQUIRE(findByPcs(candidates, { 2, 5, 9, 0 }) != nullptr);    // Dm7
    REQUIRE(findByPcs(candidates, { 7, 11, 2, 5 }) != nullptr);   // G7

    // Inversions of the current harmony are offered as separate smooth moves.
    const auto findBySymbol = [&](const std::string& symbol) -> const NextChordCandidate*
    {
        for (const auto& c : candidates)
            if (c.chord.symbol == symbol)
                return &c;
        return nullptr;
    };
    REQUIRE(findBySymbol("C/E") != nullptr);
    REQUIRE(findBySymbol("C/G") != nullptr);
    REQUIRE(findBySymbol("C") == nullptr); // current voicing excluded

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
        CHECK(wild[i - 1].rankingScore + 1.0e-5f >= wild[i].rankingScore);

    // Soft band is differentiated — not a wall of zeros at the top of the calm list.
    int zerosInTop20 = 0;
    for (std::size_t i = 0; i < std::min<std::size_t>(20, candidates.size()); ++i)
    {
        if (candidates[i].tensionPercent <= 0)
            ++zerosInTop20;
    }
    CHECK(zerosInTop20 == 0);

    // Distinct soft moves should not all share the exact same display tension.
    std::set<int> topTensions;
    for (std::size_t i = 0; i < std::min<std::size_t>(12, candidates.size()); ++i)
        topTensions.insert(candidates[i].tensionPercent);
    CHECK(topTensions.size() >= 4);
}

TEST_CASE("NextChordScorer: soft diatonic moves stay differentiated above zero", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C);
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    const int tG = tensionOf(c, g, keyScale, Degree::V);
    const int tAm = tensionOf(c, am, keyScale, Degree::VI);
    const int tF = tensionOf(c, f, keyScale, Degree::IV);
    const int tDm = tensionOf(c, dm, keyScale, Degree::II);
    const int tFs = tensionOf(c, fs, keyScale);

    // No pile-up at 0 for the common diatonic set.
    CHECK(tG >= 2);
    CHECK(tAm >= 2);
    CHECK(tF >= 2);
    CHECK(tDm >= 2);

    // Remote still harder than primaries.
    CHECK(tG < tFs);
    CHECK(tAm < tFs);
    CHECK(tF < tFs);

    // Soft primaries are not all identical after rounding.
    const std::set<int> soft { tG, tAm, tF, tDm };
    CHECK(soft.size() >= 2);
}

TEST_CASE("NextChordScorer: closer voice-leading and inversions rank as lower tension", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 0);
    const auto cOverE = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 1);
    const auto cOverG = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C, 2);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C, 0);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C, 0);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C, 0);
    const auto gOverB = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C, 1); // G/B

    // Pure inversion of the same chord is softer than a remote root jump.
    CHECK(tensionOf(c, cOverE, keyScale, Degree::I) < tensionOf(c, fs, keyScale));
    CHECK(tensionOf(c, cOverG, keyScale, Degree::I) < tensionOf(c, fs, keyScale));

    // Same-harmony inversion is softer than a typical diatonic neighbour on common drama.
    CHECK(tensionOf(c, cOverE, keyScale, Degree::I) < tensionOf(c, am, keyScale, Degree::VI));

    // Voice-leading: C → G/B (stepwise bass C→B, shared G) softer than C → G root position
    // (bass leap C→G) — closer bass motion = less tension.
    CHECK(tensionOf(c, gOverB, keyScale, Degree::V) < tensionOf(c, g, keyScale, Degree::V));

    // MIDI voice-leading cost itself is lower for inversion change than for a tritone jump.
    CHECK(NextChordScorer::voiceLeadingCost(c, cOverE) < NextChordScorer::voiceLeadingCost(c, fs));
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
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto bb = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::C);

    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::A);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::A); // bVII in A minor

    const int tBbInMajor = tensionOf(c, bb, major);
    const int tGInMinor = tensionOf(am, g, minor, Degree::VII);

    CHECK(tGInMinor < tBbInMajor);
}

TEST_CASE("NextChordScorer: progression grammar favours cadences and ii-V", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto em = TriadLibrary::makeTriad(4, TriadQuality::Minor, Key::C);
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C);
    const auto bdim = TriadLibrary::makeTriad(11, TriadQuality::Diminished, Key::C);
    const auto g7 = TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C);

    // V → I cadence softer than III → I
    CHECK(tensionOf(g, c, keyScale, Degree::I) < tensionOf(em, c, keyScale, Degree::I));

    // ii → V softer than ii → iii
    CHECK(tensionOf(dm, g, keyScale, Degree::V) < tensionOf(dm, em, keyScale, Degree::III));

    // IV → I plagal softer than III → I
    CHECK(tensionOf(f, c, keyScale, Degree::I) < tensionOf(em, c, keyScale, Degree::I));

    // V7 → I softer than V7 → random remote
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);
    CHECK(tensionOf(g7, c, keyScale, Degree::I) < tensionOf(g7, fs, keyScale));

    // Deceptive V → vi still fairly soft (not remote)
    CHECK(tensionOf(g, am, keyScale, Degree::VI) < tensionOf(g, fs, keyScale));

    // vii° → I approach
    CHECK(tensionOf(bdim, c, keyScale, Degree::I) < tensionOf(bdim, fs, keyScale));
}

TEST_CASE("NextChordScorer: quality fitness rewards expected diatonic colour", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto gMaj = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto gMin = TriadLibrary::makeTriad(7, TriadQuality::Minor, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto dMaj = TriadLibrary::makeTriad(2, TriadQuality::Major, Key::C);

    // V wants major, not minor
    CHECK(tensionOf(c, gMaj, keyScale, Degree::V) < tensionOf(c, gMin, keyScale, Degree::V));

    // ii wants minor, not major
    CHECK(tensionOf(c, dm, keyScale, Degree::II) < tensionOf(c, dMaj, keyScale, Degree::II));
}

TEST_CASE("NextChordScorer: secondary dominant and tritone sub are idiomatic colour", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto d7 = TriadLibrary::makeTriad(2, TriadQuality::Dominant7, Key::C);   // V/V
    const auto a7 = TriadLibrary::makeTriad(9, TriadQuality::Dominant7, Key::C);   // V/ii
    const auto db7 = TriadLibrary::makeTriad(1, TriadQuality::Dominant7, Key::C);  // subV/I
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);       // remote

    // Secondary dominants softer than a random remote major
    CHECK(tensionOf(c, d7, keyScale) < tensionOf(c, fs, keyScale));
    CHECK(tensionOf(c, a7, keyScale) < tensionOf(c, fs, keyScale));

    // Tritone sub of V is colourful but more idiomatic than F# major
    CHECK(tensionOf(c, db7, keyScale) < tensionOf(c, fs, keyScale));
}

TEST_CASE("NextChordScorer: mode mixture bVI/bVII softer than random chromatic", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto ab = TriadLibrary::makeTriad(8, TriadQuality::Major, Key::C);  // bVI
    const auto bb = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::C); // bVII
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    CHECK(tensionOf(c, ab, keyScale) < tensionOf(c, fs, keyScale));
    CHECK(tensionOf(c, bb, keyScale) < tensionOf(c, fs, keyScale));
}

TEST_CASE("NextChordScorer: mixture iv (Fm after C) ranks harder than diatonic F/Am/G", "[NextChord]")
{
    // Regression: C→Fm used to get a strong "mixture iv" softness bonus while sharing the same
    // soft I→IV root leap as C→F, so it floated near the top of the calm list.
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto f = TriadLibrary::makeTriad(5, TriadQuality::Major, Key::C);
    const auto fm = TriadLibrary::makeTriad(5, TriadQuality::Minor, Key::C);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    const int tF = tensionOf(c, f, keyScale, Degree::IV);
    const int tFm = tensionOf(c, fm, keyScale);
    const int tAm = tensionOf(c, am, keyScale, Degree::VI);
    const int tG = tensionOf(c, g, keyScale, Degree::V);
    const int tFs = tensionOf(c, fs, keyScale);

    CHECK(tF < tFm);
    CHECK(tAm < tFm);
    CHECK(tG < tFm);
    // Still recognisable colour, not as remote as F#.
    CHECK(tFm < tFs);

    // Calm generate: Fm must not beat diatonic F in list order.
    const auto calm = NextChordGenerator::generate(c, keyScale, 0.0f);
    const auto rankOf = [&](const std::string& symbol) -> int
    {
        for (std::size_t i = 0; i < calm.size(); ++i)
            if (calm[i].chord.symbol == symbol)
                return static_cast<int>(i);
        return -1;
    };
    const int rF = rankOf("F");
    const int rFm = rankOf("Fm");
    REQUIRE(rF >= 0);
    REQUIRE(rFm >= 0);
    CHECK(rF < rFm);
}

TEST_CASE("NextChordScorer: directed root interval and degree helpers", "[NextChord]")
{
    CHECK(NextChordScorer::directedRootInterval(0, 7) == 7);  // C→G up 5th
    CHECK(NextChordScorer::directedRootInterval(7, 0) == 5);  // G→C up 4th
    CHECK(NextChordScorer::directedRootInterval(0, 0) == 0);

    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    CHECK(NextChordScorer::keyTonicPitchClass(keyScale) == 0);
    CHECK(NextChordScorer::degreeOfRoot(0, keyScale) == Degree::I);
    CHECK(NextChordScorer::degreeOfRoot(7, keyScale) == Degree::V);
    CHECK(NextChordScorer::degreeOfRoot(9, keyScale) == Degree::VI);
    CHECK_FALSE(NextChordScorer::degreeOfRoot(1, keyScale).has_value()); // Db not diatonic

    CHECK(NextChordScorer::roleFor(Degree::I, TriadQuality::Major, NextChordScorer::ScaleFamily::Majorish)
          == NextChordScorer::HarmonicRole::Tonic);
    CHECK(NextChordScorer::roleFor(Degree::V, TriadQuality::Dominant7, NextChordScorer::ScaleFamily::Majorish)
          == NextChordScorer::HarmonicRole::Dominant);
    CHECK(NextChordScorer::roleFor(Degree::II, TriadQuality::Minor, NextChordScorer::ScaleFamily::Majorish)
          == NextChordScorer::HarmonicRole::Predominant);
}

TEST_CASE("NextChordScorer: dominant wants its resolution target", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto g7 = TriadLibrary::makeTriad(7, TriadQuality::Dominant7, Key::C);
    const auto d7 = TriadLibrary::makeTriad(2, TriadQuality::Dominant7, Key::C); // V/V
    const auto db7 = TriadLibrary::makeTriad(1, TriadQuality::Dominant7, Key::C); // subV
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    // G7 → C much softer than G7 → F#
    CHECK(tensionOf(g7, c, keyScale, Degree::I) < tensionOf(g7, fs, keyScale));

    // Secondary D7 prefers G over a remote chord
    CHECK(tensionOf(d7, g, keyScale, Degree::V) < tensionOf(d7, fs, keyScale));

    // Tritone sub Db7 → C softer than Db7 → F#
    CHECK(tensionOf(db7, c, keyScale, Degree::I) < tensionOf(db7, fs, keyScale));

    // Abandoning a dominant (G7 → Dm) is tenser than resolving (G7 → C)
    CHECK(tensionOf(g7, c, keyScale, Degree::I) < tensionOf(g7, dm, keyScale, Degree::II));
}

TEST_CASE("NextChordScorer: backdoor, sus resolve, blues I7, secondary weights", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto bb = TriadLibrary::makeTriad(10, TriadQuality::Major, Key::C); // bVII
    const auto fm = TriadLibrary::makeTriad(5, TriadQuality::Minor, Key::C);  // iv
    const auto gsus = TriadLibrary::makeTriad(7, TriadQuality::Sus4, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto c7 = TriadLibrary::makeTriad(0, TriadQuality::Dominant7, Key::C); // I7 blues
    const auto cs = TriadLibrary::makeTriad(1, TriadQuality::Major, Key::C);     // random-ish
    const auto d7 = TriadLibrary::makeTriad(2, TriadQuality::Dominant7, Key::C); // V/V
    const auto b7 = TriadLibrary::makeTriad(11, TriadQuality::Dominant7, Key::C); // V/iii
    const auto fs = TriadLibrary::makeTriad(6, TriadQuality::Major, Key::C);

    // Backdoor bVII → I softer than remote approach to I
    CHECK(tensionOf(bb, c, keyScale, Degree::I) < tensionOf(fs, c, keyScale, Degree::I));

    // Minor plagal iv → I softer than remote → I
    CHECK(tensionOf(fm, c, keyScale, Degree::I) < tensionOf(fs, c, keyScale, Degree::I));

    // Sus resolve same root
    CHECK(tensionOf(gsus, g, keyScale, Degree::V) < tensionOf(gsus, fs, keyScale));

    // Blues I7 from V is not wilder than a random chromatic major from V
    CHECK(tensionOf(g, c7, keyScale, Degree::I) < tensionOf(g, cs, keyScale));

    // V/V more idiomatic than V/iii as a next chord from I
    CHECK(tensionOf(c, d7, keyScale) < tensionOf(c, b7, keyScale));
}

TEST_CASE("NextChordScorer: sequence context completes ii-V-I and continues fifths", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto dm = TriadLibrary::makeTriad(2, TriadQuality::Minor, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);
    const auto em = TriadLibrary::makeTriad(4, TriadQuality::Minor, Key::C);
    const auto am = TriadLibrary::makeTriad(9, TriadQuality::Minor, Key::C);

    // After Dm → G, C (I) is softer than Em as the next move.
    const auto afterIiV = history({ { dm, Degree::II } });
    CHECK(tensionOf(g, c, keyScale, Degree::I, NextChordScorer::kDefaultDrama, afterIiV)
          < tensionOf(g, em, keyScale, Degree::III, NextChordScorer::kDefaultDrama, afterIiV));

    // Falling-fifths chain: Am → Dm (already +5) then G continues the chain better than Em.
    const auto afterAm = history({ { am, Degree::VI } });
    CHECK(tensionOf(dm, g, keyScale, Degree::V, NextChordScorer::kDefaultDrama, afterAm)
          < tensionOf(dm, em, keyScale, Degree::III, NextChordScorer::kDefaultDrama, afterAm));

    // Exact repeat of a recent chord is penalised vs a fresh diatonic move.
    const auto afterC = history({ { c, Degree::I } });
    // Current Am, history C: going back to C soon is tenser than going to F would be... use Dm.
    CHECK(tensionOf(am, c, keyScale, Degree::I, NextChordScorer::kDefaultDrama, afterC)
          > tensionOf(am, dm, keyScale, Degree::II, NextChordScorer::kDefaultDrama, afterC) - 5);
}

TEST_CASE("buildSequenceContext: reconstructs ordered history and drops matching current", "[NextChord]")
{
    const auto& keyScale = ChordDatabase::getInstance().get(Key::C, Scale::Major);
    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto g = TriadLibrary::makeTriad(7, TriadQuality::Major, Key::C);

    MidiEditorState state;
    // Block 0: C major at beat 0 (MIDI C3 E3 G3 roughly)
    state.chordBlocks.push_back(MidiEditorChordBlockState {
        0, "C", 0.0, 4.0, { Degree::I, 1 }
    });
    state.notes.push_back(MidiEditorNoteState { 60, 0.0, 4.0, 0 });
    state.notes.push_back(MidiEditorNoteState { 64, 0.0, 4.0, 0 });
    state.notes.push_back(MidiEditorNoteState { 67, 0.0, 4.0, 0 });
    // Block 1: G major at beat 4
    state.chordBlocks.push_back(MidiEditorChordBlockState {
        1, "G", 4.0, 4.0, { Degree::V, 1 }
    });
    state.notes.push_back(MidiEditorNoteState { 67, 4.0, 4.0, 1 });
    state.notes.push_back(MidiEditorNoteState { 71, 4.0, 4.0, 1 });
    state.notes.push_back(MidiEditorNoteState { 74, 4.0, 4.0, 1 });

    auto full = buildSequenceContext(state, keyScale, nullptr);
    REQUIRE(full.size() == 2);
    CHECK(pitchClasses(full.previous[0].chord) == pitchClasses(c));
    CHECK(pitchClasses(full.previous[1].chord) == pitchClasses(g));

    // When current is G, drop the last block from previous.
    auto beforeG = buildSequenceContext(state, keyScale, &g);
    REQUIRE(beforeG.size() == 1);
    CHECK(pitchClasses(beforeG.previous[0].chord) == pitchClasses(c));
}
