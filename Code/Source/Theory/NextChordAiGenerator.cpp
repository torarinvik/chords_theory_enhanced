#include "Theory/NextChordAiGenerator.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "Theory/ChordSeqAIModel.h"
#include "Theory/NextChordScorer.h"
#include "Theory/TriadLibrary.h"

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

    std::optional<Degree> matchingDegree(const Chord& sonority, const KeyScaleData& keyScale)
    {
        const auto pcs = pitchClassSet(sonority);
        for (const auto& degreeData : keyScale.degrees)
        {
            for (const auto& chord : degreeData.chords)
            {
                if (pitchClassSet(chord) == pcs)
                    return degreeData.degree;
            }
        }
        return std::nullopt;
    }

    std::string formatProbPercent(float p)
    {
        const int pct = static_cast<int>(std::lround(std::clamp(p, 0.0f, 1.0f) * 100.0f));
        return std::to_string(pct) + "% AI";
    }
}

bool NextChordAiGenerator::isAvailable()
{
    return ChordSeqAIModel::getInstance().isReady();
}

std::string NextChordAiGenerator::unavailableReason()
{
    const auto& model = ChordSeqAIModel::getInstance();
    if (model.isReady())
        return {};
    return model.loadError().empty() ? "AI model unavailable" : model.loadError();
}

std::vector<NextChordCandidate> NextChordAiGenerator::generate(const Chord& currentChord,
                                                               const KeyScaleData& keyScale,
                                                               const SequenceContext& sequence,
                                                               int topK)
{
    auto& model = ChordSeqAIModel::getInstance();
    if (!model.isReady() || currentChord.notes.empty())
        return {};

    // Build token sequence: previous chords (oldest→newest) then current.
    std::vector<int> tokens;
    tokens.reserve(static_cast<size_t>(sequence.size()) + 1);
    for (const auto& event : sequence.previous)
    {
        if (auto t = model.tokenForChord(event.chord))
            tokens.push_back(*t);
    }
    if (auto currentToken = model.tokenForChord(currentChord))
        tokens.push_back(*currentToken);
    else
        return {}; // cannot condition without a token for the current chord

    const auto predictions = model.predictTopK(tokens, topK, true);
    if (predictions.empty())
        return {};

    std::vector<NextChordCandidate> candidates;
    candidates.reserve(predictions.size());

    for (const auto& pred : predictions)
    {
        auto chord = model.chordForToken(pred.token);
        if (!chord || chord->notes.empty())
            continue;

        // Contextual spelling via theory layer (A# → Bb as bVII in C major, etc.).
        *chord = NextChordScorer::spellInKeyContext(*chord, keyScale);

        if (pitchClassSet(*chord) == pitchClassSet(currentChord)
            && NextChordScorer::bassPitchClass(*chord) == NextChordScorer::bassPitchClass(currentChord))
            continue;

        NextChordCandidate candidate;
        candidate.chord = std::move(*chord);
        candidate.degree = matchingDegree(candidate.chord, keyScale);

        // Pure model ranking — probability only (no theory scorer blend).
        const float p = std::clamp(pred.probability, 0.0f, 1.0f);
        candidate.rankingScore = p;
        candidate.fitPercent = static_cast<int>(std::lround(p * 100.0f));
        candidate.tensionPercent = 0;
        candidate.metrics.aiExpectedness = p;
        candidate.metrics.surprise = std::clamp(1.0f - p, 0.0f, 1.0f);
        candidate.surprisePercent = static_cast<int>(std::lround(candidate.metrics.surprise * 100.0f));
        candidate.reasonLabel = formatProbPercent(p);

        candidates.push_back(std::move(candidate));
    }

    // Stable sort by model probability (already near-sorted from predictTopK).
    std::stable_sort(candidates.begin(), candidates.end(),
                     [](const NextChordCandidate& a, const NextChordCandidate& b)
                     {
                         return a.rankingScore > b.rankingScore;
                     });

    return candidates;
}

}
