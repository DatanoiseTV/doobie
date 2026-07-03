/* ============================================================
   Doobie · hero-video UI recorder
   ============================================================
   Drives the standalone WebView UI (ui/library/dist/standalone.html)
   along tools/hero-timeline.json — the SAME timeline doobie_render_hero
   used to render the soundtrack — while feeding the meters real levels
   analysed from that soundtrack. Captures frames via CDP screencast.

     node record-hero.mjs <levels.json> <framesDir>

   Requires playwright-core next to it (npm i playwright-core) and a
   system Chrome. Writes framesDir/NNNNNN.jpg + framesDir/meta.json
   ({ startEpoch, frames: [{ f, ts }] }) for ffmpeg assembly.
   ============================================================ */
import { chromium } from 'playwright-core';
import { readFileSync, writeFileSync, mkdirSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const CHROME = process.env.DS_CHROMIUM_PATH
  || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';

const here = dirname(fileURLToPath(import.meta.url));
// repo root: this script may run from a copy, so locate the repo by the
// timeline file next to the ORIGINAL script location passed via env, or
// fall back to walking up from CWD.
const REPO = process.env.DOOBIE_REPO || resolve(here, '..');
const TIMELINE = JSON.parse(readFileSync(resolve(REPO, 'tools/hero-timeline.json'), 'utf8'));
const STANDALONE = resolve(REPO, 'ui/library/dist/standalone.html');

const [levelsPath, framesDir] = process.argv.slice(2);
if (!levelsPath || !framesDir) {
  console.error('usage: node record-hero.mjs <levels.json> <framesDir>');
  process.exit(2);
}
const LEVELS = JSON.parse(readFileSync(levelsPath, 'utf8'));
mkdirSync(framesDir, { recursive: true });

const browser = await chromium.launch({ executablePath: CHROME, headless: true });
const ctx = await browser.newContext({
  viewport: { width: 1520, height: 960 },
  deviceScaleFactor: 2,
});
const page = await ctx.newPage();
await page.goto('file://' + STANDALONE);
await page.waitForTimeout(1800);          // fonts + first paint + reels settled

// ---- in-page driver: timeline -> stub bridge, levels -> meters ----
await page.evaluate(([tl, lv]) => {
  const J = window.Juce;
  if (window.__doobieStub && window.__doobieStub.stopLevelAnimation)
    window.__doobieStub.stopLevelAnimation();

  const lerp = (pts, t) => {
    if (t <= pts[0][0]) return pts[0][1];
    for (let i = 1; i < pts.length; i++)
      if (t < pts[i][0]) {
        const [t0, v0] = pts[i - 1], [t1, v1] = pts[i];
        return v0 + (t - t0) / Math.max(1e-9, t1 - t0) * (v1 - v0);
      }
    return pts[pts.length - 1][1];
  };
  const stepAt = (pts, t) => {
    let v = null;
    for (const [pt, pv] of pts) if (pt <= t) v = pv;
    return v;
  };

  const emit = window.__doobieStub.emit;
  const D = tl.duration;

  window.__heroDone = false;
  window.__heroDriver = {
    start() {
      const t0 = performance.now();
      const startEpoch = Date.now();
      const applied = {};                 // step-event dedup
      const tick = () => {
        const t = (performance.now() - t0) / 1000;
        // sliders: continuous
        for (const id in tl.sliders)
          J.getSliderState(id).setNormalisedValue(lerp(tl.sliders[id], t));
        // toggles / combos: apply on change only
        for (const id in tl.toggles) {
          const v = stepAt(tl.toggles[id], t);
          if (v !== null && applied['t:' + id] !== v) {
            applied['t:' + id] = v;
            J.getToggleState(id).setValue(v >= 0.5);
          }
        }
        for (const id in tl.combos) {
          const v = stepAt(tl.combos[id], t);
          if (v !== null && applied['c:' + id] !== v) {
            applied['c:' + id] = v;
            J.getComboBoxState(id).setChoiceIndex(v | 0);
          }
        }
        // meters: real levels analysed from the soundtrack
        const i = Math.min(lv.n - 1, Math.floor(t * lv.fps));
        const dl = lv.delay[i], out = lv.out[i];
        const mag = Math.min(1, Math.max(0, (dl + 34) / 26));
        const head4 = stepAt(tl.toggles.head4On || [], t);
        emit('levels', {
          in: lv.inL[i], delay: dl, reverb: lv.reverb[i], out,
          midiNote: -1,
          grDb: Math.max(0, (out + 9) / 3),
          env: lv.env[i],
          lfo1v: lv.lfo[i][0], lfo2v: lv.lfo[i][1],
          lfo3v: lv.lfo[i][0] * 0.6, lfo4v: lv.lfo[i][1] * -0.8,
          headMag: [0.75 * mag, 0.55 * mag, 0.4 * mag, (head4 !== null && head4 >= 0.5) ? 0.5 * mag : 0],
          peak: { in: lv.inL[i] + 4, delay: dl + 4, reverb: lv.reverb[i] + 4,
                  out: out + 4, l: out + 3.4, r: out + 4.2 },
        });
        if (t < D + 0.2) requestAnimationFrame(tick);
        else window.__heroDone = true;
      };
      requestAnimationFrame(tick);
      return startEpoch;
    },
  };
}, [TIMELINE, LEVELS]);

// ---- screencast ----
const cdp = await ctx.newCDPSession(page);
const frames = [];
let fnum = 0;
cdp.on('Page.screencastFrame', async (ev) => {
  const f = fnum++;
  writeFileSync(resolve(framesDir, String(f).padStart(6, '0') + '.jpg'),
                Buffer.from(ev.data, 'base64'));
  frames.push({ f, ts: ev.metadata.timestamp });
  try { await cdp.send('Page.screencastFrameAck', { sessionId: ev.sessionId }); } catch {}
});
await cdp.send('Page.startScreencast', {
  format: 'jpeg', quality: 92, maxWidth: 3040, maxHeight: 1920, everyNthFrame: 1,
});
await page.waitForTimeout(400);           // capture a little pre-roll

const startEpoch = await page.evaluate(() => window.__heroDriver.start());
await page.waitForFunction(() => window.__heroDone, null, { timeout: (TIMELINE.duration + 15) * 1000 });
await page.waitForTimeout(300);

await cdp.send('Page.stopScreencast');
await page.waitForTimeout(200);
writeFileSync(resolve(framesDir, 'meta.json'),
              JSON.stringify({ startEpoch, frames }, null, 1));
console.log(`captured ${frames.length} frames, driver started at epoch ${startEpoch}`);
await browser.close();
