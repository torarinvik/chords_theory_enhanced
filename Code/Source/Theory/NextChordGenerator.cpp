#include "Theory/NextChordGenerator.h"

#include <set>

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

    bool sameHarmony(const Chord& a, const Chord& b)
    {
        return pitchClassSet(a) == pitchClassSet(b);
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
    std::set<std::set<int>> seenPitchClassSets;

    for (const auto& chord : catalogue)
    {
        const auto pcs = pitchClassSet(chord);
        if (!seenPitchClassSets.insert(pcs).second)
            continue;

        if (sameHarmony(chord, currentChord))
            continue;

        NextChordCandidate candidate;
        candidate.chord = chord;
        candidate.degree = matchingDegree(chord, keyScale);
        candidates.push_back(std::move(candidate));
    }

    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates, drama01, sequence);
    return candidates;
}

}
