#include "Theory/NextChordGenerator.h"

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

    using IdeaKey = std::tuple<int, int>; // rootPc, quality enum

    IdeaKey ideaKey(const Chord& chord)
    {
        const int root = NextChordScorer::rootPitchClass(chord);
        const auto quality = static_cast<int>(NextChordScorer::detectTriadQuality(chord));
        return { root, quality };
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

    // 1) Mechanism-driven ideas first (functionally motivated).
    for (auto& c : MechanismCandidateGenerator::generate(currentChord, keyScale, sequence))
        consider(std::move(c));

    // 2) Full catalogue for coverage (still idea-deduped later).
    for (const auto& chord : TriadLibrary::allTriads(keyScale.key))
    {
        NextChordCandidate candidate;
        candidate.chord = chord;
        candidate.degree = matchingDegree(chord, keyScale);
        consider(std::move(candidate));
    }

    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates, drama01, sequence);

    // 3) One-step lookahead: reward productive tension (strong available continuation).
    constexpr float kLookaheadWeight = 0.22f;
    for (auto& candidate : candidates)
    {
        const float productivity = lookaheadProductivity(candidate.chord, keyScale, drama01);
        candidate.rankingScore += kLookaheadWeight * productivity;
        if (productivity > 0.55f && candidate.metrics.tension > 0.4f)
        {
            // Tag productive tension lightly if reason is sparse.
            if (candidate.reasonLabel.find("→") == std::string::npos
                && candidate.metrics.resolution > 0.35f)
            {
                // Keep label clean; productivity only affects rank.
            }
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
        [](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            if (std::abs(a.rankingScore - b.rankingScore) > 1.0e-5f)
                return a.rankingScore > b.rankingScore;
            if (a.fitPercent != b.fitPercent)
                return a.fitPercent > b.fitPercent;
            return a.chord.symbol < b.chord.symbol;
        });

    // 4) Diversity: one voicing per harmonic idea (root + quality).
    std::vector<NextChordCandidate> diverse;
    diverse.reserve(candidates.size());
    std::map<IdeaKey, std::size_t> bestIndexByIdea;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const auto key = ideaKey(candidates[i].chord);
        if (bestIndexByIdea.find(key) != bestIndexByIdea.end())
            continue;
        bestIndexByIdea.emplace(key, diverse.size());
        diverse.push_back(std::move(candidates[i]));
    }

    return diverse;
}

}
