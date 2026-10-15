# AUGUR-5 brand art for the Windows installer (run from anywhere):
#   powershell -ExecutionPolicy Bypass -File installer\win\art\make_art.ps1
# Writes, next to this script:
#   icon.ico (16..256 px)                               - installer / uninstaller icon: the TONAL LAB app icon
#   wizard.bmp / wizard@2x.bmp (164x314, 328x628)       - installer side panel: AUGUR's paper, its type-only name
#                                                         and the TONAL LAB stacked logo (light-background colouring)
#   wizard_small.bmp / wizard_small@2x.bmp (55, 110 px) - installer header mark
# Sources: plugin/resources/brand/tonal_lab/tonal-lab-app-icon.png, docs/brand/tonal_lab/*.png,
# fonts (OFL) from plugin/resources/fonts.
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..\..')
$fonts = New-Object System.Drawing.Text.PrivateFontCollection
$fonts.AddFontFile((Join-Path $root 'plugin\resources\fonts\Michroma-Regular.ttf'))
$fonts.AddFontFile((Join-Path $root 'plugin\resources\fonts\Jost-Light.ttf'))
$fonts.AddFontFile((Join-Path $root 'plugin\resources\fonts\Jost-Medium.ttf'))
$michroma = $fonts.Families | Where-Object { $_.Name -like 'Michroma*' } | Select-Object -First 1
$jost = $fonts.Families | Where-Object { $_.Name -like 'Jost*' } | Select-Object -First 1

# The GUI's palette (plugin/gui/Theme.h).
$paperTop = [System.Drawing.Color]::FromArgb(255, 255, 255, 255)
$paperBottom = [System.Drawing.Color]::FromArgb(255, 233, 230, 224)
$ink = [System.Drawing.Color]::FromArgb(255, 27, 27, 29)
$accent = [System.Drawing.Color]::FromArgb(255, 200, 85, 47)
$slate = [System.Drawing.Color]::FromArgb(255, 60, 102, 132)
$caption = [System.Drawing.Color]::FromArgb(255, 106, 102, 95)
$hairline = [System.Drawing.Color]::FromArgb(255, 217, 212, 204)

function Save-Bmp($bmp, $path) {
    # Inno Setup wants a 24-bit BMP.
    $out = New-Object System.Drawing.Bitmap $bmp.Width, $bmp.Height, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [System.Drawing.Graphics]::FromImage($out); $g.DrawImage($bmp, 0, 0, $bmp.Width, $bmp.Height); $g.Dispose()
    $out.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp); $out.Dispose()
}

function Resample($img, $size, $background) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.SmoothingMode = 'AntiAlias'
    $g.Clear($background)
    $g.DrawImage($img, 0, 0, $size, $size); $g.Dispose()
    return $bmp
}

# ---------------------------------------------------------------- icon (the app's own icon: plugin/CMakeLists.txt ICON_BIG)
$appIcon = [System.Drawing.Image]::FromFile((Join-Path $root 'plugin\resources\brand\tonal_lab\tonal-lab-app-icon.png'))
# ICO with PNG-compressed entries (Windows Vista+).
$sizes = 16, 24, 32, 48, 64, 128, 256
$pngs = @()
foreach ($s in $sizes) {
    $bmp = Resample $appIcon $s ([System.Drawing.Color]::Transparent)
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png); $pngs += , $ms.ToArray(); $bmp.Dispose()
}
$fs = [System.IO.File]::Create((Join-Path $here 'icon.ico'))
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([UInt16]1); $bw.Write([UInt16]32); $bw.Write([UInt32]$pngs[$i].Length); $bw.Write([UInt32]$offset)
    $offset += $pngs[$i].Length
}
foreach ($p in $pngs) { $bw.Write($p) }
$bw.Close()

# ---------------------------------------------------------------- wizard side panel
function Make-Wizard($scale) {
    $w = 164 * $scale; $h = 314 * $scale
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'; $g.InterpolationMode = 'HighQualityBicubic'
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $br = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $paperTop, $paperBottom, 90
    $g.FillRectangle($br, $rect); $br.Dispose()

    $fmt = New-Object System.Drawing.StringFormat; $fmt.Alignment = 'Center'
    $px = [System.Drawing.GraphicsUnit]::Pixel
    $fWord = New-Object System.Drawing.Font $jost, ([float](21 * $scale)), ([System.Drawing.FontStyle]::Regular), $px
    $fFive = New-Object System.Drawing.Font $jost, ([float](40 * $scale)), ([System.Drawing.FontStyle]::Regular), $px
    $fModel = New-Object System.Drawing.Font $michroma, ([float](9 * $scale)), ([System.Drawing.FontStyle]::Regular), $px
    $fCap = New-Object System.Drawing.Font $jost, ([float](8 * $scale)), ([System.Drawing.FontStyle]::Regular), $px
    $bInk = New-Object System.Drawing.SolidBrush $ink
    $bAccent = New-Object System.Drawing.SolidBrush $accent
    $bSlate = New-Object System.Drawing.SolidBrush $slate
    $bCap = New-Object System.Drawing.SolidBrush $caption

    # The name, type only (as in the plug-in's header): AUGUR, the accent dash, 5 in the accent, the model in slate.
    $g.DrawString('A U G U R', $fWord, $bInk, (New-Object System.Drawing.RectangleF 0, ([float](40 * $scale)), $w, ([float](30 * $scale))), $fmt)
    $g.FillRectangle($bAccent, ([float]($w / 2 - 8 * $scale)), ([float](80 * $scale)), ([float](16 * $scale)), ([float](2 * $scale)))
    $g.DrawString('5', $fFive, $bAccent, (New-Object System.Drawing.RectangleF 0, ([float](84 * $scale)), $w, ([float](54 * $scale))), $fmt)
    $g.DrawString('3 3 4 0', $fModel, $bSlate, (New-Object System.Drawing.RectangleF 0, ([float](140 * $scale)), $w, ([float](16 * $scale))), $fmt)
    $g.DrawString('ANALOG MODELING', $fCap, $bCap, (New-Object System.Drawing.RectangleF 0, ([float](166 * $scale)), $w, ([float](12 * $scale))), $fmt)
    $g.DrawString('5-VOICE POLYSYNTH', $fCap, $bCap, (New-Object System.Drawing.RectangleF 0, ([float](178 * $scale)), $w, ([float](12 * $scale))), $fmt)

    $pen = New-Object System.Drawing.Pen $hairline, ([float]$scale)
    $g.DrawLine($pen, ([float](34 * $scale)), ([float](206 * $scale)), ([float]($w - 34 * $scale)), ([float](206 * $scale))); $pen.Dispose()

    # The maker: the TONAL LAB stacked logo, light-background colouring (the one the plug-in's panel uses).
    $brand = [System.Drawing.Image]::FromFile((Join-Path $root 'docs\brand\tonal_lab\tonal-lab-logo-stacked-light-bg.png'))
    $lw = 92 * $scale; $lh = $lw * $brand.Height / $brand.Width
    $g.DrawImage($brand, ([float](($w - $lw) / 2)), ([float](222 * $scale)), ([float]$lw), ([float]$lh))
    $brand.Dispose()
    $g.Dispose()
    return $bmp
}
foreach ($s in 1, 2) {
    $bmp = Make-Wizard $s
    Save-Bmp $bmp (Join-Path $here ($(if ($s -eq 1) { 'wizard.bmp' } else { 'wizard@2x.bmp' })))
    $bmp.Dispose()
}
foreach ($s in 1, 2) {
    $bmp = Resample $appIcon (55 * $s) ([System.Drawing.Color]::White)
    Save-Bmp $bmp (Join-Path $here ($(if ($s -eq 1) { 'wizard_small.bmp' } else { 'wizard_small@2x.bmp' })))
    $bmp.Dispose()
}
$appIcon.Dispose()
Write-Output "art written to $here"
