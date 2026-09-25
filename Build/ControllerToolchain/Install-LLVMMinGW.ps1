[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$version = '20260616'
$archiveName = "llvm-mingw-$version-ucrt-x86_64.zip"
$downloadUrl =
    "https://github.com/mstorsjo/llvm-mingw/releases/download/$version/$archiveName"
$expectedSha256 =
    'b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35'

$toolchainParent = [IO.Path]::GetFullPath($PSScriptRoot)
$destination = [IO.Path]::GetFullPath(
    (Join-Path $toolchainParent 'Win64'))
$compiler = Join-Path $destination 'bin\x86_64-w64-mingw32-clang++.exe'

function Restore-LicenseNotices {
    $licenseSource = Join-Path $toolchainParent '..\..\Docs\Legal\Licenses'
    $noticeDirectory = Join-Path $destination 'share\pharos-notices'
    New-Item -ItemType Directory -Path $noticeDirectory -Force | Out-Null
    foreach ($name in @(
        'llvm-mingw_ISC_LICENSE.txt', 'LLVM_Toolchain_LICENSE.txt',
        'mingw-w64_COPYING.txt', 'mingw-w64_NOTICES.txt',
        'mingw-w64_RUNTIME_NOTICES.txt', 'mingw-w64_LGPL-2.1.txt',
        'winpthreads_LICENSE.txt')) {
        Copy-Item -LiteralPath (Join-Path $licenseSource $name) `
            -Destination (Join-Path $noticeDirectory $name) -Force
    }
}

if ((Test-Path -LiteralPath $compiler) -and -not $Force)
{
    Restore-LicenseNotices
    Write-Host "LLVM-MinGW is already installed at: $destination"
    exit 0
}

$temporaryRoot = Join-Path `
    ([IO.Path]::GetTempPath()) `
    ("PHAROS-ControllerToolchain-" + [Guid]::NewGuid().ToString('N'))
$archivePath = Join-Path $temporaryRoot $archiveName
$extractRoot = Join-Path $temporaryRoot 'Extracted'

try
{
    New-Item -ItemType Directory -Path $temporaryRoot | Out-Null

    Write-Host "Downloading official LLVM-MinGW $version..."
    Invoke-WebRequest -Uri $downloadUrl -OutFile $archivePath

    $actualSha256 =
        (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    if ($actualSha256 -ne $expectedSha256)
    {
        throw "Archive SHA-256 mismatch. Expected $expectedSha256, got $actualSha256."
    }

    Write-Host 'Checksum verified. Extracting toolchain...'
    Expand-Archive -LiteralPath $archivePath -DestinationPath $extractRoot

    $extractedDirectory = Join-Path `
        $extractRoot `
        "llvm-mingw-$version-ucrt-x86_64"
    $extractedCompiler = Join-Path `
        $extractedDirectory `
        'bin\x86_64-w64-mingw32-clang++.exe'

    if (-not (Test-Path -LiteralPath $extractedCompiler))
    {
        throw 'The verified archive did not contain the expected compiler.'
    }

    if (Test-Path -LiteralPath $destination)
    {
        $destinationParent =
            [IO.Path]::GetFullPath((Split-Path -Parent $destination))

        if ($destinationParent -ne $toolchainParent)
        {
            throw 'Refusing to replace a toolchain directory outside the expected parent.'
        }

        Remove-Item -LiteralPath $destination -Recurse -Force
    }

    if ((Split-Path -Parent ([IO.Path]::GetFullPath($extractedDirectory))) -ne [IO.Path]::GetFullPath($extractRoot) -or
        (Split-Path -Parent $destination) -ne $toolchainParent) {
        throw 'Refusing to move a toolchain outside the expected extraction and installation directories.'
    }
    Move-Item -LiteralPath $extractedDirectory -Destination $destination

    # The upstream package targets several Windows architectures and includes
    # debugger/Python tooling. PHAROS only needs the x86-64 C++ frontend, linker,
    # headers, target libraries, and license notices for controller DLLs.
    $keptTopLevelDirectories = @(
        'bin',
        'include',
        'lib',
        'share',
        'x86_64-w64-mingw32')

    foreach ($directory in (Get-ChildItem -LiteralPath $destination -Directory |
        Where-Object { $_.Name -notin $keptTopLevelDirectories })) {
        if ($directory.Parent.FullName -ne $destination) {
            throw 'Refusing to prune outside the installed toolchain.'
        }
        Remove-Item -LiteralPath $directory.FullName -Recurse -Force
    }

    $keptBinFiles = @(
        'x86_64-w64-mingw32-clang++.exe',
        'clang-22.exe',
        'libLLVM-22.dll',
        'libclang-cpp.dll',
        'libc++.dll',
        'libunwind.dll',
        'ld.lld.exe',
        'x86_64-w64-windows-gnu.cfg',
        'mingw32-common.cfg')

    Get-ChildItem -LiteralPath (Join-Path $destination 'bin') -File |
        Where-Object { $_.Name -notin $keptBinFiles } |
        Remove-Item -Force

    $unneededPaths = @(
        (Join-Path $destination 'lib\clang\22\lib\linux'),
        (Join-Path $destination 'lib\libear'),
        (Join-Path $destination 'lib\libscanbuild'),
        (Join-Path $destination 'x86_64-w64-mingw32\bin'))

    foreach ($unneededPath in $unneededPaths)
    {
        if (Test-Path -LiteralPath $unneededPath)
        {
            $resolvedUnneededPath = [IO.Path]::GetFullPath($unneededPath)

            if (-not $resolvedUnneededPath.StartsWith(
                    ($destination + [IO.Path]::DirectorySeparatorChar),
                    [StringComparison]::OrdinalIgnoreCase))
            {
                throw "Refusing to prune outside the installed toolchain: $resolvedUnneededPath"
            }

            Remove-Item -LiteralPath $resolvedUnneededPath -Recurse -Force
        }
    }

    Restore-LicenseNotices
    Write-Host "LLVM-MinGW installed at: $destination"
    & $compiler --version | Select-Object -First 1
}
finally
{
    $resolvedTemporaryRoot = [IO.Path]::GetFullPath($temporaryRoot)
    $systemTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())

    if ($resolvedTemporaryRoot.StartsWith(
            $systemTemporaryRoot,
            [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $resolvedTemporaryRoot))
    {
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
