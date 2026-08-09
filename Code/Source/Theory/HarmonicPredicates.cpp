#include "Theory/HarmonicPredicates.h"

#include "Theory/Degree.h"
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
}

bool isDominantFamilyQuality(TriadQuality quality)
{
    return quality == TriadQuality::Dominant7
        || quality == TriadQuality::HalfDim7; // viiø7 secondary leading-tone family handled elsewhere
}

SecondaryDominantResult analyseSecondaryDominant(const Chord& chord, const KeyScaleData& keyScale)
{
    SecondaryDominantResult out;
    const auto quality = NextChordScorer::detectTriadQuality(chord);
    // Bare major can act as secondary V in pop, but with lower confidence and only if clearly
    // non-diatonic as a plain triad on that root... Prefer dom7 for confident labels.
    const bool isDom7 = quality == TriadQuality::Dominant7;
    const bool isMaj = quality == TriadQuality::Major;
    if (!isDom7 && !isMaj)
        return out;

    const int root = NextChordScorer::rootPitchClass(chord);
    const int resolveRoot = mod12(root + 5);
    const auto targetDeg = NextChordScorer::degreeOfRoot(resolveRoot, keyScale);
    if (!targetDeg)
        return out;

    // Primary V → I is not "secondary".
    if (*targetDeg == Degree::I)
        return out;

    // If the chord is fully diatonic major on a diatonic degree, it's primary harmony (e.g. G in C),
    // not V/x — unless it's dom7 (G7 is V7, still primary for I; already excluded by target I).
    // F major in C is diatonic IV, resolves to Bb — not diatonic, no secondary.
    // D major in C: root D is II, major triad is non-diatonic colour; resolve to G = V → V/V.
    const bool diatonicChord = NextChordScorer::isDiatonicChord(chord, keyScale);
    if (diatonicChord && isMaj)
        return out;

    // Bare major as V/x is only idiomatic for strong diatonic targets (ii, iii, IV, V, vi).
    // F# major in C is V/vii on paper — not a useful secondary label; reject bare major → VII.
    if (isMaj && *targetDeg == Degree::VII)
        return out;

    // Prefer common secondary targets: V/ii, V/iii, V/IV, V/V, V/vi over V/vii.
    float conf = isDom7 ? 0.9f : 0.62f;
    if (*targetDeg == Degree::VII)
        conf *= 0.45f; // rare / weak even as dom7
    else if (*targetDeg == Degree::V || *targetDeg == Degree::II || *targetDeg == Degree::VI)
        conf = std::min(1.0f, conf + 0.05f); // V/V, V/ii, V/vi are textbook
    else if (*targetDeg == Degree::III || *targetDeg == Degree::IV)
        conf = std::min(1.0f, conf + 0.02f);

    out.hit = true;
    out.targetDegree = *targetDeg;
    out.targetRootPc = resolveRoot;
    out.confidence = conf;
    // Quality-aware target: V/ii not V/II (case encodes expected target quality).
    const auto family = NextChordScorer::scaleFamily(keyScale.scale);
    RomanQualityHint targetHint = RomanQualityHint::MinorLike;
    if (family == NextChordScorer::ScaleFamily::Majorish
        || family == NextChordScorer::ScaleFamily::ModalSoft)
    {
        switch (*targetDeg)
        {
            case Degree::I: case Degree::IV: case Degree::V:
                targetHint = RomanQualityHint::MajorLike;
                break;
            case Degree::II: case Degree::III: case Degree::VI:
                targetHint = RomanQualityHint::MinorLike;
                break;
            case Degree::VII:
                targetHint = RomanQualityHint::DimLike;
                break;
        }
    }
    else if (family == NextChordScorer::ScaleFamily::Minorish)
    {
        switch (*targetDeg)
        {
            case Degree::I: case Degree::IV:
                targetHint = RomanQualityHint::MinorLike;
                break;
            case Degree::II:
                targetHint = RomanQualityHint::DimLike;
                break;
            case Degree::V:
                targetHint = RomanQualityHint::MajorLike; // often major V in minor
                break;
            case Degree::III: case Degree::VI: case Degree::VII:
                targetHint = RomanQualityHint::MajorLike;
                break;
        }
    }
    out.label = std::string("V/") + formatRomanNumeral(*targetDeg, targetHint);
    return out;
}

TritoneSubResult analyseTritoneSubstitution(const Chord& chord, const KeyScaleData& keyScale)
{
    TritoneSubResult out;
    const auto quality = NextChordScorer::detectTriadQuality(chord);

    // Strict: only dominant sevenths earn the "tritone substitution" label.
    // Bare major triads (including F, F/C, etc.) must never claim subV.
    if (quality != TriadQuality::Dominant7)
        return out;

    const int root = NextChordScorer::rootPitchClass(chord);
    const int tonic = NextChordScorer::keyTonicPitchClass(keyScale);

    // subV/I: root = tonic + 1 (bII7), classic tritone sub of V.
    if (root == mod12(tonic + 1))
    {
        out.hit = true;
        out.confidence = 0.92f;
        out.resolveRootPc = tonic;
        out.resolveDegree = Degree::I;
        out.label = "subV/I"; // I is major-like tonic in major; keep uppercase
        return out;
    }

    // subV of a diatonic degree: root is tritone away from that degree's V
    // (i.e. root == degRoot + 6), and the chord would resolve to degRoot.
    for (int d = 0; d < static_cast<int>(keyScale.scaleNotes.size()) && d < 7; ++d)
    {
        const int degRoot = keyScale.scaleNotes[static_cast<std::size_t>(d)].getPitchClass();
        if (degRoot == tonic)
            continue; // already handled as subV/I via bII
        if (root != mod12(degRoot + 6))
            continue;

        const auto deg = NextChordScorer::degreeOfRoot(degRoot, keyScale);
        out.hit = true;
        out.confidence = 0.78f;
        out.resolveRootPc = degRoot;
        out.resolveDegree = deg;
        if (deg)
        {
            RomanQualityHint hint = RomanQualityHint::MajorLike;
            if (*deg == Degree::II || *deg == Degree::III || *deg == Degree::VI)
                hint = RomanQualityHint::MinorLike;
            else if (*deg == Degree::VII)
                hint = RomanQualityHint::DimLike;
            out.label = std::string("subV/") + formatRomanNumeral(*deg, hint);
        }
        else
            out.label = "subV";
        return out;
    }

    return out;
}

MixtureResult analyseModeMixture(const Chord& chord, const KeyScaleData& keyScale)
{
    MixtureResult out;
    if (NextChordScorer::isDiatonicChord(chord, keyScale))
        return out;

    const auto family = NextChordScorer::scaleFamily(keyScale.scale);
    if (family != NextChordScorer::ScaleFamily::Majorish
        && family != NextChordScorer::ScaleFamily::ModalSoft)
        return out;

    const int tonic = NextChordScorer::keyTonicPitchClass(keyScale);
    const int root = NextChordScorer::rootPitchClass(chord);
    const int rel = mod12(root - tonic);
    const auto quality = NextChordScorer::detectTriadQuality(chord);

    if (rel == 5 && NextChordScorer::isMinorishQuality(quality))
    {
        out = { true, 0.85f, "iv (mixture)" };
        return out;
    }
    if (rel == 8 && NextChordScorer::isMajorishQuality(quality))
    {
        out = { true, 0.88f, "bVI" };
        return out;
    }
    if (rel == 10 && NextChordScorer::isMajorishQuality(quality))
    {
        out = { true, 0.88f, "bVII" };
        return out;
    }
    if (rel == 3 && NextChordScorer::isMajorishQuality(quality))
    {
        out = { true, 0.8f, "bIII" };
        return out;
    }
    if (rel == 1 && NextChordScorer::isMajorishQuality(quality))
    {
        out = { true, 0.75f, "Neapolitan" };
        return out;
    }
    if (rel == 0 && NextChordScorer::isMinorishQuality(quality))
    {
        out = { true, 0.9f, "i (parallel)" };
        return out;
    }

    return out;
}

bool reasonContainsInvalidTheoryClaim(const std::string& reasonLabel,
                                      const Chord& chord,
                                      const KeyScaleData& keyScale)
{
    const bool claimsTritoneSub =
        reasonLabel.find("tritone sub") != std::string::npos
        || reasonLabel.find("subV") != std::string::npos;
    if (claimsTritoneSub)
    {
        const auto r = analyseTritoneSubstitution(chord, keyScale);
        if (!r.hit || r.confidence < kMinLabelConfidence)
            return true;
    }

    const bool claimsSecondary =
        reasonLabel.find("secondary") != std::string::npos
        || reasonLabel.find("V/") != std::string::npos;
    if (claimsSecondary && reasonLabel.find("subV/") == std::string::npos)
    {
        // Allow "V/ii" style from secondary analysis only.
        const auto r = analyseSecondaryDominant(chord, keyScale);
        if (!r.hit || r.confidence < kMinLabelConfidence)
            return true;
    }

    return false;
}

}
