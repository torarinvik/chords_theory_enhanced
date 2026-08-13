#include "Theory/ChordDetector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "Theory/Chord.h"
#include "Theory/ChordDatabase.h"
#include "Theory/HarmonicPredicates.h"
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
            makeTemplate("maj7(no3)", { 11 }, { 7 }, { 3, 4 }, 50),
            makeTemplate("m7(no3)", { 3, 10 }, {}, { 4, 7 }, 49), // shell without 5 already covered

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
                                          Key spellKey,
                                          const KeyScaleData* keyScale)
    {
        std::optional<Candidate> best;
        const bool rootHeld = pcs[static_cast<std::size_t>(mod12(root))];
        const bool rootDiatonic = keyScale != nullptr
            && NextChordScorer::degreeOfRoot(root, *keyScale).has_value();

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

            // Mild key-context bias: prefer diatonic roots when the user has a key/scale set.
            if (keyScale != nullptr)
                score += rootDiatonic ? 14 : -6;

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

    std::uint16_t pitchClassMask(const std::array<bool, 12>& pcs)
    {
        std::uint16_t mask = 0;
        for (int i = 0; i < 12; ++i)
            if (pcs[static_cast<std::size_t>(i)])
                mask = static_cast<std::uint16_t>(mask | (1u << i));
        return mask;
    }

    std::uint16_t chordPitchClassMask(const Chord& chord)
    {
        std::uint16_t mask = 0;
        for (const auto& n : chord.notes)
        {
            const int pc = mod12(n.getPitchClass());
            mask = static_cast<std::uint16_t>(mask | (1u << pc));
        }
        return mask;
    }

    int chordRootPc(const Chord& chord)
    {
        for (const auto& n : chord.notes)
            if (n.positionInChord == 1)
                return mod12(n.getPitchClass());
        return chord.notes.empty() ? 0 : mod12(chord.notes.front().getPitchClass());
    }

    int chordBassPc(const Chord& chord)
    {
        return chord.notes.empty() ? 0 : mod12(chord.notes.front().getPitchClass());
    }

    // Catalogue hit against the live key/scale (chords.json).
    struct DatabaseHit
    {
        std::string name;
        int rootPc = 0;
        int bassPc = 0;
        int score = 0;
        bool exact = true; // false = near-match (e.g. missing only natural 5)
        Chord chord;
    };

    int scoreDatabaseChord(const Chord& chord, int bassPitchClass, bool exact)
    {
        const int root = chordRootPc(chord);
        const int bass = chordBassPc(chord);
        // Prefer exact bass spelling (inversions), then default popularity, shorter symbols.
        int score = (exact ? 520 : 400) - chord.popularityOrder * 8;
        if (mod12(bass) == mod12(bassPitchClass))
            score += 120;
        if (mod12(root) == mod12(bassPitchClass))
            score += 40;
        score -= static_cast<int>(chord.readableName.size());
        if (!exact)
            score -= 30; // prefer true exact set matches over near-matches
        return score;
    }

    std::optional<DatabaseHit> matchChordDatabase(const std::array<bool, 12>& pcs,
                                                  int bassPitchClass,
                                                  Key spellKey,
                                                  Scale scale)
    {
        const auto held = pitchClassMask(pcs);
        if (held == 0)
            return std::nullopt;

        const auto& keyScale = ChordDatabase::getInstance().get(spellKey, scale);
        std::optional<DatabaseHit> best;

        const auto considerHit = [&](const Chord& chord, bool exact)
        {
            if (chord.notes.empty())
                return;
            const int score = scoreDatabaseChord(chord, bassPitchClass, exact);
            if (best.has_value() && score <= best->score)
                return;

            DatabaseHit hit;
            hit.name = chord.readableName.empty() ? chord.symbol : chord.readableName;
            hit.rootPc = chordRootPc(chord);
            hit.bassPc = chordBassPc(chord);
            hit.score = score;
            hit.exact = exact;
            hit.chord = chord;
            best = std::move(hit);
        };

        // Pass 1: exact pitch-class set match.
        for (const auto& degree : keyScale.degrees)
            for (const auto& chord : degree.chords)
                if (chordPitchClassMask(chord) == held)
                    considerHit(chord, true);

        if (best.has_value())
            return best;

        // Pass 2: near-match — held is a subset of a catalogue chord missing only the natural 5th
        // (common shell / no5 voicings: C E B → Cmaj7).
        for (const auto& degree : keyScale.degrees)
        {
            for (const auto& chord : degree.chords)
            {
                const auto chordMask = chordPitchClassMask(chord);
                if ((held & chordMask) != held)
                    continue; // held must be subset of catalogue chord
                const auto missing = static_cast<std::uint16_t>(chordMask & ~held);
                if (missing == 0 || (missing & (missing - 1)) != 0)
                    continue; // need exactly one missing pitch class
                int missingPc = 0;
                while (((missing >> missingPc) & 1u) == 0)
                    ++missingPc;
                const int root = chordRootPc(chord);
                if (missingPc != mod12(root + 7))
                    continue; // only allow omitting natural 5
                // Need enough of the chord present (not a lone root matching a huge chord).
                const int chordTones = std::popcount(chordMask);
                const int heldTones = std::popcount(held);
                if (heldTones < 3 || heldTones + 1 < chordTones)
                    continue;
                considerHit(chord, false);
            }
        }
        return best;
    }

    // Prefer catalogue name when it agrees with the live bass (or shares the algorithmic root).
    // Avoids forcing Am7/C over C6 when C is in the bass.
    bool shouldPreferDatabase(const DatabaseHit& db, const Candidate& algorithmic, int bassPitchClass)
    {
        if (db.exact && mod12(db.bassPc) == mod12(bassPitchClass))
            return true;
        if (db.exact && mod12(db.rootPc) == mod12(algorithmic.root))
            return true;
        if (!db.exact && mod12(db.rootPc) == mod12(algorithmic.root)
            && algorithmic.rootInBass)
            return true;
        // Catalogue spelling with matching bass even if root differs (true inversion entry).
        if (mod12(db.bassPc) == mod12(bassPitchClass)
            && algorithmic.rootInBass
            && mod12(db.rootPc) != mod12(bassPitchClass)
            && db.exact)
            return true;
        // Strong exact catalogue hit with same root as bass algorithm preferred.
        if (db.exact && algorithmic.rootInBass && mod12(db.rootPc) == mod12(bassPitchClass))
            return true;
        return false;
    }

    std::string inversionFigure(int root, int bass, TriadQuality quality)
    {
        if (mod12(root) == mod12(bass))
            return {};
        // Only common classical figures for simple triads / sevenths.
        if (quality == TriadQuality::Major || quality == TriadQuality::Minor
            || quality == TriadQuality::Diminished || quality == TriadQuality::Augmented)
        {
            if (mod12(bass) == mod12(root + 3) || mod12(bass) == mod12(root + 4))
                return "6"; // first inversion
            if (mod12(bass) == mod12(root + 7) || mod12(bass) == mod12(root + 6)
                || mod12(bass) == mod12(root + 8))
                return "64"; // second inversion
        }
        if (quality == TriadQuality::Dominant7 || quality == TriadQuality::Major7
            || quality == TriadQuality::Minor7 || quality == TriadQuality::HalfDim7)
        {
            if (mod12(bass) == mod12(root + 3) || mod12(bass) == mod12(root + 4))
                return "65"; // approx first-inv seventh
            if (mod12(bass) == mod12(root + 7) || mod12(bass) == mod12(root + 6))
                return "43";
            if (mod12(bass) == mod12(root + 10) || mod12(bass) == mod12(root + 11))
                return "42";
        }
        return {};
    }

    // When two complete maj/min triads coexist, offer a polychord alternate (e.g. "F|G").
    std::string polychordAlternate(const std::array<bool, 12>& pcs, Key spellKey)
    {
        struct TriadHit { int root = 0; bool minor = false; };
        std::vector<TriadHit> triads;
        for (int root = 0; root < 12; ++root)
        {
            if (!pcs[static_cast<std::size_t>(root)])
                continue;
            const bool has5 = hasInterval(pcs, root, 7);
            if (!has5)
                continue;
            const bool hasM3 = hasInterval(pcs, root, 4);
            const bool hasm3 = hasInterval(pcs, root, 3);
            if (hasM3 == hasm3)
                continue; // need exactly one third
            // Avoid counting a triad that is just part of a denser stack on same root later.
            triads.push_back({ root, hasm3 });
        }
        if (triads.size() < 2)
            return {};

        // Prefer two roots a fifth or fourth apart (common polychord spacing).
        for (std::size_t i = 0; i < triads.size(); ++i)
        {
            for (std::size_t j = i + 1; j < triads.size(); ++j)
            {
                const int a = triads[i].root;
                const int b = triads[j].root;
                const int dist = std::min(mod12(a - b), mod12(b - a));
                if (dist != 5 && dist != 7 && dist != 2 && dist != 3)
                    continue;
                // Upper | lower by pitch class number is arbitrary; put higher root first for pop convention.
                const auto& upper = (a > b) ? triads[i] : triads[j];
                const auto& lower = (a > b) ? triads[j] : triads[i];
                std::string u = std::string(pcName(upper.root, spellKey)) + (upper.minor ? "m" : "");
                std::string l = std::string(pcName(lower.root, spellKey)) + (lower.minor ? "m" : "");
                if (u == l)
                    continue;
                return u + "|" + l;
            }
        }
        return {};
    }

    void fillRoman(ChordDetection& result, const Chord& chord, Key spellKey,
                   std::optional<Scale> scale, TriadQuality qualityHint = TriadQuality::Major,
                   bool hasQualityHint = false)
    {
        if (!scale.has_value() || chord.notes.empty())
            return;

        const auto& keyScale = ChordDatabase::getInstance().get(spellKey, *scale);

        // Prefer functional labels when they fire with enough confidence.
        if (const auto sec = analyseSecondaryDominant(chord, keyScale);
            sec.hit && sec.confidence >= kMinLabelConfidence)
        {
            result.romanNumeral = sec.label;
            return;
        }
        if (const auto tri = analyseTritoneSubstitution(chord, keyScale);
            tri.hit && tri.confidence >= kMinLabelConfidence)
        {
            result.romanNumeral = tri.label;
            return;
        }
        if (const auto mix = analyseModeMixture(chord, keyScale);
            mix.hit && mix.confidence >= kMinLabelConfidence)
        {
            result.romanNumeral = mix.label;
            return;
        }

        auto roman = NextChordScorer::romanForChord(chord, keyScale);
        if (roman.empty())
            return;

        // Append classical inversion figures for simple diatonic sonorities (I6, ii64, V65…).
        const int root = NextChordScorer::rootPitchClass(chord);
        const int bass = NextChordScorer::bassPitchClass(chord);
        const auto q = hasQualityHint ? qualityHint : NextChordScorer::detectTriadQuality(chord);
        const auto fig = inversionFigure(root, bass, q);
        if (!fig.empty()
            && roman.find('/') == std::string::npos
            && roman.find('(') == std::string::npos)
        {
            roman += fig;
        }
        result.romanNumeral = std::move(roman);
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

    const KeyScaleData* keyScalePtr = nullptr;
    std::optional<DatabaseHit> dbHit;
    if (scale.has_value())
    {
        keyScalePtr = &ChordDatabase::getInstance().get(spellKey, *scale);
        dbHit = matchChordDatabase(pcs, bassPitchClass, spellKey, *scale);
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

        if (auto cand = evaluateRoot(pcs, root, bassPitchClass, nHeld, true, spellKey, keyScalePtr))
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
            if (auto cand = evaluateRoot(pcs, root, bassPitchClass, nHeld, false, spellKey, keyScalePtr))
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
        // Prefer an exact catalogue match over raw pitch-class listing.
        if (dbHit.has_value())
        {
            result.matched = true;
            result.name = dbHit->name;
            result.rootPitchClass = dbHit->rootPc;
            result.bassPitchClass = bassPitchClass;
            result.toneCount = nHeld;
            result.qualityLabel = "catalogue";
            result.fromChordDatabase = true;
            result.confidence = 0.92f;
            fillRoman(result, dbHit->chord, spellKey, scale, TriadQuality::Major, false);
            return result;
        }

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

    const int secondScore = globalSecond.has_value() ? globalSecond->score : 0;
    result.confidence = confidenceFromScores(best->score, secondScore, nHeld);

    // Catalogue spelling when it agrees with bass/root context; otherwise keep algorithmic name
    // (C6 with C bass beats Am7/C catalogue twin) and surface catalogue as alternate.
    if (dbHit.has_value() && shouldPreferDatabase(*dbHit, *best, bassPitchClass))
    {
        if (dbHit->name != result.name)
            result.alternateName = result.name;
        result.name = dbHit->name;
        result.fromChordDatabase = true;
        result.rootPitchClass = dbHit->rootPc;
        result.confidence = std::max(result.confidence, dbHit->exact ? 0.9f : 0.78f);
        fillRoman(result, dbHit->chord, spellKey, scale, result.quality, result.hasLibraryQuality);
    }
    else
    {
        if (dbHit.has_value() && dbHit->name != result.name)
            result.alternateName = dbHit->name;
        else if (globalSecond.has_value() && globalSecond->name != result.name)
            result.alternateName = globalSecond->name;
        else if (bestRootPosition.has_value() && bestSlash.has_value()
                 && best->name == bestRootPosition->name
                 && bestSlash->name != result.name)
            result.alternateName = bestSlash->name;

        const Chord romanChord = buildChordForRoman(*best, spellKey, bassPitchClass);
        fillRoman(result, romanChord, spellKey, scale, result.quality, result.hasLibraryQuality);
    }

    // Polychord alternate when two complete triads coexist (e.g. "G|F"). This is often more
    // readable than a dense extension name for stacked-triad voicings.
    if (const auto poly = polychordAlternate(pcs, spellKey);
        !poly.empty() && poly != result.name)
    {
        result.alternateName = poly;
    }

    // Avoid alternate equal to primary after catalogue override.
    if (result.alternateName == result.name)
        result.alternateName.clear();

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
