[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedPackageRoot = [System.IO.Path]::GetFullPath($PackageRoot)
if (-not (Test-Path -LiteralPath $resolvedPackageRoot -PathType Container)) {
    throw "Package root does not exist: $resolvedPackageRoot"
}

$launcherPath = Join-Path $resolvedPackageRoot 'PHAROS.exe'
if (-not (Test-Path -LiteralPath $launcherPath -PathType Leaf)) {
    throw "PHAROS.exe was not found in the package root: $resolvedPackageRoot"
}

$runtimeFiles = @(Get-ChildItem -LiteralPath $resolvedPackageRoot -Recurse -File -Filter 'PHAROS-Win64-Shipping.exe')
if ($runtimeFiles.Count -ne 1) { throw 'Expected exactly one packaged PHAROS Shipping executable.' }
$runtime = $runtimeFiles[0]
$metadata = $runtime.VersionInfo
if ($metadata.ProductName -ne 'PHAROS' -or $metadata.CompanyName -ne 'PHAROS') {
    throw 'Rebuild and package the current project first; runtime metadata is out of date.'
}

# Use the materials staged with this binary, not documents from another build.
$legalSource = Join-Path $runtime.DirectoryName 'Legal'
$releaseSource = Join-Path $runtime.DirectoryName 'Documentation'
$licenseSource = Join-Path $legalSource 'PHAROS_LICENSE.txt'
$readmeSource = Join-Path $releaseSource 'README.md'
$changelogSource = Join-Path $releaseSource 'CHANGELOG.md'

$releaseDocumentNames = @(
    'RELEASE_NOTES.md',
    'COMPATIBILITY.md',
    'KNOWN_LIMITATIONS.md',
    'SUPPORT.md',
    'PRIVACY.md'
)

$requiredSources = @(
    (Join-Path $legalSource 'EULA.txt'),
    (Join-Path $legalSource 'THIRD_PARTY_NOTICES.txt'),
    (Join-Path $legalSource 'Licenses'),
    (Join-Path $legalSource 'Sources\Eigen-3.4.0.zip'),
    $licenseSource,
    $readmeSource,
    $changelogSource
)

$requiredSources += @($releaseDocumentNames | ForEach-Object {
    Join-Path $releaseSource $_
})

foreach ($requiredSource in $requiredSources) {
    if (-not (Test-Path -LiteralPath $requiredSource)) {
        throw "Required legal source is missing: $requiredSource"
    }
}

$launcherMetadata = [Diagnostics.FileVersionInfo]::GetVersionInfo($launcherPath)
if ($launcherMetadata.ProductName -ne $metadata.ProductName -or
    $launcherMetadata.CompanyName -ne $metadata.CompanyName -or
    $launcherMetadata.ProductVersion -ne $metadata.ProductVersion -or
    $launcherMetadata.LegalCopyright -ne $metadata.LegalCopyright -or
    $launcherMetadata.FileDescription -ne $metadata.FileDescription) {
    & (Join-Path $PSScriptRoot 'Set-LauncherMetadata.ps1') -Launcher $launcherPath -Runtime $runtime.FullName
}

Copy-Item -LiteralPath (Join-Path $legalSource 'EULA.txt') `
    -Destination (Join-Path $resolvedPackageRoot 'EULA.txt') -Force
Copy-Item -LiteralPath (Join-Path $legalSource 'THIRD_PARTY_NOTICES.txt') `
    -Destination (Join-Path $resolvedPackageRoot 'THIRD_PARTY_NOTICES.txt') -Force
Copy-Item -LiteralPath $licenseSource `
    -Destination (Join-Path $resolvedPackageRoot 'LICENSE.txt') -Force

$destinationDocumentation = Join-Path $resolvedPackageRoot 'Documentation'
New-Item -ItemType Directory -Force -Path $destinationDocumentation |
    Out-Null
Copy-Item -LiteralPath $readmeSource `
    -Destination (Join-Path $destinationDocumentation 'README.md') -Force
Copy-Item -LiteralPath $changelogSource `
    -Destination (Join-Path $destinationDocumentation 'CHANGELOG.md') -Force
foreach ($releaseDocumentName in $releaseDocumentNames) {
    Copy-Item -LiteralPath (Join-Path $releaseSource $releaseDocumentName) `
        -Destination (Join-Path $destinationDocumentation $releaseDocumentName) `
        -Force
}

$destinationLicenses = Join-Path $resolvedPackageRoot 'Licenses'
New-Item -ItemType Directory -Force -Path $destinationLicenses | Out-Null
Get-ChildItem -LiteralPath (Join-Path $legalSource 'Licenses') -File -Recurse |
    ForEach-Object {
        $relativePath = $_.FullName.Substring(
            (Join-Path $legalSource 'Licenses').Length).TrimStart('\')
        $destinationPath = Join-Path $destinationLicenses $relativePath
        $destinationDirectory = Split-Path -Parent $destinationPath
        New-Item -ItemType Directory -Force -Path $destinationDirectory |
            Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $destinationPath -Force
    }

$destinationSources = Join-Path $resolvedPackageRoot 'Sources'
New-Item -ItemType Directory -Force -Path $destinationSources | Out-Null
Get-ChildItem -LiteralPath (Join-Path $legalSource 'Sources') -File -Recurse |
    ForEach-Object {
        $relativePath = $_.FullName.Substring((Join-Path $legalSource 'Sources').Length).TrimStart('\')
        $destinationPath = Join-Path $destinationSources $relativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destinationPath) | Out-Null
        Copy-Item -LiteralPath $_.FullName -Destination $destinationPath -Force
    }

$manifestFiles = @(
    (Join-Path $resolvedPackageRoot 'EULA.txt'),
    (Join-Path $resolvedPackageRoot 'THIRD_PARTY_NOTICES.txt'),
    (Join-Path $resolvedPackageRoot 'LICENSE.txt')
) + @(Get-ChildItem -LiteralPath $destinationLicenses,$destinationSources -File -Recurse |
    Select-Object -ExpandProperty FullName)

$manifestLines = foreach ($file in ($manifestFiles | Sort-Object)) {
    $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
    $relativePath = $file.Substring($resolvedPackageRoot.Length).TrimStart('\')
    "$hash  $relativePath"
}

$manifestPath = Join-Path $resolvedPackageRoot 'LEGAL_MANIFEST.sha256'
[System.IO.File]::WriteAllLines(
    $manifestPath,
    $manifestLines,
    [System.Text.UTF8Encoding]::new($false))

Write-Host "PHAROS legal materials prepared successfully."
Write-Host "PHAROS release documentation prepared successfully."
Write-Host "Package root: $resolvedPackageRoot"
Write-Host "Manifest: $manifestPath"
& (Join-Path $PSScriptRoot 'Test-Publication.ps1') -PackageRoot $resolvedPackageRoot
