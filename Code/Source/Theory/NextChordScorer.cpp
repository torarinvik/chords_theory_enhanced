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

    float lerp(float a, float b, float t)
    {
        return a + (b - a) * t;
    }

    std::set<int> scalePitchClasses(const KeyScaleData& keyScale)
    {
        std::set<int> scalePcs;
        for (const auto& note : keyScale.scaleNotes)
            scalePcs.insert(note.getPitchClass());
        return scalePcs;
    }

    int fifthsIndex(int pitchClass)
    {
        return mod12(pitchClass * 7);
    }

    float rootMotionTension(int interval)
    {
        switch (interval)
        {
            case 0: return 0.08f;
            case 5: return 0.12f;
            case 4: return 0.28f;
            case 3: return 0.30f;
            case 2: return 0.38f;
            case 1: return 0.48f;
            case 6: return 0.90f;
            default: return 0.50f;
        }
    }

    float targetQualityTension(TriadQuality quality)
    {
        switch (quality)
        {
            case TriadQuality::Major:      return 0.00f;
            case TriadQuality::Minor:      return 0.04f;
            case TriadQuality::Power:      return 0.06f;
            case TriadQuality::Sus2:       return 0.09f;
            case TriadQuality::Sus4:       return 0.10f;
            case TriadQuality::Major7:     return 0.05f;
            case TriadQuality::Minor7:     return 0.07f;
            case TriadQuality::Dominant7:  return 0.14f; // wants resolution
            case TriadQuality::HalfDim7:   return 0.17f;
            case TriadQuality::Diminished: return 0.20f;
            case TriadQuality::Augmented:  return 0.26f;
        }
        return 0.0f;
    }

    float sameRootQualityChangePenalty(TriadQuality from, TriadQuality to)
    {
        if (from == to)
            return 0.0f;

        const auto isSusLike = [](TriadQuality q)
        {
            return q == TriadQuality::Sus2 || q == TriadQuality::Sus4 || q == TriadQuality::Power;
        };
        const auto isSeventh = [](TriadQuality q)
        {
            return q == TriadQuality::Major7 || q == TriadQuality::Minor7
                || q == TriadQuality::Dominant7 || q == TriadQuality::HalfDim7;
        };
        const auto isTriadCore = [](TriadQuality q)
        {
            return q == TriadQuality::Major || q == TriadQuality::Minor;
        };

        if (isSusLike(to) || isSusLike(from))
        {
            if (isTriadCore(from) && isSusLike(to))
                return 0.10f;
            if (isSusLike(from) && isTriadCore(to))
                return 0.08f;
            return 0.12f;
        }

        // C → Cmaj7 / Cm7 is a mild colour extension.
        if (isTriadCore(from) && isSeventh(to))
        {
            if (to == TriadQuality::Dominant7)
                return 0.14f;
            return 0.09f;
        }
        if (isSeventh(from) && isTriadCore(to))
            return 0.07f;

        if ((from == TriadQuality::Major && to == TriadQuality::Minor)
            || (from == TriadQuality::Minor && to == TriadQuality::Major))
            return 0.22f;

        if (to == TriadQuality::Diminished || from == TriadQuality::Diminished
            || to == TriadQuality::HalfDim7 || from == TriadQuality::HalfDim7)
            return 0.28f;

        if (to == TriadQuality::Augmented || from == TriadQuality::Augmented)
            return 0.30f;

        return 0.16f;
    }

    // Scale-aware functional offsets (negative = more expected / lower tension).
    float functionalOffset(Degree degree, NextChordScorer::ScaleFamily family)
    {
        using Family = NextChordScorer::ScaleFamily;

        if (family == Family::Minorish)
        {
            // Natural/harmonic minor habits: i–VII–VI–v/V, iv, III.
            switch (degree)
            {
                case Degree::I:   return -0.06f;
                case Degree::II:  return 0.02f;   // often half-dim — a bit edgy
                case Degree::III: return -0.10f;  // relative major — very common
                case Degree::IV:  return -0.11f;  // iv
                case Degree::V:   return -0.12f;  // V or v
                case Degree::VI:  return -0.11f;  // bVI
                case Degree::VII: return -0.09f;  // bVII rock/minor
            }
        }

        if (family == Family::Diminishedish) // Locrian-ish
        {
            switch (degree)
            {
                case Degree::I:   return 0.02f;
                case Degree::II:  return -0.04f;
                case Degree::III: return -0.02f;
                case Degree::IV:  return -0.06f;
                case Degree::V:   return 0.04f;
                case Degree::VI:  return -0.05f;
                case Degree::VII: return -0.08f;
            }
        }

        if (family == Family::ModalSoft) // Dorian / Mixolydian-ish: flatter hierarchy
        {
            switch (degree)
            {
                case Degree::I:   return -0.05f;
                case Degree::II:  return -0.08f;
                case Degree::III: return -0.04f;
                case Degree::IV:  return -0.10f;
                case Degree::V:   return -0.10f;
                case Degree::VI:  return -0.08f;
                case Degree::VII: return -0.06f; // bVII in mixo / natural 7 in dorian differs — soft either way
            }
        }

        // Majorish (Major, Lydian, …)
        switch (degree)
        {
            case Degree::V:   return -0.13f;
            case Degree::VI:  return -0.12f;
            case Degree::IV:  return -0.10f;
            case Degree::II:  return -0.11f;
            case Degree::III: return -0.02f;
            case Degree::VII: return 0.03f;
            case Degree::I:   return -0.05f;
        }
        return 0.0f;
    }

    const char* qualityTag(TriadQuality quality)
    {
        switch (quality)
        {
            case TriadQuality::Major:
            case TriadQuality::Minor:
                return nullptr;
            case TriadQuality::Diminished: return "dim";
            case TriadQuality::Augmented:  return "aug";
            case TriadQuality::Sus2:       return "sus2";
            case TriadQuality::Sus4:       return "sus4";
            case TriadQuality::Power:      return "5";
            case TriadQuality::Major7:     return "maj7";
            case TriadQuality::Minor7:     return "m7";
            case TriadQuality::Dominant7:  return "7";
            case TriadQuality::HalfDim7:   return "m7b5";
        }
        return nullptr;
    }
}

NextChordScorer::ScaleFamily NextChordScorer::scaleFamily(Scale scale)
{
    switch (scale)
    {
        case Scale::Major:
        case Scale::Lydian:
            return ScaleFamily::Majorish;

        case Scale::Mixolydian:
        case Scale::Dorian:
            return ScaleFamily::ModalSoft;

        case Scale::Minor:
        case Scale::HarmonicMinor:
        case Scale::MelodicMinor:
        case Scale::Phrygian:
        case Scale::MinorBlues:
            return ScaleFamily::Minorish;

        case Scale::Locrian:
            return ScaleFamily::Diminishedish;
    }
    return ScaleFamily::Majorish;
}

int NextChordScorer::rootPitchClass(const Chord& chord)
{
    if (chord.notes.empty())
        return 0;
    return chord.notes.front().getPitchClass();
}

int NextChordScorer::pitchClassDistance(int a, int b)
{
    const int d = mod12(a - b);
    return std::min(d, 12 - d);
}

int NextChordScorer::circleOfFifthsDistance(int rootA, int rootB)
{
    const int d = mod12(fifthsIndex(rootA) - fifthsIndex(rootB));
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
    auto fromPcs = pitchClassSet(from);
    auto toPcs = pitchClassSet(to);
    if (fromPcs.empty() || toPcs.empty())
        return 6.0f;

    std::vector<int> src(fromPcs.begin(), fromPcs.end());
    std::vector<int> dst(toPcs.begin(), toPcs.end());
    if (src.size() > dst.size())
        std::swap(src, dst);

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
            total += 3.0f;
        }
    }

    const float denom = 4.0f * static_cast<float>(std::max<std::size_t>(src.size(), 1));
    return std::clamp(total / denom, 0.0f, 1.0f);
}

bool NextChordScorer::isDiatonicChord(const Chord& chord, const KeyScaleData& keyScale)
{
    return nonScaleToneCount(chord, keyScale) == 0 && !chord.notes.empty();
}

int NextChordScorer::nonScaleToneCount(const Chord& chord, const KeyScaleData& keyScale)
{
    const auto scalePcs = scalePitchClasses(keyScale);
    int count = 0;
    for (const auto& note : chord.notes)
        if (!scalePcs.contains(note.getPitchClass()))
            ++count;
    return count;
}

TriadQuality NextChordScorer::detectTriadQuality(const Chord& chord)
{
    if (chord.notes.empty())
        return TriadQuality::Major;

    const int root = rootPitchClass(chord);
    const auto pcs = pitchClassSet(chord);
    const auto has = [&](int interval) -> bool { return pcs.contains(mod12(root + interval)); };

    // Sevenths first (4-note patterns).
    if (pcs.size() >= 4)
    {
        if (has(4) && has(7) && has(11))
            return TriadQuality::Major7;
        if (has(3) && has(7) && has(10))
            return TriadQuality::Minor7;
        if (has(4) && has(7) && has(10))
            return TriadQuality::Dominant7;
        if (has(3) && has(6) && has(10))
            return TriadQuality::HalfDim7;
    }

    switch (chord.type)
    {
        case ChordType::Sus2:  return TriadQuality::Sus2;
        case ChordType::Sus4:  return TriadQuality::Sus4;
        case ChordType::Power: return TriadQuality::Power;
        case ChordType::Triad:
        case ChordType::Seventh:
        case ChordType::Ninth:
        case ChordType::Add9:
        case ChordType::Sixth:
        case ChordType::Sus4Seventh:
        case ChordType::Sus4Ninth:
        case ChordType::Eleventh:
        case ChordType::Thirteenth:
        case ChordType::Inversion1:
        case ChordType::Inversion2:
            break;
    }

    if (pcs.size() == 2 && has(7))
        return TriadQuality::Power;
    if (has(2) && has(7) && !has(3) && !has(4))
        return TriadQuality::Sus2;
    if (has(5) && has(7) && !has(3) && !has(4))
        return TriadQuality::Sus4;
    if (has(4) && has(7))
        return TriadQuality::Major;
    if (has(3) && has(7))
        return TriadQuality::Minor;
    if (has(3) && has(6))
        return TriadQuality::Diminished;
    if (has(4) && has(8))
        return TriadQuality::Augmented;

    return TriadQuality::Major;
}

void NextChordScorer::score(const Chord& currentChord, const KeyScaleData& keyScale, NextChordCandidate& candidate,
                            float drama01)
{
    drama01 = std::clamp(drama01, 0.0f, 1.0f);

    const int shared = commonToneCount(currentChord, candidate.chord);
    const int maxTones = static_cast<int>(std::max(currentChord.notes.size(), candidate.chord.notes.size()));
    float commonToneTension = maxTones > 0
        ? 1.0f - (static_cast<float>(shared) / static_cast<float>(maxTones))
        : 1.0f;

    const bool diatonic = isDiatonicChord(candidate.chord, keyScale);
    if (diatonic && shared == 0)
        commonToneTension *= 0.55f;

    const int rootFrom = rootPitchClass(currentChord);
    const int rootTo = rootPitchClass(candidate.chord);
    const int rootInterval = pitchClassDistance(rootFrom, rootTo);
    const float rootTension = rootMotionTension(rootInterval);
    const float fifthsTension = static_cast<float>(circleOfFifthsDistance(rootFrom, rootTo)) / 6.0f;
    const float vlTension = voiceLeadingCost(currentChord, candidate.chord);

    const int outside = nonScaleToneCount(candidate.chord, keyScale);
    const float noteCount = std::max(1.0f, static_cast<float>(candidate.chord.notes.size()));
    const float chromaticism = std::clamp(static_cast<float>(outside) / noteCount, 0.0f, 1.0f);

    const auto fromQuality = detectTriadQuality(currentChord);
    const auto toQuality = detectTriadQuality(candidate.chord);
    const float qualityTension = targetQualityTension(toQuality);

    float sameRootPenalty = 0.0f;
    if (rootInterval == 0)
        sameRootPenalty = sameRootQualityChangePenalty(fromQuality, toQuality);

    const auto family = scaleFamily(keyScale.scale);
    float functional = 0.0f;
    if (diatonic && candidate.degree.has_value())
        functional = functionalOffset(*candidate.degree, family);
    else if (diatonic)
        functional = -0.04f;

    // Objective tension weights (display bar). Drama reorders via scoreAndSort, not by
    // inventing a different tension number for the same move.
    constexpr float wCt = 0.28f;
    constexpr float wRoot = 0.18f;
    constexpr float wFifths = 0.10f;
    constexpr float wVl = 0.20f;
    constexpr float wChrom = 0.14f;
    constexpr float wQual = 0.10f;
    constexpr float weightSum = wCt + wRoot + wFifths + wVl + wChrom + wQual;

    float tension01 =
        (wCt * commonToneTension +
         wRoot * rootTension +
         wFifths * fifthsTension +
         wVl * vlTension +
         wChrom * chromaticism +
         wQual * qualityTension) / weightSum;

    tension01 += sameRootPenalty;
    // Mild drama influence on function bias: wilder settings care less about "textbook" degrees.
    tension01 += functional * lerp(1.25f, 0.45f, drama01);

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
    else if (rootInterval == 6)
        reason << " · tritone";
    else if (rootInterval == 1)
        reason << " · half-step root";

    if (rootInterval == 0 && sameRootPenalty > 0.0f)
        reason << " · colour change";

    if (const char* tag = qualityTag(toQuality))
        reason << " · " << tag;

    if (outside > 0)
        reason << " · " << outside << " outside";

    candidate.reasonLabel = reason.str();
}

void NextChordScorer::scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                                   std::vector<NextChordCandidate>& candidates, float drama01)
{
    drama01 = std::clamp(drama01, 0.0f, 1.0f);

    for (auto& candidate : candidates)
        score(currentChord, keyScale, candidate, drama01);

    // Drama selects a target tension band: 0 → softest first, 1 → wildest first.
    // Ranking cost is distance to that target so the list always reads "best match first".
    const int targetTension = static_cast<int>(std::round(drama01 * 100.0f));

    std::stable_sort(candidates.begin(), candidates.end(),
        [targetTension](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            const int da = std::abs(a.tensionPercent - targetTension);
            const int db = std::abs(b.tensionPercent - targetTension);
            if (da != db)
                return da < db;
            // Tie-break toward the drama side of the band, then symbol for stability.
            if (a.tensionPercent != b.tensionPercent)
            {
                if (targetTension <= 50)
                    return a.tensionPercent < b.tensionPercent;
                return a.tensionPercent > b.tensionPercent;
            }
            return a.chord.symbol < b.chord.symbol;
        });
}

}
