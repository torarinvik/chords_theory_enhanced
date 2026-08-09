#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "Theory/ChordDatabase.h"
#include "Theory/ChordSeqAIModel.h"
#include "Theory/NextChordAiGenerator.h"
#include "Theory/Scale.h"
#include "Theory/TriadLibrary.h"

using theory::ChordSeqAIModel;
using theory::NextChordAiGenerator;
using theory::TriadLibrary;
using theory::TriadQuality;
using theory::Key;
using theory::Scale;

TEST_CASE("ChordSeqAIModel loads offline weights and vocab", "[ChordSeqAI][NextChord]")
{
    auto& model = ChordSeqAIModel::getInstance();
    REQUIRE(model.isReady());
    REQUIRE(model.loadError().empty());
    REQUIRE(NextChordAiGenerator::isAvailable());
}

TEST_CASE("ChordSeqAIModel maps common symbols to known tokens", "[ChordSeqAI][NextChord]")
{
    auto& model = ChordSeqAIModel::getInstance();
    REQUIRE(model.isReady());

    REQUIRE(model.tokenForSymbol("C") == 685);
    REQUIRE(model.tokenForSymbol("G") == 162);
    REQUIRE(model.tokenForSymbol("Am") == 677);
    REQUIRE(model.tokenForSymbol("F") == 654);
    REQUIRE(model.tokenForSymbol("Em") == 34);

    const auto c = model.chordForToken(685);
    REQUIRE(c.has_value());
    REQUIRE(c->readableName == "C");
    REQUIRE_FALSE(c->notes.empty());
}

TEST_CASE("ChordSeqAIModel after C ranks F/G/Am highly", "[ChordSeqAI][NextChord]")
{
    auto& model = ChordSeqAIModel::getInstance();
    REQUIRE(model.isReady());

    // Matches chord-seq-ai-app / ORT ground truth for [START, C].
    const auto top = model.predictTopK({ 685 }, 10, true);
    REQUIRE(top.size() >= 5);

    std::vector<std::string> names;
    for (const auto& p : top)
        names.push_back(p.primaryName);

    // Top should include the classic C-major diatonic next chords.
    const auto has = [&](const std::string& n)
    {
        return std::find(names.begin(), names.end(), n) != names.end();
    };
    CHECK(has("F"));
    CHECK(has("G"));
    CHECK(has("Am"));

    // F or G should be #1 or #2.
    REQUIRE((names[0] == "F" || names[0] == "G" || names[1] == "F" || names[1] == "G"));

    // Probabilities sum-ish ordered.
    for (size_t i = 1; i < top.size(); ++i)
        CHECK(top[i - 1].probability >= top[i].probability - 1.0e-6f);
}

TEST_CASE("ChordSeqAIModel tokenForChord works from TriadLibrary C major", "[ChordSeqAI][NextChord]")
{
    auto& model = ChordSeqAIModel::getInstance();
    REQUIRE(model.isReady());

    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto token = model.tokenForChord(c);
    REQUIRE(token.has_value());
    // Prefer exact "C" token when readable name matches.
    CHECK(*token == 685);
}

TEST_CASE("NextChordAiGenerator produces hybrid candidates from C", "[ChordSeqAI][NextChord]")
{
    REQUIRE(NextChordAiGenerator::isAvailable());

    const auto& keyScale = theory::ChordDatabase::getInstance().get(Key::C, Scale::Major);

    const auto c = TriadLibrary::makeTriad(0, TriadQuality::Major, Key::C);
    const auto candidates = NextChordAiGenerator::generate(c, keyScale, 0.35f, {}, 16);
    REQUIRE_FALSE(candidates.empty());

    std::set<std::string> names;
    for (const auto& cand : candidates)
    {
        names.insert(cand.chord.readableName);
        CHECK_FALSE(cand.chord.notes.empty());
        // AI confidence tag present in reason label.
        CHECK(cand.reasonLabel.find("AI") != std::string::npos);
    }

    // Expect familiar diatonic follow-ups among AI suggestions after C.
    const bool hasCore = names.count("F") || names.count("G") || names.count("Am")
        || names.count("Em") || names.count("Dm");
    CHECK(hasCore);
}
