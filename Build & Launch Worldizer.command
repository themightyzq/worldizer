#!/bin/bash
#
# Worldizer — build the latest source (Release) and launch the standalone app.
#
#   • Double-click in Finder (opens a Terminal window and runs this), or
#   • run it from a terminal:  ./Build\ \&\ Launch\ Worldizer.command
#
# Builds both the VST3 (installed for your DAW) and the Standalone, then opens
# the Standalone. Incremental — unchanged files are skipped, so a quick relaunch
# is fast.
#
# NOTE: this does NOT re-bake presets. If you changed the ray tracer or IRBuilder
# (anything that alters the baked IRs), re-bake first, then run this again:
#       ./build/BakePresets_artefacts/Release/BakePresets
#
set -euo pipefail

# Always operate from the project root (the folder this script lives in).
cd "$(dirname "$0")"

echo "=========================================="
echo " Worldizer — build & launch"
echo " $(date '+%Y-%m-%d %H:%M:%S')"
echo " Project: $PWD"
echo "=========================================="

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

# Configure the build directory on first run (or if it was deleted).
if [ ! -f build/CMakeCache.txt ]; then
  echo "--- configuring build (first run) ---"
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
fi

echo "--- building Worldizer (VST3 + Standalone, Release, -j${JOBS}) ---"
cmake --build build --target Worldizer_All --config Release -j"${JOBS}"

APP="build/Worldizer_artefacts/Release/Standalone/Worldizer.app"
if [ ! -d "$APP" ]; then
  echo "ERROR: built app not found at $APP" >&2
  exit 1
fi

echo "--- launching $APP ---"
open "$APP"

echo ""
echo "Launched Worldizer. You can close this window."
