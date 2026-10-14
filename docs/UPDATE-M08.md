# M08 installieren und prüfen

Voraussetzung: bestehendes Projekt mit M07. Vollständige Dateien unter
RE4-Android-Decomp übernehmen/ersetzen, Gradle synchronisieren und bauen.
Version 0.8.0-dev / versionCode 8 mit demselben Signierschlüssel über die
bisherige App installieren. Nicht deinstallieren; kein neuer Disc-Import nötig.

1. Raumtest: r100 und r300 weiterhin PASS.
2. r300-3D-Ansicht: Modell 29 mit 6.399 Dreiecken und jetzt 21 Texturen
   (16 Basisbilder plus fünf separat referenzierte Alphamasken).
3. RE4Scene: 6.399 texturierte Dreiecke, null ungelöst; zusätzlich
   Alpha-Prüfung: 304 Dreiecke, null ungelöst.
4. Alpha-maskierte Flächen sollten zuvor ausgefüllte Maskenbereiche aussparen.
   Die sichtbare Änderung hängt von Modell und Kamerawinkel ab.
5. Orbit/Abstand, LB/RB, Start-Gesamtansicht, Zurück/erneutes Öffnen und
   Hintergrund/Vordergrund prüfen. Steuerung unverändert. Bei Problemen
   Screenshot und RE4Scene/RE4Archive-Log wie in UPDATE-M06.md schicken.

Hosttests, unabhängiger Maskenvergleich aller 142 Archive, echter GLES-Test,
ARM64-Build und Lint bestanden. Gerätetest ausstehend. Keine vollständigen
Runtime-TEV-Zustände oder Gameplay. Belege/Grenzen: ALPHA.md.
