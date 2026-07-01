export default {
  name: 'ReverbPanel',
  group: 'Panels',
  summary: 'Multi-mode reverb — type selector (Spring / Plate / Hall / Shimmer / Convolution / Gated…), Mix + route, mode-specific knob rows, IR picker, Shimmer interval chips, and an RT60 decay graph.',
  cardPad: 20,
  stories: [
    { label: 'Reverb', jsx: `h('div',{style:{width:560}}, h(D.ReverbPanel,{p:demo.p,setP:demo.setP,mods:demo.mods,irInfo:demo.irInfo,midiNote:-1}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (revType, revMix, route, revSpring, revStone, revDamp, revMod, revPlate, revSize, revPre, revWidth, revHpFreq, revLpFreq, shimmerSemis, gateThr, …). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };
/** Convolution IR state — { hasIR, isFactory, factoryIndex, name, isFile }. Drives the IR picker. */
irInfo: { hasIR: boolean; isFactory: boolean; factoryIndex: number; name: string; isFile: boolean };
/** Current MIDI note number (-1 = none), shown as the tracked note in Shimmer's MIDI mode. */
midiNote?: number;`,
  doc: `The reverb stage. A type dropdown swaps the engine (Spring, Plate, Spring>Plate,
Hall, Shimmer, Convolution, Gated, …) and the knob rows below adapt: spring/plate
modes show Spring/Tone/Damp/Mod + Decay/Size/Pre-Delay/Width and the post-filter
Verb LC/HC; Convolution swaps in the IR picker with IR Gain/Speed; Gated adds
threshold/hold/release; Shimmer adds the semitone interval chip row with MIDI
tracking. Mix and the Pre/In-FB/Post route sit at the top, an RT60 decay graph
at the bottom.

\`\`\`jsx
<ReverbPanel p={p} setP={setP} mods={mods} irInfo={irInfo} midiNote={-1} />
\`\`\``,
};
