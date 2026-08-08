#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordScorer.h"
#include "Theory/NextChordSequenceContext.h"

namespace theory
{

// Builds next-chord candidates from the algorithmic catalogue (triads, sus, power, sevenths),
// deduped by pitch-class set, ranked by NextChordScorer (optionally with sequence memory).
class NextChordGenerator
{
public:
    static std::vector<NextChordCandidate> generate(const Chord& currentChord, const KeyScaleData& keyScale,
                                                    float drama01 = NextChordScorer::kDefaultDrama,
                                                    const SequenceContext& sequence = {});
};

}
