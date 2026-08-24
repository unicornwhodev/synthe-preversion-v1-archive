param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$JuceDir = "",
    [switch]$BootstrapJuce,
    [switch]$SkipBuild,
    [string]$InnoSetupPath = "",
    [string]$AppVersion = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-AppVersion {
    param([string]$RepoRoot, [string]$ExplicitVersion)
    if ($ExplicitVersion) { return $ExplicitVersion }
    $cmakeFile = Join-Path $RepoRoot "synth-perc\CMakeLists.txt"
    $cmakeText = Get-Content -LiteralPath $cmakeFile -Raw
    $versionMatch = [regex]::Match($cmakeText, 'project\([^\s\)]+\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})')
    if ($versionMatch.Success) { return $versionMatch.Groups[1].Value }
    throw "Unable to detect the application version from $cmakeFile."
}

function Resolve-InnoCompiler {
    param([string]$ExplicitPath)
    $candidates = @()
    if ($ExplicitPath) { $candidates += $ExplicitPath }
    $pathCommand = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
    if ($pathCommand) { $candidates += $pathCommand.Source }
    $candidates += @(
        "D:\InnoSetup6\ISCC.exe",
        "D:\Programs\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    )
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) { return (Resolve-Path -LiteralPath $candidate).Path }
    }
    throw "Inno Setup compiler ISCC.exe was not found. Pass -InnoSetupPath with the full path to ISCC.exe."
}

$repoRoot = Split-Path -Parent $PSCommandPath
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$installerDir = Join-Path $repoRoot "installer"
$issPath = Join-Path $installerDir "UWdeVST_Perc.iss"
$resolvedVersion = Resolve-AppVersion -RepoRoot $repoRoot -ExplicitVersion $AppVersion

if (-not (Test-Path -LiteralPath $issPath)) { throw "Inno Setup script not found at $issPath." }

if (-not $SkipBuild) {
    $buildArgs = @{ Configuration = $Configuration; BuildDir = $BuildDir }
    if ($JuceDir) { $buildArgs["JuceDir"] = $JuceDir }
    if ($BootstrapJuce) { $buildArgs["BootstrapJuce"] = $true }
    & (Join-Path $repoRoot "_build_all.ps1") @buildArgs
    if ($LASTEXITCODE -ne 0) { throw "Build failed. Installer generation stopped." }
}

$artifactRoot = Join-Path $buildPath "UWdeVST_perc_artefacts\$Configuration"
$standaloneExe = Join-Path $artifactRoot "Standalone\uwdevst_perc.exe"
$vst3Bundle = Join-Path $artifactRoot "VST3\uwdevst_perc.vst3"
if (-not (Test-Path -LiteralPath $standaloneExe -PathType Leaf)) { throw "Standalone executable not found at $standaloneExe." }
if (-not (Test-Path -LiteralPath $vst3Bundle -PathType Container)) { throw "VST3 bundle not found at $vst3Bundle." }

$stageDir = Join-Path $installerDir "staging"
$standaloneStageDir = Join-Path $stageDir "Standalone"
$vst3StageDir = Join-Path $stageDir "VST3"
if (Test-Path -LiteralPath $stageDir) { Remove-Item -LiteralPath $stageDir -Recurse -Force }
New-Item -ItemType Directory -Path $standaloneStageDir -Force | Out-Null
New-Item -ItemType Directory -Path $vst3StageDir -Force | Out-Null
Copy-Item -LiteralPath $standaloneExe -Destination $standaloneStageDir -Force
Copy-Item -LiteralPath $vst3Bundle -Destination $vst3StageDir -Recurse -Force

$iscc = Resolve-InnoCompiler -ExplicitPath $InnoSetupPath
& $iscc "/DAppVersion=$resolvedVersion" $issPath
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed with exit code $LASTEXITCODE." }

$setupPath = Join-Path $installerDir "output\uwdevst_perc_${resolvedVersion}_Windows_x64_Setup.exe"
if (-not (Test-Path -LiteralPath $setupPath)) { throw "Expected installer was not created: $setupPath" }
$sizeMb = [math]::Round((Get-Item -LiteralPath $setupPath).Length / 1MB, 2)
Write-Host "Installer created: $setupPath ($sizeMb MB)"
