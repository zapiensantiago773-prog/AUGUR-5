#!/usr/bin/env bash
# Validates the built VST3 and AU with pluginval on macOS (downloaded into tools/bin on first use).
# Usage: tools/pluginval.sh [preset=mac-release] [strictness=10] [--skip-gui]
set -euo pipefail

PRESET="${1:-mac-release}"
STRICTNESS="${2:-10}"
EXTRA=()
[[ "${3:-}" == "--skip-gui" ]] && EXTRA+=(--skip-gui-tests)

VERSION="v1.0.4"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="$ROOT/tools/bin"
EXE="$BIN/pluginval.app/Contents/MacOS/pluginval"

if [[ ! -x "$EXE" ]]; then
    mkdir -p "$BIN"
    curl -sSL "https://github.com/Tracktion/pluginval/releases/download/$VERSION/pluginval_macOS.zip" -o "$BIN/pluginval.zip"
    unzip -oq "$BIN/pluginval.zip" -d "$BIN"
    rm "$BIN/pluginval.zip"
fi

CONFIG="Release"
[[ "$PRESET" == *debug ]] && CONFIG="Debug"
ARTEFACTS="$ROOT/build/$PRESET/plugin/Augur5_artefacts/$CONFIG"

for PLUGIN in "$ARTEFACTS/VST3/AUGUR-5.vst3" "$ARTEFACTS/AU/AUGUR-5.component"; do
    "$EXE" --strictness-level "$STRICTNESS" --validate-in-process --output-dir "$ROOT/build/pluginval" ${EXTRA[@]+"${EXTRA[@]}"} --validate "$PLUGIN"
done
