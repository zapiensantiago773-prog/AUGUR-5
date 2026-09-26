#!/usr/bin/env bash
# Validates the built plugins with pluginval on macOS (downloaded into tools/bin on first use).
# The AU is copied to ~/Library/Audio/Plug-Ins/Components first: AudioUnits must be registered to load.
# Usage: tools/pluginval.sh [preset=mac-release] [strictness=10] [--skip-gui]
set -euo pipefail

PRESET="${1:-mac-release}"
STRICTNESS="${2:-10}"
EXTRA=()
if [[ "${3:-}" == "--skip-gui" ]]; then EXTRA+=(--skip-gui-tests); fi

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

COMPONENTS="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$COMPONENTS"
rm -rf "$COMPONENTS/AUGUR-5.component"
cp -R "$ARTEFACTS/AU/AUGUR-5.component" "$COMPONENTS/"
killall -9 AudioComponentRegistrar 2>/dev/null || true

LOG_DIR="$ROOT/build/pluginval"
mkdir -p "$LOG_DIR"
status=0
for PLUGIN in "$ARTEFACTS/VST3/AUGUR-5.vst3" "$COMPONENTS/AUGUR-5.component"; do
    LOG="$LOG_DIR/$(basename "$PLUGIN").log"
    if ! "$EXE" --strictness-level "$STRICTNESS" --validate-in-process ${EXTRA[@]+"${EXTRA[@]}"} --validate "$PLUGIN" > "$LOG" 2>&1; then
        status=1
        echo "pluginval FAILED for $PLUGIN"
        tail -n 60 "$LOG"
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            # Surface the failure as an annotation (readable without log access).
            echo "::error title=pluginval $(basename "$PLUGIN")::$(tail -n 25 "$LOG" | sed 's/%/%25/g' | awk '{printf "%s%%0A", $0}')"
        fi
    else
        echo "pluginval OK: $PLUGIN"
    fi
done
exit $status
