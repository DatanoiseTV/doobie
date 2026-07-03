# Doobie — Native JUCE UI Architecture Plan

This document is the blueprint for replacing the WebView+React editor at `src/ui/WebEditor.cpp` with a pure JUCE Component tree. Visual fidelity to the existing WebView UI is the explicit goal; the only intended user-visible change is "WebView is gone".

The current WebView reference is the spec. Every panel, atom, viz, modal, and live-data event in `ui/src/*.jsx` + `ui/src/doobie.css` must be reproduced.

---

## 1. Directory + file layout

All new code lives in **`src/ui/native/`**. One header + cpp pair per class, grouped by role. The existing `src/ui/{LookAndFeel,VuMeter,EchoVisualiser,ReverbResponseView,VectorCassette,ModMatrixPopup,ModSourceMeter}.{h,cpp}` files are LEGACY and stay untouched until Phase 6 removes them.

```
src/ui/native/
├── DoobieLookAndFeel.h / .cpp          # palette + drawing routines + sizes
├── Style.h                             # constexpr sizes / radii / fonts (header-only)
├── NativeEditor.h / .cpp               # new AudioProcessorEditor — replaces WebEditor
│
├── atoms/
│   ├── Knob.h / .cpp                   # rotary with arc + mod indicator
│   ├── WidthDial.h / .cpp              # the fan-shaped stereo width dial
│   ├── Fader.h / .cpp                  # vertical fader with inline VU
│   ├── Chip.h / .cpp                   # LED toggle button
│   ├── Selector.h / .cpp               # segmented multi-button (.seg)
│   ├── DropDown.h / .cpp               # styled ComboBox
│   ├── LedMeter.h / .cpp               # horizontal stage-meter (the IN/DELAY/REVERB/OUT bridge cells = DigitalMeter port)
│   ├── VuMeter.h / .cpp                # vertical L/R stereo pair (output bar)
│   ├── PowerButton.h / .cpp            # ⏻ panel bypass toggle
│   ├── PanelHeader.h / .cpp            # PHead (title + icon + meta + action slot)
│   ├── ClusterLabel.h / .cpp           # small uppercase row label
│   ├── KnobContextMenu.h / .cpp        # right-click Reset/Copy/Paste/Enter
│   └── ValueOverlay.h / .cpp           # the .kval tooltip pop above knob/fader during drag
│
├── viz/
│   ├── TapeDeck.h / .cpp               # animated Space-Echo reels + strands
│   ├── StereoScope.h / .cpp            # rolling stereo waveform inside the tape loop
│   ├── DecayGraph.h / .cpp             # reverb decay curve
│   ├── LfoScope.h / .cpp               # rolling LFO oscilloscope (per LFO)
│   ├── WaveMini.h / .cpp               # the per-matrix-row tiny scope (small variant)
│   ├── EnvViz.h / .cpp                 # env follower bar + LED
│   └── IrThumbnail.h / .cpp            # convolution IR waveform thumbnail (optional, see ReverbPanel)
│
├── panels/
│   ├── HeaderBar.h / .cpp              # logo + version + preset nav + Mod + Save
│   ├── FlowBar.h / .cpp                # the VUStrip (4 LedMeters + chevrons)
│   ├── InputPanel.h / .cpp
│   ├── HeadsPanel.h / .cpp             # 4 head strips with anti-collision
│   ├── DelayPanel.h / .cpp             # tape + big knobs + tape character
│   ├── PitchShifterPanel.h / .cpp
│   ├── FeedbackPanel.h / .cpp
│   ├── PhaserPanel.h / .cpp
│   ├── ReverbPanel.h / .cpp            # mode-branched layout
│   └── OutputBar.h / .cpp              # mix/width/duck/output + multiband + auto-gain + L/R VU
│
├── modals/
│   ├── ModalLayer.h / .cpp             # full-window overlay base (scrim + animated child)
│   ├── PresetBrowser.h / .cpp
│   ├── IrPicker.h / .cpp               # opens its own browser sub-modal
│   ├── SavePresetDialog.h / .cpp       # name input + Save/Cancel
│   ├── ConfirmDialog.h / .cpp          # unsaved-changes confirm
│   └── ModDrawer.h / .cpp              # bottom-sheet with Sources / Matrix tabs
│
├── support/
│   ├── ParamBinding.h / .cpp           # the SliderAttachment helpers + non-slider attachers
│   ├── ModMap.h / .cpp                 # native equivalent of useJuceModMap (ranges+live per APVTS id)
│   ├── LevelsTimer.h / .cpp            # one 30 Hz timer that pushes levels + presetInfo + irInfo into a Listenable
│   ├── PresetIo.h / .cpp               # wraps PresetManager calls + categoryOf logic
│   └── Format.h / .cpp                 # native versions of the JSX `fmt` table (pct, db, ms, hz, …) + noteHzFmt
│
└── (intentionally no juce_gui_extra usage — WebView is gone)
```

Naming convention: every new class lives in `namespace doobie::ui`.

---

## 2. DoobieLookAndFeel

**Lives in**: `src/ui/native/DoobieLookAndFeel.{h,cpp}`. Inherits `juce::LookAndFeel_V4`.

**Owns**: the OKLCH-resolved palette, font references (Space Grotesk + JetBrains Mono — loaded from `BinaryData` if shipped, falling back to JUCE's default sans + monospace), and centralised drawing routines used by every atom.

The existing `src/ui/LookAndFeel.{h,cpp}` is a different (cassette-style) palette and **will NOT be reused**. It stays for the legacy editor only and is deleted along with WebEditor in Phase 6.

### Palette translation (oklch → sRGB)

Pre-computed once; resolved from `doobie.css` lines 8–28. Stored as `static const juce::Colour` getters on `Palette` (a struct nested in `DoobieLookAndFeel.h`):

| CSS token | OKLCH | sRGB hex | RGB |
|---|---|---|---|
| `--c-bg` | 0.168 0.006 60 | `#110e0c` | (17, 14, 12) |
| `--c-panel` | 0.205 0.006 60 | `#191714` | (25, 23, 20) |
| `--c-panel-2` | 0.238 0.007 60 | `#211e1c` | (33, 30, 28) |
| `--c-raise` | 0.275 0.008 60 | `#2b2724` | (43, 39, 36) |
| `--c-inset` | 0.125 0.005 60 | `#080605` | (8, 6, 5) |
| `--c-well` | 0.108 0.004 60 | `#050403` | (5, 4, 3) |
| `--c-line` | 0.32 0.006 60 | `#353230` | (53, 50, 48) |
| `--c-line-2` | 0.26 0.006 60 | `#262321` | (38, 35, 33) |
| `--c-txt` | 0.93 0.008 75 | `#ebe7e2` | (235, 231, 226) |
| `--c-txt-2` | 0.66 0.010 70 | `#96918c` | (150, 145, 140) |
| `--c-txt-3` | 0.47 0.010 65 | `#5f5a55` | (95, 90, 85) |
| `--accent` | 0.745 0.150 62 | `#ef9436` | (239, 148, 54) |
| `--accent-dim` | 0.55 0.110 62 | `#9f6122` | (159, 97, 34) |
| `--accent-glow` | 0.745 0.150 62 @ 0.45α | `#ef9436` with α=115 | (239, 148, 54, 115) |
| `--peak` | 0.63 0.205 28 | `#eb453b` | (235, 69, 59) |
| hdr grad top | 0.225 0.006 60 | `#1e1b19` | (30, 27, 25) |
| hdr grad bot | 0.185 0.006 60 | `#151210` | (21, 18, 16) |
| knob body-out | 0.30 0.006 60 | `#302d2b` | (48, 45, 43) |
| hstrip-on bg | 0.235 0.012 62 | `#221d18` | (34, 29, 24) |
| body grad lo | 0.10 0.004 60 | `#040303` | (4, 3, 3) |
| btn-accent fg | 0.16 0.02 60 | `#140b05` | (20, 11, 5) |

Stored as:
```cpp
namespace doobie::ui {
struct Palette {
    static constexpr juce::uint32 bg       = 0xff110e0c;
    static constexpr juce::uint32 panel    = 0xff191714;
    static constexpr juce::uint32 panel2   = 0xff211e1c;
    static constexpr juce::uint32 raise    = 0xff2b2724;
    static constexpr juce::uint32 inset    = 0xff080605;
    static constexpr juce::uint32 well     = 0xff050403;
    static constexpr juce::uint32 line     = 0xff353230;
    static constexpr juce::uint32 line2    = 0xff262321;
    static constexpr juce::uint32 txt      = 0xffebe7e2;
    static constexpr juce::uint32 txt2     = 0xff96918c;
    static constexpr juce::uint32 txt3     = 0xff5f5a55;
    static constexpr juce::uint32 accent   = 0xffef9436;
    static constexpr juce::uint32 accentDim= 0xff9f6122;
    static constexpr juce::uint32 accentGlow = 0x73ef9436; // 45% alpha
    static constexpr juce::uint32 peak     = 0xffeb453b;
    static constexpr juce::uint32 hdrTop   = 0xff1e1b19;
    static constexpr juce::uint32 hdrBot   = 0xff151210;
    static constexpr juce::uint32 knobBody = 0xff302d2b;
    static constexpr juce::uint32 hstripOn = 0xff221d18;
    static constexpr juce::uint32 bodyLo   = 0xff040303;
    static constexpr juce::uint32 accentFg = 0xff140b05;
};
} // namespace
```

### Centralised drawing routines

Methods on `DoobieLookAndFeel`:

- `drawRotaryArc(Graphics&, Rectangle<float>, float value01, bool bipolar, float modHalf01, float liveOff01, Colour arcColor = {}, int sizeId)` — matches the JSX arc/body/pointer/hub/tick/modrange/moddot stack from `knob.jsx` lines 80–131. `sizeId` ∈ {sm=46, md=60, lg=116}, picks stroke widths from `Style::knobStrokeFor(sizeId)`.
- `drawFader(Graphics&, Rectangle<int>, float value01, float meter01, bool dragging)` — rail + cap + accent fill + meter overlay (`doobie.css` lines 290–312).
- `drawChip(Graphics&, Rectangle<int>, bool on, bool down, bool compact, juce::StringRef text, bool drawLed)` — `.chip` / `.chip-row-compact .chip`.
- `drawSegmented(Graphics&, Rectangle<int>, juce::Span<const juce::StringRef> labels, int selectedIdx)` — `.seg` (mirrors lines 839–855). Two layout variants: standard, and `mm-mode-seg` (height 22 / font 9.5 / pad 6) for the mod matrix.
- `drawPanelBackground(Graphics&, Rectangle<int>, bool compact, bool flush)` — the panel body w/ inset-shadow + line border (.panel rule).
- `drawPanelHeaderRule(Graphics&, Rectangle<int>)` — the `.phead .hrule` linear-gradient horizontal line.
- `drawPowerButton(Graphics&, Rectangle<int>, bool on)` — the ⏻ circle (.pwrbtn).
- `drawWell(Graphics&, Rectangle<int>, float radius)` — inset background under selects, faders, meters.
- `drawLedMeterCell(Graphics&, Rectangle<int>, float smoothedDb, float peakDb, bool big, bool drawScale)` — the DigitalMeter bar (mfill gradient, mzero marker, mpeak white sliver, mclip red light at right).
- `drawTextHeading(Graphics&, Rectangle<int>, juce::StringRef, float letterSpacing, FontFamily ff)` — small uppercase 600 weight.

### Size + typography constants — `Style.h`

```cpp
namespace doobie::ui::Style {
    constexpr int radLg    = 14;
    constexpr int radMd    = 10;
    constexpr int radSm    = 7;

    constexpr int knobSm   = 46;
    constexpr int knobMd   = 60;
    constexpr int knobLg   = 116;

    constexpr int rowGap   = 14;
    constexpr int padBody  = 14;
    constexpr int padPanel = 16;          // .panel padding right/bottom
    constexpr int padPanelCompact = 12;   // .panel.compact

    constexpr int hdrHeight     = 62;
    constexpr int outBarHeight  = 86;     // measured: outrow + padding (= 12 padding + ~74 content) at zoom:.85 → actual 73ish
    constexpr int rootWidth     = 1520;
    constexpr int rootHeight    = 960;

    constexpr float letterSm   = 0.18f;
    constexpr float letterMd   = 0.24f;

    juce::Font sansBold (float size);     // Space Grotesk 600
    juce::Font sansReg  (float size);
    juce::Font mono     (float size);
}
```

This stays a header-only `namespace` — no need for a `Style::` class.

---

## 3. Atoms

Each atom is a Component (or Slider/Button subclass where attachment compatibility matters). Each has `paint()` that delegates to the LookAndFeel routine. All atoms read accent from a setter (`setArcColor (juce::Colour)`) overridable from the panel — used by DelayPanel's feedback-redshift logic (`fbCol` in app.jsx line 334).

### Knob — `atoms/Knob.{h,cpp}`

```cpp
class Knob : public juce::Slider {
public:
    enum Size { Small = 46, Medium = 60, Large = 116 };
    explicit Knob (Size s = Medium);
    void setBipolar (bool);
    void setLit (bool);                    // adds the `lit` class style (label brightens)
    void setLabel (juce::String);          // the .klabel text below
    void setValueFormatter (std::function<juce::String(float)>);
    void setArcColor (juce::Colour);       // override accent — used for feedback redshift
    void setModRange (float halfNorm);     // 0..0.5; drives the mod-arc width
    void setModLiveOffset (float signedNorm);  // -mod..+mod; drives the live dot
    void setDefaultValue (float);          // double-click + context menu reset target

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;  // reset
    void mouseDown (const juce::MouseEvent&) override;         // right-click → context menu
    void mouseDrag (const juce::MouseEvent&) override;         // vertical drag w/ shift fine + 240 px range
    // We override mouseDrag explicitly to lock to the JSX feel (240 px / range,
    // shift→25% gain, no horizontal rotation). The default Slider drag math
    // doesn't match.
private:
    Size sz; bool bipolar = false, lit = false;
    juce::String label;
    std::function<juce::String(float)> formatter;
    juce::Colour arcColor; bool hasArcColor = false;
    float modHalf = 0.0f, modLive = 0.0f;
    float defaultValue = 0.0f;
    ValueOverlay tooltip;                  // child, shown during drag
};
```

**Why Slider subclass not Component**: lets JUCE's `SliderParameterAttachment` work for free (see §8). The slider's range is the canonical 0..1; APVTS attachment handles the param's real unit conversion. Visual is fully custom (text box hidden, drag math overridden).

### WidthDial — `atoms/WidthDial.{h,cpp}`

Visually distinct from Knob (no arc — has the stereo fan from `knob.jsx` lines 188–230). Subclasses Slider too; reuses the same drag math. Default value 0.6.

### Fader — `atoms/Fader.{h,cpp}`

`class Fader : public juce::Slider` (vertical). Custom paint hides the default thumb, draws the rail + accent fill + cap + per-head VU meter overlay. `setMeter (float linAmp)` from the panel; internal one-pole follower (20 ms attack / 180 ms release) matches `knob.jsx` lines 261–278. The follower runs in a `juce::Timer` at 60 Hz per Fader instance.

### Chip — `atoms/Chip.{h,cpp}`

`class Chip : public juce::Button`. `paintButton()` calls `lnf.drawChip(...)`. Supports two modes:
- **latching** (default) — toggles on click; bind via `ButtonAttachment`.
- **momentary** — calls a setter on press+release. Used by Kill FB (`panels.jsx` lines 433–441). Exposes `setMomentary (std::function<void(bool)> onChange)`.

`setCompact(true)` switches to the smaller `.chip-row-compact .chip` style (height 28 / pad 9 / font 10 / led 6).

### Selector — `atoms/Selector.{h,cpp}`

`class Selector : public juce::Component`. Owns N internal `juce::Button` children with a shared paint that draws the segmented "pill" (.seg). API: `setOptions (StringArray)`, `setSelectedIndex (int)`, `onChange (std::function<void(int)>)`. Wired to APVTS via a custom `SelectorAttachment` that mirrors `ComboBoxAttachment` — see §8.

`setCompactMm(true)` switches to `.mm-mode-seg` style (used in the mod-matrix Bi/Uni row).

### DropDown — `atoms/DropDown.{h,cpp}`

`class DropDown : public juce::ComboBox`. Overrides `paint` to draw the well background + chevron from `doobie.css` lines 263–275. Used in places the JSX uses a `<select>` (delay character, reverb mode, syncDiv, lfo wave, lfo div). Attaches via standard `ComboBoxAttachment`.

### LedMeter — `atoms/LedMeter.{h,cpp}`

`class LedMeter : public juce::Component, private juce::Timer`. Owns:
- `liveDb` (atomic float, set by LevelsTimer broadcast).
- a one-pole follower (80 ms attack / 200 ms release; matches `mounts.jsx` lines 53–72).
- a peak-hold (sample-and-decay).
- a clip latch.

Two variants: `big=false` (the bridge cells = 168 px wide / 7 px bar), `big=true` (output bar L/R = 304 px / 13 px bar with scale). The smoother runs at the display refresh via `Timer` at 60 Hz reading the latest atomic; matches the rAF approach.

### VuMeter — `atoms/VuMeter.{h,cpp}`

Pair-form of LedMeter packaged as a single Component drawing two horizontal bars stacked (the OutputBar L/R pair `.out-meters`).

### PowerButton — `atoms/PowerButton.{h,cpp}`

`class PowerButton : public juce::Button`. 28×28 circle with the ⏻ glyph from `knob.jsx` lines 176–186. Latching toggle. Attaches via `ButtonAttachment`.

### PanelHeader — `atoms/PanelHeader.{h,cpp}`

`class PanelHeader : public juce::Component`. Properties: `title` (StringRef), `meta` (juce::String, may be live), `icon` (a small `juce::Path` or pre-rendered glyph from a tiny SVG-equivalent set — see §12), and an optional `actionChild` (PowerButton instance owned by the panel).

The icons (`Ico.in`, `Ico.heads`, `Ico.delay`, `Ico.fb`, `Ico.rev`, `Ico.out` — `panels.jsx` lines 56–63) ship as `juce::Path` constants in `Style.h::Icon::in()` etc., reproduced from the SVG `<path>` data.

### ClusterLabel — `atoms/ClusterLabel.{h,cpp}`

`class ClusterLabel : public juce::Component`. Small 9 px 600-weight uppercase label with `letter-spacing:0.18em`. Used for "TAPE CHARACTER", "INTERVAL", "DUCK MODE" etc.

### ValueOverlay — `atoms/ValueOverlay.{h,cpp}`

The `.kval` and `.fader-val` tooltip pop. A small floating Component shown during drag, positioned above the parent. Has its own paint (well background + line2 border + accent text). The Knob/Fader's `mouseEnter`/`mouseDrag`/`mouseUp` toggle its visibility.

### KnobContextMenu — `atoms/KnobContextMenu.{h,cpp}`

Owned by the editor (singleton), opened from `Knob::mouseDown` on right-click via `editor.openKnobMenu (event.screenPosition, this)`. Replaces JUCE's `PopupMenu` because the spec custom-paints the menu (`doobie.css` lines 810–830).

Items: Reset / Copy / Paste / Enter value. "Enter value" opens a small inline `juce::TextEditor` modal. Clipboard uses `juce::SystemClipboard`. Paste parses with the same heuristic as `viz.jsx::parseValue` (trailing `%` → /100, else 0..1).

---

## 4. Visualisations

### TapeDeck — `viz/TapeDeck.{h,cpp}`

The hardest port. Source is `ui/src/tape.js` (the imperative SVG animator referenced from `mounts.jsx` line 18). The visual is `viewBox 0 110 680 168` — two reels at x=132 / x=558, upper strand y=126, lower strand y=232, head markers above the upper strand.

**Strategy**:
1. Port the SVG geometry to `juce::Path` objects built once in the constructor and re-stroked each frame.
2. Reels: two filled+ringed circles at the fixed reel positions, drawn rotating via `g.addTransform (AffineTransform::rotation (angle, cx, cy))`. Angle increments at `2π · speedHz · dt`, where `speedHz` is derived from the delay TIME knob like the React app (`tapeSpeed = 2.4 - p.time * 2.0`, mapped to Hz with the original tape.js multiplier).
3. Strands: a top and bottom polyline; sag is a static cosine bend baked into the Path (no per-frame deformation needed — the original is non-stretching).
4. Head markers (A/B/C/D): four small boxes above the top strand, x = `cx_left + (cx_right - cx_left) * h.time`, with text labels. Active heads brighten to accent; inactive dim to txt-3.
5. Scrolling tape diamonds: a small set of evenly-spaced markers on the strands, x-translated each frame by `dx = strandVel * dt`, wrapping modulo strand length. Pure draw-loop translation, no `<animateMotion>`.
6. Driven by a `juce::Timer` at 30 Hz; each tick advances reel + strand angle/offset, calls `repaint()`.

API:
```cpp
class TapeDeck : public juce::Component, private juce::Timer {
public:
    struct Head { char id; bool on; float time, level, pan; };
    void setHeads (std::array<Head, 4>);
    void setPlaying (bool);
    void setRecording (bool);
    void setSpeed (float multiplier);    // 0.4..2.4 from app.jsx line 331
    void setAccent (juce::Colour);
};
```

### StereoScope — `viz/StereoScope.{h,cpp}`

128-sample rolling history per channel. `pushPeak(float linL, float linR)` called from the editor's LevelsTimer (30 Hz). Drawn each frame inside the TapeDeck's interior box; the parent (DelayPanel) positions it at the same 21% / 21% / 38% / 32% rectangle as the CSS `.tape-scope` rule (lines 332–352). L deflects up (-y), R down (+y). Newest sample on the LEFT (matches `mounts.jsx` line 124 comment).

```cpp
class StereoScope : public juce::Component {
public:
    void pushPeak (float lLin, float rLin);   // called from outside at ~30 Hz
    void paint (juce::Graphics&) override;
private:
    std::array<float, 128> histL{}, histR{};
    int head = 0;
};
```

### DecayGraph — `viz/DecayGraph.{h,cpp}`

Direct port of `viz.jsx::DecayGraph`. Static path computed when `decay` changes (`k = 2.2 + (1-decay)*6`); 60 sample points, drawn as a stroked path on top of a vertical-gradient fill. RT60 label `~xx.x s` top-right.

API: `setDecay(float)`, `setType(juce::String)`. Lives in the ReverbPanel.

### LfoScope — `viz/LfoScope.{h,cpp}`

Per-instance rolling history (96 samples). One per `LfoCard` (4 cards in the Mod drawer). Falls back to a static-shape preview when no live value has been pushed yet — matches the JSX `WaveMini` shape-rendering branch.

`pushValue(float v)` called from LevelsTimer for the relevant LFO. Indexed by N=1..4.

### EnvViz — `viz/EnvViz.{h,cpp}`

Horizontal bar + LED + numeric readout. `setLevel(float 0..1)` from LevelsTimer. Repaints on change. Lit threshold = 0.1. Bar tint computed in-paint via `Colour::interpolatedWith(peak, ...)` matching `panels.jsx` lines 822–828 `color-mix` logic.

### WaveMini — `viz/WaveMini.{h,cpp}`

Small variant of LfoScope used per-row in the mod matrix. Different size (56×22), no axis line, less stroke weight. Reuses the same rolling-history class — pass `compact=true`.

### IrThumbnail — `viz/IrThumbnail.{h,cpp}`

Optional refinement; not currently in the WebView UI but the processor exposes `getIrThumbnail(int binCount)` already (PluginProcessor.h lines 112–120). Plan to add to ConvolutionPanel header but mark as **stretch goal** so panel parity ships first.

### Update rates

| Viz | Source | Rate | Mechanism |
|---|---|---|---|
| TapeDeck reels/strands | local animation | 30 Hz | per-instance Timer |
| StereoScope | processor `getOutputLevel(0/1)` peak | 30 Hz | editor-level LevelsTimer pushes |
| DecayGraph | reverb decay param | on-change | listener |
| LfoScope ×4 | `getLfo[1..4]Value()` | 30 Hz | editor LevelsTimer |
| EnvViz | `getEnvValue()` | 30 Hz | editor LevelsTimer |
| WaveMini (matrix) | depends on row's selected src | 30 Hz | LevelsTimer routes |
| LedMeter ×6 | `getInputLevel`/`getDelayLevel`/`getReverbLevel`/`getOutputLevel(L/R)` | 30 Hz push, 60 Hz local repaint | LevelsTimer + meter Timer |

---

## 5. Composite panels

Layout strategy across the board: **hand-laid `juce::Rectangle<int>` arithmetic in each panel's `resized()`**, modelled on the existing `PluginEditor.cpp`. JUCE FlexBox/Grid are tempting but the spec layout has many bespoke spans, asymmetric gaps, and CSS `zoom`-shrunk regions; explicit math gives the deterministic pixel match the head needs. Each panel owns a single `getLocalBounds()` and slices it.

Each panel:
- subclasses `juce::Component`
- owns its atom children as concrete value members (no heap)
- exposes a single `attach (AudioProcessorValueTreeState& apvts, ModMap& mods, LevelsBroadcaster& levels)` method called once from `NativeEditor` after construction
- panel rectangles + spacing live in a small private `Layout` struct with named fields (`r_phead`, `r_eqRow`, etc.)

### HeaderBar — `panels/HeaderBar.{h,cpp}`

Children: brand label (`juce::Label`), version label, two preset nav arrow buttons (`juce::TextButton`), preset name button (custom-painted to match `.preset .name`), Mod toggle button, Save button.

`onPresetPrev / onPresetNext / onOpenBrowser / onToggleMod / onSave` callbacks owned by the editor.

Dirty-state: shows trailing ` *` and re-fetches preset name from `LevelsBroadcaster::presetInfo()` cache (filled by the same 30 Hz timer).

### FlowBar — `panels/FlowBar.{h,cpp}`

Four `LedMeter` instances laid out in a row with chevron labels between, matching `.bridge`. Width 1492 (root - body padding), height ~58.

### InputPanel — `panels/InputPanel.{h,cpp}`

Top row: `Knob` Gain (size Md, lit), divider, 4 small Knobs (LowCut/HighCut/Bass/Treble) in a 4-column grid.

Filter row: Chip "Filter" + Selector LP/HP/BP + Knob "Cutoff" (sm, lit) + Knob "Reso" (sm). The filter row's visibility / opacity is gated by `p.inFilterOn` — implemented as `setVisible()` on the conditional children (the row stays at the same height; only the Chip is visible when off). This avoids the JSX `&&` style that re-mounts.

### HeadsPanel — `panels/HeadsPanel.{h,cpp}`

Children: 4 `HeadStrip` sub-components (`atoms/HeadStrip.{h,cpp}` — pulled out because the strip has its own state and anti-collision listener). Each strip owns: a head-letter Button (custom-painted to match `.hbtn`), a Fader, three Knobs (Pan / Time / Offset).

Anti-collision: `setHead(i, 'time', clampTime(...))` logic from `panels.jsx` lines 168–202 ports to a method `HeadsPanel::clampTimeForHead(int i, float requestedRatio)` that's called from each strip's Time-knob `onChange` before forwarding to the APVTS attachment.

### DelayPanel — `panels/DelayPanel.{h,cpp}`

Children:
- `PanelHeader` with PowerButton action.
- `TapeScreen` (sub-component holding the TapeDeck + StereoScope + record-dot + scan-line decoration).
- Big knob cluster: Knob "Time" (Large), the middle column (Chip Sync + DropDown division + DropDown character + ClusterLabel "Character" + compact 3-chip row), Knob "Feedback" (Large).
- Tape-character row: 4 small Knobs (Wow/Flutter/Saturation/Age) in a 4-column grid.

Mode branch: when `syncMode=true`, the Time knob remaps its value/format from raw ms to a stepped picker over `syncDivChoices`. Implemented by swapping the Knob's `valueFormatter` and overriding `mouseDrag` to snap-step. Same logic as `panels.jsx` lines 391–402.

### PitchShifterPanel — `panels/PitchShifterPanel.{h,cpp}`

Children:
- PanelHeader (compact) + PowerButton (`pitchOn`).
- Interval row (`ClusterLabel` + live readout `juce::Label` + MIDI + PORTA chips).
- Chip row (11 semitone chip buttons in a horizontal flex).
- Algo/Route segmented row (two `Selector` instances).
- Spread + optional Glide rows (each = a `Slider` LinearHorizontal + label + value readout).

The chip row's 11 buttons are owned in a `std::array<juce::TextButton, 11>` with shared paint via the LookAndFeel's `.shimmer-int-chips button` style.

### FeedbackPanel — `panels/FeedbackPanel.{h,cpp}`

Compact panel. 4-column eqrow: two "kb-with-sub" knobs (LowCut + HighCut with live note readout below) and two bipolar Knobs (Bass + Treble). Second eqrow: LC Res + HC Res Knobs (sm) — two filled cells, two blank.

The note-readout label updates via a parameter listener on `hpFreq` / `lpFreq`.

### PhaserPanel — `panels/PhaserPanel.{h,cpp}`

Compact panel + PowerButton. Route row (Selector Pre/InFeedback/Post). Eqrow: Rate / Depth / Fb / Mix Knobs.

The whole content's "dim when off" effect is implemented by `setAlpha(0.45f)` on the inner container and `setInterceptsMouseClicks(false, false)`.

### ReverbPanel — `panels/ReverbPanel.{h,cpp}`

The most mode-dependent panel. Branches on `reverbMode`:

- **Off / Spring / Plate / Spring>Plate / Spring+Plate / Hall** → eqrow1 (Spring/S.Tone/Damp/Mod) + eqrow2 (Decay/Size/Pre-Delay/Width/V-LC/V-HC, 6 cols).
- **Convolution** → IrPicker row + eqrow (V-LC/V-HC/IR Gain/IR Speed/Width, 5 cols). NO decay graph (no plate engine).
- **Gated** → eqrow1 + eqrow2 (Plate-derived) + a 3-col gate-eqrow (Gate Thr/Hold/Rel).
- **Shimmer** → eqrow1 + eqrow2 + a Shimmer interval picker (subhead + chip row + optional Glide slider).

Implementation: each set of mode-specific children lives in a sub-Component (`ReverbAlgoControls`, `ReverbConvControls`, `ReverbGatedControls`, `ReverbShimmerControls`), all added to the panel up-front. `reverbMode` listener calls `setVisible(true)` on the active subset, `setVisible(false)` on the rest, then `resized()`.

DecayGraph sits at the bottom for all non-Convolution modes.

The CSS `zoom: 0.88` on the React side (line 570) is reproduced by sizing the panel's internal `Rectangle` smaller and laying out at the smaller pixel sizes. We do NOT apply an AffineTransform — pixel snapping stays clean.

### OutputBar — `panels/OutputBar.{h,cpp}`

The `zoom: 0.85` from app.jsx line 690 maps to a 73 px tall row.

Children: icon + heading Label, knob set (Dry/Wet Md / WidthDial / Duck Md / 3-BAND chip + cluster label / DuckLow sm / DuckHigh sm / Output Md / AutoGain chip + status label), divider, two big `LedMeter` instances (L + R, big variant).

Duck Low/High knobs use `setAlpha(0.4f)` and `setInterceptsMouseClicks(false, false)` when `duckMultiband == false`. The `juce::ParameterAttachment` for `duckMultiband` (Bool) calls a method on OutputBar that flips these.

---

## 6. Modals

All modals derive from `ModalLayer` (`modals/ModalLayer.{h,cpp}`).

```cpp
class ModalLayer : public juce::Component {
public:
    void setOpen (bool, juce::Component* anchor = nullptr);  // toggles a scrim + child enter anim
    // Owns:
    //   - a full-window scrim (sized to parent on resize)
    //   - one child Component as the content
    //   - animation state for slide-in / fade
protected:
    virtual void paintContent (juce::Graphics&) = 0;
    virtual juce::Rectangle<int> contentBounds() const = 0;
};
```

Each modal is added to the root NativeEditor at construction time and toggled visible. They sit at the top of the editor's child list (last addAndMakeVisible), so JUCE z-orders them above the panels. The scrim paints `juce::Colour(0xa6000000)` (65% black) and intercepts mouse to dismiss.

### PresetBrowser — `modals/PresetBrowser.{h,cpp}`

Children: search `TextEditor`, category chips row (8 chips: ALL/USER/DUB/AMBIENT/VINTAGE/WIDE/OTHER), a `juce::ListBox` for rows, footer with count + Close button.

Data: `PresetIo::listFactory()` + `PresetIo::listUser()` calls (wrap `PresetManager::getFactoryNames()` and `getUserNames()`). `categoryOf` heuristic ports from `viz.jsx` lines 229–237.

Row click: calls `PresetManager::loadByName`. Dirty-confirm: if `processor.isCurrentPresetDirty()`, opens nested `ConfirmDialog` with Cancel / Discard / Save buttons (matching `viz.jsx` lines 342–358).

### IrPicker — `modals/IrPicker.{h,cpp}`

Two-part: an inline strip Component (the `.ir-pick` row that sits inside ReverbPanel when convolution mode is active), and an opt-in nested modal browser opened by "Browse" with the factory IR list. "Load file..." → `juce::FileChooser` (matches `WebEditor.cpp` lines 295–310). "Clear" → `processor.clearIR()`.

The factory list comes from `doobie::factoryIRNames()` (already wired in WebEditor.cpp line 278).

### SavePresetDialog — `modals/SavePresetDialog.{h,cpp}`

Modal with title, instruction text, a `juce::TextEditor` input, Cancel + Save buttons. Default value = current preset name. Enter key = Save. Calls `PresetManager::saveUser(name)`.

Carries forward the "pending switch after save" chain: a `std::optional<juce::String> pendingLoadAfterSave` member; if set on Save, the editor schedules a `MessageManager::callAsync` to `loadByName(pending)` after `saveUser` returns.

### ConfirmDialog — `modals/ConfirmDialog.{h,cpp}`

Generic 3-button (Cancel / Discard / Save…). Used by:
- PresetBrowser row click on dirty,
- HeaderBar prev/next on dirty.

### ModDrawer — `modals/ModDrawer.{h,cpp}`

Bottom-sheet style, slides up from the editor's bottom edge over a 300 ms cubic-bezier. Implemented via a `juce::ComponentAnimator` move from `y = root.height` to `y = root.height - drawerHeight`.

Two tabs (Sources / Matrix), each a sub-Component.

**Sources tab** — grid of 5 cards (4 `LfoCard` + 1 `EnvelopeCard`):
- `LfoCard` (5 atom-knobs Rate/Depth/Smooth/Offset + DropDown wave + LfoScope + sync chip + sync-div dropdown). 4 instances.
- `EnvelopeCard` (EnvViz + 3 knobs Atk/Rel/Sens + filter chip/seg/cutoff/res).

**Matrix tab** — 8 `MatrixRow` instances. Each row has:
- Index label
- Source DropDown
- WaveMini (compact scope of the selected source's live value)
- "→" arrow label
- Destination DropDown
- Bi/Uni Selector (compact mm-mode-seg variant)
- Amount Slider (custom-painted: native track + live-fill overlay + live-dot + zero-marker, matching the JSX overlay maths)
- Amount readout Label

The amount Slider is a regular `juce::Slider` with a custom LookAndFeel paint that adds the overlay. Live-fill / dot derive from `sourceLive(slot.src) * slot.amt`, where `sourceLive` reads the same LevelsBroadcaster cache (LFO1..4 or env, mapped per `juce-bridge.jsx` lines 224–225).

---

## 7. Live-data path

**Recommendation**: ONE 30 Hz editor-level `LevelsTimer` on `NativeEditor`, pushing into a `LevelsBroadcaster`.

```cpp
struct LevelsSnapshot {
    float inDb, delayDb, reverbDb, outLDb, outRDb, outMonoDb;
    float headMag[4];
    int   midiNote;
    float grDb;
    float env;
    float lfo[4];
    // preset
    juce::String presetName, presetCat;
    bool         presetDirty;
    // IR
    bool         hasIR;
    int          factoryIRIndex;
    bool         isFactory, isFile;
    juce::String irName;
};

class LevelsBroadcaster {
public:
    using Listener = std::function<void(const LevelsSnapshot&)>;
    int addListener (Listener);     // returns id
    void removeListener (int id);
    void publish (const LevelsSnapshot&);   // called by LevelsTimer
};
```

Each viz registers as a listener when it's added to the tree; `LedMeter::levelsChanged()` reads its slot's dB, `LfoScope` reads its lfo[n], `EnvViz` reads env, etc.

**Why one timer not many**: matches the WebView's single `levels` event so behaviour is identical. Lower overhead than 6 viz-local timers. Easier to keep all the visualisations phase-locked. PluginProcessor.h's getter atomics are lock-free so reading from the message thread is safe.

**Why 30 Hz**: matches the existing WebView (`WebEditor.cpp` line 403), matches the `mounts.jsx` smoothing constants. Doubling to 60 Hz would tighten meter response but the rAF-based smoothers in JSX already produce a 60-effective-Hz visual repaint; we'll port that same separation — 30 Hz data push, per-viz 60 Hz `Timer::repaint()` for smoothing.

The TapeDeck animation is independent of LevelsTimer — it has its own 30 Hz local Timer driving the reel angle. That isolates it from data updates.

---

## 8. APVTS attachments

**Recommendation**: every continuous control derives from `juce::Slider` so we get `juce::SliderParameterAttachment` for free. That means:
- Knob → Slider subclass (already proposed in §3) → `SliderAttachment`.
- WidthDial → Slider subclass → `SliderAttachment`.
- Fader → Slider subclass → `SliderAttachment`.
- DropDown → ComboBox subclass → `ComboBoxAttachment`.
- Chip (latching) → Button subclass → `ButtonAttachment`.
- PowerButton → Button subclass → `ButtonAttachment`.

For **Selector** (segmented multi-button driving a Choice param), JUCE has no built-in attachment. Two options:

**(A) — write a `SelectorAttachment`** that wraps `juce::ParameterAttachment` (lower-level, available since JUCE 6):
```cpp
class SelectorAttachment {
public:
    SelectorAttachment (juce::RangedAudioParameter& p, Selector& s, juce::UndoManager* u = nullptr)
        : selector (s), storage (p, [this](float v) { onParamChanged(v); }, u) {
        selector.onChange = [this](int idx) { storage.setValueAsCompleteGesture ((float) idx); };
        storage.sendInitialUpdate();
    }
private:
    Selector& selector;
    juce::ParameterAttachment storage;
    void onParamChanged (float v) { selector.setSelectedIndex ((int) std::round (v)); }
};
```

This is the cleanest path — same shape as JUCE's own attachments, undo-aware, host-automation-safe.

**(B)** — make `Selector` internally own a hidden `juce::ComboBox` and bridge via `ComboBoxAttachment`. Rejected: extra widget, extra value-tree churn, ugly hack.

**Plan**: ship (A). Lives in `support/ParamBinding.{h,cpp}` alongside small helpers for the per-head bindings (an `attachAllHeads(apvts, panel)` that wires the 4×5 head params in one call), and a `MomentaryButtonAttachment` for Kill FB.

For the momentary Kill FB Chip:
```cpp
class MomentaryButtonAttachment {
    Chip& chip;
    juce::ParameterAttachment storage;
public:
    MomentaryButtonAttachment (juce::RangedAudioParameter& p, Chip& c, juce::UndoManager* u)
        : chip(c), storage(p, [&](float){}, u) {
        chip.setMomentary ([this](bool pressed) {
            storage.setValueAsCompleteGesture (pressed ? 1.0f : 0.0f);
        });
    }
};
```

---

## 9. Mod matrix wiring

The React side's `useJuceModMap` (juce-bridge.jsx lines 183–260) returns two maps keyed by APVTS param id: `ranges[paramId]` (half-amplitude) and `live[paramId]` (signed offset right now).

**Native equivalent — `support/ModMap.{h,cpp}`**:

```cpp
class ModMap : private juce::Timer {
public:
    explicit ModMap (juce::AudioProcessorValueTreeState& apvts, LevelsBroadcaster& levels);

    struct Entry { float range01 = 0.0f; float liveOff01 = 0.0f; };
    Entry get (juce::StringRef paramId) const;

    using Listener = std::function<void()>;
    int addListener (Listener);
    void removeListener (int);

private:
    void timerCallback() override;
    void recompute();
    juce::AudioProcessorValueTreeState& apvts;
    LevelsBroadcaster& levels;
    std::unordered_map<juce::String, Entry> map;
    // ...
};
```

Inputs to `recompute()`:
- Per-slot src/dst/amt parameter values, read from `apvts.getRawParameterValue(modSlotSrc[i])` etc.
- Per-slot mode (Bipolar/Unipolar) — same APVTS read.
- Per-LFO depth + env sens from APVTS (mirrors juce-bridge.jsx lines 194–200).
- Live source values from `LevelsBroadcaster` (LFO1..4 + env).

`recompute()` runs on each LevelsBroadcaster publish (30 Hz). Same DEST_TO_PARAMS table from juce-bridge.jsx ports into a `static const std::unordered_map<int /* ModDest enum */, std::vector<juce::String /* param ids */>>`.

Each Knob in the tree subscribes via `ModMap::addListener` and, when notified, calls `ModMap::get(myParamId)` and pushes the result into its `setModRange` / `setModLiveOffset`. The lookup is O(1) hash, the listener cost trivial.

For per-head destinations (Head N Pan / Time / Offset / Level) the same mechanism applies — `panels.jsx` lines 197–202 lookup pattern ports directly.

**No new processor-side getters needed.** The processor already publishes lfo1..4 + env in its atomics; the LevelsBroadcaster already pushes them. ModMap is purely a derived computation on the editor side, exactly like the JS hook is purely client-side.

---

## 10. WebView removal strategy

**Recommendation: option A — CMake gated dual-build**, but the default flips immediately to native.

Why not just delete WebView: the WebView path is the working production UI today. A flag gives us:
1. A safety net if the native port regresses something visually critical.
2. A path to ship the rewrite without a "bridges burned" moment.
3. The ability to A/B-test specific panels by running both editors side-by-side during dev (a developer can `cmake -DDOOBIE_LEGACY_WEBVIEW=ON` to compare).

The flag is **default OFF** post-Phase-6 (= native is the shipped editor).

### CMake changes

```cmake
option(DOOBIE_LEGACY_WEBVIEW "Build with the legacy WebView UI" OFF)

target_compile_definitions(Doobie PUBLIC
    DOOBIE_LEGACY_WEBVIEW=$<BOOL:${DOOBIE_LEGACY_WEBVIEW}>)

if(DOOBIE_LEGACY_WEBVIEW)
    target_sources(Doobie PRIVATE src/ui/WebEditor.cpp)
    target_link_libraries(Doobie PRIVATE DoobieUI juce::juce_gui_extra)
    target_compile_definitions(Doobie PUBLIC JUCE_WEB_BROWSER=1)
else()
    # NativeEditor + atoms + viz + panels + modals + support
    file(GLOB_RECURSE DOOBIE_NATIVE_UI_SRC CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/native/*.cpp")
    target_sources(Doobie PRIVATE ${DOOBIE_NATIVE_UI_SRC})
    target_compile_definitions(Doobie PUBLIC JUCE_WEB_BROWSER=0)
endif()
```

The `NEEDS_WEB_BROWSER TRUE` line in `juce_add_plugin` (CMakeLists line 69) also gates on the flag — we need a CMake conditional there.

### Source changes

`PluginProcessor.cpp::createEditor()` (line 697) becomes:
```cpp
juce::AudioProcessorEditor* DoobieAudioProcessor::createEditor()
{
   #if DOOBIE_LEGACY_WEBVIEW
    return new doobie::WebEditor (*this);
   #else
    return new doobie::ui::NativeEditor (*this);
   #endif
}
```

### Files touched either way

- **CMakeLists.txt** — option, conditional juce_add_plugin args, conditional target_sources.
- **src/PluginProcessor.cpp** — `#if` in createEditor (~3 lines).
- **src/ui/WebEditor.{cpp,h}** — kept under the flag, never deleted until we cut the flag.
- **src/PluginEditor.{cpp,h}** — orthogonal legacy editor; survives independently as a third option only if anyone cares (recommend: **delete in Phase 6** as it's been dead code since the WebView shipped).
- **DoobieUI BinaryData target** — only linked under the flag.

### Cleanup once we cut the flag (post-stabilisation, separate PR)

Delete: `src/ui/{WebEditor.cpp,WebEditor.h,PluginEditor.cpp,PluginEditor.h,LookAndFeel.cpp,LookAndFeel.h,VuMeter.cpp,VuMeter.h,EchoVisualiser.cpp,EchoVisualiser.h,ReverbResponseView.cpp,ReverbResponseView.h,VectorCassette.h,ModMatrixPopup.h,ModSourceMeter.h}` and the entire `ui/` (React sources) folder + `ui/vendor`. Remove `DoobieUI` target. Remove `juce::juce_gui_extra` link if nothing else needs it (it's needed for FileChooser → keep it). Drop the `NEEDS_WEB_BROWSER TRUE` flag from `juce_add_plugin`. Save ~12 MB of vendored React/Babel binary data.

---

## 11. Phase / task decomposition for delegation

Each task is ~30–90 min for a focused agent. "Verification" is the gate the head uses to mark it done. Deps are by task ID.

### Phase 0 — Foundation (1 agent each, sequential)

**T0.1 — Style tokens + LookAndFeel skeleton**
- Deliverable: `src/ui/native/Style.h`, `src/ui/native/DoobieLookAndFeel.{h,cpp}` with the palette constants from §2, font helpers, empty draw-* method stubs that compile.
- Verification: file compiles into a tiny standalone harness; palette colours match the hex table above byte-for-byte.
- Deps: none.

**T0.2 — Palette/draw routines — knob+chip+seg+well+power+header-rule**
- Deliverable: implementations of all drawing routines listed in §2.
- Verification: a `tools/Doobie_lnf_preview.cpp` snapshot tool (modelled on the existing `tools/Snapshot.cpp`) renders a swatch grid PNG showing each routine with its on/off variants; head visually diffs against `doobie.css`.
- Deps: T0.1.

**T0.3 — Audit + scaffolding**
- Deliverable: empty class+header files for every atom/viz/panel/modal listed in §1 (paint = fill background bright pink, resized = no-op). `NativeEditor.{h,cpp}` containing a single `NativeEditor : public AudioProcessorEditor` that draws a 1520×960 root with the right body grid math but no children yet (or just panel-coloured rectangles). Wire it as the secondary editor under `DOOBIE_LEGACY_WEBVIEW=OFF`.
- Verification: plugin opens in standalone host with `-DDOOBIE_LEGACY_WEBVIEW=OFF`, shows a 1520×960 pink-and-panel grid. No crashes.
- Deps: T0.1, T0.2.

### Phase 1 — Atoms (parallel)

**T1.1 — Knob** (Knob.{h,cpp}, ValueOverlay.{h,cpp})
- Deliverable: Knob renders arc + body + pointer + ticks (Md size first) at fixed 0.6 value; supports drag (240 px range, shift fine); supports `setBipolar`, `setLabel`, `setValueFormatter`, `setLit`, `setArcColor`, `setModRange`, `setModLiveOffset`. ValueOverlay shows during drag.
- Verification: snapshot tool renders 4 Knobs (sm/md/lg/lg-bipolar) matching `knob.jsx` geometry pixel-for-pixel on a flat panel background. Dragging changes value, double-click resets.
- Deps: T0.2.

**T1.2 — Fader** (Fader.{h,cpp})
- Deliverable: vertical fader with rail + accent fill + cap, inline meter overlay (driven by `setMeter()`), drag math (height-relative).
- Verification: 4 faders side by side, three at different values, one being dragged; ValueOverlay floats above; meter responds to a sine-test driver.
- Deps: T0.2.

**T1.3 — Chip + PowerButton + Selector + DropDown** (Chip.{h,cpp}, PowerButton.{h,cpp}, Selector.{h,cpp}, DropDown.{h,cpp})
- Deliverable: all four atoms with both compact + standard variants where applicable.
- Verification: snapshot grid shows on/off + hover states.
- Deps: T0.2.

**T1.4 — LedMeter + VuMeter** (LedMeter.{h,cpp}, VuMeter.{h,cpp})
- Deliverable: horizontal LED bars (small + big variants), with peak hold + clip latch. VuMeter = two stacked LedMeters.
- Verification: snapshot shows -12, 0, +3 dB three meters with peak markers, scale labels, clip light lit at +3.
- Deps: T0.2.

**T1.5 — PanelHeader + ClusterLabel + Icons** (PanelHeader.{h,cpp}, ClusterLabel.{h,cpp}, Style.h::Icon::*)
- Deliverable: PanelHeader composes title + icon + meta + action slot. Style::Icon namespace holds 6 named `juce::Path` builders (`in`, `heads`, `delay`, `fb`, `rev`, `out`) reproduced from the JSX SVG path data.
- Verification: snapshot of 6 PanelHeaders in a column.
- Deps: T0.2.

**T1.6 — KnobContextMenu** (KnobContextMenu.{h,cpp})
- Deliverable: a singleton popup component spawned from `NativeEditor::openKnobMenu(Point<int> screen, Knob*)`. Items: Reset / Copy / Paste / Enter value. Enter-value opens an inline TextEditor.
- Verification: right-click on Knob from T1.1 opens menu at cursor, items invoke correctly.
- Deps: T1.1.

### Phase 2 — Visualisations (parallel after Phase 0 + T1.4)

**T2.1 — TapeDeck** (TapeDeck.{h,cpp})
- Deliverable: animated tape loop. Reels rotate, strands present, head markers positioned at h.time × strand-length, recording dot pulses, speed-multiplier knob hooked up.
- Verification: snapshot animation comparison vs. running WebView tape (head will capture a short video and diff). Reel angular velocity must match the JS-side at the same speed value within ±5%.
- Deps: T0.2.

**T2.2 — StereoScope** (StereoScope.{h,cpp})
- Deliverable: 128-sample rolling history per channel, drawn with L-up / R-down deflection, newest-on-left, scoped to a Rectangle the parent passes in. Push API `pushPeak(float, float)`.
- Verification: when fed a 1 kHz sine on L, an idle on R, the scope's upper waveform deflects to a steady-ish ridge; lower stays flat. No artefacts at buffer wraparound.
- Deps: T0.2.

**T2.3 — DecayGraph** (DecayGraph.{h,cpp})
- Deliverable: static curve drawn from a single decay-knob value; gradient fill below; RT60 text top-right. Repaints on decay change.
- Verification: snapshot at decay=0.2 / 0.6 / 0.9 matches the JSX curve shape.
- Deps: T0.2.

**T2.4 — LfoScope + WaveMini** (LfoScope.{h,cpp}, WaveMini.{h,cpp})
- Deliverable: rolling 96-sample history, scope variant + compact (matrix-row) variant. Static-shape preview fallback when no value pushed.
- Verification: snapshot of 6 WaveMinis showing live trace for sine/triangle/saw-up/saw-down/square/S&H source waveforms.
- Deps: T0.2.

**T2.5 — EnvViz** (EnvViz.{h,cpp})
- Deliverable: horizontal bar + LED + numeric readout. Tint reddens above 0.7.
- Verification: snapshot at level=0.0/0.3/0.85.
- Deps: T0.2.

### Phase 3 — Panels (parallel after Phase 1 + relevant Phase 2)

Each panel task includes (a) layout in `resized()`, (b) atom instantiation, (c) APVTS attachments via the helpers from §8, (d) ModMap subscriptions for all KB-equivalent knobs.

**T3.1 — InputPanel** — deps T1.1, T1.3, T1.5.
**T3.2 — HeadsPanel** + HeadStrip — deps T1.1, T1.2, T1.5.
**T3.3 — DelayPanel** + TapeScreen sub-component — deps T1.1, T1.3, T1.5, T2.1, T2.2.
**T3.4 — PitchShifterPanel** — deps T1.1, T1.3, T1.5.
**T3.5 — FeedbackPanel** (incl. live-note readouts) — deps T1.1, T1.5.
**T3.6 — PhaserPanel** — deps T1.1, T1.3, T1.5.
**T3.7 — ReverbPanel** (all four sub-Components for the mode branches) — deps T1.1, T1.3, T1.5, T2.3.
**T3.8 — OutputBar** — deps T1.1, T1.3, T1.4, T1.5.
**T3.9 — HeaderBar + FlowBar** — deps T1.3, T1.4, T1.5.

Verification for each: snapshot tool diffs the panel against the corresponding running-WebView screenshot the head captures up-front. Acceptance threshold: every control present, every label readable, every chip/knob in approximately the same position; tolerance ±3 px in the body layout, ±1 px within a panel.

### Phase 4 — Modals + support (parallel after Phase 1)

**T4.1 — ModalLayer + ConfirmDialog** — deps T1.3.
**T4.2 — SavePresetDialog** — deps T4.1.
**T4.3 — PresetBrowser** — deps T4.1, T4.2 (for nested dirty-confirm flow).
**T4.4 — IrPicker (inline strip + nested browser)** — deps T4.1.
**T4.5 — ModDrawer (Sources tab — LfoCard ×4 + EnvelopeCard)** — deps T1.1, T1.3, T2.4, T2.5.
**T4.6 — ModDrawer (Matrix tab — 8 MatrixRows)** — deps T1.1, T1.3, T2.4, T4.5.

**T4.7 — LevelsBroadcaster + LevelsTimer + Format helpers** — deps none from Phase 1+2; can run in parallel with the visualisation work. `support/{LevelsTimer,Format}.{h,cpp}`.

**T4.8 — ModMap** — deps T4.7. `support/ModMap.{h,cpp}`.

### Phase 5 — Integration (sequential)

**T5.1 — NativeEditor body grid layout**
- Wire HeaderBar, FlowBar, the 3 columns (`col-left`, `col-mid`, `col-right`), OutputBar at the bottom.
- Deps: T3.1–T3.9.

**T5.2 — Wire LevelsTimer → broadcaster → all visualisations + meters**
- Deps: T5.1, T4.7.

**T5.3 — Wire ModMap → all mod-aware Knobs**
- Deps: T5.1, T4.8.

**T5.4 — Wire all modals to NativeEditor + HeaderBar**
- Header Mod button toggles ModDrawer. PresetBrowser opens from preset-name click. SavePresetDialog from Save button. IrPicker stays inline in ReverbPanel. ConfirmDialog flows for dirty preset nav.
- Deps: T5.1, T4.1–T4.6.

**T5.5 — Per-head anti-collision + sync-mode time-snap port**
- The two non-trivial cross-panel behaviours: HeadsPanel `clampTime` (already part of T3.2 deliverable, but verified end-to-end here) and DelayPanel sync→syncDiv stepping (already part of T3.3, verified here).
- Deps: T5.1.

### Phase 6 — WebView removal + smoke (sequential)

**T6.1 — CMake gating + createEditor #if**
- Add `DOOBIE_LEGACY_WEBVIEW` option, wire all conditionals. Default OFF.
- Deps: T5.4.

**T6.2 — Smoke test pass**
- Standalone host: open, load every factory preset, every reverb mode, drag every knob.
- VST3 + AU: load in Reaper + Logic, automate a few params, check the editor opens/closes cleanly under host reset.
- Linux build under WSL or a Linux runner (CI's already there) — `cmake -DDOOBIE_LEGACY_WEBVIEW=OFF` should produce a plugin that loads.
- Deps: T6.1.

**T6.3 — Delete dead WebView code (separate PR, gate-merged later)**
- Drop the option, delete `WebEditor.*`, `PluginEditor.*`, all legacy `src/ui/*` non-native files, drop `DoobieUI` target, drop `NEEDS_WEB_BROWSER`, drop `JUCE_WEB_BROWSER`, drop the `ui/` React source folder.
- Verification: build + tests + linux/macos CI all green.
- Deps: at least a week of T6.2 stability.

---

## 12. Visual fidelity preservation

Agents working in parallel will drift unless given a shared truth. Recommend:

1. **Spec doc** committed at `docs/native-ui-spec.md` (written by T0.3 author). Contains:
   - The OKLCH → hex table from §2 verbatim.
   - The Style.h constants verbatim.
   - For each atom: a 1–2 line description with the doobie.css line numbers that define its CSS (e.g. "Knob — see knob.jsx Knob; ticks knob.jsx L80–87; arc geometry L89–99; colours doobie.css L216–227").
   - For each panel: the slice arithmetic the panel uses (see §5).

2. **Reference screenshots** under `docs/native-ui-spec/refs/`. Head captures these up-front from the running WebView build at:
   - `bridge.png` — the FlowBar + VU strip
   - `header.png`
   - `delay-free.png`, `delay-sync.png`
   - `heads-on.png`, `heads-off.png`
   - `input.png`, `input-filter-on.png`
   - `feedback.png`
   - `phaser.png`
   - `reverb-plate.png`, `reverb-shimmer.png`, `reverb-convolution.png`, `reverb-gated.png`
   - `pitch-shifter.png`
   - `outputbar.png`, `outputbar-multiband.png`
   - `moddrawer-sources.png`, `moddrawer-matrix.png`
   - `preset-browser.png`, `ir-picker.png`, `save-dialog.png`, `confirm-dialog.png`

3. **Task brief pattern** the head uses when posting each panel/atom task to the hive board:
   > "Reproduce the visual + interaction of `panels.jsx::ReverbPanel` (lines 535–680) and `doobie.css` `.panel.compact` + `.eqrow` + `.shimmer-int*` rules (lines 161–168, 315, 727–745). Reference screenshot: `docs/native-ui-spec/refs/reverb-plate.png`. Use atoms from §3; layout from §5."

4. **Snapshot-test rig**: the existing `tools/Snapshot.cpp` already drives the editor headlessly. Extend it with a `--panel=reverb` flag that creates only one panel and dumps it to PNG. Each panel task's verification step uses this to produce a per-panel diff.

---

## 13. Risk register

**R1 — Arc geometry math drift between JSX and JUCE.**
The arc in `knob.jsx` uses `polar(cx, cy, r, deg)` where deg=0 is top, sweeping ±135° (CSS y-axis is down; the trig handles that with `cx + r*sin(rad)`, `cy - r*cos(rad)`). JUCE's `drawRotarySlider` API speaks in `startAngle` / `endAngle` radians where 0 is up. Risk: off-by-90°, mirrored, or 1-px misalignments compound across the mod-range arc, mod-dot, tick line endpoints.

**Mitigation**: T0.2 ships with the exact same `polar` helper ported character-for-character (`polar(cx, cy, r, deg)` returning `{ cx + r * std::sin(deg*pi/180), cy - r * std::cos(deg*pi/180) }`) and the same `A0=-135, A1=135, SWEEP=270` constants. Build the snapshot of a Md-size Knob at 0.5 first, byte-diff against a screenshot from the running WebView — if anything is off, fix the trig before any panel work starts.

**R2 — Mod indicator live offset latency.**
The JSX side runs `useJuceEvent('levels')` which triggers a React re-render at 30 Hz; the indicator dot moves in lockstep with the event. The native side computes the same thing in `ModMap::recompute()` (also at 30 Hz) and pushes via listener. Risk: a forgotten listener, a stale read, or a Knob that never subscribes results in a dead dot — silently looking like the matrix isn't routing.

**Mitigation**: ModMap subscriptions live in `Knob::setModParamId(juce::StringRef)`. T5.3 verification includes a "mod sanity test": set up a 1 Hz LFO → Delay Time slot with full amount, the Time knob's dot must visibly sweep its full mod-arc width once per second. Add an editor-level integration test (`tests/NativeEditorModTest.cpp`) that drives this end-to-end.

**R3 — Tape SVG translation to JUCE Path.**
`ui/src/tape.js` is imperative SVG: strands as polylines with computed sag, reels as `<circle>` with `<animateTransform>`, head markers as `<g>` instances. Translating to JUCE Path means re-coding all of that geometry by hand. Risk: dropped detail, wrong reel size, head markers landing in the wrong x, missing scrolling tape diamonds.

**Mitigation**: T2.1 is its own task with its own snapshot diff against a TapeDeck-only WebView capture. The head captures a 1-second video of both at the same speed multiplier and visually diffs.

**R4 — Modal z-order with multiple stacked layers.**
PresetBrowser's dirty-confirm opens a second modal on top (`viz.jsx` lines 342–358). SavePresetDialog can be opened from inside the browser or from the header. The drawer can be open while a modal is shown. Risk: scrim doesn't capture click, child draws under sibling, focus lost.

**Mitigation**: ModalLayer makes the z-stack explicit — each modal `setOpen(true)` pushes itself to the front of the editor's child order via `toFront(true)`. Confirmation modals use a +1 z-bias to always paint over the modal that spawned them. Smoke test in T6.2 walks every two-deep modal combo.

**R5 — Knob right-click menu cross-platform.**
JUCE's `PopupMenu` works everywhere but doesn't match the custom paint. Our `KnobContextMenu` is a custom Component placed at screen coords. Risk: outside-window placement, missing dismissal on click-elsewhere on Linux, Enter-value `TextEditor` not getting focus on Windows.

**Mitigation**: Same modal-layer treatment — the menu attaches to `NativeEditor` as a top-level child positioned via `getScreenPosition()` math, dismisses on a `MouseListener::mouseDown` registered on `Desktop`. Test on macOS + Windows + Linux as part of T6.2.

**R6 (additional) — Font availability.**
Space Grotesk and JetBrains Mono are loaded from a CDN in the WebView. Native JUCE has no CDN. Either bundle the fonts as BinaryData (~150 KB) or fall back to system stack. The default fallback `ui-sans-serif, system-ui, sans-serif` produces noticeably different metrics.

**Mitigation**: bundle the WOFF2 files for both fonts into a small `DoobieFonts` BinaryData target, load via `juce::Typeface::createSystemTypefaceFor(BinaryData::SpaceGrotesk_woff2, ...)` in `DoobieLookAndFeel`'s constructor. Adds ~150 KB to the binary; acceptable. T0.1 includes this work.

---

### Critical Files for Implementation

- /Users/syso/dev/priv/doobie/CMakeLists.txt
- /Users/syso/dev/priv/doobie/src/PluginProcessor.cpp
- /Users/syso/dev/priv/doobie/src/ParameterIDs.h
- /Users/syso/dev/priv/doobie/src/ui/WebEditor.cpp
- /Users/syso/dev/priv/doobie/ui/src/doobie.css
