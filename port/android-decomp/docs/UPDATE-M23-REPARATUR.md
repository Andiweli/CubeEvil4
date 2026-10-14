# M23-Reparaturpaket — M22 und M23 zusammen

Das Buildlog vom04.10.2026 meldet fehlende `environment.cpp` und `environment_gl.cpp`. Diese Dateien stammen aus M22; M23 war ein Update über den vollständigen M22-Stand.

Dieses Paket enthält alle vollständigen geänderten/neuen Dateien aus M22 und M23, zusammengeführt in dieser Reihenfolge. Bei gemeinsam geänderten Dateien hat immer M23 Vorrang. Es eignet sich über M21, M22 oder einen unvollständig eingespielten M23-Stand. Ältere Basen benötigen zuerst die Updates bis M21.

## Installation

1. Android Studio und laufende Builds beenden.
2. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem vorhandenen Projektordner zusammenführen und Dateien ersetzen. Der Zielpfad muss beispielsweise `RE4-Android-Decomp/app/src/main/cpp/environment.cpp` sein, ohne doppelt verschachtelten Projektordner.
3. Die generierten Ordner `app/.cxx` und `app/build` löschen.
4. Projekt öffnen und `gradlew.bat clean assembleDebug lintDebug` ausführen.

Die Datei `app/src/main/cpp/environment_gl.cpp` muss ebenfalls vorhanden sein. CMake-Verweise und die Grafikfunktionen sollen erhalten bleiben.

Paketname `com.ast.re4decomp`, Versioncode23, Versionname `0.23.0-dev`; keine neue Gameplay-Version. Alle fünf M23-Korrekturen bleiben enthalten, einschließlich r400-Nahkampftest und Analog-Gegenwehr. Vorhandene importierte Spieldaten bleiben verwendbar. Keine APK oder Disc-/Spieldaten enthalten.

## Prüfung dieses Reparaturpakets

Die beiden ursprünglichen Archive wurden aus den gespeicherten Lieferdateien wiederhergestellt. CRC-Prüfung, vollständige Dateiliste und bytegenauer Abgleich aller Produktions-/Testdateien bestanden. Bei allen überlappenden Dateipfaden wurde M23 priorisiert; insbesondere CMake, JNI-Brücke, Szenenrenderer und Version23. M22-Umweltquellen, Header, Java-Anbindung und Tests sind vollständig enthalten. Der Quellcode entspricht dem bereits gebauten M23-Stand plus den vorher vorausgesetzten M22-Dateien.

In diesem Reparaturlauf wurde kein neuer Android-Build ausgeführt. Das reparierte ZIP beseitigt die konkret im Log genannten fehlenden Quellen; falls danach ein weiterer Fehler erscheint, bitte dessen vollständige Buildausgabe senden.
