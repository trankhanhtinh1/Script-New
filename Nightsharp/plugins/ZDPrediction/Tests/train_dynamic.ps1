$ErrorActionPreference = "Stop"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe was not found" }
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe was not found" }
$project = Join-Path $PSScriptRoot "DynamicPredictionTrainer.vcxproj"
& $msbuild $project /nologo /m /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$executable = Join-Path $PSScriptRoot "bin\Release\DynamicPredictionTrainer.exe"
$report = Join-Path $PSScriptRoot "DynamicTrainingReport.txt"
$output = & $executable
$exitCode = $LASTEXITCODE
$output | Write-Output
$output | Set-Content -Path $report -Encoding UTF8
exit $exitCode
