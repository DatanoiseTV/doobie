export default {
  name: 'IRPicker',
  group: 'Dialogs',
  summary: 'Impulse-response picker row for the convolution reverb — shows the loaded IR name and opens a browser to pick a factory IR, load a file, or clear.',
  cardPad: 24,
  stories: [
    { label: 'Loaded IR', jsx: `h('div',{style:{width:420}}, h(D.IRPicker,{irInfo:demo.irInfo}))` },
  ],
  dts: `/** Descriptor of the currently-loaded impulse response. */
irInfo: {
  /** Whether any IR is loaded. */
  hasIR: boolean;
  /** True when the loaded IR comes from the built-in factory bank. */
  isFactory: boolean;
  /** Index into the factory IR list when \`isFactory\`. */
  factoryIndex: number;
  /** True when the IR was loaded from a user file. */
  isFile: boolean;
  /** Display name of the loaded IR. */
  name: string;
};`,
  doc: `A single row showing the impulse response currently feeding the convolution
reverb, with a control to open the IR browser. From the browser the user can
pick a factory IR, load their own file, or clear the slot — these emit JUCE
events (\`ir_load_factory\` / \`ir_load_file\` / \`ir_clear\`). The factory name
list is fetched lazily via the \`listFactoryIRs\` native function. The row's
label falls back to "(no IR)" when nothing is loaded.

Give it a container of ~420px so the name and button lay out cleanly.

\`\`\`jsx
<IRPicker irInfo={irInfo} />
\`\`\``,
};
