; AUGUR-5 "3340" - Windows installer (Inno Setup 6).
;   iscc /DVersion=1.3.0 /DPreset=win-release installer\win\augur5.iss
; Components: the VST3 plug-in (the standard shared folder every host scans), the standalone application and the
; SUN ATLAS preset library. English and Spanish. The AppId never changes: new versions install over old ones.
#ifndef Version
  #define Version "1.3.0"
#endif
#ifndef Preset
  #define Preset "win-release"
#endif
#define Art "..\..\build\" + Preset + "\plugin\Augur5_artefacts\Release"
#define Atlas "SUN ATLAS"

[Setup]
AppId={{780FC564-3C21-42F9-B544-D66D96CFA480}
AppName=AUGUR-5
AppVersion={#Version}
AppVerName=AUGUR-5 {#Version}
AppPublisher=TONAL LAB
AppPublisherURL=https://tonallab.audio
AppSupportURL=https://tonallab.audio
AppCopyright=Copyright (C) 2026 TONAL LAB
VersionInfoVersion={#Version}
VersionInfoCompany=TONAL LAB
VersionInfoDescription=AUGUR-5 installer
VersionInfoProductName=AUGUR-5
DefaultDirName={autopf}\TONAL LAB\AUGUR-5
DefaultGroupName=TONAL LAB\AUGUR-5
DisableProgramGroupPage=yes
OutputDir=..\..\build\installer
OutputBaseFilename=AUGUR-5 {#Version} Setup (Windows x64)
SetupIconFile=art\icon.ico
UninstallDisplayIcon={app}\AUGUR-5.exe
UninstallDisplayName=AUGUR-5 {#Version}
WizardStyle=modern
WizardImageFile=art\wizard.bmp,art\wizard@2x.bmp
WizardSmallImageFile=art\wizard_small.bmp,art\wizard_small@2x.bmp
LicenseFile=license.txt
InfoBeforeFile=readme.txt
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=admin
UsedUserAreasWarning=no
CloseApplications=yes
ShowLanguageDialog=auto

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[CustomMessages]
english.TypeFull=Full installation (recommended)
spanish.TypeFull=Instalación completa (recomendada)
english.TypePlugins=Plug-in only (VST3)
spanish.TypePlugins=Solo el plug-in (VST3)
english.TypeCustom=Custom installation
spanish.TypeCustom=Instalación personalizada
english.CompVst3=VST3 plug-in - Ableton Live, Cubase, FL Studio, Reaper, Bitwig, Studio One, Cakewalk, Reason
spanish.CompVst3=Plug-in VST3 - Ableton Live, Cubase, FL Studio, Reaper, Bitwig, Studio One, Cakewalk, Reason
english.CompApp=Standalone application (plays without a DAW)
spanish.CompApp=Aplicación independiente (sin DAW)
english.CompAtlas=Preset library: SUN ATLAS (1000 presets)
spanish.CompAtlas=Librería de presets: SUN ATLAS (1000 presets)
english.Guide=AUGUR-5 Quick Start
spanish.Guide=AUGUR-5 Guía rápida
english.RunApp=Open AUGUR-5 now
spanish.RunApp=Abrir AUGUR-5 ahora
english.OpenGuide=Read the Quick Start guide
spanish.OpenGuide=Leer la guía rápida

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "plugins"; Description: "{cm:TypePlugins}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full plugins custom
Name: "app"; Description: "{cm:CompApp}"; Types: full custom
Name: "atlas"; Description: "{cm:CompAtlas}"; Types: full plugins custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: app

[InstallDelete]
; A clean plug-in bundle on every update (no stale files from an older version).
Type: filesandordirs; Name: "{commoncf64}\VST3\AUGUR-5.vst3"; Components: vst3
Type: filesandordirs; Name: "{userappdata}\TONAL LAB\AUGUR-5\Presets\{#Atlas}"; Components: atlas

[Files]
Source: "{#Art}\VST3\AUGUR-5.vst3\*"; DestDir: "{commoncf64}\VST3\AUGUR-5.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#Art}\Standalone\AUGUR-5.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
; Presets go to the per-user folder the plug-in browses (%APPDATA%\TONAL LAB\AUGUR-5\Presets); the browser shows the
; folder as the SUN ATLAS collection. The 170 factory sounds live inside the plug-in.
Source: "..\..\packs\{#Atlas}\*"; DestDir: "{userappdata}\TONAL LAB\AUGUR-5\Presets\{#Atlas}"; Components: atlas; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "guide.html"; DestDir: "{app}"; DestName: "AUGUR-5 Quick Start.html"; Flags: ignoreversion
Source: "license.txt"; DestDir: "{app}"; DestName: "License.txt"; Flags: ignoreversion
Source: "third_party_notices.txt"; DestDir: "{app}"; DestName: "Third-Party Notices.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\AUGUR-5"; Filename: "{app}\AUGUR-5.exe"; Components: app
Name: "{group}\{cm:Guide}"; Filename: "{app}\AUGUR-5 Quick Start.html"
Name: "{group}\{cm:UninstallProgram,AUGUR-5}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\AUGUR-5"; Filename: "{app}\AUGUR-5.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\AUGUR-5 Quick Start.html"; Description: "{cm:OpenGuide}"; Flags: postinstall shellexec skipifsilent unchecked
Filename: "{app}\AUGUR-5.exe"; Description: "{cm:RunApp}"; Components: app; Flags: postinstall nowait skipifsilent

[UninstallDelete]
; The plug-in goes; the user's own presets and settings stay.
Type: filesandordirs; Name: "{commoncf64}\VST3\AUGUR-5.vst3"
