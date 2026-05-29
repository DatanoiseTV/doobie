#!/usr/bin/env bash
# packaging/macos/build-pkg.sh — sign + notarize + bundle the Doobie macOS
# release artefacts into a single Developer-ID-signed .pkg installer.
#
# Inputs (env vars):
#   DOOBIE_VERSION       e.g. 0.12.0  (required; used in pkg metadata)
#   ARTEFACT_DIR         path to JUCE's Doobie_artefacts/Release/
#   APPLICATION_IDENTITY "Developer ID Application: Name (TEAMID)"
#   INSTALLER_IDENTITY   "Developer ID Installer: Name (TEAMID)"
#   APPLE_TEAM_ID        TEAMID (10-char)
#   NOTARY_KEYCHAIN_PROFILE  notarytool keychain-profile name to use
#   OUTPUT_PKG           destination .pkg path
#
# Produces:
#   $OUTPUT_PKG          signed, notarized, stapled installer.
#
# Process: codesign each plug-in bundle with --options runtime + the
# entitlements, then pkgbuild a component pkg per format, productbuild
# them together, productsign the combined .pkg, notarize, staple.
#
# Run locally during development with: just point it at a fresh build dir
# and a previously-configured `xcrun notarytool store-credentials`
# keychain profile. CI does the keychain bootstrap before calling this.

set -euo pipefail

: "${DOOBIE_VERSION:?DOOBIE_VERSION must be set}"
: "${ARTEFACT_DIR:?ARTEFACT_DIR must be set}"
: "${APPLICATION_IDENTITY:?APPLICATION_IDENTITY must be set}"
: "${INSTALLER_IDENTITY:?INSTALLER_IDENTITY must be set}"
: "${APPLE_TEAM_ID:?APPLE_TEAM_ID must be set}"
: "${NOTARY_KEYCHAIN_PROFILE:?NOTARY_KEYCHAIN_PROFILE must be set}"
: "${OUTPUT_PKG:?OUTPUT_PKG must be set}"

PKG_DIR="$(cd "$(dirname "$0")" && pwd)"
ENTITLEMENTS="$PKG_DIR/entitlements.plist"
DIST_TEMPLATE="$PKG_DIR/distribution.xml"
WELCOME="$PKG_DIR/welcome.txt"

AU_BUNDLE="$ARTEFACT_DIR/AU/Doobie.component"
VST3_BUNDLE="$ARTEFACT_DIR/VST3/Doobie.vst3"
APP_BUNDLE="$ARTEFACT_DIR/Standalone/Doobie.app"

for b in "$AU_BUNDLE" "$VST3_BUNDLE" "$APP_BUNDLE"; do
    if [[ ! -d "$b" ]]; then
        echo "missing artefact: $b" >&2
        exit 1
    fi
done

WORK="$(mktemp -d -t doobie-pkg-XXXX)"
trap 'rm -rf "$WORK"' EXIT
echo "work dir: $WORK"

# ----------------------------------------------------------------------------
# 1) Code-sign each bundle with the hardened runtime + entitlements.
# ----------------------------------------------------------------------------
sign_bundle() {
    local bundle="$1"
    echo "==> codesign  $bundle"
    # Strip JUCE's ad-hoc signature first so codesign --force replaces it
    # cleanly (a stale ad-hoc sig on a nested binary can poison the bundle).
    codesign --remove-signature "$bundle" 2>/dev/null || true
    codesign --force --deep --options runtime --timestamp \
             --entitlements "$ENTITLEMENTS" \
             --sign "$APPLICATION_IDENTITY" \
             "$bundle"
    codesign --verify --strict --verbose=2 "$bundle"
}

sign_bundle "$AU_BUNDLE"
sign_bundle "$VST3_BUNDLE"
sign_bundle "$APP_BUNDLE"

# ----------------------------------------------------------------------------
# 2) Build a component .pkg per format with the right install location.
# ----------------------------------------------------------------------------
mkdir -p "$WORK/components"

build_component() {
    local id="$1" bundle="$2" install_to="$3" out="$4"
    echo "==> pkgbuild  $id"
    local stage; stage="$(mktemp -d -t doobie-stage-XXXX)"
    cp -R "$bundle" "$stage/"
    pkgbuild --identifier "$id" \
             --version "$DOOBIE_VERSION" \
             --root "$stage" \
             --install-location "$install_to" \
             "$out"
    rm -rf "$stage"
}

build_component com.datanoisetv.doobie.au         "$AU_BUNDLE"   /Library/Audio/Plug-Ins/Components "$WORK/components/doobie-au.pkg"
build_component com.datanoisetv.doobie.vst3       "$VST3_BUNDLE" /Library/Audio/Plug-Ins/VST3      "$WORK/components/doobie-vst3.pkg"
build_component com.datanoisetv.doobie.standalone "$APP_BUNDLE"  /Applications                      "$WORK/components/doobie-standalone.pkg"

# ----------------------------------------------------------------------------
# 3) Combine into a distribution .pkg and sign it.
# ----------------------------------------------------------------------------
cp "$WELCOME" "$WORK/components/welcome.txt"
sed "s/@DOOBIE_VERSION@/$DOOBIE_VERSION/g" "$DIST_TEMPLATE" > "$WORK/components/distribution.xml"

UNSIGNED="$WORK/Doobie-unsigned.pkg"
echo "==> productbuild"
productbuild --distribution "$WORK/components/distribution.xml" \
             --resources    "$WORK/components" \
             --package-path "$WORK/components" \
             --version      "$DOOBIE_VERSION" \
             "$UNSIGNED"

echo "==> productsign"
mkdir -p "$(dirname "$OUTPUT_PKG")"
productsign --sign "$INSTALLER_IDENTITY" --timestamp "$UNSIGNED" "$OUTPUT_PKG"
pkgutil --check-signature "$OUTPUT_PKG"

# ----------------------------------------------------------------------------
# 4) Notarize and staple. notarytool blocks until Apple finishes (~3 min).
# ----------------------------------------------------------------------------
echo "==> notarize  (this typically takes 2-5 minutes)"
xcrun notarytool submit "$OUTPUT_PKG" \
    --keychain-profile "$NOTARY_KEYCHAIN_PROFILE" \
    --wait

echo "==> staple"
xcrun stapler staple "$OUTPUT_PKG"
xcrun stapler validate "$OUTPUT_PKG"

echo ""
echo "Built signed + notarized installer:"
echo "  $OUTPUT_PKG"
