# Contributing to Chords Theory Enhanced

Thanks for your interest in improving this project.

## This is a fork

**chords_theory_enhanced** is a community fork of
[halbehers/chords_theory](https://github.com/halbehers/chords_theory) by
Sebastien Halbeher (Nierika). Please keep that relationship clear in PRs and
docs: credit upstream, and prefer linking back to the original project when
discussing the base plugin.

Upstream issues that apply to the original product may belong on the upstream
repository rather than here.

## AI-assisted contributions are welcome

**Pull requests written or assisted by AI (including fully AI-generated patches)
are allowed and encouraged**, as long as they meet the same quality bar as any
other contribution.

If AI was used in a substantial way, a short note in the PR description is
appreciated (tool/model optional). You remain responsible for:

- correctness and that the change actually builds and is tested where practical;
- not introducing secrets, malicious code, or license violations;
- respecting the dual licensing described in [LICENSE](LICENSE).

Human review still applies. Low-effort drive-by AI spam may be closed.

## How to contribute

1. Fork this repository and clone with submodules:

   ```sh
   git clone --recurse-submodules https://github.com/torarinvik/chords_theory_enhanced.git
   ```

2. Create a branch for your change.

3. Build and test (see [README.md](README.md)):

   ```sh
   cmake --workflow --preset default
   ctest --test-dir build
   ```

4. Open a pull request against `main` with a clear description of *what* and
   *why*.

## Coding notes

- Prefer small, focused PRs.
- Match existing style and architecture (see `CLAUDE.md` for the layout of the
  Theory / Component / Audio layers).
- New theory/DSP logic should be unit-tested when practical.
- Do not commit build artefacts, secrets, or large binary blobs.

## License of your contributions

By contributing to this fork, you agree that **your original contributions** are
dedicated to the public domain under **CC0 1.0** (see [LICENSE](LICENSE)).
Upstream/original material remains under the original author’s MIT copyright
and is not re-licensed by your contribution.

## Code of conduct (simple)

Be respectful. Assume good faith. No harassment or spam.
