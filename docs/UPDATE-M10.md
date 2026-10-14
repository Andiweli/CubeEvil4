# M10 installieren und prüfen

Voraussetzung: bestehendes RE4-Android-Decomp-Projekt mit M09.
Vollständige geänderte/neue Dateien aus dem ZIP übernehmen/ersetzen,
Gradle synchronisieren und bauen. Version 0.10.0-dev / versionCode 10
mit demselben Signierschlüssel über die bestehende App installieren.
Kein Deinstallieren und kein erneuter Disc-Import nötig.

1. Raumtests r100/r300: weiterhin PASS.
2. r300-Startansicht Modell 29 bleibt unverändert: 6.399 Dreiecke,
   304 Alpha-Dreiecke ohne ungelöste Masken, vier Mipmap-Texturen/
   20 Zusatzstufen. Neue RE4Scene-Zeile: Vertexfarb-Zweig: 0 Dreiecke.
3. Mit LB/RB zu App-Modell 18 wechseln. Es verweist auf lokales BIN 12;
   RE4Scene sollte 180 Vertexfarb-Dreiecke melden. Gegenüber M09 ist diese
   Ambientvorschau entsprechend dem gespeicherten RGB-Wert 128 dunkler.
4. Weitere aktive App-Modelle: 4, 27, 28, 45, 46. Unterschiedliche gespeicherte
   Farbwerte sind beabsichtigt; App-Modellnummer und BIN-Nummer unterscheiden sich.
5. Zurück zu Modell 29: dessen Helligkeit bleibt wie in M09. Transparenzen,
   LT/RT-Abstand, Orbit und Start-Gesamtansicht prüfen.
6. Zurück/erneutes Öffnen sowie Hintergrund/Vordergrund prüfen.
   Bei Problemen Screenshot plus RE4Scene/RE4Archive-Log schicken.

VERTEX-COLORS.md beschreibt die Originalflags und den begrenzten Ambientzweig.
Gerätetest von M10 ausstehend; weiterhin statische Darstellung ohne spielbare
Engine und ohne vollständige dynamische Beleuchtung.
