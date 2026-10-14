# M37 – Prüfungen und Grenzen der Nachweise

Referenz: bereitgestellte tatsächliche Disc1-Daten und eingefrorener Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7`. Die Rohdaten/Filme werden nicht mit diesem Änderungs-ZIP verteilt. Die nachfolgenden Assetprüfungen verwenden die neuen Produktionsbesitzer, nicht Ersatzmodelle oder erfundene Spawns.

## Build und integrierte Abläufe

Der saubere Android-ARM64-Build mit Gradle8.11.1, AGP8.9.2, SDK35, NDK28.2 und CMake3.22.1 besteht `assembleDebug` und `lintDebug`. Lint meldet0 Fehler und30 Warnungen. Unveränderte importierte Mathematikdateien erzeugen zusätzlich Compilerwarnungen; sie werden nicht als warnungsfrei bezeichnet.

Der kanonische Gesamtcheck `tools/test_host.sh` besteht mit allen43 CTests, Java-Verträgen,69.382 Helligkeitsprüfungen und dem unveränderten Originalinventar58 Teile/43 Waffenzeilen/77 Rezepte. Die vollständige Leon-Quelleninventur bleibt91 Dateien/1.820 Definitionen/54 Dispatchtabellen, davon1.692 nicht übertragen und128 teilweise angebunden. Einzelne neu portierte Abläufe rechtfertigen keine pauschale Vollständigkeitsbehauptung.

| Produktions-JNI/GLES-Prüfung | Bestandener Umfang |
| --- | --- |
| Titel, Audio, r100-Kampf, Truck, Funk, Gruppenmodus, Miss, Pause/EGL und getrenntes Erkunden |1.016 Checks|
| Natürliche Engine-Introfolge, Skip, Raumtransaktion, Pause/EGL |67 Checks|
| Tatsächliche Nebenereignisse, Leon, Kamera/MDT, Funk und persistente Modelle bei EGL-Neuanlage |199 Checks|
| Schreibmaschinen-AEV→Speicherbild→neue Sitzung→Laden→EGL und beschädigte Datei |42 Checks|
| Originale Treffer-/Abschusszählung, absolute Spielzeit, Pause, EVD-/Funk-SST-Takte und tatsächliche Kartenserialisierung |48 Checks|
| Ereignisrenderer mit tatsächlichen Originalcuts und persistenter GPU-Ownership |1.525.061 Checks|
| Beide tatsächlichen Funkdialoge, Porträts, Text, DSP, Stream-Wartezeit/START, Pause und Seitenverhältnisse |1.370 Checks|

## Originaldaten und unabhängige Quellvergleiche

| Bereich | Bestandene Prüfung |
| --- | --- |
| r120s00/r120s01/r100s40 |71.652.856 Checks über5.505 Frames/32 Cuts|
| Opening-Effekte |5.329.415 Checks|
|236 SST-Records/17 Bänke/14 EAR |7.918.830 Checks|
| r100-Oberflächentreffer |74.019 Checks;3 Originaltabellen/5 Funktionskörper bytegleich|
| Falleneffekte |62.246 Checks, einschließlich7.628 Blutbeobachtungen|
| Hund/Fallen |285.653 CPU/152.484 GLES|
| Originale r100 DoorCk/WindowBreakCk |32.389 Checks|
| Vögel |75.591 Checks|
| Gegenstandseffekte |4.508 CPU/24 GLES|
| Frühe EM12-Wahrnehmung/Route/Posen |18.343 Checks;3 Awareness-/Peer- und6 Route-Funktionskörper quellgeprüft|
| Gemischte Gruppentreffer einschließlich Schuss/Messer, Tiere, Objekte und EAT |5.928 Checks|
| Vorhandener Encounter-/Treffervertrag |3.386 Checks|
| Trockene Gegner-SEQ/Schritte/Tod und standalone Treffercapture |8.527 Checks|
| Waffenabweichung/Schuss-RNG |393.831 Checks|
| Augen/Radio |1.576.049 Checks|
| Tatsächliche frühe Waffen-Soundaufrufe/Positionen bei30/60/120Hz |907 Checks|
| Stream-Prepare/Play/Stop/Fade |88.910 Checks|
| Vollständige START-GAME-Sprache |82.091 Checks,226.080 PCM-Frames/4,71Sekunden|
| Originaler seqSeCtrl über alle65.536 SE/Free-Kombinationen |196.576 Vergleiche; kein neuer vollständiger127-Raum-FSE-Durchlauf|
| Klippenfiguren |713 CPU/50 GLES;673 Originalticks/277.276 Pose-/Root-Floats, maximale Rootabweichung0|
| Cloth |184 CPU/34 GLES;526.972 unabhängige Originalvergleiche|
| Fokus |7.193 GLES/5.256 ursprüngliche Filter01-Vergleiche|
| Spiegel |342 ursprüngliche RenderTex-/Projektionsvergleiche|
| Source-Spielzeit/Schusszähler |13.840 eingefrorene Funktionsvergleiche/8 Clock-Grenzfälle|
| Tatsächliche Nebenereignisse einschließlich unabhängiger SDK-Funkmatrix |14.541.503 Checks|
| Originales Kartenformat/nativer Zustand |68 CPU/65.659 Originalbyte-/Quellenprüfungen|

Java-Verträge bestehen: Disc8, Controller11, Menünavigation80, Layout7.432, Startablauf19, Optionen17, Sprachende14, Präsentation86, LoadGate24, RoomRepository76 und Raumkatalog401. Die neuen20-Slot-Speicherverträge bestehen53 Prüfungen zu vollständigen Headern/CRC, beschädigten Dateien und atomarer Übernahme anhand der vom aktuellen nativen Originalcodec erzeugten64KiB-Fixture.

## Wiederholung

`bash tools/test_host.sh` baut die kanonischen Hosttargets, führt CTest, Java-Verträge sowie Inventar-/Leon-Quelleninventur aus. Optionale GLES/JNI-Targets werden durch `RE4_BUILD_GL_CHECK`, `RE4_GLES_INCLUDE_DIR`, `RE4_GLES_LIBRARY`, `RE4_EGL_LIBRARY` und `RE4_JNI_INCLUDE_DIR` aktiviert. Die tatsächlichen Datentests verlangen eigene importierte Originalressourcen; jeder Test nennt seine Argumente bei fehlender Eingabe. Native Codecs/Quelloracles, Bild-/GL-Prüfungen und Java-Verträge sind getrennte Nachweise.

## Praktische Grenzen

Es wurde kein neues physisches Android-/Adreno-Gerät geprüft. Der Host verwendet Mesa/EGL/GLES; seine bestandenen Wiederherstellungsprüfungen beweisen nicht jeden herstellerspezifischen Decoder-/GPU-/Launcher-Lifecycle. Das bereitgestellte Icon ist bytegleich als JPEG-Ressource übernommen; Ressourcenkompilierung und adaptive/round-Manifestverweise sind gebaut, ein Launcher-Screenshot vom Gerät steht aus.

Hohe Checkzahlen entstehen überwiegend aus sämtlichen Modellkoordinaten, Frames und Grenzfällen. Sie sind kein Beweis für vollständige Gameplay-Parität. Unportierte globale Kamera-/Kollisions-/Gegner-/Speichermanager und GX/GLES-Unterschiede bleiben in [UPDATE-M37.md](UPDATE-M37.md), [R100-SOURCE-INVENTORY-M37.md](R100-SOURCE-INVENTORY-M37.md) und den Modulnachweisen ausdrücklich aufgeführt.
