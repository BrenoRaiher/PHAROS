# Copyright (c) 2026 Breno Raiher. SPDX-License-Identifier: MIT
[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$KernelDirectory = (Join-Path $PSScriptRoot '..\..\Content\SPICEKernels')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$manifestPath = Join-Path $PSScriptRoot 'ExternalKernels.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$directory = [IO.Path]::GetFullPath($KernelDirectory).TrimEnd('\', '/')

foreach ($kernel in $manifest.kernels) {
    if ($kernel.file -notmatch '^[a-zA-Z0-9_-]+\.bsp$' -or
        $kernel.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or $kernel.sizeBytes -le 0) {
        throw 'Invalid external kernel manifest entry.'
    }
    $uri = [Uri]$kernel.url
    if ($uri.Scheme -ne 'https' -or $uri.Host -ne 'ssd.jpl.nasa.gov' -or $uri.UserInfo) {
        throw 'External kernels must be downloaded from the specified official HTTPS provider.'
    }
    $destination = [IO.Path]::GetFullPath((Join-Path $directory $kernel.file))
    if ((Split-Path -Parent $destination) -ne $directory) {
        throw 'Kernel destination is outside the requested directory.'
    }
    if (Test-Path -LiteralPath $destination) {
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf) -or
            (Get-Item -LiteralPath $destination).Length -ne $kernel.sizeBytes -or
            (Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ne $kernel.sha256) {
            throw "Existing $($kernel.file) differs from the pinned kernel. It was not overwritten."
        }
        Write-Host "Verified existing $($kernel.file); nothing was changed."
        continue
    }
    if (-not $PSCmdlet.ShouldProcess($destination, "Download pinned kernel from $uri")) { continue }
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $temporary = $destination + '.' + [Guid]::NewGuid().ToString('N') + '.download'
    try {
        Invoke-WebRequest -Uri $uri -OutFile $temporary -UseBasicParsing -TimeoutSec 300
        if ((Get-Item -LiteralPath $temporary).Length -ne $kernel.sizeBytes -or
            (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $kernel.sha256) {
            throw "Downloaded $($kernel.file) failed verification. No kernel was installed."
        }
        # Move only a verified download; File.Move will not replace a concurrent install.
        if ((Split-Path -Parent ([IO.Path]::GetFullPath($temporary))) -ne $directory) {
            throw 'Temporary kernel path is outside the requested directory.'
        }
        [IO.File]::Move($temporary, $destination)
        Write-Host "Installed and verified $($kernel.file) from NASA/JPL SSD."
    } finally {
        if ((Split-Path -Parent ([IO.Path]::GetFullPath($temporary))) -eq $directory -and
            (Test-Path -LiteralPath $temporary -PathType Leaf)) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}
