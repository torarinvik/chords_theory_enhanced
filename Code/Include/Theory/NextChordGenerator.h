#pragma once

#include <string>
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
//
// Search scope (mirrors NextScaleGenerator::Pool):
//   - Predicted: curated next-chord list, then filter by query
//   - All: full scored catalogue (no family collapse), then filter by query
class NextChordGenerator
{
public:
    enum class Pool
    {
        Predicted,
        All,
    };

    static constexpr int kDefaultMaxResults = 64;

    static std::vector<NextChordCandidate> generate(
        const Chord& currentChord,
        const KeyScaleData& keyScale,
        float drama01 = NextChordScorer::kDefaultDrama,
        const SequenceContext& sequence = {},
        Pool pool = Pool::Predicted,
        const std::string& query = {},
        int maxResults = kDefaultMaxResults);

    // Full catalogue browse when there is no "current" chord (All scope only).
    // Root-position faces, name-sorted after query filter.
    static std::vector<NextChordCandidate> generateCatalogue(
        const KeyScaleData& keyScale,
        const std::string& query = {},
        int maxResults = kDefaultMaxResults);
};

}
