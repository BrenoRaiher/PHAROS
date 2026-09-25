# Windows Installer

The installer is built with Inno Setup 7 and is deliberately unsigned.
Install Inno Setup from <https://jrsoftware.org/isdl.php>. No signing
certificate or additional commercial packaging tool is required.

## Build

First package PHAROS Shipping in Unreal, then prepare and inspect that package
with the existing release tools. Use the prepared `Windows` directory, not
the source project, as installer input:

```powershell
.\Tools\Release\BuildWindowsInstaller.ps1 `
    -PackageRoot 'C:\Path\To\Windows' `
    -OutputDirectory 'C:\Path\To\Release'
```

The command produces `PHAROS-<version>-Setup.exe` and its `.sha256` sidecar.
Distribute the single EXE to end users; the checksum is an optional companion
download. Existing release artifacts are never overwritten. The version is
read from the packaged executable, and the package itself is not modified.

## Behavior

- Current-user installation, defaulting to `%LOCALAPPDATA%\Programs\PHAROS`.
- Native x64 Windows; Windows 11 is the currently tested platform.
- Branded welcome page with a short product introduction, EULA page, location
  choice, Start Menu shortcuts, and optional desktop shortcut. The welcome and
  completion pages reuse `Content/Splash/Splash.bmp` as a proportional banner.
- Automatic detection of the bundled Visual C++ x64 runtime version or newer.
  If missing, the bundled Microsoft installer requests administrator approval.
  Failed prerequisite installation stops PHAROS installation; restart requests
  are carried through to the finished page.
- An Installed Apps entry and uninstaller are included automatically.
- The fixed AppId identifies upgrades. Do not change it between releases.
- Uninstallation removes installed files and shortcuts, not
  `%LOCALAPPDATA%\PHAROS\Saved` or the shared Visual C++ runtime.
- A fresh PHAROS user-data folder starts with an empty scenario library.
  Reinstalling does not reset an existing library, and startup does not import
  saves from other applications or development installations.
- The installed root README contains installation instructions instead of the
  portable ZIP instructions. Runtime binaries, content, and legal files are
  taken unchanged from the prepared package.

## Release Checks

Test installation, launching, simulation, controller compilation, upgrading,
and uninstallation on a clean Windows 11 computer without Unreal or Visual
Studio. Check the missing-prerequisite path as well as a machine where the
runtime is already installed. Confirm saved work survives uninstallation.

An unsigned installer can trigger Windows reputation warnings. Do not disable
Windows security settings in the installer. This package does not include a
signing key, automatic certificate installation, or antivirus exclusions.
