export default {
  name: 'KB',
  group: 'Panels',
  summary: 'Param-bound knob wrapper — binds a Knob to a param key (value = p[k], writes via setP), auto-wiring the modulation arc/live-dot from mods; supports bipolar, size, lit and custom format.',
  cardPad: 24,
  stories: [
    { label: 'Bound knobs', jsx: `h('div',{style:{display:'flex',gap:28,alignItems:'flex-end'}},
        h(D.KB,{label:'Feedback',k:'feedback',p:demo.p,setP:demo.setP,mods:demo.mods,size:'md',lit:true,format:function(v){return Math.round(v*100)+'%'}}),
        h(D.KB,{label:'Mix',k:'mix',p:demo.p,setP:demo.setP,mods:demo.mods,size:'md',format:function(v){return Math.round(v*100)+'%'}}),
        h(D.KB,{label:'Width',k:'width',p:demo.p,setP:demo.setP,mods:demo.mods,size:'md',bipolar:true,format:function(v){return Math.round(v*100)+'%'}}))` },
  ],
  dts: `/** Micro label under the knob (e.g. "Feedback"). */
label?: string;
/** Param key this knob reads/writes: value = p[k], change calls setP(k, v). */
k: string;
/** Live parameter values, keyed by design id. */
p: Record<string, number | string | boolean>;
/** Writes a parameter back: setP(key, value). */
setP: (key: string, value: number | string | boolean) => void;
/** Bipolar knob (centre-detent, ± swing) — for shelves / pan-style params. */
bipolar?: boolean;
/** Value formatter for the readout, e.g. (v) => Math.round(v*100)+'%'. */
format?: (v: number) => string;
/** Force the "lit" (active-accent) styling regardless of value. */
lit?: boolean;
/** Knob size. */
size?: 'sm' | 'md' | 'lg';
/** Modulation map — enables the mod arc + live dot for this param when passed. */
mods?: Record<string, number> & { live: Record<string, number> };
/** Override the knob arc colour (CSS colour string). */
arcColor?: string;
/** Override the APVTS mod-destination id (defaults to the built-in PARAM_MOD_KEY[k] mapping). */
modKey?: string;`,
  doc: `The shared param-binding wrapper used by every panel knob. Instead of wiring
value/onChange by hand, \`KB\` reads \`p[k]\` and writes through \`setP(k, v)\`, and —
when \`mods\` is passed — looks up the param's modulation destination id
(via the built-in map or an explicit \`modKey\`) to draw the mod arc and the live
modulation dot. Pass \`bipolar\`, \`size\`, \`lit\`, \`format\` and \`arcColor\` through to
the underlying Knob.

\`\`\`jsx
<KB label="Feedback" k="feedback" p={p} setP={setP} mods={mods} size="md" lit
    format={(v) => Math.round(v * 100) + '%'} />
\`\`\``,
};
