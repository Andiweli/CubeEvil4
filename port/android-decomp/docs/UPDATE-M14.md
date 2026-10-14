# M14 — Originale SAT-Kollisionsgeometrie

Voraussetzung: vollständiges Projekt einschließlich M13. Die vollständigen geänderten/neuen Dateien mit ihrer Struktur über **RE4-Android-Decomp** kopieren. VersionCode 14 / VersionName 0.14.0-dev; Paket com.ast.re4decomp, ARM64, API21 und GLES3 bleiben erforderlich. Mit demselben Signaturschlüssel über die vorhandene App installieren; keine Deinstallation und kein erneuter Disc-Import nötig.

Bei doppelten generierten R-Klassen im Dex einmal `clean assembleDebug` ausführen. Installierte App-Daten werden dadurch nicht verändert.

## Gerätetest

1. r100 aus Disc 1 laden und die 3D-Ansicht öffnen. Die bekannte texturierte Modellansicht muss erscheinen.
2. Die obere Controller-Taste (Y bei Xbox, Dreieck bei PlayStation; entsprechend dem gewählten Layout) drücken: Die separate SAT-Ansicht zeigt 1.507 Kollisionsdreiecke, darunter 262 Boden-, 128 Neigungs- und 1.117 Wanddreiecke. Grün = Boden, Gelb = Neigung, Blau = Wand. Orbit und Zoom benutzen, da aus dem Startwinkel überwiegend Wände sichtbar sein können.
3. SELECT schaltet die Originalnormalen ein/aus; die Gruppenfarben müssen wieder erscheinen. LB/RB/Start verändern in SAT keine Modellwahl.
4. Obere Taste erneut drücken: Das zuvor gewählte Basis-Modell kehrt mit seinen Texturen/Masken zurück. Kameraabstand und Orbit werden bei jedem Ansichtswechsel neu initialisiert.
5. r300 aus Disc 2 testen: SAT umfasst 890 Dreiecke (168 Boden, 30 Neigung, 692 Wand). Anschließend weitere geladene Räume prüfen.
6. App-Pause/Wiederaufnahme, Android-Zurück und erneutes Öffnen testen. Ein neu erstellter GL-Kontext beginnt in der Modellansicht. Controller abziehen/erneut verbinden.

Bitte das Log einschließlich `RE4Room`, `RE4Scene` und neu `RE4Collision` sowie den getesteten Raumnamen schicken. Fehlende/leere SAT-Daten erzeugen eine entsprechende Logmeldung und keine Umschaltung.

## Umfang

Es wird die erste SAT-Ressource, Variante 0, entsprechend dem originalen Raumstart gelesen. Der Parser prüft räumliche Blockverweise und erhält Polygonattribute/Kantenflags. Metadaten erscheinen auch im Raum-Ladebericht. Dynamische Kollisionsobjekte, weitere Varianten, Bodenabfragen, Spielerbewegung und Kollisionsantwort sind noch nicht umgesetzt. Dieser Patch liefert deren geprüfte Datenbasis und Diagnoseansicht. Details: SAT.md.

Validierung: unabhängiger Vergleich aller 142 Raumarchive/152 SAT-Vorkommen; 95.310 Polygone, 53.268 Blöcke, 417.445 Blattverweise. Native/Disc-/Controller-Regressionsprüfungen, SAT-Fehlertests, ASan/UBSan (ohne LeakSanitizer) und GLES3-Framebufferprüfungen bestanden. ARM64 `clean assembleDebug lintDebug`: erfolgreich, 0 Lint-Fehler/6 bestehende Warnungen. Kein Android-Gerätetest des neuen Patches im Entwicklungsumfeld.
