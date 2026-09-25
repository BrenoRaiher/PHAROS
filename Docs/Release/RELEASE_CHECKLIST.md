# Release Checklist

## Before Packaging

- Identify the publisher, source commit, version, and changes. Follow the
  [community release guidance](PUBLISHING.md#community-releases).
- Update the [third-party notices](../Legal/THIRD_PARTY_NOTICES.txt),
  [asset provenance](../Legal/ASSET_PROVENANCE.md), and corresponding source
  for any changed dependencies or assets. Preserve applicable licenses.
- Preserve the included SPICE kernels' provider notices, source records,
  filenames, and checksums.
- Build the editor and standalone runner, and install the pinned controller
  compiler with `Build/ControllerToolchain/Install-LLVMMinGW.ps1`.
- Package Windows Shipping with the MainMenu and SolarSystem maps. Exclude
  editor/debug content and Crash Reporter.

## After Packaging

Run from the project root, using the directory containing `PHAROS.exe`:

```powershell
& .\Tools\Release\PrepareWindowsPackage.ps1 -PackageRoot 'D:\Releases\PHAROS\Windows'
& .\Tools\Release\Test-Publication.ps1 -PackageRoot 'D:\Releases\PHAROS\Windows'
```

- Check product/version metadata, legal links, controller compilation,
  persistence, simulation, and playback in the packaged application.
- Complete the [clean-machine test matrix](CLEAN_MACHINE_TEST_MATRIX.md).
- Keep required redistributables, license texts, and the matching Eigen
  source archive. Remove unused build/debug artifacts after checking their
  runtime role.
- Retain `LEGAL_MANIFEST.sha256` with the release record.
- Build the installer using [INSTALLER.md](../../Tools/Release/INSTALLER.md).
  The supplied workflow produces an unsigned installer and SHA-256 sidecar.
  If signing is added, finish executable metadata changes before signing
  and calculate the download checksum after signing.

## Before Publishing

- Follow [PUBLISHING.md](PUBLISHING.md) for source-tree and Git LFS checks.
- Include the required notices and attribution; exclude local saves,
  build products, private records, and credentials from source commits.
- Publish the matching source, package version, checksums, and test record.
  Preserve the original copyright and license notices and identify the
  actual publisher of each distribution.
