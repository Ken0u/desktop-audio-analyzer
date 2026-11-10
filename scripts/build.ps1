param([switch]$Test)
. "$PSScriptRoot/environment.ps1"
if (-not (Test-Path -LiteralPath $toolCmake)) { throw 'Run scripts/setup.ps1 first.' }
Push-Location $projectRoot
try {
    & $toolCmake --preset windows-local
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $toolCmake --build --preset windows-local
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    if ($Test) {
        & $toolCtest --preset windows-local
        if ($LASTEXITCODE -ne 0) { throw 'DSP tests failed.' }
    }
} finally { Pop-Location }
