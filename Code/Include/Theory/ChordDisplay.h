#pragma once

#include <string>

#include "Theory/Chord.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace theory
{

// "Am · ii" when a roman exists in key/scale; otherwise just the absolute name (readableName).
// Used by the progression chord lane when a scale is attached to a block.
[[nodiscard]] std::string formatAbsoluteWithRoman(const Chord& chord, Key key, Scale scale,
                                                  const std::string& absoluteNameFallback = {});

// Roman only (may be empty if the root is not diatonic / not analysable in that key/scale).
[[nodiscard]] std::string romanForChordInKeyScale(const Chord& chord, Key key, Scale scale);

}
