# PHAROS for Windows

Platform for Hierarchical Articulated Rigid-Body and Orbital Simulation.

## Start PHAROS

Open PHAROS from the Start Menu or the optional desktop shortcut. Review and
accept the end-user agreement on first launch. Unreal Editor and Visual Studio
are not required.

## Your Data

Scenarios, results, controllers, preferences, and EULA acceptance are stored
under `%LOCALAPPDATA%\PHAROS\Saved`, separately from the installed application.
The application creates its storage automatically. Back up important work
before upgrading. Uninstalling PHAROS does not delete this saved data.

Install an update by running its installer. To remove the application, use
Windows Settings > Apps > Installed apps > PHAROS > Uninstall, or the
Uninstall PHAROS Start Menu shortcut. The shared Microsoft Visual C++ runtime
is not removed by the PHAROS uninstaller.

## Documentation

- [Getting started](Documentation/README.md)
- [Release notes](Documentation/RELEASE_NOTES.md)
- [Compatibility](Documentation/COMPATIBILITY.md)
- [Known limitations](Documentation/KNOWN_LIMITATIONS.md)
- [TGSCN file format](https://github.com/BrenoRaiher/PHAROS/blob/main/Docs/Reference/TGSCN.md)
- [Controller SDK](PHAROS/Binaries/Win64/ControllerSDK/README.md)
- [Privacy](Documentation/PRIVACY.md)
- [Support](Documentation/SUPPORT.md)
- [Project repository](https://github.com/BrenoRaiher/PHAROS)

## Safety and Licensing

PHAROS is not certified for operational or safety-critical decisions.
Independently validate simulation results. Custom C++ controllers execute as
native code; compile or load only code you trust.

Read `EULA.txt`, `LICENSE.txt`, and `THIRD_PARTY_NOTICES.txt`. The MIT License
covers first-party PHAROS material, not Unreal Engine or every bundled asset
and dependency. Their respective terms accompany this distribution.

This release is unsigned. Windows may display an unknown-publisher or
reputation warning, and managed computers may block unsigned applications.
Download only from the official project release page and verify its SHA-256
checksum. Security policies can also restrict user-built controller DLLs.
