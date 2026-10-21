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
if (-not (Test-Path -LiteralPath $Exe)) {
    throw "nput-transcribe.exe not found: $Exe"
}
if (-not (Test-Path -LiteralPath $Model)) {
    throw "model directory not found: $Model"
}

$outPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Out)
[System.IO.Directory]::CreateDirectory($outPath) | Out-Null

$extensions = @(".wav", ".mp3", ".mp4", ".m4a", ".aac", ".flac", ".wma", ".mov", ".mkv")
$summary = @()
$summaryColumns = @("File", "DurationS", "Segments", "WallS", "RealtimeX", "ExitCode", "Subtitle")
$failures = @()
# 目标名包含完整源文件名；Ordinal 避免大小写敏感文件系统把 A.wav.srt / a.wav.srt 误判为重名。
$targetNames = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
$maxTokens = 224
$previousOutputEncoding = [Console]::OutputEncoding
$nativePreferenceVariable = Get-Variable -Name PSNativeCommandUseErrorActionPreference `
    -ErrorAction SilentlyContinue

if (-not (Test-Path -LiteralPath $Media)) {
    $files = @()
    $failures += "媒体目录不存在: $Media"
} else {
    $files = Get-ChildItem -LiteralPath $Media -File |
        Where-Object { $extensions -contains $_.Extension.ToLower() } |
        Sort-Object Name
    if (-not $files) {
        $files = @()
        $failures += "目录中没有受支持的媒体文件: $Media"
    }
}
foreach ($file in $files) {
    $targetName = $file.Name + ".srt"
    if (-not $targetNames.Add($targetName)) {
        $failures += "$($file.Name): 目标字幕重名 $targetName，已跳过以免覆盖"
        continue
    }
    $target = Join-Path $outPath $targetName
    Write-Host "==> $($file.Name) ($([math]::Round($file.Length / 1MB, 2)) MB)" -ForegroundColor Cyan

    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $output = @()
    $exit = $null
    $invokeError = $null
    $previousNativePreference = $null
    if ($nativePreferenceVariable) {
        $previousNativePreference = $nativePreferenceVariable.Value
        $PSNativeCommandUseErrorActionPreference = $false
    }
    try {
        [Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
        $global:LASTEXITCODE = $null
        $output = @(& $Exe --model $Model --cache-dir $CacheDir -l $Language -f srt -v -o $target $file.FullName 2>&1)
    } catch {
        $invokeError = $_.Exception.Message
    } finally {
        $exit = $global:LASTEXITCODE
        [Console]::OutputEncoding = $previousOutputEncoding
        if ($nativePreferenceVariable) {
            $PSNativeCommandUseErrorActionPreference = $previousNativePreference
        }
    }
    $timer.Stop()

    $lines = @($output | ForEach-Object { [string]$_ })
    $lines | Where-Object { $_ -match "片段|解码|重采样" } | ForEach-Object { Write-Host "    $_" }

    $segments = @($lines | Select-String -Pattern "片段 \d+/\d+" -AllMatches |
        ForEach-Object { $_.Matches }).Count
    $tokenMatches = @($lines | Select-String -Pattern "（(\d+) 个 token）" -AllMatches |
        ForEach-Object { $_.Matches })
    $tokens = @($tokenMatches | ForEach-Object {
        $value = 0
        if ([int]::TryParse($_.Groups[1].Value, [ref]$value)) { $value }
    })
    $audioSeconds = 0.0
    $decoded = @($lines | Select-String -Pattern "解码 \d+ 个采样点，\d+ Hz（([\d\.]+) 秒）" -AllMatches |
        ForEach-Object { $_.Matches } | Select-Object -First 1)
    if ($decoded.Count -gt 0) {
        $parsed = 0.0
        if ([double]::TryParse($decoded[0].Groups[1].Value,
                [System.Globalization.NumberStyles]::Float,
                [System.Globalization.CultureInfo]::InvariantCulture, [ref]$parsed)) {
            $audioSeconds = $parsed
        }
    }

    $realtime = 0.0
    if ($timer.Elapsed.TotalSeconds -gt 0) {
        $realtime = [math]::Round($audioSeconds / $timer.Elapsed.TotalSeconds, 1)
    }

    if ($null -eq $exit) {
        $reason = if ($invokeError) { $invokeError } else { "未获得退出码" }
        $failures += "$($file.Name): 启动失败：$reason"
    } else {
        if ($invokeError) {
            $failures += "$($file.Name): 调用异常：$invokeError"
        }
        if ($exit -ne 0) {
            $failures += "$($file.Name): 退出码 $exit"
        }
        if ($realtime -lt 1.5) {
            $failures += "$($file.Name): 实时倍率 $realtime 低于 1.5"
        }
        $exhausted = @($tokens | Where-Object { $_ -ge $maxTokens })
        if ($exhausted.Count -gt 0) {
            $failures += "$($file.Name): $($exhausted.Count) 个片段达到 $maxTokens token 上限"
        }
    }

    $summary += [pscustomobject]@{
        File      = $file.Name
        DurationS = [math]::Round($audioSeconds, 1)
        Segments  = $segments
        WallS     = [math]::Round($timer.Elapsed.TotalSeconds, 2)
        RealtimeX = $realtime
        ExitCode  = $exit
        Subtitle  = $targetName
    }
}

$summary | Format-Table -AutoSize | Out-String | Write-Host
$summaryPath = Join-Path $outPath "summary.csv"
if ($summary.Count -eq 0) {
    $header = ($summaryColumns | ForEach-Object { '"' + $_ + '"' }) -join ","
    Set-Content -LiteralPath $summaryPath -Value $header -Encoding UTF8
} else {
    $summary | Export-Csv -LiteralPath $summaryPath -NoTypeInformation -Encoding UTF8
}
Write-Host "wrote $Out\summary.csv" -ForegroundColor Green
if ($failures.Count -gt 0) {
    Write-Host "语料回归失败：" -ForegroundColor Red
    $failures | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    exit 1
}
