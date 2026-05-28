# Changelog

All notable changes to Doobie are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/).

## [0.10.0] — 2026-05-28

### Added
- **Eight mod-matrix slots** (was four). Existing slot indices 1–4 stay where
  they were; slots 5–8 default to off.
- **Per-head pan and time** are now mod-matrix destinations alongside the
  existing per-head Level: route Env → Head 1 Pan for a sidechain-driven
  auto-panner, or LFO 2 → Head 3 Time for an evolving multi-tap pattern.
  Destinations appended at the end of the list, so existing saved slots
  keep their meaning.

### Changed
- **LFO rate goes down to 0.001 Hz** (was 0.05). At the slow end one cycle
  takes ~17 minutes, perfect for slow filter-cutoff sweeps and pad-style
  evolution; the existing 20 Hz top end is unchanged. 1 Hz still sits at
  the centre of the dial. The rate readout adapts: "Hz" up top, "s" or
  "min" at the slow end, so "0.005 Hz" reads as "3.3 min" instead.
- **TIME knob goes down to 0.5 ms** (was 20 ms). The free-mode delay now
  covers the full short-time territory: sub-millisecond flanger, ~5-30 ms
  chorus, ~80-200 ms slapback, all the way up to the existing 8 s long-tail
  range. The dial is log-skewed so 100 ms sits at the centre, and the value
  popup shows "0.50 ms" / "375 ms" / "1.20 s" depending on magnitude.
- Knob step is now 0.01 ms (was 0.1) so sub-ms values can be dialled in
  precisely. The engine already supported fractional-sample reads through
  Catmull-Rom interpolation, so sub-ms delays sound clean rather than
  quantised.

### Wired with the rest of the plugin
- Combine with the mod matrix: route LFO 1 → Delay Time at low LFO rates
  (0.1-1 Hz, sine or triangle, depth 1.0) for chorus-style detuning on top
  of a 10 ms base. The existing 450 ms master-time capstan smoother
  intentionally limits how fast delay-time modulation lands — gentle
  chorus rates work; aggressive flanger rates (>2 Hz LFO) get smoothed.
  A separate fast-mod path that bypasses the capstan smoother is queued
  for a follow-up commit if needed.

## [0.9.0] — 2026-05-28

### Added
- **Reverb response visualiser rework.** The view next to the reverb panel
  now shows the character of the active reverb, not just a smooth decay
  line:
  - **Algorithmic modes** (Spring, Plate, Series, Parallel, Hall, Shimmer)
    each have a deterministic per-mode early-reflection pattern drawn as
    discrete amber spikes during the first ~60-140 ms (sparse for Spring,
    dense for Plate, wider-spread for Hall) followed by the diffuse-tail
    envelope — modes are instantly distinguishable at a glance.
  - **Convolution mode** draws the actual loaded IR as a peak-envelope
    waveform from the cached source buffer, with a gentle gamma so the
    early reflections sit at the top and the tail stays visible. The
    timeline reflects the current IR SPEED multiplier (the label notes
    "x0.5" etc. when speed is off-centre).
- Predelay marker, second-grid lines and the mode + decay-time / IR-length
  readout are still there.

### Changed
- **TIME knob** now displays its value as "375 ms" (below 1 s) or
  "1.20 s" (1 s and above) on hover instead of the raw float, so free-mode
  delay times are readable at a glance.

## [0.8.0] — 2026-05-28

### Added
- **Modulation matrix.** Two free-running LFOs (rate 0.05–20 Hz, depth,
  waveform from Sine / Triangle / Saw Up / Saw Down / Square / Random S&H)
  and one envelope follower (attack 0.1–500 ms, release 1–2000 ms,
  ±24 dB sensitivity) feed a four-slot matrix. Each slot picks a source, a
  destination from a curated 28-entry list (Delay Time, Feedback, Mix, Width,
  **Duck**, Drive, Wow, Flutter, Age, all four filter cutoffs, both shelves,
  every head's level, Reverb Mix / Mod, Plate Decay/Size/Damp/Predelay,
  Spring Decay/Tone, IR Gain) and a bipolar amount.
- **Sidechain-style ducking** is the obvious move — route Env → Duck with a
  positive amount and the wet path drops while the input is loud. The matrix
  is general: any source can drive any destination.
- Modulation is applied per-block to the base EngineParams; the engine's
  per-sample smoothers ramp toward the modulated targets, so even fast LFO
  modulation stays click-free.

### UI
- New MODULATION panel above the output bar with three always-on sections —
  **LFO 1**, **LFO 2**, **ENVELOPE** — each with its own live meter
  (bipolar bar for the LFOs, unipolar bar for the envelope follower). The
  slot configuration is one click away behind a **MATRIX...** button that
  opens a popup, keeping the main UI tidy.

### Known follow-ups
- The reverb decay-curve visualiser is unchanged for now; turning it into a
  proper IR-style view (early reflections + tail) is queued for the next
  commit.

## [0.7.0] — 2026-05-28

### Added
- **IR GAIN** (-24 dB ... +24 dB, default +6 dB). JUCE Convolution normalises
  IRs to peak=1, which makes long reverb tails sit quietly relative to the
  dry signal; this makeup gain brings the wet up to a useful level without
  leaning on the reverb MIX. Smoothed per-sample inside the convolution
  wrapper so twiddling doesn't click.
- **IR SPEED** (0.25x ... 4x, default 1x). Re-loads the current IR with a
  lying source sample rate so JUCE's resampler stretches or compresses it
  — a -2 / +2 octave IR-playback FX. 0.5x = an octave down + double length;
  2x = an octave up + half length. Triggered via APVTS listener on the
  message thread (loading inside JUCE Convolution allocates, so not
  real-time safe).
- Both knobs appear in the reverb panel only when REVERB == Convolution;
  they share the slots normally occupied by MOD and PRE (which are inactive
  in convolution mode anyway).

### Changed
- **Smoothed the click-prone params.** REVERB MIX, SATURATION drive and the
  AGE macro are now pulled per-sample from smoothers (20-50 ms ramp) instead
  of stepping per block. Twiddling these knobs (or automating them) no
  longer clicks. The tone-filter cutoffs and shelves still update per block
  — that smoothing is queued for the planned mod-matrix work, which will
  need finer filter modulation anyway.

### Note
- Two larger features were explicitly requested in the same conversation but
  are NOT in this release: (a) full filter-param smoothing on a sub-block
  cadence, (b) a 2-LFO + envelope-follower modulation matrix. Both are queued
  for follow-up commits.

## [0.6.0] — 2026-05-28

### Added
- **38 free Voxengo impulse responses bundled with the plugin.** Real-room and
  effect IRs by Aleksey Vaneev — concert halls (Musikvereinsaal, Scala Milan),
  churches (St Nicolaes, Derlon Sanctuary), drum rooms, parking garages, caves,
  guitar cabinets and creative spaces (Deep Space, Greek 7 Echo Hall, etc.).
  Embedded via JUCE BinaryData and auto-discovered at runtime, so they replace
  the placeholder synthesised IRs entirely. License terms preserved verbatim in
  `external/voxengo-irs/license.txt`; full attribution lives in the README.
- **Delay bypass toggle** (next to PING-PONG / FREEZE). When on, the tape
  buffer is left untouched and the input is fed straight through the same
  character chain (saturation, BBD low-pass, diffusion, pitch, etc.) and the
  AGE macro — turning the plugin into a tape-style coloration / saturator
  while the reverb panel and the dry/wet mix still work normally.

### Compatibility
- Voxengo's impulse responses are © Aleksey Vaneev under their own
  royalty-free license (`external/voxengo-irs/license.txt`). They are bundled
  unaltered, with that license file intact, and acknowledged in the README —
  meeting the redistribution conditions Voxengo sets. They are NOT licensed
  under Doobie's GPL-3.0; they are aggregated with it (mere aggregation).

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
