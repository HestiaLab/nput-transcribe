<#
.SYNOPSIS
    配置、构建并可选地测试一个 CMake 预设。

.DESCRIPTION
    对 `cmake --preset`、`cmake --build --preset` 与 `ctest --preset`
    的薄封装，支持 debug / release 两个预设。

.EXAMPLE
    .\scripts\build.ps1 -Config Debug

.EXAMPLE
    .\scripts\build.ps1 -Config Release -RunTests
#>
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$preset = $Config.ToLowerInvariant()

Push-Location $root
try {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    if ($RunTests) {
        ctest --preset $preset
        exit $LASTEXITCODE
    }
}
finally {
    Pop-Location
}
