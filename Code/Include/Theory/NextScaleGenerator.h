#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"
#include "Theory/ScaleSuggestion.h"

namespace theory
{

// Ranks alternative scales for the current key (and optional current chord).
// "Predicted" pool: parallel modes + relative major/minor + strong chord-fit scales.
// "All" pool: every scale in the catalogue for the current tonic key (plus relatives when they
// differ), still ranked with the same heuristics so useful options float up.
class NextScaleGenerator
{
public:
    enum class Pool
    {
        Predicted,
        All,
    };

    // excludeCurrent drops the already-selected key+scale so the list is "what else".
    // query filters by label / scale name (case-insensitive substring); empty = no filter.
    [[nodiscard]] static std::vector<ScaleSuggestion> generate(
        Key currentKey,
        Scale currentScale,
        const std::optional<Chord>& currentChord,
        Pool pool,
        const std::string& query = {},
        int maxResults = 16);
};

}
