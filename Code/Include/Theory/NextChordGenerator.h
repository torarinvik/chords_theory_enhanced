#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"

namespace theory
{

// Builds the next-chord candidate pool from every common triad (12 roots × major/minor/dim/aug
// = 48), excluding the current harmony. Diatonic matches still carry an optional Degree label
// for UI/scoring context; non-diatonic triads are labelled via the scorer.
class NextChordGenerator
{
public:
    // Returns candidates sorted low → high tension. Empty currentChord.notes → empty result.
    static std::vector<NextChordCandidate> generate(const Chord& currentChord, const KeyScaleData& keyScale);
};

}
