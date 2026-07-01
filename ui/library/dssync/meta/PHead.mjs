export default {
  name: 'PHead',
  group: 'Controls',
  summary: 'Section header — title, a horizontal rule that fills the row, optional meta text, and optional leading icon / trailing action nodes.',
  stories: [
    { label: 'Title + meta', jsx: `h('div',{style:{width:420}},
        h(D.PHead,{title:'Reverb',meta:'post'}))` },
    { label: 'With action', jsx: `h('div',{style:{width:420}},
        h(D.PHead,{title:'Delay',meta:'stereo',action:h('span',{className:'hmeta'},'A/B')}))` },
  ],
  dts: `/** Section title (rendered as an <h2>). */
title: string;
/** Optional right-aligned meta caption (e.g. 'post', 'stereo'). */
meta?: string;
/** Optional leading icon node. */
icon?: React.ReactNode;
/** Optional trailing action node (button, toggle, badge…). */
action?: React.ReactNode;`,
  doc: `The panel section header. It lays out an optional \`icon\`, the \`title\`, a rule
line that stretches to fill the remaining width, an optional \`meta\` caption, and
an optional trailing \`action\` node (a \`PowerBtn\`, an A/B badge, etc.). Give it a
fixed-width parent so the rule has room to draw.

\`\`\`jsx
<PHead title="Reverb" meta="post"
       action={<PowerBtn on={!p.reverbBypass} onClick={toggleReverb} />} />
\`\`\``,
};
