export default {
  name: 'Selector',
  group: 'Controls',
  summary: 'Styled <select> dropdown over a string[] of options — controlled by value, emits the chosen string on change.',
  stories: [
    { label: 'Mode', jsx: `h('div',{style:{width:180}},
        h(D.Selector,{value:'Tape',options:['Digital','Tape','BBD','Diffuse','Pitch']}))` },
  ],
  dts: `/** Currently selected option (must be one of \`options\`). */
value: string;
/** The list of selectable option strings. */
options: string[];
/** Called with the newly selected option string. */
onChange?: (value: string) => void;`,
  doc: `A themed dropdown for enum-style parameters (delay mode, filter type, sync
division…). It is a thin wrapper over a native \`<select>\`, so it inherits
keyboard and accessibility behaviour; \`options\` is a plain string array and the
selected string is echoed back through \`onChange\`.

\`\`\`jsx
<Selector value={p.mode}
          options={['Digital', 'Tape', 'BBD', 'Diffuse', 'Pitch']}
          onChange={(v) => setP('mode', v)} />
\`\`\``,
};
