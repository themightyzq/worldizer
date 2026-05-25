#!/usr/bin/env bash
set -euo pipefail

# Clean Release build of Worldizer as a macOS Universal Binary (arm64 + x86_64).
# Run from anywhere; resolves the repo root relative to this script.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

VST3="build/Worldizer_artefacts/Release/VST3/Worldizer.vst3"

echo ""
echo "Build complete."
echo "VST3: $REPO_ROOT/$VST3"
