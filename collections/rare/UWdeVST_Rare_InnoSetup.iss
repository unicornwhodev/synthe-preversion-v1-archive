#ifndef MyAppName
  #define MyAppName "uwdevst_rare"
#endif

#ifndef MyAppVersion
  #define MyAppVersion "1.0.2"
#endif

#ifndef MyAppPublisher
  #define MyAppPublisher "unicorn who dev / Charli Billabert"
#endif

#ifndef StandaloneSource
  #error StandaloneSource must be provided to ISCC.
#endif

#ifndef StandaloneExeName
  #error StandaloneExeName must be provided to ISCC.
#endif

#ifndef Vst3Source
  #error Vst3Source must be provided to ISCC.
#endif

#ifndef Vst3DirName
  #error Vst3DirName must be provided to ISCC.
#endif

#ifndef OutputDir
  #define OutputDir "dist"
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "uwdevst_rare_1.0.2_Windows_x64_Setup"
#endif

[Setup]
AppId={{5A3F33AF-622D-456D-B6D4-B0FD37FB6A87}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf64}\unicorn who dev\{#MyAppName}
DefaultGroupName=unicorn who dev\{#MyAppName}
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#StandaloneExeName}

[Types]
Name: "full"; Description: "Standalone + VST3"; Flags: iscustom
Name: "standalone"; Description: "Standalone only"
Name: "vst3"; Description: "VST3 only"

[Components]
Name: "standalone"; Description: "Application standalone"; Types: full standalone
Name: "vst3"; Description: "Plugin VST3"; Types: full vst3

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked; Components: standalone

[InstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Vst3DirName}"; Components: vst3

[Files]
Source: "{#StandaloneSource}"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
Source: "{#Vst3Source}\*"; DestDir: "{commoncf64}\VST3\{#Vst3DirName}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3

[Icons]
Name: "{autoprograms}\unicorn who dev\{#MyAppName}"; Filename: "{app}\{#StandaloneExeName}"; Components: standalone
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#StandaloneExeName}"; Tasks: desktopicon; Components: standalone

[Run]
Filename: "{app}\{#StandaloneExeName}"; Description: "Launch {#MyAppName}"; Flags: nowait postinstall skipifsilent; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#Vst3DirName}"
