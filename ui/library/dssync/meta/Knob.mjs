export default {
  name: 'Knob',
  group: 'Controls',
  summary: 'Rotary knob control — SVG arc + value readout, vertical-drag to change, with bipolar, modulation-arc, and size variants.',
  stories: [
    { label: 'Sizes', jsx: `h('div',{style:{display:'flex',gap:28,alignItems:'flex-end'}},
        h(D.Knob,{value:0.35,size:'sm',label:'Small'}),
        h(D.Knob,{value:0.62,size:'md',label:'Medium'}),
        h(D.Knob,{value:0.5,size:'lg',label:'Large'}))` },
    { label: 'Bipolar + lit', jsx: `h('div',{style:{display:'flex',gap:28,alignItems:'flex-end'}},
        h(D.Knob,{value:0.72,bipolar:true,label:'Pan',lit:true}),
        h(D.Knob,{value:0.3,bipolar:true,label:'Detune'}))` },
    { label: 'Modulated', jsx: `h(D.Knob,{value:0.55,size:'lg',label:'Time',mod:0.18,modValue:0.09,lit:true,arcColor:'oklch(0.745 0.150 62)'})` },
  ],
  dts: `/** Normalised position, 0..1. */
value?: number;
/** Called with the new normalised value on drag / double-click reset. */
onChange?: (value: number) => void;
size?: 'sm' | 'md' | 'lg';
label?: string;
/** Formats the centre readout from the normalised value. */
format?: (value: number) => string;
/** Lit ring styling (accent glow). */
lit?: boolean;
/** Fill grows from centre (0.5) instead of from the minimum. */
bipolar?: boolean;
/** Modulation half-range (0..1) — draws the outer mod arc. */
mod?: number;
/** Live signed modulation offset for the animated mod dot. */
modValue?: number;
/** Override the arc colour (CSS colour). */
arcColor?: string;
/** Normalised default restored on double-click / reset. */
defaultValue?: number;`,
  doc: `The primary parameter control. Drag vertically to change (240px = full range;
hold Shift for 1/4-speed fine adjust); double-click resets; right-click opens the
knob context menu (Reset / Copy / Paste / Enter value). Pass \`bipolar\` for
centre-origin params (pan, detune), \`mod\`/\`modValue\` to show live modulation,
and \`format\` to render engineering units in the readout.

\`\`\`jsx
<Knob value={p.feedback} onChange={(v) => setP('feedback', v)}
      size="lg" label="Feedback" format={(v) => Math.round(v * 100) + '%'} />
\`\`\``,
};
