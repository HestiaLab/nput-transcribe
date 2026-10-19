<#
.SYNOPSIS
    对一个目录下的所有媒体文件运行 nput-transcribe，写出 SRT 字幕与时序汇总。

.EXAMPLE
    .\run-corpus.ps1 -Model $model -Media "D:\audio\Day-01\media" -Out .\out
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Model,
    [Parameter(Mandatory = $true)][string]$Media,
    [string]$Out = "corpus-out",
    [string]$CacheDir = "$env:USERPROFILE\.cache\ov_whisper",
    [string]$Language = "auto",
    [string]$Exe
)

$ErrorActionPreference = "Stop"

if (-not $Exe) {
    $Exe = Join-Path $PSScriptRoot "..\build\debug\bin\nput-transcribe.exe"
}
if (-not (Test-Path $Exe)) {
    throw "nput-transcribe.exe not found: $Exe"
}
if (-not (Test-Path $Model)) {
    throw "model directory not found: $Model"
}
if (-not (Test-Path $Media)) {
    throw "media directory not found: $Media"
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null

$extensions = @(".wav", ".mp3", ".mp4", ".m4a", ".aac", ".flac", ".wma", ".mov", ".mkv")
$files = Get-ChildItem $Media -File | Where-Object { $extensions -contains $_.Extension.ToLower() } |
    Sort-Object Name
if (-not $files) {
    throw "no supported media files in $Media"
}

$summary = @()
foreach ($file in $files) {
    $target = Join-Path $Out ($file.BaseName + ".srt")
    Write-Host "==> $($file.Name) ($([math]::Round($file.Length / 1MB, 2)) MB)" -ForegroundColor Cyan

    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $output = & $Exe --model $Model --cache-dir $CacheDir -l $Language -f srt -v -o $target $file.FullName 2>&1
    $exit = $LASTEXITCODE
    $timer.Stop()
    $output | Where-Object { $_ -match "片段|解码|重采样" } | ForEach-Object { Write-Host "    $_" }

    $segments = ($output | Select-String -Pattern "片段 \d+/\d+" -AllMatches).Count
    $audioSeconds = 0.0
    $decoded = $output | Select-String -Pattern "解码 \d+ 个采样点，\d+ Hz（([\d\.]+) 秒）" |
        Select-Object -First 1
    if ($decoded) {
        $audioSeconds = [double]$decoded.Matches[0].Groups[1].Value
    }

    $realtime = 0.0
    if ($timer.Elapsed.TotalSeconds -gt 0) {
        $realtime = [math]::Round($audioSeconds / $timer.Elapsed.TotalSeconds, 1)
    }

    $summary += [pscustomobject]@{
        File      = $file.Name
        DurationS = [math]::Round($audioSeconds, 1)
        Segments  = $segments
        WallS     = [math]::Round($timer.Elapsed.TotalSeconds, 2)
        RealtimeX = $realtime
        ExitCode  = $exit
        Subtitle  = Split-Path $target -Leaf
    }
}

$summary | Format-Table -AutoSize | Out-String | Write-Host
$summary | Export-Csv (Join-Path $Out "summary.csv") -NoTypeInformation -Encoding UTF8
Write-Host "wrote $Out\summary.csv" -ForegroundColor Green
