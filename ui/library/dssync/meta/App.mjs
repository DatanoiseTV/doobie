export default {
  name: 'App',
  group: 'Full UI',
  summary: 'The complete doobie plugin interface — header, all panels, meters, and output bar, wired together.',
  cardPad: 0,
  stories: [
    { label: 'Full plugin', jsx: `h(D.App)` },
  ],
  dts: `// App takes no props. It is the root component and reads all of its state
// (parameters, heads, mod matrix, live levels, preset + IR info) from the JUCE
// bridge, which is stubbed for standalone rendering.`,
  doc: `The entire assembled plugin interface: the top Header, the Input / Delay /
Reverb / Output panels, the modulation drawer, all meters and visualizers, and
the output bar — composed and wired to shared state. It self-scales to the
viewport.

App is the root and takes no props; it reads every value (parameters, delay
heads, mod matrix, live level frames, preset and IR info) from the JUCE bridge.
In the standalone preview that bridge is stubbed, so it renders with
representative demo state.

\`\`\`jsx
<App />
\`\`\``,
};
