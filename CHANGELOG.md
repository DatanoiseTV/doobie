# Changelog

All notable changes to Doobie are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/).

## [0.5.0] — 2026-05-28

### Added
- **Convolution reverb mode** (REVERB → `Convolution`). Implemented on JUCE's
  partitioned `dsp::Convolution`; the per-sample engine wraps it with a
  64-sample buffer (~1.5 ms latency on the reverb tail only — the dry path is
  untouched). Works in all three reverb routes (Post, Pre, In-Feedback).
- **Six built-in factory IRs**: Small Room, Big Room, Hall, Cathedral, Cave,
  Tunnel. Synthesised deterministically at runtime — early reflections plus a
  decorrelated stereo diffuse tail with a closing low-pass — so they ship with
  zero binary cost and no third-party licensing. Picked from a combo in the
  reverb panel.
- **LOAD CUSTOM...** button loads any WAV/AIFF/FLAC into the same convolution
  slot. State persistence stores either the factory index or the file path on
  the APVTS state tree, so sessions restore the choice automatically.
- README points to three free, royalty-free external IR collections (Voxengo,
  OpenAIR, EchoThief) for users who want to bring their own spaces.

## [0.4.1] — 2026-05-28

### Fixed
- **BBD self-oscillated too early.** The BBD character had a 1.4× pre-drive
  feeding a resonant SVF (Q=0.45, peak ~2.2×) inside the feedback loop, so the
  round-trip gain was ~3×: BBD ran away at FEEDBACK ≈ 0.32 while every other
  character needed ≈ 1.0. Pre-drive softened to 1.1× and Q raised to 0.85, so
  BBD now self-oscillates around FEEDBACK ≈ 0.8 — still a touch hotter than
  Tape (the character of an MN3005-era bucket brigade), but no runaway at
  moderate feedback. A new regression test asserts every delay character
  decays at FEEDBACK 0.6.

## [0.4.0] — 2026-05-28

### Changed
- **Longer max delay.** The tape buffer goes out to 16 s (was 8 s) and the
  free-mode TIME knob to 8 s (was 2 s), so "4 bars at 60 BPM" — and slow
  ambient drones in general — fit without running out of tape. Memory cost is
  modest: ~8 MB stereo per instance at 48 kHz, ~32 MB at 192 kHz.
- The echo visualiser's window cap is widened to match, so long delays still
  show a meaningful strip of taps.

## [0.3.0] — 2026-05-27

### Added
- **AGE is now a tape-wear macro**, not just a hiss level. One knob drives hiss,
  slow level dropouts (oxide shedding), progressive high-frequency loss, and
  extra wow/flutter from a tired transport — all compounding through the
  feedback path. At 0 it is a true bypass, so low-age patches are unchanged.

### Changed
- **Factory bank rewritten** (~60 presets) around the head matrix and the AGE
  macro: multi-head tap patterns (including combinations the old dial couldn't
  make, e.g. heads B+C+D with A off), heavier use of AGE on the lo-fi/vintage
  patches, and coverage of every delay character, reverb mode and route.

### Compatibility
- The AGE control keeps its parameter id and 0–1 range, so automation still
  works; patches that set it above 0 will sound more worn than before.

## [0.2.0] — 2026-05-27

### Added
- **Head matrix.** The 12-position MODE dial is replaced by four lit pads that
  switch playback heads A–D in or out independently — every combination of taps
  is now reachable (including none and the pairs the old dial couldn't make).
- DC blockers in the feedback loop (5 Hz) and on the wet output (8 Hz).

### Changed
- **Fewer artifacts when the delay time moves.** Per-head time ratio, level and
  pan are now smoothed, and the master time glides over a longer window, so
  moving a head TIME knob or crossing a sync-division boundary eases like a tape
  capstan instead of zipping.
- **Click-free head switching.** Toggling a head ramps its gain (~30 ms) instead
  of cutting it dead.

### Migration
- The old `modeSel` parameter is removed. Sessions, factory presets and user
  presets saved with `modeSel` are converted to the head matrix automatically on
  load, so existing patches sound the same. Automation lanes written against
  `modeSel` are not carried over.

## [0.1.0]

- Initial release: multi-head tape echo, five delay characters, tape
  saturation, wow/flutter, dual filter stages, seven reverb modes and the
  factory preset bank.
