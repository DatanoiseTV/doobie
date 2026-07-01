export default {
  name: 'DigitalMeter',
  group: 'Meters',
  summary: 'Digital dBFS bar meter — smoothed RMS fill + peak-hold marker + clip LED, driven by a single live dB number, with big and scale variants.',
  stories: [
    { label: 'Live levels', jsx: `h('div',{style:{width:360,display:'flex',flexDirection:'column',gap:16}},
        h(D.DigitalMeter,{label:'OUT L',liveDb:-8}),
        h(D.DigitalMeter,{label:'OUT R',liveDb:-24}))` },
    { label: 'Big + scale', jsx: `h('div',{style:{width:360}},
        h(D.DigitalMeter,{label:'MASTER',liveDb:-6,big:true,scale:true}))` },
  ],
  dts: `/** Micro label shown top-left (e.g. "OUT L", "REVERB"). */
label: string;
/** Live peak-hold level in dBFS. −∞ shows below ~-53.5; the RMS bar is smoothed (80 ms attack / 200 ms release). */
liveDb?: number;
/** Taller bar variant for the master / feature meter. */
big?: boolean;
/** Render the dB scale row (0, -6, -12, -24, -48 marks) under the bar. */
scale?: boolean;`,
  doc: `A single-channel dBFS meter. Feed it \`liveDb\` from the engine's \`levels\`
native event (~30 Hz) and it self-animates on requestAnimationFrame: the fill is
a one-pole smoothed RMS follower, a separate marker holds the instantaneous peak
(white below 0, red at/over 0), and the clip LED latches when \`liveDb >= 0\`.
The component owns its own rAF, so re-passing \`liveDb\` every frame is cheap.
Wrap in a fixed-width container. Pass \`scale\` for the reference-mark row and
\`big\` for the master meter.

\`\`\`jsx
<DigitalMeter label="OUT L" liveDb={levels.peak.out} scale />
\`\`\``,
};
