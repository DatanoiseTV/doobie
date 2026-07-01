export default {
  name: 'EnvViz',
  group: 'Meters',
  summary: 'Envelope-follower bar — 0..1 fill that tints amber→red past 70%, with a threshold LED and numeric readout.',
  stories: [
    { label: 'Levels', jsx: `h('div',{style:{width:260,display:'flex',flexDirection:'column',gap:16}},
        h(D.EnvViz,{level:0.6}),
        h(D.EnvViz,{level:0.2}))` },
  ],
  dts: `/** Live envelope-follower value, clamped to 0..1. Drives the fill width and tint. */
level: number;`,
  doc: `A compact horizontal bar for the envelope follower's live output. The fill grows
with \`level\` (0..1) and its tint blends from accent amber toward the peak red as
the level climbs past ~70%. An LED to the left lights once the level crosses the
~0.1 "noticeable" threshold (marked on the bar), and the trailing number shows the
level as a percent. Wrap in a narrow container (~260px).

\`\`\`jsx
<EnvViz level={levels.env} />
\`\`\``,
};
