export default {
  name: 'VUStrip',
  group: 'Meters',
  summary: 'Top meter bridge — a chevron-separated row of DigitalMeters tracing signal flow IN › DELAY › REVERB › OUT.',
  stories: [
    { label: 'Meter bridge', jsx: `h('div',{style:{width:900}}, h(D.VUStrip,{stages:demo.stages}))` },
  ],
  dts: `/** Ordered signal stages; each renders a DigitalMeter, joined by › chevrons. */
stages: Array<{
  /** Stage label (e.g. "IN", "DELAY", "REVERB", "OUT"). */
  label: string;
  /** Live peak level in dBFS, passed straight to the stage's DigitalMeter. */
  peakDb: number;
  /** Optional pre-computed bar base (unused by the strip; kept for parity with the app's stage objects). */
  base?: number;
}>;`,
  doc: `The meter bridge that spans the top of the UI. It maps \`stages\` to a row of
\`DigitalMeter\`s separated by \`›\` chevrons, giving an at-a-glance view of the
signal as it moves through the input, delay, reverb, and output stages. Build the
array from the engine's \`levels\` frame. It's wide by design — give it the full
window width.

\`\`\`jsx
<VUStrip stages={[
  { label: 'IN',     peakDb: levels.peak.in },
  { label: 'DELAY',  peakDb: levels.peak.delay },
  { label: 'REVERB', peakDb: levels.peak.reverb },
  { label: 'OUT',    peakDb: levels.peak.out },
]} />
\`\`\``,
};
