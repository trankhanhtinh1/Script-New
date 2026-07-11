param(
    [ValidateSet('handled-av', 'unhandled-av', 'failfast', 'forced', 'wait')]
    [string]$Scenario = 'handled-av',
    [string]$DumpFolder = ''
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$console = Join-Path $root 'NightSharp\bin\Release\console.exe'
$fixture = Join-Path $root 'NightSharpDebugConsole\bin\Release\NightSharpCrashFixture.exe'

if (-not $DumpFolder) {
    $DumpFolder = Join-Path $env:TEMP ("NightSharpCrashIntegration_" + [guid]::NewGuid().ToString('N'))
}
New-Item -ItemType Directory -Path $DumpFolder -Force | Out-Null

$stdout = Join-Path $DumpFolder 'console.stdout.txt'
$stderr = Join-Path $DumpFolder 'console.stderr.txt'
$monitor = $null
try {
    $monitor = Start-Process -FilePath $console `
        -ArgumentList @('--test-mode', '--no-wer', '--dump-folder', $DumpFolder) `
        -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr `
        -PassThru
    Start-Sleep -Milliseconds 500

    $target = Start-Process -FilePath $fixture -ArgumentList $Scenario -PassThru
    if (-not $target.WaitForExit(22000)) {
        Stop-Process -Id $target.Id -Force -ErrorAction SilentlyContinue
        throw "fixture timed out"
    }
    if ($Scenario -eq 'handled-av' -and $target.ExitCode -ne 0) {
        throw "handled fixture exit code $($target.ExitCode)"
    }

    Start-Sleep -Milliseconds 500
    $dumps = @(Get-ChildItem -LiteralPath $DumpFolder -Filter '*.dmp' -File)
    $reports = @(Get-ChildItem -LiteralPath $DumpFolder -Filter 'nightsharp_crash_*.txt' -File)
    $logs = @(Get-ChildItem -LiteralPath $DumpFolder -Filter 'nightsharp_crash_*.log' -File)
    if ($Scenario -in @('failfast', 'forced', 'wait')) {
        if ($dumps.Count -ne 0 -or $reports.Count -ne 1 -or $logs.Count -ne 1) {
            throw "expected exit-only incident; dumps=$($dumps.Count) reports=$($reports.Count) logs=$($logs.Count)"
        }
        $reportText = Get-Content -Raw $reports[0].FullName
        if ($reportText -notmatch 'classification=forced-termination-no-exception' -or
            $reportText -notmatch 'exception_context=unavailable') {
            throw "forced exit report invented exception evidence"
        }
    } elseif ($dumps.Count -ne 1 -or $reports.Count -ne 1 -or $logs.Count -ne 1) {
        throw "expected one incident set; dumps=$($dumps.Count) reports=$($reports.Count) logs=$($logs.Count)"
    }
    Write-Output "PASS scenario=$Scenario folder=$DumpFolder"
} finally {
    if ($monitor -and -not $monitor.HasExited) {
        Stop-Process -Id $monitor.Id -Force -ErrorAction SilentlyContinue
        $monitor.WaitForExit(3000) | Out-Null
    }
}
