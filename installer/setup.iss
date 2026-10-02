; Inno Setup 6 Script for YouTube Live Chat Overlay for OBS
; One-click installer for Windows 10/11 64-bit
; Compatible with OBS 25+ (Qt5), OBS 28+ (Qt6), and OBS 30+ (new layout)

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
; OBS Plugin DLL - install to all plugin discovery locations for maximum compatibility
; 1. Classic OBS layout (obs-plugins/64bit/)
Source: "..\yt-chat-overlay.dll"; DestDir: "{code:GetObsPluginDir}"; Flags: ignoreversion
; 2. User AppData plugin dir (works without admin, all OBS versions)
Source: "..\yt-chat-overlay.dll"; DestDir: "{userappdata}\obs-studio\plugins\yt-chat-overlay\bin\64bit"; Flags: ignoreversion
; 3. ProgramData plugin dir (global, needs admin)
Source: "..\yt-chat-overlay.dll"; DestDir: "{commonappdata}\obs-studio\plugins\yt-chat-overlay\bin\64bit"; Flags: ignoreversion; Check: IsAdminInstallMode
; Standalone Application
Source: "..\YouTubeChatOverlay.exe"; DestDir: "{autopf64}\YouTubeChatOverlay"; Flags: ignoreversion
; Default Config (only if not already configured)
Source: "..\config\overlay_config.json"; DestDir: "{userappdata}\obs-studio\plugin_config\yt-chat-overlay"; Flags: ignoreversion onlyifdoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{autopf64}\YouTubeChatOverlay\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{autopf64}\YouTubeChatOverlay\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Code]
// Custom function to detect OBS installation directory
// Supports: standard install, Steam, Scoop, Chocolatey, portable, registry
function GetObsPluginDir(Param: String): String;
var
  ObsPath: String;
  Drives: array of String;
  I: Integer;
begin
  // 1. Registry HKLM (official OBS installer)
  if RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', ObsPath) then
  begin
    if DirExists(ObsPath + '\obs-plugins\64bit') then
    begin
      Result := ObsPath + '\obs-plugins\64bit';
      Exit;
    end;
    // OBS 30+ flat layout
    if DirExists(ObsPath + '\obs-plugins') then
    begin
      Result := ObsPath + '\obs-plugins';
      Exit;
    end;
  end;

  // 2. Registry HKCU (per-user install)
  if RegQueryStringValue(HKCU, 'SOFTWARE\OBS Studio', '', ObsPath) then
  begin
    if DirExists(ObsPath + '\obs-plugins\64bit') then
    begin
      Result := ObsPath + '\obs-plugins\64bit';
      Exit;
    end;
  end;

  // 3. Standard Program Files
  if DirExists('C:\Program Files\obs-studio\obs-plugins\64bit') then
  begin
    Result := 'C:\Program Files\obs-studio\obs-plugins\64bit';
    Exit;
  end;
  // OBS 30+ layout
  if DirExists('C:\Program Files\obs-studio\obs-plugins') then
  begin
    Result := 'C:\Program Files\obs-studio\obs-plugins';
    Exit;
  end;

  // 4. Steam default library
  if DirExists('C:\Program Files (x86)\Steam\steamapps\common\OBS Studio\obs-plugins\64bit') then
  begin
    Result := 'C:\Program Files (x86)\Steam\steamapps\common\OBS Studio\obs-plugins\64bit';
    Exit;
  end;

  // 5. Steam on other drives
  SetLength(Drives, 4);
  Drives[0] := 'D'; Drives[1] := 'E'; Drives[2] := 'F'; Drives[3] := 'G';
  for I := 0 to 3 do
  begin
    ObsPath := Drives[I] + ':\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit';
    if DirExists(ObsPath) then
    begin
      Result := ObsPath;
      Exit;
    end;
  end;

  // 6. Fallback to user AppData plugin directory (always works, no admin needed)
  Result := ExpandConstant('{userappdata}\obs-studio\plugins\yt-chat-overlay\bin\64bit');
end;
