#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace theory
{

// Independent quality axes for a next-chord suggestion. Not collapsed until final ranking.
// Keep these separate: transition smoothness must never leak into intrinsic/functional tension.
struct CandidateMetrics
{
    // 0–1: contextual harmonic coherence (Fit). Absolute scale — best is not forced to 1.0.
    float coherence = 0.0f;
    // 0–1: how unresolved the music feels after this chord (Tension). Independent of voice-leading.
    float tension = 0.0f;
    // 0–1: statistical / stylistic unexpectedness (higher = more surprising).
    float surprise = 0.0f;
    // 0–1: voice-leading / bass smoothness of the transition (higher = smoother). Not tension.
    float voiceLeading = 0.0f;
    // 0–1: how strongly this chord points toward a useful resolution / next step.
    float resolution = 0.0f;
    // 0–1: unnecessary specificity (inversions, incomplete/power, extra colour tones).
    float complexity = 0.0f;

    enum class TensionDirection { Release, Maintain, Increase };
    TensionDirection tensionDirection = TensionDirection::Maintain;
};

// One ranked suggestion for "what can follow the current chord?"
// Main list shows one representative per harmonic family (root + family kind).
struct NextChordCandidate
{
    Chord chord;
    // Present when the candidate matches a known chord for a degree in the current key/scale.
    std::optional<Degree> degree;

    CandidateMetrics metrics;

    // UI-facing 0–100 mirrors of the independent metrics (rounded). Absolute, not rebased per list.
    int fitPercent = 0;           // from metrics.coherence
    int tensionPercent = 0;       // from metrics.tension
    int surprisePercent = 0;      // from metrics.surprise
    int smoothnessPercent = 0;    // from metrics.voiceLeading
    int resolutionPercent = 0;    // from metrics.resolution

    // Final combined sort key (higher = better match for the current drama target tension).
    float rankingScore = 0.0f;

    // Short theory tag, e.g. "V/ii" — only formal claims from predicates with a known target.
    std::string reasonLabel;

    // Optional human bullets (filled when analysis provides them).
    std::vector<std::string> whyBullets;

    // Coarse harmonic destination used for family diversity (root + family kind).
    // Set by the generator after scoring; -1 if unset.
    int familyRootPc = -1;
    int familyKind = -1; // NextChordScorer::HarmonicFamilyKind as int
};

}
