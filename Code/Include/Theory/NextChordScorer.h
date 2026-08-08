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

// Stateless next-chord tension scoring.
// drama01: target tension band for ranking (0 = softest first, 1 = wildest first).
// Displayed tensionPercent is an objective "how colourful is this move" value; drama
// mainly reorders the list toward that target.
//
// Layers:
//  - surface (absolute colour): common tones, voice leading, bass motion, root / fifths motion,
//    chromaticism, quality — closer VL/bass = less tension
//  - theory (reordering bias, capped on display): function, grammar, role, secondary/tritone,
//    mixture, tendency, sequence memory, …
// Surface sets most of the displayed tensionPercent; theory may pull it down only a little so
// soft diatonic moves stay differentiated (no pile-up at 0). rankingScore keeps the full theory
// effect for drama-based sorting.
class NextChordScorer
{
public:
    static constexpr float kDefaultDrama = 0.35f;

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
    // Closed-voicing MIDI distance between from→to (0 = identical/perfectly smooth, 1 = max).
    // Closer voice-leading (including inversion changes that keep common tones) scores lower.
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
