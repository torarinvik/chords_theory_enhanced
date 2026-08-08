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
// Independent axes (CandidateMetrics):
//   coherence, tension, surprise, voiceLeading, resolution, tensionDirection
//
// drama01 is the *desired tension* T* ∈ [0,1]:
//   rankingScore = coherence − |tension − T*| + VL/resolution terms − surprise·(1−T*)
// Soft moves are not ranked only by "diatonic + common tones"; wild moves must still clear a
// minimum coherence gate.
class NextChordScorer
{
public:
    static constexpr float kDefaultDrama = 0.35f;
    // Minimum coherence (0–1) to appear in results; scales slightly with drama (wild allows a bit more colour).
    static constexpr float kMinCoherenceSmooth = 0.22f;
    static constexpr float kMinCoherenceWild = 0.14f;

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

    // Key / scale helpers (also unit-tested).
    static int keyTonicPitchClass(const KeyScaleData& keyScale);
    static std::optional<Degree> degreeOfRoot(int rootPitchClass, const KeyScaleData& keyScale);
    static HarmonicRole roleFor(Degree degree, TriadQuality quality, ScaleFamily family);
    static bool isDominantLike(TriadQuality quality);
    static bool isMajorishQuality(TriadQuality quality);
    static bool isMinorishQuality(TriadQuality quality);
};

}
