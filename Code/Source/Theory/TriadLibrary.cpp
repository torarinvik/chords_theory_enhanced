#include "Theory/TriadLibrary.h"

#include "Theory/Key.h"

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

    const char* pitchClassName(int pitchClass, Key rootSpellKey)
    {
        pitchClass = mod12(pitchClass);
        return keyPrefersFlats(rootSpellKey) ? kFlatNames[pitchClass] : kSharpNames[pitchClass];
    }

    NoteName makeNote(const char* name, int positionInChord)
    {
        return NoteName { name, name, positionInChord };
    }

    int roleForInterval(int interval)
    {
        switch (interval)
        {
            case 2: return 2;
            case 3:
            case 4: return 3;
            case 5: return 4; // sus4
            case 6:
            case 7:
            case 8: return 5;
            case 9:
            case 10:
            case 11: return 7;
            default: return 1;
        }
    }
}

std::vector<int> TriadLibrary::qualityIntervals(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:      return { 4, 7 };
        case TriadQuality::Minor:      return { 3, 7 };
        case TriadQuality::Diminished: return { 3, 6 };
        case TriadQuality::Augmented:  return { 4, 8 };
        case TriadQuality::Sus2:       return { 2, 7 };
        case TriadQuality::Sus4:       return { 5, 7 };
        case TriadQuality::Power:      return { 7 };
        case TriadQuality::Major7:     return { 4, 7, 11 };
        case TriadQuality::Minor7:     return { 3, 7, 10 };
        case TriadQuality::Dominant7:  return { 4, 7, 10 };
        case TriadQuality::HalfDim7:   return { 3, 6, 10 };
    }
    return { 4, 7 };
}

std::string TriadLibrary::qualitySuffix(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:      return "";
        case TriadQuality::Minor:      return "m";
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
    return "";
}

ChordType TriadLibrary::chordTypeForQuality(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:
        case TriadQuality::Minor:
        case TriadQuality::Diminished:
        case TriadQuality::Augmented:
            return ChordType::Triad;
        case TriadQuality::Sus2:      return ChordType::Sus2;
        case TriadQuality::Sus4:      return ChordType::Sus4;
        case TriadQuality::Power:     return ChordType::Power;
        case TriadQuality::Major7:
        case TriadQuality::Minor7:
        case TriadQuality::Dominant7:
        case TriadQuality::HalfDim7:
            return ChordType::Seventh;
    }
    return ChordType::Triad;
}

Chord TriadLibrary::makeTriad(int rootPitchClass, TriadQuality quality, Key rootSpellKey)
{
    rootPitchClass = mod12(rootPitchClass);
    const auto* rootName = pitchClassName(rootPitchClass, rootSpellKey);

    Chord chord;
    chord.symbol = std::string(rootName) + qualitySuffix(quality);
    chord.readableName = chord.symbol;
    chord.type = chordTypeForQuality(quality);
    chord.popularityOrder = 1;
    chord.notes = { makeNote(rootName, 1) };

    for (const int interval : qualityIntervals(quality))
    {
        const int pc = mod12(rootPitchClass + interval);
        chord.notes.push_back(makeNote(pitchClassName(pc, rootSpellKey), roleForInterval(interval)));
    }

    return chord;
}

std::vector<Chord> TriadLibrary::allTriads(Key rootSpellKey)
{
    static constexpr TriadQuality kQualities[kNumQualities] = {
        TriadQuality::Major,
        TriadQuality::Minor,
        TriadQuality::Diminished,
        TriadQuality::Augmented,
        TriadQuality::Sus2,
        TriadQuality::Sus4,
        TriadQuality::Power,
        TriadQuality::Major7,
        TriadQuality::Minor7,
        TriadQuality::Dominant7,
        TriadQuality::HalfDim7,
    };

    std::vector<Chord> chords;
    chords.reserve(kNumNamedChords);

    for (int root = 0; root < kNumRoots; ++root)
        for (const auto quality : kQualities)
            chords.push_back(makeTriad(root, quality, rootSpellKey));

    return chords;
}

}
