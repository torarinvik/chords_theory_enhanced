#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordScorer.h"
#include "Theory/NextChordSequenceContext.h"

namespace theory
{

// Builds next-chord candidates in two stages:
//   1) Rank harmonic *destinations* (families: root + family kind)
//   2) Choose one simple representative voicing per family (simplicity prior)
//
// Inversions, sevenths-as-colour of a triad family, and power/incomplete forms do not
// flood the main list — they compete only within their family for the representative slot.
class NextChordGenerator
{
public:
    static std::vector<NextChordCandidate> generate(const Chord& currentChord, const KeyScaleData& keyScale,
                                                    float drama01 = NextChordScorer::kDefaultDrama,
                                                    const SequenceContext& sequence = {});
};

}
