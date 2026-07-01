export default {
  name: 'DecayGraph',
  group: 'Visualizers',
  summary: 'Reverb decay envelope — filled exponential-decay SVG curve with the algorithm name and an estimated RT in seconds.',
  stories: [
    { label: 'Plate', jsx: `h('div',{style:{width:360}}, h(D.DecayGraph,{decay:0.6,type:'Plate'}))` },
    { label: 'Spring', jsx: `h('div',{style:{width:360}}, h(D.DecayGraph,{decay:0.4,type:'Spring'}))` },
  ],
  dts: `/** Normalised decay amount, 0..1 — longer tail as it approaches 1. */
decay?: number;
/** Reverb algorithm name shown in the caption (e.g. "Plate", "Spring", "Hall"). */
type?: string;
/** SVG height in px (default 116). */
height?: number;`,
  doc: `A read-only visualization of the reverb tail. It plots an exponential-decay
envelope whose steepness is derived from \`decay\` (higher = slower fall), fills it
with a top-down gradient, and captions it with the \`type\` name plus an estimated
decay time in seconds. Purely presentational — no interaction. Wrap in a
container ~360px wide.

\`\`\`jsx
<DecayGraph decay={p.reverbDecay} type={p.reverbType} />
\`\`\``,
};
