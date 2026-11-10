$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolPython = Join-Path $projectRoot '.tools/python/Scripts/python.exe'
$toolCmake = Join-Path $projectRoot '.tools/python/Scripts/cmake.exe'
$toolCtest = Join-Path $projectRoot '.tools/python/Scripts/ctest.exe'
$qtDirectory = Join-Path $projectRoot '.tools/qt/6.8.3/mingw_64'
$compilerDirectory = Join-Path $projectRoot '.tools/qt/Tools/mingw1310_64/bin'
$env:PATH = "$compilerDirectory;$qtDirectory/bin;$projectRoot/.tools/python/Scripts;$env:PATH"
