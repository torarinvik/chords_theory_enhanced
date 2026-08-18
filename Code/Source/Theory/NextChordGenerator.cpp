#include "Theory/NextChordGenerator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <vector>

#include "Theory/ChordSeqAIModel.h"
#include "Theory/HarmonicPredicates.h"
#include "Theory/MechanismCandidateGenerator.h"
#include "Theory/NextChordAiGenerator.h"
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

    using VoicingKey = std::tuple<int, std::set<int>>;

    VoicingKey voicingKey(const Chord& chord)
    {
        const int bass = chord.notes.empty() ? -1 : chord.notes.front().getPitchClass();
        return { bass, pitchClassSet(chord) };
    }

    bool sameVoicing(const Chord& a, const Chord& b)
    {
        return voicingKey(a) == voicingKey(b);
    }

    using FamilyKey = std::pair<int, int>;

    FamilyKey familyKeyOf(const NextChordCandidate& c)
    {
        if (c.familyRootPc >= 0 && c.familyKind >= 0)
            return { c.familyRootPc, c.familyKind };
        const int root = NextChordScorer::rootPitchClass(c.chord);
        const int kind = static_cast<int>(
            NextChordScorer::familyKindForQuality(NextChordScorer::detectTriadQuality(c.chord)));
        return { root, kind };
    }

    float representativeScore(const NextChordCandidate& c)
    {
        // Prefer full triads as the face of an idea; power/sus lose hard.
        return c.rankingScore
            - 0.55f * c.metrics.complexity
            + NextChordScorer::ideaRepresentativeBonus(c.chord);
    }

    std::string toLower(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    bool matchesQuery(const NextChordCandidate& candidate, const std::string& queryLower)
    {
        if (queryLower.empty())
            return true;

        auto contains = [&](const std::string& hay)
        {
            return toLower(hay).find(queryLower) != std::string::npos;
        };

        return contains(candidate.chord.readableName)
            || contains(candidate.chord.symbol)
            || contains(candidate.reasonLabel);
    }

    void applyQueryAndCap(std::vector<NextChordCandidate>& candidates,
                          const std::string& query,
                          int maxResults)
    {
        const auto queryLower = toLower(query);
        if (!queryLower.empty())
        {
            candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                  [&](const NextChordCandidate& c) { return !matchesQuery(c, queryLower); }),
                             candidates.end());
        }

        if (maxResults > 0 && static_cast<int>(candidates.size()) > maxResults)
            candidates.resize(static_cast<std::size_t>(maxResults));
    }

    bool isMainListIdea(const NextChordCandidate& c, const Chord& current)
    {
        const auto q = NextChordScorer::detectTriadQuality(c.chord);
        // Incomplete sonorities only appear as variants, never as independent top ideas.
        if (NextChordScorer::isIncompleteSonority(q))
            return false;
        // Same root as current: tonic recolour/prolong (Cmaj7, C5, Csus) is not a move.
        // Exception: C7 is a distinct Dominant idea (V/IV / blues), not mere prolongation.
        if (NextChordScorer::isProlongationOf(c.chord, current)
            && q != TriadQuality::Dominant7)
            return false;
        return true;
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

    // Map AI model probability → expectedness for a pitch-class set (and bass when available).
    std::map<std::set<int>, float> buildAiExpectednessMap(const Chord& currentChord,
                                                          const KeyScaleData& keyScale,
                                                          const SequenceContext& sequence)
    {
        std::map<std::set<int>, float> out;
        if (!NextChordAiGenerator::isAvailable())
            return out;

        auto& model = ChordSeqAIModel::getInstance();
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
            return out;

        const auto predictions = model.predictTopK(tokens, 32, true);
        float maxP = 1.0e-6f;
        for (const auto& pred : predictions)
            maxP = std::max(maxP, pred.probability);

        for (const auto& pred : predictions)
        {
            auto chord = model.chordForToken(pred.token);
            if (!chord || chord->notes.empty())
                continue;
            Chord spelled = NextChordScorer::spellInKeyContext(*chord, keyScale);
            if (spelled.notes.empty())
                spelled = *chord;
            const float rel = std::clamp(pred.probability / maxP, 0.0f, 1.0f);
            const auto pcs = pitchClassSet(spelled);
            out[pcs] = std::max(out[pcs], rel);
        }
        return out;
    }

    // 2-step path value: best immediate continuation productivity × coherence of that path.
    float computePathValue(const Chord& fromCandidate, const KeyScaleData& keyScale, float drama01)
    {
        const float oneStep = lookaheadProductivity(fromCandidate, keyScale, drama01);
        // Light 2-step: take top productivity target and measure its own productivity.
        const int tonic = NextChordScorer::keyTonicPitchClass(keyScale);
        const auto spell = keyScale.key;
        std::vector<Chord> pool;
        pool.push_back(TriadLibrary::makeTriad(tonic, TriadQuality::Major, spell, 0));
        pool.push_back(TriadLibrary::makeTriad(tonic, TriadQuality::Minor, spell, 0));
        for (const auto& note : keyScale.scaleNotes)
        {
            const int pc = note.getPitchClass();
            pool.push_back(TriadLibrary::makeTriad(pc, TriadQuality::Major, spell, 0));
            pool.push_back(TriadLibrary::makeTriad(pc, TriadQuality::Minor, spell, 0));
        }
        if (const auto sec = analyseSecondaryDominant(fromCandidate, keyScale); sec.hit)
            pool.push_back(TriadLibrary::makeTriad(sec.targetRootPc, TriadQuality::Minor, spell, 0));

        float bestTwo = 0.0f;
        for (const auto& mid : pool)
        {
            if (mid.notes.empty())
                continue;
            NextChordCandidate probe;
            probe.chord = mid;
            NextChordScorer::score(fromCandidate, keyScale, probe, drama01, {});
            if (probe.metrics.coherence < 0.35f)
                continue;
            const float next = lookaheadProductivity(mid, keyScale, drama01);
            bestTwo = std::max(bestTwo, 0.55f * probe.metrics.coherence + 0.45f * next);
        }

        return std::clamp(0.55f * oneStep + 0.45f * bestTwo, 0.0f, 1.0f);
    }
}

std::vector<NextChordCandidate> NextChordGenerator::generate(const Chord& currentChord,
                                                             const KeyScaleData& keyScale,
                                                             float drama01,
                                                             const SequenceContext& sequence,
                                                             Pool pool,
                                                             const std::string& query,
                                                             int maxResults)
{
    if (currentChord.notes.empty())
    {
        if (pool == Pool::All)
            return generateCatalogue(keyScale, query, maxResults);
        return {};
    }

    std::vector<NextChordCandidate> candidates;
    std::set<VoicingKey> seenVoicings;

    auto consider = [&](NextChordCandidate candidate)
    {
        if (candidate.chord.notes.empty())
            return;
        if (sameVoicing(candidate.chord, currentChord))
            return;
        if (!seenVoicings.insert(voicingKey(candidate.chord)).second)
            return;
        if (!candidate.degree)
            candidate.degree = matchingDegree(candidate.chord, keyScale);
        candidates.push_back(std::move(candidate));
    };

    // 1) Mechanism-driven ideas.
    for (auto& c : MechanismCandidateGenerator::generate(currentChord, keyScale, sequence))
        consider(std::move(c));

    // 2) Full catalogue for coverage.
    for (const auto& chord : TriadLibrary::allTriads(keyScale.key))
    {
        NextChordCandidate candidate;
        candidate.chord = chord;
        candidate.degree = matchingDegree(chord, keyScale);
        consider(std::move(candidate));
    }

    // 3) Score every voicing (metrics + provisional ranking).
    NextChordScorer::scoreAndSort(currentChord, keyScale, candidates, drama01, sequence);

    // 4) Ensemble: AI expectedness + path value (top provisional only), re-finalize ranking.
    const auto aiMap = buildAiExpectednessMap(currentChord, keyScale, sequence);
    const float currentStanding = NextChordScorer::standingTension(currentChord, keyScale);

    // Path search is expensive — only enrich the strongest provisional candidates.
    std::vector<std::size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), 0);
    std::partial_sort(order.begin(),
                      order.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(48, order.size())),
                      order.end(),
                      [&](std::size_t a, std::size_t b) {
                          return candidates[a].rankingScore > candidates[b].rankingScore;
                      });
    std::set<std::size_t> pathEnrich;
    for (std::size_t i = 0; i < std::min<std::size_t>(48, order.size()); ++i)
        pathEnrich.insert(order[i]);

    const int currentRoot = NextChordScorer::rootPitchClass(currentChord);

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        auto& candidate = candidates[i];

        // Contextual spelling (Bb not A# as bVII in C major).
        candidate.chord = NextChordScorer::spellInKeyContext(candidate.chord, keyScale);

        // Re-assign idea family (power → diatonic triad family).
        NextChordScorer::assignIdeaFamily(candidate.chord, keyScale, currentRoot,
                                          candidate.familyRootPc, candidate.familyKind);

        const auto pcs = pitchClassSet(candidate.chord);
        if (const auto it = aiMap.find(pcs); it != aiMap.end())
            candidate.metrics.aiExpectedness = it->second;
        else
            candidate.metrics.aiExpectedness = 0.0f;

        if (pathEnrich.contains(i))
            candidate.metrics.pathValue = computePathValue(candidate.chord, keyScale, drama01);
        else
            candidate.metrics.pathValue = std::clamp(candidate.metrics.resolution * 0.7f, 0.0f, 1.0f);

        candidate.metrics.resolution = std::clamp(
            0.60f * candidate.metrics.resolution + 0.40f * candidate.metrics.pathValue, 0.0f, 1.0f);
        candidate.resolutionPercent =
            std::clamp(static_cast<int>(std::lround(candidate.metrics.resolution * 100.0f)), 0, 100);

        // Prolongations of the current root: heavy demotion (recolour, not a move).
        if (NextChordScorer::isProlongationOf(candidate.chord, currentChord))
            candidate.rankingScore -= 1.75f;

        // Incomplete sonorities as independent scores: demote hard before family pick.
        if (NextChordScorer::isIncompleteSonority(NextChordScorer::detectTriadQuality(candidate.chord)))
            candidate.rankingScore -= 1.25f;

        NextChordScorer::finalizeRanking(candidate, drama01, currentStanding);

        if (NextChordScorer::isProlongationOf(candidate.chord, currentChord))
            candidate.rankingScore -= 1.50f;
        if (NextChordScorer::isIncompleteSonority(NextChordScorer::detectTriadQuality(candidate.chord)))
            candidate.rankingScore -= 1.10f;
    }

    auto byRanking = [](const NextChordCandidate& a, const NextChordCandidate& b)
    {
        if (std::abs(a.rankingScore - b.rankingScore) > 1.0e-5f)
            return a.rankingScore > b.rankingScore;
        if (a.fitPercent != b.fitPercent)
            return a.fitPercent > b.fitPercent;
        if (std::abs(a.metrics.complexity - b.metrics.complexity) > 1.0e-5f)
            return a.metrics.complexity < b.metrics.complexity;
        return a.chord.symbol < b.chord.symbol;
    };

    // 5) Predicted: destination-first family collapse. All: full scored catalogue.
    std::vector<NextChordCandidate> result;
    if (pool == Pool::All)
    {
        result = std::move(candidates);
        // Empty query: root-position faces only (browsable). Non-empty query may match inversions
        // (e.g. "C/E") so keep every voicing that will be filtered by name/symbol next.
        if (query.empty())
        {
            result.erase(std::remove_if(result.begin(), result.end(),
                              [](const NextChordCandidate& c)
                              {
                                  if (c.chord.notes.empty())
                                      return true;
                                  const int root = NextChordScorer::rootPitchClass(c.chord);
                                  const int bass = NextChordScorer::bassPitchClass(c.chord);
                                  return root != bass;
                              }),
                         result.end());
        }
        std::stable_sort(result.begin(), result.end(), byRanking);
        applyQueryAndCap(result, query, maxResults);
        return result;
    }

    // Only complete-move candidates compete (no C5/Csus2/A5 as top-level faces).
    std::map<FamilyKey, std::size_t> bestIndexByFamily;
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        if (!isMainListIdea(candidates[i], currentChord))
            continue;

        const auto key = familyKeyOf(candidates[i]);
        const auto it = bestIndexByFamily.find(key);
        if (it == bestIndexByFamily.end())
        {
            bestIndexByFamily.emplace(key, i);
            continue;
        }
        if (representativeScore(candidates[i]) > representativeScore(candidates[it->second]) + 1.0e-5f)
            it->second = i;
        else if (std::abs(representativeScore(candidates[i]) - representativeScore(candidates[it->second]))
                     <= 1.0e-5f
                 && candidates[i].metrics.complexity < candidates[it->second].metrics.complexity)
        {
            it->second = i;
        }
    }

    result.reserve(bestIndexByFamily.size());
    for (const auto& [key, index] : bestIndexByFamily)
    {
        (void)key;
        result.push_back(std::move(candidates[index]));
    }

    // 6) Final destination order (moves only), then optional search filter.
    std::stable_sort(result.begin(), result.end(), byRanking);
    // Predicted lists are already short; only cap when the caller asks for a hard limit and
    // a query is active (avoid silently truncating the full suggestion column).
    applyQueryAndCap(result, query, query.empty() ? 0 : maxResults);
    return result;
}

std::vector<NextChordCandidate> NextChordGenerator::generateCatalogue(const KeyScaleData& keyScale,
                                                                      const std::string& query,
                                                                      int maxResults)
{
    std::vector<NextChordCandidate> candidates;
    candidates.reserve(static_cast<std::size_t>(TriadLibrary::kNumRootPositionChords));

    static constexpr TriadQuality kQualities[TriadLibrary::kNumQualities] = {
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

    for (int root = 0; root < TriadLibrary::kNumRoots; ++root)
    {
        for (const auto quality : kQualities)
        {
            NextChordCandidate candidate;
            candidate.chord = TriadLibrary::makeTriad(root, quality, keyScale.key, 0);
            candidate.chord = NextChordScorer::spellInKeyContext(candidate.chord, keyScale);
            candidate.degree = matchingDegree(candidate.chord, keyScale);
            candidate.rankingScore = 0.0f;
            candidates.push_back(std::move(candidate));
        }
    }

    std::stable_sort(candidates.begin(), candidates.end(),
        [](const NextChordCandidate& a, const NextChordCandidate& b)
        {
            return a.chord.readableName < b.chord.readableName;
        });

    applyQueryAndCap(candidates, query, maxResults);
    return candidates;
}

}
