param([switch]$Capture)
. "$PSScriptRoot/environment.ps1"
Push-Location $projectRoot
try {
    $arguments = @('scripts/smoke.py')
    if ($Capture) { $arguments += '--capture' }
    & $toolPython @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Application smoke test failed.' }
} finally { Pop-Location }
