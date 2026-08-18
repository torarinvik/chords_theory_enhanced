#include "Theory/NextChordSequenceContext.h"

#include <algorithm>
#include <set>
#include <string>

#include "Theory/NextChordScorer.h"
#include "Theory/NoteConvertor.h"

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

    // Prefer frozen notes on the block when present; else reconstruct from MIDI notes.
    Chord reconstructChord(const MidiEditorChordBlockState& block, const MidiEditorState& state)
    {
        if (!block.frozenNotes.empty())
        {
            Chord chord;
            chord.symbol = block.frozenSymbol.empty() ? block.label : block.frozenSymbol;
            chord.readableName = block.label.empty() ? chord.symbol : block.label;
            chord.type = block.frozenType;
            chord.popularityOrder = block.sourceSlot.popularityOrder;
            chord.notes = block.frozenNotes;
            return chord;
        }

        Chord chord;
        chord.symbol = block.label;
        chord.readableName = block.label;
        chord.type = ChordType::Triad;
        chord.popularityOrder = block.sourceSlot.popularityOrder;

        std::vector<int> midiInBlock;
        for (const auto& note : state.notes)
        {
            if (note.sourceChordId == block.id)
                midiInBlock.push_back(note.midiNote);
        }
        std::sort(midiInBlock.begin(), midiInBlock.end());

        // Assign roles by chord-tone function when possible: prefer matching sourceSlot
        // popularity against database later; for now use ascending pitch classes with
        // positionInChord 1 on the *slot degree root* if diatonic, else lowest note.
        std::set<int> seen;
        int role = 1;
        for (const int midi : midiInBlock)
        {
            const int pc = mod12(midi);
            if (!seen.insert(pc).second)
                continue;
            const char* name = kSharpNames[pc];
            chord.notes.push_back(NoteName { name, name, role });
            if (role == 1)
                role = 3;
            else if (role == 3)
                role = 5;
            else if (role == 5)
                role = 7;
            else
                ++role;
        }

        // Fix root role: if we can infer degree root from slot and it appears in the chord,
        // mark that note as positionInChord 1 (harmonic root), keep bass as notes.front().
        if (chord.notes.size() >= 2)
        {
            // notes stay bass-first (MIDI order). Re-tag roles: bass keeps array order;
            // set positionInChord=1 on the true root pitch if present in the set.
            // Default: assume first reconstructed role-1 is wrong for inversions — use
            // interval detection via NextChordScorer after notes exist is hard; keep bass-first
            // and set positionInChord by matching symbol root later.
            for (auto& n : chord.notes)
                if (n.positionInChord == 1)
                    n.positionInChord = 5; // clear false root tag from lowest note
            // Prefer note matching readableName root letter if possible — leave as-is for now;
            // frozenNotes path is preferred for new drops.
            if (!chord.notes.empty())
                chord.notes.front().positionInChord = 1; // legacy fallback
        }

        return chord;
    }

    SequenceEvent makeEvent(const MidiEditorChordBlockState& block, const MidiEditorState& state,
                            const KeyScaleData& keyScale)
    {
        SequenceEvent event;
        event.chord = reconstructChord(block, state);
        event.blockId = block.id;
        event.startBeat = block.startBeat;

        if (event.chord.notes.empty())
            return event;

        if (NextChordScorer::isDiatonicChord(event.chord, keyScale))
        {
            event.degree = NextChordScorer::degreeOfRoot(
                NextChordScorer::rootPitchClass(event.chord), keyScale);
            if (!event.degree)
                event.degree = block.sourceSlot.degree;
        }
        else
        {
            event.degree = NextChordScorer::degreeOfRoot(
                NextChordScorer::rootPitchClass(event.chord), keyScale);
        }

        return event;
    }
}

void SequenceContext::trim()
{
    if (static_cast<int>(previous.size()) <= kMaxLookback)
        return;
    previous.erase(previous.begin(),
                   previous.begin() + static_cast<std::ptrdiff_t>(previous.size() - kMaxLookback));
}

ProgressionTimeline buildProgressionTimeline(const MidiEditorState& state,
                                             const KeyScaleData& keyScale)
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

    ProgressionTimeline timeline;
    timeline.events.reserve(ordered.size());
    for (const auto& entry : ordered)
    {
        auto event = makeEvent(entry.block, state, keyScale);
        if (event.chord.notes.empty())
            continue;
        timeline.events.push_back(std::move(event));
    }
    return timeline;
}

SequenceContext buildSequenceContext(const MidiEditorState& state,
                                     const KeyScaleData& keyScale,
                                     const Chord* currentChord)
{
    const auto timeline = buildProgressionTimeline(state, keyScale);
    SequenceContext ctx;

    if (timeline.events.empty())
        return ctx;

    if (currentChord == nullptr || currentChord->notes.empty())
    {
        // Caller treats last as current: history is everything before last.
        if (timeline.events.size() > 1)
            ctx.previous.assign(timeline.events.begin(), timeline.events.end() - 1);
        ctx.trim();
        return ctx;
    }

    const auto currentPcs = pitchClassSet(*currentChord);

    // Latest timeline event matching current harmony (by pitch-class set).
    int matchIndex = -1;
    for (int i = static_cast<int>(timeline.events.size()) - 1; i >= 0; --i)
    {
        if (pitchClassSet(timeline.events[static_cast<std::size_t>(i)].chord) == currentPcs)
        {
            matchIndex = i;
            break;
        }
    }

    if (matchIndex >= 0)
    {
        // Strictly before the matched occurrence — never include later blocks.
        ctx.previous.assign(timeline.events.begin(),
                            timeline.events.begin() + matchIndex);
    }
    else
    {
        // Pinned current not on the timeline (e.g. browser audition): full progression is history.
        ctx.previous = timeline.events;
    }

    ctx.trim();
    return ctx;
}

SequenceContext buildSequenceContextBeforeBlock(const MidiEditorState& state,
                                                const KeyScaleData& keyScale,
                                                int blockId)
{
    const auto timeline = buildProgressionTimeline(state, keyScale);
    SequenceContext ctx;

    for (int i = 0; i < static_cast<int>(timeline.events.size()); ++i)
    {
        if (timeline.events[static_cast<std::size_t>(i)].blockId != blockId)
            continue;

        if (i > 0)
            ctx.previous.assign(timeline.events.begin(),
                                timeline.events.begin() + i);
        ctx.trim();
        return ctx;
    }

    return ctx;
}

SequenceContext buildSequenceContextBeforeLast(const MidiEditorState& state,
                                               const KeyScaleData& keyScale)
{
    return buildSequenceContext(state, keyScale, nullptr);
}

}
