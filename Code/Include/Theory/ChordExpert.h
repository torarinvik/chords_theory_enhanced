#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/ChordDetector.h"
#include "Theory/Key.h"
#include "Theory/Scale.h"

namespace theory
{

// Naming dialect for the live chord expert (user preference in Settings → Audio).
enum class ChordNamingStyle
{
    JazzChart = 0, // jazz/pop-chart symbols, C6 vs Am7 by bass, 7alt, functional labels
    PopSlash = 1,  // simpler symbols, favour slash bass spellings
    Classical = 2, // diatonic figures, roman-forward, fewer jazz alters
};

// Context the expert uses beyond the held notes (progression memory + key + style).
struct ChordExpertContext
{
    Key key = Key::C;
    Scale scale = Scale::Major;
    ChordNamingStyle style = ChordNamingStyle::JazzChart;
    // Most recent previous harmony first… wait: chronological, oldest first, newest last.
    // Expert uses back() as “just before now”.
    std::vector<Chord> previousChords;
};

// Full expert reading: primary detection + theory narrative + ranked alternatives.
struct ChordExpertResult
{
    ChordDetection detection;
    std::string explanation;              // e.g. "V → I · resolution"
    std::vector<std::string> alternatives; // up to 3 other names (excl. primary)
    bool usedProgressionContext = false;
    bool usedStyleRewrite = false;
};

// Jazz/pop/classical naming expert on top of ChordDetector.
// Re-ranks twins (C6/Am7), resolution of previous dominants, rootless shells, and style spelling.
class ChordExpert
{
public:
    [[nodiscard]] static ChordExpertResult analyse(const std::vector<int>& midiNotes,
                                                   const ChordExpertContext& context);

    [[nodiscard]] static ChordExpertResult analyse(const std::array<bool, 12>& pcs,
                                                   int bassPitchClass,
                                                   const ChordExpertContext& context);

    [[nodiscard]] static const char* styleLabel(ChordNamingStyle style);
    [[nodiscard]] static ChordNamingStyle parseStyle(const std::string& key);
    [[nodiscard]] static std::string styleKey(ChordNamingStyle style);
};

}
