<#
.SYNOPSIS
OUYA-Grafik abfragen (Stufe O2 aus port/docs/ouya-machbarkeit.md): was die GPU der OUYA
(Tegra 3, nur OpenGL ES 2.0) wirklich kann, fuer den GLES-2-Renderer in port/ouya.

.DESCRIPTION
Baut in WSL mit dem NDK r23c das kleine Programm port/ouya/tools/gles2_info.cpp fuer
Android 4.1 (armeabi-v7a), kopiert es unter der gemeinsamen ADB-Sperre
(Global\Retroid_ADB_Exclusive, port/docs/adb-coordination.md) nach
/data/local/tmp/re4-ouya/gles2_info, startet es per adb shell und legt die Ausgabe in
-OutDir\gpu-info.txt ab (Standard: Desktop\RE4-OUYA-Probe). Sie nennt Grenzwerte (Uniforms,
Varyings, Attribute), die Rechengenauigkeit im Fragment-Shader, welche Tiefenpuffer gehen,
die Erweiterungen und ob Texturkoordinaten bei vielen Wiederholungen genau bleiben.

Das NDK r23c richtet ouya-probe.ps1 ein (whole_setup_linux.sh --ouya); dieses Skript setzt es
voraus. Die OUYA wird am Modellnamen erkannt, sonst -Serial angeben.

.EXAMPLE
.\port\scripts\ouya-gpu-info.ps1
#>
param(
    [string]$Serial = '',
    [string]$Adb = '',
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-OUYA-Probe'),
    [string]$Distro = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$remote = '/data/local/tmp/re4-ouya'
$stage = Join-Path $repo 'port\build\ouya-gpu-info'
New-Item -ItemType Directory -Force -Path $OutDir, $stage | Out-Null
$result = Join-Path $OutDir 'gpu-info.txt'
if (Test-Path -LiteralPath $result) { Remove-Item -LiteralPath $result }

$wslBase = @()
if ($Distro) { $wslBase = @('-d', $Distro) }
function ConvertTo-WslPath([string]$Path) {
    $full = (Resolve-Path -LiteralPath $Path).Path -replace '\\', '/'
    $converted = & wsl.exe @wslBase --exec wslpath -a $full
    if ($LASTEXITCODE -ne 0 -or -not $converted) { throw "Pfad laesst sich nicht nach WSL umrechnen: $Path" }
    return ($converted | Select-Object -First 1).Trim()
}

# ---- 1. build (WSL, NDK r23c, Android 4.1) ---------------------------------------------------
$repoWsl = ConvertTo-WslPath $repo
$stageWsl = ConvertTo-WslPath $stage
Write-Host '1. gles2_info fuer Android 4.1 bauen (WSL, NDK r23c) ...'
$compiler = '/opt/android-ndk-r23c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++'
& wsl.exe @wslBase --exec $compiler --target=armv7a-linux-androideabi16 -std=c++17 -O2 -Wall -Wextra `
    -static-libstdc++ '-Wl,--hash-style=both' -o "$stageWsl/gles2_info" "$repoWsl/port/ouya/tools/gles2_info.cpp" `
    -lEGL -lGLESv2
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath (Join-Path $stage 'gles2_info'))) {
    throw 'Build fehlgeschlagen (NDK r23c in WSL eingerichtet? Sonst einmal ouya-probe.ps1 laufen lassen).'
}

# ---- 2. the OUYA (ADB, under the shared lock) -------------------------------------------------
if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (-not (Test-Path -LiteralPath $Adb)) { $Adb = 'adb' }
}
. (Join-Path $PSScriptRoot 'Retroid-Adb-Exclusive.ps1')
function Invoke-Adb {
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { return @(& $Adb -s $Serial @args 2>&1 | ForEach-Object { "$_".TrimEnd("`r") }) } finally { $ErrorActionPreference = $old }
}

Invoke-RetroidAdbExclusive -ThrowOnUnavailable -LockTimeoutMs 60000 -Sequence {
    if (-not $Serial) {
        $old = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try { $listed = @(& $Adb devices 2>&1 | ForEach-Object { "$_" }) } finally { $ErrorActionPreference = $old }
        $serials = @($listed | Where-Object { $_ -match '^(\S+)\s+device\s*$' } | ForEach-Object { ($_ -split '\s+')[0] })
        foreach ($candidate in $serials) {
            $Serial = $candidate
            $model = ((Invoke-Adb shell getprop ro.product.model) -join '').Trim()
            $maker = ((Invoke-Adb shell getprop ro.product.manufacturer) -join '').Trim()
            if ("$maker $model" -match 'OUYA') { break }
            $Serial = ''
        }
        if (-not $Serial) {
            throw "Keine OUYA gefunden (verbunden: $($serials -join ', ')). Kennung mit -Serial angeben; adb devices zeigt sie."
        }
    }
    $state = ((Invoke-Adb get-state) -join '').Trim()
    if ($state -ne 'device') { throw "Geraet $Serial ist nicht verbunden oder nicht freigegeben ($state)." }
    $model = ((Invoke-Adb shell getprop ro.product.model) -join '').Trim()
    $release = ((Invoke-Adb shell getprop ro.build.version.release) -join '').Trim()
    Write-Host "2. Geraet $Serial`: $model, Android $release"
    Invoke-Adb shell mkdir $remote | Out-Null
    Invoke-Adb push (Join-Path $stage 'gles2_info') "$remote/gles2_info" | Out-Null
    Invoke-Adb shell chmod 755 "$remote/gles2_info" | Out-Null
    $lines = Invoke-Adb shell "$remote/gles2_info"
    $header = @("RE4 OUYA-Grafik  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')", "device: $Serial $model Android $release")
    ($header + $lines) | Set-Content -LiteralPath $result -Encoding UTF8
}
Write-Host "3. Ergebnis: $result"
Get-Content -LiteralPath $result -Encoding UTF8 | Where-Object { $_ -notmatch '^(gl_extensions|egl_extensions|egl_configs)' }
