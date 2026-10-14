# M03 Fix01 — raumabhängige Archivteile

Voraussetzung: M01 + M02 + M03. Alle vollständigen Dateien aus diesem ZIP
über den bestehenden Ordner RE4-Android-Decomp kopieren, neu bauen und mit
dem bisherigen Signierschlüssel über die installierte App installieren.
Keine Deinstallation oder erneuter Disc-Import erforderlich.
Version 0.3.0-dev / versionCode 3 unverändert.

Der Android-Log vom 2026-10-03 meldet für r300:
`java.io.IOException: Raumarchiv: Pflichtteil fehlt: EMI`.
Die zusätzliche M03-JNI-Prüfung verlangte fälschlich sechs Tags in jedem Raum.
Die originale Datei r300 hat keinen EMI-Teil. Diese pauschale Anforderung wurde
entfernt; die Archivprüfung akzeptiert die tatsächlich vorhandenen Teil-Dateien.
Die bisherigen Prüfungen von Quelldatei, Yz2-Ausgabe und Archivgrenzen bleiben aktiv.
Eine angeforderte Texturvorschau benötigt weiterhin einen gültigen TPL-Teil.

Auf dem Gerät erneut „Raumarchiv und Yz2 prüfen“ starten: r100 und r300 sollen
PASS melden. Anschließend „Raumtextur aus r100 anzeigen“ prüfen.
Dieser Fix ergänzt keine 3D-Darstellung oder Spielsimulation.
