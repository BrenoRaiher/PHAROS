# Copyright (c) 2026 Breno Raiher. SPDX-License-Identifier: MIT
[CmdletBinding()]
param([string]$PackageRoot, [string]$SourceRoot)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if ($PackageRoot -and $SourceRoot) { throw 'Choose a package check or a source check, not both.' }
if ($SourceRoot) { $projectRoot = (Resolve-Path -LiteralPath $SourceRoot).Path }
$problems = [Collections.Generic.List[string]]::new()
$privatePattern = '(?i)(?:\bUsers[\\/]+(?!Public[\\/]|Default[\\/])[^\\/\r\n]+|(?:\.codex|\.vs)[\\/]|(?:LoginId|EpicAccountId)[\t ]*[:=][\t ]*[0-9a-f]{16,}|SecurityToken[\t ]*=[\t ]*[a-zA-Z0-9-]{16,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{30,})'

function Test-PrivateContent([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $buffer = [byte[]]::new(131072)
        $tail = ''
        $wideTail = ''
        while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $text = $tail + [Text.Encoding]::UTF8.GetString($buffer, 0, $count)
            $wide = $wideTail + [Text.Encoding]::Unicode.GetString($buffer, 0, $count)
            if ($text -match $privatePattern -or $wide -match $privatePattern) { return $true }
            $tail = $text.Substring([Math]::Max(0, $text.Length - 512))
            $wideTail = $wide.Substring([Math]::Max(0, $wide.Length - 512))
        }
        return $false
    } finally { $stream.Dispose() }
}

function Test-LfsPointer([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $buffer = [byte[]]::new(80)
        $count = $stream.Read($buffer, 0, $buffer.Length)
        return [Text.Encoding]::ASCII.GetString($buffer, 0, $count).StartsWith('version https://git-lfs.github.com/spec/v1')
    } finally { $stream.Dispose() }
}

if ($PackageRoot) {
    $root = (Resolve-Path -LiteralPath $PackageRoot).Path
    $required = @('PHAROS.exe', 'LICENSE.txt', 'EULA.txt', 'THIRD_PARTY_NOTICES.txt',
        'LEGAL_MANIFEST.sha256', 'Licenses\llvm-mingw_ISC_LICENSE.txt',
        'Licenses\mingw-w64_RUNTIME_NOTICES.txt', 'Licenses\Stellarium_Western_Lines_MIT.txt',
        'Sources\Eigen-3.4.0.zip', 'Documentation\README.md')
    foreach ($name in $required) {
        if (-not (Test-Path -LiteralPath (Join-Path $root $name) -PathType Leaf)) {
            $problems.Add("Missing packaged file: $name")
        }
    }
    $files = @(Get-ChildItem -LiteralPath $root -Recurse -File)
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length).TrimStart('\', '/')
        if ($file.Extension -in @('.pdb','.obj','.ilk','.exp','.dmp','.sav','.log','.suo','.lnk') -or
            $relative -match '(?i)(^|[\\/])(?:\.vs|\.git|Saved|Intermediate|CrashReportClient)([\\/]|\.exe$)' -or
            $file.Name -eq 'CookMetadata.ucookmeta') {
            $problems.Add("Development/private artifact in package: $relative")
        }
        # Compiler and engine third parties contain upstream paths, not PHAROS personal data.
        if (($file.Name -match '^PHAROS.*\.exe$' -or $file.Extension -in @('.ini','.md','.txt','.json','.xml','.target','.modules','.csv','.tgscn')) -and
            $relative -notmatch '(?i)(ControllerToolchain|(?:^|[\\/])Engine[\\/]|(?:^|[\\/])Licenses[\\/])' -and
            (Test-PrivateContent $file.FullName)) {
            $problems.Add("Review possible private content: $relative")
        }
    }
    foreach ($name in @('PHAROS-Win64-Shipping.exe','PHAROSScenarioRunner.exe','x86_64-w64-mingw32-clang++.exe')) {
        if (-not ($files | Where-Object Name -eq $name)) { $problems.Add("Missing runtime dependency: $name") }
    }
    $launcher = Join-Path $root 'PHAROS.exe'
    if (Test-Path -LiteralPath $launcher) {
        $version = [Diagnostics.FileVersionInfo]::GetVersionInfo($launcher)
        if ($version.ProductName -ne 'PHAROS' -or $version.CompanyName -ne 'PHAROS') {
            $problems.Add('Launcher metadata is not PHAROS metadata. Run PrepareWindowsPackage.ps1 before signing.')
        }
    }
    $manifest = Join-Path $root 'LEGAL_MANIFEST.sha256'
    if (Test-Path -LiteralPath $manifest) {
        foreach ($line in Get-Content -LiteralPath $manifest) {
            if ($line -notmatch '^([a-f0-9]{64})  (.+)$') { $problems.Add('Malformed legal manifest'); continue }
            $expected = $Matches[1]
            $relative = $Matches[2]
            $path = [IO.Path]::GetFullPath((Join-Path $root $relative))
            if (-not $path.StartsWith($root.TrimEnd('\','/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                $problems.Add('Manifest contains a path outside the package'); continue
            }
            if (-not (Test-Path -LiteralPath $path -PathType Leaf) -or
                (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $expected) {
                $problems.Add("Legal manifest mismatch: $relative")
            }
        }
    }
} else {
    $root = $projectRoot
    $lfsReady = $false
    try {
        $lfsVersion = @(& git lfs version 2>$null)
        $lfsReady = $LASTEXITCODE -eq 0 -and ($lfsVersion -join "`n") -match '^git-lfs/\d+\.\d+\.\d+'
    } catch { $lfsReady = $false }
    if (-not $lfsReady) {
        throw 'Git LFS is required for source publication. Install it from https://git-lfs.com/, reopen the terminal, and run git lfs install before staging assets.'
    }
    $temporaryGit = Join-Path ([IO.Path]::GetTempPath()) ('PHAROS-publication-' + [Guid]::NewGuid().ToString('N'))
    try {
        & git init --bare --quiet $temporaryGit
        if ($LASTEXITCODE -ne 0) { throw 'Git is required for the publication check.' }
        $candidates = @(& git --git-dir=$temporaryGit --work-tree=$root -C $root ls-files --others --exclude-standard)
        if ($LASTEXITCODE -ne 0) { throw 'Could not evaluate source publication rules.' }
        & git -C $root rev-parse --show-toplevel 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) { $candidates += @(& git -C $root ls-files) }
        $candidates = @($candidates | Sort-Object -Unique)
        $required = @('LICENSE', 'README.md', 'Docs/GettingStarted/Build.md', 'CITATION.cff', '.gitignore', '.gitattributes',
            'Docs/Legal/ASSET_PROVENANCE.md', 'Docs/Legal/README.md',
            'Docs/Legal/THIRD_PARTY_NOTICES.txt', 'Docs/Legal/Licenses/PHAROS_MIT_LICENSE.txt',
            'Docs/Legal/Licenses/Stellarium_Western_Lines_MIT.txt', 'Docs/Legal/Sources/Eigen-3.4.0.zip',
            'Tools/Release/ExternalKernels.json', 'Tools/Release/InstallExternalKernels.ps1',
            'Content/SPICEKernels/ura111.bsp')
        foreach ($relative in $required) {
            if ($relative -notin $candidates -or -not (Test-Path -LiteralPath (Join-Path $root $relative) -PathType Leaf)) {
                $problems.Add("Missing source-publication file: $relative")
            }
        }
        if (Test-Path -LiteralPath (Join-Path $root 'Content/SPICEKernels/ExternalKernels.json')) {
            $problems.Add('The external-kernel setup manifest must remain in Tools/Release, outside Unreal Content auto-import.')
        }
        $license = Join-Path $root 'LICENSE'
        $licenseCopy = Join-Path $root 'Docs/Legal/Licenses/PHAROS_MIT_LICENSE.txt'
        if ((Test-Path -LiteralPath $license) -and (Test-Path -LiteralPath $licenseCopy)) {
            $originalText = (Get-Content -LiteralPath $license -Raw).Replace("`r`n", "`n").Trim()
            $copiedText = (Get-Content -LiteralPath $licenseCopy -Raw).Replace("`r`n", "`n").Trim()
            if ($originalText -ne $copiedText) { $problems.Add('The packaged MIT license differs from the root LICENSE.') }
        }
        $binaryExtensions = @('.exe','.dll','.pdf','.uasset','.umap','.bsp','.bc','.bpc','.lib','.png','.jpg','.jpeg','.tga','.bmp','.ico','.stl','.zip','.mp4')
        foreach ($relative in $candidates) {
            if (($relative -notmatch '^Validation/.*\.log$') -and ($relative -match '(?i)(^|/)(Saved|Intermediate|Binaries|DerivedDataCache|\.vs|\.git|Backup|Reports|PrivatePermissions)/|\.(?:pdb|obj|dmp|sav|log|lnk|pfx|p12|key|pem|download)$|^Build/ControllerToolchain/Win64/|^Docs/Legal/Private/' -or
                ([IO.Path]::GetFileName($relative) -match '^\.env(?:\.|$)' -and [IO.Path]::GetFileName($relative) -ne '.env.example'))) {
                $problems.Add("Excluded file would be published: $relative")
            }
            $path = Join-Path $root $relative
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { continue }
            $file = Get-Item -LiteralPath $path -Force
            if ($file.Extension -in $binaryExtensions -or $file.Length -ge 100MB) {
                $attribute = @(& git --git-dir=$temporaryGit --work-tree=$root -C $root check-attr filter -- $relative)
                if ($LASTEXITCODE -ne 0 -or $attribute.Count -ne 1 -or -not $attribute[0].EndsWith(': filter: lfs')) {
                    $problems.Add("Binary/large file is not configured for Git LFS: $relative")
                }
                if (Test-LfsPointer $path) {
                    $problems.Add("LFS content is missing locally; run git lfs pull: $relative")
                }
            }
            # Frozen executables and numeric solution histories retain original bytes/hashes.
            # Export text metadata is screened; provenance integrity is checked by
            # Tools/Repository/Test-PublicRepository.ps1 -VerifyEvidence.
            if ($relative -match '^Validation/.*(?:\.exe|\.dll|_solution\.csv)$|^Validation/.*(?:SPICEKernels|kernels)/.*\.(bsp|bpc|bc|tpc|tf|tls)$') { continue }
            # Original third-party credits/build paths are not personal PHAROS metadata.
            if ($relative -match '^Source/ThirdParty/|^Docs/Legal/(Licenses|Sources)/|^Content/SPICEKernels/.*\.(bsp|tpc|tf|tls)$') { continue }
            if ($relative -in @('Tools/Release/Test-Publication.ps1', '.gitignore')) { continue }
            if (Test-PrivateContent $path) { $problems.Add("Review possible private content: $relative") }
        }
        Write-Host ("Checked {0} source-publication candidates." -f $candidates.Count)
    } finally {
        $tempParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
        if ((Split-Path -Parent ([IO.Path]::GetFullPath($temporaryGit))) -eq $tempParent -and
            (Test-Path -LiteralPath $temporaryGit)) {
            Remove-Item -LiteralPath $temporaryGit -Recurse -Force
        }
    }
}

if ($problems.Count) {
    $problems | ForEach-Object { Write-Warning $_ }
    throw ("Publication check found {0} issue(s)." -f $problems.Count)
}
Write-Host 'Publication file checks passed. Legal review and manual release checks remain separate.'
