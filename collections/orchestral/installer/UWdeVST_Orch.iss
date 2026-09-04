#define AppName "uwdevst_orch"
#ifndef AppVersion
#define AppVersion "1.0.2"
#endif
#define AppPublisher "unicorn who dev / Charli Billabert"
#define AppExeName "uwdevst_orch.exe"
#define Vst3BundleName "uwdevst_orch.vst3"

[Setup]
AppId={{7B2E1F4A-0D62-4D6E-9C61-9BAE1B8ACDE4}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\unicorn who dev\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=output
OutputBaseFilename=uwdevst_orch_{#AppVersion}_Windows_x64_Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
SetupLogging=yes
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Components]
Name: "standalone"; Description: "Application standalone"; Flags: fixed
Name: "vst3"; Description: "Plugin VST3 64-bit"; Flags: fixed

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone

[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Vst3BundleName}"

[Files]
Source: "staging\Standalone\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "staging\VST3\{#Vst3BundleName}\*"; DestDir: "{commoncf64}\VST3\{#Vst3BundleName}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Components: standalone
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Vst3BundleName}"
