# Chords Theory Enhanced

**A community fork** of
[halbehers/chords_theory](https://github.com/halbehers/chords_theory)
by **Sebastien Halbeher** (Nierika).

This fork keeps the original “browse diatonic chords and drag MIDI into a DAW”
plugin and adds experimental harmony tools — notably **ranked next-triad
suggestions** (tension scoring), **host MIDI output** for routing to other
tracks, and related UI — aimed at a “what can I play next?” workflow.

| | |
|---|---|
| **Upstream** | [halbehers/chords_theory](https://github.com/halbehers/chords_theory) |
| **This fork** | [torarinvik/chords_theory_enhanced](https://github.com/torarinvik/chords_theory_enhanced) |
| **License** | Original work: MIT (© Sebastien Halbeher). Fork additions: public domain ([CC0](LICENSE)). See [LICENSE](LICENSE). |
| **Contributing** | Human and **AI-assisted PRs welcome** — see [CONTRIBUTING.md](CONTRIBUTING.md). |

Built on [Nierika Plugin Template](https://github.com/halbehers/nierika_plugin_template).
Available as Standalone, AU, AUv3, and VST3.

## Features (upstream + fork)

- **Key/Scale browser**: 12 keys × 10 scales (Major, Minor, Harmonic/Melodic Minor, the modes, and
  Minor Blues), each scale degree shown with its most popular chord voicing by default.
- **Voicing picker**: click any chord card to swap to a different voicing for that degree (7th,
  9th, sus, inversions, and more, depending on what's diatonically available).
- **Drag to DAW**: drag any chord card onto a MIDI/instrument track to insert it as a one-measure
  clip, closed-voiced near middle C.
- **Progression sequencer**: drag chords from the browser into the piano-roll timeline, load a
  built-in preset, or save your own — then drag the whole progression out as one multi-chord MIDI
  clip.
- **Session state**: key, scale, every degree's chosen voicing, and the full progression sequence
  round-trip through the plugin's own state, so closing and reopening a DAW project restores
  everything exactly as it was left.
- **Next chords (this fork)**: ranked suggestions from the current chord (triads, sus, power,
  sevenths, inversions) with Fit/Tension meters and a Drama slider; sequence-aware theory ranking;
  optional **offline AI mode** (bundled ChordSeqAI GRU — no internet, no cloud API); play-preview,
  drag into the sequencer or a DAW track; optional host MIDI out for routing to another instrument track.
- **Internationalization** (English, French, Spanish, German, Italian, Portuguese) and a
  light/dark theme, both inherited from the template.

## Requirements

- macOS ≥ 14.5 or Windows ≥ 10 (2020)
- CMake ≥ 3.22, [Ninja](https://ninja-build.org/)
- A C++20 compiler (Xcode command line tools)

## Building

Clone with submodules (required for `nierika_dsp`):

```sh
git clone --recurse-submodules https://github.com/torarinvik/chords_theory_enhanced.git
# or after a normal clone:
git submodule update --init --recursive
```

Dependencies — [JUCE](https://github.com/juce-framework/JUCE) 8.0.14 and
[Catch2](https://github.com/catchorg/Catch2) are fetched automatically via
[CPM](https://github.com/cpm-cmake/CPM.cmake) on first configure.
`nierika_dsp` is a **git submodule** at `Libs/nierika_dsp`
([torarinvik/nierika_dsp](https://github.com/torarinvik/nierika_dsp), extended for this plugin).
Optional: set `USE_LOCAL_NIERIKA_DSP` `ON` in `CMakeLists.txt` to point at a separate checkout
under `~/Development/nierika_dsp` while developing the library.

```sh
cmake --workflow --preset default  # configure (first run/whenever CMakeLists.txt changes) + build
```

`--workflow` always does the right thing whether `build/` already exists or not (a fresh clone, or
after deleting it) - if you'd rather configure and build as separate steps (e.g. to build
repeatedly without reconfiguring), that still works too, as long as `build/` already exists:

```sh
cmake --preset default          # configure (Debug, Ninja) - only needed once, or after CMakeLists.txt changes
cmake --build --preset default  # build
```

Built plugin bundles land in `build/ChordsTheory_artefacts/Debug/{Standalone,AU,VST3}`. For a
Release build (also produces an installer - see below):

```sh
cmake --workflow --preset release
```

Xcode and Visual Studio project generation is available via the `Xcode`/`vs` presets.

### Installers

A Release build also packages an installer automatically:
`release-build/Packaging/Chords Theory Enhanced-<version>-macOS.pkg` (AU + VST3, ad-hoc signed unless real
Developer ID credentials are configured in the environment) on macOS, or
`release-build\Packaging\Chords Theory Enhanced-<version>-Windows.exe` (VST3, requires
[Inno Setup](https://jrsoftware.org/isdl.php)'s `iscc` on `PATH`) on Windows.

## Testing

```sh
ctest --test-dir build
```

Runs the Catch2 unit test suite plus an end-to-end
[pluginval](https://github.com/Tracktion/pluginval) validation pass against the built AU. See
`build/Tests/ChordsTheory_Tests --help` for running a subset of tests by name or tag.

Manual verification (not automatable): drag-and-drop into an actual DAW track, host MIDI routing
to another track, and the full state-persistence round-trip across closing/reopening a DAW project.

## Continuous integration

`.github/workflows/build_and_test.yml` (inherited from the template — identity-agnostic) builds a
macOS + Windows matrix on every push/PR, runs `ctest`, uploads installers as workflow artifacts,
and publishes a GitHub pre-release on any `v*` tag. Submodules are checked out recursively.

## Project layout

- `Code/Include/Theory`, `Code/Source/Theory` — chord/scale/progression data model, MIDI export,
  session-state serialization, next-chord scoring. No UI dependency.
- `Code/Include/Component`, `Code/Source/Component` — the chord browser, next-triad panel,
  progression sequencer, and the template's inherited settings components.
- `Assets/Data/chords.json` — the chord database (12 keys × 10 scales), bundled as binary data.
- `Assets/Languages` — localization strings (`.lang` files).
- `Tests` — Catch2 unit tests.
- `CMake` — build configuration helpers (dependency fetching, compiler warnings, `pluginval`
  integration).
- `Libs` — CPM-fetched JUCE; `nierika_dsp` git submodule (see `.gitmodules`).

## License

See [LICENSE](LICENSE):

- **Original / upstream** material: copyright **Sebastien Halbeher** (Nierika), **MIT**.
- **This fork’s original contributions**: dedicated to the **public domain (CC0 1.0)**.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). **AI-assisted and AI-generated pull requests are allowed.**

## Credits

- **Original Chords Theory** and Nierika stack: [Sebastien Halbeher / Nierika](https://github.com/halbehers)
- **This fork**: community enhancements on top of that foundation
