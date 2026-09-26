# macOS installer

`installer/mac/build_pkg.sh [mac-release]` (run on a Mac after building the preset, or by CI) creates
`build/installer/AUGUR-5 <version> (macOS).pkg` with three choices: VST3, AU and the standalone app.
The binaries are universal (arm64 + x86_64), macOS 11+.

## Opening an unsigned installer (until TONAL LAB has an Apple Developer ID)

The package is not notarized yet, so macOS blocks it the first time:

1. Double-click the .pkg. macOS says it "cannot be opened". Click **Done**.
2. Open **System Settings > Privacy & Security**, scroll down and click **Open Anyway** next to
   "AUGUR-5 ... (macOS).pkg". Confirm with your password.
3. Follow the installer. Then rescan plug-ins in your DAW (Logic rescans AudioUnits automatically).

(Español: la primera vez macOS lo bloquea por no estar notarizado. Ve a Ajustes del Sistema >
Privacidad y seguridad > "Abrir igualmente", y sigue el instalador.)

## Signing and notarization (when a Developer ID is available)

1. Sign the bundles instead of the ad-hoc signature:
   `codesign --force --deep --options runtime --timestamp --sign "Developer ID Application: TONAL LAB (TEAMID)" <bundle>`
2. Sign the product: `productbuild ... --sign "Developer ID Installer: TONAL LAB (TEAMID)" <pkg>`
3. Notarize and staple:
   `xcrun notarytool submit <pkg> --apple-id <id> --team-id <TEAMID> --password <app-specific-password> --wait`
   `xcrun stapler staple <pkg>`
Store the certificates and the password as GitHub secrets and add these steps to CI.
