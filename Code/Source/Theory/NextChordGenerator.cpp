#include "Theory/NextChordGenerator.h"

#include <map>
#include <set>
#include <tuple>
#include <vector>

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

    // Harmonic idea identity: harmonic root + quality (inversions of F collapse to one idea).
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

    const auto catalogue = TriadLibrary::allTriads(keyScale.key);

    std::vector<NextChordCandidate> candidates;
    candidates.reserve(catalogue.size());
    std::set<VoicingKey> seenVoicings;

    for (const auto& chord : catalogue)
    {
        if (!seenVoicings.insert(voicingKey(chord)).second)
            continue;

        if (sameVoicing(chord, currentChord))
            continue;

        NextChordCandidate candidate;
        candidate.chord = chord;
        candidate.degree = matchingDegree(chord, keyScale);
        candidates.push_back(std::move(candidate));
    }

    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates, drama01, sequence);

    // Diversity: keep one default voicing per harmonic idea (root + quality), preferring
    // root-position. Remaining inversions/extensions of the same idea are dropped from the
    // top-level list so the UI is not flooded with F/C, F5/C, Fm/C, …
    std::vector<NextChordCandidate> diverse;
    diverse.reserve(candidates.size());
    std::map<IdeaKey, std::size_t> bestIndexByIdea;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const auto key = ideaKey(candidates[i].chord);
        const auto it = bestIndexByIdea.find(key);
        if (it == bestIndexByIdea.end())
        {
            bestIndexByIdea.emplace(key, diverse.size());
            diverse.push_back(std::move(candidates[i]));
            continue;
        }

        // Already have this idea higher in the ranked list — skip lower-ranked voicings.
        // (scoreAndSort already ordered best-first for the drama target.)
    }

    return diverse;
}

}
