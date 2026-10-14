# M11 installieren und prüfen

Voraussetzung: bestehendes RE4-Android-Decomp mit M10. Die vollständigen
geänderten/neuen Dateien übernehmen/ersetzen, Gradle synchronisieren und bauen.
Version 0.11.0-dev / versionCode 11 mit demselben Signierschlüssel über die
vorhandene App installieren. Kein Deinstallieren oder erneuter Disc-Import nötig.

1. Raumtests r100/r300 weiterhin PASS.
2. r300-Modell 29 beginnt in der bisherigen Texturansicht:
   6.399 Basis-Dreiecke, 304 Alpha-Dreiecke, vier Mipmap-Texturen/
   20 Zusatzstufen, null ungelöst. Neue RE4Scene-Zeile:
   Normalen: 19197 Ecken · 0 Nullnormalen · 0 singuläre Platzierungen.
3. SELECT einmal drücken: farbige Normalen-Prüfansicht, im Log
   Normalenansicht: EIN. Farben kennzeichnen Richtungen, keine Lichtsimulation.
4. Orbit und LT/RT-Abstand testen. SELECT erneut drücken:
   unveränderte Texturansicht; Log Normalenansicht: AUS.
5. LB/RB-Modellwechsel und Start-Gesamtansicht auch im Normalenmodus testen.
   Prüfen, dass Alpha-Aussparungen bestehen bleiben. Modell 18 behält seine
   180 Vertexfarb-Dreiecke und die in M10 belegte dunklere Ambientvorschau.
6. Zurück/erneutes Öffnen und Hintergrund/Vordergrund testen. Nach einer
   Neuerstellung des GLES-Kontexts startet der Texturmodus wieder.
7. Bei Auffälligkeiten Screenshot und RE4Scene/RE4Archive-Log schicken.
   Magenta in der Normalenansicht bezeichnet eine Nullnormale und wird gezählt.

Belege und offene Engine-Schritte: NORMALS.md. Weiterhin statische Darstellung
mit Controller-Prüfsteuerung; kein spielbares RE4 und keine dynamische Beleuchtung.
