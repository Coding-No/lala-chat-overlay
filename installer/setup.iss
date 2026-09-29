; Inno Setup 6 Script for YouTube Live Chat Overlay for OBS
; One-click installer for Windows 10/11 64-bit

#define MyAppName "Lala Live Chat Overlay"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Coding-No"
#define MyAppURL "https://github.com/Coding-No"
#define MyAppExeName "LalaLiveChatOverlay.exe"

[Setup]
AppId={{C67D29E1-8B33-4F74-9A2E-4D789B1015FC}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf64}\obs-studio\obs-plugins\64bit
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=YouTubeChatOverlaySetup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; OBS Plugin DLL
Source: "..\yt-chat-overlay.dll"; DestDir: "{code:GetObsPluginDir}"; Flags: ignoreversion
; Standalone Application
Source: "..\YouTubeChatOverlay.exe"; DestDir: "{autopf64}\YouTubeChatOverlay"; Flags: ignoreversion
; Default Config
Source: "..\config\overlay_config.json"; DestDir: "{userappdata}\obs-studio\plugin_config\yt-chat-overlay"; Flags: ignoreversion onlyifdoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{autopf64}\YouTubeChatOverlay\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{autopf64}\YouTubeChatOverlay\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Code]
// Custom function to detect OBS installation directory
function GetObsPluginDir(Param: String): String;
var
  ObsPath: String;
begin
  if RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', ObsPath) then
  begin
    Result := ObsPath + '\obs-plugins\64bit';
    Exit;
  end;
  if DirExists('C:\Program Files\obs-studio\obs-plugins\64bit') then
  begin
    Result := 'C:\Program Files\obs-studio\obs-plugins\64bit';
    Exit;
  end;
  // Fallback to user appdata plugin directory
  Result := ExpandConstant('{userappdata}\obs-studio\plugins\yt-chat-overlay\bin\64bit');
end;
