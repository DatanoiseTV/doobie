export default {
  name: 'Chip',
  group: 'Controls',
  summary: 'LED toggle button — a compact labelled pill with an on/off LED dot; children is the label, extra handlers forward for momentary press-and-hold.',
  stories: [
    { label: 'On / off', jsx: `h('div',{style:{display:'flex',gap:14,alignItems:'center'}},
        h(D.Chip,{on:true},'P-PONG'),
        h(D.Chip,{on:false},'FREEZE'))` },
    { label: 'No LED', jsx: `h('div',{style:{display:'flex',gap:14,alignItems:'center'}},
        h(D.Chip,{on:true,led:false},'SYNC'),
        h(D.Chip,{on:false,led:false},'TRIPLET'))` },
  ],
  dts: `/** On/off state — sets the \`data-on\` attribute driving the LED + lit styling. */
on?: boolean;
/** Click handler (toggle). */
onClick?: (e: MouseEvent) => void;
/** Label content. */
children?: React.ReactNode;
/** Render the leading LED dot (default true). */
led?: boolean;
/** Any extra props (onMouseDown/onTouchStart/aria-*) forward to the <button>. */
[key: string]: unknown;`,
  doc: `A small toggle pill used for boolean effect switches (ping-pong, freeze, sync…).
The \`data-on\` state drives the LED colour and lit border. Because extra props
spread onto the underlying \`<button>\`, you can attach \`onMouseDown\`/\`onMouseUp\`
(or touch equivalents) to implement momentary press-and-hold behaviour without a
separate component. Pass \`led={false}\` for a plain text chip.

\`\`\`jsx
<Chip on={p.pingpong} onClick={() => setP('pingpong', !p.pingpong)}>P-PONG</Chip>
\`\`\``,
};
