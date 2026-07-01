export default {
  name: 'InputPanel',
  group: 'Panels',
  summary: 'Input stage panel — gain trim, pre-delay EQ (low/high cut, bass, treble), and a collapsible multimode filter (LP/HP/BP + cutoff + resonance).',
  cardPad: 20,
  stories: [
    { label: 'Input panel', jsx: `h('div',{style:{width:520}}, h(D.InputPanel,{p:demo.p,setP:demo.setP,mods:demo.mods}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (inTrim, inLowCut, inFilterType, …). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };`,
  doc: `The left-column input stage. Reads every value from the plain \`p\` object and
writes through \`setP\` — no direct engine coupling. Contains the gain trim knob,
the four pre-delay EQ knobs, and a filter sub-section (on/off chip, LP/HP/BP
selector, cutoff + resonance) that only shows its cutoff/resonance controls when
the filter is enabled.

\`\`\`jsx
<InputPanel p={p} setP={setP} mods={mods} />
\`\`\``,
};
