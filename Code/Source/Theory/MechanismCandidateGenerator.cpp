#include "Theory/MechanismCandidateGenerator.h"

#include <algorithm>
#include <set>
#include <unordered_set>

#include "Theory/HarmonicPredicates.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

namespace
{
    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }

    std::set<int> pitchClassSet(const Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }

    using IdeaKey = std::uint32_t;
    IdeaKey ideaKey(int rootPc, TriadQuality q)
    {
        return static_cast<std::uint32_t>((rootPc & 15) | (static_cast<int>(q) << 4));
    }

    void addIdea(std::vector<NextChordCandidate>& out, std::unordered_set<IdeaKey>& seen,
                 int rootPc, TriadQuality quality, Key spellKey,
                 std::optional<Degree> degree = std::nullopt)
    {
        const auto key = ideaKey(rootPc, quality);
        if (!seen.insert(key).second)
            return;

        NextChordCandidate c;
        c.chord = TriadLibrary::makeTriad(rootPc, quality, spellKey, 0);
        c.degree = degree;
        out.push_back(std::move(c));
    }

    std::optional<Degree> degreeForRoot(int rootPc, const KeyScaleData& keyScale)
    {
        return NextChordScorer::degreeOfRoot(rootPc, keyScale);
    }
}

std::optional<int> inferLocalTonicPc(const SequenceContext& sequence, const KeyScaleData& keyScale)
{
    if (sequence.empty())
        return std::nullopt;

    // Walk newest → oldest; first confident secondary dominant implies its target as local tonic.
    for (int i = sequence.size() - 1; i >= 0; --i)
    {
        const auto& event = sequence.previous[static_cast<std::size_t>(i)];
        const auto sec = analyseSecondaryDominant(event.chord, keyScale);
        if (sec.hit && sec.confidence >= kMinLabelConfidence)
            return sec.targetRootPc;

        const auto sub = analyseTritoneSubstitution(event.chord, keyScale);
        if (sub.hit && sub.confidence >= kMinLabelConfidence)
            return sub.resolveRootPc;
    }

    return std::nullopt;
}

float lookaheadProductivity(const Chord& fromCandidate, const KeyScaleData& keyScale, float drama01)
{
    // Small resolution pool: diatonic triads + I/i and a few functional targets.
    const int tonic = NextChordScorer::keyTonicPitchClass(keyScale);
    const auto spell = keyScale.key;

    std::vector<Chord> resolvePool;
    resolvePool.reserve(16);

    // Global tonic major + minor colour
    resolvePool.push_back(TriadLibrary::makeTriad(tonic, TriadQuality::Major, spell, 0));
    resolvePool.push_back(TriadLibrary::makeTriad(tonic, TriadQuality::Minor, spell, 0));

    for (const auto& note : keyScale.scaleNotes)
    {
        const int pc = note.getPitchClass();
        // Prefer expected quality lightly: try both major and minor for diatonic roots.
        resolvePool.push_back(TriadLibrary::makeTriad(pc, TriadQuality::Major, spell, 0));
        resolvePool.push_back(TriadLibrary::makeTriad(pc, TriadQuality::Minor, spell, 0));
    }

    // If candidate is secondary/subV, include its formal target explicitly.
    if (const auto sec = analyseSecondaryDominant(fromCandidate, keyScale); sec.hit)
        resolvePool.push_back(TriadLibrary::makeTriad(sec.targetRootPc, TriadQuality::Minor, spell, 0));
    if (const auto sub = analyseTritoneSubstitution(fromCandidate, keyScale); sub.hit)
        resolvePool.push_back(TriadLibrary::makeTriad(sub.resolveRootPc, TriadQuality::Major, spell, 0));

    float best = -1.0e9f;
    std::set<std::set<int>> seen;
    for (const auto& next : resolvePool)
    {
        if (next.notes.empty())
            continue;
        if (!seen.insert(pitchClassSet(next)).second)
            continue;

        NextChordCandidate probe;
        probe.chord = next;
        probe.degree = degreeForRoot(NextChordScorer::rootPitchClass(next), keyScale);
        NextChordScorer::score(fromCandidate, keyScale, probe, drama01, {});
        best = std::max(best, probe.rankingScore);
    }

    // Map ranking into a 0–1 productivity bonus (rankingScore roughly -0.5…1.5).
    if (best < -1.0e8f)
        return 0.0f;
    return std::clamp((best + 0.2f) * 0.45f, 0.0f, 1.0f);
}

std::vector<NextChordCandidate> MechanismCandidateGenerator::generate(const Chord& currentChord,
                                                                      const KeyScaleData& keyScale,
                                                                      const SequenceContext& sequence)
{
    std::vector<NextChordCandidate> out;
    out.reserve(64);
    std::unordered_set<IdeaKey> seen;

    const auto spell = keyScale.key;
    const int tonic = NextChordScorer::keyTonicPitchClass(keyScale);
    const int fromRoot = NextChordScorer::rootPitchClass(currentChord);
    const auto localTonic = inferLocalTonicPc(sequence, keyScale);

    // --- Diatonic degrees (triad + 7th colours) ---------------------------------
    for (const auto& degData : keyScale.degrees)
    {
        if (degData.chords.empty())
            continue;
        // Use database default voicing idea via its root/quality when possible.
        const auto& def = degData.chords.front();
        const int root = NextChordScorer::rootPitchClass(def);
        const auto q = NextChordScorer::detectTriadQuality(def);
        addIdea(out, seen, root, q, spell, degData.degree);

        // Also emit common 7th on the same root.
        if (q == TriadQuality::Major)
            addIdea(out, seen, root, TriadQuality::Major7, spell, degData.degree);
        else if (q == TriadQuality::Minor)
            addIdea(out, seen, root, TriadQuality::Minor7, spell, degData.degree);
        else if (q == TriadQuality::Diminished)
            addIdea(out, seen, root, TriadQuality::HalfDim7, spell, degData.degree);

        // Primary V7 on V degree
        if (degData.degree == Degree::V)
            addIdea(out, seen, root, TriadQuality::Dominant7, spell, Degree::V);
    }

    // --- Secondary dominants V7/x for each diatonic degree except I ---------------
    for (const auto& degData : keyScale.degrees)
    {
        if (degData.degree == Degree::I)
            continue;
        const int targetRoot = degData.chords.empty()
            ? -1
            : NextChordScorer::rootPitchClass(degData.chords.front());
        if (targetRoot < 0)
            continue;
        const int vRoot = mod12(targetRoot + 7); // V of target
        addIdea(out, seen, vRoot, TriadQuality::Dominant7, spell, std::nullopt);
        // Secondary leading-tone half-dim on vii of target (root = target - 1)
        addIdea(out, seen, mod12(targetRoot + 11), TriadQuality::HalfDim7, spell, std::nullopt);
        addIdea(out, seen, mod12(targetRoot + 11), TriadQuality::Diminished, spell, std::nullopt);
    }

    // --- Tritone subs of V and of secondary V ------------------------------------
    // subV/I = bII7
    addIdea(out, seen, mod12(tonic + 1), TriadQuality::Dominant7, spell, std::nullopt);
    for (const auto& degData : keyScale.degrees)
    {
        if (degData.degree == Degree::I || degData.chords.empty())
            continue;
        const int targetRoot = NextChordScorer::rootPitchClass(degData.chords.front());
        // subV of target: root = target + 6
        addIdea(out, seen, mod12(targetRoot + 6), TriadQuality::Dominant7, spell, std::nullopt);
    }

    // --- Modal mixture (majorish keys) -------------------------------------------
    const auto family = NextChordScorer::scaleFamily(keyScale.scale);
    if (family == NextChordScorer::ScaleFamily::Majorish
        || family == NextChordScorer::ScaleFamily::ModalSoft)
    {
        addIdea(out, seen, mod12(tonic + 5), TriadQuality::Minor, spell, std::nullopt);   // iv
        addIdea(out, seen, mod12(tonic + 8), TriadQuality::Major, spell, std::nullopt);   // bVI
        addIdea(out, seen, mod12(tonic + 10), TriadQuality::Major, spell, std::nullopt);  // bVII
        addIdea(out, seen, mod12(tonic + 3), TriadQuality::Major, spell, std::nullopt);   // bIII
        addIdea(out, seen, mod12(tonic + 1), TriadQuality::Major, spell, std::nullopt);   // Neapolitan
        addIdea(out, seen, tonic, TriadQuality::Minor, spell, Degree::I);                 // parallel i
    }

    // --- Chromatic mediants from current root ------------------------------------
    for (const int step : { 3, 4, 8, 9 })
    {
        const int r = mod12(fromRoot + step);
        addIdea(out, seen, r, TriadQuality::Major, spell, std::nullopt);
        addIdea(out, seen, r, TriadQuality::Minor, spell, std::nullopt);
    }

    // --- Local tonic: prefer its triad / V7 if sequence implies tonicization -------
    if (localTonic)
    {
        addIdea(out, seen, *localTonic, TriadQuality::Minor, spell, degreeForRoot(*localTonic, keyScale));
        addIdea(out, seen, *localTonic, TriadQuality::Major, spell, degreeForRoot(*localTonic, keyScale));
        addIdea(out, seen, mod12(*localTonic + 7), TriadQuality::Dominant7, spell, std::nullopt);
    }

    // --- Approach chords by semitone into current root / diatonic targets ----------
    for (const int target : { fromRoot, tonic })
    {
        addIdea(out, seen, mod12(target + 1), TriadQuality::Major, spell, std::nullopt);
        addIdea(out, seen, mod12(target + 11), TriadQuality::Major, spell, std::nullopt);
        addIdea(out, seen, mod12(target + 1), TriadQuality::Dominant7, spell, std::nullopt);
        addIdea(out, seen, mod12(target + 11), TriadQuality::Dominant7, spell, std::nullopt);
    }

    // Sus on V and I
    addIdea(out, seen, tonic, TriadQuality::Sus4, spell, Degree::I);
    addIdea(out, seen, mod12(tonic + 7), TriadQuality::Sus4, spell, Degree::V);
    addIdea(out, seen, mod12(tonic + 7), TriadQuality::Sus2, spell, Degree::V);

    // Drop exact current voicing idea if present.
    const auto curKey = ideaKey(fromRoot, NextChordScorer::detectTriadQuality(currentChord));
    out.erase(std::remove_if(out.begin(), out.end(),
                  [&](const NextChordCandidate& c) {
                      return ideaKey(NextChordScorer::rootPitchClass(c.chord),
                                     NextChordScorer::detectTriadQuality(c.chord)) == curKey
                          && pitchClassSet(c.chord) == pitchClassSet(currentChord);
                  }),
              out.end());

    return out;
}

}
