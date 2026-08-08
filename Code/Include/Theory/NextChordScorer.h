#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"

namespace theory
{

// Stateless scoring for next-chord ranking. All inputs are pitch-class sets derived from Chord::notes.
// Weights are fixed for the triad MVP; a Drama slider can rebalance them later.
class NextChordScorer
{
public:
    // Fills candidate.tensionPercent (0–100) and reasonLabel from currentChord + key/scale context.
    static void score(const Chord& currentChord, const KeyScaleData& keyScale, NextChordCandidate& candidate);

    // Convenience: score every entry and sort ascending by tensionPercent (stable by symbol).
    static void scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                             std::vector<NextChordCandidate>& candidates);

    // Exposed for unit tests.
    static int commonToneCount(const Chord& a, const Chord& b);
    static int rootPitchClass(const Chord& chord);
    static int pitchClassDistance(int a, int b); // min distance on the circle, 0–6
    static float voiceLeadingCost(const Chord& from, const Chord& to);
    static bool isDiatonicTriad(const Chord& chord, const KeyScaleData& keyScale);
};

}
