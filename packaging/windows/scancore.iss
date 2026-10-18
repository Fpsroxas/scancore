; Inno Setup script for the Windows installer. The release workflow passes
; /DAppVersion=x.y.z and /DSourceDir=<windeployqt output>.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\dist\windows"
#endif

[Setup]
AppId={{6B0C7E9E-2D51-4C1B-9E1B-5C0A3E2D7F41}
AppName=SCANCORE
AppVersion={#AppVersion}
AppPublisher=Medium Dev
AppPublisherURL=https://github.com/medium-gh/scancore
AppSupportURL=https://github.com/medium-gh/scancore/issues
DefaultDirName={autopf}\SCANCORE
DefaultGroupName=SCANCORE
DisableProgramGroupPage=yes
OutputBaseFilename=SCANCORE-{#AppVersion}-windows-x64-setup
SetupIconFile=scancore.ico
UninstallDisplayIcon={app}\scancore.exe
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern
ChangesAssociations=yes
LicenseFile=..\..\LICENSE

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Registry]
; .sct theme files open in SCANCORE, which offers to use the theme.
Root: HKA; Subkey: "Software\Classes\.sct"; ValueType: string; ValueName: ""; ValueData: "SCANCORE.Theme"; Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\SCANCORE.Theme"; ValueType: string; ValueName: ""; ValueData: "SCANCORE theme"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SCANCORE.Theme\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\scancore.exe,0"
Root: HKA; Subkey: "Software\Classes\SCANCORE.Theme\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\scancore.exe"" ""%1"""
Root: HKA; Subkey: "Software\Classes\Applications\scancore.exe\SupportedTypes"; ValueType: string; ValueName: ".sct"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\scancore.exe\SupportedTypes"; ValueType: string; ValueName: ".cbz"; ValueData: ""
Root: HKA; Subkey: "Software\Classes\Applications\scancore.exe\SupportedTypes"; ValueType: string; ValueName: ".epub"; ValueData: ""

[Icons]
Name: "{autoprograms}\SCANCORE"; Filename: "{app}\scancore.exe"
Name: "{autodesktop}\SCANCORE"; Filename: "{app}\scancore.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\scancore.exe"; Description: "{cm:LaunchProgram,SCANCORE}"; Flags: nowait postinstall skipifsilent
; In-app updates run the installer silently; start the new version afterwards.
Filename: "{app}\scancore.exe"; Flags: nowait skipifnotsilent
