#include "Theory/NextScaleGenerator.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <set>

#include "Theory/ChordDatabase.h"

namespace theory
{

namespace
{
    std::string toLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    int clampInt(int value, int low, int high)
    {
        return value < low ? low : (value > high ? high : value);
    }

    std::string makeLabel(Key key, Scale scale)
    {
        return getKeyLabel(key) + " " + getScaleJsonKey(scale);
    }

    std::array<bool, 12> chordPitchClasses(const Chord& chord)
    {
        std::array<bool, 12> pcs {};
        pcs.fill(false);
        for (const auto& note : chord.notes)
            pcs[static_cast<std::size_t>(clampInt(note.getPitchClass(), 0, 11))] = true;
        return pcs;
    }

    std::array<bool, 12> scalePitchClasses(Key key, Scale scale)
    {
        std::array<bool, 12> pcs {};
        pcs.fill(false);
        const auto& data = ChordDatabase::getInstance().get(key, scale);
        for (const auto& note : data.scaleNotes)
            pcs[static_cast<std::size_t>(clampInt(note.getPitchClass(), 0, 11))] = true;
        return pcs;
    }

    int fitPercentForChord(const std::array<bool, 12>& scalePcs, const std::array<bool, 12>& chordPcs)
    {
        auto chordTones = 0;
        auto matched = 0;
        for (int i = 0; i < 12; ++i)
        {
            if (!chordPcs[static_cast<std::size_t>(i)])
                continue;
            ++chordTones;
            if (scalePcs[static_cast<std::size_t>(i)])
                ++matched;
        }
        if (chordTones == 0)
            return 50;
        return clampInt(static_cast<int>(std::lround(100.0 * matched / chordTones)), 0, 100);
    }

    bool chordAppearsInScale(Key key, Scale scale, const Chord& chord)
    {
        const auto& data = ChordDatabase::getInstance().get(key, scale);
        for (const auto& degree : data.degrees)
            for (const auto& c : degree.chords)
                if (c.symbol == chord.symbol || c.readableName == chord.readableName)
                    return true;
        return false;
    }

    std::optional<std::pair<Key, Scale>> relativeCompanion(Key key, Scale scale)
    {
        const auto k = static_cast<int>(key);
        if (scale == Scale::Major)
            return std::make_pair(static_cast<Key>((k + 9) % 12), Scale::Minor);
        if (scale == Scale::Minor)
            return std::make_pair(static_cast<Key>((k + 3) % 12), Scale::Major);
        if (scale == Scale::Dorian)
            return std::make_pair(static_cast<Key>((k + 10) % 12), Scale::Major);
        if (scale == Scale::Mixolydian)
            return std::make_pair(static_cast<Key>((k + 5) % 12), Scale::Major);
        return std::nullopt;
    }

    bool matchesQuery(const ScaleSuggestion& suggestion, const std::string& queryLower)
    {
        if (queryLower.empty())
            return true;

        auto contains = [&](const std::string& hay)
        {
            return toLower(hay).find(queryLower) != std::string::npos;
        };

        return contains(suggestion.label)
            || contains(getScaleJsonKey(suggestion.scale))
            || contains(getKeyLabel(suggestion.key))
            || contains(suggestion.reasonLabel);
    }

    struct CandidateKey
    {
        Key key;
        Scale scale;
        bool operator<(const CandidateKey& other) const
        {
            if (key != other.key)
                return static_cast<int>(key) < static_cast<int>(other.key);
            return static_cast<int>(scale) < static_cast<int>(other.scale);
        }
    };

    float scalePrior(Scale scale)
    {
        switch (scale)
        {
            case Scale::Major:
            case Scale::Minor:
            case Scale::Dorian:
            case Scale::Mixolydian:
                return 4.f;
            case Scale::HarmonicMinor:
            case Scale::MelodicMinor:
                return 2.f;
            case Scale::Phrygian:
            case Scale::Lydian:
            case Scale::Locrian:
            case Scale::MinorBlues:
                return 0.f;
        }
        return 0.f;
    }
}

std::vector<ScaleSuggestion> NextScaleGenerator::generate(
    Key currentKey,
    Scale currentScale,
    const std::optional<Chord>& currentChord,
    Pool pool,
    const std::string& query,
    int maxResults)
{
    const auto queryLower = toLower(query);
    // trim simple whitespace
    auto first = queryLower.find_first_not_of(" \t\n\r");
    auto last = queryLower.find_last_not_of(" \t\n\r");
    const std::string queryTrimmed = first == std::string::npos
        ? std::string {}
        : queryLower.substr(first, last - first + 1);

    std::optional<std::array<bool, 12>> chordPcs;
    if (currentChord.has_value())
        chordPcs = chordPitchClasses(*currentChord);

    std::set<CandidateKey> poolKeys;

    for (int s = 0; s < kNumScales; ++s)
        poolKeys.insert({ currentKey, static_cast<Scale>(s) });

    if (const auto relative = relativeCompanion(currentKey, currentScale))
        poolKeys.insert({ relative->first, relative->second });

    if (currentChord.has_value() && !currentChord->notes.empty())
    {
        const auto rootPc = clampInt(currentChord->notes.front().getPitchClass(), 0, 11);
        const auto chordRootKey = static_cast<Key>(rootPc);
        for (int s = 0; s < kNumScales; ++s)
            poolKeys.insert({ chordRootKey, static_cast<Scale>(s) });
    }

    std::vector<ScaleSuggestion> suggestions;
    suggestions.reserve(poolKeys.size());

    for (const auto& ck : poolKeys)
    {
        if (ck.key == currentKey && ck.scale == currentScale)
            continue;

        ScaleSuggestion suggestion;
        suggestion.key = ck.key;
        suggestion.scale = ck.scale;
        suggestion.label = makeLabel(ck.key, ck.scale);

        const auto scalePcs = scalePitchClasses(ck.key, ck.scale);
        if (chordPcs.has_value())
            suggestion.fitPercent = fitPercentForChord(scalePcs, *chordPcs);
        else
            suggestion.fitPercent = 70;

        float score = static_cast<float>(suggestion.fitPercent);

        if (ck.key == currentKey)
        {
            suggestion.reasonLabel = "Parallel";
            score += 18.f;
        }
        else if (const auto relative = relativeCompanion(currentKey, currentScale);
                 relative && relative->first == ck.key && relative->second == ck.scale)
        {
            suggestion.reasonLabel = "Relative";
            score += 22.f;
        }
        else if (currentChord.has_value() && chordAppearsInScale(ck.key, ck.scale, *currentChord))
        {
            suggestion.reasonLabel = "Contains " + currentChord->readableName;
            score += 12.f;
        }
        else
        {
            suggestion.reasonLabel = "Related";
        }

        score += scalePrior(ck.scale);

        if (pool == Pool::Predicted && chordPcs.has_value() && suggestion.fitPercent < 66
            && ck.key != currentKey)
        {
            continue;
        }

        suggestion.score = score;

        if (!matchesQuery(suggestion, queryTrimmed))
            continue;

        suggestions.push_back(std::move(suggestion));
    }

    std::sort(suggestions.begin(), suggestions.end(), [](const ScaleSuggestion& a, const ScaleSuggestion& b)
    {
        if (std::abs(a.score - b.score) > 0.001f)
            return a.score > b.score;
        if (a.fitPercent != b.fitPercent)
            return a.fitPercent > b.fitPercent;
        return a.label < b.label;
    });

    if (static_cast<int>(suggestions.size()) > maxResults)
        suggestions.resize(static_cast<std::size_t>(maxResults));

    return suggestions;
}

}
