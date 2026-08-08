#pragma once

#include <optional>
#include <string>

#include "Theory/Chord.h"
#include "Theory/Degree.h"
#include "Theory/KeyScaleData.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

// Formal harmonic predicates for next-chord analysis / labels.
// A label string may only be shown when the corresponding predicate returns true
// with confidence >= kMinLabelConfidence.

inline constexpr float kMinLabelConfidence = 0.55f;

struct SecondaryDominantResult
{
    bool hit = false;
    float confidence = 0.0f;
    Degree targetDegree = Degree::I;
    int targetRootPc = 0;
    std::string label; // e.g. "V/ii"
};

struct TritoneSubResult
{
    bool hit = false;
    float confidence = 0.0f;
    int resolveRootPc = 0; // where it wants to resolve (as if V → I of that root)
    std::optional<Degree> resolveDegree;
    std::string label; // e.g. "subV/I"
};

struct MixtureResult
{
    bool hit = false;
    float confidence = 0.0f;
    std::string label; // e.g. "iv (mixture)"
};

// Dominant-family sonorities eligible for V/x and subV labels (not bare major triads).
[[nodiscard]] bool isDominantFamilyQuality(TriadQuality quality);

// Secondary dominant: dominant-family chord whose root is a perfect fifth above a diatonic
// degree root (resolves up a fourth to that degree). Primary V/I is excluded (that is just V).
[[nodiscard]] SecondaryDominantResult analyseSecondaryDominant(const Chord& chord,
                                                               const KeyScaleData& keyScale);

// Tritone substitution: dominant-family only; root is a tritone from V of a plausible target
// (global I or a diatonic degree). Bare major triads and inversions of non-dominants never pass.
[[nodiscard]] TritoneSubResult analyseTritoneSubstitution(const Chord& chord,
                                                          const KeyScaleData& keyScale);

// Modal mixture relative to majorish global keys (iv, bVI, bVII, bIII, Neapolitan, parallel i).
[[nodiscard]] MixtureResult analyseModeMixture(const Chord& chord, const KeyScaleData& keyScale);

// Whether a reason string is allowed to contain a high-stakes theoretical claim.
[[nodiscard]] bool reasonContainsInvalidTheoryClaim(const std::string& reasonLabel,
                                                    const Chord& chord,
                                                    const KeyScaleData& keyScale);

}
