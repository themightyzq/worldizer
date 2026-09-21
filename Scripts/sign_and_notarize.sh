#!/usr/bin/env bash
set -euo pipefail

# Fill in environment variables before running. Not yet wired up for CI.
#
# TEMPLATE — code-sign and notarize the Worldizer VST3 for macOS distribution.
# See JUCE_VST3_BEST_PRACTICES.md §5 for the full explanation and prerequisites
# (Apple Developer membership, "Developer ID Application" certificate in Keychain).
#
# Credentials come from a notarytool keychain profile, never from an argument or an
# environment variable: a password on a command line is visible to every process via `ps`.
# One-time setup (notarytool prompts for the app-specific password itself):
#   xcrun notarytool store-credentials AC_PASSWORD --apple-id <apple-id> --team-id <team-id>

# --- Configure these ---------------------------------------------------------
DEVELOPER_ID="${DEVELOPER_ID:-Developer ID Application: Your Name (TEAM_ID)}"
NOTARY_PROFILE="${NOTARY_PROFILE:-AC_PASSWORD}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VST3="${1:-$REPO_ROOT/build/Worldizer_artefacts/Release/VST3/Worldizer.vst3}"
ZIP="${VST3%.vst3}.zip"
# -----------------------------------------------------------------------------

echo "Signing $VST3 ..."
codesign --force --deep --options runtime --timestamp \
    --sign "$DEVELOPER_ID" \
    "$VST3"

echo "Verifying signature ..."
codesign --verify --verbose=2 "$VST3"

echo "Zipping for notarization ..."
ditto -c -k --keepParent "$VST3" "$ZIP"

echo "Submitting for notarization (this can take a few minutes) ..."
xcrun notarytool submit "$ZIP" \
    --keychain-profile "$NOTARY_PROFILE" \
    --wait

echo "Stapling the ticket ..."
xcrun stapler staple "$VST3"

echo "Validating ..."
xcrun stapler validate "$VST3"
spctl --assess --type open --context context:primary-signature -v "$VST3"

echo "Done."
