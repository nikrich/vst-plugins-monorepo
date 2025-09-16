#define AppName "Hungry Ghost Spectral Limiter"
#define AppVersion "0.1.1"
#define Company "Hungry Ghost"

[Setup]
AppId={{E1F1D5F3-4F9C-40FD-9C9D-123456789ABC}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#Company}
DefaultDirName={autopf}\{#Company}\{#AppName}   ; uninstaller lives here
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
OutputDir=dist
OutputBaseFilename={#AppName}-Setup-{#AppVersion}
Compression=lzma
SolidCompression=yes

[Files]
; --- VST3 ---
Source: "build\\Release\\{#AppName}.vst3"; DestDir: "{commoncf}\\VST3"; Flags: ignoreversion
; --- AAX (optional) ---
Source: "build\\Release\\{#AppName}.aaxplugin\\*"; \
    DestDir: "{commoncf}\\Avid\\Audio\\Plug-Ins\\{#AppName}.aaxplugin"; \
    Flags: recursesubdirs createallsubdirs
; --- Presets / resources (optional) ---
Source: "Presets\\*"; DestDir: "{commonappdata}\\{#Company}\\{#AppName}\\Presets"; Flags: recursesubdirs

; Bundle Microsoft VC++ Redistributable (optional but recommended)
Source: "redist\\VC_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Run]
; Install VC++ runtime silently
Filename: "{tmp}\\VC_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    Flags: waituntilterminated; StatusMsg: "Installing Microsoft Visual C++ Runtime..."

[UninstallDelete]
Type: filesandordirs; Name: "{commonappdata}\\{#Company}\\{#AppName}"
