# Runs automation tests headless. Output: Saved\Reports\tests.log, Saved\Reports\Automation\index.json, tests_summary.txt
param(
    [string]$Filter = 'ObsidianShrine'
)

. "$PSScriptRoot\Common.ps1"

$log       = Join-Path $ReportDir 'tests.log'
$exportDir = Join-Path $ReportDir 'Automation'
$summary   = Join-Path $ReportDir 'tests_summary.txt'

Remove-Item $exportDir -Recurse -ErrorAction SilentlyContinue

Write-Step "Run tests: $Filter"

& $EditorCmd $ProjectFile "-ExecCmds=Automation RunTests $Filter" '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$exportDir" -unattended -nullrhi -nosplash -nop4 -stdout "-abslog=$log" | Out-Null

$index = Join-Path $exportDir 'index.json'
if (-not (Test-Path $index)) {
    Write-Host "Tests FAILED: no report produced. See $log" -ForegroundColor Red
    exit 1
}

$report = Get-Content $index -Raw | ConvertFrom-Json
$lines = @("passed: $($report.succeeded)  warnings: $($report.succeededWithWarnings)  failed: $($report.failed)  not run: $($report.notRun)")
foreach ($test in $report.tests) {
    $lines += "[$($test.state)] $($test.fullTestPath)"
    foreach ($entry in $test.entries) {
        if ($entry.event.type -ne 'Info') {
            $lines += "    $($entry.event.type): $($entry.event.message)"
        }
    }
}
$lines | Set-Content -Path $summary
$lines | ForEach-Object { Write-Host $_ }

if ($report.failed -gt 0 -or @($report.tests).Count -eq 0) { exit 1 }
Write-Host 'Tests OK' -ForegroundColor Green
