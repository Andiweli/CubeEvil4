# M15 — Begehbare Bewegungsdiagnose

Voraussetzung: vollständiges Projekt bis einschließlich M14. Alle vollständigen geänderten/neuen Dateien mit ihrer Struktur über **RE4-Android-Decomp** kopieren. VersionCode 15 / VersionName 0.15.0-dev; Paket com.ast.re4decomp, ARM64, API21 und GLES3. Mit demselben Signaturschlüssel über die vorhandene App installieren; vorhandene Disc-Imports bleiben erhalten. Bei doppelten generierten R-Klassen einmal `clean assembleDebug` bauen.

## Steuerung mit normalem Controller

| Taste | Bewegungsdiagnose |
| --- | --- |
| A / PlayStation Kreuz | Testfigur ein-/ausschalten |
| Linker Stick | Bewegen, abhängig von der Kamerarichtung |
| Rechter Stick | Kamera drehen/kippen |
| B / PlayStation Kreis halten | Schneller bewegen |
| Y / PlayStation Dreieck | Texturen / SAT-Farben wechseln |
| LT / RT | Kameraabstand |
| SELECT / Back | Normalenansicht |
| Start | Im Bewegungstest zum Teststartpunkt zurück |
| LB / RB | Basis-Modell wechseln, nur ohne Bewegungstest/SAT |

Die Bezeichnungen gelten für Standard-Android/Xbox-/PlayStation-Tastenmeldungen. Abweichende Gerätebeschriftungen können andere Buchstaben zeigen. Y/Dreieck ist die Taste, die in M14 bereits die Kollisionsansicht umgeschaltet hat. Keine Nintendo-Tastenbelegung wird vorausgesetzt.

## Gerätetest

1. Zuerst r300 aus Disc 2 laden und in 3D öffnen. A/Kreuz drücken. Eine gelbe Drahtkapsel mit kleiner Fußmarke erscheint, die Kamera folgt ihr. Die vorhandene unterstützte starre Raumgeometrie wird nun insgesamt dargestellt.
2. Linker Stick vor/zurück/seitlich, rechter Stick drehen. An geraden Wänden und schräg entlang einer Wand testen. Die Figur soll stoppen beziehungsweise entlanggleiten, ohne hindurchzuspringen. B/Kreis halten und loslassen: Geschwindigkeitswechsel prüfen.
3. An Bodenrändern oder großen Höhenunterschieden testen: Der Prototyp soll stehenbleiben, nicht fallen. Gelb = frei, Orange = Wand-/Randkontakt. Status zeigt Bodenpolygon, Höhe und Begrenzung.
4. Y/Dreieck drücken: Die Position bleibt erhalten, jetzt sind die SAT-Flächen grün/gelb/blau. Wieder zurückschalten und SELECT ein/aus testen. Texturen und Masken müssen erhalten bleiben.
5. LT/RT und Kamerablick in engen Bereichen testen. Bei einem Sichtstrahl-Wandtreffer kommt die Kamera näher heran; der Drahtmarker bleibt sichtbar. Das ist eine Diagnosekamera.
6. Start setzt die Testposition zurück. A/Kreuz beendet den Bewegungstest und stellt die bisher gewählte Modellansicht wieder her. LB/RB wählen danach wieder Modelle.
7. r100 und weitere unterstützte Räume ausprobieren. r100 kann an den automatisch gewählten Bodenflächen in der Texturansicht sehr dunkel sein; SAT hilft bei der Geometrieprüfung. Dies ist keine originale Spielstartposition oder vollständige Raumbeleuchtung.
8. Controller trennen/neu verbinden sowie App-Pause/Wiederaufnahme testen: kein hängenbleibender Ausschlag, keine große nachgeholte Bewegung. Bei neuem GL-Kontext muss A/Kreuz erneut den Test starten.

Bitte Raumnamen und Auffälligkeiten nennen, insbesondere Durchgänge, Wandgleiten, Randbegrenzung und Kamera. Im Log sind **RE4Walk**, RE4Collision, RE4Scene und RE4Room relevant. RE4Walk zeigt Position, Bodenpolygon/Attribute, Wandpolygon und Randstatus; beim Bewegen höchstens ungefähr einmal pro Sekunde.

## Was dieser größere Schritt liefert

Original-SAT-Abfragen mit belegten Attributfiltern, eigene BVH-Suche, kontinuierliche Testkapsel, Bodenbezug, Wandgleiten, Randbegrenzung, Followkamera, kleine Bodenmarke und vorhandene Texturgeometrie. Kein Leon-Modell, keine Animationen, Gegner, Audio, Interaktion, originaler Spawn, Türwechsel oder fertiges Gameplay. Technische Grenzen und Testnachweise: WALK-PROBE.md.
