export default {
  name: 'FeedbackPanel',
  group: 'Panels',
  summary: 'In-loop tone shaper — Low/High cut with live musical-note readouts, Bass/Treble shelves, and LC/HC resonance knobs for tuning resonant feedback peaks to the song key.',
  cardPad: 20,
  stories: [
    { label: 'Feedback loop', jsx: `h('div',{style:{width:520}}, h(D.FeedbackPanel,{p:demo.p,setP:demo.setP,mods:demo.mods}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (fbLowCut, fbHighCut, fbBass, fbTreble, hpRes, lpRes). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };`,
  doc: `Tone shaping inside the recirculating feedback path. The Low Cut and High Cut
knobs each carry a live nearest-note readout (e.g. "C3 +12¢") so resonant
repeats can be tuned to the song key. Bass and Treble are bipolar shelves; the
LC Res / HC Res knobs add resonance at the two cut frequencies.

\`\`\`jsx
<FeedbackPanel p={p} setP={setP} mods={mods} />
\`\`\``,
};
