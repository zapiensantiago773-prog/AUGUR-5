# Validates the built VST3 with pluginval (downloaded into tools/bin on first use).
# Usage: powershell -File tools/pluginval.ps1 [-Preset win-release] [-Strictness 10] [-SkipGui]
param(
    [string]$Preset = "win-release",
    [int]$Strictness = 10,
    [switch]$SkipGui
)
$ErrorActionPreference = "Stop"

$version = "v1.0.4"
$root = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $PSScriptRoot "bin"
$exe = Join-Path $binDir "pluginval.exe"

if (-not (Test-Path $exe)) {
    New-Item -ItemType Directory -Force $binDir | Out-Null
    $zip = Join-Path $binDir "pluginval.zip"
    Invoke-WebRequest "https://github.com/Tracktion/pluginval/releases/download/$version/pluginval_Windows.zip" -OutFile $zip
    Expand-Archive $zip -DestinationPath $binDir -Force
    Remove-Item $zip
}

$config = if ($Preset -like "*debug") { "Debug" } else { "Release" }
$plugin = Join-Path $root "build/$Preset/plugin/Augur5_artefacts/$config/VST3/AUGUR-5.vst3"
if (-not (Test-Path $plugin)) { throw "Plugin not found: $plugin (build the '$Preset' preset first)" }

$pvArgs = @("--strictness-level", $Strictness, "--validate-in-process", "--output-dir", (Join-Path $root "build/pluginval"))
if ($SkipGui) { $pvArgs += "--skip-gui-tests" }

& $exe @pvArgs --validate $plugin
exit $LASTEXITCODE
