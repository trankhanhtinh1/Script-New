<#
.SYNOPSIS
    Extract all champion .bin files from WAD and convert to JSON.
    Pipeline: wad → extract .bin → ritobin → .json
#>
$ErrorActionPreference = 'Stop'

$WADTOOLS = "C:\Users\MR THINH\Desktop\Script-New\bin\wadtools.exe"
$RITOBIN = "C:\Users\MR THINH\Desktop\Script-New\bin\ritobin_cli.exe"
$HASHDIR = "C:\Users\MR THINH\Desktop\Script-New\bin\hashes"
$WAD_DIR = "E:\Riot Games\League of Legends\Game\DATA\FINAL\Champions"
$OUT_DIR = "E:\DamageData\Database"
$TMP_DIR = Join-Path $OUT_DIR "_tmp_bin"

# Create output directories
New-Item -ItemType Directory -Force -Path $OUT_DIR | Out-Null
New-Item -ItemType Directory -Force -Path $TMP_DIR | Out-Null

# Get all champion WAD files (exclude vi_VN localized versions)
$wads = Get-ChildItem $WAD_DIR -Filter "*.wad.client" | Where-Object { $_.Name -notmatch "vi_VN" }
Write-Host "Found $($wads.Count) champion WAD files"

$success = 0
$failed = 0
$skipped = 0

foreach ($wad in $wads) {
    $champName = $wad.BaseName -replace '\.wad$', ''
    $jsonPath = Join-Path $OUT_DIR "$champName.json"

    # Skip if already exists
    if (Test-Path $jsonPath) {
        $skipped++
        Write-Host "[SKIP] $champName (already exists)"
        continue
    }

    # Extract .bin from WAD (only the main champion bin file)
    $extractDir = Join-Path $TMP_DIR $champName
    if (Test-Path $extractDir) { Remove-Item $extractDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $extractDir | Out-Null

    # Pattern: data/characters/<champname>/<champname>.bin
    $pattern = "data/characters/$($champName.ToLower())/$($champName.ToLower())\.bin$"

    try {
        & $WADTOOLS --hashtable-dir $HASHDIR extract `
            -i $wad.FullName `
            -o $extractDir `
            -f "bin" `
            -x $pattern `
            --overwrite --stats=false 2>&1 | Out-Null

        # Find extracted .bin file
        $binFile = Get-ChildItem $extractDir -Recurse -Filter "*.bin" | Select-Object -First 1
        if (-not $binFile) {
            Write-Host "[FAIL] $champName - no .bin found"
            $failed++
            continue
        }

        # Convert .bin → .json using ritobin
        & $RITOBIN $binFile.FullName $jsonPath -i bin -o json -d $HASHDIR 2>&1 | Out-Null

        if (Test-Path $jsonPath) {
            $size = (Get-Item $jsonPath).Length
            Write-Host "[OK]   $champName ($size bytes)"
            $success++
        } else {
            Write-Host "[FAIL] $champName - ritobin failed"
            $failed++
        }

        # Cleanup tmp
        Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    } catch {
        Write-Host "[FAIL] $champName - $_"
        $failed++
        Remove-Item $extractDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Cleanup tmp directory
Remove-Item $TMP_DIR -Recurse -Force -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "=== Summary ==="
Write-Host "Success: $success"
Write-Host "Skipped: $skipped"
Write-Host "Failed:  $failed"
Write-Host "Total:   $($wads.Count)"
