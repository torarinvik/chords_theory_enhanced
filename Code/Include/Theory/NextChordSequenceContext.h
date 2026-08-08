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
    int blockId = -1;
    double startBeat = 0.0;
};

// Chords that already happened *before* the "current" chord being scored from.
// Empty means single-step ranking (no phrase memory).
struct SequenceContext
{
    static constexpr int kMaxLookback = 8;

    std::vector<SequenceEvent> previous;

    [[nodiscard]] bool empty() const { return previous.empty(); }
    [[nodiscard]] int size() const { return static_cast<int>(previous.size()); }

    // Trims to the most recent kMaxLookback events.
    void trim();
};

// Chronological reconstruction of every chord block on the piano-roll timeline.
struct ProgressionTimeline
{
    std::vector<SequenceEvent> events; // oldest → newest by startBeat

    [[nodiscard]] bool empty() const { return events.empty(); }
    [[nodiscard]] const SequenceEvent* last() const
    {
        return events.empty() ? nullptr : &events.back();
    }
};

[[nodiscard]] ProgressionTimeline buildProgressionTimeline(const MidiEditorState& state,
                                                           const KeyScaleData& keyScale);

// Builds phrase memory strictly *before* the current chord.
//
// Policy:
//  - If `currentChord` is null: previous = all timeline events except the last (last is assumed
//    current when the caller uses timeline.last() as current).
//  - If `currentChord` is non-null: find the latest timeline event whose pitch-class set matches
//    current; previous = events strictly before that index. If no match, previous = full timeline
//    (browser pin with full progression as history) — never treat later blocks as "before".
//
// This replaces the old bug where a mismatched current left *all* blocks (including later ones)
// in previous, producing "From C (2 in sequence)" with C→Em on the roll.
SequenceContext buildSequenceContext(const MidiEditorState& state,
                                     const KeyScaleData& keyScale,
                                     const Chord* currentChord = nullptr);

// Convenience: previous = everything before the last timeline chord.
SequenceContext buildSequenceContextBeforeLast(const MidiEditorState& state,
                                               const KeyScaleData& keyScale);

}
