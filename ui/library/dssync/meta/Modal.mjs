export default {
  name: 'Modal',
  group: 'Dialogs',
  summary: 'Text-input dialog — title, message, single text field, Cancel / Confirm actions, with Escape and Enter shortcuts.',
  cardPad: 0,
  stories: [
    { label: 'Save preset', jsx: `h(D.Modal,{open:true,title:'Save preset',message:'Pick a name for your patch.',defaultValue:'Dub Chamber',confirmLabel:'Save',onConfirm:demo.noop,onCancel:demo.noop})` },
  ],
  dts: `/** When false the dialog renders nothing. */
open: boolean;
title: string;
message?: string;
/** Initial text-field value. */
defaultValue?: string;
/** Called with the field text when the user confirms. */
onConfirm?: (value: string) => void;
onCancel?: () => void;
/** Confirm-button label (default "Save"). */
confirmLabel?: string;`,
  doc: `A modal text-input dialog rendered over a full-viewport scrim. Used for naming
presets and any single-value entry. Enter confirms, Escape cancels; clicking the
scrim cancels. Controlled via \`open\`.

\`\`\`jsx
<Modal open={saveOpen} title="Save preset" message="Pick a name for your patch."
       defaultValue={presetName} confirmLabel="Save"
       onConfirm={(name) => save(name)} onCancel={() => setSaveOpen(false)} />
\`\`\``,
};
