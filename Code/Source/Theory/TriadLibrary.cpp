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

    // Sharp-leaning vs flat-leaning pitch-class names. Chosen by key signature "side".
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
            case Key::F:
            case Key::Bb:
            case Key::Eb:
            case Key::Ab:
            case Key::Db:
            case Key::Gb:
                return true;
            case Key::C:
            case Key::D:
            case Key::E:
            case Key::G:
            case Key::A:
            case Key::B:
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
}

int TriadLibrary::qualityThirdInterval(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:
        case TriadQuality::Augmented:
            return 4;
        case TriadQuality::Minor:
        case TriadQuality::Diminished:
            return 3;
    }
    return 4;
}

int TriadLibrary::qualityFifthInterval(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:
        case TriadQuality::Minor:
            return 7;
        case TriadQuality::Diminished:
            return 6;
        case TriadQuality::Augmented:
            return 8;
    }
    return 7;
}

std::string TriadLibrary::qualitySuffix(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:       return "";
        case TriadQuality::Minor:       return "m";
        case TriadQuality::Diminished:  return "dim";
        case TriadQuality::Augmented:   return "aug";
    }
    return "";
}

Chord TriadLibrary::makeTriad(int rootPitchClass, TriadQuality quality, Key rootSpellKey)
{
    rootPitchClass = mod12(rootPitchClass);
    const int thirdPc = mod12(rootPitchClass + qualityThirdInterval(quality));
    const int fifthPc = mod12(rootPitchClass + qualityFifthInterval(quality));

    const auto* rootName = pitchClassName(rootPitchClass, rootSpellKey);
    const auto* thirdName = pitchClassName(thirdPc, rootSpellKey);
    const auto* fifthName = pitchClassName(fifthPc, rootSpellKey);

    Chord chord;
    chord.symbol = std::string(rootName) + qualitySuffix(quality);
    chord.readableName = chord.symbol;
    chord.type = ChordType::Triad;
    chord.popularityOrder = 1;
    chord.notes = {
        makeNote(rootName, 1),
        makeNote(thirdName, 3),
        makeNote(fifthName, 5),
    };
    return chord;
}

std::vector<Chord> TriadLibrary::allTriads(Key rootSpellKey)
{
    static constexpr TriadQuality kQualities[kNumQualities] = {
        TriadQuality::Major,
        TriadQuality::Minor,
        TriadQuality::Diminished,
        TriadQuality::Augmented,
    };

    std::vector<Chord> triads;
    triads.reserve(kNumTriads);

    for (int root = 0; root < kNumRoots; ++root)
        for (const auto quality : kQualities)
            triads.push_back(makeTriad(root, quality, rootSpellKey));

    return triads;
}

}
