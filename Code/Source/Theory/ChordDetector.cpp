#include "Theory/ChordDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

#include "Theory/Chord.h"
#include "Theory/ChordDatabase.h"
#include "Theory/NextChordScorer.h"
#include "Theory/NoteName.h"

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

    bool hasInterval(const std::array<bool, 12>& pcs, int root, int interval)
    {
        return pcs[static_cast<std::size_t>(mod12(root + interval))];
    }

    struct QualityTemplate
    {
        const char* label = "";
        int required[8] {};
        int requiredCount = 0;
        int optional[6] {};
        int optionalCount = 0;
        int forbidden[4] {};
        int forbiddenCount = 0;
        int specificity = 0;
        bool hasLibraryQuality = false;
        TriadQuality libraryQuality = TriadQuality::Major;
        bool allowMissingRoot = false;
    };

    QualityTemplate makeTemplate(const char* label,
                                 std::initializer_list<int> required,
                                 std::initializer_list<int> optional,
                                 std::initializer_list<int> forbidden,
                                 int specificity,
                                 bool hasLib = false,
                                 TriadQuality libQ = TriadQuality::Major,
                                 bool allowMissingRoot = false)
    {
        QualityTemplate t;
        t.label = label;
        for (int i : required)
            if (t.requiredCount < 8)
                t.required[t.requiredCount++] = i;
        for (int i : optional)
            if (t.optionalCount < 6)
                t.optional[t.optionalCount++] = i;
        for (int i : forbidden)
            if (t.forbiddenCount < 4)
                t.forbidden[t.forbiddenCount++] = i;
        t.specificity = specificity;
        t.hasLibraryQuality = hasLib;
        t.libraryQuality = libQ;
        t.allowMissingRoot = allowMissingRoot;
        return t;
    }

    const std::vector<QualityTemplate>& catalogue()
    {
        static const std::vector<QualityTemplate> kCatalogue = {
            // ---- rich extensions ----
            makeTemplate("maj13#11", { 4, 11, 9, 2, 6 }, { 7, 5 }, {}, 140),
            makeTemplate("maj13", { 4, 11, 9, 2 }, { 7, 5 }, { 3 }, 130),
            makeTemplate("m13", { 3, 10, 9, 2 }, { 7, 5 }, { 4 }, 128),
            makeTemplate("13#11", { 4, 10, 9, 2, 6 }, { 7, 5 }, {}, 126),
            makeTemplate("13sus4", { 5, 10, 9, 2 }, { 7 }, { 3, 4 }, 125),
            makeTemplate("13", { 4, 10, 9, 2 }, { 7, 5 }, { 3 }, 124),
            makeTemplate("maj11", { 4, 11, 2, 5 }, { 7 }, { 3 }, 118),
            makeTemplate("m11", { 3, 10, 2, 5 }, { 7 }, { 4 }, 116),
            makeTemplate("11", { 4, 10, 2, 5 }, { 7 }, { 3 }, 114),
            makeTemplate("maj9#11", { 4, 11, 2, 6 }, { 7 }, {}, 112),
            makeTemplate("9#11", { 4, 10, 2, 6 }, { 7 }, {}, 110),
            makeTemplate("maj9", { 4, 11, 2 }, { 7 }, { 3 }, 108),
            makeTemplate("m9", { 3, 10, 2 }, { 7 }, { 4 }, 106),
            makeTemplate("9", { 4, 10, 2 }, { 7 }, { 3 }, 104),
            makeTemplate("9sus4", { 5, 10, 2 }, { 7 }, { 3, 4 }, 102),
            makeTemplate("m(maj9)", { 3, 11, 2 }, { 7 }, { 4 }, 100),

            // ---- altered / colour dominants ----
            makeTemplate("7alt", { 4, 10, 1, 3, 8 }, {}, {}, 98),
            makeTemplate("7#9#5", { 4, 10, 3, 8 }, {}, {}, 96),
            makeTemplate("7#9b13", { 4, 10, 3, 8 }, { 7 }, {}, 95),
            makeTemplate("7#9", { 4, 10, 3 }, { 7 }, {}, 94),
            makeTemplate("7b9", { 4, 10, 1 }, { 7 }, {}, 92),
            // 7#11 keeps natural 5; pure tritone-sub colour without 5 is 7b5.
            makeTemplate("7#11", { 4, 10, 6, 7 }, {}, {}, 90),
            makeTemplate("7#5", { 4, 10, 8 }, {}, { 7 }, 88),
            makeTemplate("7b13", { 4, 10, 8 }, { 7 }, {}, 86),
            makeTemplate("7b5", { 4, 10, 6 }, {}, { 7 }, 85),
            makeTemplate("maj7#11", { 4, 11, 6 }, { 7 }, {}, 84),
            makeTemplate("maj7#5", { 4, 8, 11 }, {}, {}, 82),
            makeTemplate("maj7b5", { 4, 6, 11 }, {}, { 7 }, 81),
            makeTemplate("m7b9b13", { 3, 10, 1, 8 }, { 7, 5 }, { 4 }, 80),
            makeTemplate("m7b9add11", { 3, 10, 1, 5 }, { 7 }, { 4 }, 78),
            makeTemplate("m7b9", { 3, 10, 1 }, { 7 }, { 4 }, 76),
            makeTemplate("m7b13", { 3, 10, 8 }, { 7 }, { 4 }, 74),
            makeTemplate("m9#11", { 3, 10, 2, 6 }, { 7 }, { 4 }, 72),
            makeTemplate("m7#5", { 3, 10, 8 }, {}, { 4, 7 }, 71),

            // ---- sevenths ----
            makeTemplate("maj7", { 4, 11 }, { 7 }, { 3, 10 }, 70, true, TriadQuality::Major7),
            makeTemplate("m(maj7)", { 3, 11 }, { 7 }, { 4, 10 }, 68),
            makeTemplate("7", { 4, 10 }, { 7 }, { 3, 11 }, 66, true, TriadQuality::Dominant7),
            makeTemplate("m7", { 3, 10 }, { 7 }, { 4, 11 }, 64, true, TriadQuality::Minor7),
            makeTemplate("m7b5", { 3, 6, 10 }, {}, { 4, 7 }, 62, true, TriadQuality::HalfDim7),
            makeTemplate("dim7", { 3, 6, 9 }, {}, { 4, 7 }, 60),
            makeTemplate("7sus4", { 5, 10 }, { 7 }, { 3, 4 }, 58),
            makeTemplate("7sus2", { 2, 10 }, { 7 }, { 3, 4 }, 56),
            makeTemplate("maj7sus2", { 2, 11 }, { 7 }, { 3, 4 }, 54),
            makeTemplate("dim(maj7)", { 3, 6, 11 }, {}, { 4, 7 }, 52),
            makeTemplate("7(no3)", { 10 }, { 7 }, { 3, 4 }, 51),

            // ---- sixths / add ----
            makeTemplate("6/9", { 4, 9, 2 }, { 7 }, { 3, 10 }, 50),
            makeTemplate("m6/9", { 3, 9, 2 }, { 7 }, { 4, 10 }, 48),
            makeTemplate("6", { 4, 9 }, { 7 }, { 3, 10 }, 46),
            makeTemplate("m6", { 3, 9 }, { 7 }, { 4, 10 }, 44),
            makeTemplate("add9", { 4, 2 }, { 7 }, { 3, 10 }, 42),
            makeTemplate("m(add9)", { 3, 2 }, { 7 }, { 4, 10 }, 40),
            makeTemplate("add11", { 4, 5 }, { 7 }, { 3, 10 }, 38),
            makeTemplate("m(add11)", { 3, 5 }, { 7 }, { 4, 10 }, 36),
            makeTemplate("add#11", { 4, 6 }, { 7 }, { 3 }, 34),

            // ---- triads / sus / power ----
            makeTemplate("aug", { 4, 8 }, {}, { 3, 7 }, 32, true, TriadQuality::Augmented),
            makeTemplate("dim", { 3, 6 }, {}, { 4, 7 }, 30, true, TriadQuality::Diminished),
            makeTemplate("", { 4, 7 }, {}, { 3 }, 28, true, TriadQuality::Major),
            makeTemplate("m", { 3, 7 }, {}, { 4 }, 26, true, TriadQuality::Minor),
            makeTemplate("sus4", { 5, 7 }, {}, { 3, 4 }, 24, true, TriadQuality::Sus4),
            makeTemplate("sus2", { 2, 7 }, {}, { 3, 4 }, 22, true, TriadQuality::Sus2),
            makeTemplate("5", { 7 }, {}, { 3, 4 }, 18, true, TriadQuality::Power),

            // ---- rootless jazz shells ----
            makeTemplate("maj7", { 4, 11 }, { 7, 2 }, { 3, 10 }, 16, true, TriadQuality::Major7, true),
            makeTemplate("7", { 4, 10 }, { 7, 2 }, { 3, 11 }, 14, true, TriadQuality::Dominant7, true),
            makeTemplate("m7", { 3, 10 }, { 7, 2 }, { 4, 11 }, 12, true, TriadQuality::Minor7, true),
            makeTemplate("m7b5", { 3, 6, 10 }, { 2 }, { 4, 7 }, 10, true, TriadQuality::HalfDim7, true),
        };
        return kCatalogue;
    }

    bool isChordTone(int root, const QualityTemplate& tmpl, int pc)
    {
        if (mod12(pc) == mod12(root))
            return true;
        for (int i = 0; i < tmpl.requiredCount; ++i)
            if (mod12(root + tmpl.required[i]) == mod12(pc))
                return true;
        for (int i = 0; i < tmpl.optionalCount; ++i)
            if (mod12(root + tmpl.optional[i]) == mod12(pc))
                return true;
        return false;
    }

    bool hasForbidden(const std::array<bool, 12>& pcs, int root, const QualityTemplate& tmpl)
    {
        for (int i = 0; i < tmpl.forbiddenCount; ++i)
            if (hasInterval(pcs, root, tmpl.forbidden[i]))
                return true;
        return false;
    }

    int extraToneCount(const std::array<bool, 12>& pcs, int root, const QualityTemplate& tmpl)
    {
        auto extras = 0;
        for (int i = 0; i < 12; ++i)
            if (pcs[static_cast<std::size_t>(i)] && !isChordTone(root, tmpl, i))
                ++extras;
        return extras;
    }

    int optionalHits(const std::array<bool, 12>& pcs, int root, const QualityTemplate& tmpl)
    {
        auto hits = 0;
        for (int i = 0; i < tmpl.optionalCount; ++i)
            if (hasInterval(pcs, root, tmpl.optional[i]))
                ++hits;
        return hits;
    }

    int chordToneHits(const std::array<bool, 12>& pcs, int root, const QualityTemplate& tmpl)
    {
        auto hits = 0;
        for (int i = 0; i < 12; ++i)
            if (pcs[static_cast<std::size_t>(i)] && isChordTone(root, tmpl, i))
                ++hits;
        return hits;
    }

    std::string formatName(int root, int bass, const QualityTemplate& tmpl, Key spellKey)
    {
        std::string name = std::string(pcName(root, spellKey)) + tmpl.label;
        if (mod12(bass) != mod12(root))
        {
            name += "/";
            name += pcName(bass, spellKey);
        }
        return name;
    }

    std::optional<std::string> nameDyad(int lowPc, int highPc, Key spellKey)
    {
        const int interval = mod12(highPc - lowPc);
        const auto* a = pcName(lowPc, spellKey);
        const auto* b = pcName(highPc, spellKey);
        switch (interval)
        {
            case 1: return std::string(a) + "–" + b + " (m2)";
            case 2: return std::string(a) + "–" + b + " (M2)";
            case 3: return std::string(a) + "m (no 5)";
            case 4: return std::string(a) + " (no 5)";
            case 5: return std::string(a) + "sus4 (no 5)";
            case 6: return std::string(a) + "–" + b + " (TT)";
            case 7: return std::string(a) + "5";
            case 8: return std::string(a) + "aug (no 3)";
            case 9: return std::string(a) + "6 (no 3)";
            case 10: return std::string(a) + "7 (no 3)";
            case 11: return std::string(a) + "maj7 (no 3)";
            default: break;
        }
        return std::nullopt;
    }

    struct Candidate
    {
        int root = 0;
        const QualityTemplate* tmpl = nullptr;
        int score = 0;
        int requiredMatched = 0;
        int extras = 0;
        int coverage = 0;
        bool rootInBass = false;
        bool rootMissing = false;
        std::string name;
    };

    bool betterCandidate(const Candidate& a, const Candidate& b)
    {
        if (a.score != b.score)
            return a.score > b.score;
        if (a.coverage != b.coverage)
            return a.coverage > b.coverage;
        if (a.requiredMatched != b.requiredMatched)
            return a.requiredMatched > b.requiredMatched;
        if (a.extras != b.extras)
            return a.extras < b.extras;
        if (a.rootMissing != b.rootMissing)
            return !a.rootMissing && b.rootMissing;
        return a.name < b.name;
    }

    void consider(std::optional<Candidate>& slot, const Candidate& cand)
    {
        if (!slot.has_value() || betterCandidate(cand, *slot))
            slot = cand;
    }

    std::optional<Candidate> evaluateRoot(const std::array<bool, 12>& pcs,
                                          int root,
                                          int bassPitchClass,
                                          int nHeld,
                                          bool requireRootHeld,
                                          Key spellKey)
    {
        std::optional<Candidate> best;
        const bool rootHeld = pcs[static_cast<std::size_t>(mod12(root))];

        for (const auto& tmpl : catalogue())
        {
            if (requireRootHeld && !rootHeld)
                continue;
            if (!requireRootHeld && rootHeld)
                continue;
            if (!requireRootHeld && !tmpl.allowMissingRoot)
                continue;
            if (!requireRootHeld && nHeld < 3)
                continue;

            if (hasForbidden(pcs, root, tmpl))
                continue;

            auto allRequired = true;
            for (int i = 0; i < tmpl.requiredCount; ++i)
            {
                if (!hasInterval(pcs, root, tmpl.required[i]))
                {
                    allRequired = false;
                    break;
                }
            }
            if (!allRequired)
                continue;

            const int requiredMatched = tmpl.requiredCount + (rootHeld ? 1 : 0);
            const int extras = extraToneCount(pcs, root, tmpl);
            if (extras > 2)
                continue;

            const int coverage = chordToneHits(pcs, root, tmpl);
            if (nHeld >= 3 && coverage < nHeld - 1 && extras > 0)
                continue;

            const int optHits = optionalHits(pcs, root, tmpl);
            const bool bassInChord = isChordTone(root, tmpl, bassPitchClass);
            const bool rootInBass = mod12(bassPitchClass) == mod12(root);

            if (!rootHeld && !bassInChord)
                continue;

            int score = requiredMatched * 25
                + tmpl.specificity
                + optHits * 5
                + coverage * 6
                - extras * 16;
            if (extras == 0)
                score += 24;
            if (bassInChord)
                score += 8;
            if (rootHeld)
                score += 12;
            else
                score -= 18;

            score += static_cast<int>(std::lround(40.0 * coverage / std::max(1, nHeld)));

            Candidate cand {
                root, &tmpl, score, requiredMatched, extras, coverage, rootInBass, !rootHeld,
                formatName(root, bassPitchClass, tmpl, spellKey)
            };
            consider(best, cand);
        }
        return best;
    }

    Chord buildChordForRoman(const Candidate& cand, Key spellKey, int bassPitchClass)
    {
        if (cand.tmpl != nullptr && cand.tmpl->hasLibraryQuality)
        {
            int inv = 0;
            if (mod12(bassPitchClass) != mod12(cand.root))
            {
                const auto intervals = TriadLibrary::qualityIntervals(cand.tmpl->libraryQuality);
                for (std::size_t i = 0; i < intervals.size(); ++i)
                {
                    if (mod12(cand.root + intervals[i]) == mod12(bassPitchClass))
                    {
                        inv = static_cast<int>(i) + 1;
                        break;
                    }
                }
                if (inv == 0)
                    inv = 1;
            }
            return TriadLibrary::makeTriad(cand.root, cand.tmpl->libraryQuality, spellKey, inv);
        }

        // Synthetic chord: bass-first notes with root tagged positionInChord=1.
        Chord chord;
        chord.readableName = cand.name;
        chord.symbol = cand.name;
        auto push = [&](int pc, int role)
        {
            const char* n = pcName(pc, spellKey);
            chord.notes.push_back(NoteName { n, n, role });
        };

        // Bass first if different from root.
        if (mod12(bassPitchClass) != mod12(cand.root))
            push(bassPitchClass, 5);
        push(cand.root, 1);
        if (cand.tmpl != nullptr)
        {
            for (int i = 0; i < cand.tmpl->requiredCount; ++i)
            {
                const int pc = mod12(cand.root + cand.tmpl->required[i]);
                if (pc == mod12(bassPitchClass) || pc == mod12(cand.root))
                    continue;
                const int iv = cand.tmpl->required[i];
                int role = 3;
                if (iv == 7 || iv == 6 || iv == 8)
                    role = 5;
                else if (iv == 10 || iv == 11 || iv == 9)
                    role = 7;
                else if (iv == 2 || iv == 1)
                    role = 9;
                else if (iv == 5)
                    role = 4;
                push(pc, role);
            }
        }
        return chord;
    }

    void fillRoman(ChordDetection& result, const Candidate& cand, Key spellKey,
                   std::optional<Scale> scale, int bassPitchClass)
    {
        if (!scale.has_value())
            return;

        const auto& keyScale = ChordDatabase::getInstance().get(spellKey, *scale);
        const Chord chord = buildChordForRoman(cand, spellKey, bassPitchClass);
        result.romanNumeral = NextChordScorer::romanForChord(chord, keyScale);
    }

    float confidenceFromScores(int bestScore, int secondScore, int nHeld)
    {
        if (nHeld <= 0)
            return 0.f;
        if (secondScore <= 0)
            return std::clamp(0.55f + static_cast<float>(nHeld) * 0.06f, 0.55f, 0.98f);

        const float margin = static_cast<float>(bestScore - secondScore);
        // Soft map: small margin → ~0.45, large margin → ~0.95
        const float c = 0.45f + 0.5f * (1.f - std::exp(-margin / 55.f));
        return std::clamp(c, 0.2f, 0.99f);
    }
}

ChordDetection ChordDetector::detect(const std::array<bool, 12>& pcs, int bassPitchClass, Key spellKey,
                                     std::optional<Scale> scale)
{
    ChordDetection result;
    const auto nHeld = heldCount(pcs);
    if (nHeld == 0)
        return result;

    bassPitchClass = mod12(bassPitchClass);

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
        result.qualityLabel = "note";
        result.confidence = 0.9f;
        return result;
    }

    std::optional<Candidate> bestRootPosition;
    std::optional<Candidate> bestSlash;
    std::optional<Candidate> bestOther;
    std::optional<Candidate> bestRootless;
    // Track global runner-up across buckets for alternateName / confidence.
    std::optional<Candidate> globalBest;
    std::optional<Candidate> globalSecond;

    const auto trackGlobal = [&](const Candidate& cand)
    {
        if (!globalBest.has_value() || betterCandidate(cand, *globalBest))
        {
            globalSecond = globalBest;
            globalBest = cand;
        }
        else if ((!globalSecond.has_value() || betterCandidate(cand, *globalSecond))
                 && cand.name != globalBest->name)
        {
            globalSecond = cand;
        }
    };

    for (int root = 0; root < 12; ++root)
    {
        if (!pcs[static_cast<std::size_t>(root)])
            continue;

        if (auto cand = evaluateRoot(pcs, root, bassPitchClass, nHeld, true, spellKey))
        {
            trackGlobal(*cand);
            if (cand->rootInBass)
                consider(bestRootPosition, *cand);
            else if (isChordTone(root, *cand->tmpl, bassPitchClass))
                consider(bestSlash, *cand);
            else
                consider(bestOther, *cand);
        }
    }

    const bool thinRooted = !bestRootPosition.has_value() && !bestSlash.has_value() && nHeld >= 3;
    if (thinRooted || (bestRootPosition.has_value() && bestRootPosition->extras > 0 && nHeld >= 4))
    {
        for (int root = 0; root < 12; ++root)
        {
            if (pcs[static_cast<std::size_t>(root)])
                continue;
            if (auto cand = evaluateRoot(pcs, root, bassPitchClass, nHeld, false, spellKey))
            {
                trackGlobal(*cand);
                consider(bestRootless, *cand);
            }
        }
    }

    constexpr int kRootPositionPreference = 80;
    std::optional<Candidate> best;

    if (bestRootPosition.has_value())
    {
        best = bestRootPosition;
        if (bestSlash.has_value()
            && bestSlash->score > bestRootPosition->score + kRootPositionPreference)
            best = bestSlash;
    }
    else if (bestSlash.has_value())
    {
        best = bestSlash;
    }
    else if (bestOther.has_value())
    {
        best = bestOther;
    }

    if (bestRootless.has_value())
    {
        if (!best.has_value() || bestRootless->score > best->score + 25)
            best = bestRootless;
    }

    if (!best.has_value() || best->tmpl == nullptr)
    {
        if (nHeld == 2)
        {
            int a = -1, b = -1;
            for (int i = 0; i < 12; ++i)
            {
                if (!pcs[static_cast<std::size_t>(i)])
                    continue;
                if (a < 0)
                    a = i;
                else
                    b = i;
            }
            const int low = bassPitchClass;
            const int high = (a == low) ? b : a;
            if (const auto dyad = nameDyad(low, high, spellKey))
            {
                result.matched = true;
                result.name = *dyad;
                result.rootPitchClass = low;
                result.bassPitchClass = bassPitchClass;
                result.toneCount = 2;
                result.qualityLabel = "dyad";
                result.confidence = 0.55f;
                return result;
            }
        }

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
        result.qualityLabel = "cluster";
        result.confidence = 0.25f;
        return result;
    }

    const auto& tmpl = *best->tmpl;
    result.matched = true;
    result.name = best->name;
    result.rootPitchClass = best->root;
    result.bassPitchClass = bassPitchClass;
    result.toneCount = best->requiredMatched;
    result.qualityLabel = tmpl.label[0] == '\0' ? "maj" : tmpl.label;
    result.hasLibraryQuality = tmpl.hasLibraryQuality;
    if (tmpl.hasLibraryQuality)
        result.quality = tmpl.libraryQuality;

    // Alternate: best differently-named candidate (often the enharmonic/inversion twin).
    if (globalSecond.has_value() && globalSecond->name != result.name)
        result.alternateName = globalSecond->name;
    else if (bestRootPosition.has_value() && bestSlash.has_value()
             && best->name == bestRootPosition->name
             && bestSlash->name != result.name)
        result.alternateName = bestSlash->name;

    const int secondScore = globalSecond.has_value() ? globalSecond->score : 0;
    result.confidence = confidenceFromScores(best->score, secondScore, nHeld);

    fillRoman(result, *best, spellKey, scale, bassPitchClass);
    return result;
}

ChordDetection ChordDetector::detectFromMidiNotes(const std::vector<int>& midiNotes, Key spellKey,
                                                  std::optional<Scale> scale)
{
    if (midiNotes.empty())
        return {};

    std::array<bool, 12> pcs {};
    pcs.fill(false);
    int lowestMidi = 128;
    auto any = false;
    for (const int note : midiNotes)
    {
        if (note < 0 || note > 127)
            continue;
        pcs[static_cast<std::size_t>(mod12(note))] = true;
        lowestMidi = std::min(lowestMidi, note);
        any = true;
    }

    if (!any || heldCount(pcs) == 0)
        return {};

    return detect(pcs, mod12(lowestMidi), spellKey, scale);
}

}
