#!/usr/bin/env bash
# Builds the macOS installer: AUGUR-5 <version> (macOS).pkg
#   VST3       -> /Library/Audio/Plug-Ins/VST3        (Ableton Live, Cubase, Bitwig, FL Studio, Reaper, Studio One)
#   AU         -> /Library/Audio/Plug-Ins/Components  (Logic Pro, GarageBand)
#   Standalone -> /Applications
# The bundles are universal (arm64 + x86_64). Without a Developer ID certificate they are ad-hoc signed
# (required on Apple Silicon); signing + notarization with a real certificate is documented in
# installer/mac/README.md.
# Usage: installer/mac/build_pkg.sh [preset=mac-release]
set -euo pipefail

PRESET="${1:-mac-release}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="$ROOT/build/$PRESET/plugin/Augur5_artefacts/Release"
VERSION="$(sed -nE 's/^project\(AUGUR5 VERSION ([0-9.]+).*/\1/p' "$ROOT/CMakeLists.txt")"
OUT="$ROOT/build/installer"
STAGE="$OUT/stage"

rm -rf "$OUT"
mkdir -p "$STAGE/vst3" "$STAGE/au" "$STAGE/app" "$OUT/pkgs" "$OUT/scripts/vst3" "$OUT/scripts/au" "$OUT/scripts/app"

cp -R "$ART/VST3/AUGUR-5.vst3" "$STAGE/vst3/"
cp -R "$ART/AU/AUGUR-5.component" "$STAGE/au/"
cp -R "$ART/Standalone/AUGUR-5.app" "$STAGE/app/"

# Ad-hoc signature over the finished bundles (a Developer ID signature replaces this when available).
for b in "$STAGE/vst3/AUGUR-5.vst3" "$STAGE/au/AUGUR-5.component" "$STAGE/app/AUGUR-5.app"; do
    codesign --force --deep --sign - "$b"
done

# Post-install: clear the download quarantine and make Logic rescan its AudioUnits.
cat > "$OUT/scripts/vst3/postinstall" <<'SH'
#!/bin/sh
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/VST3/AUGUR-5.vst3" 2>/dev/null || true
exit 0
SH
cat > "$OUT/scripts/au/postinstall" <<'SH'
#!/bin/sh
xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/AUGUR-5.component" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
SH
cat > "$OUT/scripts/app/postinstall" <<'SH'
#!/bin/sh
xattr -dr com.apple.quarantine "/Applications/AUGUR-5.app" 2>/dev/null || true
exit 0
SH
chmod +x "$OUT"/scripts/*/postinstall

pkgbuild --root "$STAGE/vst3" --install-location "/Library/Audio/Plug-Ins/VST3" --identifier com.tonallab.augur5.vst3 \
    --version "$VERSION" --scripts "$OUT/scripts/vst3" "$OUT/pkgs/augur5-vst3.pkg"
pkgbuild --root "$STAGE/au" --install-location "/Library/Audio/Plug-Ins/Components" --identifier com.tonallab.augur5.au \
    --version "$VERSION" --scripts "$OUT/scripts/au" "$OUT/pkgs/augur5-au.pkg"
pkgbuild --root "$STAGE/app" --install-location "/Applications" --identifier com.tonallab.augur5.app \
    --version "$VERSION" --scripts "$OUT/scripts/app" "$OUT/pkgs/augur5-app.pkg"

sed "s/@VERSION@/$VERSION/g" "$ROOT/installer/mac/distribution.xml" > "$OUT/distribution.xml"
productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT/pkgs" --resources "$ROOT/installer/mac/resources" \
    "$OUT/AUGUR-5 $VERSION (macOS).pkg"

pkgutil --check-signature "$OUT/AUGUR-5 $VERSION (macOS).pkg" || true
ls -la "$OUT"/*.pkg
