# Shared helpers for the Windows build scripts. Dot-source it: . "$PSScriptRoot\Common.ps1"
# Compatible with Windows PowerShell 5.1.

$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot 'ObsidianShrine.uproject'
$ReportDir   = Join-Path $ProjectRoot 'Saved\Reports'

New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

function Get-EngineRoot {
    # Explicit override wins, e.g. $env:UE_ROOT = 'D:\Epic\UE_5.7'
    if ($env:UE_ROOT) { return $env:UE_ROOT }

    $association = (Get-Content $ProjectFile -Raw | ConvertFrom-Json).EngineAssociation

    # Launcher installs
    $launcherKey = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$association"
    if (Test-Path $launcherKey) {
        return (Get-ItemProperty $launcherKey).InstalledDirectory
    }

    # Source builds register a GUID under HKCU
    $buildsKey = 'HKCU:\SOFTWARE\Epic Games\Unreal Engine\Builds'
    if (Test-Path $buildsKey) {
        $path = (Get-ItemProperty $buildsKey).$association
        if ($path) { return $path }
    }

    # Fallback: newest installed 5.x
    $newest = Get-ChildItem 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -like '5.*' } |
        Sort-Object { [version]$_.PSChildName } -Descending |
        Select-Object -First 1
    if ($newest) {
        Write-Warning "Engine $association not found, using $($newest.PSChildName). Update EngineAssociation in ObsidianShrine.uproject."
        return (Get-ItemProperty $newest.PSPath).InstalledDirectory
    }

    throw "Unreal Engine $association not found. Install it via Epic Games Launcher or set `$env:UE_ROOT."
}

$EngineRoot = Get-EngineRoot
$BuildBat   = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$EditorCmd  = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

function Write-Step([string]$Message) {
    Write-Host ''
    Write-Host "=== $Message ===" -ForegroundColor Cyan
}
