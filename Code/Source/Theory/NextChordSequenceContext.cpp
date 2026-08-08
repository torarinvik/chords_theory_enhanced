#include "Theory/NextChordSequenceContext.h"

#include <algorithm>
#include <set>
#include <string>

#include "Theory/NoteConvertor.h"
#include "Theory/NextChordScorer.h"

namespace theory
{

namespace
{
    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }

    constexpr const char* kSharpNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };

    std::set<int> pitchClassSet(const Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }

    Chord reconstructChord(const MidiEditorChordBlockState& block, const MidiEditorState& state)
    {
        Chord chord;
        chord.symbol = block.label;
        chord.readableName = block.label.empty() ? block.label : block.label;
        chord.type = ChordType::Triad;
        chord.popularityOrder = block.sourceSlot.popularityOrder;

        std::set<int> pcs;
        for (const auto& note : state.notes)
        {
            if (note.sourceChordId != block.id)
                continue;
            pcs.insert(mod12(note.midiNote));
        }

        // Prefer bass-first order: lowest MIDI note among the block's notes first.
        std::vector<int> midiInBlock;
        for (const auto& note : state.notes)
        {
            if (note.sourceChordId == block.id)
                midiInBlock.push_back(note.midiNote);
        }
        std::sort(midiInBlock.begin(), midiInBlock.end());

        std::set<int> seen;
        int role = 1;
        for (const int midi : midiInBlock)
        {
            const int pc = mod12(midi);
            if (!seen.insert(pc).second)
                continue;
            const char* name = kSharpNames[pc];
            chord.notes.push_back(NoteName { name, name, role });
            // Rough role: first = root/bass, then 3/5/7.
            if (role == 1)
                role = 3;
            else if (role == 3)
                role = 5;
            else if (role == 5)
                role = 7;
            else
                ++role;
        }

        // Fallback if notes were deleted but the block remains.
        if (chord.notes.empty() && !pcs.empty())
        {
            int r = 1;
            for (const int pc : pcs)
            {
                const char* name = kSharpNames[pc];
                chord.notes.push_back(NoteName { name, name, r });
                r += 2;
            }
        }

        return chord;
    }
}

void SequenceContext::trim()
{
    if (static_cast<int>(previous.size()) <= kMaxLookback)
        return;
    previous.erase(previous.begin(),
                   previous.begin() + static_cast<std::ptrdiff_t>(previous.size() - kMaxLookback));
}

SequenceContext buildSequenceContext(const MidiEditorState& state,
                                     const KeyScaleData& keyScale,
                                     const Chord* currentChord)
{
    struct Indexed
    {
        double startBeat = 0.0;
        MidiEditorChordBlockState block;
    };

    std::vector<Indexed> ordered;
    ordered.reserve(state.chordBlocks.size());
    for (const auto& block : state.chordBlocks)
        ordered.push_back({ block.startBeat, block });

    std::sort(ordered.begin(), ordered.end(),
              [](const Indexed& a, const Indexed& b) { return a.startBeat < b.startBeat; });

    SequenceContext ctx;
    ctx.previous.reserve(ordered.size());

    for (const auto& entry : ordered)
    {
        SequenceEvent event;
        event.chord = reconstructChord(entry.block, state);
        if (event.chord.notes.empty())
            continue;

        // Prefer degree from frozen slot when the chord is still diatonic; otherwise infer.
        if (NextChordScorer::isDiatonicChord(event.chord, keyScale))
        {
            event.degree = entry.block.sourceSlot.degree;
            // If slot degree root doesn't match reconstructed root, re-infer.
            if (event.degree)
            {
                const int root = NextChordScorer::rootPitchClass(event.chord);
                const auto inferred = NextChordScorer::degreeOfRoot(root, keyScale);
                if (inferred)
                    event.degree = inferred;
            }
        }
        else
        {
            event.degree = NextChordScorer::degreeOfRoot(
                NextChordScorer::rootPitchClass(event.chord), keyScale);
        }

        ctx.previous.push_back(std::move(event));
    }

    if (currentChord != nullptr && !ctx.previous.empty()
        && pitchClassSet(ctx.previous.back().chord) == pitchClassSet(*currentChord))
    {
        ctx.previous.pop_back();
    }

    ctx.trim();
    return ctx;
}

}
