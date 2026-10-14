# M04 installieren und prüfen

Voraussetzung: M01 + M02 + M03. Alle vollständigen geänderten/neuen Dateien
über den bestehenden Ordner RE4-Android-Decomp kopieren. Der M03-Fix01 zur
optional vorhandenen EMI-Datei ist im neuen JNI-Quellstand enthalten.
Neu bauen und mit demselben Signierschlüssel über die vorhandene App installieren.
Keine Deinstallation und kein erneuter Disc-Import erforderlich.

Version 0.4.0-dev / versionCode 4. Paket com.ast.re4decomp, ARM64,
Android 5.0/API 21 und GLES 3.0 unverändert.

1. „Raumarchiv und Yz2 prüfen“ starten. r100 und r300 sollen beide PASS melden.
2. In den ausführlichen Listen nach unten scrollen: neue SMD[0]/SMD[1]-Zusammenfassungen.
3. r100: 13 und 18 Basis-Platzierungen, 5.290 und 13.086 Vertices.
4. r300: 48 Basis-Platzierungen, 39.684 Vertices und 246 Displaylist-Blöcke.
5. Bisherige Raumtexturvorschau öffnen und Controller-Test weiter verwenden.

Bei Abweichungen Logcat mit RE4Archive schicken. Der neue native Leser ist
auf dem Host mit beiden Discs geprüft; die M04-Geräteprüfung steht noch aus.
Noch keine neue 3D-Ansicht. Formatbelege und Grenzen: docs/SMD.md.
