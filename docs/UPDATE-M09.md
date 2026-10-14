# M09 installieren und prüfen

Voraussetzung: bestehendes Projekt einschließlich M08. Die vollständigen
geänderten Dateien aus diesem ZIP in RE4-Android-Decomp übernehmen/ersetzen,
Gradle synchronisieren und bauen. Version 0.9.0-dev / versionCode 9 mit dem
bisherigen Signierschlüssel über die bestehende App installieren.
Kein Deinstallieren oder erneuter Disc-Import erforderlich.

1. Raumtest: r100 und r300 weiterhin PASS.
2. r300-Modell 29: 6.399 Dreiecke, 21 Texturen; Alpha-Prüfung 304, null ungelöst.
3. RE4Scene meldet Original-Mipmaps: 4 Texturen, 20 Zusatzstufen,
   0 begrenzte Ketten für dieses Modell.
4. Mit rechtem Stick drehen, mit LT/RT den Abstand verändern. Entfernte
   Texturdetails und Maskenkanten sollten sich ruhiger verhalten; leichte
   Weichzeichnung bei großer Entfernung ist bei Mipmap-Filterung erwartbar.
5. LB/RB, Start-Gesamtansicht, Zurück/erneutes Öffnen und Hintergrund/Vordergrund
   prüfen. Modelle dürfen unterschiedliche Mipmap- und Maskenzahlen besitzen.
   Begrenzte Ketten sind diagnostiziert und verwenden nur belegte Bildstufen.
6. Bei einer Auffälligkeit Screenshot plus RE4Scene/RE4Archive-Log schicken.
   Log-Anleitung: UPDATE-M06.md.

Technische Belege und bewusst offene Runtime-Filterzustände: MIPMAPS.md.
Die Geräteprüfung von M09 ist nach dem Update erforderlich.
Dies bleibt ein statischer Daten-/Grafikstand ohne spielbare Engine.
