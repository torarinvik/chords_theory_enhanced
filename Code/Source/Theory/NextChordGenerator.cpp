#include "Theory/NextChordGenerator.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <vector>

#include "Theory/MechanismCandidateGenerator.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

namespace
{
    std::set<int> pitchClassSet(const Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }

    using VoicingKey = std::tuple<int, std::set<int>>;

    VoicingKey voicingKey(const Chord& chord)
    {
        const int bass = chord.notes.empty() ? -1 : chord.notes.front().getPitchClass();
        return { bass, pitchClassSet(chord) };
    }

    bool sameVoicing(const Chord& a, const Chord& b)
    {
        return voicingKey(a) == voicingKey(b);
    }

    // Harmonic destination family: root + coarse family kind (not every voicing/quality).
    using FamilyKey = std::pair<int, int>;

    FamilyKey familyKeyOf(const NextChordCandidate& c)
    {
        if (c.familyRootPc >= 0 && c.familyKind >= 0)
            return { c.familyRootPc, c.familyKind };
        const int root = NextChordScorer::rootPitchClass(c.chord);
        const int kind = static_cast<int>(
            NextChordScorer::familyKindForQuality(NextChordScorer::detectTriadQuality(c.chord)));
        return { root, kind };
    }

    // Prefer simpler ordinary representatives within a family (root position F over F/C / Fmaj7).
    float representativeScore(const NextChordCandidate& c)
    {
        return c.rankingScore - 0.55f * c.metrics.complexity;
    }

    std::optional<Degree> matchingDegree(const Chord& sonority, const KeyScaleData& keyScale)
    {
        const auto pcs = pitchClassSet(sonority);
        for (const auto& degreeData : keyScale.degrees)
        {
            for (const auto& chord : degreeData.chords)
            {
                if (pitchClassSet(chord) == pcs)
                    return degreeData.degree;
            }
        }
        return std::nullopt;
    }
}

std::vector<NextChordCandidate> NextChordGenerator::generate(const Chord& currentChord, const KeyScaleData& keyScale,
                                                             float drama01, const SequenceContext& sequence)
{
    if (currentChord.notes.empty())
        return {};

    std::vector<NextChordCandidate> candidates;
    std::set<VoicingKey> seenVoicings;

    auto consider = [&](NextChordCandidate candidate)
    {
        if (candidate.chord.notes.empty())
            return;
        if (sameVoicing(candidate.chord, currentChord))
            return;
        if (!seenVoicings.insert(voicingKey(candidate.chord)).second)
            return;
        if (!candidate.degree)
            candidate.degree = matchingDegree(candidate.chord, keyScale);
        candidates.push_back(std::move(candidate));
    };

    // 1) Mechanism-driven ideas (functionally motivated roots/qualities).
    for (auto& c : MechanismCandidateGenerator::generate(currentChord, keyScale, sequence))
        consider(std::move(c));

    // 2) Full catalogue for coverage (all inversions compete only within family later).
    for (const auto& chord : TriadLibrary::allTriads(keyScale.key))
    {
        NextChordCandidate candidate;
        candidate.chord = chord;
        candidate.degree = matchingDegree(chord, keyScale);
        consider(std::move(candidate));
    }

    // 3) Score every voicing (five independent axes + ranking).
    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates, drama01, sequence);

    // 4) One-step lookahead → resolution potential (productive tension).
    constexpr float kLookaheadWeight = 0.30f;
    for (auto& candidate : candidates)
    {
        const float productivity = lookaheadProductivity(candidate.chord, keyScale, drama01);
        // Blend into resolution metric (keeps axes meaningful for diagnostics).
        candidate.metrics.resolution = std::clamp(
            0.65f * candidate.metrics.resolution + 0.35f * productivity, 0.0f, 1.0f);
        candidate.resolutionPercent =
            std::clamp(static_cast<int>(std::lround(candidate.metrics.resolution * 100.0f)), 0, 100);
        candidate.rankingScore += kLookaheadWeight * productivity;
    }

    // 5) Destination-first: one representative per harmonic family.
    //    Within family, pick best by representativeScore (ranking − complexity).
    std::map<FamilyKey, std::size_t> bestIndexByFamily;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const auto key = familyKeyOf(candidates[i]);
        const auto it = bestIndexByFamily.find(key);
        if (it == bestIndexByFamily.end())
        {
            bestIndexByFamily.emplace(key, i);
            continue;
        }
        if (representativeScore(candidates[i]) > representativeScore(candidates[it->second]) + 1.0e-5f)
            it->second = i;
        else if (std::abs(representativeScore(candidates[i]) - representativeScore(candidates[it->second]))
                     <= 1.0e-5f
                 && candidates[i].metrics.complexity < candidates[it->second].metrics.complexity)
        {
            it->second = i;
        }
    }

    std::vector<NextChordCandidate> diverse;
    diverse.reserve(bestIndexByFamily.size());
    for (const auto& [key, index] : bestIndexByFamily)
    {
        (void)key;
        diverse.push_back(std::move(candidates[index]));
    }

    // 6) Rank families by destination score (not by which inversion gamed Fit).
    std::stable_sort(diverse.begin(), diverse.end(),
        [](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            if (std::abs(a.rankingScore - b.rankingScore) > 1.0e-5f)
                return a.rankingScore > b.rankingScore;
            if (a.fitPercent != b.fitPercent)
                return a.fitPercent > b.fitPercent;
            if (std::abs(a.metrics.complexity - b.metrics.complexity) > 1.0e-5f)
                return a.metrics.complexity < b.metrics.complexity;
            return a.chord.symbol < b.chord.symbol;
        });

    return diverse;
}

}
