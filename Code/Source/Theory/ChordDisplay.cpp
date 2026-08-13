#include "Theory/ChordDisplay.h"

#include "Theory/ChordDatabase.h"
#include "Theory/HarmonicPredicates.h"
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

    int keyTonicPc(Key key)
    {
        return static_cast<int>(key); // Key enum is C=0 … B=11
    }

    // Chromatic roman relative to tonic (jazz/pop chart form: bVII, #iv, …).
    // rel = (root - tonic) mod 12.
    std::string chromaticRomanBody(int rel, bool minorLike, bool dimLike)
    {
        // Prefer flat spellings for mixture colours (bII, bIII, bVI, bVII); #iv for raised 4th.
        static constexpr const char* kMajorBodies[12] = {
            "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII"
        };
        static constexpr const char* kMinorBodies[12] = {
            "i", "bii", "ii", "biii", "iii", "iv", "#iv", "v", "bvi", "vi", "bvii", "vii"
        };

        rel = mod12(rel);
        std::string body = minorLike || dimLike ? kMinorBodies[rel] : kMajorBodies[rel];
        if (dimLike)
            body += "o";
        return body;
    }

    std::string chromaticRomanForChord(const Chord& chord, Key key)
    {
        const int tonic = keyTonicPc(key);
        const int root = NextChordScorer::rootPitchClass(chord);
        const int bass = NextChordScorer::bassPitchClass(chord);
        const auto q = NextChordScorer::detectTriadQuality(chord);

        const bool dimLike = q == TriadQuality::Diminished || q == TriadQuality::HalfDim7;
        const bool minorLike = !dimLike && NextChordScorer::isMinorishQuality(q);
        // Sus/power on chromatic roots stay uppercase (major-like).
        const bool useMinor = minorLike
            || (q == TriadQuality::Sus2 || q == TriadQuality::Sus4 || q == TriadQuality::Power
                ? false
                : false);

        std::string roman = chromaticRomanBody(root - tonic, useMinor || dimLike, dimLike);

        // Optional inversion figures for simple triads (same as detector).
        if (mod12(bass) != mod12(root)
            && (q == TriadQuality::Major || q == TriadQuality::Minor
                || q == TriadQuality::Diminished || q == TriadQuality::Augmented))
        {
            if (mod12(bass) == mod12(root + 3) || mod12(bass) == mod12(root + 4))
                roman += "6";
            else if (mod12(bass) == mod12(root + 7) || mod12(bass) == mod12(root + 6)
                     || mod12(bass) == mod12(root + 8))
                roman += "64";
        }

        return roman;
    }
}

std::string romanForChordInKeyScale(const Chord& chord, Key key, Scale scale)
{
    if (chord.notes.empty())
        return {};

    const auto& keyScale = ChordDatabase::getInstance().get(key, scale);

    // Functional labels first (V/ii, subV/I, mixture tags that already include bVI etc.).
    if (const auto sec = analyseSecondaryDominant(chord, keyScale);
        sec.hit && sec.confidence >= kMinLabelConfidence)
        return sec.label;
    if (const auto tri = analyseTritoneSubstitution(chord, keyScale);
        tri.hit && tri.confidence >= kMinLabelConfidence)
        return tri.label;

    // Diatonic quality-aware roman (I, ii, V, … / I6, …).
    auto diatonic = NextChordScorer::romanForChord(chord, keyScale);
    if (!diatonic.empty())
    {
        // Append inversion figures when scorer didn't (it only does this in ChordDetector path).
        const int root = NextChordScorer::rootPitchClass(chord);
        const int bass = NextChordScorer::bassPitchClass(chord);
        const auto q = NextChordScorer::detectTriadQuality(chord);
        if (mod12(bass) != mod12(root)
            && diatonic.find('/') == std::string::npos
            && diatonic.find('(') == std::string::npos
            && (q == TriadQuality::Major || q == TriadQuality::Minor
                || q == TriadQuality::Diminished || q == TriadQuality::Augmented))
        {
            if (mod12(bass) == mod12(root + 3) || mod12(bass) == mod12(root + 4))
                diatonic += "6";
            else if (mod12(bass) == mod12(root + 7) || mod12(bass) == mod12(root + 6)
                     || mod12(bass) == mod12(root + 8))
                diatonic += "64";
        }
        return diatonic;
    }

    // Chromatic / mixture roots not in the scale: bII, bIII, bVI, bVII, #IV, …
    return chromaticRomanForChord(chord, key);
}

std::string formatAbsoluteWithRoman(const Chord& chord, Key key, Scale scale,
                                    const std::string& absoluteNameFallback)
{
    const std::string absolute = !absoluteNameFallback.empty()
        ? absoluteNameFallback
        : (!chord.readableName.empty() ? chord.readableName : chord.symbol);

    if (absolute.empty())
        return romanForChordInKeyScale(chord, key, scale);

    const auto roman = romanForChordInKeyScale(chord, key, scale);
    if (roman.empty())
        return absolute;

    return absolute + " - " + roman;
}

}
