#include "Theory/ChordSeqAIModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>
#include <sstream>

#include <juce_core/juce_core.h>

#include "BinaryData.h"
#include "Theory/ChordType.h"
#include "Theory/NoteConvertor.h"

namespace theory
{

namespace
{
#pragma pack(push, 1)
    struct WeightsHeader
    {
        char magic[4];
        uint32_t version = 0;
        uint32_t vocabSize = 0;
        uint32_t hidden = 0;
        uint32_t numLayers = 0;
        uint32_t linearBeforeReset = 0;
        float lnEps = 0.0f;
    };
#pragma pack(pop)

    static_assert(sizeof(WeightsHeader) == 28, "ChordSeqAI weights header size");
}

ChordSeqAIModel& ChordSeqAIModel::getInstance()
{
    static ChordSeqAIModel instance;
    return instance;
}

ChordSeqAIModel::ChordSeqAIModel()
{
    if (!loadFromBinaryData())
    {
        _ready = false;
        if (_loadError.empty())
            _loadError = "ChordSeqAI model failed to load";
    }
}

bool ChordSeqAIModel::loadFromBinaryData()
{
    if (BinaryData::chordseqai_weights_bin == nullptr || BinaryData::chordseqai_weights_binSize <= 0)
    {
        _loadError = "Missing chordseqai_weights.bin BinaryData";
        return false;
    }
    if (BinaryData::chordseqai_vocab_json == nullptr || BinaryData::chordseqai_vocab_jsonSize <= 0)
    {
        _loadError = "Missing chordseqai_vocab.json BinaryData";
        return false;
    }

    if (!loadWeights(BinaryData::chordseqai_weights_bin, BinaryData::chordseqai_weights_binSize))
        return false;

    const auto jsonText = juce::String::createStringFromData(BinaryData::chordseqai_vocab_json,
                                                             BinaryData::chordseqai_vocab_jsonSize);
    if (!loadVocabJson(jsonText.toStdString()))
        return false;

    _ready = true;
    return true;
}

bool ChordSeqAIModel::loadWeights(const void* data, int size)
{
    if (data == nullptr || size < static_cast<int>(sizeof(WeightsHeader)))
    {
        _loadError = "Weights blob too small";
        return false;
    }

    WeightsHeader header {};
    std::memcpy(&header, data, sizeof(header));
    if (std::memcmp(header.magic, "CSAI", 4) != 0)
    {
        _loadError = "Bad weights magic";
        return false;
    }
    if (header.version != 1
        || header.vocabSize != static_cast<uint32_t>(kVocabSize)
        || header.hidden != static_cast<uint32_t>(kHiddenSize)
        || header.numLayers != static_cast<uint32_t>(kNumGruLayers))
    {
        _loadError = "Unsupported weights header dimensions";
        return false;
    }

    _linearBeforeReset = static_cast<int>(header.linearBeforeReset);
    _lnEps = header.lnEps > 0.0f ? header.lnEps : 1.0e-5f;

    const auto* bytes = static_cast<const uint8_t*>(data) + sizeof(WeightsHeader);
    const int payloadBytes = size - static_cast<int>(sizeof(WeightsHeader));
    if (payloadBytes % 4 != 0)
    {
        _loadError = "Weights payload not float-aligned";
        return false;
    }

    const int floatCount = payloadBytes / 4;
    _storage.resize(static_cast<size_t>(floatCount));
    std::memcpy(_storage.data(), bytes, static_cast<size_t>(payloadBytes));

    const int H = kHiddenSize;
    const int V = kVocabSize;
    // Expected floats:
    // emb V*H
    // per layer: lnW H + lnB H + W 3H*H + R 3H*H + B 6H
    // mlp W1 H*H + b1 H + prelu 1 + W2 H*V + b2 V
    const int perLayer = H + H + (3 * H * H) + (3 * H * H) + (6 * H);
    const int expected = V * H + kNumGruLayers * perLayer + (H * H) + H + 1 + (H * V) + V;
    if (floatCount != expected)
    {
        _loadError = "Unexpected weights float count: " + std::to_string(floatCount)
            + " (expected " + std::to_string(expected) + ")";
        return false;
    }

    const float* p = _storage.data();
    auto take = [&](int n) -> const float*
    {
        const float* out = p;
        p += n;
        return out;
    };

    _emb = take(V * H);
    for (int layer = 0; layer < kNumGruLayers; ++layer)
    {
        _lnW[layer] = take(H);
        _lnB[layer] = take(H);
        _gruW[layer] = take(3 * H * H);
        _gruR[layer] = take(3 * H * H);
        _gruB[layer] = take(6 * H);
    }
    _mlpW1 = take(H * H);
    _mlpB1 = take(H);
    _preluSlope = *take(1);
    _mlpW2 = take(H * V);
    _mlpB2 = take(V);

    jassert(p == _storage.data() + floatCount);
    return true;
}

bool ChordSeqAIModel::loadVocabJson(const std::string& jsonText)
{
    const auto parsed = juce::JSON::parse(jsonText);
    if (!parsed.isObject())
    {
        _loadError = "Vocab JSON parse failed";
        return false;
    }

    auto* root = parsed.getDynamicObject();
    if (root == nullptr)
    {
        _loadError = "Vocab JSON root missing";
        return false;
    }

    _tokenNames.assign(static_cast<size_t>(kChordTokenCount), {});
    _symbolToToken.clear();
    _chordToMidi.clear();
    _pcSignatureToToken.clear();

    const auto tokenToNames = root->getProperty("tokenToNames");
    if (auto* tokensObj = tokenToNames.getDynamicObject())
    {
        for (const auto& prop : tokensObj->getProperties())
        {
            const int token = prop.name.toString().getIntValue();
            if (token < 0 || token >= kChordTokenCount)
                continue;

            std::vector<std::string> names;
            if (auto* arr = prop.value.getArray())
            {
                for (const auto& v : *arr)
                    names.push_back(v.toString().toStdString());
            }
            _tokenNames[static_cast<size_t>(token)] = names;
            for (const auto& name : names)
            {
                if (!name.empty() && _symbolToToken.find(name) == _symbolToToken.end())
                    _symbolToToken.emplace(name, token);
            }
        }
    }
    else
    {
        _loadError = "tokenToNames missing";
        return false;
    }

    const auto chordToNotes = root->getProperty("chordToNotes");
    if (auto* notesObj = chordToNotes.getDynamicObject())
    {
        for (const auto& prop : notesObj->getProperties())
        {
            const auto name = prop.name.toString().toStdString();
            std::vector<int> midi;
            if (auto* arr = prop.value.getArray())
            {
                for (const auto& v : *arr)
                    midi.push_back(static_cast<int>(v));
            }
            if (!midi.empty())
            {
                _chordToMidi.emplace(name, midi);
                const auto sig = pitchClassSignature(midi);
                if (_pcSignatureToToken.find(sig) == _pcSignatureToToken.end())
                {
                    // Prefer mapping the primary symbol's token when available.
                    if (const auto it = _symbolToToken.find(name); it != _symbolToToken.end())
                        _pcSignatureToToken.emplace(sig, it->second);
                }
            }
        }
    }
    else
    {
        _loadError = "chordToNotes missing";
        return false;
    }

    // Ensure every token with known notes has a PC signature → token entry.
    for (int token = 0; token < kChordTokenCount; ++token)
    {
        const auto& names = _tokenNames[static_cast<size_t>(token)];
        if (names.empty())
            continue;
        const auto notesIt = _chordToMidi.find(names.front());
        if (notesIt == _chordToMidi.end())
            continue;
        const auto sig = pitchClassSignature(notesIt->second);
        if (_pcSignatureToToken.find(sig) == _pcSignatureToToken.end())
            _pcSignatureToToken.emplace(sig, token);
    }

    return true;
}

std::string ChordSeqAIModel::pitchClassSignature(const std::vector<int>& midiNotes)
{
    std::vector<int> pcs;
    pcs.reserve(midiNotes.size());
    for (int m : midiNotes)
        pcs.push_back(((m % 12) + 12) % 12);
    std::sort(pcs.begin(), pcs.end());
    std::ostringstream oss;
    for (size_t i = 0; i < pcs.size(); ++i)
    {
        if (i)
            oss << ',';
        oss << pcs[i];
    }
    return oss.str();
}

std::string ChordSeqAIModel::pitchClassSignatureFromChord(const Chord& chord)
{
    std::vector<int> pcs;
    pcs.reserve(chord.notes.size());
    for (const auto& n : chord.notes)
        pcs.push_back(n.getPitchClass());
    std::sort(pcs.begin(), pcs.end());
    std::ostringstream oss;
    for (size_t i = 0; i < pcs.size(); ++i)
    {
        if (i)
            oss << ',';
        oss << pcs[i];
    }
    return oss.str();
}

std::string ChordSeqAIModel::midiToNoteName(int midi)
{
    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    return kNames[((midi % 12) + 12) % 12];
}

int ChordSeqAIModel::parseRootPitchClass(const std::string& chordName)
{
    if (chordName.empty())
        return 0;

    // Strip bass slash for root: "C/E" → "C", "F#m7/A" → "F#m7"
    std::string head = chordName;
    if (const auto slash = head.find('/'); slash != std::string::npos)
        head = head.substr(0, slash);

    // Leading pitch class: letter + optional #/b
    if (head.empty())
        return 0;
    std::string root;
    root.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(head[0]))));
    size_t i = 1;
    if (i < head.size() && (head[i] == '#' || head[i] == 'b'))
    {
        root.push_back(head[i]);
        ++i;
    }
    return NoteConvertor::parsePitchClass(root);
}

std::optional<int> ChordSeqAIModel::tokenForSymbol(const std::string& symbol) const
{
    if (!_ready || symbol.empty())
        return std::nullopt;
    if (const auto it = _symbolToToken.find(symbol); it != _symbolToToken.end())
        return it->second;
    return std::nullopt;
}

std::optional<int> ChordSeqAIModel::tokenForChord(const Chord& chord) const
{
    if (!_ready)
        return std::nullopt;

    if (auto t = tokenForSymbol(chord.readableName))
        return t;
    if (auto t = tokenForSymbol(chord.symbol))
        return t;

    // Try without spaces and common aliases.
    auto compact = chord.readableName;
    compact.erase(std::remove(compact.begin(), compact.end(), ' '), compact.end());
    if (auto t = tokenForSymbol(compact))
        return t;

    const auto sig = pitchClassSignatureFromChord(chord);
    if (const auto it = _pcSignatureToToken.find(sig); it != _pcSignatureToToken.end())
        return it->second;

    return std::nullopt;
}

const std::vector<std::string>& ChordSeqAIModel::namesForToken(int token) const
{
    static const std::vector<std::string> kEmpty;
    if (token < 0 || token >= kChordTokenCount)
        return kEmpty;
    return _tokenNames[static_cast<size_t>(token)];
}

std::optional<Chord> ChordSeqAIModel::chordForToken(int token) const
{
    if (!_ready || token < 0 || token >= kChordTokenCount)
        return std::nullopt;

    const auto& names = _tokenNames[static_cast<size_t>(token)];
    if (names.empty())
        return std::nullopt;

    const std::string& primary = names.front();
    auto notesIt = _chordToMidi.find(primary);
    if (notesIt == _chordToMidi.end())
    {
        // Fall back through aliases.
        for (const auto& name : names)
        {
            notesIt = _chordToMidi.find(name);
            if (notesIt != _chordToMidi.end())
                break;
        }
    }
    if (notesIt == _chordToMidi.end())
        return std::nullopt;

    const auto& midi = notesIt->second;
    const int rootPc = parseRootPitchClass(primary);

    Chord chord;
    chord.symbol = primary;
    chord.readableName = primary;
    chord.popularityOrder = 1;
    chord.type = (midi.size() >= 4 ? ChordType::Seventh
                  : (primary.find("sus4") != std::string::npos ? ChordType::Sus4
                     : (primary.find("sus2") != std::string::npos ? ChordType::Sus2
                        : (primary.find('5') != std::string::npos && primary.size() <= 3
                               ? ChordType::Power
                               : ChordType::Triad))));

    // Bass-first voicing from ChordSeqAI; assign chord-tone roles so root detection works.
    int nextRole = 3;
    chord.notes.reserve(midi.size());
    for (int m : midi)
    {
        NoteName note;
        note.rawNote = midiToNoteName(m);
        note.readableNote = note.rawNote;
        const int pc = ((m % 12) + 12) % 12;
        if (pc == rootPc)
            note.positionInChord = 1;
        else
        {
            note.positionInChord = nextRole;
            nextRole = (nextRole == 3 ? 5 : (nextRole == 5 ? 7 : nextRole + 2));
        }
        chord.notes.push_back(std::move(note));
    }

    // If no note matched root (odd enharmonic), mark first as root role.
    bool hasRoot = false;
    for (const auto& n : chord.notes)
        if (n.positionInChord == 1)
            hasRoot = true;
    if (!hasRoot && !chord.notes.empty())
        chord.notes.front().positionInChord = 1;

    return chord;
}

void ChordSeqAIModel::layerNorm(const float* x, const float* gamma, const float* beta, float* y) const
{
    float mean = 0.0f;
    for (int i = 0; i < kHiddenSize; ++i)
        mean += x[i];
    mean /= static_cast<float>(kHiddenSize);

    float var = 0.0f;
    for (int i = 0; i < kHiddenSize; ++i)
    {
        const float d = x[i] - mean;
        var += d * d;
    }
    var /= static_cast<float>(kHiddenSize);
    const float inv = 1.0f / std::sqrt(var + _lnEps);

    for (int i = 0; i < kHiddenSize; ++i)
        y[i] = gamma[i] * (x[i] - mean) * inv + beta[i];
}

void ChordSeqAIModel::gruStep(const float* x, float* h, const float* W, const float* R, const float* B) const
{
    const int H = kHiddenSize;
    // W/R: [3H, H] row-major blocks (z, r, n), B: [6H] = Wbz|Wbr|Wbn|Rbz|Rbr|Rbn
    const float* Wz = W;
    const float* Wr = W + H * H;
    const float* Wh = W + 2 * H * H;
    const float* Rz = R;
    const float* Rr = R + H * H;
    const float* Rh = R + 2 * H * H;
    const float* Wbz = B;
    const float* Wbr = B + H;
    const float* Wbh = B + 2 * H;
    const float* Rbz = B + 3 * H;
    const float* Rbr = B + 4 * H;
    const float* Rbh = B + 5 * H;

    float zt[kHiddenSize];
    float rt[kHiddenSize];
    float ht[kHiddenSize];
    float hPrev[kHiddenSize];
    std::memcpy(hPrev, h, sizeof(float) * static_cast<size_t>(H));

    for (int i = 0; i < H; ++i)
    {
        float xz = Wbz[i] + Rbz[i];
        float xr = Wbr[i] + Rbr[i];
        const float* wzRow = Wz + i * H;
        const float* wrRow = Wr + i * H;
        const float* rzRow = Rz + i * H;
        const float* rrRow = Rr + i * H;
        for (int j = 0; j < H; ++j)
        {
            xz += wzRow[j] * x[j] + rzRow[j] * hPrev[j];
            xr += wrRow[j] * x[j] + rrRow[j] * hPrev[j];
        }
        zt[i] = 1.0f / (1.0f + std::exp(-xz));
        rt[i] = 1.0f / (1.0f + std::exp(-xr));
    }

    if (_linearBeforeReset != 0)
    {
        // ht = tanh(x·Whᵀ + (r⊙h)·Rhᵀ + Rbh + Wbh)
        float rh[kHiddenSize];
        for (int j = 0; j < H; ++j)
            rh[j] = rt[j] * hPrev[j];
        for (int i = 0; i < H; ++i)
        {
            float val = Wbh[i] + Rbh[i];
            const float* whRow = Wh + i * H;
            const float* rhRow = Rh + i * H;
            for (int j = 0; j < H; ++j)
                val += whRow[j] * x[j] + rhRow[j] * rh[j];
            ht[i] = std::tanh(val);
        }
    }
    else
    {
        // linear_before_reset=0 (matches ORT for this export):
        // ht = tanh(x·Whᵀ + r ⊙ (h·Rhᵀ + Rbh) + Wbh)
        for (int i = 0; i < H; ++i)
        {
            float xh = Wbh[i];
            float hh = Rbh[i];
            const float* whRow = Wh + i * H;
            const float* rhRow = Rh + i * H;
            for (int j = 0; j < H; ++j)
            {
                xh += whRow[j] * x[j];
                hh += rhRow[j] * hPrev[j];
            }
            ht[i] = std::tanh(xh + rt[i] * hh);
        }
    }

    for (int i = 0; i < H; ++i)
        h[i] = (1.0f - zt[i]) * ht[i] + zt[i] * hPrev[i];
}

void ChordSeqAIModel::matmulBias(const float* x, const float* W, const float* bias,
                                 int inDim, int outDim, float* y) const
{
    // y = x @ W + bias, W is [inDim, outDim] row-major (numpy: shape (in, out))
    for (int o = 0; o < outDim; ++o)
    {
        float sum = bias != nullptr ? bias[o] : 0.0f;
        for (int i = 0; i < inDim; ++i)
            sum += x[i] * W[i * outDim + o];
        y[o] = sum;
    }
}

std::vector<float> ChordSeqAIModel::predictLogits(const std::vector<int>& tokens, bool prependStart) const
{
    if (!_ready)
        return {};

    std::vector<int> seq;
    seq.reserve(tokens.size() + 1);
    if (prependStart)
        seq.push_back(kStartToken);
    for (int t : tokens)
    {
        if (t < 0 || t >= kVocabSize)
            continue;
        // Skip consecutive duplicates (web app behaviour).
        if (!seq.empty() && seq.back() == t)
            continue;
        seq.push_back(t);
    }

    if (seq.empty())
        return {};
    if (static_cast<int>(seq.size()) > kMaxSequenceLength)
        seq.resize(static_cast<size_t>(kMaxSequenceLength));

    const int H = kHiddenSize;
    float hState[kNumGruLayers][kHiddenSize];
    for (int layer = 0; layer < kNumGruLayers; ++layer)
        std::fill(hState[layer], hState[layer] + H, 0.0f);

    float x[kHiddenSize];
    float y[kHiddenSize];
    float lastHidden[kHiddenSize];

    for (int t : seq)
    {
        // Embedding row.
        const float* embRow = _emb + t * H;
        std::memcpy(x, embRow, sizeof(float) * static_cast<size_t>(H));

        for (int layer = 0; layer < kNumGruLayers; ++layer)
        {
            layerNorm(x, _lnW[layer], _lnB[layer], y);
            gruStep(y, hState[layer], _gruW[layer], _gruR[layer], _gruB[layer]);
            std::memcpy(x, hState[layer], sizeof(float) * static_cast<size_t>(H));
        }
        std::memcpy(lastHidden, x, sizeof(float) * static_cast<size_t>(H));
    }

    // MLP head on last position.
    float mid[kHiddenSize];
    matmulBias(lastHidden, _mlpW1, _mlpB1, H, H, mid);
    for (int i = 0; i < H; ++i)
        mid[i] = mid[i] >= 0.0f ? mid[i] : _preluSlope * mid[i];

    std::vector<float> logits(static_cast<size_t>(kVocabSize));
    matmulBias(mid, _mlpW2, _mlpB2, H, kVocabSize, logits.data());
    return logits;
}

std::vector<ChordSeqAIModel::TokenPrediction> ChordSeqAIModel::predictTopK(
    const std::vector<int>& chordTokens, int topK, bool maskLastInputToken) const
{
    if (!_ready || topK <= 0)
        return {};

    const auto logits = predictLogits(chordTokens, true);
    if (logits.empty())
        return {};

    std::vector<float> scores = logits;
    // Mask START/END
    scores[static_cast<size_t>(kStartToken)] = -std::numeric_limits<float>::infinity();
    scores[static_cast<size_t>(kEndToken)] = -std::numeric_limits<float>::infinity();
    if (maskLastInputToken && !chordTokens.empty())
    {
        const int last = chordTokens.back();
        if (last >= 0 && last < kVocabSize)
            scores[static_cast<size_t>(last)] = -std::numeric_limits<float>::infinity();
    }

    // Softmax over finite chord tokens.
    float maxLogit = -std::numeric_limits<float>::infinity();
    for (int i = 0; i < kChordTokenCount; ++i)
        maxLogit = std::max(maxLogit, scores[static_cast<size_t>(i)]);

    if (!std::isfinite(maxLogit))
        return {};

    double sum = 0.0;
    std::vector<float> probs(static_cast<size_t>(kChordTokenCount));
    for (int i = 0; i < kChordTokenCount; ++i)
    {
        const float s = scores[static_cast<size_t>(i)];
        const float e = std::isfinite(s) ? std::exp(s - maxLogit) : 0.0f;
        probs[static_cast<size_t>(i)] = e;
        sum += static_cast<double>(e);
    }
    if (sum <= 0.0)
        return {};
    for (int i = 0; i < kChordTokenCount; ++i)
        probs[static_cast<size_t>(i)] = static_cast<float>(
            static_cast<double>(probs[static_cast<size_t>(i)]) / sum);

    std::vector<int> order(static_cast<size_t>(kChordTokenCount));
    std::iota(order.begin(), order.end(), 0);
    const int limit = std::min(topK, kChordTokenCount);
    std::partial_sort(order.begin(), order.begin() + limit, order.end(),
                      [&](int a, int b)
                      {
                          return probs[static_cast<size_t>(a)] > probs[static_cast<size_t>(b)];
                      });

    std::vector<TokenPrediction> out;
    out.reserve(static_cast<size_t>(limit));
    for (int i = 0; i < limit; ++i)
    {
        const int token = order[static_cast<size_t>(i)];
        TokenPrediction p;
        p.token = token;
        p.probability = probs[static_cast<size_t>(token)];
        p.logit = logits[static_cast<size_t>(token)];
        const auto& names = namesForToken(token);
        p.primaryName = names.empty() ? ("#" + std::to_string(token)) : names.front();
        out.push_back(std::move(p));
    }
    return out;
}

}
