# Doobie

An analog dub delay plugin (VST3 / AU / Standalone) built with JUCE. A stereo
multi-head tape echo whose feedback path carries tape saturation, wow & flutter
and tone shaping, paired with a spring + plate reverb section that can sit
before, after, or inside the feedback loop. Equally at home producing classic
dub echoes and as a general-purpose modulated delay / ambience for other genres.

## Features

- **Multi-head tape echo** — four playback heads tapping a single tape, each with
  its own level, pan and time ratio. A 12-position mode dial selects head
  combinations in the spirit of a Roland Space Echo.
- **Free or tempo-synced** — free-running time (20 ms – 2 s) or musical divisions
  from 1/64 up to 4 bars, including dotted and triplet values, locked to the host.
- **Analog character** — tape saturation, wow & flutter (with random drift), and
  a "tape age" hiss control, all in the recirculating path.
- **Dub tone controls in the feedback loop** — bass/treble shelves plus low-cut and
  high-cut filters, so each repeat dissolves and darkens the way a real tape echo does.
- **Chained reverbs** — a dispersive spring tank and a modulated 8-line plate (FDN).
  Run either alone, in series (spring → plate) or in parallel, and route the reverb
  post-delay, pre-delay, or inside the feedback loop for washing dub textures.
- **Performance controls** — ping-pong feedback, freeze (infinite hold), wet ducking,
  stereo width.
- **12 factory presets** plus user preset save/load.
- **Vintage UI** — brushed-metal panel, amber-lit knobs, analog VU meters, a live
  echo-pattern visualiser and an animated spring tank.

## Building

Requires CMake ≥ 3.22 and a C++20 toolchain. JUCE is vendored as a git submodule.

```sh
git clone --recurse-submodules <repo-url> doobie
cd doobie
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Doobie_All
```

If you already cloned without submodules:

```sh
git submodule update --init --recursive
```

For a fast local build limited to the host architecture, add
`-DCMAKE_OSX_ARCHITECTURES=arm64` (or `x86_64`). The default produces a universal
binary on macOS.

Built plugins are copied to the system plugin folders (`COPY_PLUGIN_AFTER_BUILD`).
The standalone app is at `build/Doobie_artefacts/<config>/Standalone/`.

## Tests

```sh
cmake --build build --target doobie_tests
ctest --test-dir build
```

The tests cover the framework-independent DSP cores (delay line, saturation,
spring and plate reverb stability, wow/flutter).

## Layout

See `BRANDING.md` for the visual language. Source is under `src/` with DSP in
`src/dsp`, UI in `src/ui`, presets in `src/presets`, and the JUCE plugin
boilerplate in `src/PluginProcessor.*` / `src/PluginEditor.*`.
