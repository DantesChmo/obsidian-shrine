# Compiles the project. Output: Saved\Reports\build.log and build_errors.txt
param(
    [string]$Target = 'ObsidianShrineEditor',
    [string]$Configuration = 'Development'
)

. "$PSScriptRoot\Common.ps1"

$log    = Join-Path $ReportDir 'build.log'
$errors = Join-Path $ReportDir 'build_errors.txt'

Write-Step "Build $Target $Configuration (engine: $EngineRoot)"

& $BuildBat $Target Win64 $Configuration "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE 2>&1 |
    Tee-Object -FilePath $log
$exitCode = $LASTEXITCODE

Select-String -Path $log -Pattern ': (fatal )?error ', 'Error:', 'LogCompile: Error' |
    ForEach-Object { $_.Line } |
    Set-Content -Path $errors

if ($exitCode -ne 0) {
    Write-Host "Build FAILED (exit $exitCode). Errors: $errors" -ForegroundColor Red
    exit $exitCode
}
Write-Host 'Build OK' -ForegroundColor Green
