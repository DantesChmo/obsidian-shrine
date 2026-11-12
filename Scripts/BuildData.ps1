# Validates the YAML manifests in Game\ and checks that the generated JSON is up to date, then runs the
# data tools' own tests. Needs no build: uses the Python bundled with the engine.
# Output: Saved\Reports\data.log
param(
    # Rewrite the generated files instead of only checking them.
    [switch]$Write
)

. "$PSScriptRoot\Common.ps1"

$log = Join-Path $ReportDir 'data.log'
$tool = Join-Path $ProjectRoot 'Tools\build_data.py'
$tests = Join-Path $ProjectRoot 'Tools\tests'

if (-not (Test-Path $EnginePython)) {
    "Engine Python not found: $EnginePython" | Set-Content -Path $log
    Write-Host "Data FAILED: engine Python not found at $EnginePython" -ForegroundColor Red
    exit 1
}

Write-Step 'Build data'

# Python reports problems on stdout and through the exit code; do not let stderr text abort the script.
$ErrorActionPreference = 'Continue'

if ($Write) {
    & $EnginePython -B $tool 2>&1 | Tee-Object -FilePath $log
} else {
    & $EnginePython -B $tool --check 2>&1 | Tee-Object -FilePath $log
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "Data FAILED. See $log" -ForegroundColor Red
    exit 1
}

& $EnginePython -B -m unittest discover -s $tests 2>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) {
    Write-Host "Data tool tests FAILED. See $log" -ForegroundColor Red
    exit 1
}
Write-Host 'Data OK' -ForegroundColor Green
