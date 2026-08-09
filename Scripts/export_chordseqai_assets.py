#!/usr/bin/env python3
"""Export ChordSeqAI recurrent_net weights + vocab for offline C++ inference.

Requires: pip install onnx onnxruntime numpy
Source model (default): /tmp/chord-seq-ai-app/public/models/recurrent_net.onnx
  clone: https://github.com/PetrIvan/chord-seq-ai-app

Outputs (under Assets/):
  Data/chordseqai_weights.bin
  Data/chordseqai_vocab.json
  ThirdParty/ChordSeqAI_NOTICE.txt

Binary format (little-endian):
  magic[4]='CSAI', version u32=1, vocabSize u32, hidden u32, numLayers u32=3,
  linearBeforeReset u32 (0 = formula matching ORT for this export), lnEps f32,
  then float32 arrays in fixed order (see C++ ChordSeqAIModel).
"""

from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

import numpy as np


def parse_args() -> argparse.Namespace:
    root = Path(__file__).resolve().parents[1]
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "--onnx",
        type=Path,
        default=Path("/tmp/chord-seq-ai-app/public/models/recurrent_net.onnx"),
    )
    p.add_argument(
        "--token-ts",
        type=Path,
        default=Path("/tmp/chord-seq-ai-app/src/data/token_to_chord.ts"),
    )
    p.add_argument(
        "--notes-ts",
        type=Path,
        default=Path("/tmp/chord-seq-ai-app/src/data/chord_to_notes.ts"),
    )
    p.add_argument(
        "--license",
        type=Path,
        default=Path("/tmp/chord-seq-ai-app/LICENSE.txt"),
    )
    p.add_argument("--out-data", type=Path, default=root / "Assets" / "Data")
    p.add_argument("--out-notice", type=Path, default=root / "Assets" / "ThirdParty" / "ChordSeqAI_NOTICE.txt")
    return p.parse_args()


def main() -> int:
    args = parse_args()
    try:
        import onnx
        from onnx import numpy_helper
    except ImportError:
        print("Need onnx: pip install onnx", file=sys.stderr)
        return 1

    if not args.onnx.is_file():
        print(f"ONNX not found: {args.onnx}", file=sys.stderr)
        return 1

    m = onnx.load(str(args.onnx))
    W = {init.name: numpy_helper.to_array(init).astype(np.float32) for init in m.graph.initializer}

    # The exported recurrent_net.onnx reports linear_before_reset=1, but the
    # ONNX Runtime outputs match the linear_before_reset=0 formula. Store 0 so
    # the C++ path matches ORT / the web app.
    formula_lbr = 0
    eps = 1e-5
    for n in m.graph.node:
        if n.op_type == "LayerNormalization":
            for a in n.attribute:
                if a.name == "epsilon":
                    eps = float(a.f)

    emb = W["chord_embeddings.weight"]
    vocab_size, hidden = emb.shape
    assert vocab_size == 1035 and hidden == 96

    parts = [
        emb,
        W["gru.0.ln.weight"],
        W["gru.0.ln.bias"],
        W["onnx::GRU_289"],
        W["onnx::GRU_290"],
        W["onnx::GRU_291"],
        W["gru.1.ln.weight"],
        W["gru.1.ln.bias"],
        W["onnx::GRU_310"],
        W["onnx::GRU_311"],
        W["onnx::GRU_312"],
        W["gru.2.ln.weight"],
        W["gru.2.ln.bias"],
        W["onnx::GRU_331"],
        W["onnx::GRU_332"],
        W["onnx::GRU_333"],
        W["onnx::MatMul_335"],
        W["mlp.0.bias"],
        W["onnx::PRelu_336"].reshape(-1),
        W["onnx::MatMul_337"],
        W["mlp.2.bias"],
    ]

    header = struct.pack(
        "<4sIIIIIf",
        b"CSAI",
        1,
        vocab_size,
        hidden,
        3,
        formula_lbr,
        float(eps),
    )
    blob = bytearray(header)
    for arr in parts:
        flat = np.ascontiguousarray(arr, dtype=np.float32).ravel()
        blob.extend(flat.tobytes())

    args.out_data.mkdir(parents=True, exist_ok=True)
    weights_path = args.out_data / "chordseqai_weights.bin"
    weights_path.write_bytes(blob)
    print(f"wrote {weights_path} ({len(blob)} bytes)")

    token_text = args.token_ts.read_text(encoding="utf-8")
    token_pairs = re.findall(r'"(\d+)":\s*\[(.*?)\]', token_text, re.S)
    token_to_names = {int(tid): re.findall(r'"([^"]+)"', body) for tid, body in token_pairs}
    assert len(token_to_names) == 1033

    notes_text = args.notes_ts.read_text(encoding="utf-8")
    entries = re.findall(
        r'(?:^|\n)\s*(?:"([^"]+)"|([A-Za-z0-9#/()+b\-\.]+))\s*:\s*\[([^\]]+)\]',
        notes_text,
    )
    chord_to_notes = {}
    for quoted, bare, nums in entries:
        name = quoted or bare
        chord_to_notes[name] = [int(x.strip()) for x in nums.split(",") if x.strip()]

    vocab = {
        "source": "ChordSeqAI recurrent_net (Student Trainee Center / PetrIvan)",
        "license": "MIT",
        "vocabSize": 1035,
        "chordTokenCount": 1033,
        "startToken": 1033,
        "endToken": 1034,
        "hiddenSize": 96,
        "maxSequenceLength": 256,
        "tokenToNames": {str(k): v for k, v in sorted(token_to_names.items())},
        "chordToNotes": chord_to_notes,
    }
    vocab_path = args.out_data / "chordseqai_vocab.json"
    vocab_path.write_text(json.dumps(vocab, separators=(",", ":"), ensure_ascii=False), encoding="utf-8")
    print(f"wrote {vocab_path} ({vocab_path.stat().st_size} bytes)")

    license_text = args.license.read_text(encoding="utf-8") if args.license.is_file() else "MIT"
    args.out_notice.parent.mkdir(parents=True, exist_ok=True)
    args.out_notice.write_text(
        "ChordSeqAI model assets (recurrent_net weights + chord vocabulary)\n"
        "================================================================\n"
        "Source project: https://github.com/PetrIvan/chord-seq-ai-app\n"
        "Related research/code: https://github.com/StudentTraineeCenter/chord-seq-ai\n"
        "License: MIT (see below)\n\n"
        "The bundled chordseqai_weights.bin was exported from the publicly distributed\n"
        "recurrent_net.onnx model. The vocabulary (chordseqai_vocab.json) is derived\n"
        "from token_to_chord.ts and chord_to_notes.ts in the same project.\n\n"
        "Used offline-only for next-chord suggestions inside Chords Theory Enhanced.\n"
        "No network calls are made at runtime.\n\n"
        "---------------------------------------------------------------------------\n"
        + license_text,
        encoding="utf-8",
    )
    print(f"wrote {args.out_notice}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
