# M19 — originaler Ganado und tatsächliche Pistolentreffer

Version **0.19.0-dev**, versionCode **19**. Paket `com.ast.re4decomp`, ARM64, API 21, GLES 3.0. Dieses ZIP enthält vollständige geänderte/neue Dateien mit dem Stammordner `RE4-Android-Decomp`; es setzt M01 bis M18 voraus. Es enthält keine Spieldaten.

## Installation und Test

1. ZIP über den vorhandenen Projektordner entpacken und Dateien ersetzen.
2. Im Projekt `bash ./gradlew clean assembleDebug` beziehungsweise `gradlew.bat clean assembleDebug` ausführen; neu installieren. Anzeige muss **M19** melden.
3. Eine 3D-Raumansicht öffnen und **„Ganado-Test: r100“** wählen. Originale r100-Daten von Disc 1 werden im Hintergrund geprüft und geladen. Leon erscheint etwa 5,6 m vom unveränderten Original-Ganado entfernt und schaut für diesen Test zu ihm.
4. LT/L2 halten, mit dem rechten Stick auf Körper/Kopf zielen und RT/R2 drücken. Die Anzeige nennt Treffer, verbleibende HP und zuletzt angewendeten Schaden. Die dünne Ziellinie verursacht selbst keinen Schaden. Schüsse behalten die originale geringe Streuung.
5. X/Quadrat lädt nach. B/Kreis läuft; linker Stick bewegt/dreht Leon. Der Ganado geht mit Original-Root-Motion auf Leon zu, stoppt vor ihm und greift in dieser Stufe noch nicht an. Lebender Akteurkontakt stoppt die Testkapsel.
6. Mehrfach Körper/Kopf treffen, Tod abwarten, pausieren und fortsetzen. „Modellprüfung / Leon-Steuerung“, Hintergrund/Vordergrund und Controller-Trennung prüfen. Nach Unterbrechung beide Trigger freigeben und neu anlegen.
7. Erneut „Ganado-Test: r100“ erzeugt einen frischen Gegner mit 1.000 HP; Leons verbrauchte Munition bleibt erhalten. „Türtest“ kehrt zum geprüften r301/r300-Türlauf zurück. Ein Prozessneustart setzt das Testinventar zurück; es gibt noch keinen Spielstand.

Vorhandene vollständige Disc-Imports genügen. Zusätzlich verwendet werden `em/em12.drs` und `etc/emleon00.esl` von Disc 1, die bereits vollständig importiert wurden. Beide Dateien werden vor Freigabe des Ganado-Tests mit Größe und SHA-256 geprüft. Fehler werden angezeigt und unter `RE4Enemy` protokolliert. Ein neuer Import ist nur bei unvollständigen/falschen Daten nötig.

## Tatsächlicher Fortschritt

- Vier echte EM12-BIN-Modelle, 34 Gelenke, 3.013 Dreiecke und drei Originaltexturen mit den vorhandenen GPU-Skinningregeln.
- Originale Stand-/Geh-/Kopf-/Körper-/Todes-FCV/SEQ, Root-Motion, Hokan und Spiegelung.
- Exakte ESL-Lesegrenzen und unveränderter Gegnerdatensatz 0x25. Spielerposition stammt aus `r100_Sce_look`; Testblickrichtung und Aktivierung sind eigene Adapter.
- Zehn aktive Originaltrefferkapseln an den **animierten** Gelenken; Kopf-/Körperauswertung, EAT-Weltbegrenzung und echte RT-Schüsse statt bloßer Rückstoßanimation.
- FN57-Ausgangsschaden 135, normaler Kopf 162, kritischer Kopf 1.350 vor Begrenzung auf verbleibende HP. Fester neutraler Rang 5; kein adaptiver Schwierigkeitsgrad.
- Gemeinsamer 30-Hz-Takt; Treffer werden wie im Original nach dem Spielerschuss vorgemerkt und im nächsten Gegnertakt angewendet. Kurze Schadenssperre, mehrfach treffbarer Gegner während einer Reaktion und dauerhafte finale Todespose.
- Gegnerzustand, Munition, Pose und eigener Test-RNG bleiben bei Pause/Inspektor/Kontextverlust erhalten. Raumdaten/EAT/Gegner bilden einen gemeinsamen CPU-/GPU-Commit mit Rückfall bei Lade- oder Grafikfehlern.
- Die seitliche Zielkamera wurde vergrößert, weil Leon bei wandbedingt verkürztem Kameraabstand den Ganado verdeckte. Auch der neue Kameraoffset wird vollständig gegen SAT geprüft; Gegner werden nicht durch Wände sichtbar gezeichnet.

## Grenzen

Dies ist ein **einzelner Gegner- und Treffertest**, kein startender Originalkampagnenabschnitt. Vollständige `cEm10`-KI, Wege-/Tür-/Leiterlogik, Griff und Leons Befreiung, Gegnerwaffen, Parasiten/Kopfverlust, weitere Schadensvarianten, Beute, Inventar, Effekte, Audio und Speichern fehlen. Die direkte Annäherung, ausgewählten Frontreaktionen und stehende Todesanimation sind ausdrücklich begrenzt. Ein kritischer Kopftreffer benutzt bislang denselben vollständigen Körper statt Kopfverlust. Details und Quellen stehen in [ENEMY-ENCOUNTER.md](ENEMY-ENCOUNTER.md).

Für die nächste Prüfung bitte Logcat mit `RE4Enemy`, `RE4Weapon`, `RE4Player`, `RE4Flow`, `RE4Scene` und etwaigen Crash-Markern senden. Relevant sind Treffer/HP, verdeckte Schüsse, finale Todespose, Pause/Fortsetzung und Rückkehr zum Türtest. M19 wurde auf dem Host und im Android-Build geprüft; der Gerätetest dieser Version steht aus.
