; Copyright (c) 2026 Breno Raiher. Licensed under the MIT License.
; Build with BuildWindowsInstaller.ps1 after preparing a Shipping package.

#ifndef PackageRoot
  #error PackageRoot must identify a prepared Windows package.
#endif
#ifndef InstallerOutputDir
  #error InstallerOutputDir must identify the installer output directory.
#endif
#ifndef AppVersion
  #error AppVersion must match the packaged application.
#endif
#ifndef RequiredVCRuntimeVersion
  #error RequiredVCRuntimeVersion must match the bundled prerequisite.
#endif

[Setup]
AppId={{37168EF5-4F3B-448E-A307-45C587C55A88}
AppName=PHAROS
AppVersion={#AppVersion}
AppVerName=PHAROS {#AppVersion}
AppPublisher=PHAROS
AppPublisherURL=https://github.com/BrenoRaiher/PHAROS
AppSupportURL=https://github.com/BrenoRaiher/PHAROS/issues
AppUpdatesURL=https://github.com/BrenoRaiher/PHAROS/releases
VersionInfoVersion={#AppVersion}
VersionInfoCompany=PHAROS
VersionInfoDescription=PHAROS Installer
VersionInfoProductName=PHAROS
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright (c) 2026 Breno Raiher
DefaultDirName={localappdata}\Programs\PHAROS
DefaultGroupName=PHAROS
DisableWelcomePage=no
DisableDirPage=no
DisableProgramGroupPage=yes
UsePreviousAppDir=yes
PrivilegesRequired=lowest
SetupArchitecture=x64
ArchitecturesAllowed=x64compatible and not arm64
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
LicenseFile={#PackageRoot}\EULA.txt
SetupIconFile=..\..\Build\Windows\Application.ico
WizardStyle=modern dynamic
WizardSizePercent=130,150
WizardImageFile=..\..\Content\Splash\Splash.bmp
WizardImageFileDynamicDark=..\..\Content\Splash\Splash.bmp
WizardSmallImageFile=
WizardSmallImageFileDynamicDark=
OutputDir={#InstallerOutputDir}
OutputBaseFilename=PHAROS-{#AppVersion}-Setup
Compression=lzma2/normal
SolidCompression=yes
LZMAUseSeparateProcess=yes
DiskSpanning=no
CloseApplications=yes
RestartApplications=no
Uninstallable=yes
UninstallDisplayName=PHAROS
UninstallDisplayIcon={app}\PHAROS.exe
UninstallFilesDir={app}\Uninstall
SignedUninstaller=no
SetupLogging=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
WelcomeLabel1=Welcome to PHAROS
WelcomeLabel2=PHAROS is a spacecraft dynamics and orbital simulation platform for studying the motion and attitude of articulated spacecraft.%n%nBuild spacecraft models, configure environmental forces and actuators, and develop custom C++ controllers. Explore simulation results through interactive 3D visualization, plots, and data export.%n%nThis wizard will install [name/ver] on your computer.

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
; Store the prerequisite first so checking it never unpacks the full application.
Source: "{#PackageRoot}\Engine\Extras\Redist\en-us\vc_redist.x64.exe"; Flags: dontcopy nocompression
Source: "{#PackageRoot}\*"; DestDir: "{app}"; Excludes: "\README.md"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "InstallerReadme.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\PHAROS\PHAROS"; Filename: "{app}\PHAROS.exe"; WorkingDir: "{app}"
Name: "{autoprograms}\PHAROS\Uninstall PHAROS"; Filename: "{uninstallexe}"
Name: "{autodesktop}\PHAROS"; Filename: "{app}\PHAROS.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\PHAROS.exe"; WorkingDir: "{app}"; Description: "Launch PHAROS"; Flags: nowait postinstall skipifsilent runasoriginaluser; Check: CanLaunchPHAROS

[Code]
var
  RuntimeRestartRequired: Boolean;

procedure LayoutBrandedPage(Image: TBitmapImage; Heading, Body: TNewStaticText);
var
  PageWidth, Margin, ImageHeight: Integer;
begin
  PageWidth := WizardForm.OuterNotebook.ClientWidth;
  Margin := ScaleX(24);
  // Reuse the application artwork without stretching it into a portrait sidebar.
  ImageHeight := (PageWidth * Image.Bitmap.Height) div Image.Bitmap.Width;
  Image.SetBounds(0, 0, PageWidth, ImageHeight);
  Heading.Caption := Trim(Heading.Caption);
  Heading.SetBounds(Margin, ImageHeight + ScaleY(18),
    PageWidth - 2 * Margin, Heading.Height);
  WizardForm.AdjustLabelHeight(Heading);
  Body.SetBounds(Margin, Heading.Top + Heading.Height + ScaleY(12),
    Heading.Width, Body.Height);
  WizardForm.AdjustLabelHeight(Body);
end;

procedure CurPageChanged(CurPageID: Integer);
var
  OptionsTop, ExtraHeight: Integer;
begin
  if CurPageID = wpWelcome then
  begin
    LayoutBrandedPage(WizardForm.WizardBitmapImage,
      WizardForm.WelcomeLabel1, WizardForm.WelcomeLabel2);
    ExtraHeight := WizardForm.WelcomeLabel2.Top + WizardForm.WelcomeLabel2.Height +
      ScaleY(24) - WizardForm.OuterNotebook.ClientHeight;
    if ExtraHeight > 0 then
      WizardForm.ClientHeight := WizardForm.ClientHeight + ExtraHeight;
  end
  else if CurPageID = wpFinished then
  begin
    LayoutBrandedPage(WizardForm.WizardBitmapImage2,
      WizardForm.FinishedHeadingLabel, WizardForm.FinishedLabel);
    OptionsTop := WizardForm.FinishedLabel.Top + WizardForm.FinishedLabel.Height + ScaleY(12);
    WizardForm.RunList.SetBounds(WizardForm.FinishedLabel.Left, OptionsTop,
      WizardForm.FinishedLabel.Width,
      WizardForm.OuterNotebook.ClientHeight - OptionsTop - ScaleY(24));
    WizardForm.YesRadio.SetBounds(WizardForm.FinishedLabel.Left, OptionsTop,
      WizardForm.FinishedLabel.Width, WizardForm.YesRadio.Height);
    WizardForm.NoRadio.SetBounds(WizardForm.FinishedLabel.Left, OptionsTop + ScaleY(22),
      WizardForm.FinishedLabel.Width, WizardForm.NoRadio.Height);
  end;
end;

function HasVCRuntime: Boolean;
var
  Installed: Cardinal;
  VersionText: String;
  InstalledVersion, RequiredVersion: Int64;
begin
  Result := False;
  if not RegQueryDWordValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
    'Installed', Installed) then Exit;
  if Installed <> 1 then Exit;
  if not RegQueryStringValue(HKLM64,
    'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64',
    'Version', VersionText) then Exit;
  if Copy(VersionText, 1, 1) = 'v' then Delete(VersionText, 1, 1);
  if not StrToVersion(VersionText, InstalledVersion) then Exit;
  if not StrToVersion('{#RequiredVCRuntimeVersion}', RequiredVersion) then Exit;
  Result := ComparePackedVersion(InstalledVersion, RequiredVersion) >= 0;
  if Result then Log('A compatible Visual C++ x64 runtime is already installed.');
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ExitCode: Integer;
begin
  Result := '';
  if HasVCRuntime then Exit;

  Log('Installing the bundled Microsoft Visual C++ x64 runtime.');
  ExtractTemporaryFile('vc_redist.x64.exe');
  if not ShellExec('runas', ExpandConstant('{tmp}\vc_redist.x64.exe'),
    '/install /passive /norestart', '', SW_SHOWNORMAL,
    ewWaitUntilTerminated, ExitCode) then
  begin
    Result := 'PHAROS requires the Microsoft Visual C++ x64 runtime. ' +
      'Its installer could not start or administrator permission was declined. ' +
      'Allow the prerequisite installation and try again.';
    Exit;
  end;

  RuntimeRestartRequired := (ExitCode = 3010) or (ExitCode = 1641);
  if (ExitCode <> 0) and not RuntimeRestartRequired then
  begin
    if (ExitCode = 1638) and HasVCRuntime then Exit;
    Result := 'The Microsoft Visual C++ prerequisite could not be installed ' +
      '(error ' + IntToStr(ExitCode) + '). Complete that installation and try again.';
    Exit;
  end;
  if not RuntimeRestartRequired and not HasVCRuntime then
    Result := 'The required Microsoft Visual C++ x64 runtime is still unavailable. ' +
      'Restart Windows and run PHAROS Setup again.';
end;

function NeedRestart: Boolean;
begin
  Result := RuntimeRestartRequired;
end;

function CanLaunchPHAROS: Boolean;
begin
  Result := not RuntimeRestartRequired;
end;

// Intentionally no AppData deletion: saved work belongs to the user.
