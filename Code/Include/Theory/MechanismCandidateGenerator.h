#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordSequenceContext.h"

namespace theory
{

// Emits next-chord *ideas* from named harmonic mechanisms (not a flat chromatic catalogue).
// Catalogue-wide fill-in still happens in NextChordGenerator for coverage; this prioritises
// functionally motivated moves (diatonic, V/x, subV, mixture, mediants, approaches).
class MechanismCandidateGenerator
{
public:
    // Root-position representatives; inversions are left to diversity/voicing later.
    static std::vector<NextChordCandidate> generate(const Chord& currentChord,
                                                    const KeyScaleData& keyScale,
                                                    const SequenceContext& sequence = {});
};

// Infer a temporary local tonic from the recent sequence (e.g. A7 ⇒ Dm).
// Returns pitch class of likely local tonic, or nullopt if none is confident.
[[nodiscard]] std::optional<int> inferLocalTonicPc(const SequenceContext& sequence,
                                                   const KeyScaleData& keyScale);

// One-step lookahead: best ranking score of resolving *from* candidate as if it were current
// (productive tension). Uses a small fixed resolution pool (I, diatonic targets, etc.).
[[nodiscard]] float lookaheadProductivity(const Chord& fromCandidate,
                                          const KeyScaleData& keyScale,
                                          float drama01);

}
