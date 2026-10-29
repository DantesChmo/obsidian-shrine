# Full check: build -> content -> tests. Stops at the first failing stage.
# Writes Saved\Reports\summary.md - the single file to read after a run.

. "$PSScriptRoot\Common.ps1"

$summary = Join-Path $ReportDir 'summary.md'
$stages = @(
    @{ Name = 'Build';   Script = 'Build.ps1';        Report = 'build_errors.txt' },
    @{ Name = 'Content'; Script = 'BuildContent.ps1'; Report = 'content_status.json' },
    @{ Name = 'Tests';   Script = 'RunTests.ps1';     Report = 'tests_summary.txt' }
)

$lines = @("# Verify $(Get-Date -Format 'yyyy-MM-dd HH:mm')", '', "Engine: $EngineRoot", '')
$failed = $false

foreach ($stage in $stages) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot $stage.Script)
    $ok = ($LASTEXITCODE -eq 0)
    $lines += "## $($stage.Name): $(if ($ok) { 'OK' } else { 'FAILED' })"

    $reportPath = Join-Path $ReportDir $stage.Report
    if (-not $ok -and (Test-Path $reportPath)) {
        $lines += '```'
        $lines += Get-Content $reportPath | Select-Object -First 200
        $lines += '```'
    }
    $lines += ''

    if (-not $ok) { $failed = $true; break }
}

$lines | Set-Content -Path $summary -Encoding UTF8
Write-Host ''
Write-Host "Summary: $summary"
if ($failed) { exit 1 }
