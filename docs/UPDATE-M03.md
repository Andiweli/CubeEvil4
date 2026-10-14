# M03 installieren und prüfen

Alle vollständigen Dateien dieses ZIPs über den bestehenden Projektordner
**RE4-Android-Decomp** kopieren. Voraussetzung ist M01 plus M02.
Danach neu bauen und über die vorhandene App installieren; nicht deinstallieren.
Bei gleichem Signierschlüssel bleiben die bisherigen Disc-Imports verwendbar.
Version 0.3.0-dev / versionCode 3; Paket, ARM64 und minSdk 21 unverändert.

1. „Raumarchiv und Yz2 prüfen“ öffnen und „Prüfung starten“ drücken.
2. Beide Räume müssen PASS melden; zusätzlich erscheint die Anzahl der Teil-Dateien.
3. Nach unten scrollen: vollständige Inhaltslisten mit Tags, Vorkommen und Grenzen.
4. „Raumtextur aus r100 anzeigen“ öffnen: jetzt 256×256/GX-Format 14 aus dem Raumarchiv.
5. Android-Zurück führt wieder zur Prüfung. Controller weiterhin in der Testansicht prüfen.

Bei Fehlern Screenshot oder Logcat mit Tag **RE4Archive** schicken.
Dies ist eine echte Archiv-/Textur-Ladekette, aber noch kein 3D-Raum oder spielbares RE4.
