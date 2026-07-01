# Preset demos

The landing-page player (`site/index.html`, "Listen" section) reads
`manifest.json` from this folder. Until that file and its audio clips exist,
the Listen section stays hidden — the page is clean without it.

## Generating the demos

From the repo root, run the renderer with any short, dry, loop-able source
(a Rhodes phrase, a drum groove, a vocal — a few seconds is plenty). wav, aiff,
flac or mp3 all work; ffmpeg decodes it:

```sh
tools/render-demos.sh path/to/source-loop.wav
```

It builds the `doobie_render_demo` tool, runs your loop through **every non-MIDI
factory preset** (MIDI-note presets are skipped — they need incoming notes),
auto-derives a category and one-line descriptor for each from the engine state,
and writes one clip per preset plus `dry.wav` and `manifest.json` here. With
`ffmpeg` present the clips are transcoded to mp3 and the manifest updated to
match.

The current demos were rendered from a Rhodes electric-piano loop. Category
grouping and the MIDI-skip logic live in `describe()` / the render loop in
`tools/RenderDemo.cpp`.

## manifest.json shape

```json
{
  "dry": "dry.mp3",
  "presets": [
    { "slug": "classic-dub", "name": "Classic Dub", "category": "Dub",
      "tagline": "Springy quarter-note echoes with in-loop tone shaping.",
      "file": "classic-dub.mp3" }
  ]
}
```
