#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Theory/Chord.h"

namespace theory
{

// Offline ChordSeqAI recurrent_net (3×GRU, hidden=96, vocab=1035).
// Weights + vocab are bundled BinaryData — no network, no ONNX Runtime.
//
// Sequence convention (matches chord-seq-ai-app):
//   [START, token0, token1, …]  (no END token in the input)
// Logits are taken at the last provided token position.
class ChordSeqAIModel
{
public:
    static constexpr int kVocabSize = 1035;
    static constexpr int kChordTokenCount = 1033;
    static constexpr int kStartToken = 1033;
    static constexpr int kEndToken = 1034;
    static constexpr int kHiddenSize = 96;
    static constexpr int kMaxSequenceLength = 256;
    static constexpr int kNumGruLayers = 3;

    struct TokenPrediction
    {
        int token = 0;
        float probability = 0.0f;
        float logit = 0.0f;
        std::string primaryName; // first alias in the vocab
    };

    // Process-wide singleton; loads BinaryData on first use.
    static ChordSeqAIModel& getInstance();

    [[nodiscard]] bool isReady() const { return _ready; }
    [[nodiscard]] const std::string& loadError() const { return _loadError; }

    // Map a chord symbol (e.g. "C", "Am", "G7", "C/E") to a model token, if known.
    [[nodiscard]] std::optional<int> tokenForSymbol(const std::string& symbol) const;

    // Best-effort: exact symbol match, then pitch-class multiset match against vocab notes.
    [[nodiscard]] std::optional<int> tokenForChord(const Chord& chord) const;

    // Build a playable Chord for a model token (primary name + chord_to_notes voicing).
    [[nodiscard]] std::optional<Chord> chordForToken(int token) const;

    [[nodiscard]] const std::vector<std::string>& namesForToken(int token) const;

    // Forward: tokens must already include START as first entry (or pass raw chord tokens
    // with prependStart=true). Returns full vocab logits at the last position.
    // Returns empty on failure / empty input.
    [[nodiscard]] std::vector<float> predictLogits(const std::vector<int>& tokens,
                                                   bool prependStart = true) const;

    // Softmax top-K over chord tokens only (0..1032). Masks START/END and optionally
    // the last input chord token (same as the web app).
    [[nodiscard]] std::vector<TokenPrediction> predictTopK(const std::vector<int>& chordTokens,
                                                           int topK = 32,
                                                           bool maskLastInputToken = true) const;

private:
    ChordSeqAIModel();
    bool loadFromBinaryData();
    bool loadWeights(const void* data, int size);
    bool loadVocabJson(const std::string& jsonText);

    void layerNorm(const float* x, const float* gamma, const float* beta, float* y) const;
    void gruStep(const float* x, float* h, const float* W, const float* R, const float* B) const;
    void matmulBias(const float* x, const float* W, const float* bias, int inDim, int outDim, float* y) const;

    bool _ready = false;
    std::string _loadError;

    float _lnEps = 1.0e-5f;
    int _linearBeforeReset = 0;

    // Weight storage (owned).
    std::vector<float> _storage;
    const float* _emb = nullptr;       // [vocab, hidden]
    const float* _lnW[kNumGruLayers] {};
    const float* _lnB[kNumGruLayers] {};
    const float* _gruW[kNumGruLayers] {}; // [1, 3H, H] flattened
    const float* _gruR[kNumGruLayers] {};
    const float* _gruB[kNumGruLayers] {}; // [1, 6H]
    const float* _mlpW1 = nullptr;     // [H, H]
    const float* _mlpB1 = nullptr;     // [H]
    float _preluSlope = 0.0f;
    const float* _mlpW2 = nullptr;     // [H, vocab]
    const float* _mlpB2 = nullptr;     // [vocab]

    std::vector<std::vector<std::string>> _tokenNames; // size chordTokenCount
    std::unordered_map<std::string, int> _symbolToToken;
    std::unordered_map<std::string, std::vector<int>> _chordToMidi; // name → MIDI notes
    // Pitch-class multiset signature (sorted 0-11, with multiplicity) → token (first seen).
    std::unordered_map<std::string, int> _pcSignatureToToken;

    static std::string pitchClassSignature(const std::vector<int>& midiNotes);
    static std::string pitchClassSignatureFromChord(const Chord& chord);
    static std::string midiToNoteName(int midi);
    static int parseRootPitchClass(const std::string& chordName);
};

}
