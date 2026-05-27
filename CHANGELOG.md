# Changelog

All notable changes to Doobie are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/).

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
