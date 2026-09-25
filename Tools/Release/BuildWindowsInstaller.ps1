# Copyright (c) 2026 Breno Raiher. Licensed under the MIT License.
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$CompilerPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$package = (Get-Item -LiteralPath $PackageRoot -ErrorAction Stop).FullName.TrimEnd('\')
$output = [IO.Path]::GetFullPath($OutputDirectory).TrimEnd('\')
if ($output.Equals($package, [StringComparison]::OrdinalIgnoreCase) -or
    $output.StartsWith($package + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Installer output must be outside the packaged application directory.'
}

$required = @(
    'PHAROS.exe',
    'PHAROS\Binaries\Win64\PHAROS-Win64-Shipping.exe',
    'Engine\Extras\Redist\en-us\vc_redist.x64.exe',
    'EULA.txt', 'LICENSE.txt', 'THIRD_PARTY_NOTICES.txt', 'LEGAL_MANIFEST.sha256',
    'Documentation\README.md'
)
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $package $relative) -PathType Leaf)) {
        throw "Required package file is missing: $relative"
    }
}
if (Get-ChildItem -LiteralPath $package -Recurse -Force |
    Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } |
    Select-Object -First 1) {
    throw 'Review and remove package reparse points before building the installer.'
}

& (Join-Path $PSScriptRoot 'Test-Publication.ps1') -PackageRoot $package

if (-not $CompilerPath) {
    $candidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'),
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe')
    )
    $CompilerPath = $candidates | Where-Object {
        Test-Path -LiteralPath $_ -PathType Leaf
    } | Select-Object -First 1
}
if (-not $CompilerPath -or -not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) {
    throw 'Install Inno Setup 7 from https://jrsoftware.org/isdl.php or specify -CompilerPath.'
}

$runtime = Get-Item -LiteralPath (Join-Path $package $required[1])
$version = $runtime.VersionInfo.ProductVersion
$prerequisite = Get-Item -LiteralPath (Join-Path $package $required[2])
$prerequisiteVersion = $prerequisite.VersionInfo.FileVersion
foreach ($value in @($version, $prerequisiteVersion)) {
    if ($value -notmatch '^\d+(\.\d+){1,3}$') {
        throw "Invalid version in a packaged executable: $value"
    }
}
$prerequisiteSignature = Get-AuthenticodeSignature -LiteralPath $prerequisite.FullName
if ($prerequisiteSignature.Status -ne 'Valid' -or
    $prerequisiteSignature.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation(?:,|$)') {
    throw 'The bundled Visual C++ prerequisite does not have a valid Microsoft signature.'
}

$filename = "PHAROS-$version-Setup.exe"
$installer = Join-Path $output $filename
$checksum = "$installer.sha256"
foreach ($path in @($installer, $checksum)) {
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite an existing release: $path. Choose a new output directory."
    }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null

$arguments = @(
    '/Qp',
    "/DPackageRoot=$package",
    "/DInstallerOutputDir=$output",
    "/DAppVersion=$version",
    "/DRequiredVCRuntimeVersion=$prerequisiteVersion",
    (Join-Path $PSScriptRoot 'PHAROS.iss')
)
& $CompilerPath @arguments
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE." }
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw 'Inno Setup did not produce the expected installer.'
}

$hash = (Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText($checksum, "$hash  $filename`n", [Text.Encoding]::ASCII)
Write-Host "Unsigned installer: $installer"
Write-Host "SHA-256 checksum: $checksum"
