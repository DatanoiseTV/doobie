export default {
  name: 'PitchShifterPanel',
  group: 'Panels',
  summary: 'Delay-chain pitch shifter — interval chip row (-24..+24 st), FFT/Granular algo, Pre / In-FB route, Spread, plus MIDI-note tracking with optional portamento glide.',
  cardPad: 20,
  stories: [
    { label: 'Pitch shifter', jsx: `h('div',{style:{width:560}}, h(D.PitchShifterPanel,{p:demo.p,setP:demo.setP,midiNote:-1}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (pitchOn, pitchSemis, pitchAlgo, pitchRoute, pitchSpread, midiPitchMode, midiPortaOn, midiPortaMs, character). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Current MIDI note number (-1 = none), shown as the tracked note when midiPitchMode is on. */
midiNote?: number;`,
  doc: `A stand-alone card for the delay chain's pitch shifter. The interval is set by
the named chip row (unison, 4th, 5th, octave, …) or by MIDI-note tracking
(C3 = unison) with an optional portamento glide. Picks FFT or Granular
algorithm and a Pre or In-Feedback route — the latter injects into the delay's
feedback chain for classic climbing-octave shimmer. Dims to an Activate state
when the delay character isn't Pitch.

\`\`\`jsx
<PitchShifterPanel p={p} setP={setP} midiNote={-1} />
\`\`\``,
};
