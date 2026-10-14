# Update M12 — gemeinsame Raumressourcen

Voraussetzung: vollständiges Projekt einschließlich M11. Alle Dateien aus dem ZIP mit ihrer Struktur über den bestehenden Ordner **RE4-Android-Decomp** kopieren. Es sind vollständige geänderte/neue Dateien, keine Diffs. VersionCode 12, VersionName 0.12.0-dev; Paket weiterhin com.ast.re4decomp, ARM64/API 21/GLES 3.

Mit demselben Signaturschlüssel über die vorhandene Installation installieren. Nicht deinstallieren: bestehende Disc-Imports bleiben verwendbar. Falls Gradle doppelte generierte R-Klassen im Dex meldet, einmal `clean assembleDebug` ausführen; die App-Daten auf dem Gerät werden dadurch nicht gelöscht.

## Gerätetest

1. „Raumarchiv und Yz2 prüfen“ öffnen und „Prüfung starten“ ausführen.
2. Die neue Schaltfläche „r100: starre 3D-Geometrie anzeigen“ öffnen. Sie benötigt Disc 1. Die Startauswahl ist Basis-Modell 6, die Brücke.
3. LB/RB durch die Modelle schalten; Start zeigt die primäre starre Gesamt-Teilansicht. Dafür muss RE4Scene melden:
   - 13 Platzierungen, 6.020 Dreiecke.
   - 6.020 texturierte Basisdreiecke, 0 ungelöst.
   - Common-Texturen: 6.020 Basisdreiecke, 3.543 Maskendreiecke.
4. SELECT ein/aus testen, Orbit und Abstand prüfen. Danach sollte wieder dasselbe Texturbild sichtbar sein.
5. r300 erneut prüfen: Basis-Modell 29 weiterhin 6.399 Dreiecke, 304 Alpha-Dreiecke, 0 ungelöst. Modell 18 weiterhin 180 Vertexfarb-Dreiecke. Normalenansicht und App-Pause/Wiederaufnahme prüfen.

Die 18 Records in der gemeinsamen r100-SMD werden nicht zusätzlich als Raumgeometrie gezeichnet. Gemeinsame BIN-, Textur- und Motion-Verweise werden gemäß ihren Rollen gelesen; Animationen laufen noch nicht. Die Darstellung bleibt eine starre Teilansicht ohne Gameplay.

Bitte das nächste Log mit RE4Archive-/RE4Scene-Zeilen und gegebenenfalls sichtbaren Auffälligkeiten schicken. Ein M12-Gerätetest wurde beim Erstellen dieses ZIPs noch nicht durchgeführt.
