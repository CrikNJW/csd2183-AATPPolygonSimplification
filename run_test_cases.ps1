param(
    [string]$SimplifyPath = "",
    [string]$TestCasesDir = "test_cases",
    [switch]$KeepOutputs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-SimplifyPath {
    param([string]$InputPath)

    if ($InputPath) {
        if (-not (Test-Path $InputPath)) {
            throw "Simplify executable not found at: $InputPath"
        }
        return (Resolve-Path $InputPath).Path
    }

    $candidates = @(".\simplify.exe", ".\simplify")
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $resolved = (Resolve-Path $candidate).Path
            if ($IsWindows -and [System.IO.Path]::GetExtension($resolved).ToLowerInvariant() -ne ".exe") {
                continue
            }
            return $resolved
        }
    }

    if ($IsWindows) {
        throw "Could not find a runnable simplify.exe. Build a Windows executable or pass -SimplifyPath."
    }
    throw "Could not find simplify executable. Build first or pass -SimplifyPath."
}

function Get-Metrics {
    param([string]$Path)

    $metrics = @{}
    $patterns = @(
        "Total signed area in input:",
        "Total signed area in output:",
        "Total areal displacement:"
    )

    foreach ($pattern in $patterns) {
        $line = Select-String -Path $Path -Pattern $pattern | Select-Object -First 1
        $metrics[$pattern] = if ($line) { $line.Line.Trim() } else { "" }
    }

    return $metrics
}

$cases = @(
    @{ Input = "input_rectangle_with_two_holes.csv"; Target = 7 },
    @{ Input = "input_cushion_with_hexagonal_hole.csv"; Target = 13 },
    @{ Input = "input_blob_with_two_holes.csv"; Target = 17 },
    @{ Input = "input_wavy_with_three_holes.csv"; Target = 21 },
    @{ Input = "input_lake_with_two_islands.csv"; Target = 17 },
    @{ Input = "input_original_01.csv"; Target = 99 },
    @{ Input = "input_original_02.csv"; Target = 99 },
    @{ Input = "input_original_03.csv"; Target = 99 },
    @{ Input = "input_original_04.csv"; Target = 99 },
    @{ Input = "input_original_05.csv"; Target = 99 },
    @{ Input = "input_original_06.csv"; Target = 99 },
    @{ Input = "input_original_07.csv"; Target = 99 },
    @{ Input = "input_original_08.csv"; Target = 99 },
    @{ Input = "input_original_09.csv"; Target = 99 },
    @{ Input = "input_original_10.csv"; Target = 99 }
)

$simplifyExe = Resolve-SimplifyPath -InputPath $SimplifyPath
$testCasesRoot = (Resolve-Path $TestCasesDir).Path
$tmpDir = Join-Path $PSScriptRoot "tmp_test_outputs"

if (Test-Path $tmpDir) {
    Remove-Item -Path $tmpDir -Recurse -Force
}
New-Item -Path $tmpDir -ItemType Directory | Out-Null

$results = @()

foreach ($case in $cases) {
    $inputPath = Join-Path $testCasesRoot $case.Input
    if (-not (Test-Path $inputPath)) {
        throw "Missing input file: $inputPath"
    }

    $expectedName = $case.Input -replace "^input_", "output_" -replace "\.csv$", ".txt"
    $expectedPath = Join-Path $testCasesRoot $expectedName
    if (-not (Test-Path $expectedPath)) {
        throw "Missing expected output file: $expectedPath"
    }

    $actualPath = Join-Path $tmpDir $expectedName
    try {
        $actualOutput = & $simplifyExe $inputPath $case.Target 2>&1
    }
    catch {
        throw "Failed to execute '$simplifyExe' for '$($case.Input)': $($_.Exception.Message)"
    }

    if ($LASTEXITCODE -ne 0) {
        $stderrText = ($actualOutput | Out-String).Trim()
        throw "Execution failed for '$($case.Input)' with exit code $LASTEXITCODE. Output: $stderrText"
    }

    $actualOutput | Set-Content -Path $actualPath
    if (-not (Test-Path $actualPath)) {
        throw "Failed to write output file: $actualPath"
    }

    $actualLines = Get-Content $actualPath
    $expectedLines = Get-Content $expectedPath
    $diff = Compare-Object -ReferenceObject $expectedLines -DifferenceObject $actualLines
    $exactMatch = ($null -eq $diff)

    $actualMetrics = Get-Metrics -Path $actualPath
    $expectedMetrics = Get-Metrics -Path $expectedPath
    $metricsMatch = $true
    foreach ($k in $expectedMetrics.Keys) {
        if ($expectedMetrics[$k] -ne $actualMetrics[$k]) {
            $metricsMatch = $false
            break
        }
    }

    $results += [PSCustomObject]@{
        Case        = $case.Input
        Target      = $case.Target
        ExactMatch  = $exactMatch
        Metrics     = $metricsMatch
        OutputFile  = $actualPath
    }
}

$results | Sort-Object Case | Format-Table -AutoSize

$failures = @($results | Where-Object { -not $_.ExactMatch })
Write-Host ""
Write-Host ("Summary: {0}/{1} exact matches" -f ($results.Count - $failures.Count), $results.Count)

if ($failures.Count -gt 0) {
    Write-Host "Mismatched cases:"
    $failures | Sort-Object Case | ForEach-Object {
        Write-Host ("- {0}" -f $_.Case)
    }
}

if (-not $KeepOutputs) {
    Remove-Item -Path $tmpDir -Recurse -Force
}
else {
    Write-Host ("Kept generated outputs at: {0}" -f $tmpDir)
}

if ($failures.Count -gt 0) {
    exit 1
}

