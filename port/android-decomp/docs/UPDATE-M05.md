# M05 installieren und prüfen

Voraussetzung: M01 + M02 + M03 + M04. Alle vollständigen geänderten/neuen
Dateien über RE4-Android-Decomp kopieren, neu bauen und mit demselben
Signierschlüssel über die vorhandene App installieren. Kein erneuter Disc-Import.

Version 0.5.0-dev / versionCode 5. Paket com.ast.re4decomp, ARM64,
Android 5.0/API 21 und GLES 3.0 unverändert.

1. „Raumarchiv und Yz2 prüfen“ starten: r100/r300 sollen weiterhin PASS melden.
2. Unter den SMD-Zusammenfassungen stehen jetzt GX-Ergebnisse.
3. r100 SMD[0]: 552 Primitive, 10.534 Referenzen, 6.020 Dreiecke.
4. r100 SMD[1]: 3.577 Primitive, 36.267 Referenzen, 24.351 Dreiecke.
5. r300 SMD[0]: 6.300 Primitive, 98.097 Referenzen, 59.559 Dreiecke.
6. Bestehende Raumtextur-/Controller-Testansicht weiterhin prüfen.

Dreieckszahlen enthalten Degeneraten. Bei Abweichungen den RE4Archive-Log
schicken. Die neue Version ist gebaut und auf dem Host mit beiden Discs
geprüft; ihr Android-Gerätelauf steht noch aus. Noch keine neue 3D-Ansicht.
Formatbelege, Tests und Grenzen: docs/GX.md.
