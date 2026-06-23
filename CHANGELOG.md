# Changelog

All notable changes to Doobie are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project
uses [Semantic Versioning](https://semver.org/).

## [0.20.0] — 2026-06-23

### Added
- **Env follower sidechain filter.** LP / HP / BP TPT-SVF on the
  envelope follower's input tap, so modulation can be driven by a
  chosen band (kick-triggered ducks on LP, hat-triggered chops on
  HP, vocal-band swells on BP) instead of the broadband mix. Off =
  follower sees raw input. The filter is internal to the follower;
  the main audio path is untouched. New params: `envFilterOn`,
  `envFilterType`, `envFilterCutoff`, `envFilterRes`.

### Changed
- **UI auto-scales to the window.** The 1520x960 design canvas now
  uniformly scales to fill whatever WebView bounds JUCE hands us,
  via a JS handler driving the existing `#plugin` transform.
  Default editor size dropped to 1216x768 so the plug-in opens
  comfortably on a 13" MacBook; resize freely from there.
- **Master TIME knob is now useful in sync mode.** In sync mode the
  knob maps to the syncDiv list (one notch per division) instead of
  writing to a value the engine ignores. The dropdown beside it
  stays authoritative for keyboard / preset cases.

### Fixed
- **Per-head VUs were sluggish.** Three layered fixes: the engine
  now peak-holds each head magnitude across all blocks since the
  last UI tick (previously sampled one block in ~6 at 30 Hz UI vs
  256-sample blocks at 48 k); the Fader / DigitalMeter rAF loops no
  longer tear down on every `levels` event (live value moved to a
  ref); and a fighting 40 ms CSS transition on the fader meter bar
  was removed. Meters now register transients and animate at
  display refresh.

## [0.14.0] — 2026-06-22

### Added — hardware-port features
Ported the months of work done on the embedded Keinedelay/DFM build
(`/Users/syso/dev/Keinedelay2.1_DFM`) back into the plug-in. Stops the
two codebases from drifting and lands the curated factory bank.

- **Phaser.** Six-stage all-pass cascade with feedback, three insert
  points (`Post` = on the wet echoes, `Pre` = into the delay input,
  `In Feedback` = cumulative per repeat — phaser sits BEFORE the reverb
  at the same insert point). Right channel runs at a 1/4-cycle phase
  offset so the notch motion paints across the stereo image instead of
  summing to mono. New APVTS params: `phaserOn`, `phaserRoute`,
  `phaserRate`, `phaserDepth`, `phaserFb`, `phaserMix`. Source:
  `src/dsp/Phaser.h`.
- **Input multimode filter.** TPT state-variable (Zavalishin), LP / HP /
  BP selectable per sample from the same core (so sweeping the type
  can never blow the filter up). Per-sample `setParams` tracks the
  smoothed cutoff zipper-free; the stereo pair shares one `tan()` call
  via `copyCoefsFrom`. OFF = true bypass — pre-delay only colours the
  signal when you reach for it. New APVTS params: `inFilterOn`,
  `inFilterType`, `inFilterCutoff`, `inFilterRes`. Source:
  `src/dsp/Svf.h`.
- **Greatly expanded TapeAge.** The previous version was a hiss-floor
  plus a one-pole low-pass. Replaced with the 250-line hardware model:
  generation-loss HF roll-off that compounds in feedback; modulation
  noise that rides the signal envelope with a tape-band spectrum
  (HP 120 Hz + LP 9 kHz); discrete dropout events (oxide shedding —
  random 20-120 ms dips with extra HF loss during the dip);
  programme-dependent compression / headroom loss; sparse crackle
  grains (filtered tick at ~3.5 kHz with 2-6 ms decay, scales `age^3`);
  transport-instability boosts that fold into the wow/flutter
  generator. All bounded ≤ unity, all gated by the AGE knob — AGE 0
  is still true bypass.
- **New modulation destinations.** Append-only at the end of the
  `ModDest` enum (existing slot indices stay stable for already-saved
  states): `InFilterCutoff`, `InFilterRes`, `Pan`, `OutLevel`,
  `PhaserRate`, `PhaserDepth`, `PhaserMix`. Plugin keeps its 8-slot
  matrix (hardware uses 6 due to screen size); 6-slot presets just
  leave the extra two slots `Off`.
- **64 factory presets** — straight port of the hardware's curated
  bank, replacing the previous 59-preset set. Each entry exercises a
  distinct corner of the engine; sweeps span dub voices, ambient
  washes, modulated filters, phaser routings, frozen pads, gated 80s,
  octave shimmer, dyna-pan, trance gate, slapback / doubler /
  Nashville / Swell / Infinity. Tones written in Hz on the hardware
  side are translated into the plugin's `lpFreq` + `treble` pair so
  the sonic intent ports 1:1. Reverb-mode index gets a swap (hardware
  Gated=6 / Shimmer=7 maps to plugin Gated=8 / Shimmer=6). See
  `src/presets/PresetManager.cpp`.

### Compatibility notes
- Existing user sessions / saved states continue to work — the new
  parameters are append-only with `OFF`-equivalent defaults
  (`inFilterOn=false`, `phaserOn=false`).
- `EngineParams` gained a `pan` field; the engine's existing per-head
  pan logic is unchanged.

## [0.13.5] — 2026-06-01

### Fixed (Linux)
- **Restored GPU compositing on Linux.** v0.13.3/v0.13.4 unconditionally
  set `WEBKIT_DISABLE_DMABUF_RENDERER=1` and
  `WEBKIT_DISABLE_COMPOSITING_MODE=1` as a blanket fix for the
  NVIDIA-proprietary-Wayland white-window class. That worked, but it
  also turned off GPU compositing for **everyone else** -- a user on
  Ubuntu 26.04 + Intel iGPU reported **1 fps** in Ardour because of
  this. Both env vars removed from the defaults. Only `GDK_BACKEND=x11`
  stays (correctness fix -- JUCE's WebKit child is an X11 client). The
  README now documents these env vars as opt-in workarounds for users
  who actually hit the NVIDIA-Wayland white window.

## [0.13.4] — 2026-06-01

### Changed (Linux)
- **Never use `/tmp` for JUCE's WebKit subprocess helper.** Previous
  release probed `/tmp` and relocated `$TMPDIR` only if exec was
  denied. Cleaner approach: always point `$TMPDIR` at a user-owned
  location from the start. We prefer `$XDG_RUNTIME_DIR/doobie`
  (= `/run/user/$UID/doobie` under systemd — always exec-allowed,
  cleared at logout), falling back to `~/.cache/doobie` for non-
  systemd systems. A user-set `$TMPDIR` is still respected. Drops
  the probe-and-warn branch in favour of always-correct.

## [0.13.3] — 2026-06-01

### Fixed (Linux defensive hardening)
A Linux user still reported a white window on v0.13.2 even after the
webkit2gtk-4.1 fix. Deep audit surfaced three additional silent
failure modes JUCE 8 has no host-side log for. All three are now
guarded in `WebEditor.cpp` before the `WebBrowserComponent` is
constructed:

- **WebKitGTK DMA-BUF renderer**. WebKitGTK 2.42+ defaults to a
  DMA-BUF/EGL path that fails silently (white window, no log) on
  NVIDIA proprietary drivers under Wayland, in VMs without virgl,
  and in several sandbox configs (WebKit bug 262607). Now we
  pre-set `WEBKIT_DISABLE_DMABUF_RENDERER=1`,
  `WEBKIT_DISABLE_COMPOSITING_MODE=1` and `GDK_BACKEND=x11` defensively
  (only when those vars aren't already set, so power users can still
  override).
- **`/tmp` mounted noexec**. JUCE extracts a WebKit helper binary to
  `$TMPDIR/_juce_linux_subprocess*` and `execv`s it; on hardened
  RHEL/Rocky, Snap-packaged DAWs (AppArmor), Fedora 41+ (SELinux),
  and Flatpak/bubblewrap-confined hosts the exec silently fails →
  no WebKit process → blank window. We now probe with a tiny shell
  script, and if exec is denied, relocate `$TMPDIR` to
  `$XDG_RUNTIME_DIR` (always exec-allowed under systemd) or
  `~/.cache/doobie` before JUCE looks at it. Either way a stderr
  warning is logged so support reports become actionable.
- **CI matrix now covers Ubuntu 24.04** alongside 22.04 — modern
  distros catch webkit2gtk-4.1 regressions before users do.

### Documentation
- README + CHANGELOG both reference the new env-var workarounds
  users on weird configs can fall back to.

## [0.13.2] — 2026-05-31

### Fixed
- **Linux builds now load on modern distros.** CI was installing only
  `libwebkit2gtk-4.0-dev`, so the binary preferred libsoup-2 at runtime.
  On distros that ship WebKitGTK 4.1 / libsoup-3 (Ubuntu 24.04+, Fedora
  40+, Debian 13+, Arch, …) any other process-loaded library that pulled
  in libsoup-2 aborted WebKit's network process with
  `libsoup2 symbols detected. Using libsoup2 and libsoup3 in the same
  process is not supported.` → white window. Now we build with both
  4.1 + 4.0 dev packages so JUCE's pkg-config picks the modern path and
  the binary dlopens whichever the user's distro provides. Aligns with
  the WebKitGTK upstream deprecation of libsoup2 (final removal in
  2.52.0, March 2026).
- **macOS hardened-runtime: added `com.apple.security.cs.allow-jit`.**
  Required for JavaScriptCore's modern `MAP_JIT` API on Sequoia+.
  `allow-unsigned-executable-memory` (already present) covers the older
  exception used by `juce_dsp`'s FFT JIT but doesn't satisfy the new
  WKWebView check on its own.

### Documentation
- **README's Install section** now distinguishes the two Linux runtime
  paths (WebKitGTK 4.1 vs 4.0), warns against mixing them in one
  system, and documents the `TMPDIR` workaround for `/tmp` mounted
  `noexec` (RHEL hardened images, some snap-confined hosts).

## [0.13.1] — 2026-05-31

### Added
- **Diagnostic banner** at the top of the WebView (auto-hides as soon as
  React mounts). When the UI fails to load on a user's machine the
  banner stays visible and ticks off the stages we got past
  (HTML → juce bridge → React → Babel → App mounted), plus shows any
  uncaught JS error. Makes Linux WebKit-related bugs reportable
  without needing the user to open dev tools.

### Documentation
- **Linux runtime dependencies** spelled out in the README's Install
  section: `libwebkit2gtk-4.0-37 libgtk-3-0 libglib2.0-0 libsoup2.4-1
  libasound2 libjack-jackd2-0` plus the WebKitGTK 4.0 vs 4.1
  compatibility note for Ubuntu 24.04+. A Linux user reported a
  white-window-after-install on 0.13.0; this is the documentation half
  of the fix.

## [0.13.0] — 2026-05-30

### Added
- **Full UI rebuild on JUCE 8's `WebBrowserComponent`** — the entire editor
  is now an HTML/CSS/JS app served from BinaryData (no network), bridged to
  the audio engine via `WebSliderRelay` / `WebToggleButtonRelay` /
  `WebComboBoxRelay` + their matching `*ParameterAttachment`s. All 96 APVTS
  parameters are two-way bound; host automation and preset loads flow
  through the same path as user drags. React + Babel-standalone vendored
  locally; the JUCE frontend JS is wrapped into a non-module IIFE that
  exposes `window.Juce`. CSP + WKWebView native scheme (`juce://`)
  configured correctly so the bundle loads fully offline.
- **Gated reverb** — new `src/dsp/GatedReverb.{h}`: plate core with an
  envelope-keyed sidechain gate. Classic 80s drum-gate sound — snare
  triggers a bright bloom that snaps to silence at the gate boundary.
  Surfaced as reverb mode #8 in the dropdown; three new APVTS params
  (`gateThreshold`, `gateHold`, `gateRelease`) appear as a row in the
  Reverb panel only when Gated is selected.
- **Right-click knob context menu** — Reset to default, Copy value, Paste
  value, Enter value… (the Enter-value path opens the styled modal). Per-
  knob via `onContextMenu`. The browser's native context menu is suppressed
  globally so the plugin reads as a native desktop app.
- **Styled save-preset modal** — replaces the JUCE `AlertWindow` that
  looked alien against the dark chrome. Cancel on Esc / scrim click; Enter
  to confirm; payload travels through the existing
  `Juce.backend.emitEvent('preset_save', {name})` channel.
- **Per-stage live VU bridge** — engine now publishes `inputLevel`,
  `delayLevel`, `reverbLevel` atomics in addition to the per-channel
  `outputLevel`. The WebView's `levels` event ticks at 30 Hz with proper
  20·log10 dBFS values; the digital meters do their own RMS / peak-hold
  + clip indicator on top.
- **Modulation indicators on every knob** — any knob whose APVTS id is a
  matrix destination shows a dim outer arc + animated dot driven live by
  `matrix amount × source depth`. Includes the per-head pan/time
  destinations.
- **Native-feel polish** — selection / drag-select / right-click-Inspect
  suppressed via CSS + JS for everything except real `<input>` /
  `<textarea>` elements (so the preset-rename modal still accepts typing).
- **Reverb route selector** moved out of the top meter bridge into the
  Reverb panel itself, where it belongs.

### Changed
- **DecayGraph wired to the decay knob** (was wired to size, which is
  unrelated to tail length — graph looked decorative).
- **REC indicator removed** from the cassette area (we're a tape echo, not
  a recorder; the light was misleading).

### Fixed
- `WebBrowserComponent` use-after-free on close: the relays are
  `WebViewLifetimeListener`s whose destruction order had to come *after*
  the browser. Reordered class members.
- WKWebView showed "Could not connect to the server" because we navigated
  to `https://doobie.localhost`; JUCE only registers the `juce://` scheme
  for served resources. Switched to `getResourceProviderRoot()`.
- Two `juce_add_binary_data` targets (Voxengo IRs + UI assets) shared the
  `BinaryData::` namespace, so the linker silently picked one and the UI
  resource lookup ran against the WAV file table. Each target now lives in
  its own namespace (`DoobieIRData::` and `DoobieUIData::`).
- Vendored JUCE frontend JS still had an `import` at the top — illegal in
  a non-module script — so the IIFE never executed, `window.Juce` was
  undefined, and React threw on first render. The check-native-interop
  file is now inlined into the IIFE; verifier asserts no `import`/`export`
  survives the wrap.
- Hooks (`useState`, `useEffect`, etc.) were re-declared in multiple
  babel-script files → redeclaration `SyntaxError`. Hoisted to globals in
  the HTML shell so each .jsx can reference them directly.

### Known issues (next iteration)
- **Window scaling** — the editor is 1520 × 960 native pixels and doesn't
  scale to fit smaller displays. Pending: CSS `transform: scale()` driven
  off the editor's actual bounds.
- **Convolution-mode controls** — `irGain` / `irSpeed` and the factory IR
  picker / "Load custom…" file dialog aren't surfaced; the Reverb panel
  shows the plate knobs even when the mode is Convolution.
- **DecayGraph is approximated**, not live. It draws an `exp(-t/τ)`
  envelope from the decay knob rather than the actual reverb impulse
  response. Live data would need an extra IR/RT60 emission from the
  engine.
- **WKWebView inspection** — patched the vendored JUCE source so
  `developerExtrasEnabled` is set in Release and `setInspectable:YES` is
  called on macOS 13.3+. Carry that patch forward when bumping JUCE.

## [0.12.0] — 2026-05-29

### Changed
- **Whole UI restyled as a cassette deck.** Pure-black panels (`#000`) matching
  the cassette interior, white linework everywhere, knobs redrawn as cassette
  spindle / idler-roller analogues (dark inset + white rim + white pointer +
  white centre dot), toggles as small white-rim circles with an amber LED dot
  when lit, head-matrix pads flip to a solid white face with black letter when
  selected. Teal dropped entirely; amber kept only as the single "this is
  live" accent (head body when playing, lit toggle LED, VU bar). The
  brushed-metal gradients and horizontal grain on the chassis are gone.
- **Cassette grew to 180 px** as the centerpiece of the DELAY panel (was 98 px),
  with the head, both reels including 3-spoke spindles, and the tape line
  through all idler rollers clearly readable.
- **Reel rotation now tracks delay time.** Real tape echoes have fixed
  head-to-head distance, so longer delay → slower capstan. Mapping:
  `speed = clamp(pow(0.375 s / t, 0.6), 0.15, 5.0)` — 1.0 × at the 375 ms
  default, ~4.3 × at a 30 ms flanger, ~0.15 × at an 8 s dub. Sync mode
  reapplies `quarters * 60 / bpm` so the visual tracks host tempo. Freeze
  and delay-bypass stop the capstan.
- **Window height 870 → 1024** to fit the bigger cassette + the echo-tap
  strip without crushing the knob rows.

### Added
- **Echo-tap timeline strip** sitting below the cassette in the DELAY panel
  (56 px). The cassette shows transport state; the strip shows where the four
  heads land along 0 → master-delay with feedback fall-off. Complementary,
  not replacing each other.
- **macOS code-signed + notarized release pipeline.** `.github/workflows/`
  builds Doobie on every push to `main` and every `vX.Y.Z` tag — universal
  arm64 + x86_64, code-signed with the Developer ID Application cert, packaged
  as a Distribution `.pkg` signed with the Developer ID Installer cert,
  notarized via Apple's notary service, stapled. Versioned tags publish a
  stable release; main pushes update a rolling `nightly` prerelease. End users
  install with no Gatekeeper warning.
- **Linux CI build** on `ubuntu-22.04`. Catches platform-specific build issues
  at PR time (would have caught the SIMD bug below).
- **`packaging/macos/`** with `bootstrap-signing.sh` that takes a fresh Apple
  Developer membership all the way to populated GitHub Secrets in one pass:
  generates RSA key + CSR locally, opens the developer portal for the cert
  bits Apple's API refuses to issue, bundles each cert into a `.p12` with a
  random password, derives the codesign identity CN from the cert subject,
  and pushes all 11 secrets via `gh secret set` — cert bytes stream via
  stdin so they never appear in shell history.

### Fixed
- **Linux build error** at `ConvolutionReverb.h:205` with
  `juce::jmax<juce::int64>`. JUCE 8's maths overload set includes
  `dsp::SIMDRegister<T>` variants, so the compiler considers
  `SIMDRegister<long long>` during overload resolution — which requires a
  complete type, which on Linux's SSE-only fallback is forward-declared but
  never defined for `long long`. Replaced the templated call with a plain
  ternary; same call now resolves on all platforms.

## [0.11.0] — 2026-05-28

### Added
- **Vector cassette visualiser.** The lane visualiser at the top of the DELAY
  panel is replaced by a vector cassette transport — two reels with their
  spindles, idler rollers, tape head, pinch roller and REC indicator, drawn
  as scalable JUCE Graphics. Visual design and proportions are ported 1:1
  from the author's own Recordy project
  (https://github.com/DatanoiseTV/Recordy) and reimplemented in idiomatic
  JUCE C++ — see the attribution in `src/ui/VectorCassette.h`. Idles with a
  steady reel rotation; later iterations on this branch will wire it to the
  multi-head taps + an animated tape loop and re-tune the colour scheme.

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
