export default {
  name: 'TapeDeck',
  group: 'Visualizers',
  summary: 'Space-Echo tape loop animation — spinning reels with record/playback heads, driven by the head array plus play/record/speed state.',
  stories: [
    { label: 'Running', jsx: `h('div',{style:{width:620}}, h(D.TapeDeck,{heads:demo.heads,playing:true,recording:true,speed:1.3,accent:'var(--accent)'}))` },
  ],
  dts: `/** Playback heads; only \`id\`, \`on\`, \`time\`, and \`level\` are read for placement/opacity. */
heads: Array<{
  id: number | string;
  /** Head enabled — off heads render dimmed. */
  on: boolean;
  /** Normalised tap time (0..1) — position along the tape loop. */
  time: number;
  /** Normalised tap level (0..1) — head brightness. */
  level: number;
  [key: string]: unknown;
}>;
/** Reels spin while true. */
playing?: boolean;
/** Lights the record head. */
recording?: boolean;
/** Transport speed multiplier — scales reel rotation / scroll rate. */
speed?: number;
/** Accent colour; \`var(--accent)\` is resolved to a concrete value before it reaches the SVG. */
accent?: string;`,
  doc: `The Space-Echo tape transport. This is a React wrapper over the vanilla
\`createTapeDeck\` (tape.js): on mount it creates the deck and drives it imperatively,
pushing \`heads\`, \`playing\`, \`recording\`, and \`speed\` through on change. Each head's
\`time\` positions its playback head along the loop and \`level\` sets its brightness;
disabled heads dim out. Because \`accent\` feeds an SVG string, the CSS var is
resolved to a real colour first. Give it a wide container (~620px).

\`\`\`jsx
<TapeDeck heads={heads} playing recording speed={tapeSpeed} accent="var(--accent)" />
\`\`\``,
};
