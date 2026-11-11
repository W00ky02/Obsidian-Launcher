#ifndef PackageDir
  #define PackageDir "..\..\Windows"
#endif
#ifndef InstallerOutput
  #define InstallerOutput "..\.."
#endif

[Setup]
AppId={{6D964CAB-6450-45F6-A095-31CC146B7D17}
AppName=Obsidian Launcher
AppVersion=2.3
AppPublisher=Obsidian
DefaultDirName={autopf}\Obsidian Launcher
DefaultGroupName=Obsidian Launcher
DisableDirPage=auto
UsePreviousAppDir=yes
UsePreviousTasks=yes
UsePreviousPrivileges=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir={#InstallerOutput}
OutputBaseFilename=Obsidian-v2.3-setup
SetupIconFile={#PackageDir}\obsidian.ico
UninstallDisplayIcon={app}\obsidian.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Desktop-Verknüpfung erstellen"; GroupDescription: "Zusätzliche Verknüpfungen:"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Obsidian Launcher"; Filename: "{app}\ObsidianLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\obsidian.ico"
Name: "{autodesktop}\Obsidian Launcher"; Filename: "{app}\ObsidianLauncher.exe"; WorkingDir: "{app}"; IconFilename: "{app}\obsidian.ico"; Tasks: desktopicon

[Run]
Filename: "{app}\ObsidianLauncher.exe"; Description: "Obsidian Launcher starten"; Flags: nowait postinstall skipifsilent unchecked

[Code]
const
  UninstallKey = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{6D964CAB-6450-45F6-A095-31CC146B7D17}_is1';
var
  PreviousDirectory: String;

function ExistingDirectory: String;
begin
  Result := '';
  if IsAdminInstallMode then
    RegQueryStringValue(HKLM64, UninstallKey, 'Inno Setup: App Path', Result)
  else
    RegQueryStringValue(HKCU64, UninstallKey, 'Inno Setup: App Path', Result);
end;

procedure InitializeWizard;
begin
  PreviousDirectory := ExistingDirectory;
  if PreviousDirectory <> '' then begin
    WizardForm.DirEdit.Text := PreviousDirectory;
    if ActiveLanguage = 'german' then begin
      WizardForm.WelcomeLabel1.Caption := 'Obsidian Launcher aktualisieren';
      WizardForm.WelcomeLabel2.Caption := 'Die vorhandene Installation wird auf Version 2.3 aktualisiert.' + #13#10 + #13#10 +
        'Installationsordner und Verknüpfungen bleiben erhalten. Deine Minecraft-Dateien werden nicht verändert.';
    end else begin
      WizardForm.WelcomeLabel1.Caption := 'Update Obsidian Launcher';
      WizardForm.WelcomeLabel2.Caption := 'Your existing installation will be updated to version 2.3.' + #13#10 + #13#10 +
        'The installation folder and shortcuts are preserved. Your Minecraft files are not changed.';
    end;
  end;
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := (PreviousDirectory <> '') and ((PageID = wpSelectDir) or (PageID = wpSelectProgramGroup) or (PageID = wpSelectTasks));
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  // Also enforce the registered location during silent updates or a /DIR override.
  if PreviousDirectory <> '' then WizardForm.DirEdit.Text := PreviousDirectory;
  Result := '';
end;
