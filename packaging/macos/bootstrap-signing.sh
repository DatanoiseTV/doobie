#!/usr/bin/env bash
# packaging/macos/bootstrap-signing.sh
#
# One-shot bootstrap for the Doobie macOS release pipeline. Goes from
# "I have a paid Apple Developer membership and nothing else set up" to
# "GitHub Secrets are populated, release.yml will sign + notarize on the
# next push to main".
#
# Two unavoidable manual steps Apple's surface forces:
#   1. Create the App Store Connect API key (one-time, 30s). There is no
#      API to create API keys -- chicken-and-egg.
#   2. Possibly create the Developer ID Installer certificate in the web
#      portal -- the App Store Connect API's `DEVELOPER_ID_INSTALLER`
#      certificate type is unreliable; we try the API first and fall back
#      to opening the portal page if it 4xxs.
#
# Everything else is fully automated: CSR generation, App cert issuance
# via the ASC API, .p12 bundling with random passwords, and setting all
# 10 GitHub Secrets via `gh secret set`. Cert/key bytes are streamed
# straight into `gh secret set` via stdin, never logged or echoed.

set -euo pipefail

# ---- Pre-flight -------------------------------------------------------------
for cmd in gh openssl curl python3 jq; do
    if ! command -v "$cmd" >/dev/null; then
        echo "Missing required tool: $cmd" >&2
        echo "  Install: brew install gh jq    (gh + jq)" >&2
        echo "           openssl + curl + python3 ship with macOS." >&2
        exit 1
    fi
done

if ! gh auth status >/dev/null 2>&1; then
    echo "gh is not authenticated. Run: gh auth login" >&2
    exit 1
fi

REPO="${REPO:-$(gh repo view --json nameWithOwner -q .nameWithOwner)}"
WORK="$(mktemp -d -t doobie-bootstrap-XXXX)"
trap 'rm -rf "$WORK"' EXIT

echo "Target repo:   $REPO"
echo "Scratch dir:   $WORK"
echo

# ---- Helpers ----------------------------------------------------------------
pause() { read -rp "  ↵ press return when done... " _; }
prompt() { local p="$1" v; read -rp "$p" v; echo "$v"; }
prompt_secret() { local p="$1" v; read -rsp "$p" v; echo >&2; echo "$v"; }

open_url() {
    if command -v open >/dev/null; then open "$1"; else echo "  open: $1"; fi
}

# JWT generation for the ASC API. Uses a venv so we don't pollute the
# system Python. PyJWT does ES256 + the DER-to-raw signature conversion
# correctly, which is fiddly in pure bash.
setup_python() {
    if [[ -x "$WORK/venv/bin/python3" ]]; then return; fi
    python3 -m venv "$WORK/venv"
    "$WORK/venv/bin/pip" install --quiet --upgrade pip
    "$WORK/venv/bin/pip" install --quiet "pyjwt[crypto]>=2.8"
}

mint_jwt() {
    # $1 = path to .p8   $2 = key id   $3 = issuer id
    setup_python
    "$WORK/venv/bin/python3" - "$1" "$2" "$3" <<'PY'
import sys, time, jwt
p8, kid, iss = sys.argv[1], sys.argv[2], sys.argv[3]
with open(p8, "rb") as f: key = f.read()
print(jwt.encode(
    {"iss": iss, "exp": int(time.time()) + 1200, "aud": "appstoreconnect-v1"},
    key, algorithm="ES256", headers={"kid": kid, "typ": "JWT"}))
PY
}

# Try POST /v1/certificates with a given type + base64 CSR.
# Echoes the cert body (PEM) on stdout, exits non-zero on failure.
api_create_cert() {
    local jwt="$1" csr_b64="$2" type="$3"
    local body
    body="$(jq -c -n --arg csr "$csr_b64" --arg t "$type" '{
        data: {
            type: "certificates",
            attributes: { csrContent: $csr, certificateType: $t }
        }
    }')"
    local resp http
    resp="$(curl -sS -w '\n%{http_code}' -X POST \
        -H "Authorization: Bearer $jwt" \
        -H "Content-Type: application/json" \
        --data "$body" \
        https://api.appstoreconnect.apple.com/v1/certificates)"
    http="$(printf '%s\n' "$resp" | tail -n1)"
    local json; json="$(printf '%s\n' "$resp" | sed '$d')"
    if [[ "$http" != "201" ]]; then
        echo "  API error ($http):" >&2
        echo "$json" | jq -r '.errors // .' >&2
        return 1
    fi
    # certificateContent is base64-DER of the issued cert.
    local b64; b64="$(printf '%s' "$json" | jq -r '.data.attributes.certificateContent')"
    if [[ -z "$b64" || "$b64" == "null" ]]; then
        echo "  API succeeded but returned no certificateContent" >&2
        return 1
    fi
    printf '%s' "$b64" | base64 --decode | openssl x509 -inform DER -outform PEM
}

# Generate a private key + a CSR for it. Outputs (key, csr_b64) into the
# given path prefix.
make_csr() {
    local prefix="$1" cn="$2"
    openssl genrsa -out "$prefix.key" 2048 >/dev/null 2>&1
    openssl req -new -key "$prefix.key" -out "$prefix.csr" \
        -subj "/CN=$cn/O=DatanoiseTV" >/dev/null 2>&1
    # ASC API wants the CSR base64-encoded *without* the BEGIN/END lines
    # and without newlines.
    sed -n '/BEGIN CERTIFICATE REQUEST/,/END CERTIFICATE REQUEST/p' "$prefix.csr" \
        | sed '1d;$d' | tr -d '\n'
}

bundle_p12() {
    # $1 = pem cert   $2 = private key   $3 = output .p12   $4 = password
    openssl pkcs12 -export \
        -in "$1" -inkey "$2" \
        -out "$3" -passout "pass:$4" \
        -name "$5"
}

set_secret_from_file() {
    # $1 = secret name   $2 = file path (will be base64-encoded into the secret)
    base64 -i "$2" | gh secret set "$1" --repo "$REPO"
}

set_secret_text() {
    gh secret set "$1" --repo "$REPO" --body "$2"
}

# ---- Step 1: App Store Connect API key (manual, one-time) -------------------
cat <<'BANNER'

================================================================================
 Step 1 / 3  --  App Store Connect API key
================================================================================
There is no API to create an API key (chicken-and-egg). One-time, ~30 seconds:

  1. Browser will open https://appstoreconnect.apple.com/access/integrations/api
  2. Click "+", give it a name like "doobie-ci", role: "Developer".
  3. Download the .p8 file (you only get one chance -- back it up somewhere).
  4. Note the Key ID (table column) and Issuer ID (text above the table).

BANNER
read -rp "Open the page in your browser now? [Y/n] " yn
[[ "${yn:-Y}" =~ ^[Yy] ]] && open_url "https://appstoreconnect.apple.com/access/integrations/api"
pause

P8_PATH="$(prompt        '  Path to the .p8 you just downloaded : ')"
P8_PATH="${P8_PATH/#\~/$HOME}"
[[ -f "$P8_PATH" ]] || { echo "file not found: $P8_PATH" >&2; exit 1; }
ASC_KEY_ID="$(prompt     '  Key ID (10-char alphanumeric)        : ')"
ASC_ISSUER_ID="$(prompt  '  Issuer ID (UUID)                     : ')"
TEAM_ID="$(prompt        '  Team ID (10-char, dev membership)    : ')"

# Quick sanity check: mint a JWT and ping the user-info-ish endpoint.
echo
echo "  Verifying API key by listing existing certificates..."
JWT="$(mint_jwt "$P8_PATH" "$ASC_KEY_ID" "$ASC_ISSUER_ID")"
EXISTING="$(curl -sS -H "Authorization: Bearer $JWT" \
            'https://api.appstoreconnect.apple.com/v1/certificates?limit=200')"
if ! printf '%s' "$EXISTING" | jq -e '.data' >/dev/null; then
    echo "  API key verification failed:" >&2
    printf '%s\n' "$EXISTING" | jq -r '.errors // .' >&2
    exit 1
fi
echo "  OK -- API key works."

# ---- Step 2: Developer ID Application certificate (auto) -------------------
cat <<'BANNER'

================================================================================
 Step 2 / 3  --  Developer ID Application certificate
================================================================================
Generating a fresh RSA 2048 key + CSR locally, then asking the App Store
Connect API to issue a "Developer ID Application" cert against it.

BANNER

CN_APP="Doobie CI Application $(date +%Y%m%d)"
APP_CSR_B64="$(make_csr "$WORK/app" "$CN_APP")"

APP_CERT_PEM="$WORK/app.cert.pem"
if api_create_cert "$JWT" "$APP_CSR_B64" "DEVELOPER_ID_APPLICATION_G2" > "$APP_CERT_PEM"; then
    echo "  Developer ID Application cert issued."
elif api_create_cert "$JWT" "$APP_CSR_B64" "DEVELOPER_ID_APPLICATION" > "$APP_CERT_PEM"; then
    echo "  Developer ID Application cert issued (legacy type)."
else
    echo
    echo "  API would not issue this cert. Fallback: do it in the portal." >&2
    echo "  1. The CSR we generated is at: $WORK/app.csr" >&2
    echo "  2. Open: https://developer.apple.com/account/resources/certificates/add" >&2
    echo "  3. Pick 'Developer ID Application' -> upload the CSR above." >&2
    echo "  4. Download the .cer -> save its path." >&2
    open_url "https://developer.apple.com/account/resources/certificates/add"
    APP_CER="$(prompt '  Path to the downloaded .cer : ')"
    APP_CER="${APP_CER/#\~/$HOME}"
    openssl x509 -inform DER -in "$APP_CER" -outform PEM -out "$APP_CERT_PEM"
fi

# Derive the human-readable identity CN (codesign needs the exact string).
APP_SUBJECT_CN="$(openssl x509 -in "$APP_CERT_PEM" -noout -subject -nameopt RFC2253 \
                  | sed -n 's/^subject= *//p' | sed -n 's/.*CN=\([^,]*\).*/\1/p')"
APP_IDENTITY="$APP_SUBJECT_CN"
echo "  Identity CN: $APP_IDENTITY"

APP_P12_PWD="$(openssl rand -hex 16)"
APP_P12="$WORK/app.p12"
bundle_p12 "$APP_CERT_PEM" "$WORK/app.key" "$APP_P12" "$APP_P12_PWD" "Doobie Developer ID Application"

# ---- Step 3: Developer ID Installer certificate ----------------------------
cat <<'BANNER'

================================================================================
 Step 3 / 3  --  Developer ID Installer certificate
================================================================================
Trying the API first; falling back to the web portal if the API rejects it
(fastlane reports the same behaviour: API-key auth can't reliably issue
DEVELOPER_ID_INSTALLER, only Apple-ID web sessions can).

BANNER

CN_INS="Doobie CI Installer $(date +%Y%m%d)"
INS_CSR_B64="$(make_csr "$WORK/ins" "$CN_INS")"

INS_CERT_PEM="$WORK/ins.cert.pem"
if api_create_cert "$JWT" "$INS_CSR_B64" "DEVELOPER_ID_INSTALLER" > "$INS_CERT_PEM" 2>/dev/null; then
    echo "  Developer ID Installer cert issued via API (lucky!)."
else
    echo
    echo "  API rejected (expected). Doing it in the portal now:" >&2
    echo "    1. Click '+' on the Certificates page that just opened." >&2
    echo "    2. Pick 'Developer ID Installer' -> Continue." >&2
    echo "    3. Upload the CSR at:  $WORK/ins.csr" >&2
    echo "    4. Download the issued .cer file." >&2
    open_url "https://developer.apple.com/account/resources/certificates/add"
    INS_CER="$(prompt '  Path to the downloaded Installer .cer : ')"
    INS_CER="${INS_CER/#\~/$HOME}"
    [[ -f "$INS_CER" ]] || { echo "file not found" >&2; exit 1; }
    openssl x509 -inform DER -in "$INS_CER" -outform PEM -out "$INS_CERT_PEM"
fi

INS_SUBJECT_CN="$(openssl x509 -in "$INS_CERT_PEM" -noout -subject -nameopt RFC2253 \
                  | sed -n 's/^subject= *//p' | sed -n 's/.*CN=\([^,]*\).*/\1/p')"
INS_IDENTITY="$INS_SUBJECT_CN"
echo "  Identity CN: $INS_IDENTITY"

INS_P12_PWD="$(openssl rand -hex 16)"
INS_P12="$WORK/ins.p12"
bundle_p12 "$INS_CERT_PEM" "$WORK/ins.key" "$INS_P12" "$INS_P12_PWD" "Doobie Developer ID Installer"

# ---- Set the secrets --------------------------------------------------------
echo
echo "================================================================================"
echo " Pushing 10 secrets to $REPO ..."
echo "================================================================================"

KEYCHAIN_PWD="$(openssl rand -hex 16)"

set_secret_from_file MACOS_CERTIFICATE             "$APP_P12"
set_secret_text       MACOS_CERTIFICATE_PWD         "$APP_P12_PWD"

set_secret_from_file MACOS_INSTALLER_CERTIFICATE   "$INS_P12"
set_secret_text       MACOS_INSTALLER_CERTIFICATE_PWD "$INS_P12_PWD"

set_secret_from_file APPLE_NOTARY_KEY_P8           "$P8_PATH"
set_secret_text       APPLE_NOTARY_KEY_ID           "$ASC_KEY_ID"
set_secret_text       APPLE_NOTARY_ISSUER_ID        "$ASC_ISSUER_ID"

set_secret_text       APPLE_TEAM_ID                 "$TEAM_ID"
set_secret_text       MACOS_APPLICATION_IDENTITY    "$APP_IDENTITY"
set_secret_text       MACOS_INSTALLER_IDENTITY      "$INS_IDENTITY"

set_secret_text       MACOS_KEYCHAIN_PWD            "$KEYCHAIN_PWD"

cat <<DONE

================================================================================
 Done.
================================================================================
Verify with:    gh secret list --repo $REPO
Trigger build:  push any commit to main, or run release.yml manually.

The .p12 files + private keys + CSRs live in $WORK and will be removed when
this script exits. The secrets are now in GitHub -- you do NOT need to keep
the .p12 files around unless you want a local copy. Apple lets you re-issue
both certs at any time if you lose them; the App Store Connect API key can
also be revoked + re-issued at any time.

If you ever want to rotate everything: re-run this script with a fresh
.p8, and pass --revoke-old to revoke the previous certs before issuing new
ones (not yet implemented).
DONE
