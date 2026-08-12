#pragma once

#include <string>

#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace theory
{

// Why this scale was suggested (UI localises these; generator stays language-agnostic).
enum class ScaleSuggestionReason
{
    Parallel,
    Relative,
    ContainsChord,
    Related,
};

// One ranked scale option for the scale-suggestions UI (parallel modes, relative keys, etc.).
struct ScaleSuggestion
{
    Key key = Key::C;
    Scale scale = Scale::Major;
    std::string label;       // e.g. "C Dorian"
    ScaleSuggestionReason reason = ScaleSuggestionReason::Related;
    std::string reasonChordName; // when reason == ContainsChord
    int fitPercent = 0;      // 0-100, how well the current chord (if any) sits in this scale
    float score = 0.f;       // internal ranking key (higher = better)
};

}
