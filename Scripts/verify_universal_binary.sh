#!/usr/bin/env bash
set -euo pipefail

# Verify that a built .vst3 contains both arm64 and x86_64 slices.
# Usage: ./Scripts/verify_universal_binary.sh [path/to/Worldizer.vst3]
# Defaults to the standard Release build location.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VST3="${1:-$REPO_ROOT/build/Worldizer_artefacts/Release/VST3/Worldizer.vst3}"

BINARY="$VST3/Contents/MacOS/$(basename "$VST3" .vst3)"

if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: binary not found at $BINARY"
    exit 1
fi

ARCH_INFO="$(file "$BINARY")"

if [[ "$ARCH_INFO" == *"arm64"* ]] && [[ "$ARCH_INFO" == *"x86_64"* ]]; then
    echo "Universal Binary: OK"
    echo "$ARCH_INFO"
else
    echo "WARNING: Not a Universal Binary"
    echo "$ARCH_INFO"
    exit 1
fi
