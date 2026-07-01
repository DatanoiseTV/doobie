export default {
  name: 'ModDrawer',
  group: 'Dialogs',
  summary: 'Slide-in modulation drawer — Sources tab (4 LFO cards + envelope follower) and Matrix tab (8-slot routing grid with live scopes).',
  cardPad: 0,
  stories: [
    { label: 'Open (Sources)', jsx: `h(D.ModDrawer,{open:true,onClose:demo.noop,p:demo.p,setP:demo.setP,matrix:demo.matrix,setMx:demo.setMx,numSlots:8,levels:demo.levels})` },
  ],
  dts: `/** When false the drawer slides off-screen. */
open: boolean;
/** Close the drawer (scrim click / Close button). */
onClose?: () => void;
/** Live parameter object (LFO rates/depths, envelope times, sidechain filter). */
p: Record<string, number | string | boolean>;
/** Parameter setter: (key, value) => void. */
setP: (key: string, value: number | string | boolean) => void;
/** 8-slot modulation matrix — each entry \`{ src, dst, amt, mode }\`. */
matrix: Array<{ src: string; dst: string; amt: number; mode: string }>;
/** Matrix setter: (rowIndex, field, value) => void. */
setMx: (index: number, field: string, value: string | number) => void;
/** Number of matrix slots (typically 8). */
numSlots: number;
/** Live levels frame — drives the per-source mini-scopes (lfo1v..lfo4v, env). */
levels: { lfo1v?: number; lfo2v?: number; lfo3v?: number; lfo4v?: number; env?: number };`,
  doc: `The modulation drawer, a full-height panel that slides in over a scrim. Two
tabs: **Sources** shows the four LFO cards plus the envelope-follower card (with
attack/release/sens knobs and an optional sidechain filter), each with a live
mini-scope fed from \`levels\`. **Matrix** shows the routing grid: per row a
source select, an inline live scope, a destination select, Bi/Uni polarity, and
an amount slider whose fill reflects the currently-applied modulation.

Render open with \`open={true}\` and \`cardPad: 0\` (it is a full overlay).

\`\`\`jsx
<ModDrawer open={modOpen} onClose={() => setModOpen(false)}
           p={p} setP={setP} matrix={matrix} setMx={setMx}
           numSlots={8} levels={levels} />
\`\`\``,
};
