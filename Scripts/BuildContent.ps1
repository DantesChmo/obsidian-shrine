# Runs the Python content pipeline (Content\Python\build_content.py) in the editor.
# Output: Saved\Reports\content.log and content_status.json

. "$PSScriptRoot\Common.ps1"

$script = Join-Path $ProjectRoot 'Content\Python\build_content.py'
$log    = Join-Path $ReportDir 'content.log'
$status = Join-Path $ReportDir 'content_status.json'

Remove-Item $status -ErrorAction SilentlyContinue

Write-Step 'Build content'

& $EditorCmd $ProjectFile "-ExecutePythonScript=$script" -unattended -nosplash -nop4 -stdout -FullStdOutLogOutput "-abslog=$log" | Out-Null

if (-not (Test-Path $status)) {
    Write-Host "Content build FAILED: no status file, the script did not finish. See $log" -ForegroundColor Red
    exit 1
}

$result = Get-Content $status -Raw | ConvertFrom-Json
foreach ($builder in $result.builders) {
    if ($builder.ok) {
        Write-Host "  [ok]   $($builder.name)" -ForegroundColor Green
    } else {
        Write-Host "  [FAIL] $($builder.name)" -ForegroundColor Red
        Write-Host $builder.error
    }
}

if (-not $result.ok) { exit 1 }
Write-Host 'Content OK' -ForegroundColor Green
