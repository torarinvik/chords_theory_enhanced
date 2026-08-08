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
// Layers (combined into tensionPercent):
//  - surface: common tones, voice leading (bass + upper voices), bass motion, directed root /
//    circle-of-fifths motion, chromaticism — closer voice-leading / bass steps = less tension;
//    pure inversions of the same harmony are especially soft
//  - quality: target sonority colour + same-root colour changes + blues I7/IV7
//  - function: scale-family degree bias + quality-vs-degree fitness + diatonic 7th colour
//  - grammar: classic progressions (ii–V, V–I, plagal, deceptive, …) + falling-fifths glue
//  - prepare/resolve: secondary V/x (weighted), tritone sub, dominant resolution / abandon
//  - colour idioms: mode mixture, backdoor, sus resolve, chromatic mediants, approach chords
//  - tendency tones: leading-tone / 4→3 / guide-tone resolutions
//  - sequence context: phrase memory from previous progression slots (ii–V–I, fifths chains,
//    repeat avoidance, modal tetrachords)
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
