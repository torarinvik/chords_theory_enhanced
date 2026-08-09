#include "Theory/NoteConvertor.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

#include "Theory/Key.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

namespace
{
    // Wraps x into [0, 12) - std::% keeps the sign of its left operand in C++, so a plain x % 12
    // returns a negative result for negative x, which every mod-12 computation below relies on
    // NOT happening.
    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }

    int getBasePitchClass(char letter)
    {
        switch (letter)
        {
            case 'C': return 0;
            case 'D': return 2;
            case 'E': return 4;
            case 'F': return 5;
            case 'G': return 7;
            case 'A': return 9;
            case 'B': return 11;
            default:  throw std::invalid_argument(std::string("Invalid note letter: ") + letter);
        }
    }
}

int NoteConvertor::parsePitchClass(const std::string& noteName)
{
    if (noteName.empty())
        throw std::invalid_argument("Empty note name");

    int pitchClass = getBasePitchClass(static_cast<char>(std::toupper(static_cast<unsigned char>(noteName[0]))));

    for (std::size_t i = 1; i < noteName.size(); ++i)
    {
        if (noteName[i] == '#')
            ++pitchClass;
        else if (noteName[i] == 'b')
            --pitchClass;
        else
            throw std::invalid_argument("Invalid accidental in note name: " + noteName);
    }

    return mod12(pitchClass);
}

std::vector<int> NoteConvertor::voiceChordCloseToMiddleC(const std::vector<int>& chordTonePitchClasses)
{
    std::vector<int> result;
    result.reserve(chordTonePitchClasses.size());

    for (const int pitchClass : chordTonePitchClasses)
    {
        if (result.empty())
        {
            result.push_back(kMiddleC - mod12(kMiddleC - pitchClass));
            continue;
        }

        const int previousMidiNote = result.back();
        const int candidateBase = previousMidiNote + 1;
        const int distanceToNextMatch = mod12(pitchClass - candidateBase);
        result.push_back(candidateBase + distanceToNextMatch);
    }

    return result;
}

std::vector<int> NoteConvertor::voiceChordCloseToMiddleC(const Chord& chord)
{
    std::vector<int> pitchClasses;
    pitchClasses.reserve(chord.notes.size());

    for (const auto& note : chord.notes)
        pitchClasses.push_back(note.getPitchClass());

    return voiceChordCloseToMiddleC(pitchClasses);
}

Chord NoteConvertor::chooseSmoothestInversion(const Chord& previous, const Chord& target)
{
    if (previous.notes.empty() || target.notes.empty())
        return target;

    // Prefer TriadLibrary inversions when quality is known (correct roles + slash names).
    const int root = NextChordScorer::rootPitchClass(target);
    const auto quality = NextChordScorer::detectTriadQuality(target);
    const int invCount = TriadLibrary::inversionCount(quality);

    Chord best = target;
    float bestCost = 1.0e9f;

    const auto consider = [&](const Chord& candidate)
    {
        if (candidate.notes.empty())
            return;
        // Primary: closed voice-leading cost (0 = smoothest).
        float cost = NextChordScorer::voiceLeadingCost(previous, candidate);
        // Secondary: bass step size (stepwise / common bass preferred).
        const int bassFrom = NextChordScorer::bassPitchClass(previous);
        const int bassTo = NextChordScorer::bassPitchClass(candidate);
        cost += 0.08f * (static_cast<float>(NextChordScorer::pitchClassDistance(bassFrom, bassTo)) / 6.0f);
        // Tiny simplicity preference for root position when costs tie.
        if (bassTo == root)
            cost -= 0.005f;

        if (cost < bestCost - 1.0e-6f)
        {
            bestCost = cost;
            best = candidate;
        }
    };

    // Enumerate catalogue inversions for standard qualities.
    for (int inv = 0; inv < invCount; ++inv)
    {
        // Spelling: rebuild from target root; Key::C is fine for playback pitch classes.
        Chord cand = TriadLibrary::makeTriad(root, quality, Key::C, inv);
        // Preserve display symbol style when root position matches target spelling intent.
        if (inv == 0 && !target.readableName.empty())
        {
            // Keep library spelling; playback only cares about pitch classes.
        }
        consider(cand);
    }

    // Also consider the exact target voicing as given (database slash chords, etc.).
    consider(target);

    // Fallback: cyclic bass rotations of the target's own note list (handles exotic colours).
    if (target.notes.size() > 1)
    {
        for (std::size_t bassIdx = 0; bassIdx < target.notes.size(); ++bassIdx)
        {
            Chord cand = target;
            cand.notes.clear();
            cand.notes.reserve(target.notes.size());
            for (std::size_t k = 0; k < target.notes.size(); ++k)
                cand.notes.push_back(target.notes[(bassIdx + k) % target.notes.size()]);

            // Slash name for non-root bass.
            const int bassPc = cand.notes.front().getPitchClass();
            if (bassPc != root && !cand.notes.front().readableNote.empty())
            {
                // Strip existing slash from symbol if present.
                auto base = target.readableName;
                const auto slash = base.find('/');
                if (slash != std::string::npos)
                    base = base.substr(0, slash);
                cand.readableName = base + "/" + cand.notes.front().readableNote;
                cand.symbol = cand.readableName;
            }
            consider(cand);
        }
    }

    return best;
}

std::vector<int> NoteConvertor::voiceSmoothestPreview(const Chord& previous, const Chord& target)
{
    return voiceChordCloseToMiddleC(chooseSmoothestInversion(previous, target));
}

}
