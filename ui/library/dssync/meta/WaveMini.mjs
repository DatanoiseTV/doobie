export default {
  name: 'WaveMini',
  group: 'Visualizers',
  summary: 'Mini LFO oscilloscope — draws the characteristic shape of the selected waveform, or a live rolling trace when a value is plumbed in.',
  stories: [
    { label: 'Shapes', jsx: `h('div',{style:{display:'flex',gap:14,alignItems:'flex-end',flexWrap:'wrap'}},
        ['Sine','Triangle','Saw Up','Square','Random S&H'].map(function(s){
          return h('div',{key:s,style:{width:120}},
            h('div',{style:{font:'10px system-ui',letterSpacing:'.08em',textTransform:'uppercase',opacity:0.6,marginBottom:4}}, s),
            h(D.WaveMini,{shape:s,rate:0.4,depth:0.6}));
        }))` },
  ],
  dts: `/** Waveform shape: 'Sine' | 'Triangle' | 'Saw Up' | 'Saw Down' | 'Square' | 'Random S&H'. */
shape?: string;
/** Normalised LFO rate, 0..1 — reference only (static preview draws a fixed number of cycles). */
rate?: number;
/** Normalised depth, 0..1 — scales the trace amplitude (floored at 0.15 so it stays visible). */
depth?: number;
/** Live LFO output in −1..1. When provided, plots a rolling scope; when null, draws the static shape preview. */
value?: number | null;`,
  doc: `A tiny LFO scope. With no \`value\` it draws a static, recognisable preview of the
selected \`shape\` (square is flat-flat, triangle has straight ramps, saw shows
teeth, S&H is a jagged pulse train) — this is what the Wave dropdown shows. Pass a
live \`value\` (−1..1) each frame and it switches to a rolling oscilloscope, newest
sample on the right, like hardware. \`depth\` scales the amplitude.

\`\`\`jsx
<WaveMini shape={p.lfo1Wave} rate={p.lfo1Rate} depth={p.lfo1Depth} value={levels.lfo1v} />
\`\`\``,
};
