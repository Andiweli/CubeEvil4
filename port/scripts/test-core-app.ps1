<#
.SYNOPSIS
Kernmodus der App RE4-Android-Decomp auf dem Retroid: Kern bauen, APK bauen,
installieren, starten, Bildschirmfotos und Protokoll holen.

.DESCRIPTION
Drei Schritte, jeder einzeln abschaltbar:
  1. WSL2 (Ubuntu): Android-NDK einmalig einrichten (whole_setup_linux.sh --android,
     braucht Internet) und den Original-Spielkern fuer ARM32 bauen
     (whole_build_android.sh). Die Bibliotheken landen in
     port\android-decomp\app\src\core\jniLibs\armeabi-v7a (nicht im Repository).
  2. Windows: die App-Variante "core" bauen (gradlew.bat :app:assembleCoreDebug,
     mit dem Android-SDK von Android Studio).
  3. Retroid per ADB, unter der gemeinsamen Sperre Global\Retroid_ADB_Exclusive
     (port/docs/adb-coordination.md): APK installieren (App "RE4 Kern",
     com.ast.re4decomp.core, neben der bisherigen App), starten, mehrere
     Bildschirmfotos aufnehmen, das Protokoll der App holen, App beenden.

Beim ersten Mal in der App "RE4 Kern" Disc 1 importieren (Menu > Import Disc 1;
Disc 2 erst fuer spaetere Raeume). Fehlt der Import, startet das Skript die App,
bittet darum und wartet bis zu -ImportMinutes Minuten (Standard 45), ohne die App
zu beenden; erst danach beginnt der Test. Der Kern braucht einen neuen, vollstaendigen
Import in der App "RE4 Kern" (die bisherige App hat einen eigenen Datenordner).

Standard ist ein Debug-Start direkt in r100 mit derselben Test-Tastenfolge wie
die PC-Pruefung (Speicherkarten-Dialog, Titel, Debug-Menue); sobald der Raum
laedt, endet die Tastenfolge und dein Controller steuert. -Manual startet ohne
Tastenfolge (Titelbild, du spielst selbst).

Ergebnis in -OutDir (Standard: Desktop\RE4-Kern-App): bericht.txt, logcat.txt,
bild-*.png, kern-build.log, apk-build.log. Bitte bericht.txt und die Bilder schicken.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\port\scripts\test-core-app.ps1

Gradle braucht Java 17 bis 23. Das Skript nimmt JAVA_HOME, wenn es passt, sonst
das Java von Android Studio (jbr) oder ein JDK 17/21 unter C:\Program Files;
-JavaHome gibt den Ordner fest vor. JAVA_HOME wird nur fuer diesen Lauf gesetzt.

.EXAMPLE
.\port\scripts\test-core-app.ps1 -SkipCore -Seconds 180
Nur APK bauen und testen (Kern-Bibliotheken liegen schon in der App), 3 Minuten.
#>
param(
    [string]$Serial = 'dbdcacc',
    [string]$Adb = '',
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-Kern-App'),
    [int]$Seconds = 120,
    [switch]$Manual,
    [switch]$SkipCore,
    [switch]$SkipApk,
    [switch]$SkipDevice,
    [switch]$SkipSetup,
    [string]$Distro = '',
    [string]$JavaHome = '',
    [int]$ImportMinutes = 45
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$app = Join-Path $repo 'port\android-decomp'
$package = 'com.ast.re4decomp.core'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$report = Join-Path $OutDir 'bericht.txt'
Remove-Item -LiteralPath $report -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $OutDir -Filter 'bild-*.png' -ErrorAction SilentlyContinue | Remove-Item
function Say([string]$Text) {
    Write-Host $Text
    Add-Content -LiteralPath $report -Value $Text -Encoding UTF8
}
# A native program with its output on the screen and in LOGFILE; returns the
# exit code. Windows PowerShell 5.1 would stop at the first line on stderr
# (compiler warnings, Gradle progress) while 'Stop' is in force.
function Invoke-Logged([string]$LogFile, [scriptblock]$Command) {
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & $Command 2>&1 | ForEach-Object {
            # stderr lines arrive as error records; their text is the message
            $line = if ($_ -is [System.Management.Automation.ErrorRecord]) { $_.Exception.Message } else { "$_" }
            # leading CR: wsl.exe can leave the console without automatic
            # carriage return (lines would drift to the right)
            Write-Host ("`r" + $line)
            $line
        } | Set-Content -LiteralPath $LogFile -Encoding UTF8
        return $LASTEXITCODE
    } finally { $ErrorActionPreference = $old }
}
# the end of a log into the report (already shown on the screen)
function Add-ReportTail([string]$LogFile, [int]$Lines) {
    if (Test-Path -LiteralPath $LogFile) {
        Get-Content -LiteralPath $LogFile -Encoding UTF8 | Select-Object -Last $Lines |
            ForEach-Object { Add-Content -LiteralPath $report -Value "   $_" -Encoding UTF8 }
    }
}
# major version of the JDK in a folder, 0 when there is none
function Get-JavaMajor([string]$Folder) {
    if (-not $Folder) { return 0 }
    $java = Join-Path $Folder 'bin\java.exe'
    if (-not (Test-Path -LiteralPath $java)) { return 0 }
    $old = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { $text = (& $java -version 2>&1 | ForEach-Object { "$_" }) -join ' ' } finally { $ErrorActionPreference = $old }
    if ($text -match 'version "(\d+)(\.(\d+))?') {
        $major = [int]$Matches[1]
        if ($major -eq 1 -and $Matches[3]) { $major = [int]$Matches[3] }  # 1.8 = 8
        return $major
    }
    return 0
}
Say "RE4 Kernmodus-Test  $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Say "Repository: $repo"
$previousEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

# ---- 1. the game core for ARM32 (WSL) --------------------------------------------------
if (-not $SkipCore) {
    if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        throw 'WSL fehlt. Einmal in PowerShell als Administrator: wsl --install -d Ubuntu-24.04'
    }
    $wslBase = @()
    if ($Distro) { $wslBase = @('-d', $Distro) }
    $full = (Resolve-Path -LiteralPath $repo).Path -replace '\\', '/'
    $repoWsl = ((& wsl.exe @wslBase --exec wslpath -a $full) | Select-Object -First 1).Trim()
    if (-not $SkipSetup) {
        Say '1. Werkzeuge und Android-NDK in WSL (beim ersten Mal ca. 700 MB Download) ...'
        & wsl.exe @wslBase -u root --exec bash "$repoWsl/port/tools/whole_setup_linux.sh" --android
        if ($LASTEXITCODE -ne 0) { throw 'Einrichtung in WSL fehlgeschlagen (Internet noetig).' }
    }
    Say '1. Spielkern fuer ARM32 bauen (beim ersten Mal 10 bis 30 Minuten) ...'
    $coreLog = Join-Path $OutDir 'kern-build.log'
    $code = Invoke-Logged $coreLog {
        wsl.exe @wslBase --exec bash "$repoWsl/port/tools/whole_build_android.sh" --source $repoWsl `
            --into "$repoWsl/port/android-decomp/app"
    }
    Add-ReportTail $coreLog 30
    if ($code -ne 0) { throw 'Kern-Build fehlgeschlagen (Einzelheiten oben und in kern-build.log).' }
}
$libs = Join-Path $app 'app\src\core\jniLibs\armeabi-v7a'
if (-not (Test-Path -LiteralPath (Join-Path $libs 'libre4_main.so'))) {
    throw "Kern-Bibliotheken fehlen in $libs (Schritt 1 ausfuehren)."
}
Say "Kern-Bibliotheken: $((Get-ChildItem -LiteralPath $libs -Filter '*.so').Count) Dateien"

# ---- 2. the APK of the core flavor (Windows, Gradle) --------------------------------------
$apk = Join-Path $app 'app\build\outputs\apk\core\debug\app-core-debug.apk'
if (-not $SkipApk) {
    Say '2. APK bauen (gradlew :app:assembleCoreDebug) ...'
    # Gradle 8.11 with the Android plugin 8.9: Java 17 to 23
    $candidates = @()
    if ($JavaHome) { $candidates += $JavaHome }
    else {
        if ($env:JAVA_HOME) { $candidates += $env:JAVA_HOME }
        foreach ($base in @($env:ProgramFiles, $env:LOCALAPPDATA, (Join-Path $env:LOCALAPPDATA 'Programs'))) {
            if (-not $base) { continue }
            foreach ($studio in @(Get-ChildItem -Path (Join-Path $base 'Android') -Filter 'Android Studio*' -Directory -ErrorAction SilentlyContinue) +
                                @(Get-ChildItem -Path $base -Filter 'Android Studio*' -Directory -ErrorAction SilentlyContinue)) {
                $candidates += (Join-Path $studio.FullName 'jbr')
            }
        }
        foreach ($vendor in @('Eclipse Adoptium', 'Java', 'Microsoft', 'Zulu', 'Amazon Corretto')) {
            $candidates += @(Get-ChildItem -Path (Join-Path $env:ProgramFiles $vendor) -Directory -ErrorAction SilentlyContinue |
                Sort-Object Name -Descending | ForEach-Object { $_.FullName })
        }
    }
    $jdk = $null
    foreach ($c in $candidates) {
        $major = Get-JavaMajor $c
        if ($major -ge 17 -and $major -le 23) { $jdk = $c; break }
    }
    if (-not $jdk) {
        $seen = ($candidates | ForEach-Object { "$_ (Java $(Get-JavaMajor $_))" }) -join '; '
        throw "Kein Java 17 bis 23 gefunden (Gradle braucht es). Gefunden: $seen. Abhilfe: Android Studio installieren oder -JavaHome 'Ordner eines JDK 17' angeben."
    }
    $env:JAVA_HOME = $jdk
    Say "   Java fuer Gradle: $jdk (Java $(Get-JavaMajor $jdk))"
    $localProps = Join-Path $app 'local.properties'
    if (-not (Test-Path -LiteralPath $localProps)) {
        $sdk = Join-Path $env:LOCALAPPDATA 'Android\Sdk'
        if (-not (Test-Path -LiteralPath $sdk)) { throw 'Android-SDK nicht gefunden: einmal das Projekt in Android Studio oeffnen.' }
        "sdk.dir=$($sdk -replace '\\', '\\\\')" | Set-Content -LiteralPath $localProps -Encoding ASCII
    }
    $gradleLog = Join-Path $OutDir 'apk-build.log'
    Push-Location $app
    try {
        $code = Invoke-Logged $gradleLog { .\gradlew.bat ':app:assembleCoreDebug' }
    } finally { Pop-Location }
    Add-ReportTail $gradleLog 25
    if ($code -ne 0) { throw 'APK-Build fehlgeschlagen (Einzelheiten oben und in apk-build.log).' }
}
if (-not (Test-Path -LiteralPath $apk)) { throw "APK fehlt: $apk" }
Say "APK: $apk ($([math]::Round((Get-Item -LiteralPath $apk).Length / 1MB, 1)) MB)"

# ---- 3. the device ------------------------------------------------------------------------
if ($SkipDevice) {
    Say 'Geraetetest uebersprungen.'
    [Console]::OutputEncoding = $previousEncoding
    exit 0
}
if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (-not (Test-Path -LiteralPath $Adb)) { $Adb = 'adb' }
}
$settings = ''
if (-not $Manual) {
    # the timing of the PC check (port/tools/whole_r100_check.sh)
    $settings = 'RE4_START_ROOM=0x100;RE4_PAD_UNTIL_ROOM=1;RE4_PAD_SCRIPT=UP@100-60000/60 A@115-60000/60 START@130-60000/60'
}
. (Join-Path $PSScriptRoot 'Retroid-Adb-Exclusive.ps1')
Invoke-RetroidAdbExclusive -ThrowOnUnavailable -LockTimeoutMs 60000 -Sequence {
    if ((& $Adb -s $Serial get-state) -ne 'device' -or $LASTEXITCODE -ne 0) {
        throw "Retroid ($Serial) ist nicht verbunden oder nicht freigegeben."
    }
    $abis = ((& $Adb -s $Serial shell getprop ro.product.cpu.abilist) -join '').Trim()
    Say "3. Geraet $Serial, ABIs: $abis"
    if ($abis -notmatch 'armeabi-v7a') { throw 'Das Geraet kann keine 32-bit-Apps ausfuehren.' }
    & $Adb -s $Serial install -r $apk | ForEach-Object { Say "   $_" }
    if ($LASTEXITCODE -ne 0) { throw 'Installation fehlgeschlagen.' }
    $info = (& $Adb -s $Serial shell dumpsys package $package) -join "`n"
    if ($info -notmatch 'primaryCpuAbi=armeabi-v7a') { throw 'Die App laeuft nicht als armeabi-v7a.' }
    Say '   App installiert (armeabi-v7a)'
    # The import runs inside the app (several minutes per disc): the test waits for it instead of
    # stopping the app, because stopping it cancels the import.
    $game = "/sdcard/Android/data/$package/files/game"
    function Get-ImportState {
        $out = (& $Adb -s $Serial shell "cd $game 2>/dev/null && ls -d disc1/sys/fst.bin disc1/ready.properties .incoming .work-disc1 .work-disc2 2>/dev/null; du -sm .incoming .work-disc1 .work-disc2 2>/dev/null") -join "`n"
        $mb = 0
        foreach ($line in ($out -split "`n")) { if ($line -match '^(\d+)\s') { $mb += [int]$Matches[1] } }
        return [pscustomobject]@{
            Ready = ($out -match 'disc1/sys/fst\.bin') -and ($out -match 'disc1/ready\.properties')
            Busy  = $out -match '(^|\n)\.(incoming|work-disc\d)\s*(\n|$)'
            MB    = $mb
        }
    }
    $state = Get-ImportState
    if (-not $state.Ready -or $state.Busy) {
        # the installation above ended any running import; the app clears what it left behind
        # (DiscInstaller.recover) when it starts
        & $Adb -s $Serial shell input keyevent KEYCODE_WAKEUP
        & $Adb -s $Serial shell am start -n "$package/com.ast.re4decomp.MainActivity" | Out-Null
        Say '   In der App "RE4 Kern" fehlt Disc 1 (oder ein Import laeuft noch).'
        Say '   Bitte jetzt auf dem Retroid in der App "Import Disc 1" waehlen und die Disc-1-Datei (.gcm/.iso) aussuchen.'
        Say '   Disc 2 ist fuer diesen Test nicht noetig. Das Skript wartet und beendet die App waehrenddessen NICHT.'
        Say "   Wartezeit hoechstens $ImportMinutes Minuten; Abbruch mit Strg+C."
        $deadline = (Get-Date).AddMinutes($ImportMinutes)
        $lastShown = -1
        while ($true) {
            Start-Sleep -Seconds 10
            $state = Get-ImportState
            if ($state.Ready -and -not $state.Busy) { Say '   Disc 1 ist importiert.'; break }
            if ($state.Busy -and $state.MB -ne $lastShown) {
                Write-Host ("`r   Import laeuft: {0} MB bisher geschrieben   " -f $state.MB) -NoNewline
                $lastShown = $state.MB
            }
            if ((Get-Date) -gt $deadline) {
                Write-Host ''
                throw "Disc 1 ist nach $ImportMinutes Minuten nicht importiert. Bitte den Import in der App abschliessen und das Skript mit -SkipCore -SkipApk erneut starten."
            }
        }
        Write-Host ''
    }
    & $Adb -s $Serial shell am force-stop $package
    # device time in seconds (logcat -T accepts 'sssss.mmm'; no spaces through adb shell)
    $since = ((& $Adb -s $Serial shell date +%s) -join '').Trim() + '.000'
    & $Adb -s $Serial shell input keyevent KEYCODE_WAKEUP
    $start = "am start -n $package/com.ast.re4decomp.MainActivity"
    if ($settings) { $start += " --es coreSettings '$settings'" }
    & $Adb -s $Serial shell $start | ForEach-Object { Say "   $_" }
    Say "   gestartet ($(if ($Manual) { 'Titelbild, du steuerst' } else { 'Debug-Start r100 mit Test-Tastenfolge' })), $Seconds s Beobachtung"
    $shots = @(15, 40) + @(70..([math]::Max(70, $Seconds)) | Where-Object { ($_ - 70) % 25 -eq 0 })
    $elapsed = 0
    $firstPid = ''
    foreach ($at in $shots) {
        if ($at -gt $Seconds) { break }
        Start-Sleep -Seconds ($at - $elapsed)
        $elapsed = $at
        $remote = '/data/local/tmp/re4-core-shot.png'
        & $Adb -s $Serial shell screencap -p $remote
        & $Adb -s $Serial pull $remote (Join-Path $OutDir ('bild-{0:D3}s.png' -f $at)) | Out-Null
        & $Adb -s $Serial shell rm -f $remote
        $alive = ((& $Adb -s $Serial shell pidof $package) -join '').Trim()
        if ($alive -and -not $firstPid) { $firstPid = $alive }
        $appState = if (-not $alive) { 'LAEUFT NICHT MEHR' }
                 elseif ($alive -ne $firstPid) { "NEU GESTARTET (PID $alive, vorher ${firstPid}: abgestuerzt?)" }
                 else { "laeuft (PID $alive)" }
        Say ("   {0,3} s: Bild aufgenommen, App {1}" -f $at, $appState)
    }
    if ($Seconds -gt $elapsed) { Start-Sleep -Seconds ($Seconds - $elapsed) }
    $log = & $Adb -s $Serial logcat -d -v threadtime -T $since
    $appPid = ((& $Adb -s $Serial shell pidof $package) -join '').Trim()
    $keep = $log | Where-Object { $_ -match ' (re4|RE4\w*|DEBUG|AndroidRuntime|libc|crash_dump\w*)\s*:' -or ($appPid -and $_ -match " $appPid ") }
    $keep | Set-Content -LiteralPath (Join-Path $OutDir 'logcat.txt') -Encoding UTF8
    & $Adb -s $Serial shell am force-stop $package
    Say "   Protokoll: $($keep.Count) Zeilen (logcat.txt); App beendet"

    Say ''
    Say '== Auswertung =='
    $text = $keep -join "`n"
    function Has([string]$Pattern) { return [bool]($text -match $Pattern) }
    Say ("Kern geladen, Speicher reserviert: {0}" -f $(if (Has 'core loaded, MEM1 reserved') { 'ja' } else { 'NEIN' }))
    Say ("Spiel gestartet:                  {0}" -f $(if (Has 'core: running') { 'ja' } else { 'NEIN' }))
    $rooms = $keep | Where-Object { $_ -match 'RE4Core.*room [0-9a-f]{3}: ' }
    Say "Raeume aufgebaut:                 $(if ($rooms) { ($rooms | ForEach-Object { ($_ -split 'RE4Core\s*:\s*')[-1] }) -join ' | ' } else { 'keiner' })"
    $models = @($keep | Where-Object { $_ -match 'RE4Core.*model ' })
    Say "Figuren-Modelle geladen:          $($models.Count) (Fehler: $(@($models | Where-Object { $_ -notmatch 'joints' }).Count))"
    # the opening event and the radio call end on their own: the core logs the
    # moment the player is free (no event, no sub screen for 300 frames)
    $free = $keep | Where-Object { $_ -match 'run: player free' } | Select-Object -First 1
    Say "Intro und Funkspruch beendet:     $(if ($free) { 'ja (' + (($free -split 'run: ')[-1]) + ')' } else { 'NEIN (Spieler nie frei)' })"
    $presses = @($keep | Where-Object { $_ -match 're4\s*: pad: frame' })
    Say "Tastendruecke im Kern angekommen: $($presses.Count)"
    if (Has 'Fatal signal|FATAL EXCEPTION|SIGSEGV|SIGABRT') {
        Say 'ABSTURZ erkannt; Ausschnitt:'
        $keep | Where-Object { $_ -match 'Fatal signal|FATAL|backtrace|#\d\d pc |Abort message' } | Select-Object -First 40 |
            ForEach-Object { Say "  $_" }
    }
    Say 'Letzte 40 Zeilen des Kern-Protokolls (ohne Tastenzeilen):'
    $keep | Where-Object { $_ -match ' (re4|RE4Core)\s*:' -and $_ -notmatch 'pad: frame' } | Select-Object -Last 40 |
        ForEach-Object { Say "  $_" }
}
Say ''
Say "Fertig. Bitte bericht.txt und die Bilder (bild-*.png) aus $OutDir schicken."
[Console]::OutputEncoding = $previousEncoding
