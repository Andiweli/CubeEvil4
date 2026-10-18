<#
.SYNOPSIS
Holt die Speicherbelegung der App "CubeEvil4" unter 4 GB vom Retroid
(Meilenstein 12, 64 bit).

.DESCRIPTION
Der Spielkern braucht auf 64 bit den Bereich 0x80000000..0x83000000 (MEM1, der
Hauptspeicher des GameCube an seiner Originaladresse) und darueber bis 4 GB Platz
fuer seine Bibliotheken. Dieses Skript zeigt, was dort im laufenden App-Prozess
liegt: unter der gemeinsamen ADB-Sperre (port/docs/adb-coordination.md) startet es
die App, falls sie nicht laeuft, liest ihre Speicherbelegung (/proc/<pid>/maps,
ueber run-as, geht nur mit einer Debug-APK) und die Java-Speichergroessen des
Geraets und legt beides als adressen-64bit.txt in -OutDir ab (Standard:
Desktop\RE4-Kern-App). Die App bleibt so, wie sie war (laufend oder gestartet).

.EXAMPLE
powershell -ExecutionPolicy Bypass -File .\port\scripts\pull-address-map.ps1
#>
param(
    [string]$Serial = 'dbdcacc',
    [string]$Adb = '',
    [string]$OutDir = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'RE4-Kern-App')
)
$ErrorActionPreference = 'Stop'
$package = 'com.ast.cubeevil4'  # until 18.10.2026 com.ast.re4decomp
if (-not $Adb) {
    $Adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
    if (-not (Test-Path -LiteralPath $Adb)) { $Adb = 'adb' }
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$target = Join-Path $OutDir 'adressen-64bit.txt'
. (Join-Path $PSScriptRoot 'Retroid-Adb-Exclusive.ps1')
Invoke-RetroidAdbExclusive -ThrowOnUnavailable -LockTimeoutMs 60000 -Sequence {
    if ((& $Adb -s $Serial get-state) -ne 'device' -or $LASTEXITCODE -ne 0) {
        throw "Retroid ($Serial) ist nicht verbunden oder nicht freigegeben."
    }
    $appPid = ((& $Adb -s $Serial shell pidof $package) -join ' ').Trim() -split '\s+' | Select-Object -First 1
    if (-not $appPid) {
        & $Adb -s $Serial shell input keyevent KEYCODE_WAKEUP
        & $Adb -s $Serial shell am start -n "$package/com.ast.re4decomp.MainActivity" | Out-Null
        Start-Sleep -Seconds 6
        $appPid = ((& $Adb -s $Serial shell pidof $package) -join ' ').Trim() -split '\s+' | Select-Object -First 1
    }
    if (-not $appPid) { throw "Die App $package laeuft nicht (ist sie installiert?)." }
    $info = (& $Adb -s $Serial shell dumpsys package $package) -join "`n"
    $abi = if ($info -match 'primaryCpuAbi=(\S+)') { $Matches[1] } else { '?' }
    $lines = @(& $Adb -s $Serial shell run-as $package cat /proc/$appPid/maps)
    if ($LASTEXITCODE -ne 0 -or $lines.Count -lt 10) {
        throw "Die Speicherbelegung liess sich nicht lesen (run-as geht nur mit einer Debug-APK): $($lines -join ' ')"
    }
    $props = foreach ($p in 'dalvik.vm.heapsize', 'dalvik.vm.heapgrowthlimit', 'dalvik.vm.heapstartsize',
        'ro.build.type', 'ro.build.fingerprint') {
        "${p}: $(((& $Adb -s $Serial shell getprop $p) -join '').Trim())"
    }
    # below 4 GB only; marked: MEM1 and the window above it
    $low = foreach ($line in $lines) {
        if ($line -notmatch '^([0-9a-f]+)-([0-9a-f]+)\s') { continue }
        $a = [Convert]::ToUInt64($Matches[1], 16)
        $b = [Convert]::ToUInt64($Matches[2], 16)
        if ($a -ge 0x100000000L) { continue }
        if ($a -lt 0x83000000L -and $b -gt 0x80000000L) { "$line   <- MEM1" }
        elseif ($b -gt 0x83000000L) { "$line   <- window" }
        else { $line }
    }
    $text = @("Speicherbelegung unter 4 GB, $package (PID $appPid, $abi), $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')") +
        $props + @("$($lines.Count) Eintraege insgesamt, $(@($low).Count) unter 4 GB:", '') + $low
    Set-Content -LiteralPath $target -Value $text -Encoding UTF8
    $mem1 = @($low | Where-Object { $_ -match '<- MEM1' })
    Write-Host "PID $appPid ($abi): $(@($low).Count) Eintraege unter 4 GB, davon $($mem1.Count) in MEM1."
    $mem1 | Select-Object -First 5 | ForEach-Object { Write-Host "   $_" }
    Write-Host "Gespeichert: $target"
}
