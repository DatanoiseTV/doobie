/* ============================================================
   Doobie · off-script ds-bundle generator
   ============================================================
   Emits the claude.ai/design upload layout directly from the
   standalone bundle (dist/doobie-bundle.js) + per-component
   metadata under dssync/meta/<Name>.mjs. Gated by the skill's
   package-validate.mjs (layout-agnostic; tolerates off-script).

   Layout produced under ui/library/ds-bundle/:
     _ds_bundle.js         bundle + first-line @ds-bundle header
     _ds_bundle.css        doobie.css (real component CSS)
     styles.css            @import "./_ds_bundle.css"
     _vendor/react.js       vendored UMD (render check loads these)
     _vendor/react-dom.js
     _ds_sync.json          verification anchor
     .ds-build-meta.json    local build metadata
     README.md
     components/<group>/<Name>/{<Name>.html,.d.ts,.prompt.md,.jsx}
   ============================================================ */
import { readFileSync, writeFileSync, mkdirSync, rmSync, readdirSync, cpSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { dirname, resolve, join } from 'node:path';

const __dir = dirname(fileURLToPath(import.meta.url));
const LIB = resolve(__dir, '..');                 // ui/library
const UI = resolve(LIB, '..');                    // ui
const DIST = resolve(LIB, 'dist');
const META = resolve(__dir, 'meta');
const OUT = resolve(LIB, 'ds-bundle');

const NAMESPACE = 'Doobie';
const sha256 = (buf) => createHash('sha256').update(buf).digest('hex');

// Demo state captured from the running bundle (dssync/capture-demo). Inlined
// into every card so previews depend ONLY on the real window.Doobie.<Name>
// exports — claude.ai/design reconstructs the namespace from the declared
// components and drops any non-standard helper (a `window.Doobie.__demo` is
// NOT preserved there), so the cards must carry their own fixtures.
let DEMO_STATE = '{}';
try { DEMO_STATE = readFileSync(resolve(META, '..', 'demo-state.json'), 'utf8').trim(); }
catch { console.error('! dssync/demo-state.json missing — run the capture step (see NOTES.md) first'); }

// ---- load metadata modules ----
const metaFiles = readdirSync(META).filter((f) => f.endsWith('.mjs')).sort();
const components = [];
for (const f of metaFiles) {
  const mod = await import(pathToFileURL(join(META, f)).href);
  const m = mod.default;
  if (!m || !m.name) { console.error(`! ${f}: no default export with a name — skipped`); continue; }
  components.push(m);
}
if (!components.length) { console.error('no component metadata found under dssync/meta/'); process.exit(1); }

// ---- clean + scaffold ----
rmSync(OUT, { recursive: true, force: true });
mkdirSync(join(OUT, '_vendor'), { recursive: true });
mkdirSync(join(OUT, 'components'), { recursive: true });

// ---- bundle + header ----
const rawBundle = readFileSync(join(DIST, 'doobie-bundle.js'), 'utf8');
const header = {
  namespace: NAMESPACE,
  components: components.map((c) => ({ name: c.name, group: c.group })),
  sourceHashes: {},        // off-script: not tracked per-source-file
  inlinedExternals: [],    // React is provided by _vendor at card runtime
};
// Escape */ inside the JSON so the single-line comment can't be closed early.
const headerLine = `/* @ds-bundle: ${JSON.stringify(header).replace(/\*\//g, '*\\/')} */`;
const bundleJs = `${headerLine}\n${rawBundle}`;
writeFileSync(join(OUT, '_ds_bundle.js'), bundleJs);
const bundleSha12 = sha256(bundleJs).slice(0, 12);

// ---- css ----
const css = readFileSync(resolve(UI, 'src', 'doobie.css'), 'utf8');
writeFileSync(join(OUT, '_ds_bundle.css'), css);
const stylesCss = `/* Doobie design system — styles entry point.\n`
  + `   Rendered designs consume this file's @import closure. */\n`
  + `@import "./_ds_bundle.css";\n`;
writeFileSync(join(OUT, 'styles.css'), stylesCss);
const styleSha = sha256(css);

// ---- vendored react (render check loads /_vendor/react.js + react-dom.js) ----
cpSync(resolve(UI, 'vendor', 'react.production.min.js'), join(OUT, '_vendor', 'react.js'));
cpSync(resolve(UI, 'vendor', 'react-dom.production.min.js'), join(OUT, '_vendor', 'react-dom.js'));

// ---- per-component card + docs ----
const renderHashes = {};
const esc = (s) => String(s);

function card(m) {
  const group = m.group;
  const stories = m.stories && m.stories.length ? m.stories : [{ label: m.name, jsx: `h(D.${m.name})` }];
  // Each story becomes a labeled cell; all cells mount into #root so the
  // validator's render check (roots = #root) sees one non-empty root.
  const cellsJs = stories.map((s, i) => `  { label: ${JSON.stringify(s.label)}, node: (function(){ try { return ${s.jsx}; } catch(e){ return h('div',{style:{color:'#f88',font:'12px monospace'}}, '⚠ '+e.message); } })() }`).join(',\n');
  const pad = m.cardPad != null ? m.cardPad : 24;
  return `<!-- @dsCard group="${esc(group)}" -->
<!doctype html>
<html lang="en"><head><meta charset="UTF-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1.0"/>
<title>${esc(m.name)} — Doobie</title>
<link rel="stylesheet" href="../../../styles.css"/>
<style>
  html,body{margin:0;background:oklch(0.168 0.006 60);}
  .ds-wrap{padding:${pad}px;display:flex;flex-wrap:wrap;gap:28px;align-items:flex-start;}
  .ds-cell{display:flex;flex-direction:column;gap:10px;}
  .ds-cell > .ds-label{font:600 11px/1.2 'Space Grotesk',ui-sans-serif,system-ui,sans-serif;
    letter-spacing:.08em;text-transform:uppercase;color:oklch(0.66 0.010 70);}
  .ds-cell > .ds-body{display:flex;gap:20px;align-items:flex-start;}
</style>
</head><body>
<div id="root"></div>
<script src="../../../_vendor/react.js"></script>
<script src="../../../_vendor/react-dom.js"></script>
<script src="../../../_ds_bundle.js"></script>
<script>
(function(){
  var D = window.Doobie, h = React.createElement;
  var noop = function(){};
  // Self-contained demo fixtures (baked from the running bundle) + no-op
  // setters. No dependency on any non-standard bundle global.
  var demo = ${DEMO_STATE};
  demo.setP = noop; demo.setHead = noop; demo.setMx = noop; demo.noop = noop;
  demo.mods = { live: {} };
  var cells = [
${cellsJs}
  ];
  var tree = h('div', { className: 'ds-wrap' }, cells.map(function(c, i){
    return h('div', { className: 'ds-cell', key: i },
      h('div', { className: 'ds-label' }, c.label),
      h('div', { className: 'ds-body' }, c.node));
  }));
  ReactDOM.createRoot(document.getElementById('root')).render(tree);
})();
</script>
</body></html>
`;
}

function dtsFile(m) {
  const body = (m.dts || '').trim();
  const iface = `export interface ${m.name}Props {\n${body ? '  ' + body.replace(/\n/g, '\n  ') + '\n' : ''}}`;
  const jsdoc = m.summary ? `/** ${m.summary} */\n` : '';
  return `${jsdoc}${iface}\n\nexport declare function ${m.name}(props: ${m.name}Props): JSX.Element;\n`;
}

function promptFile(m) {
  const summary = (m.summary || `${m.name} — Doobie UI component.`).trim();
  const doc = (m.doc || '').trim();
  return `${summary}\n\n# ${m.name}\n\nGroup: ${m.group}\n\n${doc}\n`;
}

function jsxStub(m) {
  return `// Re-export of the compiled component from the design-system bundle.\n`
    + `// The real implementation ships in _ds_bundle.js on window.${NAMESPACE}.${m.name}.\n`
    + `export const ${m.name} = (typeof window !== 'undefined' && window.${NAMESPACE}) ? window.${NAMESPACE}.${m.name} : undefined;\n`;
}

// Directory-safe group slug (the @dsCard keeps the human label with spaces).
const groupDir = (g) => g.replace(/[^\w.-]+/g, '-');
for (const m of components) {
  const dir = join(OUT, 'components', groupDir(m.group), m.name);
  mkdirSync(dir, { recursive: true });
  const html = card(m);
  writeFileSync(join(dir, `${m.name}.html`), html);
  writeFileSync(join(dir, `${m.name}.d.ts`), dtsFile(m));
  writeFileSync(join(dir, `${m.name}.prompt.md`), promptFile(m));
  writeFileSync(join(dir, `${m.name}.jsx`), jsxStub(m));
  renderHashes[m.name] = sha256(html).slice(0, 16);
}

// ---- anchor + metadata + readme ----
writeFileSync(join(OUT, '_ds_sync.json'), JSON.stringify({
  shape: 'package',
  styleSha,
  bundleSha12,
  renderHashes,
  sourceKeys: {},
}, null, 2));

writeFileSync(join(OUT, '.ds-build-meta.json'), JSON.stringify({
  componentCount: components.length,
  shape: 'package',
  namespace: NAMESPACE,
}, null, 2));

const groups = [...new Set(components.map((c) => c.group))];
// Prepend the committed conventions header (the design-agent guidance) when present.
let conventions = '';
try { conventions = readFileSync(resolve(LIB, '.design-sync', 'conventions.md'), 'utf8').trim() + '\n\n---\n\n'; } catch {}
const readme = conventions
  + `# Doobie — WebView UI design system\n\n`
  + `The doobie plugin's interface, extracted from its JUCE WebView UI and\n`
  + `shipped as a component library. Every component here is the real,\n`
  + `unmodified UI code the plugin runs — only the native audio backend is\n`
  + `stubbed for standalone rendering.\n\n`
  + `- Namespace: \`window.${NAMESPACE}\`\n`
  + `- ${components.length} components across ${groups.length} groups: ${groups.join(', ')}\n\n`
  + `## Components\n\n`
  + groups.map((g) => `### ${g}\n\n`
      + components.filter((c) => c.group === g).map((c) => `- **${c.name}** — ${c.summary || ''}`).join('\n')
    ).join('\n\n')
  + '\n';
writeFileSync(join(OUT, 'README.md'), readme);

console.log(`ds-bundle: ${components.length} components, ${groups.length} groups → ${OUT}`);
console.log(`  groups: ${groups.join(', ')}`);
