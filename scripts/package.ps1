<#
.SYNOPSIS
    构建并打包 Windows x64 便携版。

.DESCRIPTION
    生成 build/dist/nput-transcribe-v<版本>-windows-x64.zip，包含：
    nput-transcribe.exe、OpenVINO 运行时 DLL、LICENSE、THIRD_PARTY_NOTICES.md、
    LICENSES/Apache-2.0.txt、README.md 与 SHA256SUMS.txt。

.EXAMPLE
    .\scripts\package.ps1

.EXAMPLE
    .\scripts\package.ps1 -VerifyDevices

.EXAMPLE
    .\scripts\package.ps1 -SkipBuild -OutDir D:\release
#>
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$OutDir,
    [switch]$SkipBuild,
    [switch]$VerifyDevices
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$preset = $Config.ToLowerInvariant()
$buildDir = Join-Path $root "build\$preset"
$binDir = Join-Path $buildDir "bin"

if (-not $SkipBuild) {
    Write-Host "==> 配置 $preset"
    & cmake --preset $preset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --preset $preset 失败"
    }
    Write-Host "==> 构建 $preset"
    & cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) {
        throw "cmake --build --preset $preset 失败"
    }
}

$exe = Join-Path $binDir "nput-transcribe.exe"
if (-not (Test-Path $exe)) {
    throw "未找到 $exe，请先构建（去掉 -SkipBuild）"
}

$dlls = @(Get-ChildItem $binDir -File -Filter *.dll)
if ($dlls.Count -eq 0) {
    throw "$binDir 中没有 OpenVINO 运行时 DLL"
}

$cache = Join-Path $buildDir "CMakeCache.txt"
$versionMatch = Select-String -Path $cache -Pattern '^CMAKE_PROJECT_VERSION:STATIC=(.+)$' |
    Select-Object -First 1
if (-not $versionMatch) {
    throw "无法从 $cache 读取项目版本"
}
$version = $versionMatch.Matches[0].Groups[1].Value.Trim()

if (-not $OutDir) {
    $OutDir = Join-Path $root "build\dist"
}
New-Item -ItemType Directory -Force $OutDir | Out-Null

$suffix = if ($Config -eq "Release") { "" } else { "-$preset" }
$packageName = "nput-transcribe-v$version-windows-x64$suffix"
$stageDir = Join-Path $OutDir $packageName
$zipPath = Join-Path $OutDir "$packageName.zip"
if (Test-Path $stageDir) {
    Remove-Item -Recurse -Force $stageDir
}
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

New-Item -ItemType Directory -Force $stageDir | Out-Null
Copy-Item $exe $stageDir
$dlls | ForEach-Object { Copy-Item $_.FullName $stageDir }
Copy-Item (Join-Path $root "LICENSE") $stageDir
Copy-Item (Join-Path $root "README.md") $stageDir
Copy-Item (Join-Path $root "THIRD_PARTY_NOTICES.md") $stageDir
$licenseDir = Join-Path $stageDir "LICENSES"
New-Item -ItemType Directory -Force $licenseDir | Out-Null
Copy-Item (Join-Path $root "LICENSES\Apache-2.0.txt") $licenseDir

$openvinoDll = Join-Path $binDir "openvino.dll"
if (Test-Path $openvinoDll) {
    $dllVersion = (Get-Item $openvinoDll).VersionInfo.FileVersion
    if ($dllVersion) {
        $noticePath = Join-Path $stageDir "THIRD_PARTY_NOTICES.md"
        $notice = [System.IO.File]::ReadAllText($noticePath, [System.Text.Encoding]::UTF8)
        [System.IO.File]::WriteAllText(
            $noticePath,
            $notice + "`n---`n`n打包信息：OpenVINO 运行时版本 $dllVersion。`n",
            (New-Object System.Text.UTF8Encoding($false)))
    }
}

$sums = Get-ChildItem $stageDir -Recurse -File |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($stageDir.Length + 1).Replace('\', '/')
        "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $relative
    }
[System.IO.File]::WriteAllLines(
    (Join-Path $stageDir "SHA256SUMS.txt"),
    $sums,
    (New-Object System.Text.UTF8Encoding($false)))

$stagedExe = Join-Path $stageDir "nput-transcribe.exe"
& $stagedExe --version
if ($LASTEXITCODE -ne 0) {
    throw "打包后的 nput-transcribe.exe 无法运行"
}
if ($VerifyDevices) {
    & $stagedExe --list-devices
    if ($LASTEXITCODE -ne 0) {
        throw "打包后的 nput-transcribe.exe 无法加载 OpenVINO 运行时"
    }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory(
    $stageDir,
    $zipPath,
    [System.IO.Compression.CompressionLevel]::Optimal,
    $true)

$zip = Get-Item $zipPath
$sha256 = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLower()
$fileCount = (Get-ChildItem $stageDir -Recurse -File).Count

Write-Host ""
Write-Host "包:     $zipPath"
Write-Host "大小:   $([math]::Round($zip.Length / 1MB, 1)) MB"
Write-Host "SHA256: $sha256"
Write-Host "文件数: $fileCount"
