export default {
  name: 'HeadsPanel',
  group: 'Panels',
  summary: 'Four-tap playback-head mixer — per-head on/off, level fader with live magnitude meter, and Pan / Time / Offset knobs, with anti-collision on the tap times.',
  cardPad: 20,
  stories: [
    { label: 'Playback heads', jsx: `h('div',{style:{width:360}}, h(D.HeadsPanel,{heads:demo.heads,setHead:demo.setHead,mods:demo.mods,synced:false,headMag:demo.levels.headMag}))` },
  ],
  dts: `/** The four playback-head strips. Each head: id, on, level, pan, time, offset (all 0..1 except id). */
heads: Array<{ id: number; on: boolean; level: number; pan: number; time: number; offset: number }>;
/** Writes one field of one head: setHead(index, field, value). */
setHead: (index: number, field: 'on' | 'level' | 'pan' | 'time' | 'offset', value: number | boolean) => void;
/** Modulation map — mods['head1Pan'…], mods['head1Ratio'…] give arc half-ranges; mods.live[...] the live offsets. */
mods: Record<string, number> & { live: Record<string, number> };
/** True when the master delay is tempo-locked; per-head Time knobs snap to musical fractions. */
synced?: boolean;
/** Per-head live output magnitude (0..1), indexed by head position, drives each fader's meter. */
headMag?: number[];`,
  doc: `The playback-head mixer — a Space-Echo-style ladder of four independent taps.
Each strip toggles its head on/off, sets output level (with a live magnitude
meter), and dials Pan, Time (fraction of the master delay, or a snapped musical
division when \`synced\`), and a bipolar per-head Offset in ms. Anti-collision
keeps two active heads from landing on the same tap time.

\`\`\`jsx
<HeadsPanel heads={heads} setHead={setHead} mods={mods} synced={false} headMag={levels.headMag} />
\`\`\``,
};
