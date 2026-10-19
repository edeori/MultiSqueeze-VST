[Setup]
AppId={{FBF20548-714A-4BC2-9442-40D79CB46C8A}
AppName=MultiSqueeze
AppVersion={#AppVersion}
AppPublisher=Moth Production
AppPublisherURL=https://mothproduction.hu
DefaultDirName={autopf}\Moth Production\MultiSqueeze
DefaultGroupName=Moth Production
OutputBaseFilename=installer
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
UninstallDisplayIcon={app}\MultiSqueeze.exe
WizardStyle=modern
[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom
[Components]
Name: "vst3"; Description: "VST3 plug-in"; Types: full custom
Name: "standalone"; Description: "Standalone application"; Types: full custom
[Files]
Source: "{#SourceRoot}\VST3\MultiSqueeze.vst3\*"; DestDir: "{commoncf64}\VST3\MultiSqueeze.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#SourceRoot}\Standalone\MultiSqueeze.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: standalone
[Icons]
Name: "{group}\MultiSqueeze"; Filename: "{app}\MultiSqueeze.exe"; Components: standalone
