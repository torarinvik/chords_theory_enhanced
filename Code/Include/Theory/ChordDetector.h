#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "Theory/Key.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

// Result of identifying a held set of pitch classes as a named chord.
struct ChordDetection
{
    bool matched = false;
    std::string name; // e.g. "Am9", "C/E", "G7b9", single note "F#"
    int rootPitchClass = 0;
    int bassPitchClass = 0;
    // Set when the match is one of TriadLibrary's core qualities; otherwise Major as placeholder.
    TriadQuality quality = TriadQuality::Major;
    bool hasLibraryQuality = false;
    std::string qualityLabel; // e.g. "m9", "maj7", "7sus4", "dim7"
    int toneCount = 0; // required tones matched (excl. optional omissions)
};

// Stateless detector: maps live MIDI pitch classes → a readable chord name.
// Catalogue covers triads through 13ths, alters (#9/b9/#11/b13/alt), sixths, add tones,
// sus variants, dim7, and rootless jazz shells. Forbidden-interval rules keep maj/min/sus distinct.
class ChordDetector
{
public:
    // pcs[i] true when pitch class i is held. bassPitchClass is the lowest sounding note's PC
    // (drives slash-chord naming). spellKey chooses sharp/flat spellings.
    [[nodiscard]] static ChordDetection detect(const std::array<bool, 12>& pcs,
                                               int bassPitchClass,
                                               Key spellKey = Key::C);

    // Convenience: MIDI note numbers (any octave). Bass = lowest MIDI value.
    [[nodiscard]] static ChordDetection detectFromMidiNotes(const std::vector<int>& midiNotes,
                                                            Key spellKey = Key::C);
};

}
