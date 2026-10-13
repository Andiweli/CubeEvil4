# Gemeinsamer Retroid-Zugriff

Verbindlicher Standard für BioShock iOS und Resident Evil 4 Android:
**Windows Named Mutex `Global\Retroid_ADB_Exclusive`**. Beide Projekte verwenden
byteidentische PowerShell- und Python-Helfer. Es ist ein Windows-Kernelobjekt,
kein projektinternes Statusfile. Keine Seriennummer wird dem Mutexnamen angehängt:
auch unterschiedliche Aufruferangaben führen zu derselben exklusiven Sperre.

`Retroid-Adb-Exclusive.ps1` verwendet `System.Threading.Mutex`, `WaitOne(60000)`
und `ReleaseMutex`/`Dispose` in `try/finally`. Warten und Freigeben geschehen auf
demselben PowerShell-Thread. Defaulttimeout 60 Sekunden; der Python-Runner kann
ihn über `RETROID_ADB_LOCK_TIMEOUT_MS` im Bereich 0..300000 ms einstellen.
Bei Timeout, Startfehler oder abgebrochenem Besitzer wird kein neuer Hardwaretest
gestartet. Abandonment benötigt zuerst eine Prüfung eventuell noch laufender
Gerätetests. Ein verlorener Mutexbesitzer blockiert weitere ADB-Aufrufe des Runners.

Die bestehenden Python-Einstiegspunkte erwerben automatisch die Sperre für ihre
gesamte Hardwarefunktion: Geräteprüfung, Installation/Transfer, alle Testläufe,
App-Start, Fault-/Crash-Tests, Logsammlung, Auswertung und vorhandene Bereinigung.
Ein PowerShell-Broker hält den Mutex bis zur expliziten Freigabe im Python-`finally`.
Sein stdin wird bei beendetem Runner geschlossen. Timeout wird als
`HARDWARE_PENDING` und Exitcode 73 ausgegeben; ein erfolgreicher Lock ist kein
erfolgreicher Hardwaretest. Lokale RE4-Aufrufe ohne `--adb` bleiben ungesperrt.
Die beiden RE4-PowerShell-Testskripte halten denselben Mutex unmittelbar um ihre
komplette Testsequenz.

Vor dem ersten Hardwarezugriff wird `adb -s <SERIAL> get-state` geprüft. Jeder
ADB-Aufruf verwendet diese Kennung. Die Python-Schutzschicht prüft dies zusätzlich
vor jedem ADB-Aufruf und verbietet globale Server-/Gerätemanagementbefehle.
Keine globalen Logcat-Clears, kein `kill-server`, kein Neustart, kein Stoppen oder
Löschen fremder Pakete. BioShock: `org.bioshock.c1.test` und
`/data/local/tmp/bioshock-c1-*`; RE4-Fixed: `org.re4.port`,
`/data/local/tmp/re4-*` (darunter `re4-port-tests` und `re4-port-import`). Logs liegen in getrennten Projektverzeichnissen.
Koordinationsjournale stehen in `build/adb-coordination/` (BioShock) bzw.
`port/build/adb-coordination/` (RE4). Frühere Hardwareberichte bleiben erhalten.

Alle zukünftigen Hardwareeinstiegspunkte müssen diesen Standard übernehmen.
Der Mutex kann einen manuell gestarteten, ungeschützten ADB-Prozess oder einen
bereits vor der Umstellung laufenden Test nicht rückwirkend sperren. Vor der ersten
Hardwaresequenz nach Installation ist deshalb zu prüfen, dass kein solcher alter
Test aktiv ist. Bei unklarer Lage bleiben Hardwaretests ausstehend.

In PowerShell für eine zusammengehörige neue Hardwaresequenz:

```powershell
. ./android/scripts/Retroid-Adb-Exclusive.ps1
Invoke-RetroidAdbExclusive -ThrowOnUnavailable -Sequence {
    # Hier die gesamte Sequenz einschließlich Logs und Bereinigung ausführen.
    # Ausschließlich adb -s <SERIAL> verwenden.
}
```

RE4 verwendet den Helfer unter `port/scripts/Retroid-Adb-Exclusive.ps1`.
Python-Runners besitzen den Lock bereits: nicht zusätzlich in einem zweiten
Prozess um sie herum sperren, sonst wartet der Kindprozess auf seinen Elternprozess.

## Tatsächliche lokale Verifikation

Am 11. Oktober 2026 bestanden sechs Koordinationstests mit unabhängigen Windows-
Prozessen und den tatsächlichen BioShock-/RE4-Kopien: beidseitige Konkurrenz und
Timeout, Erwerb nach Freigabe, Python-Ausnahme, PowerShell-Ausnahme, abgebrochener
Mutexbesitzer sowie direkt aufgerufene Hardware-Runners bei belegtem Mutex.
Die drei blockierten Runner führten jeweils **null ADB-Kommandos** aus; ihre
Koordinationsjournale bestätigen `hardware_pending`. Beide Helferdateien sind
byteidentisch. Die gesamte lokale BioShock-Suite besteht mit **38/38 Tests**.
Log: `analysis/reports/coordination_local_tests_final.log`. Das ist ein tatsächlicher
Lock-Nachweis auf Windows, kein zusätzlicher Retroid-Ausführungsnachweis.

Die folgende C3-Hardwaresequenz hielt denselben Mutex tatsächlich vom 11. Oktober
2026, 19:05:32.787713 bis 19:05:51.197993 UTC, Besitzer-PID32296. Alle 71 ADB-Aufrufe
verwendeten `-s dbdcacc`; Installation, sechs frische App-Prozesse, native Logs,
Crash-/Prozessauskunft, vollständige Auswertung und eigenes abschließendes
force-stop waren eingeschlossen. Brokerexit0, eigenes Paket bereinigt, kein fremdes
Paket betroffen. Die Identität aller Commands wurde unabhängig zwischen innerem
Hardwarejournal und äußerem Lockjournal abgeglichen:
`build/adb-coordination/run_c1_app_20261011T190532.527762Z_26804.json`.
Tatsächliche Hardwareergebnisse stehen im [C3-Bericht](E:/Development/Android/BioShock-iOS/docs/PHASE_C3.md) des
BioShock-Projekts. Die inzwischen erweiterte lokale Suite besteht mit 58/58 Tests;
die sechs projektübergreifenden Koordinationstests bestehen weiterhin.
