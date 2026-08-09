#pragma once

#include <optional>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Degree.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/NextChordSequenceContext.h"
#include "Theory/Scale.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

// Multi-metric next-chord scoring.
//
// Independent axes (CandidateMetrics) — never merge early:
//   coherence (Fit), tension, surprise, voiceLeading (smoothness), resolution, complexity
//
// drama01 is the *desired tension* T* ∈ [0,1] (Smooth→Wild), not a "chromatic weight".
// Ranking prefers high-coherence candidates near T*, with resolution potential and a
// simplicity prior (root-position ordinary chords beat needless inversions/extensions).
class NextChordScorer
{
public:
    static constexpr float kDefaultDrama = 0.35f;
    // Minimum coherence (0–1) to appear in results; wild allows a bit more colour.
    static constexpr float kMinCoherenceSmooth = 0.28f;
    static constexpr float kMinCoherenceWild = 0.18f;

    enum class ScaleFamily { Majorish, Minorish, ModalSoft, Diminishedish };

    // Coarse harmonic role used by the progression grammar.
    enum class HarmonicRole
    {
        Tonic,
        Predominant,
        Dominant,
        Modal,
        Chromatic
    };

    // Harmonic *destination* family — coarser than TriadQuality.
    // F, Fmaj7, F5, F/C share MajorColour@F; G vs G7 are different families.
    enum class HarmonicFamilyKind
    {
        MajorColour = 0, // Major, Major7, Power
        MinorColour = 1, // Minor, Minor7
        Dominant = 2,    // Dominant7
        Diminished = 3,  // Dim, HalfDim7
        Augmented = 4,
        Sus = 5          // Sus2, Sus4
    };

    static void score(const Chord& currentChord, const KeyScaleData& keyScale, NextChordCandidate& candidate,
                      float drama01 = kDefaultDrama,
                      const SequenceContext& sequence = {});

    static void scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                             std::vector<NextChordCandidate>& candidates,
                             float drama01 = kDefaultDrama,
                             const SequenceContext& sequence = {});

    static int commonToneCount(const Chord& a, const Chord& b);
    // Harmonic root: note with positionInChord == 1 when present (correct for slash-chord
    // inversions where notes are bass-first); otherwise notes.front().
    static int rootPitchClass(const Chord& chord);
    // Lowest sounding tone = first entry in the chord's bass-first note array.
    static int bassPitchClass(const Chord& chord);
    static int pitchClassDistance(int a, int b);
    static int directedRootInterval(int fromRoot, int toRoot); // 0–11 steps up
    static int circleOfFifthsDistance(int rootA, int rootB);
    // Closed-voicing distance between from→to (0 = smoothest, 1 = max motion).
    static float voiceLeadingCost(const Chord& from, const Chord& to);
    static bool isDiatonicChord(const Chord& chord, const KeyScaleData& keyScale);
    static int nonScaleToneCount(const Chord& chord, const KeyScaleData& keyScale);
    static TriadQuality detectTriadQuality(const Chord& chord);
    static ScaleFamily scaleFamily(Scale scale);

    // Harmonic-family helpers (destination-first ranking).
    static HarmonicFamilyKind familyKindForQuality(TriadQuality quality);
    // Idea-level family: maps power chords to contextual triad families; sus stays Sus.
    // Returns {rootPc, kind}. For same-root prolongations, kind may still be set — use isProlongation.
    static void assignIdeaFamily(const Chord& chord, const KeyScaleData& keyScale,
                                 int currentRootPc, int& outRootPc, int& outKind);
    // Incomplete / ambiguous sonorities (power, bare sus) — not independent top-level ideas.
    static bool isIncompleteSonority(TriadQuality quality);
    // Same harmonic root as current: recolour/prolong, not a new destination.
    static bool isProlongationOf(const Chord& candidate, const Chord& current);
    // Prefer full triad/dom7 over power/sus as family representative.
    static float ideaRepresentativeBonus(const Chord& chord);
    // Contextual enharmonic spelling for display (Bb not A# as bVII in C major).
    static Chord spellInKeyContext(const Chord& chord, const KeyScaleData& keyScale);
    static float voicingComplexity(const Chord& chord);
    // Map drama slider → preferred tension centre (for diagnostics); ranking uses soft curves.
    static float targetTensionFromDrama(float drama01);
    // Soft preference curves (0–1, may dip slightly negative when strongly mismatched).
    static float tensionPreference(float tension01, float drama01);
    static float surprisePreference(float surprise01, float drama01);
    // Standing tension of a chord in key (independent of transition) for trajectory.
    static float standingTension(const Chord& chord, const KeyScaleData& keyScale);
    // Quality-aware roman numeral for a degree (ii vs II).
    static std::string romanForChord(const Chord& chord, const KeyScaleData& keyScale,
                                     std::optional<Degree> degree = std::nullopt);
    // Recompute final ranking from filled metrics (+ optional AI/path already in metrics).
    static void finalizeRanking(NextChordCandidate& candidate, float drama01,
                                float currentStandingTension);

    // Key / scale helpers (also unit-tested).
    static int keyTonicPitchClass(const KeyScaleData& keyScale);
    static std::optional<Degree> degreeOfRoot(int rootPitchClass, const KeyScaleData& keyScale);
    static HarmonicRole roleFor(Degree degree, TriadQuality quality, ScaleFamily family);
    static bool isDominantLike(TriadQuality quality);
    static bool isMajorishQuality(TriadQuality quality);
    static bool isMinorishQuality(TriadQuality quality);
};

}
