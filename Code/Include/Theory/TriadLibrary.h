#pragma once

#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Key.h"

namespace theory
{

// Qualities in the algorithmic next-chord catalogue (root position + inversions).
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

// Catalogue of common sonorities on every chromatic root (triads, sus, power, sevenths),
// each emitted in every inversion (bass-first note order, slash-chord readableName, e.g. "C/E").
class TriadLibrary
{
public:
    static constexpr int kNumRoots = 12;
    static constexpr int kNumQualities = 11;
    // Root positions only (legacy count used by older tests / docs).
    static constexpr int kNumRootPositionChords = kNumRoots * kNumQualities; // 132
    // Power(2) + 6×3-note qualities(3) + 4×7ths(4)  →  2+18+16 = 36 positions per root × 12.
    static constexpr int kNumNamedChords = kNumRoots * (2 + 6 * 3 + 4 * 4); // 432

    static std::vector<Chord> allTriads(Key rootSpellKey = Key::C);

    // inversion: 0 = root position, 1 = first inversion (3rd/next chord-tone in bass), …
    // Clamped to [0, toneCount-1]. Notes are always bass-first (matches chords.json inversions).
    // preferFlats: when true, spell roots like Bb/Eb even in C/G/D keys (modal mixture).
    static Chord makeTriad(int rootPitchClass, TriadQuality quality, Key rootSpellKey = Key::C,
                           int inversion = 0, bool preferFlats = false);

    // Number of distinct bass placements for this quality (tone count).
    static int inversionCount(TriadQuality quality);

    static std::vector<int> qualityIntervals(TriadQuality quality);
    static std::string qualitySuffix(TriadQuality quality);
    static ChordType chordTypeForQuality(TriadQuality quality);
};

}
