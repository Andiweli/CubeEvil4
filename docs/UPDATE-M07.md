# M07 installieren und prüfen

Voraussetzung: bestehendes Projekt einschließlich M06. ZIP-Dateien im Stammordner
RE4-Android-Decomp ersetzen; neue Dateien ebenfalls übernehmen. Es werden
vollständige geänderte Dateien geliefert, keine Diff-Fragmente.

1. Gradle synchronisieren, assembleDebug und lintDebug ausführen.
2. Version 0.7.0-dev / versionCode 7 mit demselben Signierschlüssel über die
   bisherige App installieren. Nicht deinstallieren; kein neuer Disc-Import nötig.
3. Raumarchiv-Test erneut ausführen. r100/r300 müssen weiterhin PASS melden.
4. „r300: starre 3D-Geometrie anzeigen“ öffnen. Erwartet wird Basis-Modell 29
   mit 6.399 Dreiecken und 16 originalen Basistexturen statt der türkisfarbenen
   M06-Diagnoseflächen. RE4Scene protokolliert 6.399 texturierte Dreiecke und
   null ungelöste Verweise für dieses Modell.
5. Rechter Stick dreht, LT/RT verändern den Abstand, LB/RB wechseln Modelle;
   Start zeigt die Gesamtgeometrie. Modellwechsel, Zurück/erneutes Öffnen sowie
   Hintergrund/Vordergrund prüfen. Die Gesamtansicht besitzt weiterhin sehr
   große Hintergrundgeometrie und ist nicht sichtbarkeitsgefiltert.

Android-Build, Lint, Hostverträge, unabhängiger Vergleich aller 142 Archive und
echter GLES-3-Texturtest bestanden. M07 muss auf dem Gerät geprüft werden.
Noch keine vollständigen TEV-Materialien, Alpha-Effekte oder Spielsimulation;
Details: MATERIALS.md. Bei Problemen Screenshot und RE4Scene/RE4Archive-Log
wie in UPDATE-M06.md schicken.
