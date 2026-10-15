<#
.SYNOPSIS
Holt die Grafik-Aufnahmen der App "RE4 Kern" vom Retroid (R3 im Spiel).

.DESCRIPTION
Ein Druck auf den rechten Stick (R3) speichert in der App die vollstaendigen
Grafikdaten des naechsten Bildes (zum Nachzeichnen in der Cloud). Nach einem
Spiel ohne test-core-app.ps1 holt dieses Skript sie unter der gemeinsamen
ADB-Sperre (port/docs/adb-coordination.md) und legt sie als gx-geraet.zip in
-OutDir ab (Standard: Desktop\RE4-Kern-App). Danach werden sie auf dem Retroid
geloescht (-Keep laesst sie dort). gx-geraet.zip nicht ins Repository legen.

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\port\scripts\pull-gx-dumps.ps1
#>
param(
    [string]$Serial = 'dbdcacc',
    [string]$Adb = '',
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-Kern-App'),
    [switch]$Keep
)
$ErrorActionPreference = 'Stop'
$package = 'com.ast.re4decomp.core'
$remote = "/sdcard/Android/data/$package/files/gx-dumps"
if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (-not (Test-Path -LiteralPath $Adb)) { $Adb = 'adb' }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
. (Join-Path $PSScriptRoot 'Retroid-Adb-Exclusive.ps1')
Invoke-RetroidAdbExclusive -ThrowOnUnavailable -LockTimeoutMs 60000 -Sequence {
    if ((& $Adb -s $Serial get-state) -ne 'device' -or $LASTEXITCODE -ne 0) {
        throw "Retroid ($Serial) ist nicht verbunden oder nicht freigegeben."
    }
    $local = Join-Path $OutDir 'gx-geraet'
    Remove-Item -LiteralPath $local -Recurse -ErrorAction SilentlyContinue
    # (no dumps: adb reports a missing folder on stderr, which 'Stop' must not turn into an error)
    $previousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $Adb -s $Serial pull $remote $local 2>&1 | Out-Null
    $ErrorActionPreference = $previousPreference
    $dumps = @(Get-ChildItem -LiteralPath $local -Filter 'gx-geraet-*.bin' -ErrorAction SilentlyContinue)
    if (-not $dumps.Count) {
        Write-Host 'Keine Grafik-Aufnahmen auf dem Retroid (im Spiel R3 druecken, dann dieses Skript).'
        return
    }
    $zip = Join-Path $OutDir 'gx-geraet.zip'
    Remove-Item -LiteralPath $zip -ErrorAction SilentlyContinue
    Compress-Archive -Path $dumps.FullName -DestinationPath $zip
    Write-Host ("{0} Grafik-Aufnahmen in {1} ({2} MB). Bitte mitschicken." -f $dumps.Count, $zip,
        [math]::Round((Get-Item -LiteralPath $zip).Length / 1MB, 1))
    if (-not $Keep) { & $Adb -s $Serial shell rm -rf $remote }
}
