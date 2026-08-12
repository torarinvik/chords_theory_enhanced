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
    std::string name; // e.g. "Am7", "C/E", "G5", or a single note "F#"
    int rootPitchClass = 0;
    int bassPitchClass = 0;
    TriadQuality quality = TriadQuality::Major;
    int toneCount = 0; // size of the matched quality (incl. root)
};

// Stateless detector: maps live MIDI pitch classes → a readable chord name using the same
// quality catalogue as TriadLibrary (triads, sus, power, common sevenths + inversions).
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
