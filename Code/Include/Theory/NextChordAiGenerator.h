#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordSequenceContext.h"

namespace theory
{

// Offline ChordSeqAI-backed next-chord suggestions.
// Pure model ranking by probability — no symbolic scorer blend.
// Fit / Tension meters are left unset (0); reasonLabel carries AI confidence.
class NextChordAiGenerator
{
public:
    static constexpr int kDefaultTopK = 24;

    // Returns empty when the model failed to load or the current chord cannot be tokenised.
    static std::vector<NextChordCandidate> generate(const Chord& currentChord,
                                                    const KeyScaleData& keyScale,
                                                    const SequenceContext& sequence = {},
                                                    int topK = kDefaultTopK);

    [[nodiscard]] static bool isAvailable();
    [[nodiscard]] static std::string unavailableReason();
};

}
