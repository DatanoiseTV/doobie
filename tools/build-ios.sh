#!/usr/bin/env bash
# Build Doobie for iPad (Standalone app with embedded AUv3).
#
#   tools/build-ios.sh sim              # iPad Simulator (unsigned)
#   tools/build-ios.sh device [TEAMID]  # real hardware (needs an Apple team id)
#
# The simulator app lands in build-ios/Doobie_artefacts/Release-iphonesimulator/
# Standalone/Doobie.app — install it with:
#   xcrun simctl install booted <path>/Doobie.app
#   xcrun simctl launch booted com.datanoisetv.doobie
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
mode="${1:-sim}"
team="${2:-${DOOBIE_IOS_DEV_TEAM:-}}"
build="$here/build-ios"

cfg=(-B "$build" -G Xcode -DCMAKE_SYSTEM_NAME=iOS)
if [[ "$mode" == "device" ]]; then
  [[ -n "$team" ]] || { echo "device builds need a team id: tools/build-ios.sh device TEAMID" >&2; exit 2; }
  cfg+=(-DDOOBIE_IOS_DEV_TEAM="$team")
fi

echo ">> configuring ($mode)"
cmake "${cfg[@]}" "$here"

echo ">> building Doobie_Standalone ($mode)"
if [[ "$mode" == "sim" ]]; then
  cmake --build "$build" --config Release --target Doobie_Standalone -- \
    -sdk iphonesimulator CODE_SIGNING_ALLOWED=NO
  echo ">> app: $build/Doobie_artefacts/Release-iphonesimulator/Standalone/Doobie.app"
else
  cmake --build "$build" --config Release --target Doobie_Standalone -- \
    -sdk iphoneos -allowProvisioningUpdates
  echo ">> app: $build/Doobie_artefacts/Release-iphoneos/Standalone/Doobie.app"
fi
