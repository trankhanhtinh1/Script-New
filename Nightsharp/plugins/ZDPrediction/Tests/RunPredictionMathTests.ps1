$ErrorActionPreference = "Stop"

$testDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $testDirectory "PredictionMathTests.cpp"
$binary = Join-Path $testDirectory "PredictionMathTests.exe"

$compiler = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($null -eq $compiler) {
    throw "cl.exe was not found. Run this script from a Visual Studio Developer PowerShell prompt."
}

& $compiler.Source /nologo /std:c++17 /EHsc /W4 /permissive- /I $testDirectory /Fe:$binary $source
if ($LASTEXITCODE -ne 0) {
    throw "Prediction math test compilation failed with exit code $LASTEXITCODE."
}

& $binary
if ($LASTEXITCODE -ne 0) {
    throw "Prediction math tests failed with exit code $LASTEXITCODE."
}
