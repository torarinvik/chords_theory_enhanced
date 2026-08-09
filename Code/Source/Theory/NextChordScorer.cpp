#include "Theory/NextChordScorer.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <string_view>
#include <vector>

#include "Theory/Degree.h"
#include "Theory/HarmonicPredicates.h"
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

    int degreeIndex(Degree degree)
    {
        return static_cast<int>(degree);
    }

    Degree degreeFromIndex(int index)
    {
        return static_cast<Degree>(std::clamp(index, 0, 6));
    }

    bool isSusLike(TriadQuality q)
    {
        return q == TriadQuality::Sus2 || q == TriadQuality::Sus4 || q == TriadQuality::Power;
    }

    bool isSeventh(TriadQuality q)
    {
        return q == TriadQuality::Major7 || q == TriadQuality::Minor7
            || q == TriadQuality::Dominant7 || q == TriadQuality::HalfDim7;
    }

    bool isTriadCore(TriadQuality q)
    {
        return q == TriadQuality::Major || q == TriadQuality::Minor;
    }

    [[maybe_unused]] float rootMotionTension(int minInterval, int directed)
    {
        // Prefer directed cadential motion (up a 4th / down a 5th = +5) over plain distance.
        if (directed == 5)
            return 0.08f; // strongest root cadence
        if (directed == 7)
            return 0.14f; // up a 5th / down a 4th — common departure
        if (directed == 2 || directed == 10)
            return 0.34f; // step
        if (directed == 9 || directed == 3)
            return 0.30f; // mediant-ish
        if (directed == 8 || directed == 4)
            return 0.32f;
        if (directed == 1 || directed == 11)
            return 0.46f; // half-step root
        if (directed == 6)
            return 0.88f; // tritone
        if (directed == 0)
            return 0.08f;

        switch (minInterval)
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

    [[maybe_unused]] float targetQualityTension(TriadQuality quality)
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
            case TriadQuality::Dominant7:  return 0.14f;
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

        if (isSusLike(to) || isSusLike(from))
        {
            if (isTriadCore(from) && isSusLike(to))
                return 0.09f;
            if (isSusLike(from) && isTriadCore(to))
                return 0.07f;
            return 0.11f;
        }

        if (isTriadCore(from) && isSeventh(to))
        {
            if (to == TriadQuality::Dominant7)
                return 0.12f;
            return 0.07f;
        }
        if (isSeventh(from) && isTriadCore(to))
            return 0.06f;
        if (isSeventh(from) && isSeventh(to))
            return 0.10f;

        if ((from == TriadQuality::Major && to == TriadQuality::Minor)
            || (from == TriadQuality::Minor && to == TriadQuality::Major))
            return 0.22f;

        if (to == TriadQuality::Diminished || from == TriadQuality::Diminished
            || to == TriadQuality::HalfDim7 || from == TriadQuality::HalfDim7)
            return 0.26f;

        if (to == TriadQuality::Augmented || from == TriadQuality::Augmented)
            return 0.28f;

        return 0.15f;
    }

    // Scale-aware static degree bias (negative = more expected).
    float functionalOffset(Degree degree, NextChordScorer::ScaleFamily family)
    {
        using Family = NextChordScorer::ScaleFamily;

        if (family == Family::Minorish)
        {
            switch (degree)
            {
                case Degree::I:   return -0.07f;
                case Degree::II:  return 0.01f;
                case Degree::III: return -0.11f;
                case Degree::IV:  return -0.11f;
                case Degree::V:   return -0.13f;
                case Degree::VI:  return -0.12f;
                case Degree::VII: return -0.10f;
            }
        }

        if (family == Family::Diminishedish)
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

        if (family == Family::ModalSoft)
        {
            switch (degree)
            {
                case Degree::I:   return -0.06f;
                case Degree::II:  return -0.08f;
                case Degree::III: return -0.04f;
                case Degree::IV:  return -0.10f;
                case Degree::V:   return -0.10f;
                case Degree::VI:  return -0.08f;
                case Degree::VII: return -0.07f;
            }
        }

        // Majorish
        switch (degree)
        {
            case Degree::V:   return -0.13f;
            case Degree::VI:  return -0.12f;
            case Degree::IV:  return -0.10f;
            case Degree::II:  return -0.11f;
            case Degree::III: return -0.03f;
            case Degree::VII: return 0.02f;
            case Degree::I:   return -0.06f;
        }
        return 0.0f;
    }

    // How well the quality matches the "default" colour of a diatonic degree.
    float qualityDegreeFitness(Degree degree, TriadQuality quality, NextChordScorer::ScaleFamily family)
    {
        using Family = NextChordScorer::ScaleFamily;

        // Expected primary quality class per degree.
        enum class Expect { MajorLike, MinorLike, DimLike, Flexible };
        Expect expect = Expect::Flexible;

        if (family == Family::Majorish)
        {
            switch (degree)
            {
                case Degree::I: case Degree::IV: case Degree::V:
                    expect = Expect::MajorLike; break;
                case Degree::II: case Degree::III: case Degree::VI:
                    expect = Expect::MinorLike; break;
                case Degree::VII:
                    expect = Expect::DimLike; break;
            }
        }
        else if (family == Family::Minorish)
        {
            switch (degree)
            {
                case Degree::I: case Degree::IV:
                    expect = Expect::MinorLike; break;
                case Degree::III: case Degree::VI: case Degree::VII:
                    expect = Expect::MajorLike; break;
                case Degree::II:
                    expect = Expect::DimLike; break;
                case Degree::V:
                    expect = Expect::Flexible; // V or v both common
                    break;
            }
        }
        else if (family == Family::ModalSoft)
        {
            // Flatter expectations — reward diatonic colour lightly.
            switch (degree)
            {
                case Degree::I: case Degree::IV: case Degree::V:
                    expect = Expect::MajorLike; break;
                case Degree::II: case Degree::III: case Degree::VI:
                    expect = Expect::MinorLike; break;
                case Degree::VII:
                    expect = Expect::Flexible; break;
            }
        }
        else // Diminishedish
        {
            expect = Expect::Flexible;
        }

        const bool majorLike = NextChordScorer::isMajorishQuality(quality)
            || quality == TriadQuality::Dominant7
            || quality == TriadQuality::Sus2
            || quality == TriadQuality::Sus4
            || quality == TriadQuality::Power;
        const bool minorLike = NextChordScorer::isMinorishQuality(quality)
            || quality == TriadQuality::Minor7;
        const bool dimLike = quality == TriadQuality::Diminished
            || quality == TriadQuality::HalfDim7;

        switch (expect)
        {
            case Expect::MajorLike:
                if (majorLike) return -0.05f;
                if (minorLike) return 0.10f;
                if (dimLike) return 0.08f;
                return 0.04f;
            case Expect::MinorLike:
                if (minorLike) return -0.05f;
                if (majorLike) return 0.10f;
                if (dimLike) return 0.04f;
                return 0.03f;
            case Expect::DimLike:
                if (dimLike) return -0.05f;
                if (quality == TriadQuality::Dominant7) return 0.02f;
                if (majorLike) return 0.08f;
                return 0.03f;
            case Expect::Flexible:
                return 0.0f;
        }
        return 0.0f;
    }

    // Progression grammar: classic degree-to-degree affinity (negative = smoother).
    float progressionGrammar(Degree from, Degree to, NextChordScorer::ScaleFamily family)
    {
        if (from == to)
            return 0.02f; // same degree, quality change handled elsewhere

        using Family = NextChordScorer::ScaleFamily;
        const int f = degreeIndex(from);
        const int t = degreeIndex(to);

        // Circle-of-fifths chain along the scale: ii→V→I, vi→ii, iii→vi, etc.
        // Degree index steps of +3 or +4 (rough) are not exact; use explicit pairs.

        struct Pair { Degree a; Degree b; float majorBias; float minorBias; };
        // majorBias / minorBias are additive offsets (more negative = more expected).
        static constexpr Pair kPairs[] = {
            // Cadences & approaches (kept modest so surface terms still differentiate).
            { Degree::V,   Degree::I,   -0.11f, -0.10f },
            { Degree::V,   Degree::VI,  -0.07f, -0.05f }, // deceptive
            { Degree::IV,  Degree::I,   -0.10f, -0.09f }, // plagal
            { Degree::II,  Degree::V,   -0.10f, -0.06f }, // ii–V
            { Degree::II,  Degree::I,   -0.03f, -0.02f },
            { Degree::IV,  Degree::V,   -0.08f, -0.07f },
            { Degree::I,   Degree::V,   -0.06f, -0.05f },
            { Degree::I,   Degree::IV,  -0.06f, -0.06f },
            { Degree::I,   Degree::VI,  -0.07f, -0.04f },
            { Degree::I,   Degree::II,  -0.06f, -0.03f },
            { Degree::I,   Degree::III, -0.02f, -0.06f }, // major: weaker; minor: to III strong
            { Degree::VI,  Degree::II,  -0.06f, -0.04f },
            { Degree::VI,  Degree::IV,  -0.05f, -0.05f },
            { Degree::VI,  Degree::V,   -0.04f, -0.04f },
            { Degree::III, Degree::VI,  -0.06f, -0.05f },
            { Degree::III, Degree::IV,  -0.03f, -0.04f },
            { Degree::IV,  Degree::II,  -0.03f, -0.03f },
            { Degree::VII, Degree::I,   -0.06f, -0.08f }, // leading / bVII→i
            { Degree::VII, Degree::III, -0.03f, -0.05f },
            // Minor favourites
            { Degree::I,   Degree::VII, -0.02f, -0.07f }, // i→bVII
            { Degree::I,   Degree::VI,  -0.04f, -0.07f }, // i→bVI
            { Degree::VI,  Degree::V,   -0.03f, -0.06f },
            { Degree::VI,  Degree::VII, -0.02f, -0.05f },
            { Degree::VII, Degree::VI,  -0.03f, -0.06f },
            { Degree::III, Degree::VII, -0.02f, -0.05f },
            { Degree::III, Degree::I,   -0.02f, -0.05f },
        };

        float best = 0.0f;
        bool found = false;
        for (const auto& p : kPairs)
        {
            if (p.a != from || p.b != to)
                continue;
            const float v = (family == Family::Minorish) ? p.minorBias : p.majorBias;
            if (!found || v < best)
            {
                best = v;
                found = true;
            }
        }

        if (found)
            return best;

        (void) f;
        (void) t;
        return 0.0f;
    }

    // Mode mixture / borrowed chords relative to major tonic (and inverse for minor).
    struct MixtureHit
    {
        bool hit = false;
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    MixtureHit detectModeMixture(int tonic, int rootTo, TriadQuality toQuality,
                                 NextChordScorer::ScaleFamily family, bool targetDiatonic)
    {
        MixtureHit out;
        if (targetDiatonic)
            return out;

        const int rel = mod12(rootTo - tonic);
        using Family = NextChordScorer::ScaleFamily;

        if (family == Family::Majorish || family == Family::ModalSoft)
        {
            // Borrowed from parallel minor. These are idiomatic *colour*, not soft cadences:
            // root motion can already look plagal/stepwise (C→Fm shares the I→IV bass leap), so a
            // negative theory bonus made mixture iv outrank plain diatonic F/Am/G. Keep tags for
            // the UI, but bias slightly *up* in tension — still softer than random chromatic.
            if (rel == 5 && NextChordScorer::isMinorishQuality(toQuality))
            { out = { true, 0.07f, "mixture iv" }; return out; }   // Fm after C — darker than F
            if (rel == 8 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, -0.02f, "mixture bVI" }; return out; } // Ab still a common rock colour
            if (rel == 3 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, 0.05f, "mixture bIII" }; return out; }
            if (rel == 10 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, -0.03f, "mixture bVII" }; return out; } // Bb — very common
            if (rel == 1 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, 0.06f, "Neapolitan" }; return out; }
            if (rel == 0 && NextChordScorer::isMinorishQuality(toQuality))
            { out = { true, 0.10f, "parallel minor" }; return out; } // i — clear colour shift
        }

        if (family == Family::Minorish)
        {
            // Picardy / major IV / V already partly diatonic; reward bright borrows lightly.
            if (rel == 0 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, -0.04f, "Picardy" }; return out; }
            if (rel == 5 && NextChordScorer::isMajorishQuality(toQuality))
            { out = { true, -0.05f, "major IV" }; return out; }
        }

        return out;
    }

    struct SecondaryHit
    {
        bool hit = false;
        float offset = 0.0f;
        const char* tag = nullptr;
        std::optional<Degree> targetDegree;
    };

    // How often a secondary dominant of this degree shows up in common practice / pop / jazz.
    float secondaryTargetWeight(Degree target)
    {
        switch (target)
        {
            case Degree::V:   return 1.00f; // V/V
            case Degree::II:  return 0.95f; // V/ii
            case Degree::VI:  return 0.90f; // V/vi
            case Degree::IV:  return 0.80f; // V/IV
            case Degree::III: return 0.32f; // V/iii — less common from I
            case Degree::VII: return 0.20f; // V/vii — rare
            case Degree::I:   return 0.0f;  // primary V, not secondary
        }
        return 0.5f;
    }

    SecondaryHit detectSecondaryDominant(int rootTo, TriadQuality toQuality, const KeyScaleData& keyScale)
    {
        SecondaryHit out;
        // Bare major triads can act as secondary V; sus is usually primary V colour, not V/x.
        const float qualityBoost = (toQuality == TriadQuality::Dominant7) ? -0.11f
            : (toQuality == TriadQuality::Major) ? -0.05f
            : 0.0f;
        if (qualityBoost == 0.0f)
            return out;

        // Resolves up a fourth to a diatonic degree root.
        const int resolveRoot = mod12(rootTo + 5);
        const auto targetDeg = NextChordScorer::degreeOfRoot(resolveRoot, keyScale);
        if (!targetDeg)
            return out;

        const float weight = secondaryTargetWeight(*targetDeg);
        if (weight <= 0.0f)
            return out;

        out.hit = true;
        out.targetDegree = targetDeg;
        out.offset = qualityBoost * weight;
        out.tag = "secondary V";
        return out;
    }

    // When the *current* chord is dominant-like, prefer its natural resolution target.
    struct DominantResolveHit
    {
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    DominantResolveHit dominantResolutionBias(int rootFrom, TriadQuality fromQuality,
                                              int rootTo, TriadQuality toQuality,
                                              int tonic, const KeyScaleData& keyScale)
    {
        DominantResolveHit out;
        const bool fromDom7 = fromQuality == TriadQuality::Dominant7;
        const bool fromDomTriad = fromQuality == TriadQuality::Major
            || fromQuality == TriadQuality::Sus4;
        if (!fromDom7 && !fromDomTriad)
            return out;

        // Only treat bare major as "dominant needing resolution" when it is V of something diatonic.
        if (!fromDom7)
        {
            const auto resolvesTo = NextChordScorer::degreeOfRoot(mod12(rootFrom + 5), keyScale);
            if (!resolvesTo)
                return out;
        }

        const int dir = NextChordScorer::directedRootInterval(rootFrom, rootTo);
        const bool targetStable = NextChordScorer::isMajorishQuality(toQuality)
            || NextChordScorer::isMinorishQuality(toQuality)
            || toQuality == TriadQuality::Major7
            || toQuality == TriadQuality::Minor7
            || toQuality == TriadQuality::Power;

        // Classic V7 → I (root up a 4th).
        if (dir == 5 && targetStable)
        {
            out.offset = fromDom7 ? -0.10f : -0.06f;
            out.tag = "resolve";
            return out;
        }

        // Tritone-sub resolution: bII7 → I (root down a half-step / up 11).
        if (fromDom7 && dir == 11 && targetStable)
        {
            out.offset = -0.09f;
            out.tag = "subV resolve";
            return out;
        }

        // Deceptive from any dominant: up a step to a minor/major sixth-related target (V→vi shape).
        if (fromDom7 && dir == 2 && NextChordScorer::isMinorishQuality(toQuality))
        {
            out.offset = -0.04f;
            out.tag = "deceptive";
            return out;
        }

        // Leaving an unresolved dominant 7th is a bit more tense than average motion.
        if (fromDom7 && targetStable)
        {
            out.offset = 0.045f;
            out.tag = nullptr;
        }

        (void) tonic;
        return out;
    }

    // bVII→I / iv→I backdoor & minor-plagal colour (from non-diatonic or modal roots).
    struct BackdoorHit
    {
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    BackdoorHit detectBackdoor(int tonic, int rootFrom, TriadQuality fromQuality,
                               int /*rootTo*/, TriadQuality toQuality, bool toIsTonic)
    {
        BackdoorHit out;
        if (!toIsTonic)
            return out;
        if (!(NextChordScorer::isMajorishQuality(toQuality) || NextChordScorer::isMinorishQuality(toQuality)))
            return out;

        const int fromRel = mod12(rootFrom - tonic);
        // bVII major → I
        if (fromRel == 10 && NextChordScorer::isMajorishQuality(fromQuality))
        {
            out = { -0.08f, "backdoor" };
            return out;
        }
        // iv minor → I (minor plagal / backdoor cousin)
        if (fromRel == 5 && NextChordScorer::isMinorishQuality(fromQuality))
        {
            out = { -0.07f, "minor plagal" };
            return out;
        }
        return out;
    }

    // Sus2/Sus4 want to settle to a triad (same root) or drive as V-sus → I.
    struct SusHit
    {
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    SusHit detectSusMotion(int rootFrom, TriadQuality fromQuality,
                           int rootTo, TriadQuality toQuality, int tonic)
    {
        SusHit out;
        if (fromQuality != TriadQuality::Sus2 && fromQuality != TriadQuality::Sus4)
            return out;

        if (rootFrom == rootTo
            && (toQuality == TriadQuality::Major || toQuality == TriadQuality::Minor
                || toQuality == TriadQuality::Dominant7 || toQuality == TriadQuality::Major7
                || toQuality == TriadQuality::Minor7 || toQuality == TriadQuality::Power))
        {
            out = { -0.07f, "sus resolve" };
            return out;
        }

        // Vsus → I
        if (rootFrom == mod12(tonic + 7) && rootTo == tonic
            && (NextChordScorer::isMajorishQuality(toQuality) || NextChordScorer::isMinorishQuality(toQuality)))
        {
            out = { -0.08f, "sus cadence" };
            return out;
        }
        return out;
    }

    // Diatonic falling-fifths glue (…→ii→V→I, vi→ii, etc.) when grammar is silent.
    float fallingFifthsBonus(bool fromDiatonic, bool toDiatonic, int rootDir, float grammarAlready)
    {
        if (!fromDiatonic || !toDiatonic || rootDir != 5)
            return 0.0f;
        // Avoid double-paying moves that already have a strong grammar entry.
        if (grammarAlready <= -0.06f)
            return -0.015f;
        return -0.04f;
    }

    // Blues / rock: I7 and IV7 are idiomatic colour, not "wrong V-ness".
    [[maybe_unused]] float bluesDominantAdjust(TriadQuality toQuality, std::optional<Degree> toDegree,
                              NextChordScorer::ScaleFamily family, float qualityTension)
    {
        if (toQuality != TriadQuality::Dominant7 || !toDegree)
            return qualityTension;

        using Family = NextChordScorer::ScaleFamily;
        if (family != Family::Majorish && family != Family::ModalSoft && family != Family::Minorish)
            return qualityTension;

        if (*toDegree == Degree::I || *toDegree == Degree::IV)
            return qualityTension * 0.45f;
        if (*toDegree == Degree::V)
            return qualityTension * 0.65f;
        return qualityTension;
    }

    // Prefer diatonic seventh colours that match jazz/pop default voicings.
    float seventhColourBonus(Degree degree, TriadQuality quality, NextChordScorer::ScaleFamily family)
    {
        using Family = NextChordScorer::ScaleFamily;
        if (family == Family::Majorish || family == Family::ModalSoft)
        {
            if (degree == Degree::V && quality == TriadQuality::Dominant7) return -0.035f;
            if (degree == Degree::II && quality == TriadQuality::Minor7) return -0.03f;
            if (degree == Degree::I && quality == TriadQuality::Major7) return -0.025f;
            if (degree == Degree::VI && quality == TriadQuality::Minor7) return -0.02f;
            if (degree == Degree::IV && quality == TriadQuality::Major7) return -0.02f;
            if (degree == Degree::VII && quality == TriadQuality::HalfDim7) return -0.025f;
        }
        if (family == Family::Minorish)
        {
            if (degree == Degree::V && quality == TriadQuality::Dominant7) return -0.03f;
            if (degree == Degree::II && quality == TriadQuality::HalfDim7) return -0.03f;
            if (degree == Degree::I && quality == TriadQuality::Minor7) return -0.025f;
            if (degree == Degree::IV && quality == TriadQuality::Minor7) return -0.02f;
        }
        return 0.0f;
    }

    // Chromatic approach: diminished or dom a half-step below/above a diatonic root.
    struct ApproachHit
    {
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    ApproachHit detectApproachChord(int rootTo, TriadQuality toQuality, const KeyScaleData& keyScale)
    {
        ApproachHit out;
        const bool approachColour = toQuality == TriadQuality::Diminished
            || toQuality == TriadQuality::Dominant7
            || toQuality == TriadQuality::HalfDim7;
        if (!approachColour)
            return out;

        // Chord root is a half-step below a diatonic degree (classic ♯i°/ct° approach into ii, etc.)
        for (const int delta : { 1, 11 })
        {
            const int targetRoot = mod12(rootTo + delta);
            if (NextChordScorer::degreeOfRoot(targetRoot, keyScale))
            {
                out = { -0.04f, "approach" };
                return out;
            }
        }
        return out;
    }

    struct TritoneSubHit
    {
        bool hit = false;
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    TritoneSubHit detectTritoneSub(const Chord& chord, const KeyScaleData& keyScale)
    {
        TritoneSubHit out;
        // Strict predicate: dominant-seventh only; never bare major / slash chords like F/C.
        const auto analysed = analyseTritoneSubstitution(chord, keyScale);
        if (!analysed.hit || analysed.confidence < kMinLabelConfidence)
            return out;
        out.hit = true;
        out.offset = -0.10f * analysed.confidence;
        out.tag = "tritone sub";
        return out;
    }

    struct MediantHit
    {
        bool hit = false;
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    MediantHit detectChromaticMediant(int rootFrom, int rootTo, TriadQuality fromQ, TriadQuality toQ,
                                      bool fromDiatonic, bool toDiatonic)
    {
        MediantHit out;
        const int dir = NextChordScorer::directedRootInterval(rootFrom, rootTo);
        const bool mediantRoot = (dir == 3 || dir == 4 || dir == 8 || dir == 9);
        if (!mediantRoot)
            return out;

        const bool sameMajor = NextChordScorer::isMajorishQuality(fromQ)
            && NextChordScorer::isMajorishQuality(toQ);
        const bool sameMinor = NextChordScorer::isMinorishQuality(fromQ)
            && NextChordScorer::isMinorishQuality(toQ);
        if (!sameMajor && !sameMinor)
            return out;

        // Chromatic mediant when not both diatonic (diatonic III/vi already covered by grammar).
        if (fromDiatonic && toDiatonic)
            return out;

        out = { true, -0.06f, "chrom. mediant" };
        return out;
    }

    float tendencyToneBonus(const Chord& from, const Chord& to, int tonic,
                            NextChordScorer::ScaleFamily family)
    {
        // Reward classic resolutions of tendency tones present in the current chord.
        const auto fromPcs = pitchClassSet(from);
        const auto toPcs = pitchClassSet(to);
        float bonus = 0.0f; // negative = lower tension

        const int leading = mod12(tonic + 11); // 7 in major / raised 7 often
        const int tonicPc = tonic;
        const int fa = mod12(tonic + 5);      // 4
        const int mi = mod12(tonic + 4);      // 3 major
        const int me = mod12(tonic + 3);      // b3 minor

        if (fromPcs.contains(leading) && !toPcs.contains(leading) && toPcs.contains(tonicPc))
            bonus -= 0.04f;

        if (family == NextChordScorer::ScaleFamily::Majorish
            || family == NextChordScorer::ScaleFamily::ModalSoft)
        {
            if (fromPcs.contains(fa) && !toPcs.contains(fa) && toPcs.contains(mi))
                bonus -= 0.025f;
        }
        else if (family == NextChordScorer::ScaleFamily::Minorish)
        {
            if (fromPcs.contains(fa) && !toPcs.contains(fa) && (toPcs.contains(me) || toPcs.contains(mi)))
                bonus -= 0.02f;
        }

        // Dominant seventh: guide-tone resolution (3↑ half-step, b7↓ half-step).
        const int fromRoot = NextChordScorer::rootPitchClass(from);
        if (NextChordScorer::detectTriadQuality(from) == TriadQuality::Dominant7)
        {
            const int third = mod12(fromRoot + 4);
            const int seventh = mod12(fromRoot + 10);
            if (fromPcs.contains(third) && fromPcs.contains(seventh))
            {
                const int thirdUp = mod12(third + 1);
                const int seventhDown = mod12(seventh - 1);
                if (toPcs.contains(thirdUp) && toPcs.contains(seventhDown))
                    bonus -= 0.05f;
            }
        }

        return bonus;
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

    float roleTransitionBias(NextChordScorer::HarmonicRole from, NextChordScorer::HarmonicRole to)
    {
        using Role = NextChordScorer::HarmonicRole;
        if (from == Role::Predominant && to == Role::Dominant) return -0.05f;
        if (from == Role::Dominant && to == Role::Tonic) return -0.07f;
        if (from == Role::Predominant && to == Role::Tonic) return -0.05f; // plagal family
        if (from == Role::Tonic && to == Role::Predominant) return -0.03f;
        if (from == Role::Tonic && to == Role::Dominant) return -0.03f;
        if (from == Role::Dominant && to == Role::Predominant) return 0.04f; // retrogression
        if (from == Role::Dominant && to == Role::Dominant) return 0.02f;
        if (from == Role::Tonic && to == Role::Tonic) return 0.02f; // I↔iii/vi prolongation is mild, not a cadence
        if (from == Role::Chromatic && to == Role::Tonic) return -0.03f;
        if (from == Role::Modal && to == Role::Tonic) return -0.03f;
        return 0.0f;
    }

    // Phrase-memory bias from progression slots that already happened before `current`.
    struct SequenceBias
    {
        float offset = 0.0f;
        const char* tag = nullptr;
    };

    SequenceBias sequenceContextBias(const Chord& currentChord,
                                     const NextChordCandidate& candidate,
                                     const KeyScaleData& keyScale,
                                     const SequenceContext& sequence,
                                     std::optional<Degree> fromDegree,
                                     std::optional<Degree> toDegree,
                                     int rootFrom,
                                     int rootTo,
                                     int rootDir)
    {
        SequenceBias out;
        if (sequence.empty())
            return out;

        const auto candPcs = pitchClassSet(candidate.chord);
        float offset = 0.0f;
        const char* tag = nullptr;

        // --- Repeat / saturation ---
        // Exact PC-set seen recently is a bit boring (except after a long gap of different chords).
        const int look = std::min(sequence.size(), 4);
        for (int i = 0; i < look; ++i)
        {
            const auto& event = sequence.previous[static_cast<std::size_t>(sequence.size() - 1 - i)];
            if (pitchClassSet(event.chord) == candPcs)
            {
                // Immediate previous (i==0 relative to sequence, not current) is "came back soon".
                offset += (i == 0) ? 0.06f : 0.035f;
                if (tag == nullptr)
                    tag = "repeat";
            }
        }

        // Same root thrice in the recent window → prefer a different root.
        {
            int sameRoot = 0;
            const int candRoot = rootTo;
            if (NextChordScorer::rootPitchClass(currentChord) == candRoot)
                ++sameRoot;
            for (int i = 0; i < look; ++i)
            {
                const auto& event = sequence.previous[static_cast<std::size_t>(sequence.size() - 1 - i)];
                if (NextChordScorer::rootPitchClass(event.chord) == candRoot)
                    ++sameRoot;
            }
            if (sameRoot >= 3)
            {
                offset += 0.05f;
                if (tag == nullptr)
                    tag = "root fatigue";
            }
        }

        // --- Falling-fifths chain continuation ---
        // If the last step into current was a falling fifth (root +5), keep the chain going.
        if (!sequence.previous.empty())
        {
            const int prevRoot = NextChordScorer::rootPitchClass(sequence.previous.back().chord);
            const int lastDir = NextChordScorer::directedRootInterval(prevRoot, rootFrom);
            if (lastDir == 5 && rootDir == 5)
            {
                offset -= 0.055f;
                tag = "5ths chain";
            }
        }

        // --- ii–V–I completion ---
        // History … II, current V → prefer I (and Imaj7 / tonic minor in minor keys).
        if (fromDegree && *fromDegree == Degree::V && toDegree && *toDegree == Degree::I
            && !sequence.previous.empty())
        {
            const auto& prev = sequence.previous.back();
            if (prev.degree && *prev.degree == Degree::II)
            {
                offset -= 0.09f;
                tag = "ii–V–I";
            }
        }

        // History … V, current is not yet I, and candidate is I after a ii–V setup in last two.
        if (sequence.size() >= 2 && toDegree && *toDegree == Degree::I)
        {
            const auto& a = sequence.previous[static_cast<std::size_t>(sequence.size() - 2)];
            const auto& b = sequence.previous.back();
            if (a.degree && b.degree && *a.degree == Degree::II && *b.degree == Degree::V
                && fromDegree && *fromDegree == Degree::V)
            {
                offset -= 0.08f;
                tag = "ii–V–I";
            }
        }

        // Current is ii → strongly prefer V next when history already sat on predominant material.
        if (fromDegree && *fromDegree == Degree::II && toDegree && *toDegree == Degree::V)
        {
            offset -= 0.03f; // extra on top of grammar
            if (tag == nullptr)
                tag = "ii–V";
        }

        // --- Turnaround / loop home ---
        // After several chords away from I, returning to I is welcome.
        if (toDegree && *toDegree == Degree::I && sequence.size() >= 2)
        {
            bool leftTonic = true;
            for (const auto& event : sequence.previous)
            {
                if (event.degree && *event.degree == Degree::I)
                    leftTonic = false;
            }
            if (leftTonic && (!fromDegree || *fromDegree != Degree::I))
            {
                offset -= 0.03f;
                if (tag == nullptr)
                    tag = "home";
            }
        }

        // --- Andalusian / minor tetrachord: i → bVII → bVI → V ---
        if (NextChordScorer::scaleFamily(keyScale.scale) == NextChordScorer::ScaleFamily::Minorish
            && !sequence.previous.empty() && fromDegree && toDegree)
        {
            const auto& prev = sequence.previous.back();
            if (prev.degree && *prev.degree == Degree::I && *fromDegree == Degree::VII
                && *toDegree == Degree::VI)
            {
                offset -= 0.07f;
                tag = "andalusian";
            }
            if (prev.degree && *prev.degree == Degree::VII && *fromDegree == Degree::VI
                && *toDegree == Degree::V)
            {
                offset -= 0.07f;
                tag = "andalusian";
            }
        }

        // --- Pop loop continuation: I–V–vi–IV ---
        if (fromDegree && toDegree && sequence.size() >= 1)
        {
            const auto& prev = sequence.previous.back();
            if (prev.degree && *prev.degree == Degree::I && *fromDegree == Degree::V
                && *toDegree == Degree::VI)
            {
                offset -= 0.05f;
                if (tag == nullptr)
                    tag = "pop loop";
            }
            if (prev.degree && *prev.degree == Degree::V && *fromDegree == Degree::VI
                && *toDegree == Degree::IV)
            {
                offset -= 0.05f;
                if (tag == nullptr)
                    tag = "pop loop";
            }
        }

        // --- Blues-ish: after I material, IV is extra welcome; after IV, V ---
        if (fromDegree && toDegree)
        {
            if (*fromDegree == Degree::I && *toDegree == Degree::IV && sequence.size() >= 1)
                offset -= 0.02f;
            if (*fromDegree == Degree::IV && *toDegree == Degree::V)
                offset -= 0.025f;
        }

        (void) rootFrom;
        out.offset = offset;
        out.tag = tag;
        return out;
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

int NextChordScorer::keyTonicPitchClass(const KeyScaleData& keyScale)
{
    if (!keyScale.scaleNotes.empty())
        return keyScale.scaleNotes.front().getPitchClass();

    switch (keyScale.key)
    {
        case Key::C:  return 0;
        case Key::Db: return 1;
        case Key::D:  return 2;
        case Key::Eb: return 3;
        case Key::E:  return 4;
        case Key::F:  return 5;
        case Key::Gb: return 6;
        case Key::G:  return 7;
        case Key::Ab: return 8;
        case Key::A:  return 9;
        case Key::Bb: return 10;
        case Key::B:  return 11;
    }
    return 0;
}

std::optional<Degree> NextChordScorer::degreeOfRoot(int root, const KeyScaleData& keyScale)
{
    root = mod12(root);
    for (std::size_t i = 0; i < keyScale.scaleNotes.size() && i < 7; ++i)
    {
        if (keyScale.scaleNotes[i].getPitchClass() == root)
            return degreeFromIndex(static_cast<int>(i));
    }
    return std::nullopt;
}

bool NextChordScorer::isDominantLike(TriadQuality quality)
{
    return quality == TriadQuality::Dominant7
        || quality == TriadQuality::Major // bare V triad treated as dominant-capable
        || quality == TriadQuality::Sus4;
}

bool NextChordScorer::isMajorishQuality(TriadQuality quality)
{
    return quality == TriadQuality::Major
        || quality == TriadQuality::Major7
        || quality == TriadQuality::Dominant7
        || quality == TriadQuality::Augmented;
}

bool NextChordScorer::isMinorishQuality(TriadQuality quality)
{
    return quality == TriadQuality::Minor
        || quality == TriadQuality::Minor7
        || quality == TriadQuality::Diminished
        || quality == TriadQuality::HalfDim7;
}

NextChordScorer::HarmonicFamilyKind NextChordScorer::familyKindForQuality(TriadQuality quality)
{
    switch (quality)
    {
        case TriadQuality::Major:
        case TriadQuality::Major7:
        case TriadQuality::Power:
            return HarmonicFamilyKind::MajorColour;
        case TriadQuality::Minor:
        case TriadQuality::Minor7:
            return HarmonicFamilyKind::MinorColour;
        case TriadQuality::Dominant7:
            return HarmonicFamilyKind::Dominant;
        case TriadQuality::Diminished:
        case TriadQuality::HalfDim7:
            return HarmonicFamilyKind::Diminished;
        case TriadQuality::Augmented:
            return HarmonicFamilyKind::Augmented;
        case TriadQuality::Sus2:
        case TriadQuality::Sus4:
            return HarmonicFamilyKind::Sus;
    }
    return HarmonicFamilyKind::MajorColour;
}

float NextChordScorer::voicingComplexity(const Chord& chord)
{
    if (chord.notes.empty())
        return 0.0f;

    const int root = rootPitchClass(chord);
    const int bass = bassPitchClass(chord);
    const auto q = detectTriadQuality(chord);

    float c = 0.0f;
    // Inversion / slash bass is specificity unless justified later in ranking.
    if (bass != root)
        c += 0.30f;

    switch (q)
    {
        case TriadQuality::Major:
        case TriadQuality::Minor:
            break; // simplest
        case TriadQuality::Power:
            c += 0.14f; // incomplete triad
            break;
        case TriadQuality::Sus2:
        case TriadQuality::Sus4:
            c += 0.12f;
            break;
        case TriadQuality::Major7:
        case TriadQuality::Minor7:
            c += 0.18f;
            break;
        case TriadQuality::Dominant7:
            c += 0.04f; // distinct harmonic idea (V7), not needless colour on a triad
            break;
        case TriadQuality::HalfDim7:
            c += 0.20f;
            break;
        case TriadQuality::Diminished:
        case TriadQuality::Augmented:
            c += 0.08f;
            break;
    }

    if (chord.notes.size() > 3)
        c += 0.05f * static_cast<float>(chord.notes.size() - 3);

    return std::clamp(c, 0.0f, 1.0f);
}

float NextChordScorer::targetTensionFromDrama(float drama01)
{
    // Smooth ~0.12–0.28, Mild ~0.28–0.42, Dramatic ~0.42–0.68, Wild ~0.68–0.88
    drama01 = std::clamp(drama01, 0.0f, 1.0f);
    return 0.14f + 0.72f * drama01;
}

NextChordScorer::HarmonicRole NextChordScorer::roleFor(Degree degree, TriadQuality quality, ScaleFamily family)
{
    if (isDominantLike(quality) && (degree == Degree::V || degree == Degree::VII))
        return HarmonicRole::Dominant;

    if (family == ScaleFamily::Minorish)
    {
        switch (degree)
        {
            case Degree::I: case Degree::III: case Degree::VI:
                return HarmonicRole::Tonic;
            case Degree::II: case Degree::IV:
                return HarmonicRole::Predominant;
            case Degree::V: case Degree::VII:
                return HarmonicRole::Dominant;
        }
    }

    // Major / modal default functional map
    switch (degree)
    {
        case Degree::I: case Degree::VI: case Degree::III:
            return HarmonicRole::Tonic;
        case Degree::II: case Degree::IV:
            return HarmonicRole::Predominant;
        case Degree::V: case Degree::VII:
            return HarmonicRole::Dominant;
    }
    return HarmonicRole::Chromatic;
}

int NextChordScorer::rootPitchClass(const Chord& chord)
{
    if (chord.notes.empty())
        return 0;

    // Inversions store notes bass-first but keep positionInChord roles (1 = root). Prefer that so
    // C/E reports root C, not bass E. Fall back to front for reconstructed/incomplete chords.
    for (const auto& note : chord.notes)
    {
        if (note.positionInChord == 1)
            return note.getPitchClass();
    }

    return chord.notes.front().getPitchClass();
}

int NextChordScorer::bassPitchClass(const Chord& chord)
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

int NextChordScorer::directedRootInterval(int fromRoot, int toRoot)
{
    return mod12(toRoot - fromRoot);
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
    if (from.notes.empty() || to.notes.empty())
        return 1.0f;

    // Bass motion is first-class: closer bass steps (including common-tone pedals and
    // stepwise inversions) rank smoother than leaps. Upper voices use pitch-class
    // matching so pure inversions of the same harmony aren't punished for register shifts
    // the way absolute-MIDI pairing would be.
    const float bassCost = static_cast<float>(
        pitchClassDistance(bassPitchClass(from), bassPitchClass(to)));

    std::vector<int> fromPcs;
    std::vector<int> toPcs;
    fromPcs.reserve(from.notes.size());
    toPcs.reserve(to.notes.size());
    for (const auto& note : from.notes)
        fromPcs.push_back(note.getPitchClass());
    for (const auto& note : to.notes)
        toPcs.push_back(note.getPitchClass());

    // Match non-bass tones after removing one instance of each bass (already paid above).
    const auto eraseOne = [](std::vector<int>& pcs, int value)
    {
        const auto it = std::find(pcs.begin(), pcs.end(), value);
        if (it != pcs.end())
            pcs.erase(it);
    };
    eraseOne(fromPcs, bassPitchClass(from));
    eraseOne(toPcs, bassPitchClass(to));

    auto& src = fromPcs;
    auto& dst = toPcs;
    if (src.size() > dst.size())
        std::swap(src, dst);

    float upperTotal = 0.0f;
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
            upperTotal += best;
        }
        else
        {
            upperTotal += 3.0f;
        }
    }
    for (const bool wasUsed : used)
    {
        if (!wasUsed)
            upperTotal += 2.0f; // added chord tone
    }

    // Bass weighted equal to one upper voice; upper motion averaged per remaining voice.
    const float upperDenom = 6.0f * static_cast<float>(std::max<std::size_t>(src.size(), 1));
    const float upper01 = src.empty() ? 0.0f : std::clamp(upperTotal / upperDenom, 0.0f, 1.0f);
    const float bass01 = bassCost / 6.0f;

    // Slightly prefer bass smoothness (inversions / stepwise bass lines).
    return std::clamp(0.55f * bass01 + 0.45f * upper01, 0.0f, 1.0f);
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
                            float drama01, const SequenceContext& sequence)
{
    drama01 = std::clamp(drama01, 0.0f, 1.0f);

    const bool toDiatonic = isDiatonicChord(candidate.chord, keyScale);
    const bool fromDiatonic = isDiatonicChord(currentChord, keyScale);

    const int rootFrom = rootPitchClass(currentChord);
    const int rootTo = rootPitchClass(candidate.chord);
    const int bassFrom = bassPitchClass(currentChord);
    const int bassTo = bassPitchClass(candidate.chord);
    const int rootMin = pitchClassDistance(rootFrom, rootTo);
    const int rootDir = directedRootInterval(rootFrom, rootTo);
    const float fifthsTension = static_cast<float>(circleOfFifthsDistance(rootFrom, rootTo)) / 6.0f;
    const float vlTension = voiceLeadingCost(currentChord, candidate.chord);

    const auto fromPcs = pitchClassSet(currentChord);
    const auto toPcs = pitchClassSet(candidate.chord);
    const bool pureInversionChange = fromPcs == toPcs && bassFrom != bassTo;

    const int outside = nonScaleToneCount(candidate.chord, keyScale);
    const float noteCount = std::max(1.0f, static_cast<float>(candidate.chord.notes.size()));
    const float chromaticism = std::clamp(static_cast<float>(outside) / noteCount, 0.0f, 1.0f);

    const auto fromQuality = detectTriadQuality(currentChord);
    const auto toQuality = detectTriadQuality(candidate.chord);

    const auto family = scaleFamily(keyScale.scale);
    const int tonic = keyTonicPitchClass(keyScale);

    std::optional<Degree> toDegree = candidate.degree;
    if (!toDegree)
        toDegree = degreeOfRoot(rootTo, keyScale);

    std::optional<Degree> fromDegree = degreeOfRoot(rootFrom, keyScale);

    float functional = 0.0f;
    if (toDiatonic && toDegree)
        functional = functionalOffset(*toDegree, family);
    else if (toDiatonic)
        functional = -0.04f;

    float fitness = 0.0f;
    if (toDegree)
    {
        // Always evaluate quality-vs-degree fit when a degree is claimed; wrong quality
        // (D major as "ii") must lose to expected colour (Dm).
        fitness = qualityDegreeFitness(*toDegree, toQuality, family);
        if (toDiatonic)
            fitness += seventhColourBonus(*toDegree, toQuality, family);
        else
            fitness += 0.06f; // non-diatonic claiming a degree is slightly worse than base
    }

    float grammar = 0.0f;
    const char* grammarTag = nullptr;
    // Degree grammar only when the *target sonority* is actually diatonic — a forced degree
    // pin on a chromatic chord (e.g. D major labeled II) must not inherit ii-grammar credit.
    if (fromDegree && toDegree && toDiatonic && (fromDiatonic || toDiatonic))
    {
        grammar = progressionGrammar(*fromDegree, *toDegree, family);
        if (*fromDegree == Degree::V && *toDegree == Degree::I)
            grammarTag = "cadence";
        else if (*fromDegree == Degree::V && *toDegree == Degree::VI)
            grammarTag = "deceptive";
        else if (*fromDegree == Degree::IV && *toDegree == Degree::I)
            grammarTag = "plagal";
        else if (*fromDegree == Degree::II && *toDegree == Degree::V)
            grammarTag = "ii–V";
        else if (*fromDegree == Degree::IV && *toDegree == Degree::V)
            grammarTag = "IV–V";
        else if (*fromDegree == Degree::VII && *toDegree == Degree::I)
            grammarTag = family == ScaleFamily::Minorish ? "bVII–i" : "vii–I";
    }

    HarmonicRole fromRole = HarmonicRole::Chromatic;
    HarmonicRole toRole = HarmonicRole::Chromatic;
    if (fromDegree)
        fromRole = roleFor(*fromDegree, fromQuality, family);
    if (toDegree)
        toRole = roleFor(*toDegree, toQuality, family);
    else if (!toDiatonic)
        toRole = HarmonicRole::Chromatic;

    if (toDegree && *toDegree == Degree::V && isDominantLike(toQuality))
        toRole = HarmonicRole::Dominant;
    if (fromDegree && *fromDegree == Degree::V && isDominantLike(fromQuality))
        fromRole = HarmonicRole::Dominant;

    const float roleBias = roleTransitionBias(fromRole, toRole);

    // Formal predicates for secondary / subV (confidence-gated labels with known targets).
    const auto secondaryPred = analyseSecondaryDominant(candidate.chord, keyScale);
    const auto secondary = detectSecondaryDominant(rootTo, toQuality, keyScale);
    float secondaryOffset = secondary.offset;
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
        secondaryOffset = std::min(secondaryOffset, -0.14f * secondaryPred.confidence);
    if (secondary.hit && fromRole == HarmonicRole::Tonic)
        secondaryOffset *= 1.20f;

    // Tritone-sub only via formal predicate with implied resolve target.
    const auto tritonePred = analyseTritoneSubstitution(candidate.chord, keyScale);
    const auto tritoneSub = detectTritoneSub(candidate.chord, keyScale);
    const bool tritoneOk = tritonePred.hit && tritonePred.confidence >= kMinLabelConfidence;

    const auto mixture = detectModeMixture(tonic, rootTo, toQuality, family, toDiatonic);
    const auto mixturePredEarly = analyseModeMixture(candidate.chord, keyScale);
    const bool mixtureOk = mixturePredEarly.hit && mixturePredEarly.confidence >= kMinLabelConfidence;
    const auto mediant = detectChromaticMediant(rootFrom, rootTo, fromQuality, toQuality,
                                                fromDiatonic, toDiatonic);
    const float tendency = tendencyToneBonus(currentChord, candidate.chord, tonic, family);

    const auto domResolve = dominantResolutionBias(rootFrom, fromQuality, rootTo, toQuality, tonic, keyScale);
    float resolveBias = domResolve.offset;
    const char* resolveTag = domResolve.tag;
    if (grammarTag != nullptr && resolveTag != nullptr
        && (std::string_view(resolveTag) == "resolve" || std::string_view(resolveTag) == "deceptive"))
    {
        resolveBias *= 0.35f;
        resolveTag = nullptr;
    }

    const bool toIsTonic = (toDegree && *toDegree == Degree::I) || rootTo == tonic;
    const auto backdoor = detectBackdoor(tonic, rootFrom, fromQuality, rootTo, toQuality, toIsTonic);
    const auto sus = detectSusMotion(rootFrom, fromQuality, rootTo, toQuality, tonic);
    const float fifthsChain = fallingFifthsBonus(fromDiatonic, toDiatonic, rootDir, grammar);
    const auto approach = (!toDiatonic) ? detectApproachChord(rootTo, toQuality, keyScale) : ApproachHit{};
    const auto seqBias = sequenceContextBias(currentChord, candidate, keyScale, sequence,
                                             fromDegree, toDegree, rootFrom, rootTo, rootDir);

    // --- Smoothness (transition only; never mixed into Tension) -------------------------
    const float voiceLeading = std::clamp(1.0f - vlTension, 0.0f, 1.0f);
    // Reward genuine stepwise bass; do not reward pedal/common bass as "Fit gaming".
    const float stepwiseBassBonus = (pitchClassDistance(bassFrom, bassTo) == 1) ? 0.12f : 0.0f;
    const float commonBass = (bassFrom == bassTo) ? 1.0f : 0.0f;

    // --- Complexity (unnecessary inversions / extensions) ------------------------------
    const float complexity = voicingComplexity(candidate.chord);

    // --- Tension: "how unresolved does the music feel after this chord?" ---------------
    // Intrinsic quality dissonance (independent of voice-leading and common tones).
    float intrinsic = 0.12f;
    switch (toQuality)
    {
        case TriadQuality::Major:      intrinsic = 0.12f; break;
        case TriadQuality::Minor:      intrinsic = 0.18f; break;
        case TriadQuality::Power:      intrinsic = 0.14f; break;
        case TriadQuality::Sus2:
        case TriadQuality::Sus4:       intrinsic = 0.26f; break;
        case TriadQuality::Major7:     intrinsic = 0.34f; break; // maj7 dissonance > triad
        case TriadQuality::Minor7:     intrinsic = 0.30f; break;
        case TriadQuality::Dominant7:  intrinsic = 0.52f; break; // tritone + tendency
        case TriadQuality::Diminished: intrinsic = 0.68f; break;
        case TriadQuality::HalfDim7:   intrinsic = 0.62f; break;
        case TriadQuality::Augmented:  intrinsic = 0.64f; break;
    }

    // Tonal distance from global tonic (not from current bass).
    const float tonalDistance = static_cast<float>(circleOfFifthsDistance(rootTo, tonic)) / 6.0f;

    // Functional tension: dominant / secondary / diminished roles.
    float functionalTension = 0.0f;
    if (toDegree && *toDegree == Degree::I && !isDominantLike(toQuality)
        && toQuality != TriadQuality::Diminished && toQuality != TriadQuality::Augmented)
        functionalTension -= 0.06f; // resting place
    if (toDegree && *toDegree == Degree::V)
        functionalTension += (toQuality == TriadQuality::Dominant7) ? 0.22f : 0.12f;
    if (toDegree && (*toDegree == Degree::II || *toDegree == Degree::IV))
        functionalTension += 0.04f;
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
    {
        // Dom7 secondaries are strongly unresolved; bare-major V/x less so.
        const float qScale = (toQuality == TriadQuality::Dominant7) ? 1.0f : 0.45f;
        functionalTension += 0.20f * qScale * secondaryPred.confidence;
    }
    if (tritoneOk)
        functionalTension += 0.18f * tritonePred.confidence;
    if (toQuality == TriadQuality::Diminished || toQuality == TriadQuality::HalfDim7
        || toQuality == TriadQuality::Augmented)
        functionalTension += 0.10f;

    // Unresolved tritone inside the sonority (dom7 / dim).
    float tritoneInstability = 0.0f;
    if (toQuality == TriadQuality::Dominant7 || toQuality == TriadQuality::HalfDim7
        || toQuality == TriadQuality::Diminished)
        tritoneInstability = 0.12f;

    // Altered / non-scale tones contribute to unresolved colour (feature, not the whole definition).
    const float altered = 0.14f * chromaticism;

    float tension = intrinsic
        + 0.10f * tonalDistance
        + functionalTension
        + tritoneInstability
        + altered;
    // Same-root quality change (C→Cm) is a real colour shift, not VL.
    if (rootMin == 0)
        tension += sameRootQualityChangePenalty(fromQuality, toQuality) * 0.35f;

    tension = std::clamp(tension, 0.04f, 0.95f);

    // --- Resolution potential (local functional pull; lookahead added by generator) ----
    float resolution = 0.0f;
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
        resolution = std::max(resolution, 0.58f + 0.32f * secondaryPred.confidence);
    if (tritoneOk)
        resolution = std::max(resolution, 0.52f + 0.25f * tritonePred.confidence);
    if (resolveBias < -0.03f)
        resolution = std::max(resolution, 0.45f);
    if (tendency < -0.02f)
        resolution = std::max(resolution, 0.35f);
    if (toQuality == TriadQuality::Dominant7)
        resolution = std::max(resolution, 0.48f);
    if (toDegree && *toDegree == Degree::V)
        resolution = std::max(resolution, 0.40f);
    resolution = std::clamp(resolution, 0.0f, 1.0f);

    // --- Fit / coherence: contextual harmonic plausibility (absolute scale) ------------
    // Negative theory affinity → better expected move. Do NOT auto-normalise best to 100.
    const float theory =
        functional * 0.85f
        + fitness
        + grammar * 1.30f
        + roleBias
        + secondaryOffset
        + (tritoneOk ? tritoneSub.offset : 0.0f)
        + mixture.offset
        + (mediant.hit ? mediant.offset * 0.55f : 0.0f) // de-emphasise bare mediant vs V/x
        + tendency
        + resolveBias
        + backdoor.offset
        + sus.offset
        + fifthsChain
        + approach.offset
        + seqBias.offset;

    // Map theory affinity into a headroom-preserving Fit band:
    // excellent ~88–96, strong ~75–87, reasonable ~60–74, unusual ~40–59.
    float coherence = 0.52f - 0.85f * theory;
    if (toDiatonic)
        coherence += 0.10f;
    if (grammar < -0.06f)
        coherence += 0.10f;
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
        coherence += 0.12f * secondaryPred.confidence;
    if (tritoneOk)
        coherence += 0.08f * tritonePred.confidence;
    if (seqBias.offset < -0.02f)
        coherence += 0.06f;
    // Small VL contribution — never dominant, never a path to Fit=100 via pedal bass.
    coherence += 0.05f * voiceLeading;
    coherence += 0.04f * stepwiseBassBonus;
    // Pedal/common bass alone must not dominate Fit (was gaming F/C, Am/C, etc.).
    coherence += 0.01f * commonBass;
    // Idiomatic mixture (bVI/bVII/iv…) is functional colour, not random chroma.
    if (mixtureOk)
        coherence += 0.10f * mixturePredEarly.confidence;
    // Random chromatic without function is weak Fit (must lose to diatonic / secondary / mixture).
    if (!toDiatonic && !secondaryPred.hit && !tritoneOk && !mixtureOk && approach.tag == nullptr)
        coherence -= 0.24f;
    else if (!toDiatonic && theory > -0.02f && !secondaryPred.hit && !tritoneOk && !mixtureOk)
        coherence -= 0.12f;
    // Complexity is not Fit, but gross over-specification slightly weakens perceived coherence.
    coherence -= 0.04f * complexity;
    // Absolute ceiling: ordinary moves should not all pin at 100.
    coherence = std::clamp(coherence, 0.05f, 0.96f);

    // --- Surprise (unexpectedness; separate from tension) ------------------------------
    float surprise = 0.30f * chromaticism
        + 0.22f * fifthsTension
        + 0.22f * (toDiatonic ? 0.0f : 0.55f)
        + 0.26f * std::clamp(0.12f + theory, 0.0f, 1.0f);
    if (grammar < -0.08f)
        surprise *= 0.50f;
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
        surprise *= 0.72f; // secondary is colourful but expected as a device
    surprise = std::clamp(surprise, 0.0f, 1.0f);

    auto& m = candidate.metrics;
    m.coherence = coherence;
    m.tension = tension;
    m.surprise = surprise;
    m.voiceLeading = voiceLeading;
    m.resolution = resolution;
    m.complexity = complexity;

    constexpr float kMid = 0.38f;
    if (tension < kMid - 0.08f)
        m.tensionDirection = CandidateMetrics::TensionDirection::Release;
    else if (tension > kMid + 0.12f)
        m.tensionDirection = CandidateMetrics::TensionDirection::Increase;
    else
        m.tensionDirection = CandidateMetrics::TensionDirection::Maintain;

    candidate.fitPercent = std::clamp(static_cast<int>(std::lround(coherence * 100.0f)), 0, 100);
    candidate.tensionPercent = std::clamp(static_cast<int>(std::lround(tension * 100.0f)), 1, 100);
    candidate.surprisePercent = std::clamp(static_cast<int>(std::lround(surprise * 100.0f)), 0, 100);
    candidate.smoothnessPercent = std::clamp(static_cast<int>(std::lround(voiceLeading * 100.0f)), 0, 100);
    candidate.resolutionPercent = std::clamp(static_cast<int>(std::lround(resolution * 100.0f)), 0, 100);

    // Drama → target tension region. Coherence is primary; tension match is secondary
    // (must not promote random chromatic over idiomatic moves just because T matches).
    const float Tstar = targetTensionFromDrama(drama01);
    const float tensionDistance = std::abs(tension - Tstar);
    // Soft band match: full credit near target; wide falloff.
    const float tensionMatch = 1.0f - std::clamp(tensionDistance / 0.55f, 0.0f, 1.0f);

    candidate.rankingScore =
        1.65f * coherence
        + 0.48f * tensionMatch
        + 0.38f * resolution
        + 0.05f * voiceLeading
        + 0.03f * stepwiseBassBonus // genuine bass step — small; never dominate function
        - 0.42f * complexity // simplicity prior: ordinary F beats F/C / Fmaj7/C by default
        - 0.20f * surprise * (1.0f - drama01)
        + 0.14f * surprise * drama01;

    // Slight demotion for pure same-harmony inversion unless bass is stepwise (already in VL).
    if (pureInversionChange)
        candidate.rankingScore -= 0.06f;
    // Pedal-bass inversions that only keep common tones: extra simplicity demotion.
    if (bassTo != rootTo && commonBass > 0.5f && !secondaryPred.hit)
        candidate.rankingScore -= 0.10f;

    // Secondary target preference: V/V and V/ii outrank V/iii as next chords from I.
    // Bare-major "V/x" is a weaker claim than a true dominant seventh.
    if (secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence)
    {
        const float qualityScale = (toQuality == TriadQuality::Dominant7) ? 1.0f : 0.30f;
        candidate.rankingScore += 0.55f * qualityScale
            * secondaryTargetWeight(secondaryPred.targetDegree)
            * secondaryPred.confidence;
        // From a tonic start, V/iii is a weaker next move than V/V or V/ii.
        if (fromRole == HarmonicRole::Tonic && secondaryPred.targetDegree == Degree::III)
            candidate.rankingScore -= 0.18f;
    }
    if (tritoneOk)
        candidate.rankingScore += 0.12f * tritonePred.confidence;

    // Family tags for generator diversity.
    candidate.familyRootPc = rootTo;
    candidate.familyKind = static_cast<int>(familyKindForQuality(toQuality));

    // --- Reason label: strongest functional explanation with target when known ---------
    std::ostringstream reason;
    const bool secondaryOk = secondaryPred.hit && secondaryPred.confidence >= kMinLabelConfidence;

    if (secondaryOk)
        reason << secondaryPred.label; // e.g. V/ii
    else if (tritoneOk)
        reason << (tritonePred.label.empty() ? "subV" : tritonePred.label);
    else if (mixtureOk)
        reason << mixturePredEarly.label;
    else if (candidate.degree)
        reason << getDegreeLabel(*candidate.degree);
    else if (toDegree && toDiatonic)
        reason << getDegreeLabel(*toDegree);
    else if (toDiatonic)
        reason << "diatonic";
    else if (approach.tag)
        reason << approach.tag;
    else
        reason << "chromatic";

    if (grammarTag)
        reason << " · " << grammarTag;
    else if (resolveTag)
        reason << " · " << resolveTag;
    else if (backdoor.tag)
        reason << " · " << backdoor.tag;
    else if (sus.tag)
        reason << " · " << sus.tag;

    // Only attach mediant when no stronger functional claim was used.
    if (!secondaryOk && !tritoneOk && mediant.hit && mediant.tag)
        reason << " · " << mediant.tag;

    if (fifthsChain <= -0.03f && grammarTag == nullptr && resolveTag == nullptr
        && (seqBias.tag == nullptr || std::string_view(seqBias.tag) != "5ths chain"))
        reason << " · 5ths";

    if (seqBias.tag != nullptr)
        reason << " · " << seqBias.tag;

    // Inversion annotation only when the representative is not root position.
    if (bassTo != rootTo && !candidate.chord.notes.empty())
        reason << " · /" << candidate.chord.notes.front().readableNote;

    if (const char* tag = qualityTag(toQuality))
        reason << " · " << tag;

    candidate.reasonLabel = reason.str();

    if (reasonContainsInvalidTheoryClaim(candidate.reasonLabel, candidate.chord, keyScale))
    {
        std::ostringstream safe;
        if (candidate.degree)
            safe << getDegreeLabel(*candidate.degree);
        else if (toDiatonic)
            safe << "diatonic";
        else
            safe << "chromatic";
        if (bassTo != rootTo)
            safe << " · inversion";
        candidate.reasonLabel = safe.str();
    }
}

void NextChordScorer::scoreAndSort(const Chord& currentChord, const KeyScaleData& keyScale,
                                   std::vector<NextChordCandidate>& candidates, float drama01,
                                   const SequenceContext& sequence)
{
    drama01 = std::clamp(drama01, 0.0f, 1.0f);

    for (auto& candidate : candidates)
        score(currentChord, keyScale, candidate, drama01, sequence);

    // Minimum-coherence gate: wild still requires musical sense.
    const float minCoh = lerp(kMinCoherenceSmooth, kMinCoherenceWild, drama01);
    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                         [minCoh](const NextChordCandidate& c) {
                             return c.metrics.coherence < minCoh;
                         }),
                     candidates.end());

    if (candidates.empty())
        return;

    // Higher rankingScore = better match for the desired tension band.
    std::stable_sort(candidates.begin(), candidates.end(),
        [](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            if (std::abs(a.rankingScore - b.rankingScore) > 1.0e-5f)
                return a.rankingScore > b.rankingScore;
            if (a.fitPercent != b.fitPercent)
                return a.fitPercent > b.fitPercent;
            if (std::abs(a.metrics.complexity - b.metrics.complexity) > 1.0e-5f)
                return a.metrics.complexity < b.metrics.complexity;
            return a.chord.symbol < b.chord.symbol;
        });
}

}
