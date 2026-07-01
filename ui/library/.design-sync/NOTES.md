# design-sync notes — doobie WebView UI

This is an **off-script** sync (the design-sync converter is bypassed): doobie's
UI has no TypeScript / `.d.ts` tree, so components can't be discovered by
`package-build.mjs`. Instead the layout is generated directly.

## How it's built

1. `node build.mjs` — transpiles the UNMODIFIED `ui/src/*.jsx` (esbuild transform,
   classic runtime) + concatenates in `index.html` order, substituting
   `src/stub-juce.js` (fake JUCE bridge) and appending `src/demo-props.js`.
   Produces `dist/doobie-bundle.js` exposing everything on `window.Doobie`.
2. `node dssync/gen.mjs` — emits `ds-bundle/` (the design-sync upload layout) from
   that bundle + per-component metadata in `dssync/meta/<Name>.mjs`. Card previews
   are authored HTML that mount `window.Doobie.<Name>` with `window.Doobie.__demo`
   state. Prepends `.design-sync/conventions.md` to the README.
3. Gate: `DS_CHROMIUM_PATH="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"`
   `node .ds-sync/package-validate.mjs ./ds-bundle` — must exit 0. Playwright module
   is installed in `.ds-sync/` WITHOUT its chromium (uses the system Chrome via
   `DS_CHROMIUM_PATH`).

To re-sync after UI changes: `node build.mjs && node dssync/gen.mjs`, re-validate,
then re-upload (see the design-sync skill §5 upload sequence).

## Verification

All 31 components render clean and were graded `good` on the absolute rubric from
the validate contact sheets (2 sheets, `ds-bundle/_screenshots/`).

## Known render notes

- `StereoScope` and the digital meters are **live-driven** (rolling scope / rAF
  meters). A still card shows a single static frame (a sparse trace / a fixed bar),
  which is correct but less lively than the running plugin. Not a defect.
- Fonts (Space Grotesk, JetBrains Mono) load via the Google-Fonts `@import` inside
  `doobie.css` → validate reports `[FONT_REMOTE]` (non-blocking, served at runtime).

## Re-sync risks

- The stub (`src/stub-juce.js`) hard-codes demo parameter defaults + preset/IR
  lists. If the real param ids in `ui/src/app.jsx` PARAM_MAP change, update the
  stub's seeded defaults or panels may render at neutral positions.
- The `dssync/meta/*.mjs` stories hard-code prop names. If a component's props
  change in `ui/src/`, its meta story must be updated or the card shows a caught
  `⚠` error (validate flags it `bad`).
- `ui/src/*.jsx` is transpiled as-is; a syntax feature esbuild's classic-JSX
  transform can't handle would break the bundle (none currently).

## Upload gotcha

`dssync/gen.mjs` does `rmSync(ds-bundle)` at the start, which deletes the
`_ds_needs_recompile` sentinel. Create the sentinel
(`printf '{"by":"design-sync-cli"}' > ds-bundle/_ds_needs_recompile`) AFTER the
final `gen.mjs` run and right before the upload sequence, or the sentinel
write_files calls fail with ENOENT. The project is:
`https://claude.ai/design/p/40aeec42-3c2f-49bb-bf5c-9b980607e914` (name "Doobie").
