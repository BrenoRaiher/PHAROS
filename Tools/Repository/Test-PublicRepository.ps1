[CmdletBinding()]
param([string]$Python = 'python', [switch]$VerifyEvidence)
$ErrorActionPreference = 'Stop'
$arguments = @((Join-Path $PSScriptRoot 'check_repository.py'))
if ($VerifyEvidence) { $arguments += '--verify-evidence' }
& $Python @arguments
if ($LASTEXITCODE -ne 0) { throw 'Repository checks failed.' }
