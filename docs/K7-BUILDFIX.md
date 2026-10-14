# K7 – Buildfix mit vollständigen Dateien

Stand: 7. Oktober 2026. Android-Version **0.37.7-dev**, Versionscode **40**.

## Ursache des gemeldeten Fehlers

Die bisherige K7-Korrektur-ZIP enthielt die Änderungen gegenüber K6 und setzte dessen Dateien voraus. Das übermittelte Buildprotokoll zeigt, dass diese Abhängigkeiten im gebauten Projekt fehlen:

- `actor_gl.cpp` und `scene_gl.cpp` finden `model_textures_gl.h` nicht.
- `room_lighting_gl.h` verwendet `RoomLightCut::lodBias`, das im vorhandenen `room_lighting.h` fehlt.

Die Textur-LOD-Verarbeitung benötigt außerdem `minimumLod`, `maximumLod` und `anisotropy` sowie den passenden Parser in `room_lighting.cpp`. Diese zusammengehörigen Dateien sind jetzt vollständig enthalten. Die Renderingkorrekturen bleiben Teil des K7-Quellstands.

## Inhalt und Einbau

Diese ZIP enthält **54 vollständige Code- und Testdateien**: 42 Dateien unter `app/` und 12 unter `tests/` beziehungsweise `tools/`. Sie führt die geänderten Dateien aus K5, K6 und K7 in ihrem aktuellen K7-Stand zusammen. Ein separates Einspielen der K6-Korrektur-ZIP ist damit nicht erforderlich.

1. Die ZIP entpacken.
2. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden R100-Projektordner zusammenführen und vorhandene Dateien ersetzen. Der Projektordner soll dabei nicht ein zweites Mal innerhalb des bestehenden Projektordners angelegt werden.
3. In Android Studio das Projekt synchronisieren und erneut bauen.

`FILES-K7-BUILDFIX.sha256` enthält die SHA256-Prüfsummen aller Code-, Test- und Berichtsdateien dieses Pakets.

## Prüfung dieses Pakets

Für die Prüfung wurde eine frische Projektkopie aus dem vorhandenen Quellcode-Zwischenstand erstellt und mit den Dateien der neuen kumulativen ZIP ergänzt. **Alle 516 geprüften Build-Eingaben** unter `app/`, `tests/` und `tools/` stimmen mit dem zuvor erfolgreich gebauten K7-Quellstand überein.

Anschließend wurden genau die vier Aufgaben aus dem übermittelten Buildprotokoll ausgeführt:

```text
:app:clean
:app:assembleDebug
:app:assembleDebugUnitTest
:app:assembleDebugAndroidTest
```

Ergebnis: **BUILD SUCCESSFUL in 2m 24s**, 66 Aufgaben, davon 65 ausgeführt und eine aktuell. Gebaut wurde für Android ARM64 mit NDK **28.2.13676358**, CMake **3.22.1**, Gradle **8.11.1** und JDK **17**.

Dieser Neuaufbau erfolgte unter Linux. Die Unit-Test- und Android-Testpakete wurden dabei gebaut; diese Assemble-Aufgaben führen die Tests selbst nicht aus. Eine Geräteprüfung oder vollständige visuelle Abnahme der Spielszenen war nicht Teil dieses Buildfixes.

