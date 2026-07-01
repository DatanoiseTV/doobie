export default {
  name: 'PresetBrowser',
  group: 'Dialogs',
  summary: 'Full-screen preset browser — search box, category filter chips, and a merged factory + user preset list with an unsaved-changes guard.',
  cardPad: 0,
  stories: [
    { label: 'Open', jsx: `h(D.PresetBrowser,{open:true,onClose:demo.noop,currentName:'Dub Chamber',dirty:false,onRequestSave:demo.noop})` },
  ],
  dts: `/** When false the browser renders nothing. */
open: boolean;
/** Dismiss the browser (Escape / close). */
onClose?: () => void;
/** Name of the currently-loaded preset — highlighted in the list. */
currentName?: string;
/** Whether the current patch has unsaved edits (triggers the switch-confirmation guard). */
dirty?: boolean;
/** Invoked when the user chooses to save before switching away from a dirty patch. */
onRequestSave?: () => void;`,
  doc: `A near-fullscreen modal listing the factory bank merged with user presets (user
saves shadow same-named factory entries). A search field filters by name and the
category chips (ALL / USER / DUB / AMBIENT / VINTAGE / WIDE / OTHER) filter by
tag. The currently-loaded preset is highlighted. If the patch is \`dirty\`,
picking a different preset queues the switch behind a confirmation so unsaved
edits aren't lost; \`onRequestSave\` lets the user save first.

It pulls its lists via JUCE native functions (\`listFactoryPresets\` /
\`listUserPresets\`), which the standalone stub provides. Render with
\`open={true}\` and \`cardPad: 0\`.

\`\`\`jsx
<PresetBrowser open={browseOpen} onClose={() => setBrowseOpen(false)}
               currentName={presetInfo.name} dirty={presetInfo.dirty}
               onRequestSave={openSave} />
\`\`\``,
};
