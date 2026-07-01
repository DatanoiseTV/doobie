export default {
  name: 'PowerBtn',
  group: 'Controls',
  summary: 'Power / bypass icon button — a power glyph whose lit state follows on, with an accessible title/aria-label.',
  stories: [
    { label: 'Enabled / bypassed', jsx: `h('div',{style:{display:'flex',gap:16,alignItems:'center'}},
        h(D.PowerBtn,{on:true,title:'Reverb enabled'}),
        h(D.PowerBtn,{on:false,title:'Reverb bypassed'}))` },
  ],
  dts: `/** On (engaged) vs off (bypassed) — sets \`data-on\` for the lit power glyph. */
on?: boolean;
/** Click handler (toggle bypass). */
onClick?: (e: MouseEvent) => void;
/** Tooltip + aria-label (default 'Bypass'). */
title?: string;`,
  doc: `The per-section enable / bypass toggle. Renders a power-symbol SVG that lights
when \`on\` is true and dims when bypassed. \`title\` doubles as the accessible
label, so give it a descriptive per-section string.

\`\`\`jsx
<PowerBtn on={!p.reverbBypass} onClick={() => setP('reverbBypass', !p.reverbBypass)}
          title="Reverb" />
\`\`\``,
};
