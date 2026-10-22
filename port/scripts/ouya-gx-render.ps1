<#
.SYNOPSIS
Spielbilder auf der OUYA-GPU zeichnen (Stufe O2 aus port/docs/ouya-machbarkeit.md): der
GLES-2-Renderer aus port/ouya zeichnet mitgeschnittene GX-Bilder des Spielkerns auf der OUYA;
die Bilder werden mit denen des GLES-3-Renderers der Haupt-App verglichen, die Zeit je Bild
gemessen.

.DESCRIPTION
Schritte:
  1. WSL: port/ouya/tools/gx_gl2_render.cpp mit dem NDK r23c fuer Android 4.1 bauen, dazu die
     PC-Werkzeuge (port/ouya/tools/check.sh-Build unter ~/re4-ouya/o2/build).
  2. Die Bilder (-Frames, ein Ordner in WSL mit gx-*.bin, wie RE4_GX_DUMP sie schreibt; Standard:
     ~/re4-ouya/o2/frames-compact, von port/ouya/tools/compare_frames.sh im Kompaktformat erzeugt)
     unter der gemeinsamen ADB-Sperre (Global\Retroid_ADB_Exclusive) nach
     /data/local/tmp/re4-ouya/gx2 kopieren, dort zeichnen, die Bilder zurueckholen.
  3. WSL: gx_gl2_compare zeichnet dieselben Bilder mit GLES 3 (Referenz) und vergleicht die der
     OUYA damit.
Ergebnis in -OutDir (Standard: Desktop\RE4-OUYA-Probe\gx2): bericht.txt, render.txt (Ausgabe
auf der OUYA: Zeit je Bild), vergleich.txt, die Bilder als PPM.

.EXAMPLE
.\port\scripts\ouya-gx-render.ps1
#>
param(
    [string]$Serial = '',
    [string]$Adb = '',
    [string]$Frames = '~/re4-ouya/o2/frames-compact',
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-OUYA-Probe\gx2'),
    [int]$Repeat = 5,
    [string]$Distro = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$remote = '/data/local/tmp/re4-ouya/gx2'
$stage = Join-Path $repo 'port\build\ouya-gx-render'
New-Item -ItemType Directory -Force -Path $OutDir, $stage | Out-Null
foreach ($pattern in @('bericht.txt', 'render.txt', 'vergleich.txt', '*.ppm')) {
    Get-ChildItem -LiteralPath $OutDir -Filter $pattern -ErrorAction SilentlyContinue | Remove-Item
}
$report = Join-Path $OutDir 'bericht.txt'
function Say([string]$Text) {
    Write-Host $Text
    Add-Content -LiteralPath $report -Value $Text -Encoding UTF8
}
$wslBase = @()
if ($Distro) { $wslBase = @('-d', $Distro) }
function ConvertTo-WslPath([string]$Path) {
    $full = (Resolve-Path -LiteralPath $Path).Path -replace '\\', '/'
    $converted = & wsl.exe @wslBase --exec wslpath -a $full
    if ($LASTEXITCODE -ne 0 -or -not $converted) { throw "Pfad laesst sich nicht nach WSL umrechnen: $Path" }
    return ($converted | Select-Object -First 1).Trim()
}
# a command line in WSL (bash -lc), its output on the screen; returns the exit code
function Invoke-Wsl([string]$Command) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & wsl.exe @wslBase --exec bash -lc $Command 2>&1 | ForEach-Object { Write-Host "   $_" }
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
}

Say "RE4 OUYA-Grafik: Spielbilder auf der OUYA-GPU  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$repoWsl = ConvertTo-WslPath $repo
$stageWsl = ConvertTo-WslPath $stage
$outWsl = ConvertTo-WslPath $OutDir

# ---- 1. build (WSL) ---------------------------------------------------------------------------
Say '1. gx_gl2_render fuer Android 4.1 bauen (WSL, NDK r23c) und die PC-Werkzeuge ...'
$cpp = "$repoWsl/port/ouya/app/src/main/cpp"
$main = "$repoWsl/port/android-decomp/app/src/main/cpp"
$build = "/opt/android-ndk-r23c/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++ --target=armv7a-linux-androideabi16 " +
    "-std=c++17 -O2 -Wall -Wextra -static-libstdc++ -Wl,--hash-style=both -I $cpp -I $main -I $repoWsl/port/whole/runtime " +
    "-o $stageWsl/gx_gl2_render $repoWsl/port/ouya/tools/gx_gl2_render.cpp $cpp/gx_gl2.cpp $cpp/gx_gl2_shader.cpp " +
    "$main/gx_texture.cpp -lEGL -lGLESv2 -llog"
if ((Invoke-Wsl $build) -ne 0) { throw 'Build fuer die OUYA fehlgeschlagen (NDK r23c in WSL? Sonst einmal ouya-probe.ps1).' }
$pcBuild = "cmake -S $cpp -B ~/re4-ouya/o2/build -DCMAKE_BUILD_TYPE=RelWithDebInfo > /dev/null && " +
    "cmake --build ~/re4-ouya/o2/build --parallel 8 --target gx_gl2_compare > /dev/null"
if ((Invoke-Wsl $pcBuild) -ne 0) { throw 'Build der PC-Werkzeuge fehlgeschlagen (Mesa-Pakete: port/tools/whole_setup_linux.sh).' }

# ---- 2. the frames ------------------------------------------------------------------------------
Say "2. Bilder aus $Frames ..."
if ((Invoke-Wsl "rm -rf $stageWsl/frames && mkdir -p $stageWsl/frames && cp $Frames/gx-*.bin $stageWsl/frames/") -ne 0) {
    throw "Keine Bilder in $Frames (erst port/ouya/tools/compare_frames.sh laufen lassen)."
}
$frameFiles = @(Get-ChildItem -LiteralPath (Join-Path $stage 'frames') -Filter 'gx-*.bin' | Sort-Object Name)
$megabytes = [math]::Round(($frameFiles | Measure-Object -Sum Length).Sum / 1MB, 1)
Say "   $($frameFiles.Count) Bilder, $megabytes MB"

# ---- 3. the OUYA (ADB, under the shared lock) -------------------------------------------------
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
$pictures = Join-Path $OutDir 'ouya'
New-Item -ItemType Directory -Force -Path $pictures | Out-Null
Get-ChildItem -LiteralPath $pictures -Filter '*.ppm' -ErrorAction SilentlyContinue | Remove-Item
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
    Say "3. Geraet $Serial`: $model; kopieren ($megabytes MB, etwa 1 MB/s) ..."
    Invoke-Adb shell rm -r $remote | Out-Null
    Invoke-Adb shell mkdir -p "$remote/out" | Out-Null
    Invoke-Adb shell mkdir "$remote/frames" | Out-Null
    Invoke-Adb push (Join-Path $stage 'gx_gl2_render') "$remote/gx_gl2_render" | Out-Null
    Invoke-Adb shell chmod 755 "$remote/gx_gl2_render" | Out-Null
    foreach ($file in $frameFiles) { Invoke-Adb push $file.FullName "$remote/frames/$($file.Name)" | Out-Null }
    Say '   zeichnen ...'
    $names = ($frameFiles | ForEach-Object { "$remote/frames/$($_.Name)" }) -join ' '
    $lines = Invoke-Adb shell "cd $remote && RE4_GX2_REPEAT=$Repeat ./gx_gl2_render out $names"
    $lines | Set-Content -LiteralPath (Join-Path $OutDir 'render.txt') -Encoding UTF8
    $lines | Where-Object { $_ -match '^(in all|device|gx_gl2_render|GPU)' } | ForEach-Object { Say "   $_" }
    foreach ($file in $frameFiles) {
        $stem = [IO.Path]::GetFileNameWithoutExtension($file.Name)
        Invoke-Adb pull "$remote/out/$stem-ouya.ppm" (Join-Path $pictures "$stem-ouya.ppm") | Out-Null
    }
    Invoke-Adb shell rm -r "$remote/frames" | Out-Null
}

# ---- 4. compare on the PC (WSL) -----------------------------------------------------------------
Say '4. Vergleich mit dem GLES-3-Renderer der Haupt-App (WSL, Mesa) ...'
$picturesWsl = ConvertTo-WslPath $pictures
$compare = "mkdir -p $outWsl/pc && RE4_GX2_DEVICE=$picturesWsl ~/re4-ouya/o2/build/gx_gl2_compare $outWsl/pc " +
    "$stageWsl/frames/gx-*.bin 2> /dev/null > $outWsl/vergleich.txt; tail -3 $outWsl/vergleich.txt"
Invoke-Wsl $compare | Out-Null
Get-Content -LiteralPath (Join-Path $OutDir 'vergleich.txt') -Encoding UTF8 | ForEach-Object {
    if ($_ -match 'device:') { Say ('   ' + ($_ -replace '^(\S+\s+\S+).*?device:', '$1 OUYA:')) }
    elseif ($_ -match '^(in all|gx_gl2_compare)') { Say "   $_" }
}
Say "Ergebnis: $OutDir"
