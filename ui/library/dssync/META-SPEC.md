# Authoring `dssync/meta/<Name>.mjs` — spec for subagents

You author ONE file per component: `dssync/meta/<Name>.mjs`, an ES module with a
default export. The generator (`dssync/gen.mjs`) turns it into a claude.ai/design
preview card + `.d.ts` + `.prompt.md`. **Do not run gen.mjs, package-build,
package-validate, or any build** — the orchestrator generates and validates
centrally. Only write your assigned `meta/<Name>.mjs` files. Do not edit any
other file.

## The format

```js
export default {
  name: 'Fader',                 // MUST match the window.Doobie.<Name> export exactly
  group: 'Controls',             // one of: Controls, Meters, Visualizers, Panels, Dialogs, Full UI
  summary: 'One-sentence element-index line — what it is + its key props/variants.',
  cardPad: 24,                   // optional; px padding around the card. Use 0 for full-bleed overlays.
  stories: [
    // Each story = one labeled cell in the card. jsx is a STRING of JS evaluated
    // in the BROWSER with these locals in scope:
    //   D    = window.Doobie (all components: D.Fader, D.Knob, ...)
    //   demo = window.Doobie.__demo (shared demo state, see below)
    //   h    = React.createElement
    // Return a single React element from the expression.
    { label: 'Levels', jsx: `h('div',{style:{display:'flex',gap:24}},
        h(D.Fader,{value:0.8,label:'A',meter:0.6}),
        h(D.Fader,{value:0.5,label:'B',meter:0.3}))` },
  ],
  dts: `value?: number;
onChange?: (value: number) => void;
label?: string;`,               // BODY of the Props interface (fields only). Valid TS. Use JSDoc /** */ on fields.
  doc: `Longer usage prose for the .prompt.md, with a fenced <Component .../> example.`,
};
```

## The demo helper (`demo` = `window.Doobie.__demo`)

For connected panels/meters, DO NOT hand-build fixtures — use the shared demo
state (it is the exact populated state the real App feeds its panels):

- `demo.p` — live parameter object (read `p.feedback`, `p.character`, …). Pass as `p={demo.p}`.
- `demo.setP` — no-op setter. Pass as `setP={demo.setP}`.
- `demo.heads` — array of 4 head objects `{id,on,level,pan,time,offset}`. Pass as `heads={demo.heads}`.
- `demo.setHead`, `demo.setMx` — no-op setters.
- `demo.matrix` — 8-slot mod matrix array `{src,dst,amt,mode}`.
- `demo.mods` — modulation map `{}` with `.live` (empty → no mod arcs; fine).
- `demo.levels` — static levels frame (in/delay/reverb/out dBFS, headMag[], peak{}, lfo1v..4v, env). Pass as `levels={demo.levels}`.
- `demo.stages` — VU-strip stage array `[{label,base,peakDb}]`.
- `demo.presetInfo` — `{name:'Dub Chamber',cat:'DUB',dirty:false}`.
- `demo.irInfo` — `{hasIR:true,isFactory:true,factoryIndex:0,name:'Concert Hall',isFile:false}`.
- `demo.noop` — `() => {}`.

Example panel story: `h('div',{style:{width:520}}, h(D.DelayPanel,{p:demo.p,setP:demo.setP,heads:demo.heads,mods:demo.mods,levels:demo.levels,tapeSpeed:1.4,accent:'var(--accent)',midiNote:-1}))`

## Rules

- Read the component's SOURCE (paths given in your task) to get props EXACTLY
  right — required props must be provided or the cell renders a ⚠ error.
- Realistic content only — real labels/values, never `foo`/`test`.
- 1–3 stories per component. Atoms: sweep the main variant axis (size, on/off,
  bipolar). Panels: one populated instance (they're wide — one cell is enough).
  Overlays (Modal/PresetBrowser/ModDrawer/context menu): render the OPEN state,
  `cardPad: 0`.
- `summary` first line must be non-empty and descriptive (it's the design
  agent's element index).
- Match the doobie visual idiom: dark charcoal bg, amber accent, uppercase micro
  labels. The card chrome handles layout; you just provide the component.
- For components that need a width to look right, wrap in
  `h('div',{style:{width:NNN}}, h(D.Name,{...}))`.
