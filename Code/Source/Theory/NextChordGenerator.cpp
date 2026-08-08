#include "Theory/NextChordGenerator.h"

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

    // Bass pitch class + full pitch-class set: inversions of the same harmony are distinct.
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
        // Keep inversions as separate candidates; only collapse exact duplicates (same bass + pcs).
        if (!seenVoicings.insert(voicingKey(chord)).second)
            continue;

        // Skip the current voicing itself, but allow other inversions of the same harmony
        // (e.g. C → C/E is a valid smooth next-chord move).
        if (sameVoicing(chord, currentChord))
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
