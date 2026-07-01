export default {
  name: 'DelayPanel',
  group: 'Panels',
  summary: 'Hero delay panel — tape-deck visual + stereo scope, big Time / Feedback knobs, sync division + character selectors, transport chips (Ping-Pong / Freeze / Kill FB) and the tape-character EQ row.',
  cardPad: 20,
  stories: [
    { label: 'Delay', jsx: `h('div',{style:{width:560}}, h(D.DelayPanel,{p:demo.p,setP:demo.setP,heads:demo.heads,mods:demo.mods,levels:demo.levels,tapeSpeed:1.4,accent:'var(--accent)',midiNote:-1}))` },
  ],
  dts: `/** Live parameter values, keyed by design id (time, feedback, sync, division, character, wow, flutter, sat, age, …). */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** The four playback heads, drawn on the tape deck. */
heads: Array<{ id: number; on: boolean; level: number; pan: number; time: number; offset: number }>;
/** Tape transport speed multiplier for the deck animation (1 = normal). */
tapeSpeed?: number;
/** Accent colour for the tape deck / heads (CSS colour string). */
accent?: string;
/** Modulation map — mods[paramId] = arc half-range, mods.live[paramId] = live offset. */
mods: Record<string, number> & { live: Record<string, number> };
/** Optional feedback-knob arc colour (goes hot as feedback nears self-oscillation). */
fbCol?: string;
/** Current MIDI note number (-1 = none); only surfaced by pitch-linked character modes. */
midiNote?: number;
/** Static levels frame feeding the inline stereo scope (in/delay/reverb/out dBFS, peak, headMag[], …). */
levels: Record<string, unknown>;`,
  doc: `The centre-column hero. Shows the animated tape deck with its playback heads
and an overlaid stereo scope, the two large Time and Feedback knobs, the
sync/division + character selectors, the three transport chips (Ping-Pong,
Freeze, momentary Kill FB), and the tape-character EQ row (Wow, Flutter,
Saturation, Age). In sync mode the Time knob steps through tempo divisions
instead of writing milliseconds.

\`\`\`jsx
<DelayPanel p={p} setP={setP} heads={heads} mods={mods} levels={levels}
            tapeSpeed={1.4} accent="var(--accent)" midiNote={-1} />
\`\`\``,
};
