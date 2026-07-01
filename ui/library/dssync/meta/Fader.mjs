export default {
  name: 'Fader',
  group: 'Controls',
  summary: 'Vertical mixer fader with a live VU-meter overlay — value fill, peak-held meter bar, drag-to-set with fine (Shift) mode and lit accent.',
  stories: [
    { label: 'Level row + meters', jsx: `h('div',{style:{display:'flex',gap:28}},
        h(D.Fader,{value:0.82,label:'A',meter:0.62,lit:true}),
        h(D.Fader,{value:0.55,label:'B',meter:0.35}),
        h(D.Fader,{value:0.3,label:'C',meter:0.12}))` },
    { label: 'With readout', jsx: `h(D.Fader,{value:0.7,label:'Out',meter:0.48,lit:true,format:(v)=>Math.round((v*72-72))+' dB'})` },
  ],
  dts: `/** Normalised fader position, 0..1. */
value?: number;
/** Called with the new normalised value on drag / double-click reset (0.7). */
onChange?: (value: number) => void;
label?: string;
/** Track height in px (also the drag-distance for a full 0..1 sweep). */
height?: number;
/** Formats the floating value readout from the normalised value. */
format?: (value: number) => string;
/** Lit styling (accent glow on the fill / cap). */
lit?: boolean;
/** Live VU magnitude, 0..1 — peak-held meter overlay (values >1 clip at 1.2 scale). */
meter?: number;`,
  doc: `The mixer-metaphor level control. Drag vertically to change (full track height =
full range; hold Shift for fine adjust); double-click resets to 0.7. The \`meter\`
prop drives a peak-held VU overlay animated at display refresh (20 ms attack /
180 ms release) independent of React renders — feed it the engine's per-channel
magnitude. Pass \`format\` to show a dB readout and \`lit\` for the accent treatment.

\`\`\`jsx
<Fader value={head.level} onChange={(v) => setHead(head.id, 'level', v)}
       label="A" meter={levels.headMag[0]} lit
       format={(v) => Math.round(v * 72 - 72) + ' dB'} />
\`\`\``,
};
