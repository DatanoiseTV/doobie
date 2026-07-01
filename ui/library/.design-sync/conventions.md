# Building with the Doobie design system

Doobie is an analog dub-delay audio plugin. These are its real interface
components — dark charcoal surfaces, an amber accent, and dense studio-hardware
layouts. Build with them exactly as the plugin does.

## Setup and state — no context provider

Components take **plain props**, not React context. There is nothing to wrap.
Parameter-driven panels read from a flat `p` object and write through a
`setP(key, value)` callback:

- `p` — an object of current values keyed by design id (`p.feedback`, `p.mix`,
  `p.character`, …). Numeric params are normalised `0..1`; choices are strings;
  toggles are booleans.
- `setP(key, value)` — writes one value back.
- `mods` — modulation map: `mods[paramId]` is a knob's mod-arc half-range and
  `mods.live[paramId]` its live offset. Pass `{ live: {} }` when unused.
- `levels` — a live-metering frame (dBFS stage levels, `headMag[]`, `peak{}`,
  LFO/env values) for meters and scopes.
- `heads` — array of the four tape playback heads `{ id, on, level, pan, time,
  offset }`.

The single full-interface component, `App`, is the whole plugin wired together;
in the plugin it binds `p`/`setP` to the audio engine, but every other component
is a pure function of its props and renders standalone.

## Styling idiom — global classes + CSS custom properties

There are **no utility classes and no CSS-in-JS**. The entire look lives in one
stylesheet (`_ds_bundle.css`, imported by `styles.css`) as semantic class names
(`.knob`, `.fader`, `.chip`, `.panel`, `.dm`, `.sel`, `.phead`, `.hdr`) driven by
`:root` custom properties. To restyle, override the tokens — do not hand-write
component CSS.

Core tokens (all `oklch`):

| Token | Role |
|---|---|
| `--accent` / `--accent-dim` / `--accent-glow` | amber accent (arcs, LEDs, active states) |
| `--peak` | red peak / danger (clip, near-self-oscillation feedback) |
| `--c-bg` `--c-panel` `--c-panel-2` `--c-raise` `--c-inset` `--c-well` | charcoal surface ramp |
| `--c-txt` `--c-txt-2` `--c-txt-3` | text (primary → dim micro-labels) |
| `--c-line` `--c-line-2` | hairline borders |
| `--r-lg` `--r-md` `--r-sm` | corner radii |
| `--font` (Space Grotesk) / `--mono` (JetBrains Mono) | type |

Micro-labels are uppercase, letter-spaced, in `--c-txt-2`/`--c-txt-3`. Read
`_ds_bundle.css` for the full token set and each component's `.prompt.md` for its
props and a usage example.

## One idiomatic composition

```jsx
// A section of controls, doobie-style: a titled panel of knobs + a toggle chip.
<div className="panel">
  <PHead title="Feedback" meta="in-loop tone" />
  <div style={{ display: 'flex', gap: 20, alignItems: 'flex-end' }}>
    <Knob size="lg" label="Feedback" value={p.feedback}
          onChange={(v) => setP('feedback', v)}
          format={(v) => Math.round(v * 100) + '%'} />
    <Knob label="Low Cut" value={p.fbLowCut} onChange={(v) => setP('fbLowCut', v)} />
    <Knob label="High Cut" value={p.fbHighCut} onChange={(v) => setP('fbHighCut', v)} />
  </div>
  <Chip on={p.freeze} onClick={() => setP('freeze', !p.freeze)}>Freeze</Chip>
</div>
```
