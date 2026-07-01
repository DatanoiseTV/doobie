export default {
  name: 'WidthDial',
  group: 'Controls',
  summary: 'Stereo-width dial rendered as a fan — the fan half-angle visualises the panorama spread, drag to change, double-click resets to 0.6.',
  stories: [
    { label: 'Widths', jsx: `h('div',{style:{display:'flex',gap:28,alignItems:'flex-end'}},
        h(D.WidthDial,{value:0.2,label:'Narrow'}),
        h(D.WidthDial,{value:0.6,label:'Width'}),
        h(D.WidthDial,{value:0.95,label:'Wide'}))` },
  ],
  dts: `/** Normalised width, 0..1 — drives the stereo-fan half-angle (8deg..72deg). */
value?: number;
/** Called with the new normalised value on drag / double-click reset (0.6). */
onChange?: (value: number) => void;
label?: string;
/** Formats the centre readout; default renders \`value * 200\` as a percentage. */
format?: (value: number) => string;`,
  doc: `A stereo-width / panorama control drawn as a fan whose opening angle grows with the
value — an at-a-glance picture of how wide the image is spread. Drag vertically to
change (240 px = full range, Shift for 1/4-speed fine); double-click resets to 0.6.
Default readout is \`value * 200\` % (so 0.5 reads as 100 %, i.e. unity width).

\`\`\`jsx
<WidthDial value={p.width} onChange={(v) => setP('width', v)} label="Width" />
\`\`\``,
};
