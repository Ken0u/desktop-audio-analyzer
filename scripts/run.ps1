param([switch]$Demo, [ValidateRange(1, 32)][int]$Windows = 1, [string[]]$AppArguments = @())
. "$PSScriptRoot/environment.ps1"
$executable = Join-Path $projectRoot 'build/release/DesktopAudioAnalyzer.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'Run scripts/build.ps1 first.' }
$launchArguments = @('--windows', $Windows)
if ($Demo) { $launchArguments += '--demo' }
$launchArguments += $AppArguments
& $executable @launchArguments
