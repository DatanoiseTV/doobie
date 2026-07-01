export default {
  name: 'KnobContextMenu',
  group: 'Dialogs',
  summary: 'Global right-click menu for any knob — Reset to default, Copy / Paste value, and Enter value… (numeric or percentage entry).',
  cardPad: 0,
  stories: [
    { label: 'Open on a knob', jsx: `h(function(){ React.useEffect(function(){ window.openKnobMenu && window.openKnobMenu(48,40,{value:0.62,defaultValue:0.5,label:'Feedback',format:function(v){return Math.round(v*100)+'%';},onChange:demo.noop}); },[]); return h(D.KnobContextMenu); })` },
  ],
  dts: `// KnobContextMenu takes no props. It is a singleton controlled imperatively
// through the global window.openKnobMenu(x, y, opts) function it registers on
// mount, where opts is:
//   { value: number; defaultValue: number; label: string;
//     format?: (v: number) => string; onChange: (v: number) => void }`,
  doc: `A singleton context menu for knob controls. Mount it once anywhere in the tree
(it renders nothing until opened). It registers a global
\`window.openKnobMenu(x, y, opts)\` on mount; a knob's right-click / long-press
handler calls that with the click position and the knob's descriptor to pop the
menu at the cursor. Items: Reset to default, Copy value, Paste value, and
"Enter value…" (which opens a numeric Modal accepting \`0–1\` or a percentage
like \`75%\`). A document mousedown dismisses it.

\`\`\`jsx
// mount once, near the app root
<KnobContextMenu />

// from a knob:
window.openKnobMenu(e.clientX, e.clientY, {
  value: 0.62, defaultValue: 0.5, label: 'Feedback',
  format: (v) => Math.round(v * 100) + '%',
  onChange: (v) => setParam('feedback', v),
});
\`\`\``,
};
