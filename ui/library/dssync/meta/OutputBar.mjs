export default {
  name: 'OutputBar',
  group: 'Panels',
  summary: 'Wide output bar — Dry/Wet, stereo Width dial, Duck with optional 3-band crossovers, Output trim, Auto-Gain leveler, and L/R digital peak meters.',
  cardPad: 20,
  stories: [
    { label: 'Output bar', jsx: `h('div',{style:{width:1000}}, h(D.OutputBar,{p:demo.p,setP:demo.setP,levels:demo.levels,mods:demo.mods}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (mix, width, duck, duckMultiband, duckCrossLow, duckCrossHigh, output, autoGain). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Static levels frame — levels.peak.l / levels.peak.r (dBFS) feed the L/R output meters. */
levels: { peak: { l: number; r: number } } & Record<string, unknown>;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };`,
  doc: `The full-width output strip along the bottom of the UI. Holds the Dry/Wet mix,
a stereo Width dial, the Duck control with an opt-in 3-band mode (Low/High
crossover knobs dim until enabled), the Output trim, and an Auto-Gain leveler
pill. Two large L/R digital peak meters sit at the right, fed from the live
levels frame.

\`\`\`jsx
<OutputBar p={p} setP={setP} levels={levels} mods={mods} />
\`\`\``,
};
