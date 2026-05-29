<div align="center">

# Doobie

### Analog dub delay — VST3 · AU · Standalone

A stereo multi-head tape echo with tape saturation, wow & flutter, in-loop tone
shaping, and a chained spring + plate reverb. Built for classic dub, equally at
home as a modulated delay and ambience for any genre.

![License](https://img.shields.io/badge/license-GPL--3.0-blue)
![Formats](https://img.shields.io/badge/formats-VST3%20%C2%B7%20AU%20%C2%B7%20Standalone-f4a024)
![JUCE](https://img.shields.io/badge/JUCE-8.0-cccccc)
![C++](https://img.shields.io/badge/C%2B%2B-20-555)
![macOS](https://img.shields.io/badge/macOS-universal%20%C2%B7%20signed%20%2B%20notarized-lightgrey)
![Linux](https://img.shields.io/badge/Linux-x86__64-lightgrey)
[![Release](https://img.shields.io/github/v/release/DatanoiseTV/doobie?include_prereleases&sort=semver&color=cccccc)](https://github.com/DatanoiseTV/doobie/releases)

<img src="docs/screenshots/doobie.png" width="850" alt="Doobie — Classic Dub preset, cassette transport at top of the DELAY panel">

</div>

## Highlights

- **Multi-head tape echo** — four playback heads tapping one tape, each with its
  own level, pan and time. In sync each head's time snaps to a musical division
  (1/32 … 1/1) so taps land on the grid; free-running, it's a continuous fraction
  of the repeat. A four-pad **head matrix** switches each head in or out
  independently for any combination of taps, and switching is click-free — heads
  ramp in and out instead of cutting dead.
- **Five delay characters** — Digital (clean), Tape (wow/flutter + saturation),
  BBD (dark analog bucket-brigade), Diffuse (repeats smeared into a wash) and
  Pitch (each repeat climbs an octave).
- **Free or tempo-synced** — free time (20 ms – 8 s) or musical divisions from 1/64
  up to 4 bars (dotted and triplet included), locked to the host. The tape buffer
  goes out to 16 s, so "4 bars at 60 BPM" or slower ambient drones fit comfortably.
  Division and sync changes glide like a tape capstan rather than jumping.
- **Analog character** — tape saturation, wow & flutter with random drift, and an
  **AGE** macro that wears the tape down: hiss, slow level dropouts, progressive
  high-frequency loss and extra transport instability, compounding across repeats.
- **Two independent filter stages** — an **input** filter (low-cut, high-cut, bass
  and treble) shapes only the signal that gets echoed, leaving the dry output clean;
  a separate **feedback** filter (same four controls) darkens and dissolves each
  repeat the way real tape does. Each stage has its own dedicated knobs.
- **Eight reverb modes** — a dispersive spring tank, a modulated plate, a dense
  16-line **hall**, an octave-up **shimmer**, plus spring→plate series and
  spring+plate parallel, and a **convolution** mode bundled with **38 free
  Voxengo impulse responses** (concert halls, churches, rooms, cabinets and
  effect spaces) that also loads any WAV/AIFF/FLAC impulse response of your
  own. Route any of them **post**, **pre**, or **inside the feedback loop**
  for washing dub textures and blooming ambient tails.
- **Delay bypass** — flick the BYPASS toggle next to PING-PONG/FREEZE and the
  tape buffer goes silent, but the input still passes through the character
  chain (Tape/BBD/Diffuse/Pitch saturation, head-bump and HF loss) plus AGE
  and the reverb panel. Lets you use Doobie as a tape-style saturator /
  colouration insert.
- **Performance controls** — ping-pong feedback, freeze (infinite hold), wet ducking,
  stereo width.
- **60+ factory presets** across dub/reggae, ambient/cinematic, rhythmic/electronic,
  lo-fi/vintage, hall & shimmer, delay characters, sound-design FX and instrument
  patches, plus user save/load.
- **Cassette-deck UI** — a vector cassette transport sits at the top of the DELAY
  panel: both reels with their three-spoke spindles, the tape line threaded
  through idler rollers + head + pinch roller, and a head body that lights
  amber when the tape is "playing". Reel rotation **tracks the delay-time
  knob** with proper physics (constant tape speed, inverse-radius scaling),
  so a 30 ms flanger setting spins the reels visibly faster than an 8 s dub
  delay; freeze + delay-bypass stop the capstan. Below it sits the
  **echo-tap timeline strip** showing where the four heads land along
  0 → master-delay with feedback fall-off. Monochrome white-on-black chrome
  matching the cassette, amber kept only as the "this is live" accent on
  lit toggles + VU peaks. Analog VU meters at the output and a reverb
  decay-curve display on the right.

## Install

Grab the latest signed build from the
[**Releases page**](https://github.com/DatanoiseTV/doobie/releases) — every push
to `main` updates a rolling `nightly` prerelease, and `vX.Y.Z` tags cut stable
versioned releases.

- **macOS:** `Doobie-X.Y.Z-macOS.pkg`. Universal arm64 + x86_64, code-signed
  with a Developer ID Application certificate and notarized through Apple's
  notary service, so it installs without a Gatekeeper warning. Double-click
  the `.pkg`; the installer lets you pick any subset of AU
  (`/Library/Audio/Plug-Ins/Components`), VST3
  (`/Library/Audio/Plug-Ins/VST3`) and the Standalone app (`/Applications`).
- **Linux:** `Doobie-X.Y.Z-Linux-x86_64.tar.gz`. Contains the VST3 bundle and
  the Standalone executable. Drop the `.vst3` into `~/.vst3/` (user) or
  `/usr/local/lib/vst3/` (system); run the Standalone directly.

Want the bleeding edge?
[Releases → `nightly`](https://github.com/DatanoiseTV/doobie/releases/tag/nightly)
gets a fresh signed build on every commit to `main`.

## Screenshots

<div align="center">

| Multi-head, plate reverb | Ambient wash (spring + plate) | Lo-fi vintage echo |
|:---:|:---:|:---:|
| <img src="docs/screenshots/multihead.png" width="270"> | <img src="docs/screenshots/ambient.png" width="270"> | <img src="docs/screenshots/vintage.png" width="270"> |

</div>

The cassette transport (top of the DELAY panel) shows the reels rotating at a
speed derived from the current delay time, with the head body lit when the tape
is active. The strip below it visualises each active head's tap landing along
the 0 → master-delay axis with feedback fall-off. The reverb panel's decay
curve shows the active reverb's tail length and pre-delay at a glance.

## Getting dub sounds

- **Long dark echoes:** sync to 1/4, FEEDBACK ~0.7, pull HIGH CUT down to ~3 kHz so
  repeats get darker each pass; add a little SATURATION.
- **Spring crashes / wash:** set REVERB to *Spring* and ROUTE to *In Feedback* — the
  reverb builds across repeats. Ride REVERB MIX.
- **Siren feedback:** push FEEDBACK to ~1.0 and ride the LOW CUT / HIGH CUT.
- **Freeze a moment:** hit FREEZE to hold the buffer infinitely, then play over it.

## Impulse responses

Set REVERB to *Convolution*. **38 impulse responses ship with the plugin** —
all from [Voxengo's free IR pack](https://www.voxengo.com/impulses/), by
**Aleksey Vaneev**, distributed royalty-free under his own terms (see
[`external/voxengo-irs/license.txt`](external/voxengo-irs/license.txt)).
The set covers real concert halls (Musikvereinsaal, Scala Milan Opera Hall),
churches (St Nicolaes Church, Derlon Sanctuary), drum rooms, parking garages,
caves, guitar cabinets and effect spaces (Deep Space, Greek 7 Echo Hall, On a
Star, etc.). Pick one from the IR combo in the reverb panel.

**LOAD CUSTOM...** loads any WAV / AIFF / FLAC of your own; the session
restores either the factory or the custom selection automatically.

These impulse responses are © Aleksey Vaneev; they are bundled unaltered with
Doobie under Voxengo's terms — they are **not** under Doobie's GPL-3.0. By
using them you acknowledge that Aleksey Vaneev retains exclusive ownership of
the impulse files including all intellectual property rights therein, at all
times.

A few other permissively-licensed external IR collections that work well as
starting points for custom files:

- **[Voxengo free impulses](https://www.voxengo.com/impulses/)** — royalty-free for
  any use including commercial; the cleanest licensing of the lot. Halls, plates,
  cabinets, springs.
- **[Open AIR Library](https://openair.hosted.york.ac.uk/)** — large catalogue of
  real-space IRs (cathedrals, halls, tunnels) under Creative Commons.
- **[EchoThief](http://www.echothief.com/)** — over a hundred real-world spaces
  sampled around North America.

## Building from source

Requires CMake ≥ 3.22 and a C++20 toolchain. JUCE is vendored as a git submodule.

```sh
git clone --recurse-submodules https://github.com/DatanoiseTV/doobie.git
cd doobie
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Doobie_All
```

Already cloned without submodules? `git submodule update --init --recursive`

For a fast single-architecture build on macOS add
`-DCMAKE_OSX_ARCHITECTURES=arm64` (or `x86_64`); the default produces a
universal binary. Built plugins are copied into your personal plug-in folders
(`~/Library/Audio/Plug-Ins/...` on macOS) and the standalone app lands in
`build/Doobie_artefacts/<config>/Standalone/`. Pass
`-DDOOBIE_INSTALL_LOCAL=OFF` to opt out of the local install (matches what CI
does).

On Linux you'll need the JUCE GUI / audio prerequisites:

```sh
sudo apt-get install -y \
    libasound2-dev libjack-jackd2-dev libcurl4-openssl-dev \
    libfreetype6-dev libx11-dev libxcomposite-dev libxcursor-dev \
    libxext-dev libxinerama-dev libxrandr-dev libxrender-dev \
    libwebkit2gtk-4.0-dev libglu1-mesa-dev mesa-common-dev \
    libgtk-3-dev ninja-build
```

## Release pipeline

`.github/workflows/` defines two workflows:

- **`ci.yml`** — runs on every PR + non-`main` branch push. Builds Doobie +
  the test executables on macOS-14 and ubuntu-22.04, runs `ctest`. Fast
  feedback, no signing.
- **`release.yml`** — runs on `main` pushes and `v*` tags. macOS job imports
  the Developer ID certs from GitHub Secrets, builds universal, runs tests,
  then signs + notarizes + stapled-packages the artefacts into a `.pkg`
  installer. Linux job builds + tarballs. Publish job uploads both to a
  rolling `nightly` prerelease (main pushes) or a versioned release (tag
  pushes).

If you fork Doobie and want signed releases of your own builds:
[`packaging/macos/`](packaging/macos/README.md) has a one-shot
`bootstrap-signing.sh` that takes a fresh Apple Developer membership all the
way to populated GitHub Secrets — generates RSA keys + CSRs locally, walks
you through the two manual portal clicks Apple's API still doesn't expose
(API key creation + Developer ID Installer cert), bundles each cert into a
`.p12` with a random password, and pushes all 11 secrets via `gh secret set`.

## Tests

```sh
cmake --build build --target doobie_tests
ctest --test-dir build
```

Tests cover the framework-independent DSP cores (delay line, saturation, spring and
plate stability/decay, wow/flutter). The Audio Unit also passes Apple's `auval`.

## Project layout

DSP lives in `src/dsp`, the UI in `src/ui`, presets in `src/presets`, and the JUCE
plugin glue in `src/PluginProcessor.*` / `src/PluginEditor.*`. `tools/Snapshot.cpp`
renders the editor to a PNG offline (used for the screenshots above).

## License

Copyright © 2026 **DatanoiseTV**.

Doobie is free software, licensed under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)). In short: you may use, study, modify and redistribute it,
but any distributed version or derivative **must be released under the GPLv3 with
full source code**, and **must keep this attribution to DatanoiseTV**.

GPLv3 is required because Doobie links the [JUCE](https://juce.com) framework under
its GPLv3 option. Building Doobie into a closed-source product would require a
commercial JUCE license.
