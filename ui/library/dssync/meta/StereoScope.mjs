export default {
  name: 'StereoScope',
  group: 'Meters',
  summary: 'Rolling stereo scope — L deflects up, R down from a centre axis, fed from the live peak levels frame (newest sample on the left).',
  stories: [
    { label: 'Stereo scope', jsx: `h('div',{style:{width:520}}, h(D.StereoScope,{levels:demo.levels}))` },
  ],
  dts: `/** Live levels frame — only \`peak.l\` / \`peak.r\` (per-channel dBFS) are read for the trace. */
levels: {
  peak?: { l?: number; r?: number };
  [key: string]: unknown;
};`,
  doc: `A mastering-style stereo scope that lives inside the tape loop. Each render it
pushes the current \`levels.peak.l\`/\`.r\` (dBFS, converted to linear) into a
128-frame ring buffer and redraws: the left channel deflects up from the centre
axis, the right channel mirrors down, newest sample on the left so the trace
scrolls the same direction as the tape transport. When the signal is quiet both
traces collapse to the axis. Give it a wide container (~520px).

\`\`\`jsx
<StereoScope levels={levels} />
\`\`\``,
};
