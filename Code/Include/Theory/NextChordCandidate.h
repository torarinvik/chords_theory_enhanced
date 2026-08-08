#pragma once

#include <optional>
#include <string>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace theory
{

// One ranked suggestion for "what can follow the current chord?"
struct NextChordCandidate
{
    Chord chord;
    // Present when the candidate matches a known chord for a degree in the current key/scale.
    std::optional<Degree> degree;
    // 0 = maximally safe/smooth, 100 = maximally tense/remote (UI-facing integer).
    int tensionPercent = 0;
    // Short theory tag for the list, e.g. "V · cadence · ↑4th", "secondary V/II".
    std::string reasonLabel;
};

}
