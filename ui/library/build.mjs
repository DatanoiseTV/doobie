/* ============================================================
   Doobie · library build
   ============================================================
   Transpiles the UNMODIFIED ui/src/*.jsx at build time (esbuild
   transform, classic runtime) and concatenates them in the exact
   order index.html loads them, so global-scope semantics match
   the plugin's in-browser Babel setup byte-for-byte in behaviour.
   The only substitution is the JUCE backend: stub-juce.js stands
   in for the native relays. Produces:
     dist/doobie-bundle.js   global-scope bundle + window.Doobie namespace
     dist/standalone.html     self-contained demo (React inlined)
   ============================================================ */
import esbuild from 'esbuild';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';

const __dir = dirname(fileURLToPath(import.meta.url));
const UI = resolve(__dir, '..');          // ui/
const SRC = resolve(UI, 'src');           // ui/src
const VENDOR = resolve(UI, 'vendor');     // ui/vendor
const OUT = resolve(__dir, 'dist');
mkdirSync(OUT, { recursive: true });

const read = (p) => readFileSync(p, 'utf8');

async function jsx(file) {
  const code = read(resolve(SRC, file));
  const out = await esbuild.transform(code, {
    loader: 'jsx',
    jsx: 'transform',                     // classic React.createElement
    jsxFactory: 'React.createElement',
    jsxFragment: 'React.Fragment',
    sourcefile: file,
  });
  return `\n/* ==== ${file} ==== */\n${out.code}`;
}

// app.jsx auto-mounts at the bottom (ReactDOM.createRoot(...).render(<App/>)).
// For a component library we keep App + all helpers but drop that tail so the
// bundle exposes App as a component instead of mounting itself.
function stripMount(appCode) {
  const marker = '// Fail-loud mount.';
  const i = appCode.indexOf(marker);
  return i >= 0 ? appCode.slice(0, i) : appCode;
}

const EXPORTS = [
  // atoms
  'Knob', 'Fader', 'Chip', 'Selector', 'PHead', 'PowerBtn', 'WidthDial',
  // meters
  'TapeDeck', 'DigitalMeter', 'StereoScope',
  // viz + dialogs
  'DecayGraph', 'WaveMini', 'Modal', 'KnobContextMenu', 'PresetBrowser', 'IRPicker',
  // panels
  'Header', 'VUStrip', 'InputPanel', 'HeadsPanel', 'DelayPanel', 'PitchShifterPanel',
  'FeedbackPanel', 'PhaserPanel', 'ReverbPanel', 'OutputBar', 'ModDrawer', 'LfoCard',
  'EnvViz', 'KB',
  // full interface
  'App',
];

const prelude = `
/* React hook globals — mirrors index.html so each transpiled file can use
   bare useState/useEffect/etc. exactly as it does in the plugin. */
window.useState = React.useState;
window.useEffect = React.useEffect;
window.useRef = React.useRef;
window.useCallback = React.useCallback;
window.useMemo = React.useMemo;
`;

const collector = `
/* ==== window.Doobie namespace ==== */
window.Doobie = window.Doobie || {};
${EXPORTS.map((n) => `try { window.Doobie.${n} = ${n}; } catch (e) {}`).join('\n')}
`;

async function build() {
  const bridge = await jsx('juce-bridge.jsx');
  const knob = await jsx('knob.jsx');
  const viz = await jsx('viz.jsx');
  const mounts = await jsx('mounts.jsx');
  const panels = await jsx('panels.jsx');
  let app = await jsx('app.jsx');
  app = stripMount(app);

  const tape = read(resolve(SRC, 'tape.js'));
  const stub = read(resolve(__dir, 'src', 'stub-juce.js'));
  const demo = read(resolve(__dir, 'src', 'demo-props.js'));

  // Order mirrors index.html: bridge stub, tape, juce-bridge, then components.
  const bundle = [
    prelude,
    `\n/* ==== stub-juce.js ==== */\n${stub}`,
    `\n/* ==== tape.js ==== */\n${tape}`,
    bridge, knob, viz, mounts, panels, app,
    collector,
    `\n/* ==== demo-props.js ==== */\n${demo}`,
  ].join('\n');

  writeFileSync(resolve(OUT, 'doobie-bundle.js'), bundle);
  console.log('wrote dist/doobie-bundle.js', (bundle.length / 1024).toFixed(1), 'kB');

  // Standalone demo: inline React + the CSS + bundle, mount App, animate.
  const react = read(resolve(VENDOR, 'react.production.min.js'));
  const reactDom = read(resolve(VENDOR, 'react-dom.production.min.js'));
  const css = read(resolve(SRC, 'doobie.css'));

  const html = `<!doctype html>
<html lang="en"><head><meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1.0"/>
<title>Doobie — standalone UI</title>
<style>${css}</style>
</head><body>
<div id="root"></div>
<script>${react}</script>
<script>${reactDom}</script>
<script>${bundle}</script>
<script>
  ReactDOM.createRoot(document.getElementById('root')).render(
    React.createElement(window.Doobie.App)
  );
  if (window.__doobieStub) window.__doobieStub.startLevelAnimation();
</script>
</body></html>`;
  writeFileSync(resolve(OUT, 'standalone.html'), html);
  console.log('wrote dist/standalone.html', (html.length / 1024).toFixed(1), 'kB');
}

build().catch((e) => { console.error(e); process.exit(1); });
