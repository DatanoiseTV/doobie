#!/usr/bin/env bash
# packaging/macos/setup-secrets.sh
#
# Interactive helper that populates the GitHub Secrets the release.yml
# workflow needs to sign + notarize + publish Doobie. Run this ONCE,
# locally, after you have:
#
#   * Developer ID Application certificate exported as a .p12 (with a
#     password) from Keychain Access on the machine that originally
#     created it. The cert itself is created at
#     https://developer.apple.com/account/resources/certificates
#     under "Developer ID Application".
#
#   * Developer ID Installer certificate exported the same way ("Developer
#     ID Installer" type on the same page).
#
#   * App Store Connect API key (.p8 file) created at
#     https://appstoreconnect.apple.com/access/integrations/api
#     with role "Developer". The download is one-time only — keep a copy.
#     You'll also need the Key ID (shown in the table) and the Issuer ID
#     (shown above the table on the same page).
#
#   * Your Team ID (10-char string, e.g. "AB12CD34EF"). Find it at
#     https://developer.apple.com/account → Membership details, or run
#     `security find-identity -v -p codesigning` on a machine that has
#     the Developer ID cert imported (the ID in parentheses).
#
# This script never echoes any secret values to stdout or to your shell
# history — passwords are read with -s, and cert contents are streamed
# from disk straight into `gh secret set` via stdin, never via argv.

set -euo pipefail

if ! command -v gh >/dev/null; then
    echo "gh CLI not found. Install: brew install gh, then 'gh auth login'." >&2
    exit 1
fi

REPO="${REPO:-$(gh repo view --json nameWithOwner -q .nameWithOwner)}"
echo "Setting secrets on repo: $REPO"
echo

read_path() {
    local prompt="$1" path
    while :; do
        read -rp "$prompt " path
        # Expand ~ if the user typed it literally.
        path="${path/#\~/$HOME}"
        if [[ -f "$path" ]]; then echo "$path"; return; fi
        echo "  ! file not found: $path" >&2
    done
}

read_text() {
    local prompt="$1" val
    read -rp "$prompt " val
    echo "$val"
}

read_secret() {
    local prompt="$1" val
    read -rsp "$prompt " val
    echo >&2
    echo "$val"
}

# -- Developer ID Application -------------------------------------------------
APP_P12="$(read_path        'Path to Developer ID Application .p12        :')"
APP_PWD="$(read_secret      'Password for that .p12                       :')"

# -- Developer ID Installer ---------------------------------------------------
INS_P12="$(read_path        'Path to Developer ID Installer .p12          :')"
INS_PWD="$(read_secret      'Password for that .p12                       :')"

# -- Notarization (App Store Connect API key) --------------------------------
API_P8="$(read_path         'Path to App Store Connect API key .p8        :')"
API_KEY_ID="$(read_text     'API Key ID (e.g. ABCD1234EF)                 :')"
API_ISSUER_ID="$(read_text  'API Issuer ID (UUID)                         :')"

# -- Identity strings + team id ----------------------------------------------
TEAM_ID="$(read_text        'Apple Team ID (10-char)                      :')"
APP_IDENTITY="$(read_text   'Application identity CN (e.g. \"Developer ID Application: Your Name ('"$TEAM_ID"')\"):')"
INS_IDENTITY="$(read_text   'Installer identity CN   (e.g. \"Developer ID Installer: Your Name ('"$TEAM_ID"')\")  :')"

# -- Keychain password for the runner (arbitrary; pick anything) -------------
KEYCHAIN_PWD="$(openssl rand -hex 16)"
echo "Generated random keychain password for runners (it never leaves GitHub)."

echo
echo "About to set 10 secrets on $REPO. Proceeding..."
echo

# All cert contents stream via stdin so they never appear in process args or
# shell history. Text-only values use --body which is fine for non-secret
# identifiers (team id, identity CN, etc.).
base64 -i "$APP_P12" | gh secret set MACOS_CERTIFICATE                  --repo "$REPO"
gh secret set MACOS_CERTIFICATE_PWD                                     --repo "$REPO" --body "$APP_PWD"

base64 -i "$INS_P12" | gh secret set MACOS_INSTALLER_CERTIFICATE        --repo "$REPO"
gh secret set MACOS_INSTALLER_CERTIFICATE_PWD                           --repo "$REPO" --body "$INS_PWD"

base64 -i "$API_P8"  | gh secret set APPLE_NOTARY_KEY_P8                --repo "$REPO"
gh secret set APPLE_NOTARY_KEY_ID                                       --repo "$REPO" --body "$API_KEY_ID"
gh secret set APPLE_NOTARY_ISSUER_ID                                    --repo "$REPO" --body "$API_ISSUER_ID"

gh secret set APPLE_TEAM_ID                                             --repo "$REPO" --body "$TEAM_ID"
gh secret set MACOS_APPLICATION_IDENTITY                                --repo "$REPO" --body "$APP_IDENTITY"
gh secret set MACOS_INSTALLER_IDENTITY                                  --repo "$REPO" --body "$INS_IDENTITY"

gh secret set MACOS_KEYCHAIN_PWD                                        --repo "$REPO" --body "$KEYCHAIN_PWD"

echo
echo "Done. release.yml will now sign + notarize on the next push to main."
echo "Verify the list with:  gh secret list --repo $REPO"
