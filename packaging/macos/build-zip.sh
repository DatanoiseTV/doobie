#!/usr/bin/env bash
# packaging/macos/build-zip.sh — bundle the macOS build artefacts into an
# unsigned .zip while Apple Developer signing is unavailable.
#
# Produces:  $OUTPUT_ZIP           a zip of .vst3 + .component + .app
#                                  with an INSTALL.txt explaining the
#                                  xattr -cr workaround that the user
#                                  needs to apply once on first install
#                                  (Gatekeeper would otherwise quarantine
#                                  the bundles as "untrusted publisher").
#
# Inputs (env vars):
#   DOOBIE_VERSION   e.g. 0.20.0  (required; baked into the zip name)
#   ARTEFACT_DIR     path to JUCE's Doobie_artefacts/Release/
#   OUTPUT_ZIP       destination .zip path
#
# This is a stopgap while the team's Apple Developer agreements are
# being re-accepted. When build-pkg.sh is usable again the release
# workflow should switch back; this script + the related `if: false`
# blocks in .github/workflows/release.yml are tagged with the same
# UNSIGNED-RELEASE marker for easy grep / revert.

set -euo pipefail

: "${DOOBIE_VERSION:?DOOBIE_VERSION must be set}"
: "${ARTEFACT_DIR:?ARTEFACT_DIR must be set}"
: "${OUTPUT_ZIP:?OUTPUT_ZIP must be set}"

AU_BUNDLE="$ARTEFACT_DIR/AU/Doobie.component"
VST3_BUNDLE="$ARTEFACT_DIR/VST3/Doobie.vst3"
APP_BUNDLE="$ARTEFACT_DIR/Standalone/Doobie.app"

for b in "$AU_BUNDLE" "$VST3_BUNDLE" "$APP_BUNDLE"; do
    if [[ ! -d "$b" ]]; then
        echo "missing artefact: $b" >&2
        exit 1
    fi
done

WORK="$(mktemp -d -t doobie-zip-XXXX)"
trap 'rm -rf "$WORK"' EXIT
STAGE="$WORK/Doobie-$DOOBIE_VERSION-macOS-unsigned"
mkdir -p "$STAGE"

echo "==> copy bundles"
cp -R "$AU_BUNDLE"   "$STAGE/"
cp -R "$VST3_BUNDLE" "$STAGE/"
cp -R "$APP_BUNDLE"  "$STAGE/"

cat > "$STAGE/INSTALL.txt" <<'EOF'
Doobie — unsigned macOS build
=============================

This build is NOT code-signed or notarized (the Apple Developer
agreement is in the process of being re-accepted). macOS will
quarantine it as "untrusted publisher" until you strip the quarantine
attribute. One command after install does it:

  xattr -cr /Library/Audio/Plug-Ins/Components/Doobie.component
  xattr -cr /Library/Audio/Plug-Ins/VST3/Doobie.vst3
  xattr -cr /Applications/Doobie.app

Install steps:

  1. Drop  Doobie.component  →  ~/Library/Audio/Plug-Ins/Components
                              or /Library/Audio/Plug-Ins/Components
  2. Drop  Doobie.vst3        →  ~/Library/Audio/Plug-Ins/VST3
                              or /Library/Audio/Plug-Ins/VST3
  3. Drop  Doobie.app         →  /Applications  (or anywhere)
  4. Run the three xattr -cr commands above (paths adjusted to where
     you actually installed the bundles).

If you'd rather not do this by hand, install via the Homebrew tap
which automates the xattr step:

  brew tap DatanoiseTV/doobie
  brew install --cask doobie

This is a temporary state. Signed + notarized releases will resume
in the next cycle once the developer agreement is sorted.
EOF

echo "==> zip"
mkdir -p "$(dirname "$OUTPUT_ZIP")"
# ditto preserves macOS bundle metadata + symlinks correctly across
# zip round-trips (plain `zip` mangles framework symlinks inside .app).
ditto -c -k --sequesterRsrc --keepParent "$STAGE" "$OUTPUT_ZIP"

echo ""
echo "Built unsigned macOS zip:"
echo "  $OUTPUT_ZIP"
