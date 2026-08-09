# Changelog — Chords Theory Enhanced

Hand-written log of **this fork’s** work on top of upstream
[halbehers/chords_theory](https://github.com/halbehers/chords_theory).
Use this to resume development without re-discovering context.

**Branch:** `work`  
**Product name:** Chords Theory Enhanced (© 2026 fork packaging)  
**Repo path:** `chords_theory` → symlink to `chords_theory_enhanced`

---

## Handoff for next session (read this first)

### What works now

- Standalone / AU / VST3 **Chords Theory Enhanced** builds.
- **Next chords** panel: ranked suggestions from current chord, with:
  - Catalogue: maj / min / dim / aug / sus2 / sus4 / power / maj7 / m7 / dom7 / m7b5 (132 named → **112 unique** PC sets after dedupe).
  - **Drama** slider (Smooth ↔ Wild): reorders list toward a tension target; bar still shows objective tension %.
  - **Sequence memory**: previous progression chords feed the scorer (ii–V–I, 5ths chains, repeats, pop loop, Andalusian, etc.).
  - Preview (play), drag to sequencer/DAW, click row → new “current”.
- **Host MIDI out** for routing suggestions to other tracks.
- Progression sequencer: play, **clear all (✕)**, presets, drag-out, session persistence.

### How to build / test

```bash
# From repo root (chords_theory or chords_theory_enhanced)
cmake --build build --target ChordsTheory_Tests -j"$(sysctl -n hw.ncpu)"
./build/Tests/ChordsTheory_Tests "[NextChord]"

cmake --build build --target ChordsTheory_Standalone -j"$(sysctl -n hw.ncpu)"
open "build/ChordsTheory_artefacts/Debug/Standalone/Chords Theory Enhanced.app"
```

- Local DSP: `USE_LOCAL_NIERIKA_DSP` + submodule `Libs/nierika_dsp` (fork of nierika_dsp).
- CPM hardened against empty/missing downloads (`c60774a`).

### Key files (next-chord stack)

| Area | Paths |
|------|--------|
| Catalogue | `Code/Include/Theory/TriadLibrary.h`, `Code/Source/Theory/TriadLibrary.cpp` |
| Score / rank | `NextChordScorer.h/.cpp` |
| Sequence memory | `NextChordSequenceContext.h/.cpp` — `buildSequenceContext(MidiEditorState, …)` |
| Pool + sort | `NextChordGenerator.h/.cpp` |
| Offline AI | `ChordSeqAIModel.h/.cpp`, `NextChordAiGenerator.h/.cpp`, assets `chordseqai_*` (pure model rank) |
| UI | `Component/NextChordPanel.h/.cpp` (dual columns: Theory left, AI right) |
| Wiring | `AppLayout.h/.cpp` — `setCurrentChordForSuggestions`, `refreshNextChordSequenceContext` |
| Clear sequence | `ProgressionEditor::clearAll()` + ✕ header button |
| Host MIDI | `Audio/HostMidiEmitter` (from earlier next-triad commit) |
| Tests | `Tests/NextChordGeneratorTests.cpp` `[NextChord]`; `ChordSeqAIModelTests.cpp` `[ChordSeqAI]` |
| Strings | `Assets/Languages/en.lang` (`next_chord_*`, `progression_clear_tooltip`) |

### Known limits (not “perfect” yet)

1. **No multi-bar planning** — ranks one next chord, not a full path (though phrase memory helps).
2. **No style profile** — drama ≠ jazz vs pop vs metal priors.
3. **Root-position catalogue** — no inversions / bass-line model in the pool.
4. **Theory column is rules-based**; **AI column** uses offline ChordSeqAI (pure model probability, no hybrid). Still no multi-bar planning or style conditioning.
5. **Chromatic drops** still freeze `ProgressionSlot` (degree may be a fallback for non-diatonic next-chords); history rebuilds from **MIDI notes + labels**, so PC sets stay correct.
6. `MidiEditor::clear()` still does **not** fire `onContentChanged` itself; user clear goes through `ProgressionEditor::clearAll()` which notifies listeners.

### Good next steps (priority ideas)

1. **Style / genre bias** (or second slider) morphing grammar weights.
2. **Richer history UI** (show last few chord symbols, not only “N in sequence”).
3. **Store full `Chord` on chord blocks** so history never depends on MIDI reconstruction.
4. **Inversions / bass motion** in catalogue or scoring.
5. **Commit / push** `work` when ready; keep CHANGELOG updated per feature batch.
6. Optional: unit tests for `ProgressionEditor::clearAll` and AppLayout sequence wiring (panel currently covered via theory tests).

---

## [Unreleased] / recent on `work`

### Offline ChordSeqAI next-chord column

- Pure C++ GRU inference (no ONNX Runtime, no network) from bundled
  `Assets/Data/chordseqai_weights.bin` + `chordseqai_vocab.json` (MIT, ChordSeqAI).
- `Theory/ChordSeqAIModel` + `NextChordAiGenerator`: pure model probability ranking (no symbolic blend).
- Next chords panel **dual columns**: Theory (left, Drama/Fit/Tension) | AI (right, confidence %).
- Export script: `Scripts/export_chordseqai_assets.py`.
- Tests: `Tests/ChordSeqAIModelTests.cpp` tag `[ChordSeqAI]`.
- Attribution: `Assets/ThirdParty/ChordSeqAI_NOTICE.txt`.

### Theory ranking: harmonic destinations before voicings

- Rank **families** first (root + Major/Minor/Dominant/Dim/Aug/Sus); one representative each.
- Simplicity prior: root-position ordinary chords beat needless inversions/extensions (F ≻ F/C).
- Tension = unresolved-feel (intrinsic + functional); **not** voice-leading / pedal bass.
- Fit = absolute contextual coherence (not renormalised to 100); Drama = target tension region.
- Labels target-aware (`V/ii`, `subV/I` only with implied resolve); secondary V/vii bare-major rejected.
- Diagnostic tests: `[NextChord][Architecture]` for C major smooth + history-sensitive ordering.


### e57ee43 — `feat: sequence-aware next-chord ranking and progression clear`

**Sequence context**

- New `SequenceContext` / `SequenceEvent` + `buildSequenceContext()` from `MidiEditorState`.
- Rebuilds chords from note pitch classes (bass-first), degrees from slots/inference.
- Drops last block when it matches the “current” chord (avoids double-counting).
- Lookback cap: 6 events.

**Scorer phrase memory**

- ii–V–I completion, falling-fifths chain continuation  
- Repeat / root-fatigue penalties  
- Pop loop (I–V–vi–IV), Andalusian (minor), “home” to I, light blues I→IV / IV→V  
- Reason tags: `ii–V–I`, `5ths chain`, `repeat`, `pop loop`, `andalusian`, etc.

**UI / wiring**

- `NextChordPanel::setSequenceContext` / `setCurrentChord(chord, sequence)`  
- `AppLayout` refreshes context on content change, key/scale change, current-chord change, session restore  
- Header shows `From G  ·  (2 in sequence)` when history non-empty  

**Progression clear**

- ✕ button beside play (`nui::Icons::getCross()`)  
- `ProgressionEditor::clearAll()` → `MidiEditor::clear()` + `onContentChanged()`  
- Tooltip / lang: `progression_clear_tooltip`

**Tests:** sequence + `buildSequenceContext` cases; full `[NextChord]` green at commit time.

### bb8d0f5 — `feat: expand next-chord ranking with sevenths, drama, and scale tables`

**Catalogue (`TriadLibrary`)**

- 11 qualities × 12 roots = 132 named  
- Sevenths: maj7, m7, 7, m7b5  
- Unique PC sets: **112** (Csus2 ≡ Gsus4, etc.)

**Scoring (multi-layer)**

- Surface: common tones, directed root (↑4th / ↑5th), circle-of-fifths, VL, chromaticism  
- Quality + same-root colour changes  
- Scale families: Majorish / Minorish / ModalSoft / Diminishedish  
- Degree fitness + diatonic 7th colour (V7, ii7, Imaj7, …)  
- Progression grammar (cadence, ii–V, plagal, deceptive, …)  
- Roles: tonic / predominant / dominant transitions  
- Idioms: secondary V/x (weighted), tritone sub, mode mixture, backdoor, sus resolve, mediants, approach chords, tendency / guide tones  
- Blues-friendly I7 / IV7  

**Drama**

- Objective `tensionPercent` for display  
- Sort by distance to target tension band (0 = softest first, 1 = wildest first)  
- Panel slider Smooth ↔ Wild (`next_chord_drama_*` strings)

### Earlier Enhanced commits (context)

| Commit | Summary |
|--------|---------|
| `c60774a` | Harden CPM bootstrap when download missing/empty |
| `992c18f` | Product rename **Chords Theory Enhanced**, 2026 packaging ids |
| `14551de` | Fork docs, dual license MIT+CC0, AI-friendly CONTRIBUTING |
| `3a1f735` | Vendor **nierika_dsp** as git submodule |
| `1a9b0e4` | **Initial next-triad** suggestions, MIDI out, preview, drag |

Upstream baseline (still in history): Midi editor, presets, loop/play, synth UI, etc.

---

## Architecture snapshot (next chords)

```
User picks/previews chord  ──►  AppLayout::setCurrentChordForSuggestions
                                      │
                                      ├─ buildSequenceContext(MidiEditorState, keyScale, current)
                                      └─ NextChordPanel::setCurrentChord(chord, sequence)
                                              │
                                              └─ NextChordGenerator::generate(current, keyScale, drama, sequence)
                                                      │
                                                      ├─ TriadLibrary::allTriads → dedupe PC sets → drop current
                                                      └─ NextChordScorer::scoreAndSort(..., drama, sequence)
                                                              │
                                                              └─ ranked NextChordCandidate[] (tension %, reason label)
```

**Drama vs tension:** tension is “how colourful is this move?”; drama only chooses which band of tension floats to the top of the list.

**Host MIDI:** processor advertises MIDI out; `HostMidiEmitter` used when previewing for DAW routing (see `1a9b0e4` / processor flags).

---

## Test map

```bash
./build/Tests/ChordsTheory_Tests "[NextChord]"
```

Covers (non-exhaustive): catalogue sizes, intervals, pool size 111 from C, drama reorder, quality fitness, secondary/tritone/mixture, cadence grammar, sequence ii–V–I / fifths, `buildSequenceContext` reconstruction.

---

## Product / legal notes

- **Upstream:** MIT, Sebastien Halbeher / Nierika  
- **Fork additions:** CC0 (see `LICENSE`, `CONTRIBUTING.md`)  
- Bundle / codes from rename: e.g. `com.nierika.chordstheoryenhanced`, plugin code `CthE` (confirm in root `CMakeLists.txt` if packaging)  
- i18n: new strings currently in **en.lang** only; other locales may fall back to keys until translated  

---

## Session log (high level)

Work done across Enhanced fork sessions up through `e57ee43`:

1. Restore / reimplement local **nierika_dsp**, then submodule fork.  
2. Ship **next-chord** MVP (triads) + host MIDI + panel.  
3. Rename product, dual license, CONTRIBUTING for AI PRs.  
4. Fix CPM configure fragility.  
5. Expand catalogue (sus/power/7ths), drama slider, scale-aware scoring.  
6. Deepen scorer (grammar, secondaries, mixture, resolution, blues, …).  
7. Wire **progression history** into ranking + **clear sequence** button.  
8. This changelog for continuity.

*Last updated: 2026-08-08 (commit `e57ee43` on `work`).*
