#include "Theory/NextChordScorer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

#include "Theory/Degree.h"
#include "Theory/NoteConvertor.h"

namespace theory
{

namespace
{
    std::set<int> pitchClassSet(const Chord& chord)
    {
        std::set<int> pcs;
        for (const auto& note : chord.notes)
            pcs.insert(note.getPitchClass());
        return pcs;
    }

    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }
}

int NextChordScorer::rootPitchClass(const Chord& chord)
{
    if (chord.notes.empty())
        return 0;
    // chords.json is bass-first for inversions; for triads popularityOrder 1 is root position.
    return chord.notes.front().getPitchClass();
}

int NextChordScorer::pitchClassDistance(int a, int b)
{
    const int d = mod12(a - b);
    return std::min(d, 12 - d);
}

int NextChordScorer::commonToneCount(const Chord& a, const Chord& b)
{
    const auto sa = pitchClassSet(a);
    const auto sb = pitchClassSet(b);
    int count = 0;
    for (const int pc : sa)
        if (sb.contains(pc))
            ++count;
    return count;
}

float NextChordScorer::voiceLeadingCost(const Chord& from, const Chord& to)
{
    // Greedy min-cost assignment of each source pitch class to a unique target pitch class.
    auto fromPcs = pitchClassSet(from);
    auto toPcs = pitchClassSet(to);
    if (fromPcs.empty() || toPcs.empty())
        return 6.0f;

    std::vector<int> src(fromPcs.begin(), fromPcs.end());
    std::vector<int> dst(toPcs.begin(), toPcs.end());

    float total = 0.0f;
    std::vector<bool> used(dst.size(), false);

    for (const int s : src)
    {
        float best = 6.0f;
        std::size_t bestIndex = 0;
        bool found = false;
        for (std::size_t i = 0; i < dst.size(); ++i)
        {
            if (used[i])
                continue;
            const float d = static_cast<float>(pitchClassDistance(s, dst[i]));
            if (d < best)
            {
                best = d;
                bestIndex = i;
                found = true;
            }
        }
        if (found)
        {
            used[bestIndex] = true;
            total += best;
        }
        else
        {
            total += 3.0f; // unmatched source tone
        }
    }

    // Normalise roughly into 0..1 (three tones × max 6 → 18).
    return std::clamp(total / 12.0f, 0.0f, 1.0f);
}

bool NextChordScorer::isDiatonicTriad(const Chord& chord, const KeyScaleData& keyScale)
{
    if (chord.type != ChordType::Triad)
        return false;

    std::set<int> scalePcs;
    for (const auto& note : keyScale.scaleNotes)
        scalePcs.insert(note.getPitchClass());

    for (const auto& note : chord.notes)
        if (!scalePcs.contains(note.getPitchClass()))
            return false;

    return true;
}

void NextChordScorer::score(const Chord& currentChord, const KeyScaleData& keyScale, NextChordCandidate& candidate)
{
    const int shared = commonToneCount(currentChord, candidate.chord);
    const int maxTones = static_cast<int>(std::max(currentChord.notes.size(), candidate.chord.notes.size()));
    const float commonToneTension = maxTones > 0
        ? 1.0f - (static_cast<float>(shared) / static_cast<float>(maxTones))
        : 1.0f;

    const int rootFrom = rootPitchClass(currentChord);
    const int rootTo = rootPitchClass(candidate.chord);
    const int rootInterval = pitchClassDistance(rootFrom, rootTo);
    // 5th/4th (7/5 semitones → distance 5) is strong and expected; tritone (6) and chromatic (1) higher.
    float rootTension = 0.0f;
    if (rootInterval == 0)
        rootTension = 0.05f; // same root (rare after exclude)
    else if (rootInterval == 5)
        rootTension = 0.15f; // fourth/fifth
    else if (rootInterval == 3 || rootInterval == 4)
        rootTension = 0.35f; // third/sixth
    else if (rootInterval == 2)
        rootTension = 0.45f; // whole step
    else if (rootInterval == 1)
        rootTension = 0.75f; // half step
    else
        rootTension = 0.65f; // tritone

    const float vlTension = voiceLeadingCost(currentChord, candidate.chord);
    const bool diatonic = isDiatonicTriad(candidate.chord, keyScale);
    const float chromaticism = diatonic ? 0.0f : 0.85f;

    // Fixed MVP weights (sum = 1.0).
    const float tension01 =
        0.30f * commonToneTension +
        0.25f * rootTension +
        0.25f * vlTension +
        0.20f * chromaticism;

    candidate.tensionPercent = std::clamp(static_cast<int>(std::round(tension01 * 100.0f)), 0, 100);

    std::ostringstream reason;
    if (candidate.degree)
        reason << getDegreeLabel(*candidate.degree);
    else if (diatonic)
        reason << "diatonic";
    else
        reason << "chromatic";

    if (shared > 0)
        reason << " · " << shared << " common";

    if (rootInterval == 5)
        reason << " · 4th/5th";
    else if (rootInterval == 1)
        reason << " · chromatic root";

    candidate.reasonLabel = reason.str();
}

void NextChordScorer::scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                                   std::vector<NextChordCandidate>& candidates)
{
    for (auto& candidate : candidates)
        score(currentChord, keyScale, candidate);

    std::stable_sort(candidates.begin(), candidates.end(),
        [](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            if (a.tensionPercent != b.tensionPercent)
                return a.tensionPercent < b.tensionPercent;
            return a.chord.symbol < b.chord.symbol;
        });
}

}
