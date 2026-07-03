#!/usr/bin/env bash
# Render one landing-page demo bank.
#
#   tools/render-demos.sh <key> <source-loop> [bpm]
#   e.g. tools/render-demos.sh rhodes ~/loops/rhodes.mp3 120
#        tools/render-demos.sh vocals ~/loops/vocal.mp3 90
#
# Runs the dry source through every non-MIDI factory preset at the given host
# tempo (default 120 — tempo-synced presets follow it) and writes mp3 clips +
# manifest.json into site/assets/demos/<key>/. The player lists banks from
# site/assets/demos/sources.json — add the key there with a label.
#
# Any short, dry, loop-able source works: drums, keys, a vocal phrase.
# 4-20 seconds is plenty; the renderer adds a 6 s tail per clip.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
key="${1:-}"
src="${2:-}"
bpm="${3:-120}"
out="$here/site/assets/demos/$key"
build="$here/build"

if [[ -z "$key" || -z "$src" || ! -f "$src" ]]; then
  echo "usage: tools/render-demos.sh <key> <source-loop.(wav|mp3|flac|aiff)> [bpm=120]" >&2
  exit 2
fi
command -v ffmpeg >/dev/null 2>&1 || { echo "error: ffmpeg is required (source decode + mp3 encode)" >&2; exit 1; }

echo ">> configuring / building doobie_render_demo"
cmake -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1 || \
  cmake -B "$build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$build" --target doobie_render_demo

bin="$(find "$build" -name doobie_render_demo -type f -perm -111 | head -1)"
if [[ -z "$bin" ]]; then echo "error: built binary not found" >&2; exit 1; fi

tmpwav="$(mktemp -t doobie-src).wav"
echo ">> decoding source -> 48k stereo wav"
ffmpeg -y -loglevel error -i "$src" -ac 2 -ar 48000 "$tmpwav"

mkdir -p "$out"
echo ">> rendering bank '$key' at ${bpm} BPM -> $out"
"$bin" "$tmpwav" "$out" "$bpm"
rm -f "$tmpwav"

echo ">> transcoding to mp3"
for wav in "$out"/*.wav; do
  [[ -e "$wav" ]] || continue
  ffmpeg -y -loglevel error -i "$wav" -codec:a libmp3lame -q:a 5 "${wav%.wav}.mp3"
  rm -f "$wav"
done
sed -i.bak 's/\.wav"/.mp3"/g' "$out/manifest.json" && rm -f "$out/manifest.json.bak"

if ! grep -q "\"$key\"" "$here/site/assets/demos/sources.json" 2>/dev/null; then
  echo ">> NOTE: add '$key' to site/assets/demos/sources.json so the player lists it"
fi
echo ">> done — $(ls "$out"/*.mp3 | wc -l | tr -d ' ') clips in $out"
