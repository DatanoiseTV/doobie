/* ============================================================
   Doobie · capture demo fixtures for the preview cards
   ============================================================
   claude.ai/design reconstructs window.<namespace> from the
   DECLARED components in the bundle header — any non-standard
   helper (e.g. a window.Doobie.__demo) is NOT preserved there.
   So the preview cards must carry their own fixtures rather than
   read them from the bundle at render time.

   This runs the standalone bundle once in headless Chrome, reads
   the fully-built demo state off window.Doobie.__demo (p / heads /
   matrix / levels / stages / presetInfo / irInfo), and writes it
   to dssync/demo-state.json, which gen.mjs inlines into each card.

   Run after `node build.mjs`, before `node dssync/gen.mjs`, when
   the seeded stub defaults or PARAM_MAP change:
     node dssync/capture-demo.mjs
   ============================================================ */
import { readFileSync, writeFileSync, mkdtempSync, rmSync } from 'node:fs';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { dirname, resolve, join } from 'node:path';
import { tmpdir } from 'node:os';

const __dir = dirname(fileURLToPath(import.meta.url));
const LIB = resolve(__dir, '..');
const UI = resolve(LIB, '..');

const CHROME = process.env.DS_CHROMIUM_PATH
  || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

const react = readFileSync(resolve(UI, 'vendor', 'react.production.min.js'), 'utf8');
const rdom = readFileSync(resolve(UI, 'vendor', 'react-dom.production.min.js'), 'utf8');
const bundle = readFileSync(resolve(LIB, 'dist', 'doobie-bundle.js'), 'utf8');

const page = `<!doctype html><html><head><meta charset="utf-8"></head><body><pre id="out"></pre>
<script>${react}</script><script>${rdom}</script><script>${bundle}</script>
<script>
 try {
   var d = window.Doobie.__demo;
   var data = { p: d.p, heads: d.heads, matrix: d.matrix, levels: d.levels,
                stages: d.stages, presetInfo: d.presetInfo, irInfo: d.irInfo };
   document.getElementById('out').textContent = 'B64:' + btoa(unescape(encodeURIComponent(JSON.stringify(data))));
 } catch (e) { document.getElementById('out').textContent = 'ERR:' + e.message; }
</script></body></html>`;

const work = mkdtempSync(join(tmpdir(), 'doobie-capture-'));
try {
  const htmlPath = join(work, 'capture.html');
  writeFileSync(htmlPath, page);
  const dump = execFileSync(CHROME, [
    '--headless', '--disable-gpu', '--no-sandbox',
    '--virtual-time-budget=3000', '--dump-dom', `file://${htmlPath}`,
  ], { encoding: 'utf8', maxBuffer: 64 * 1024 * 1024, stdio: ['ignore', 'pipe', 'ignore'] });
  const m = dump.match(/B64:([A-Za-z0-9+/=]+)/);
  if (!m) {
    const err = dump.match(/ERR:[^<]*/);
    throw new Error(`capture failed: ${err ? err[0] : 'no payload in dump'}`);
  }
  const data = JSON.parse(Buffer.from(m[1], 'base64').toString('utf8'));
  writeFileSync(resolve(__dir, 'demo-state.json'), JSON.stringify(data, null, 2));
  console.log(`demo-state.json: ${Object.keys(data.p).length} params, ${data.heads.length} heads, ${data.matrix.length} mod slots, preset "${data.presetInfo.name}", ir "${data.irInfo.name}"`);
} finally {
  rmSync(work, { recursive: true, force: true });
}
