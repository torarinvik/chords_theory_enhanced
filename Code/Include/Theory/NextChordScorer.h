#pragma once

#include <vector>

#include "Theory/Chord.h"
#include "Theory/KeyScaleData.h"
#include "Theory/NextChordCandidate.h"
#include "Theory/Scale.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

// Stateless next-chord tension scoring.
// drama01: target tension band for ranking (0 = softest first, 1 = wildest first).
// Displayed tensionPercent is an objective "how colourful is this move" value; drama
// mainly reorders the list toward that target.
class NextChordScorer
{
public:
    static constexpr float kDefaultDrama = 0.35f;

    static void score(const Chord& currentChord, const KeyScaleData& keyScale, NextChordCandidate& candidate,
                      float drama01 = kDefaultDrama);

    static void scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                             std::vector<NextChordCandidate>& candidates,
                             float drama01 = kDefaultDrama);

    static int commonToneCount(const Chord& a, const Chord& b);
    static int rootPitchClass(const Chord& chord);
    static int pitchClassDistance(int a, int b);
    static int circleOfFifthsDistance(int rootA, int rootB);
    static float voiceLeadingCost(const Chord& from, const Chord& to);
    static bool isDiatonicChord(const Chord& chord, const KeyScaleData& keyScale);
    static int nonScaleToneCount(const Chord& chord, const KeyScaleData& keyScale);
    static TriadQuality detectTriadQuality(const Chord& chord);

    // Major-ish vs minor-ish function tables depend on Scale.
    enum class ScaleFamily { Majorish, Minorish, ModalSoft, Diminishedish };
    static ScaleFamily scaleFamily(Scale scale);
};

}
