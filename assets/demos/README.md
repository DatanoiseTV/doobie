# Preset demos

The landing-page player (`site/index.html`, "Listen" section) reads
`manifest.json` from this folder. Until that file and its audio clips exist,
the Listen section stays hidden — the page is clean without it.

## Generating the demos

From the repo root, run the renderer with any short, dry, loop-able source
(a drum groove, a chord, a vocal phrase — 4–8 seconds is plenty):

```sh
tools/render-demos.sh path/to/source-loop.wav
```

It builds the `doobie_render_demo` tool, runs your loop through a curated set of
factory presets (Classic Dub, King Tubby, Space Echo, Ambient Wash, Cathedral,
Shimmer Drift, Phaser Bloom, Gated 80s), and writes the clips plus `dry.wav` and
`manifest.json` here. If `ffmpeg` is on your PATH the clips are transcoded to
mp3 and the manifest is updated to match.

Edit the `kDemos` list in `tools/RenderDemo.cpp` to change which presets are
showcased.

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
