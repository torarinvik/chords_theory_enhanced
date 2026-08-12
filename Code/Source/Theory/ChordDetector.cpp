#include "Theory/ChordDetector.h"

#include <algorithm>
#include <cmath>

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
    constexpr const char* kFlatNames[12] = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
    };

    bool keyPrefersFlats(Key key)
    {
        switch (key)
        {
            case Key::F: case Key::Bb: case Key::Eb: case Key::Ab: case Key::Db: case Key::Gb:
                return true;
            case Key::C: case Key::D: case Key::E: case Key::G: case Key::A: case Key::B:
                return false;
        }
        return false;
    }

    const char* pcName(int pitchClass, Key spellKey)
    {
        pitchClass = mod12(pitchClass);
        return keyPrefersFlats(spellKey) ? kFlatNames[pitchClass] : kSharpNames[pitchClass];
    }

    int heldCount(const std::array<bool, 12>& pcs)
    {
        auto n = 0;
        for (bool held : pcs)
            if (held)
                ++n;
        return n;
    }

    bool hasPc(const std::array<bool, 12>& pcs, int root, int interval)
    {
        return pcs[static_cast<std::size_t>(mod12(root + interval))];
    }

    // How many non-chord tones are held beyond the quality template.
    int extraToneCount(const std::array<bool, 12>& pcs, int root, TriadQuality quality)
    {
        std::array<bool, 12> inChord {};
        inChord[static_cast<std::size_t>(mod12(root))] = true;
        for (const int interval : TriadLibrary::qualityIntervals(quality))
            inChord[static_cast<std::size_t>(mod12(root + interval))] = true;

        auto extras = 0;
        for (int i = 0; i < 12; ++i)
            if (pcs[static_cast<std::size_t>(i)] && !inChord[static_cast<std::size_t>(i)])
                ++extras;
        return extras;
    }

    int inversionForBass(int root, TriadQuality quality, int bass)
    {
        if (mod12(bass) == mod12(root))
            return 0;

        const auto intervals = TriadLibrary::qualityIntervals(quality);
        for (std::size_t i = 0; i < intervals.size(); ++i)
        {
            if (mod12(root + intervals[i]) == mod12(bass))
                return static_cast<int>(i) + 1;
        }
        // Bass is a non-chord tone — still show slash using generic first-inversion style via makeTriad.
        return 1;
    }

    // Prefer richer exact matches (7ths over triads) and clean inversions.
    int scoreMatch(int toneCount, int extras, bool bassIsChordTone, bool exactBassInversion)
    {
        auto score = toneCount * 20;
        score -= extras * 8;
        if (extras == 0)
            score += 15;
        if (bassIsChordTone)
            score += 6;
        if (exactBassInversion)
            score += 4;
        // Slightly prefer common qualities when scores tie-break later via sort.
        return score;
    }
}

ChordDetection ChordDetector::detect(const std::array<bool, 12>& pcs, int bassPitchClass, Key spellKey)
{
    ChordDetection result;
    const auto nHeld = heldCount(pcs);
    if (nHeld == 0)
        return result;

    bassPitchClass = mod12(bassPitchClass);

    // Single tone — just name the note.
    if (nHeld == 1)
    {
        int only = 0;
        for (int i = 0; i < 12; ++i)
            if (pcs[static_cast<std::size_t>(i)])
                only = i;

        result.matched = true;
        result.name = pcName(only, spellKey);
        result.rootPitchClass = only;
        result.bassPitchClass = only;
        result.toneCount = 1;
        return result;
    }

    static constexpr TriadQuality kQualities[] = {
        TriadQuality::Major7,
        TriadQuality::Minor7,
        TriadQuality::Dominant7,
        TriadQuality::HalfDim7,
        TriadQuality::Major,
        TriadQuality::Minor,
        TriadQuality::Diminished,
        TriadQuality::Augmented,
        TriadQuality::Sus2,
        TriadQuality::Sus4,
        TriadQuality::Power,
    };

    struct Candidate
    {
        int root = 0;
        TriadQuality quality = TriadQuality::Major;
        int score = 0;
        int toneCount = 0;
        int inversion = 0;
    };

    std::optional<Candidate> best;

    for (int root = 0; root < 12; ++root)
    {
        if (!pcs[static_cast<std::size_t>(root)])
            continue; // root must be sounding (standard chord-ID assumption)

        for (const auto quality : kQualities)
        {
            const auto intervals = TriadLibrary::qualityIntervals(quality);
            auto allPresent = true;
            for (const int interval : intervals)
            {
                if (!hasPc(pcs, root, interval))
                {
                    allPresent = false;
                    break;
                }
            }
            if (!allPresent)
                continue;

            const int toneCount = 1 + static_cast<int>(intervals.size());
            const int extras = extraToneCount(pcs, root, quality);
            // Allow a couple of extras (e.g. melody note) but reject very messy clusters.
            if (extras > 2)
                continue;

            const int inv = inversionForBass(root, quality, bassPitchClass);
            bool bassIsChordTone = mod12(bassPitchClass) == mod12(root);
            for (const int interval : intervals)
                if (mod12(root + interval) == mod12(bassPitchClass))
                    bassIsChordTone = true;

            const bool exactBassInversion = bassIsChordTone
                && inv < TriadLibrary::inversionCount(quality);

            const int score = scoreMatch(toneCount, extras, bassIsChordTone, exactBassInversion);

            if (!best.has_value() || score > best->score
                || (score == best->score && toneCount > best->toneCount))
            {
                best = Candidate { root, quality, score, toneCount, inv };
            }
        }
    }

    if (!best.has_value())
    {
        // Fallback: list held pitch classes low→high (still useful when no catalogue match).
        std::string joined;
        for (int i = 0; i < 12; ++i)
        {
            if (!pcs[static_cast<std::size_t>(i)])
                continue;
            if (!joined.empty())
                joined += " ";
            joined += pcName(i, spellKey);
        }
        result.matched = true;
        result.name = joined;
        result.rootPitchClass = bassPitchClass;
        result.bassPitchClass = bassPitchClass;
        result.toneCount = nHeld;
        return result;
    }

    // Prefer bass-correct inversion spelling; clamp inv to quality range.
    const int inv = std::clamp(best->inversion, 0, TriadLibrary::inversionCount(best->quality) - 1);
    const Chord named = TriadLibrary::makeTriad(best->root, best->quality, spellKey, inv);

    result.matched = true;
    result.name = named.readableName.empty() ? named.symbol : named.readableName;
    result.rootPitchClass = best->root;
    result.bassPitchClass = bassPitchClass;
    result.quality = best->quality;
    result.toneCount = best->toneCount;
    return result;
}

ChordDetection ChordDetector::detectFromMidiNotes(const std::vector<int>& midiNotes, Key spellKey)
{
    if (midiNotes.empty())
        return {};

    std::array<bool, 12> pcs {};
    pcs.fill(false);
    int lowestMidi = midiNotes.front();
    for (const int note : midiNotes)
    {
        if (note < 0 || note > 127)
            continue;
        pcs[static_cast<std::size_t>(mod12(note))] = true;
        lowestMidi = std::min(lowestMidi, note);
    }

    if (heldCount(pcs) == 0)
        return {};

    return detect(pcs, mod12(lowestMidi), spellKey);
}

}
