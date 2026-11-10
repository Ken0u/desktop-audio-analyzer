. "$PSScriptRoot/environment.ps1"
$source = Join-Path $projectRoot 'build/release/DesktopAudioAnalyzer.exe'
if (-not (Test-Path -LiteralPath $source)) { throw 'Run scripts/build.ps1 first.' }
$destination = Join-Path $projectRoot 'dist/DesktopAudioAnalyzer'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
Copy-Item -LiteralPath $source -Destination $destination
Copy-Item -LiteralPath "$projectRoot/README.md" -Destination $destination
Copy-Item -LiteralPath "$projectRoot/THIRD_PARTY.md" -Destination $destination
& "$qtDirectory/bin/windeployqt.exe" --release --verbose 0 --no-translations --no-system-dxc-compiler --qmldir "$projectRoot/qml" "$destination/DesktopAudioAnalyzer.exe"
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
Copy-Item -LiteralPath "$compilerDirectory/libstdc++-6.dll","$compilerDirectory/libgcc_s_seh-1.dll","$compilerDirectory/libwinpthread-1.dll" -Destination $destination
Write-Output "Runnable build: $destination/DesktopAudioAnalyzer.exe"
