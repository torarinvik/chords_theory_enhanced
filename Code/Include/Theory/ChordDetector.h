#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "Theory/Key.h"
#include "Theory/Scale.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

// Result of identifying a held set of pitch classes as a named chord.
struct ChordDetection
{
    bool matched = false;
    std::string name; // e.g. "Am9", "C/E", "G7b9", single note "F#"
    std::string alternateName; // second-best reading when useful (e.g. "Am7" vs "C6")
    std::string romanNumeral; // function in key: "ii", "V", "V/ii", "subV/I", "iv (mixture)"
    int rootPitchClass = 0;
    int bassPitchClass = 0;
    // Set when the match is one of TriadLibrary's core qualities; otherwise Major as placeholder.
    TriadQuality quality = TriadQuality::Major;
    bool hasLibraryQuality = false;
    bool fromChordDatabase = false; // true when name came from an exact chords.json match
    std::string qualityLabel; // e.g. "m9", "maj7", "7sus4", "dim7"
    int toneCount = 0; // required tones matched (excl. optional omissions)
    float confidence = 0.f; // 0–1, higher = clearer match vs runners-up
};

// Stateless detector: maps live MIDI pitch classes → a readable chord name.
// Combines a large quality catalogue with optional exact matches against chords.json for the
// current key/scale. Forbidden-interval rules keep maj/min/sus distinct. With scale set: roman
// numerals, secondary dominants, mixture labels, and diatonic scoring bias.
class ChordDetector
{
public:
    // pcs[i] true when pitch class i is held. bassPitchClass is the lowest sounding note's PC
    // (drives slash-chord naming). spellKey chooses sharp/flat spellings.
    // When scale is set, fills romanNumeral for diatonic roots in that key/scale.
    [[nodiscard]] static ChordDetection detect(const std::array<bool, 12>& pcs,
                                               int bassPitchClass,
                                               Key spellKey = Key::C,
                                               std::optional<Scale> scale = std::nullopt);

    // Convenience: MIDI note numbers (any octave). Bass = lowest MIDI value.
    [[nodiscard]] static ChordDetection detectFromMidiNotes(const std::vector<int>& midiNotes,
                                                            Key spellKey = Key::C,
                                                            std::optional<Scale> scale = std::nullopt);
};

}
