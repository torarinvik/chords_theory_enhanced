#include "Theory/ChordExpert.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

#include "Theory/HarmonicPredicates.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

namespace theory
{

namespace
{
    int mod12(int x)
    {
        const int m = x % 12;
        return m < 0 ? m + 12 : m;
    }

    int heldCount(const std::array<bool, 12>& pcs)
    {
        auto n = 0;
        for (bool p : pcs)
            if (p)
                ++n;
        return n;
    }

    bool hasPc(const std::array<bool, 12>& pcs, int pc)
    {
        return pcs[static_cast<std::size_t>(mod12(pc))];
    }

    bool hasInterval(const std::array<bool, 12>& pcs, int root, int interval)
    {
        return hasPc(pcs, root + interval);
    }

    const Chord* previousChord(const ChordExpertContext& ctx)
    {
        if (ctx.previousChords.empty())
            return nullptr;
        return &ctx.previousChords.back();
    }

    bool looksDominant(const Chord& chord)
    {
        const auto q = NextChordScorer::detectTriadQuality(chord);
        return isDominantFamilyQuality(q) || q == TriadQuality::Dominant7;
    }

    // Dominant resolves up a fourth (root + 5 semitones).
    int resolutionTargetRoot(const Chord& dominant)
    {
        return mod12(NextChordScorer::rootPitchClass(dominant) + 5);
    }

    std::string applyStyleSpelling(std::string name, ChordNamingStyle style, const ChordDetection& det)
    {
        if (style == ChordNamingStyle::JazzChart)
        {
            // Chart spelling: m7b5 → ø7
            if (det.qualityLabel == "m7b5" || name.find("m7b5") != std::string::npos)
            {
                const auto slash = name.find('/');
                std::string head = slash == std::string::npos ? name : name.substr(0, slash);
                const std::string tail = slash == std::string::npos ? std::string {} : name.substr(slash);
                const auto pos = head.rfind("m7b5");
                if (pos != std::string::npos && pos + 4 == head.size())
                    name = head.substr(0, pos) + "ø7" + tail;
            }
        }
        else if (style == ChordNamingStyle::PopSlash)
        {
            auto strip = [&](const char* tag)
            {
                for (;;)
                {
                    const auto p = name.find(tag);
                    if (p == std::string::npos)
                        break;
                    name.erase(p, std::strlen(tag));
                }
            };
            strip("(no3)");
            strip("(no 5)");
        }
        return name;
    }

    bool shouldPreferAlternate(const ChordDetection& det, const ChordExpertContext& ctx)
    {
        if (det.alternateName.empty() || det.alternateName == det.name)
            return false;

        if (ctx.style == ChordNamingStyle::Classical)
        {
            const bool primaryJazz = det.name.find("alt") != std::string::npos
                || det.name.find("#9") != std::string::npos
                || det.name.find("b9") != std::string::npos
                || det.name.find("ø") != std::string::npos
                || det.name.find("13") != std::string::npos;
            const bool altJazz = det.alternateName.find("alt") != std::string::npos
                || det.alternateName.find("#9") != std::string::npos
                || det.alternateName.find("13") != std::string::npos;
            if (primaryJazz && !altJazz)
                return true;
        }

        if (ctx.style == ChordNamingStyle::PopSlash)
        {
            const bool primaryDense = det.name.find('9') != std::string::npos
                || det.name.find("11") != std::string::npos
                || det.name.find("13") != std::string::npos
                || det.name.find("alt") != std::string::npos;
            const bool altSlash = det.alternateName.find('/') != std::string::npos
                || det.alternateName.find('|') != std::string::npos;
            // Prefer short slash/poly over dense jazz soup when pop style is selected.
            if (primaryDense && altSlash && det.alternateName.size() + 2 < det.name.size())
                return true;
        }

        // Prefer alternate if it is the resolution target of previous dominant and primary is not.
        if (const auto* prev = previousChord(ctx); prev != nullptr && looksDominant(*prev))
        {
            const int target = resolutionTargetRoot(*prev);
            if (det.rootPitchClass != target)
            {
                // Polychord alternates start with upper root — skip those.
                if (det.alternateName.find('|') != std::string::npos)
                    return false;
            }
        }

        return false;
    }

    std::string buildExplanation(const ChordDetection& det, const ChordExpertContext& ctx,
                                 bool swapped, bool rootless)
    {
        std::vector<std::string> parts;

        if (!det.romanNumeral.empty())
            parts.push_back(det.romanNumeral);

        if (const auto* prev = previousChord(ctx))
        {
            const int prevRoot = NextChordScorer::rootPitchClass(*prev);
            const int curRoot = det.rootPitchClass;

            if (looksDominant(*prev) && curRoot == resolutionTargetRoot(*prev))
                parts.push_back("resolves previous dominant");
            else if (prevRoot == curRoot)
                parts.push_back("same root - recolour");
            else if (mod12(curRoot - prevRoot) == 5)
                parts.push_back("up a fourth");
            else if (mod12(prevRoot - curRoot) == 5)
                parts.push_back("down a fourth");
        }

        if (rootless)
            parts.push_back("rootless shell");
        if (det.fromChordDatabase)
            parts.push_back("catalogue");
        if (swapped)
            parts.push_back("context alternate");

        switch (ctx.style)
        {
            case ChordNamingStyle::JazzChart: parts.push_back("jazz"); break;
            case ChordNamingStyle::PopSlash: parts.push_back("pop"); break;
            case ChordNamingStyle::Classical: parts.push_back("classical"); break;
        }

        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            if (i > 0)
                out += " - ";
            out += parts[i];
        }
        return out;
    }

    // Previous V7 → held guide tones of I (no tonic root): name the resolution chord.
    std::optional<ChordDetection> tryRootlessResolution(const std::array<bool, 12>& pcs,
                                                        int bass,
                                                        const ChordExpertContext& ctx)
    {
        const auto* prev = previousChord(ctx);
        if (prev == nullptr || !looksDominant(*prev))
            return std::nullopt;

        const int tonic = resolutionTargetRoot(*prev);
        if (hasPc(pcs, tonic))
            return std::nullopt; // root is sounding — not rootless

        const bool maj3 = hasInterval(pcs, tonic, 4);
        const bool min3 = hasInterval(pcs, tonic, 3);
        const bool maj7 = hasInterval(pcs, tonic, 11);
        const bool min7 = hasInterval(pcs, tonic, 10);
        const bool fifth = hasInterval(pcs, tonic, 7);

        if (!(maj3 || min3))
            return std::nullopt;
        if (!(maj7 || min7 || fifth))
            return std::nullopt;
        if (heldCount(pcs) < 3)
            return std::nullopt;

        // Complete the chord with the implied tonic and re-detect.
        auto completed = pcs;
        completed[static_cast<std::size_t>(mod12(tonic))] = true;
        auto det = ChordDetector::detect(completed, bass, ctx.key, ctx.scale);
        if (!det.matched)
            return std::nullopt;

        // Prefer readings whose root is the resolution tonic.
        if (det.rootPitchClass != tonic)
        {
            // Force a simple library spelling on the tonic.
            const auto q = maj3
                ? (maj7 ? TriadQuality::Major7 : TriadQuality::Major)
                : (min7 ? TriadQuality::Minor7 : TriadQuality::Minor);
            const auto forced = TriadLibrary::makeTriad(tonic, q, ctx.key, 0);
            det.name = forced.readableName.empty() ? forced.symbol : forced.readableName;
            if (mod12(bass) != tonic)
            {
                // Keep slash if bass is a chord tone of the forced chord.
                det.name += "/";
                // Re-detect bass name via single-note detect.
                const auto bassDet = ChordDetector::detectFromMidiNotes({ 48 + mod12(bass) }, ctx.key);
                det.name += bassDet.matched ? bassDet.name : "?";
            }
            det.rootPitchClass = tonic;
            det.hasLibraryQuality = true;
            det.quality = q;
        }

        det.confidence = std::max(det.confidence, 0.74f);
        det.qualityLabel = "rootless";
        det.bassPitchClass = bass;
        return det;
    }

    void pushUnique(std::vector<std::string>& out, const std::string& name, const std::string& primary)
    {
        if (name.empty() || name == primary)
            return;
        if (std::find(out.begin(), out.end(), name) != out.end())
            return;
        if (out.size() >= 3)
            return;
        out.push_back(name);
    }
}

const char* ChordExpert::styleLabel(ChordNamingStyle style)
{
    switch (style)
    {
        case ChordNamingStyle::JazzChart: return "Jazz chart";
        case ChordNamingStyle::PopSlash: return "Pop / slash";
        case ChordNamingStyle::Classical: return "Classical";
    }
    return "Jazz chart";
}

std::string ChordExpert::styleKey(ChordNamingStyle style)
{
    switch (style)
    {
        case ChordNamingStyle::JazzChart: return "jazz";
        case ChordNamingStyle::PopSlash: return "pop";
        case ChordNamingStyle::Classical: return "classical";
    }
    return "jazz";
}

ChordNamingStyle ChordExpert::parseStyle(const std::string& key)
{
    if (key == "pop")
        return ChordNamingStyle::PopSlash;
    if (key == "classical")
        return ChordNamingStyle::Classical;
    return ChordNamingStyle::JazzChart;
}

ChordExpertResult ChordExpert::analyse(const std::vector<int>& midiNotes,
                                       const ChordExpertContext& context)
{
    if (midiNotes.empty())
        return {};

    std::array<bool, 12> pcs {};
    pcs.fill(false);
    int lowest = 128;
    for (const int n : midiNotes)
    {
        if (n < 0 || n > 127)
            continue;
        pcs[static_cast<std::size_t>(mod12(n))] = true;
        lowest = std::min(lowest, n);
    }
    if (lowest > 127)
        return {};
    return analyse(pcs, mod12(lowest), context);
}

ChordExpertResult ChordExpert::analyse(const std::array<bool, 12>& pcs, int bassPitchClass,
                                       const ChordExpertContext& context)
{
    ChordExpertResult result;
    if (heldCount(pcs) == 0)
        return result;

    bassPitchClass = mod12(bassPitchClass);

    auto det = ChordDetector::detect(pcs, bassPitchClass, context.key, context.scale);
    bool usedContext = previousChord(context) != nullptr;
    bool rootless = false;

    if (auto rootlessDet = tryRootlessResolution(pcs, bassPitchClass, context))
    {
        usedContext = true;
        if (!det.matched || det.confidence < 0.82f || !hasPc(pcs, det.rootPitchClass))
        {
            if (det.matched && det.name != rootlessDet->name)
                result.alternatives.push_back(det.name);
            det = *rootlessDet;
            rootless = true;
        }
        else if (rootlessDet->name != det.name)
        {
            result.alternatives.push_back(rootlessDet->name);
        }
    }

    bool swapped = false;
    if (shouldPreferAlternate(det, context))
    {
        std::swap(det.name, det.alternateName);
        swapped = true;
        usedContext = true;
    }

    if (const auto* prev = previousChord(context);
        prev != nullptr && looksDominant(*prev)
        && det.rootPitchClass == resolutionTargetRoot(*prev))
    {
        det.confidence = std::min(0.99f, det.confidence + 0.1f);
        usedContext = true;
    }

    const auto beforeStyle = det.name;
    det.name = applyStyleSpelling(det.name, context.style, det);
    const bool styleRewrite = det.name != beforeStyle;

    pushUnique(result.alternatives, det.alternateName, det.name);
    pushUnique(result.alternatives, beforeStyle, det.name);

    result.detection = std::move(det);
    result.usedProgressionContext = usedContext;
    result.usedStyleRewrite = styleRewrite;
    result.explanation = buildExplanation(result.detection, context, swapped, rootless);
    return result;
}

}
