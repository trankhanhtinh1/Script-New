$ErrorActionPreference = "Stop"
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "vswhere.exe was not found" }
$msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "MSBuild.exe was not found" }
$project = Join-Path $PSScriptRoot "ZDPredictionIntegrationCompile.vcxproj"
& $msbuild $project /nologo /m:1 /nr:false /p:Configuration=Release /p:Platform=x64 /p:PreferredToolArchitecture=x64
exit $LASTEXITCODE
