#pragma once

#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Key.h"

namespace theory
{

// Qualities in the algorithmic next-chord catalogue (root-position).
enum class TriadQuality
{
    Major,
    Minor,
    Diminished,
    Augmented,
    Sus2,
    Sus4,
    Power,       // root + fifth
    Major7,      // 1 3 5 7
    Minor7,      // 1 b3 5 b7
    Dominant7,   // 1 3 5 b7
    HalfDim7     // 1 b3 b5 b7  (m7b5)
};

// Catalogue of common sonorities on every chromatic root (triads, sus, power, sevenths).
class TriadLibrary
{
public:
    static constexpr int kNumRoots = 12;
    static constexpr int kNumQualities = 11;
    static constexpr int kNumNamedChords = kNumRoots * kNumQualities; // 132

    static std::vector<Chord> allTriads(Key rootSpellKey = Key::C);

    static Chord makeTriad(int rootPitchClass, TriadQuality quality, Key rootSpellKey = Key::C);

    static std::vector<int> qualityIntervals(TriadQuality quality);
    static std::string qualitySuffix(TriadQuality quality);
    static ChordType chordTypeForQuality(TriadQuality quality);
};

}
