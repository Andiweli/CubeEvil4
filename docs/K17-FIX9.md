# K17 Fix9 – Ingame Options

Kumulatives Austauschpaket einschließlich Fix1–Fix8. Den Ordner RE4-Android-Decomp über den bisherigen Projektstand kopieren und Dateien ersetzen. Vollständige Dateien, keine APK.

Nur CheatDialog.java wurde in der Implementierung gegenüber Fix8 geändert:

- Größere, fette Überschrift Ingame Options (22sp).
- Beschriftung Anisotropic Filtering:; gemeinsame Beschriftungsspalte mit 196dp Breite.
- MSAA- und AF-Radiogruppen wieder horizontal, jeweils vier gleich breite Spalten. Die vierte MSAA-Zelle bleibt leer. Dadurch steht No MSAA über No, 2x MSAA über 2x AF und 4x MSAA über 4x AF. 8x AF belegt die vierte Spalte.
- Beim Wiederherstellen der wirksamen Auswahl wird die leere Zelle übersprungen.

Vollständige Android-Java-Kompilierung bestanden. Keine physische Popup-Layoutprüfung. Native Grafik- und Animationsquellen sind gegenüber Fix8 unverändert; deren Prüfnachweise liegen weiterhin bei. Im neuen Gerätelog werden Fenster-Samples 0 sowie die Anwendung von 2x/4x MSAA und 8x AF auf Adreno 650 bestätigt. Das allein ersetzt keinen optischen Vergleich.
