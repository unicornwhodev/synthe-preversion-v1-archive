param(
    [string]$Configuration = "Release",
    [string]$BuildDir = "build",
    [string]$IsccPath = "",
    [string]$OutputDir = "dist",
    [string]$AppVersion = "",
    [switch]$BuildFirst
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repoRoot = Split-Path -Parent $PSCommandPath
$projectDir = Get-ChildItem -LiteralPath $repoRoot -Directory |
    Where-Object {
        $_.Name -ne "JUCE" -and (Test-Path (Join-Path $_.FullName "CMakeLists.txt"))
    } |
    Sort-Object Name |
    Select-Object -First 1

if (-not $projectDir) {
    throw "No top-level project directory with CMakeLists.txt was found under $repoRoot."
}

$projectCMake = Join-Path $projectDir.FullName "CMakeLists.txt"
$projectMatch = Select-String -Path $projectCMake -Pattern '^\s*project\(([^ )]+)\s+VERSION\s+([^ )]+)' | Select-Object -First 1
if (-not $projectMatch) {
    throw "Unable to detect project name and version from $projectCMake."
}

$pluginTarget = $projectMatch.Matches[0].Groups[1].Value
$appVersion = if ($AppVersion) { $AppVersion } else { $projectMatch.Matches[0].Groups[2].Value }

$productMatch = Select-String -Path $projectCMake -Pattern 'PRODUCT_NAME\s+"([^"]+)"' | Select-Object -First 1
if (-not $productMatch) {
    throw "Unable to detect PRODUCT_NAME from $projectCMake."
}

$productName = $productMatch.Matches[0].Groups[1].Value
$publisherMatch = Select-String -Path $projectCMake -Pattern 'COMPANY_NAME\s+"([^"]+)"' | Select-Object -First 1
$publisherName = if ($publisherMatch) { $publisherMatch.Matches[0].Groups[1].Value } else { "Musique" }

if ($BuildFirst) {
    & (Join-Path $repoRoot "_build_all.ps1") -Configuration $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed while preparing installer artefacts."
    }
}

$artefactsDir = Join-Path $repoRoot "$BuildDir\${pluginTarget}_artefacts\$Configuration"
$standaloneDir = Join-Path $artefactsDir "Standalone"
$vst3RootDir = Join-Path $artefactsDir "VST3"

$standaloneExe = Join-Path $standaloneDir "$productName.exe"
if (-not (Test-Path $standaloneExe)) {
    $standaloneCandidate = Get-ChildItem -Path $standaloneDir -File -Filter *.exe -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $standaloneCandidate) {
        throw "No standalone executable found in $standaloneDir. Build the standalone target first."
    }
    $standaloneExe = $standaloneCandidate.FullName
}

$vst3Bundle = Join-Path $vst3RootDir "$productName.vst3"
if (-not (Test-Path $vst3Bundle)) {
    $vst3Candidate = Get-ChildItem -Path $vst3RootDir -Directory -Filter *.vst3 -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $vst3Candidate) {
        throw "No VST3 bundle found in $vst3RootDir. Build the VST3 target first."
    }
    $vst3Bundle = $vst3Candidate.FullName
}

function Resolve-IsccPath {
    param([string]$PreferredPath)

    if ($PreferredPath) {
        if (Test-Path $PreferredPath) {
            return (Resolve-Path $PreferredPath).Path
        }
        throw "Specified ISCC.exe path does not exist: $PreferredPath"
    }

    $candidates = @(
        'D:\InnoSetup6\ISCC.exe',
        'D:\Programs\Inno Setup 6\ISCC.exe',
        'D:\Programs\Antigravity\resources\app\node_modules\innosetup\bin\ISCC.exe',
        (Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
    ) | Where-Object { $_ }

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw 'ISCC.exe was not found. Install Inno Setup or pass -IsccPath explicitly.'
}

$resolvedIsccPath = Resolve-IsccPath -PreferredPath $IsccPath
$resolvedOutputDir = Join-Path $repoRoot $OutputDir
if (-not (Test-Path $resolvedOutputDir)) {
    New-Item -ItemType Directory -Path $resolvedOutputDir | Out-Null
}

$scriptPath = Join-Path $repoRoot 'UWdeVST_Rare_InnoSetup.iss'
$outputBaseFilename = "${productName}_${appVersion}_Windows_x64_Setup"
$standaloneExeName = Split-Path -Leaf $standaloneExe
$vst3DirName = Split-Path -Leaf $vst3Bundle

$isccArgs = @(
    "/DMyAppName=$productName",
    "/DMyAppVersion=$appVersion",
    "/DMyAppPublisher=$publisherName",
    "/DStandaloneSource=$standaloneExe",
    "/DStandaloneExeName=$standaloneExeName",
    "/DVst3Source=$vst3Bundle",
    "/DVst3DirName=$vst3DirName",
    "/DOutputDir=$resolvedOutputDir",
    "/DOutputBaseFilename=$outputBaseFilename",
    $scriptPath
)

Write-Host "Using ISCC: $resolvedIsccPath"
Write-Host "Standalone: $standaloneExe"
Write-Host "VST3: $vst3Bundle"
Write-Host "Output: $resolvedOutputDir"

& $resolvedIsccPath @isccArgs
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup compilation failed."
}

Write-Host "Installer created: $(Join-Path $resolvedOutputDir ($outputBaseFilename + '.exe'))"
