# Doobie — Brand & Visual Language

Doobie is an analog dub delay. The visual identity is a **vintage tape echo / spring
reverb unit**: think Roland RE-201 Space Echo crossed with a spring tank, warm and
tactile, but legible and modern under the skin.

## Personality
- Warm, analog, hand-built. Brushed metal, screen-printed labels, amber glow.
- Confident but not pretentious. No skeuomorphic gimmicks that hurt usability.
- Reads instantly: the big controls are TIME, FEEDBACK (INTENSITY), TONE, MIX.

## Colour palette
Names are used as constants in `src/ui/LookAndFeel`.

| Token            | Hex       | Use                                            |
|------------------|-----------|------------------------------------------------|
| panel            | `#1C1A17` | Main panel background (dark brushed metal)     |
| panelLight       | `#2A2622` | Raised sections, knob plates                   |
| panelShadow      | `#100F0D` | Recesses, drop shadows                         |
| cream            | `#E8DCC0` | Screen-printed text, light labels              |
| amber            | `#F4A024` | Primary accent: glow, active values, pointers  |
| amberDim         | `#8A5E1E` | Inactive amber, tick marks                     |
| teal             | `#3FB6A8` | Secondary accent: reverb section, meters peak  |
| red              | `#D8492E` | Clip / self-oscillation warning                |
| metal            | `#3A3631` | Knob bodies, switch caps                        |
| line             | `#4A443C` | Hairlines, dividers, panel screws              |

## Typography
- Labels: condensed uppercase, letter-spaced, cream. (JUCE default sans, tracked.)
- Values: amber, tabular figures.

## Lighting
- A single soft top-left light source. Knobs and switches cast subtle shadows down-right.
- The amber accents "glow" (additive halo) when a control is active or a value is hot.

## Layout zones
1. **Header** — logo "DOOBIE", subtitle "analog dub delay", build/version string.
2. **Mode selector** — rotary 12-position dial (Space Echo style) selecting head/reverb combos.
3. **Delay block** — TIME, FEEDBACK, the multi-head row, WOW/FLUTTER, SATURATION.
4. **Tone block** — BASS / TREBLE shelves + HP/LP feedback filters.
5. **Reverb block** — spring + plate, routing selector, decay/tone/mix. Tinted teal.
6. **Output** — MIX, output gain, stereo width, dual VU meters.

## Motion
- VU meters ballistically damped (VU-style ~300 ms integration).
- The echo visualiser shows decaying taps as fading amber dots on a time grid.
- Self-oscillation pushes the FEEDBACK ring from amber toward red.
