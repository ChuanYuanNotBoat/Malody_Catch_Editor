param(
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-RobocopySync {
    param(
        [Parameter(Mandatory = $true)][string]$SourceDir,
        [Parameter(Mandatory = $true)][string]$TargetDir,
        [switch]$DryRunMode
    )

    if (-not (Test-Path -LiteralPath $TargetDir)) {
        New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
    }

    $args = @(
        $SourceDir,
        $TargetDir,
        "/E",
        "/R:1",
        "/W:1",
        "/XD", "__pycache__", ".pytest_cache", ".mypy_cache",
        "/XF", "*.pyc", "*.pyo"
    )

    if ($DryRunMode) {
        $args += "/L"
    }

    & robocopy @args | Out-Null
    $code = $LASTEXITCODE
    if ($code -gt 7) {
        throw "robocopy failed ($code): $SourceDir -> $TargetDir"
    }
}

$pluginsRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
$repoRoot = Split-Path -Parent $pluginsRoot

$targetPluginDirs = Get-ChildItem -Path $repoRoot -Recurse -Directory -Filter "plugins" |
    Where-Object {
        (Resolve-Path -LiteralPath $_.FullName).Path -ne $pluginsRoot
    } |
    Sort-Object -Property FullName -Unique

if (-not $targetPluginDirs -or $targetPluginDirs.Count -eq 0) {
    Write-Host "No build plugin directories found under: $repoRoot"
    exit 0
}

$sourceTopDirs = Get-ChildItem -LiteralPath $pluginsRoot -Directory
$scriptName = Split-Path -Leaf $PSCommandPath
$sourceTopFiles = Get-ChildItem -LiteralPath $pluginsRoot -File |
    Where-Object { $_.Name -ne $scriptName }

Write-Host "Plugins source: $pluginsRoot"
Write-Host "Targets found: $($targetPluginDirs.Count)"
if ($DryRun) {
    Write-Host "Dry run mode enabled. No files will be changed."
}

foreach ($target in $targetPluginDirs) {
    $targetPath = (Resolve-Path -LiteralPath $target.FullName).Path
    Write-Host ""
    Write-Host "Syncing to: $targetPath"

    foreach ($file in $sourceTopFiles) {
        $dstFile = Join-Path $targetPath $file.Name
        if ($DryRun) {
            Write-Host "  [DRY] file $($file.Name)"
        } else {
            Copy-Item -LiteralPath $file.FullName -Destination $dstFile -Force
            Write-Host "  file $($file.Name)"
        }
    }

    foreach ($dir in $sourceTopDirs) {
        $srcDir = $dir.FullName
        $dstDir = Join-Path $targetPath $dir.Name
        Invoke-RobocopySync -SourceDir $srcDir -TargetDir $dstDir -DryRunMode:$DryRun
        if ($DryRun) {
            Write-Host "  [DRY] dir  $($dir.Name)"
        } else {
            Write-Host "  dir  $($dir.Name)"
        }
    }
}

Write-Host ""
Write-Host "Plugin sync completed."
