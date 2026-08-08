#pragma once

#include <optional>
#include <vector>

#include "Theory/Chord.h"
#include "Theory/Degree.h"
#include "Theory/KeyScaleData.h"
#include "Theory/MidiEditorState.h"

namespace theory
{

// One past chord in the progression, oldest → newest.
// `degree` is present when the slot/root maps onto the active key/scale.
struct SequenceEvent
{
    Chord chord;
    std::optional<Degree> degree;
};

// Chords that already happened *before* the "current" chord being scored from.
// Empty means single-step ranking (no phrase memory).
struct SequenceContext
{
    static constexpr int kMaxLookback = 6;

    std::vector<SequenceEvent> previous;

    [[nodiscard]] bool empty() const { return previous.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(previous.size()); }

    // Trims to the most recent kMaxLookback events.
    void trim();
};

// Rebuilds chronological sequence events from a MidiEditor snapshot (notes → pitch classes,
// labels → symbols, slots → optional degrees). If `currentChord` is provided and its pitch-class
// set matches the last block, that last block is treated as "current" and omitted from previous.
SequenceContext buildSequenceContext(const MidiEditorState& state,
                                     const KeyScaleData& keyScale,
                                     const Chord* currentChord = nullptr);

}
