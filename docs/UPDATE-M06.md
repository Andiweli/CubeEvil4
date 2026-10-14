# M06 installieren und prüfen

Voraussetzung: vollständiges M01-Projekt mit M02, M03 einschließlich Fix01,
M04 und M05. Dieses ZIP enthält ausschließlich vollständige neue/geänderte
Dateien mit dem Stammordner RE4-Android-Decomp.

1. Dateien in das bestehende Projekt kopieren und vorhandene Dateien ersetzen.
2. Gradle synchronisieren; assembleDebug und lintDebug ausführen.
3. Mit demselben Signierschlüssel über die bestehende App installieren.
   Nicht deinstallieren: vorhandene Disc-Importe bleiben beim Update verfügbar.
4. Version 0.6.0-dev / versionCode 6 öffnen und den Raumarchiv-Test ausführen.
5. r100 und r300 müssen PASS melden. Zusätzlich die Werte der starren Ruhepose
   prüfen: r100 13/6.020 und 18/24.351; r300 48/59.559
   (Basis-Platzierungen/Welt-Dreiecke).
6. „r300: starre 3D-Geometrie anzeigen“ öffnen. Erwartet wird eine untexturierte
   Architekturansicht, Basis-Modell 29 mit 6.399 Dreiecken.
7. Rechter Stick: drehen. LT/RT: Abstand. LB/RB: Modellwahl. Start: Gesamtansicht.
   Die Gesamtansicht enthält große Hintergrundgeometrie; kleine Details sind
   dort beim Einpassen entsprechend klein. LB/RB bringen Einzelmodelle zurück.
8. Android-Zurück, erneutes Öffnen sowie Hintergrund/Vordergrund prüfen.
   Controller-Trennung und Wiederverbindung dürfen keine hängenden Eingaben erzeugen.

Native Hosttests, unabhängiger Weltgeometrievergleich, echter GLES-Hosttest,
ARM64-Debug-Build und Android-Lint sind bestanden. Die M06-Geräteprüfung steht
aus. Dieser Patch enthält keine Textur-/TEV-Materialumsetzung und keinen Spielstart.

Bei Problemen den vollständigen Text des Raumtests, Logcat mit RE4Scene und
RE4Archive sowie einen Screenshot schicken. Beispiel für angeschlossenes Gerät:

```sh
adb logcat -d -v threadtime RE4Scene:V RE4Archive:V AndroidRuntime:E '*:S' > re4-m06-log.txt
```

Technische Belege und Grenzen: SCENE.md und VALIDATION.md.
