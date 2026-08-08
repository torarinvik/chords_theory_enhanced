#pragma once

#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Key.h"

namespace theory
{

enum class TriadQuality
{
    Major,
    Minor,
    Diminished,
    Augmented
};

// Algorithmic catalogue of all 48 common triads (12 roots × major/minor/dim/aug).
// Spelling prefers accidentals that fit the given key when provided.
class TriadLibrary
{
public:
    static constexpr int kNumRoots = 12;
    static constexpr int kNumQualities = 4;
    static constexpr int kNumTriads = kNumRoots * kNumQualities; // 48

    // Builds every triad once. rootSpellKey only affects note/symbol spelling (enharmonics).
    static std::vector<Chord> allTriads(Key rootSpellKey = Key::C);

    static Chord makeTriad(int rootPitchClass, TriadQuality quality, Key rootSpellKey = Key::C);

    static int qualityThirdInterval(TriadQuality quality);  // semitones above root
    static int qualityFifthInterval(TriadQuality quality); // semitones above root
    static std::string qualitySuffix(TriadQuality quality); // "", "m", "dim", "aug"
};

}
