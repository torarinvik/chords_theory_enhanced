#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordScorer.h"
#include "Theory/NextChordSequenceContext.h"

namespace theory
{

// Offline ChordSeqAI-backed next-chord suggestions.
// Model probability drives expectedness (inverse surprise) and ranking; theory scorer
// fills Fit / Tension / labels so the UI stays consistent with the rule-based list.
class NextChordAiGenerator
{
public:
    static constexpr int kDefaultTopK = 24;

    // Returns empty when the model failed to load or the current chord cannot be tokenised.
    static std::vector<NextChordCandidate> generate(const Chord& currentChord,
                                                    const KeyScaleData& keyScale,
                                                    float drama01 = NextChordScorer::kDefaultDrama,
                                                    const SequenceContext& sequence = {},
                                                    int topK = kDefaultTopK);

    [[nodiscard]] static bool isAvailable();
    [[nodiscard]] static std::string unavailableReason();
};

}
