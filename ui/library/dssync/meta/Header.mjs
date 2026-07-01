export default {
  name: 'Header',
  group: 'Dialogs',
  summary: 'The top plugin bar — brand, version, preset name with prev/next nav, and Mod / Save / Reload buttons.',
  cardPad: 12,
  stories: [
    { label: 'Plugin bar', jsx: `h('div',{style:{width:1200}},
        h(D.Header,{preset:demo.presetInfo,onPrev:demo.noop,onNext:demo.noop,onSave:demo.noop,modOpen:false,setModOpen:demo.noop,onBrowse:demo.noop}))` },
  ],
  dts: `/** Current preset descriptor. \`name\` shows in the centre; \`cat\` renders as a chip; \`dirty\` appends " *". */
preset: { name?: string; cat?: string; dirty?: boolean };
/** Step to the previous preset. */
onPrev?: () => void;
/** Step to the next preset. */
onNext?: () => void;
/** Open the save dialog. */
onSave?: () => void;
/** Whether the Mod drawer is currently open (toggles the Mod button's lit state). */
modOpen: boolean;
/** Toggle the Mod drawer open/closed; called with the next value. */
setModOpen: (open: boolean) => void;
/** Open the full preset browser (click on the preset name). */
onBrowse?: () => void;`,
  doc: `The full-width top bar of the doobie plugin. Left: brand mark + "Analog Dub
Delay" tag. Centre: version string and the current preset with ‹ / › nav
arrows; clicking the preset name opens the browser. Right: a Mod toggle (lit
while the drawer is open), an amber Save button, and a small reload (↻) glyph.

It is a full-width bar — give it a wide container so the spacer distributes the
groups correctly.

\`\`\`jsx
<Header preset={presetInfo} onPrev={prev} onNext={next} onSave={openSave}
        modOpen={modOpen} setModOpen={setModOpen} onBrowse={openBrowser} />
\`\`\``,
};
