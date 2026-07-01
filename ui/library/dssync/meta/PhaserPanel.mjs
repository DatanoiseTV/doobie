export default {
  name: 'PhaserPanel',
  group: 'Panels',
  summary: 'Six-stage all-pass phaser — power/bypass, Pre / In-Feedback / Post routing, and Rate / Depth / Feedback / Mix knobs.',
  cardPad: 20,
  stories: [
    { label: 'Phaser', jsx: `h('div',{style:{width:520}}, h(D.PhaserPanel,{p:demo.p,setP:demo.setP,mods:demo.mods}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (phaserOn, phaserRoute, phaserRate, phaserDepth, phaserFb, phaserMix). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };`,
  doc: `A six-stage all-pass phaser with feedback. The route selector matches the
reverb's insert points so signal flow stays one mental model: Pre (into the
delay input), In Feedback (cumulative per repeat — flange-y at high feedback),
or Post (on the wet echoes). Rate, Depth, Feedback and Mix control the sweep;
the whole knob row dims when the phaser is bypassed.

\`\`\`jsx
<PhaserPanel p={p} setP={setP} mods={mods} />
\`\`\``,
};
