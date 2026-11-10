# Downloads dependencies into .tools; does not install system-wide software.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot/environment.ps1"
Push-Location $projectRoot
try {
    if (-not (Test-Path -LiteralPath $toolPython)) {
        & python -m venv .tools/python
        if ($LASTEXITCODE -ne 0) { throw 'Python 3.10 or newer is required.' }
    }
    & $toolPython -m pip install --disable-pip-version-check -r requirements-dev.txt
    if ($LASTEXITCODE -ne 0) { throw 'Development tool installation failed.' }
    if (-not (Test-Path -LiteralPath "$qtDirectory/bin/qmake.exe")) {
        & $toolPython -m aqt install-qt windows desktop 6.8.3 win64_mingw -O .tools/qt --archives qtbase qtdeclarative qtsvg qttools
        if ($LASTEXITCODE -ne 0) { throw 'Qt SDK download failed.' }
    }
    if (-not (Test-Path -LiteralPath "$compilerDirectory/g++.exe")) {
        & $toolPython -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O .tools/qt
        if ($LASTEXITCODE -ne 0) { throw 'MinGW compiler download failed.' }
    }
    Write-Output 'Project-local Qt 6.8.3 / MinGW 13.1 toolchain is ready.'
} finally { Pop-Location }
