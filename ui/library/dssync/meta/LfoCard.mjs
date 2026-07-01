export default {
  name: 'LfoCard',
  group: 'Panels',
  summary: 'One LFO source card (of four) — Rate (Hz) or tempo Div when synced, Depth / Smooth / bipolar Offset knobs, waveform selector, and a live mini-scope of the current output.',
  cardPad: 20,
  stories: [
    { label: 'LFO 1', jsx: `h('div',{style:{width:300}}, h(D.LfoCard,{n:1,p:demo.p,setP:demo.setP,live:0.4}))` },
  ],
  dts: `/** Which LFO this card drives (1–4); selects the lfo\${n}Rate / lfo\${n}Depth / … param keys. */
n: 1 | 2 | 3 | 4;
/** Live parameter values, keyed by design id (lfo1Rate, lfo1Depth, lfo1Wave, lfo1Sync, lfo1Div, lfo1Smooth, lfo1Offset, …). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Current live LFO output value (roughly -1..+1) driving the mini-scope trace. */
live?: number;`,
  doc: `A single LFO panel, instantiated four times in the Mod drawer. The Sync toggle
swaps the rate readout between a free Hz knob and a tempo-division picker. Depth,
Smooth (meaningful for Random S&H) and a bipolar Offset sit alongside the
waveform selector (Sine, Triangle, Saw, Square, Random S&H), with a live
mini-scope that traces the current \`live\` output value.

\`\`\`jsx
<LfoCard n={1} p={p} setP={setP} live={0.4} />
\`\`\``,
};
