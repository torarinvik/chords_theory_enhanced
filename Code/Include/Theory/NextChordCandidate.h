#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Degree.h"

namespace theory
{

// Independent quality axes for a next-chord suggestion. Not collapsed until final ranking.
struct CandidateMetrics
{
    // 0–1: how convincingly this continues the progression (higher = better fit).
    float coherence = 0.0f;
    // 0–1: absolute unresolved harmonic tension (higher = more tense). Independent of drama.
    float tension = 0.0f;
    // 0–1: statistical / stylistic unexpectedness (higher = more surprising).
    float surprise = 0.0f;
    // 0–1: voice-leading smoothness (higher = smoother / less motion).
    float voiceLeading = 0.0f;
    // 0–1: how strongly the chord points toward a resolution target.
    float resolution = 0.0f;

    enum class TensionDirection { Release, Maintain, Increase };
    TensionDirection tensionDirection = TensionDirection::Maintain;
};

// One ranked suggestion for "what can follow the current chord?"
struct NextChordCandidate
{
    Chord chord;
    // Present when the candidate matches a known chord for a degree in the current key/scale.
    std::optional<Degree> degree;

    CandidateMetrics metrics;

    // UI-facing 0–100 mirrors of the independent metrics (rounded).
    int fitPercent = 0;      // from metrics.coherence
    int tensionPercent = 0;  // from metrics.tension
    int surprisePercent = 0; // from metrics.surprise

    // Final combined sort key (higher = better match for the current drama target).
    // ranking = coherence − |tension − T*| + VL/resolution bonuses − surprise when smooth.
    float rankingScore = 0.0f;

    // Short theory tag, e.g. "V/II · ↑4th" — only formal claims from predicates.
    std::string reasonLabel;

    // Optional human bullets (filled when analysis provides them).
    std::vector<std::string> whyBullets;
};

}
