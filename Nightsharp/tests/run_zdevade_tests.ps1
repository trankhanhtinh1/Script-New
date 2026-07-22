$ErrorActionPreference = "Stop"

$testsDir = $PSScriptRoot
$nightsharpRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $nightsharpRoot "build\tests"

$RequiredSuites = @(
    @{
        Source = "zdevade_routing_policy_test.cpp"
        Name = "ZDEVADE ROUTING POLICY"
    },
    @{
        Source = "zdevade_geometry_runtime_test.cpp"
        Name = "ZDEVADE GEOMETRY RUNTIME"
    },
    @{
        Source = "zdevade_controller_policy_test.cpp"
        Name = "ZDEVADE CONTROLLER POLICY"
    },
    @{
        Source = "zdevade_detector_policy_test.cpp"
        Name = "ZDEVADE DETECTOR POLICY"
    }
)

$CompilerFlags = @(
    "/nologo",
    "/std:c++20",
    "/EHsc",
    "/W4",
    "/DNOMINMAX",
    "/I", "."
)

function Write-RunnerFailure {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [Parameter(Mandatory = $true)][int]$ExitCode
    )

    Write-Host "ZDEVADE TEST RUNNER FAILED: $Message"
    exit $ExitCode
}

$compiler = Get-Command cl.exe -ErrorAction SilentlyContinue
if ($null -eq $compiler) {
    Write-RunnerFailure -Message "cl.exe was not found. Initialize a Visual Studio 18 Developer environment first." -ExitCode 1
}

if (-not (Test-Path -LiteralPath $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$RunnerArtifactExtensions = @(
    ".exe",
    ".obj",
    ".compile.log"
)
foreach ($suite in $RequiredSuites) {
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($suite.Source)
    foreach ($extension in $RunnerArtifactExtensions) {
        $artifact = Join-Path $buildDir "$baseName$extension"
        if (Test-Path -LiteralPath $artifact -PathType Leaf) {
            Remove-Item -LiteralPath $artifact -Force
        }
    }
}

Write-Host "ZDEvade test runner: $($RequiredSuites.Count) required suites"
foreach ($suite in $RequiredSuites) {
    Write-Host "  - $($suite.Name) ($($suite.Source))"
}

Push-Location $nightsharpRoot
try {
    foreach ($suite in $RequiredSuites) {
        $sourcePath = Join-Path $testsDir $suite.Source
        if (-not (Test-Path -LiteralPath $sourcePath)) {
            Write-RunnerFailure -Message "Required ZDEvade test source is missing: $($suite.Source)" -ExitCode 1
        }

        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($suite.Source)
        $binary = Join-Path $buildDir "$baseName.exe"
        $objectFile = Join-Path $buildDir "$baseName.obj"
        $compileLog = Join-Path $buildDir "$baseName.compile.log"

        Write-Host "Compiling $($suite.Name) ..."
        $compileOutput = & cl.exe `
            @CompilerFlags `
            "/Fe:$binary" `
            "/Fo:$objectFile" `
            $sourcePath `
            2>&1
        $compileOutput | Out-File -FilePath $compileLog -Encoding ASCII
        if ($LASTEXITCODE -ne 0) {
            Write-Host $compileOutput
            Write-RunnerFailure -Message "Compilation failed for $($suite.Source) with exit code $LASTEXITCODE." -ExitCode $LASTEXITCODE
        }

        Write-Host "Running $($suite.Name) ..."
        $runOutput = & $binary 2>&1
        $runOutput | ForEach-Object { Write-Host $_ }
        if ($LASTEXITCODE -ne 0) {
            Write-RunnerFailure -Message "Test execution failed for $($suite.Name) with exit code $LASTEXITCODE." -ExitCode $LASTEXITCODE
        }

        $passMarker = "ALL $($suite.Name) TESTS PASSED"
        $matchedPass = $false
        foreach ($line in $runOutput) {
            if ($line -eq $passMarker) {
                $matchedPass = $true
                break
            }
        }
        if (-not $matchedPass) {
            Write-RunnerFailure -Message "Suite $($suite.Name) exited 0 but did not print '$passMarker'." -ExitCode 1
        }

        Write-Host "PASSED: $($suite.Name)"
    }

    $strayArtifacts = Get-ChildItem -Path $nightsharpRoot -File -ErrorAction SilentlyContinue |
        Where-Object {
            $_.DirectoryName -eq $nightsharpRoot -and
            $_.Name -like "zdevade_*_test.*"
        }
    if ($null -ne $strayArtifacts -and $strayArtifacts.Count -gt 0) {
        $names = ($strayArtifacts | ForEach-Object { $_.Name }) -join ", "
        Write-RunnerFailure -Message "Standalone test artifacts leaked outside build\tests: $names" -ExitCode 1
    }

    Write-Host "ZDEVADE TEST RUNNER PASSED: all $($RequiredSuites.Count) suites green"
    exit 0
}
finally {
    Pop-Location
}
