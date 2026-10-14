<#
.SYNOPSIS
Lokale Pruefung des Spielkerns mit deinen Disc-Daten (Raum r100).

.DESCRIPTION
Der Spielkern ist fuer den Test ein Linux-Programm, deshalb laeuft die Pruefung in
WSL2 (Ubuntu). Dieses Skript erledigt alles von Windows aus:

  1. richtet die Werkzeuge in WSL einmalig ein (port/tools/whole_setup_linux.sh,
     braucht Internet; danach schnell),
  2. startet port/tools/whole_r100_check.sh: kopiert den Quelltext dieses
     Ordners nach ~/re4-kern in WSL, baut den Kern und prueft ohne Disc-Daten,
     vergleicht die Formatumwandler mit den App-Lesern auf deinen Disc-Dateien,
     startet das Spiel vom Titelbild mit Debug-Start nach r100 und laesst es
     10.000 Bilder laufen,
  3. legt den Bericht und die Logs in -OutDir ab (Standard: Desktop\RE4-Kern-Pruefung).

Disc-Daten (-DataDir), eine dieser Formen:
  - ein Ordner mit den Disc-Abbildern (.iso oder .gcm), z. B. D:\RE4\Disc
  - mit Dolphin entpackte Disc ("Gesamte Disc extrahieren": Ordner mit sys und files)
  - der Import-Ordner der App (disc1, disc2) oder ein Port-Ordner (disc0, disc1)
Die Disc-Daten werden nie ins Repository kopiert.

Vorher den Branch aktualisieren:
  git fetch origin
  git checkout claude/re4-android-decomp-core
  git pull

.EXAMPLE
.\port\scripts\test-r100-core.ps1 -DataDir 'D:\RE4\Disc'

.EXAMPLE
.\port\scripts\test-r100-core.ps1 -DataDir 'D:\RE4\Disc' -Arm
Zusaetzlich der ARM32-Build unter qemu (wie auf dem Retroid, aber langsam).
#>
param(
    [Parameter(Mandatory = $true)][string]$DataDir,
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-Kern-Pruefung'),
    [int]$Frames = 10000,
    [string]$Room = '100',
    [switch]$Arm,
    [switch]$SkipSetup,
    [string]$Distro = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
# Linux programs write UTF-8; show it (and read wslpath's answer) as such.
$previousEncoding = [Console]::OutputEncoding
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
    throw 'WSL fehlt. Einmal in PowerShell als Administrator: wsl --install -d Ubuntu-24.04 , Rechner neu starten, dann dieses Skript erneut starten.'
}
if (-not (Test-Path -LiteralPath $DataDir)) { throw "Ordner mit den Disc-Daten nicht gefunden: $DataDir" }
if ($Room -notmatch '^[0-5][0-9A-Fa-f]{2}$') { throw "Raum als drei Hex-Ziffern angeben, z. B. 100 (nicht: $Room)" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$wslBase = @()
if ($Distro) { $wslBase = @('-d', $Distro) }

# Windows path -> WSL path (/mnt/c/...); --exec runs without a shell, so
# spaces and backslashes in the path stay as they are.
function ConvertTo-WslPath([string]$Path) {
    $full = (Resolve-Path -LiteralPath $Path).Path -replace '\\', '/'
    $converted = & wsl.exe @wslBase --exec wslpath -a $full
    if ($LASTEXITCODE -ne 0 -or -not $converted) { throw "Pfad laesst sich nicht nach WSL umrechnen: $Path" }
    return ($converted | Select-Object -First 1).Trim()
}

$repoWsl = ConvertTo-WslPath $repo
$dataWsl = ConvertTo-WslPath $DataDir
$outWsl = ConvertTo-WslPath $OutDir

if (-not $SkipSetup) {
    Write-Host 'Werkzeuge in WSL pruefen und bei Bedarf einrichten (beim ersten Mal einige Minuten) ...'
    & wsl.exe @wslBase -u root --exec bash "$repoWsl/port/tools/whole_setup_linux.sh"
    if ($LASTEXITCODE -ne 0) { throw 'Einrichtung der Werkzeuge in WSL fehlgeschlagen (Internetverbindung noetig).' }
}

$checkArgs = @('--source', $repoWsl, '--data', $dataWsl, '--out', $outWsl, '--frames', "$Frames", '--room', $Room)
if ($Arm) { $checkArgs += '--arm' }
Write-Host 'Pruefung laeuft (Build beim ersten Mal 10 bis 30 Minuten, danach schneller) ...'
& wsl.exe @wslBase --exec bash "$repoWsl/port/tools/whole_r100_check.sh" @checkArgs
$code = $LASTEXITCODE

$report = Join-Path $OutDir 'bericht.txt'
Write-Host ''
if (Test-Path -LiteralPath $report) {
    if ($code -eq 0) { Write-Host 'Ergebnis: ALLES OK' -ForegroundColor Green }
    else { Write-Host 'Ergebnis: NICHT BESTANDEN (Einzelheiten im Bericht)' -ForegroundColor Yellow }
    Write-Host "Bericht: $report"
    Write-Host 'Bitte schicke mir die Datei bericht.txt (die uebrigen Dateien im Ordner nur, wenn ich danach frage).'
} else {
    Write-Host "Kein Bericht entstanden (Exit-Code $code). Bitte schicke mir die Ausgabe oben." -ForegroundColor Red
}
[Console]::OutputEncoding = $previousEncoding
exit $code
