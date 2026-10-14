# M37 Buildfix – CMake/Ninja-Regenerationsschleife

Den Inhalt dieses ZIPs über den vollständigen M37-Stand entpacken und `app/build.gradle` ersetzen. Anschließend wie gewohnt `assembleDebug` in Android Studio starten. Das ZIP enthält die vollständige Datei, keine Diff-/Einfügeanleitung. Version bleibt0.37.0-dev/VersionCode38.

## Ursache und Korrektur

Der bereitgestellte Log bricht mit `manifest 'build.ninja' still dirty after 100 tries` ab. CMake konfiguriert dabei100-mal erfolgreich; es ist kein C++-Compilerfehler. Das M37-ZIP enthält die `CMakeLists.txt` mit dem ZIP-Zeitstempel06.10.2026,11:38:34, also nach dem Zeitpunkt der Buildmeldung11:31. ZIP-Zeitstempel enthalten keine Zeitzone. Ein dadurch in der Zukunft liegender CMake-Eingang lässt Ninja sein Manifest immer wieder als veraltet erkennen.

`app/build.gradle` enthält nun `normalizeNativeSourceTimestamps`. Diese Vorbereitung läuft vor sämtlichen `configureCMake*`- und `buildCMake*`-Aufgaben, auch bei inkrementellen Builds mit bereits aktueller Konfiguration. Sie korrigiert ausschließlich zukünftige Änderungszeitpunkte unter `app/src/main/cpp` auf die aktuelle Rechnerzeit. Dateiinhalte und bereits normale Zeitstempel bleiben erhalten. Die normalen CMake-/Ninja-Abhängigkeitsprüfungen bleiben aktiv.

Bei fehlenden Schreibrechten nennt die Vorbereitung die konkrete Datei. Signierdateien werden nicht gelesen oder verändert.

## Prüfung

Der ursprüngliche Fehler wurde in einer isolierten Android-Projektkopie mit einer um eine Stunde vordatierten `CMakeLists.txt` identisch reproduziert:100 CMake-Regenerationen, anschließend derselbe Ninja-Abbruch. Mit dem Fix wurden zukünftige CMake-, C++- und Header-Zeitstempel korrigiert; `assembleDebug` und `lintDebug` bestanden. Der Änderungszeitpunkt einer normalen Quelldatei blieb exakt erhalten. Auch der unveränderte Folgebuild bestand. Eine echte CMake-Änderung wurde anschließend erfolgreich übernommen und unabhängig im erzeugten Cache nachgewiesen; `RERUN_CMAKE` bleibt im Ninja-Manifest aktiv. Prüfung mit Gradle8.11.1/AGP8.9.2/CMake3.22.1/NDK28.2 und ARM64-Android-Toolchain auf Linux, ausgehend vom bereitgestellten Windows-Log.

Referenzen: [Ninja-Fehlerbericht zu zukünftigen Zeitstempeln](https://github.com/ninja-build/ninja/issues/1704), [CMake-Dokumentation der Regenerationsprüfung](https://cmake.org/cmake/help/latest/variable/CMAKE_SUPPRESS_REGENERATION.html). Die Regenerationsprüfung wird in diesem Fix nicht abgeschaltet.
