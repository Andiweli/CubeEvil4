# M18 — Originale Handfeuerwaffe und Spieleraktionen

Version **0.18.0-dev**, versionCode **18**. Weiterhin ARM64, Android 5.0 / API 21 und GLES 3.0, Paket `com.ast.re4decomp`, Projektordner **RE4-Android-Decomp**.

## Bestätigte Grundlage

Das Gerätelog `Eingefügter Text(20261003-225513).txt` enthält erfolgreiche Übergänge r301 → r300 und r300 → r301 mit den ursprünglichen Zielpositionen. Der Nutzer bestätigt den Türlauf. M18 setzt auf diesem Stand auf; daraus wird keine Behebung sämtlicher früher gemeldeter Materialdurchsichten abgeleitet.

## Änderungen

- Die originale normale WEP01-Handfeuerwaffe wird aus den bereits importierten Discdaten geladen: 605 Dreiecke, eine Originaltextur, vier eigene Gelenke. Das Waffenskelett hängt an Leons roher rechter Handmatrix #10; beide Modelle haben getrennte Skinning-Paletten und GPU-Ressourcen.
- Neun zusätzliche originale Spieler-FCVs ergänzen Anlegen/Ablegen, den dreifachen Ziel-/Rückstoßblend und normales Nachladen. Der native Animationsadapter übernimmt `MotionMove`, Kurven-IK, Hokan, cMot3-Blending und Hüftrotation. Die erste sieben Bewegungsclips und ihre Root-Bewegung bleiben erhalten.
- Der feste 30-Hz-Aktionsadapter hält Leon während Waffenaktionen stationär. RT/R2 wiederholt die Schussanimation im Basistakt von 14 Simulationstakten. Die Nachladeaktion überträgt die Testmunition einmal nach dem Überqueren von Kurvenframe 31, vor der nächsten Pose 32. Player-Recovery und Waffenpose besitzen eigene Zeitstände.
- Testmunition beginnt mit **10 im Magazin + 60 Reserve**. Die Magazingröße stammt aus der normalen Originalwaffe; die Reserve ist ausdrücklich ein Prüfbestand. Munition bleibt bei Türen und Grafik-Kontextwechseln erhalten. Pause/Controllerreset löschen Zeitnachlauf und Tastenkanten; beide Trigger müssen vor erneuten Aktionen physisch freigegeben werden.
- X/Quadrat ist jetzt Nachladen. Die Modellprüfung hat eine eigene Schaltfläche und friert Spieler- und Waffenbewegung ein. Die kurze Steuerungshilfe lässt sich für die vollständige Belegung antippen.
- Die eigene SAT-geprüfte Zielkamera hält die Kameraposition auf Schulterhöhe. Der sichtbare SAT-Zielstrahl folgt der originalen Mündung und Handrichtung. Er ist eine Kollisionsdiagnose, kein Treffer-/Schadenssystem.

Quellen, Datenzuordnung und Grenzen: `WEAPON-ACTIONS.md`, `PLAYER-PREVIEW.md`, `ROOM-FLOW.md`, `VALIDATION.md`.

## Installation und Gerätetest

1. Das ZIP über **RE4-Android-Decomp** entpacken und die enthaltenen vollständigen Dateien ersetzen. Im Projekt `bash ./gradlew clean assembleDebug lintDebug` ausführen und installieren. Vollständige vorhandene Discimporte reichen aus; Neuimport/Deinstallation sind nicht erforderlich.
2. Einen Raum mit Leon öffnen. **LT/L2 halten**: Waffe anlegen. **Rechter Stick**: nach oben/unten zielen und nach links/rechts drehen. Der linke Stick darf Leon während dieser Aktionen nicht verschieben.
3. **RT/R2 drücken/halten**: Original-Rückstoß und Waffenanimation, Magazinverbrauch im Status. Noch kein Schussgeräusch, Mündungsblitz, Schaden oder Gegner. Bei leerem Magazin nach erneutem RT/R2-Drücken oder mit X/Quadrat nachladen.
4. Bei gehaltenem LT/L2 **X/Quadrat drücken**. Die Nachladebewegung muss den Bestand einmalig auffüllen. Danach **LT/L2 loslassen**, die Ablegebewegung abwarten und mit dem linken Stick weitergehen. B/Kreis bleibt Laufen beziehungsweise mit Rückwärtsinput die 180°-Drehung.
5. Mit Start pausieren, beide Trigger freigeben, fortsetzen und erneut anlegen. Dasselbe nach Hintergrund/Fokusverlust oder Controllerwechsel prüfen. Gehaltenes RT/R2 darf dabei keine neue Salve starten. „Modellprüfung / Leon-Steuerung“ soll die Pose/Patronen erhalten; nur dort ändern LT/RT den Kameraabstand.
6. Den bisherigen r301 → r300 → r301-Türlauf bei abgelegter Waffe wiederholen. Testmunition muss nach dem Übergang unverändert bleiben.

Relevante Logcat-Tags: **RE4Weapon, RE4Player, RE4Flow, RE4Walk, RE4Scene** sowie Fatal-/ANR-Zeilen. M18 wurde mit echten Assets und Host-GLES/JNI geprüft; der neue Android-Gerätetest steht bei Auslieferung aus.

## Grenze dieses Schritts

Dies ist eine native Waffen-/Spielerteilruntime, noch kein vollständiger Spielstart. Originaler ItemMgr, Gegner, Treffer/Schaden, Szenario, Audio, Schusseffekte, Speichern, Leiteraktionen, Terrain-IK und vollständige dynamische GX-/TEV-Raumdarstellung fehlen. Der nächste Kampfschritt muss die tatsächlichen Treffer- und Gegnerabläufe anbinden; der SAT-Zielstrahl ersetzt diese nicht.
