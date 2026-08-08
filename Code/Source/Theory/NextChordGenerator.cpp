#include "Theory/NextChordGenerator.h"

#include <set>

#include "Theory/NextChordScorer.h"
#include "Theory/NoteConvertor.h"
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

    // If this triad matches a degree's default triad in the current key/scale, attach that degree.
    std::optional<Degree> matchingDegree(const Chord& triad, const KeyScaleData& keyScale)
    {
        const auto pcs = pitchClassSet(triad);
        for (const auto& degreeData : keyScale.degrees)
        {
            for (const auto& chord : degreeData.chords)
            {
                if (chord.type != ChordType::Triad)
                    continue;
                if (pitchClassSet(chord) == pcs)
                    return degreeData.degree;
            }
        }
        return std::nullopt;
    }
}

std::vector<NextChordCandidate> NextChordGenerator::generate(const Chord& currentChord, const KeyScaleData& keyScale)
{
    if (currentChord.notes.empty())
        return {};

    // Full chromatic triad catalogue (maj/min/dim/aug × 12 roots), spelled for this key.
    // Augmented qualities repeat every major third (Caug == Eaug == G#aug as pitch-class sets),
    // so we also dedupe by pitch-class set to keep one row per distinct harmony.
    const auto allTriads = TriadLibrary::allTriads(keyScale.key);

    std::vector<NextChordCandidate> candidates;
    candidates.reserve(allTriads.size());
    std::set<std::set<int>> seenPitchClassSets;

    for (const auto& triad : allTriads)
    {
        const auto pcs = pitchClassSet(triad);
        if (!seenPitchClassSets.insert(pcs).second)
            continue;

        if (sameHarmony(triad, currentChord))
            continue;

        NextChordCandidate candidate;
        candidate.chord = triad;
        candidate.degree = matchingDegree(triad, keyScale);
        candidates.push_back(std::move(candidate));
    }

    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates);
    return candidates;
}

}
