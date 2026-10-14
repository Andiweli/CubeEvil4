# K17 Fix3 – aktueller Prüfstand, 2026-10-08

VersionCode 52 / `0.37.17-fix3`. Originalabgleich, aktuelle Originaldaten-/JNI-/GLES-Ergebnisse und Build-/Gerätegrenzen: [K17-FIX3.md](K17-FIX3.md), [K17-FIX3-PRUEFUNGEN.json](K17-FIX3-PRUEFUNGEN.json) und [K17-FIX3-TESTAUSGABEN.txt](K17-FIX3-TESTAUSGABEN.txt). Die folgenden Abschnitte beschreiben historische Prüfstände.

# K17 Fix2 – aktueller Prüfstand, 2026-10-08

VersionCode 51 / `0.37.17-fix2`. Ursachen, vollständige aktuelle Ergebnisse, Originaldaten-/JNI-/GLES-Prüfungen und Build-/Gerätegrenzen: [K17-FIX2.md](K17-FIX2.md), [K17-FIX2-PRUEFUNGEN.json](K17-FIX2-PRUEFUNGEN.json) und [K17-FIX2-TESTAUSGABEN.txt](K17-FIX2-TESTAUSGABEN.txt). Die folgenden Abschnitte beschreiben historische Prüfstände.

# M36 validation, 2026-10-05

Android 37/0.36.0-dev, assembleDebug/lintDebug bestanden, 0 Fehler/0 Fatal/27 Lint-Hinweise, 51 JNI-Methoden, ARM64 und 16KiB ELF-/APK-Ausrichtung. Kanonisches tools/test_host.sh und alle 38 CTest-Gruppen bestanden. Tatsächlicher kombinierter JNI-/GLES-Lauf: 598 Prüfungen bestanden. Die Originaldaten-, Renderer- und Ladezeit-Ergebnisse sowie verbleibende Grenzen stehen in [VALIDATION-M36.md](VALIDATION-M36.md) und [UPDATE-M36.md](UPDATE-M36.md). Kein neuer Android-/OUYA-Gerätelauf.

# M35 validation, 2026-10-05

Actual ARM64 Android clean build and final assembleDebug/lintDebug pass: versionCode 36, versionName 0.35.0-dev, 0 lint errors/0 fatal/25 warnings, 49 matching JNI exports, ELF LOAD and APK ZIP 16 KiB alignment. No device run. All 36 CTest groups and canonical tools/test_host.sh pass. Original-data and GLES results: player 4727, context 9093, water 194, unchanged foot-sound oracle1,286,419, r100 413, mirrored axe 134 complete poses, axe GLES 14, title assets 446, menu layout 6872/navigation 80, startup 18/options 17, brightness 69,382 plus 567 original coefficients, production combined JNI/GLES 100. See [UPDATE-M35.md](UPDATE-M35.md) for scope, source evidence and explicit remaining original-parity gaps.

# M34: aktueller Prüfstand (2026-10-05)

VersionCode35 / `0.34.0-dev`, `com.ast.re4decomp`, ARM64/API21/GLES3. Der abschließende `clean assembleDebug lintDebug`-Lauf nach sämtlichen Änderungen besteht mit Gradle8.11.1, AGP8.9.2, SDK/BuildTools35, NDK28.2.13676358/Clang19 und CMake3.22.1. Lint:0 Fehler,18 Hinweise. Der gesamte Hoststand wurde mit GCC13 und den Originalmotion-Routinen neu gebaut; nach den letzten Änderungen meldet Ninja keinen weiteren Arbeitsauftrag. Ein Android-/Adreno-Gerätelauf wurde nicht durchgeführt.

| M34-Prüfung | Ergebnis |
| --- | --- |
| Gemeinsame native CTest-Vertragsgruppen |36/36 bestanden |
| Sauberer tatsächlicher ARM64-Android-Build und Lint |bestanden;0 Lint-Fehler |
| APK-Version, APIs, ABI |35/0.34.0-dev,21/35,nur arm64-v8a |
| Generierter Java-JNI-Header gegen Produktionsbridge |alle40 Methodensignaturen stimmen überein |
| ELF-/APK-Ausrichtung |16KiB LOAD-Segmente; `zipalign -P16` bestanden |
| Leon-Schrotflintenablauf gegen Original-FCV/SEQ |1010 Prüfungen; Loslassen zu verschiedenen Frames, gehaltenes Zielen und Feuerkadenz |
| Vorhandene drei Originalwaffenprofile |5345 Daten-/Simulationsprüfungen bestanden |
| Tatsächliches Mündungsfeuer mit gelöster Waffenmatrix |52148 GLES-Prüfungen; Translation/Drehung/Pause/EGL/Snapshot |
| Originalfenster-/Leiter-/Zaunpfade |9070 Prüfungen,24 wiederholte Ein-/Austritte und Originalbewegungsvergleiche |
| Tatsächlicher Controller-/JNI-/GLES-Aktionsweg |5696 Prüfungen einschließlich kaltem r400-Start und bereits zerbrochenem Fenster |
| Profilkonstruktoren aus beiden Discs |alle127 Archive ohne Konstruktorfehler;410 Kisten,62 Fässer,918 freie Funde,68 Fenster,26 Türen |
| Tatsächliche gebundene ITM-Modellschichten aller Räume |127 bestanden;363 Besitzer,1117 vorhandene und128 quellengemäß modelllose Kandidaten in23 Räumen;Snapshot/RNG unverändert |
| ITM-Vertrag mit gemischten vorhandenen/fehlenden Modellen |r101/r400 bestanden;stabile Besitzer-/Drawindizes,keine erfundene Geometrie |
| Tatsächliche raumübergreifende Schatzflags |39 Prüfungen, u.a. r103/r113 gemeinsames Flag7; alter Snapshot und erneutes Laden |
| Bestehende Pickup-/Raum-/Sprung-Assetregression |8462 Prüfungen bestanden |
| Originalmenü-, Koffer- und Portraitdaten |16687 Prüfungen; alle vier Koffergrößen und drei bewaffnete Portraits |
| Tatsächliche originale Gegenstandsansichten |1069 GLES-Prüfungen;58 IDM-Modelle,32 sichtbar reflektierende Modelle |
| Waffenwechsel und Koffer über Produktions-JNI/GLES |627 Prüfungen bestanden |
| Tatsächliche JNI-PCM-/Audio-Lifecycle-Regeln |71 Prüfungen einschließlich nicht blockierender UI-Gates und erstem Menüsound |
| Unveränderte Espgen42/45_Move00 gegen Port |3687300 exakt gleiche Float-Werte;maximale Abweichung0 |
| Alle Raumumgebungen und tatsächliches Wasser-GLES |1216930 Prüfungen;127 Cut0,123 unterstützte Geometrien,20 Startgitter |
| r10a/r10b an originalen Wasserstartpunkten |7660/52247 sichtbare Wasserpixel;Animation,Pause und neuer EGL-Kontext bestanden |
| Abschließender vollständiger Produktions-JNI-/GLES-Raumkatalog |1173 Prüfungen;alle123 unterstützten Räume gerendert,122 geerdete Starts,keine Behälter-Loaderwarnungen |
| Reine Java-Verträge, frisch mit JDK17 |Disc8,Controller11,Menünavigation80,Ladegate24,Repository76,Katalog401;600 gesamt |
| Quelleninventar und Dispatchtabellen |91 Leon/Waffen/Item-Dateien,1820 Definitionen,54 Tabellen;alle104 ET/64 Objekt/21 AEV-Registrierungen erfasst |

Die Millionen Wasserprüfungen sind Float-/Mesh-/Pixelvergleiche innerhalb der übertragenen Routinen, keine Millionen Spielszenarien. Die vollständige Quelleninventarisierung belegt ebenfalls keine vollständige Portierung:122 Definitionen haben ausdrücklich beschriebene Teiladapter-Zuordnungen;1698 sind nicht übertragen oder nicht nachgewiesen. Allgemeine Szenarioskripte, Kamera-/Licht-Cut-Wechsel, vollständige GX-/TEV-/Schatteneffekte, weitere Leon-/Waffen-/Objektaktionen sowie die vollständige Menüanimation fehlen weiterhin.

Der letzte Wasservergleich berücksichtigt den Quellvorlauf: `EspGenLoopMove` ruft200 Bewegungen mit unverändertem `Game.Frame_cnt` auf. Rauschen und Pufferparität bleiben während dieses Vorlaufs konstant. Die normalen folgenden240 Frames werden mit dem jeweils nächsten globalen Spieltick verglichen. Rechteckige Raster und alle drei Wellentypen beider Generatoren sind enthalten.

Die Grafik-Sichtprüfung verwendet echte gerenderte r10a/r10b-Szenen an den unveränderten Debug-Wasserstartpunkten Point1 und eine Hostkomposition der finalen Kofferansicht. Point0 liegt in beiden Wasserräumen hinter Gelände; korrekt verdecktes Wasser wurde nicht durch eine frei erfundene Position sichtbar gemacht. Die Menükomposition ist kein Android-Screenshot. Spielgrafiken und Werkzeugketten gehören nicht zum Änderungs-ZIP.

## M34 reproduzieren

```text
gradlew.bat clean assembleDebug lintDebug
ctest --output-on-failure
subscreen_data_tests DISC1_FILES
m34_player_weapon_flow_check PL00 WEP01 WEP02 WEP07 CORE_DRS R101
m32_weapons_check PL00 WEP01 WEP02 WEP07 CORE_DRS R101
m32_muzzle_gl_check WEP01 WEP02 WEP07 CORE_DRS
m33_context_actions_check R101 R400 PL00 WEP01
m33_context_runtime_check R101 R400 PL00 WEP01 roominfo CORE_DRS EM12 emleon00
m34_interaction_profiles_check ALL_127_DECODED_ROOMS PL00
m33_subscreen_gl_check DISC1_FILES
m34_water_check CORE_DAT ALL_127_DECODED_ROOMS roominfo OUTPUT_DIRECTORY
python3 tools/check_water_reference.py --build HOST_BUILD
python3 tools/audit_player_actions.py --check
```

`CORE_DRS` bezeichnet das ursprüngliche CORE-DAS/DRS mit seinem0x400-Containerheader, `CORE_DAT` dessen dekodierten DATA-Teil ohne diesen Header. Audiofixtures benötigen den vollständigen ursprünglichen Container einschließlich DVD-Teil und die Importpfade `disc1/files` beziehungsweise `disc2/files`. Für Produktions-GLES-/JNI-Ziele CMake mit `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR` auf den JDK-Includeordner konfigurieren. Die Optionalziele werden nicht mit der App ausgeliefert.

Quellen, genaue Funktionsgrenzen und gezielter Gerätetest: [UPDATE-M34.md](UPDATE-M34.md), [PLAYER-ACTION-COVERAGE-M34.md](PLAYER-ACTION-COVERAGE-M34.md), [M34-INTERACTIONS-AUDIT.md](M34-INTERACTIONS-AUDIT.md), [M34_ENVIRONMENT_PARITY.md](M34_ENVIRONMENT_PARITY.md), [MENU-M34.md](MENU-M34.md), [WEAPONS-M34.md](WEAPONS-M34.md).

---

# M33: historischer Prüfstand (2026-10-05)

VersionCode34 / `0.33.0-dev`, `com.ast.re4decomp`, ARM64/API21/GLES3. Dieser Stand wurde mit den echten Android-Werkzeugen gebaut: Gradle8.11.1, AGP8.9.2, SDK/BuildTools35, NDK28.2.13676358/Clang19 und CMake3.22.1. `assembleDebug` und `lintDebug` bestanden; Lint:0 Fehler,17 Hinweise. Ein Android-/Adreno-Gerätelauf wurde nicht durchgeführt. Die Originaldiscdaten und installierten Werkzeugketten sind nicht im Änderungs-ZIP.

| M33-Prüfung | Ergebnis |
| --- | --- |
| Frisch gebaute CTest-Vertragsgruppen |36/36 bestanden |
| Tatsächliches ARM64-Debug-APK + Android-Lint |bestanden |
| APK-Paket/Version/API/GLES/ABI |com.ast.re4decomp,34/0.33.0-dev,21/35,GLES3,nur arm64-v8a |
| Generierter Java-JNI-Header gegen Produktionsbridge |alle40 Methodensignaturen übereinstimmend |
| Native ELF-Ausrichtung und APK-Zipalign |16KiB LOAD-Segmente; `zipalign -P16` bestanden |
| Originalleiter-/Fenster-/Zaunregeln mit r101/r400/PL00 |4276 Prüfungen bestanden |
| Aktionskamera und Renderinterpolation mit r101/PL00 |4317 Prüfungen bestanden |
| Tatsächlicher Controller-/JNI-/GLES-Aktionsweg |2511 Prüfungen;3 Hausleiterzyklen, Paneintritt, Priorität, Pause/EGL und r400-Ganado-Fenster |
| Reale Ingame-Gegenstandsansicht,58 Originalmodelle |1069 GLES-Prüfungen; vollständige Silhouetten, Reflexion, Pause/EGL |
| Originalmenü-/Schrift-/Dokument-/Specular-Daten |16666 Prüfungen;5 Menügrafikpakete,58 Modelle/Symbole,256 Titel,156 englische Dateiseiten |
| Originalbereichskarten |17 Listen mit113 Originalzuordnungen und korrekten Grenzräumen |
| Original EM12/cEm10-Ton-/Blutaufträge |159380 Daten-/Simulationsprüfungen bestanden |
| Tatsächlicher Blut-GLES-Renderer |alle6 EST-Aufträge/22 Animationen sichtbar; Tiefe/GL-Zustand/Pause/EGL bestanden |
| Reine Java-Verträge |Disc8,Controller11,Menünavigation32,Repository76,Katalog401 bestanden |
| Gesamte Produktions-Java-Oberfläche |echte SDK35-Kompilierung bestanden |

Der GLES-Bluttest enthält9113087 Einzelprüfungen einschließlich framebufferweiter Pixelvergleiche. Das sind keine Millionen Spielszenarien: der Nachweis umfasst die sechs übertragenen Spritzer-/Streifen-/Bodenblutaufträge, Verdeckung durch nähere Geometrie, unverändertes Alpha und Wiederherstellung des pausierten Bildes im frischen EGL-Kontext. Die zusätzliche Tonzufallsquelle lässt die bestehenden Kampfsimulationen unverändert.

Die Gegenstandsprüfung zeichnet alle58 originalen IDM-Modelle einschließlich Quelllicht und Specular-Materialien. Eine reflektionslose Vergleichsausgabe unterscheidet sich bei32 Modellen sichtbar; kein pauschaler Helligkeitsaufschlag ersetzt die Quelltexturen. Die Silhouetten bleiben vor/nach Drehung innerhalb des Modellfensters. Originalreflexionen werden bei gleichen Materialien zwischen Draws geteilt.

Der Produktionsaktionslauf entdeckte die falsche Priorität am Hausfenster: Ein gültiges Leiterumwerfen wurde als Fenstersprung gestartet. Die Bridge verwendet jetzt Quellpriorität5 für Objektleitern vor Priorität2 für Fenster; der tatsächliche Controllerweg ist als Regression erhalten.

Die Kamera-/Aktionsprüfungen verwenden echte FCV/SEQ-/SAT-Daten. Darstellungsglättung verändert keine Ereigniscounter. Die eingefrorene Originalquelle ist `upstream/re4`, Commit `9b76ff003081d6ece8b838453fb8310743a972c7`. Relevante Pfade: `game/obj13.cpp`, `game/pl_class.cpp`, `game/emwindow.cpp`, `game/player.cpp`, `game/examine.cpp`, `game/trans.cpp`, `game/sscrn.cpp`, `Sscrn/ss_main.cpp`, `Sscrn/ss_pzzl.cpp`, `Sscrn/ss_item.cpp`, `Sscrn/ss_file.cpp`, `Sscrn/ss_map.cpp`, `em10/em10.cpp`.

## M33 reproduzieren

Native Vertragsgruppen: CMake mit `app/src/main/cpp` konfigurieren, bauen und `ctest --output-on-failure` ausführen. Für tatsächliche Produktions-GLES-/JNI-Prüfungen zusätzlich `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR` auf den JDK-Includeordner setzen. Optionalziele werden nicht mit der App ausgeliefert. Die folgenden Pfade zeigen auf eigene lokal importierte Originaldaten; CORE muss dekodiert bereitliegen.

```text
subscreen_data_tests DISC1_FILES
m33_subscreen_gl_check DISC1_FILES [IMAGE_PREFIX]
m33_context_actions_check R101 R400 PL00 WEP01
m33_traversal_camera_check R101 PL00 WEP01
m33_context_runtime_check R101 R400 PL00 WEP01 roominfo CORE EM12 emleon00
enemy_feedback_check EM12 CORE
m33_enemy_gl_check EM12 CORE
```

Die Android-App verwendet weiterhin ausschließlich lokale Discimporte. Host-GLES-Prüfungen mit Mesa ersetzen keinen Test auf dem Zielgerät. Quellen- und Funktionsgrenzen sowie der gezielte Gerätetest stehen in [UPDATE-M33.md](UPDATE-M33.md).

---

# M32: historischer Prüfstand (2026-10-05)

VersionCode33 / `0.32.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Die aktuelle native Plattform, Parser, Originalmotion und JNI-/GLES-Renderer wurden gemeinsam neu gebaut: C++17, GCC13.3, Assertions, Mesa/EGL/GLES3. **Kein aktueller Android-ARM64-/Gradle-/NDK-/Lint-/Adreno-Gerätelauf.** Java-Syntaxprüfung ist kein Android-API-Typcheck. Die Discdaten liegen außerhalb des Projekts und gehören nicht zum ZIP.

| Aktuelle M32-Prüfung | Ergebnis |
| --- | --- |
| Neu gebaute CTest-Vertragsgruppen | 33/33 bestanden |
| Waffen: PL00/WEP01/WEP02/WEP07/CORE/r101 | 5347 Originaldatenprüfungen bestanden |
| Mündungsfeuer: tatsächliche GLES-TPL/ANM-/Blend-/EGL-Ausgabe | 49299 Prüfungen bestanden |
| Fensterkamera/FCV/Glassplitter und ItemExam | 25217 unabhängige Originaldatenprüfungen bestanden |
| Fenster: echte Controller-/JNI-/GLES-Aktionen | 3696 Prüfungen bestanden |
| Hausleitern: sieben ET06/ET08-Profile | 1464 Originaldatenprüfungen bestanden |
| Hausleitern: tatsächlicher JNI-/GLES-Spielweg | 670 Prüfungen bestanden |
| Raumlicht/Fußschatten/Esp11-Licht: GLES und alle127 LIT | 184 Prüfungen bestanden |
| MIDI/Instrumente/ESE/SHD, alle23 Sequenzen und307 Streams | 1648 Originaldatenprüfungen bestanden |
| Audio: tatsächliches JNI-PCM, Koffer/Hintergrund/EGL/Streamkontinuität | 68 Produktionsprüfungen bestanden |
| Koffer: echte Aufnahme, Waffenwechsel, Schuss/Nachladen/EGL | 627 Produktionsprüfungen bestanden |
| Koffer: native Transaktionen und5000 gemischte Packoperationen | bestanden |
| Vergleich mit `puzzle.cpp`/`item.cpp` | 58 Formen /43 Waffenzeilen /77 Rezepte übereinstimmend |
| M31-Katalog: neuer gemeinsamer CPU-/JNI-/GLES-Build | 1444 /1173 Prüfungen bestanden |
| Tatsächlich geöffnete/gezeichnete Modellarchive | 123 unterstützt, 122 geerdete Starts |
| Neu verlinkte M27/M29/full bridge Regressionen | 774 /209 /2941 Prüfungen bestanden |
| Frischer gemeinsamer ASan+UBSan-Build | Waffen5347 /Aktionen25217 /Leitern1464 /Audio1648 /Katalog1444 /Koffer/Items bestanden |
| Zusätzliche manuelle Vertragsgruppen | 30 Gruppen bestanden; korrigierter letzter Kofferlink separat ausgeführt |
| Java Disc /Controller /Repository /Katalog | 8 /11 /81 /401 Prüfungen bestanden |
| Produktions-Java-Syntax | 14 Dateien geparst |
| XML-/String-Verweise | 5 XML-Dateien gültig, 64 Stringverweise verfügbar |

Die tatsächlichen Fensterläufe vergleichen jede Original-Eye/Target/FOV/Roll-Pose und Rootbewegung. Bruch15/16/13, EFM-Auftrag und reale GLES-Fragmente kommen aus derselben Ereignisnummer. Pause und Kontextneuerstellung erhalten CPU-Partikelzustand und exakte Bildpixel. Die Gläser verwenden die transparenten Quellen-Typen; Holz bekommt keinen erfundenen Alpha-Wert. Die Leitertests prüfen Start-/Endböden und Kollisionskörper statt beliebiger Höhenkorrekturen.

Die Waffenprüfungen enthalten beide neuen Quellen-WEPs, Kapazitäten, Ziehen/Feuer/Nachladen, Handbindung, 13 Schrotstrahlen und atomare Objektklassifizierung vor Zerstörung. Mündungseffekte behalten echte Texturen, Animationen, Quellblending und Zielalpha. Esp11 hellt im isolierten tatsächlichen Renderer71116 Pixel auf, ohne Billboardpass, und erlischt im Originaltakt. Der Fußschattenpass ist in104 Anfangs-Cuts verfügbar. Diese Ergebnisse belegen nicht sämtliche GX-/TEV-/Cut-/Cast-/Filtereffekte.

Die Kofferprüfung betätigt den tatsächlichen A-Controllerweg an ITA.no138, bestätigt/annulliert die Aufnahme, ignoriert alte Antworten und behält Flags/Mengen nach EGL. Ein pausierter Waffenwechsel erfolgt als CPU-Transaktion ohne aktuellen Grafik-Kontext; GPU-Ressourcen werden erst beim Zeichnen erneuert. Schießen/Nachladen teilen sich die native Magazin-/Reserveautorität. Dokumente benötigen keinen optionalen sichtbaren ITM-Besitzer; dieser Kompatibilitätsfall nutzt explizit die originalen ITA-Daten bei fehlender ITM-Schicht. HP∞ verhindert Heilmittelverbrauch bei voller Gesundheit.

Der frische gemeinsame Sanitizer-Build instrumentiert die gesamte Plattform-/Motion-/Parserbibliothek, nicht nur die Prüfhülle. `ASAN_OPTIONS=detect_leaks=0`, UBSan stoppt beim ersten Fehler. Kein LeakSanitizer-/Android-Sanitizernachweis wird behauptet. Originaldaten- und Katalogläufe wurden nach dem letzten gemeinsamen Header-/ABI-Stand neu verlinkt.

## M32 reproduzieren

EGL-/GLES-/JNI-Hostziele mit `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR` konfigurieren. Die Produktions-App bekommt diese Testschalter nicht. Originaldateien lokal bereitstellen; R100 benötigt seine fünf rohen BLK-Dateien neben dem dekodierten Raum. Der optional angegebene Raumordner enthält die127 anhand `rooms.tsv` geprüften Archive. Audio verwendet den tatsächlichen Discimportbaum `disc1/files/`, einschließlich St1-Raum-DAS, CORE und BGM.

```text
m32_weapons_check PL00 WEP01 WEP02 WEP07 CORE R101
m32_muzzle_gl_check WEP01 WEP02 WEP07 CORE
m32_actions_check R101 PL00 WEP01 CORE
m32_action_runtime_check R100 R101 PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX
m32_ladders_check R101 R400 PL00 WEP01
m32_ladder_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00
m32_lighting_gl_check CORE PL00 WEP01 R101 ALL_ROOMS OUTPUT_PREFIX
m32_inventory_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 WEP02 WEP07 R100 OUTPUT_PREFIX
m32_audio_check DISC1_FILES R101 --all-streams
m32_audio_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 WEP02 WEP07 BGM_DIR R111 R113
```

Die ausführbare Source-Parität prüft `tools/check_inventory_source.py upstream/re4`. Die manuellen Pure-Java-Verträge und nativen Basiskontrakte sind zusätzlich über `tools/test_host.sh` reproduzierbar. Dokumentierte Portgrenzen stehen in [UPDATE-M32.md](UPDATE-M32.md), [LIGHTING-M32.md](LIGHTING-M32.md), [AUDIO-M32.md](AUDIO-M32.md) und [INVENTORY-PORT.md](INVENTORY-PORT.md).

---

# M31: aktueller Prüfstand (2026-10-05)

VersionCode32 / `0.31.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. C++17/GCC13.3, Assertions aktiv, tatsächlicher Produktions-JNI-/GLES-Code mit Mesa/EGL/GLES3 und separat bereitgestellten Originaldaten. **Kein neuer Android-ARM64-/Gradle-/Lint-/Adreno-Gerätelauf**; Java-Syntaxprüfung ist ausdrücklich kein Android-API-Typcheck.

| Prüfung | Ergebnis |
| --- | --- |
| Neu verlinkte CTest-Vertragsgruppen | 32/32 bestanden |
| M31-Originaldaten einschließlich Raumkatalog-Audit | 1444 Prüfungen bestanden |
| M31-Originaldaten mit ASan+UBSan, frischer Build | 1444 Prüfungen bestanden |
| LIT-/Item-Verträge, normal und ASan+UBSan | jeweils 19 Prüfungen bestanden |
| M31 Produktions-JNI/GLES | 1173 Prüfungen bestanden |
| Automatisch geöffnete/gezeichnete Raumarchive | 123/123 unterstützt; 122 geerdete Spielerstarts |
| Untersuchte Start-Lichtarchive | 127/127 verwendbar; vier alte unregistrierte SMD-Archive ausdrücklich gesperrt |
| Java-Raumkatalog mit Originalzeilen | 401 Prüfungen bestanden |
| Java Disc / Controller / Raum- und Blockrepository | 8 / 11 / 81 Prüfungen bestanden |
| Produktions-Java-Syntax | 13 Dateien geparst |
| M30-Originaldaten / Produktions-JNI/GLES | 125 / 1871 Prüfungen bestanden |
| M29-Originaldaten / Produktions-JNI/GLES | 8462 / 204 Prüfungen bestanden |
| M27 Tür-/Fenster-/Rollback-JNI/GLES | 774 Prüfungen bestanden |
| M28 Leiter-JNI/GLES | 850 Prüfungen bestanden |
| Originale r101-Innenraumprüfung | 511 Prüfungen bestanden |
| Tatsächlicher r101-Laser-GLES-/Farbnachweis | 15 Prüfungen bestanden |
| XML und Java-Ressourcenreferenzen | XML gültig; keine fehlenden String-IDs |

Der Katalog-Audit verwendet alle 127 einzigartigen Raumdateien der tatsächlichen Discs; komprimierte und dekodierte Hashes wurden gegen `rooms.tsv` geprüft. 123 Modellarchive bauen die native statische Szene auf und werden anschließend durch echte JNI-/GLES-Aufrufe geöffnet/gezeichnet. Das ist kein Nachweis aller originalen GX-/TEV-/Filter-/Spezialeffekte. r30f besitzt im anfänglichen SAT-Teil keinen Laufboden: die Original-Baggerfahrt aktiviert weitere Teile im Raum-Task; der Browser kennzeichnet diese Grenze. r313/r314/r322/r323 fehlen in St3 `setTbl()` und besitzen alte nicht unterstützte SMD-Versionen; r324 ist ebenfalls unregistriert, sein Modellformat aber verwendbar. Die 108 registrierten Kampagnenräume, 14 Zusatzmodi und fünf unbenutzten Datensätze werden getrennt geprüft.

Alle 127 LIT-Start-Cuts sind verwendbar. Ältere Versionen, leere Cut-Offsets, alte EnableMask-Felder und mehr als 64 gespeicherte Lichter sind geprüft. Die 26 unbrauchbaren späteren r120-Cuts werden ausdrücklich als nicht verfügbar markiert; aktive Cut0-Werte werden nicht geraten oder ersetzt. Aktuell wird nur Cut0 gerendert. Die ursprüngliche strikte Ablehnung ungültiger Anfangsdaten bleibt geprüft.

Sieben reale Fensterwege führen vom echten Startboden über die drei Original-FCV-Richtungen auf echten Landeboden; Gegenrichtung, intakte Holzplanken, dekorative Fenster, Frame15/16/13, Bruchflags, sechs originale Spieler-Tonzeitpunkte, pausierte Zähler und unterdrückte Waffenaktionen sind enthalten. Ein Clip-Anhängen während eines laufenden Zielblends behält gegenüber einem unangetasteten zweiten Actor 60 spätere Posen und die Motion-/Hermite-Historie bytegleich. Wiederholtes Binden hängt keine Clips erneut an.

Im tatsächlichen JNI-/GLES-Weg löst A/Kreuz die Fensteraktion aus, LT/L2 zeichnet den Laser bereits während des Ziehens ab Frame4. Glasdraws erhalten wirklich keine Tiefenschreibfreigabe; das tatsächliche Bild zeigt die Geometrie hinter der Scheibe. Pause/neuer EGL-Kontext behalten Owner, Pose, Frame, Raumflags und identische Pixel. Nach r100→r101→r100 bleibt r100s Bruchflag in seinem eigenen Raum. Erkundungsmodus erzeugt keinen Gegner; der ausdrücklich ausgewählte r400-Test startet die existente echte Ganado-Instanz, behält sie über Kontextverlust und entfernt sie bei einer neuen Erkundungssitzung. Bilder von intaktem Fenster, früher Laseranzeige und Landung wurden visuell kontrolliert.

ASan und UBSan wurden auf der gesamten neu gebauten Plattform-/Motion-/Parserbibliothek und dem neuen Originaldatenprogramm ausgeführt. `ASAN_OPTIONS=detect_leaks=0`: keine LeakSanitizer-/Android-Sanitizer-Coverage behauptet. Die bekannte Java-Testwarnung über ein unbenutztes AutoCloseable-Handle bleibt unverändert. Die fehlenden separaten DVD-Audiodateien im Standalone-Kataloglauf sind erwartete Testumgebungshinweise, kein Voll-Audiotest. Die bisherigen absichtlichen GPU-/Raum-Rollbackfehler bleiben bestandene Fehlerinjektionen.

## M31 reproduzieren

Die Originaldaten selbst bereitstellen, keine Assets sind im ZIP. Für `R100` müssen die fünf rohen `r100_00.dat` bis `r100_04.dat` danebenliegen. Der optionale Katalogordner enthält die 127 anhand von `rooms.tsv` geprüften und Yz2-dekodierten `rXXX.dat`-Archive; rohe r100-Blöcke ebenfalls daneben. Eigene GL-Ziele wie bisher mit `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR` konfigurieren.

```text
m31_assets_check R100 R101 PL00 WEP01 CORE [ALL_127_DECODED_ROOMS]
m31_runtime_check R100 R101 R400 PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX [ALL_127_DECODED_ROOMS]
```

Die Pure-Java-Verträge werden durch `tools/test_host.sh` einschließlich `RoomCatalogTests rooms.tsv` ausgeführt. Für die fünf zusätzlichen Original-RoomRepository-Prüfungen `RoomRepositoryTest roominfo.dat rooms.tsv blocks.tsv` aufrufen. Für ASan+UBSan einen separaten Build mit `-fsanitize=address,undefined -fno-omit-frame-pointer` für Compiler/Linker verwenden. Host-/Testschalter werden nicht in der Android-App aktiviert.

## M31-Änderungspaket

Das ZIP ersetzt vollständige geänderte/neue Dateien über M30 unter `RE4-Android-Decomp/`. CRC, SHA-256 jedes enthaltenen Dokuments/Quell-/Testfiles, virtuelles vollständiges Overlay, unverändertes Upstream sowie Ausschluss von Assets/APK/Build-/Bytecode-Dateien werden geprüft. `FILES-M31.sha256` enthält die gelieferten Dateien außer dem Hashkatalog selbst. ZIP-Zeitstempel liegen bewusst vor der Auslieferung, um zukünftige CMake-Eingabedaten auf Windows zu vermeiden. Keine Löschungen sind erforderlich. Installation: [UPDATE-M31.md](UPDATE-M31.md).

---

# M30: Prüfstand (2026-10-05)

VersionCode31 / `0.30.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Native Hostkompilation: GCC13.3, C++17, `-O2 -UNDEBUG`; Assertions bleiben aktiv. JNI-/Renderer-Prüfungen benutzen den tatsächlichen Produktionscode mit Mesa/EGL/GLES3 und separat bereitgestellten, unveränderten Originalassets. Kein neuer Android-ARM64-/Gradle-/Lint-/Adreno-Gerätelauf wurde ausgeführt.

## Aktuelle Ergebnisse

| Prüfung | Ergebnis |
| --- | --- |
| CTest-Vertragsgruppen, final neu verlinkt | 32/32 bestanden |
| Neue M30-Originaldatenprüfung | 125 Prüfungen bestanden |
| Neue Produktions-JNI/GLES-Regression | 1871 Prüfungen bestanden |
| M30-Originaldatenprüfung mit ASan+UBSan | 125 Prüfungen bestanden |
| Bestehende M29-Originaldatenprüfung | 8462 Prüfungen bestanden |
| Bestehende M29-JNI/GLES-Prüfung | 204 Prüfungen bestanden |
| Bestehende Tür-/Fenster-/Rollback-JNI/GLES-Prüfung | 774 Prüfungen bestanden |
| Bestehende Leiter-JNI/GLES-Prüfung | 850 Prüfungen bestanden |
| Bestehende Innenraum-Originaldatenprüfung | 511 Prüfungen bestanden |
| Laser-GLES mit tatsächlicher r101-LIT und Farbnachweis | 15 Prüfungen bestanden |
| Java17 Disc / Controller / Raum- und Blockrepository | 8 / 11 / 81 Prüfungen bestanden |
| Produktions-Java-Syntax | 12 Dateien geparst; ausdrücklich kein Android-API-Typcheck |
| Versions-/XML-/Paketprüfung | 31 / 0.30.0-dev / Anzeige 0.30.0; Ressourcen-XML gültig |

Die Treppenregression verwendet die blockierte Position aus dem aktuellen Log `(3408.8, 2342.8, -14045.9)` und die unveränderte r101-SAT. Die obere Position erreicht `(3408.8, 3206.16, -15795.9)`; derselbe Weg ist wieder abwärts begehbar. Alle drei unverschlossenen Türen werden von beiden Seiten mit Originalkurven geöffnet/eingetreten. Die letzte blockierte Logposition `(-10956.7, 0, -6374.3)` wird zu `(-10937.8, 0, -6290.21)` getrennt und erlaubt danach Bewegung. Dynamische Körper behalten ihre vollständige Höhe; die statische Scenery-Regel wird getrennt geprüft.

Die Möbelprüfung verwendet originale PL00-Kurven, die drei echten ETS-Racks und deren echten Grenzen. Rack13 stoppt nach 1000 Einheiten nach Osten, Rack17 nach 800; weitere freie Wege bewegen sich im Test um rund 1899. Nullgrenzen blockieren. Gegenprobe, reale SAT-Bodenbindung, Modell-/Kollisionsgleichheit, Stop-/Loslasszustände und der Abbruch während des Greifens sind enthalten. Der Produktions-JNI-Test verwendet wirklich LS-vorwärts, erhält den laufenden Möbelowner, Position und Originalbones über Pause/neuen EGL-Kontext und vergleicht unveränderte Pixel. Der aktive Greif-/Schiebeweg versteckt die Waffe.

Die originale ET04-Basistextur und Alphamaske werden direkt aus den tatsächlichen GLES-Texturen gelesen. Nur die sechs Ecken der Bodenfläche bekommen die separate transparente, nicht tiefenschreibende Submission; die 24 Griff-Ecken behalten Tiefenschreiben. Bei identischer Kamera/Beleuchtung dunkelt die echte Schattenfläche den bestehenden Holzboden ab und hellt keinen Pixel auf. Ein tatsächlicher Fall aus r101 wird während der Luftphase gezeichnet: keine alte 36-Ecken-Kontaktfläche; Landung hält 1200 HP. Die separate Laserprüfung weist unter der wirklichen Raum-LIT mehr als drei originale rote Zielpixel nach; der Produktionslaser erhält unbeleuchtete, ungefogte Uniforms. Die vorhandene Nicht-durch-Wand-Zielpunktprüfung bleibt aktiv.

r100 lädt fünf originale Blockarchive mit 297 zusätzlichen starren Platzierungen und 331118 Dreiecken. Alle Dreiecke lösen ihre echten lokalen/gemeinsamen Materialien auf. Die Java-Prüfung deckt rohe Blockkopie ohne Yz2, vorhandenen Hauptcache bei fehlendem Block, falsche gleich große Daten, atomare Cache-Reparatur, Abbruch/Stage-Aufräumen und Pfad-/Symlinkgrenzen ab. Native Aktivierung einer gültigen r100-Hauptdatei ohne ihre Blöcke scheitert kontrolliert: vorheriger Raum, CPU-Owner und Framebuffer bleiben gleich. Tatsächlicher r100-Start, LIT/Himmel, Brückenvarianten und Kontextwiederherstellung sind geprüft. Renderbilder von r100 und vom Möbelboden wurden visuell kontrolliert.

Die zusätzliche CPU-Prüfung wurde aus einem frischen ASan+UBSan-Build ausgeführt, einschließlich Plattform-, Motion- und Parserbibliotheken. `detect_leaks=0`: keine LeakSanitizer- oder Android-Sanitizer-Coverage wird behauptet. Java meldet die bereits vorhandene Warnung über ein unbenutztes AutoCloseable-Testhandle. Erwartete fehlende Block-/GPU-Fehlerinjektionen sind kontrollierte Rollbacktests, keine beobachteten App-Abstürze.

## Reproduktion

Die Originalassets selbst bereitstellen. Haupt-Raumarchive sind die korrekt Yz2-entpackten `.dat`-Dateien; CORE ist das originale unkomprimierte DRS-/MRAM-Archiv `core.das`. Die fünf rohen `r100_00.dat` bis `r100_04.dat` müssen unverändert neben `r100.dat` liegen; beim Appcache entsprechend `room-r100_00.dat` neben `room-r100.dat`. Blockdateien weder Yz2-dekodieren noch die zusätzlichen Common-SMD-Platzierungen als eigene Instanzen zeichnen.

```sh
cmake -S app/src/main/cpp -B .host-tests -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O2 -UNDEBUG'
cmake --build .host-tests --target m30_assets_check
.host-tests/m30_assets_check R101.dat PL00.drs WEP01.drs CORE.das r100.dat
```

GLES-Ziele benötigen zusätzlich `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR`. Die jeweiligen Nutzungsparameter stehen in den Testprogrammen:

```text
m30_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 R100 OUTPUT_PREFIX
m29_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 OUTPUT_PREFIX
m27_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 R301
m28_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00
laser_gl_check CORE [R101]
```

Für die zusätzliche CPU-Speicherprüfung einen separaten Build mit `-fsanitize=address,undefined -fno-omit-frame-pointer` für Compiler und Linker erstellen; `m30_assets_check` mit `ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` ausführen. Die Android-App bekommt weder Sanitizer-Testschalter noch diese Testdaten.

## Änderungs-Paket

Das M30-ZIP enthält vollständige geänderte/neue Dateien über M29 unter `RE4-Android-Decomp/`. Verpackung prüft CRC, Dateihashes des vollständigen Overlay-Ergebnisses, unverändertes Upstream und den Ausschluss von Originalassets/APK/Build-/Bytecode-Dateien. `FILES-M30.sha256` enthält die gelieferten Quell-/Test-/Dokumentdateien, ausgenommen den Hashkatalog selbst. Keine Dateien müssen gelöscht werden. Installation und kombinierter Gerätetest: [UPDATE-M30.md](UPDATE-M30.md).

---

# M29: aktueller Prüfstand (2026-10-05)

VersionCode30 / `0.29.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Der finale native Hoststand wurde mit GCC13.3.0, C++17, aktiviertem `assert`, `-O2`, Warnungen als Fehler in den Adaptern und echter Mesa/EGL/GLES3-Ausgabe kompiliert. Frühere M28-/M27-Prüfungen werden darunter als historischer Stand erhalten.

## Aktuelle Ergebnisse

| Prüfung | Ergebnis |
| --- | --- |
| CTest-Vertragsgruppen | 32/32 bestanden |
| Neue LIT-/Itemregeln, enthalten in CTest | 16 Prüfungen bestanden |
| M29 Originalassets: LIT, Items, Sprünge, Godmode, Renderinterpolation | 8462 Prüfungen bestanden |
| Neue Produktions-JNI/GLES-Prüfung | 204 Prüfungen bestanden |
| Bestehende Produktionsbrücke | 2926 Prüfungen bestanden |
| Tür-/Fenster-/Raumrollback-JNI/GLES | 774 Prüfungen bestanden |
| Feste Leiter-JNI/GLES | 850 Prüfungen bestanden |
| Messer-/Effekt-/Bruch-/PCM-JNI/GLES | 788 Prüfungen bestanden |
| Nahe-Wand-/Schulterkamera-GLES | 1383 Prüfungen bestanden |
| Originale PL-Damage-/Blastposes, LifeDown/RNG und Enemy-Hits | 265030 Prüfungen bestanden |
| Java17 Disc / Controller / RoomRepository | 8 / 11 / 64 Prüfungen bestanden |
| Generierte Fixture-Archive / Spielerinitialisierung | bestanden, ausdrücklich keine Originaldatenvalidierung |
| Versionscode/-name, Ressourcen-XML | geprüft: 30 / 0.29.0-dev / Anzeige 0.29.0 |

Die Originaldatenprüfung springt an 58 gültigen Anläufen über 19 SAT-Kanten in r101 und 61 über 20 in r400. Sämtliche Landungen erreichen echten Boden. Fallpause, Handlungsausschluss, endliche Zeitgrenzen, unbeeinflusste Health-/RNG-Regeln und die reine Renderinterpolation bei 120Hz werden geprüft. Die 23 freien r101-ITA-Records liefern 23 Original-ITM-Platzierungen und 6410 Dreiecke; Besitzflags verhindern die zweite Aufnahme nach Raumrekonstruktion. Gegner- und Behälterwartebedingungen werden nicht zu freien Items umgedeutet.

Die neue Produktionsprüfung liest die echte r101-LIT und vergleicht die tatsächlich gezeichneten GLES-Pixel bei Tiefen 0/80000/200000 mit einer unabhängigen linearen GX-Rechnung und den negativen Originalstartwerten (höchstens eine Byte-Abweichung). Sie prüft den Himmels-Work0, die native A-Aufnahme von originaler Munitionsgeometrie und deren sichtbar verschwindendes Modell (176 unterschiedliche Framebufferpixel), unveränderte Held-A-Menge und separate Item-/ETC-Wörter. Ein tatsächlicher A-Sprung wird beim Fall pausiert; ein neuer EGL-Kontext hält dieselben CPU-Owner, Pose, Kamera, Munition, Beleuchtung und Itemflags. Fortsetzen landet auf Originalboden mit HP∞.

Die bestehende Brücke bestätigt den standardmäßig eingeschalteten Gesundheitsmodus. Nur innerhalb der alten reinen Heilungsregression wird er ausdrücklich abgeschaltet und anschließend wieder eingeschaltet. Die App erhält dadurch keine Abschaltfunktion. Die alte unabhängige Schadens-/RNG-Prüfung verwendet den CPU-Modus ohne Cheat und bleibt unverändert aussagekräftig.

Die Java-Testkompilation meldet eine bestehende Warnung zum unbenutzten AutoCloseable-Testhandle. Das ist kein Android-Lintlauf. Erwartete CPU-/GPU-Fehlerinjektionen in den Rollbacktests werden nicht als App-Abstürze eingeordnet.

## Reproduktion

Originaldateien selbst bereitstellen. R101/R400 sind **entpackte `.dat`-Raumarchive**; CORE ist das ursprüngliche unkomprimierte DRS-/MRAM-Archiv `core.das`. Native JNI/GLES-Ziele benötigen `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR`; CPU-Ziele benötigen diese GL-Optionen nicht. Assertions müssen auch bei Release-Hosttests aktiv bleiben.

```text
ctest --test-dir HOST_BUILD --output-on-failure
m29_assets_check R101.dat R400.dat PL00.drs WEP01.drs
m29_runtime_check R101.dat PL00.drs WEP01.drs roominfo.dat CORE.das EM12.drs emleon00.esl OUTPUT_PREFIX
m28_runtime_check R101.dat PL00.drs WEP01.drs roominfo.dat CORE.das EM12.drs emleon00.esl
m27_runtime_check R101.dat PL00.drs WEP01.drs roominfo.dat CORE.das EM12.drs emleon00.esl R301.dat
android_bridge_gl_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat
blast_combat_check EM12.drs emleon00.esl PL00.drs WEP01.drs
```

## Tatsächliche Grenzen

**Kein neuer Android-ARM64-/Gradle-/Lint-/Emulator-/Adreno-Gerätelauf für M29.** SDK/NDK für diesen Lauf sind hier nicht verfügbar. Die frischen Hostprüfungen ersetzen den Gerätebuild mit `clean assembleDebug lintDebug` und den kombinierten Gerätetest aus [UPDATE-M29.md](UPDATE-M29.md) nicht. Historische Android-Builds unten gehören ausschließlich zu ihren genannten Versionen. Leistung und Darstellung auf dem Nutzergerät sind noch zu bestätigen.

r101 benutzt den originalen Standard-LIT-Cut0 und ein begrenztes Lichtbackend; vollständige dynamische Schnitte, Shadows/Specular/Filter sind offen. Schrotflinte/weitere Gegenstände sind aufnehmbar, ihre bisher fehlenden Waffen-/Inventar-/Dokumentroutinen werden nicht als fertig erklärt. Der ursprüngliche Mehrgegner-Dorfkampf mit EM15 und den EVD-/Glockenereignissen ist ausstehend. [R101-GAMEPLAY-STATUS.md](R101-GAMEPLAY-STATUS.md) trennt diese Grenzen von den überprüften Funktionen.

## Änderungs-Paket

Das M29-ZIP enthält ausschließlich vollständige geänderte/neue Dateien über M28 mit dem Präfix `RE4-Android-Decomp/`. Der Verpackungslauf prüft CRC, vollständige Dateihashes nach dem Overlay, unverändertes Upstream und den Ausschluss von Originalassets, Buildprodukten und Python-Bytecode. `FILES-M29.sha256` dokumentiert die gelieferten Quell-/Test-/Dokumentdateien, ausgenommen den Hashkatalog selbst.

---

# M28: aktueller Prüfstand (2026-10-05)

M28 wurde auf dem Host mit GCC 12, C++17, aktivierten Assertions, Warnungen als Fehler in den Adaptern und echter Mesa/EGL/GLES3-Ausgabe geprüft. 31 CTest-Tests bestehen. Java 17: Disc 8, Controller 11 und RoomRepository 64 Prüfungen bestehen. Originaldaten: feste Turmleitern in r101 und r400 jeweils 16 Prüfungen; Produktions-JNI/GLES-Leiteraktion 850, Laser-GLES 14, Wandkamera 1.383, bestehende Brücke 2.924 und Innenrauminteraktionen 774 Prüfungen bestanden. Die Tests verwenden die tatsächliche JNI-, Eingabe-, Animations- und Rendererimplementierung; keine Originaldaten sind im ZIP enthalten.

M28 erhielt **keinen neuen Android-ARM64-/Gradle-/Lint-/Gerätelauf**: benötigte SDK-/NDK-Downloads waren hier nicht erreichbar. Frühere Android-Ergebnisse unten gelten für die dort dokumentierten Versionen. Den M28-Build in Android Studio mit `clean assembleDebug lintDebug` prüfen. Reproduzierbare neue Testprogramme: `m28_assets_check`, `m28_runtime_check`, `laser_gl_check`; Aufrufparameter stehen in ihren Fehlermeldungen und Quelltexten. Host-CMake: `RE4_BUILD_GL_CHECK=ON`, GLES/EGL und JDK-Includes setzen, Tests mit Assertions bauen. Originaldaten bleiben außerhalb des Quellprojekts.

Details und kurzer Gerätetest: [UPDATE-M28.md](UPDATE-M28.md).

---

# Validierung — M27, aktueller Stand

Prüfdatum: 2026-10-04. VersionCode 28 / `0.27.0-dev`, Paket `com.ast.re4decomp`, ARM64, minSdk21, targetSdk35, GLES3. Geprüft wird der erste gemeinsame r101-Raumzustand mit Innenraumtüren, Schloss und Fensterregeln. Die historischen M26-Fix1-/M26-Ergebnisse folgen darunter.

## Neue CPU- und Originaldatenprüfungen

**30/30 CTest-Gruppen bestanden.** Die zusätzliche Gruppe prüft MSB-Flaggrenzen 0/31/32/63/64/127/255, abgewiesene Indices ohne Mutation, getrennte Szenario-/Schlüssel-/Raum-/Objekt-/Framewörter, Kopierisolation und das echte Haus-/Glockenprofil. Dynamische Wandflächen werden auf endliche Koordinaten, Vertikalität, degenerierte Dreiecke und Budget geprüft. Sie erzeugen keinen Boden; eine Kapsel wird bei schneller Bewegung gebremst, gleitet an der Wand und wird nach Entfernen der Zusatzkollision wieder freigegeben.

`room_interactions_check` besteht mit **511 Prüfungen auf Originalassets**. Alle drei unverschlossenen Türen werden von beiden Seiten an SAT-gültigen Originalpositionen ausgelöst. Tatsächliche PL00-Kurven, endliche Leonposes, gespiegelte Blattöffnung um ±PI/2, der unveränderte Scharnierpunkt, freier Durchgang und Original-Auto-Close werden geprüft. Hinzu kommen das echte rechte Schloss, einmalige Serien, die fünf Fenster, vor Ereignissen geschützte Objekte, zwei echte BIN-Varianten mit Originalpalette und Wiederherstellung der Bruch-/Schlossflags. Die Fensterwaffenprüfung benutzt kurze Strahlen an den tatsächlichen Paneelen und die echte EAT; sie wird nicht als Prüfung jedes möglichen Schusswinkels ausgegeben.

Frisch instrumentierte **ASan+UBSan** bestehen für `room_state_tests` und denselben vollständigen 511-Verträge-Originaldatenablauf. Native Plattform- und Motionarchive dieses CPU-Builds werden vollständig instrumentiert. Leckprüfung ist in dieser Executor-Umgebung deaktiviert; es wird weder LeakSanitizer- noch Android-Sanitizer-Coverage behauptet. Die neue Wand-/BVH-Bindung wird vor der Freigabe des alten Owners ersetzt; nach Schäden wird die Körperkollision vor der nächsten Bewegung aktualisiert.

## Produktions-JNI und GLES

Der neue `m27_runtime_check` benutzt echte Produktions-JNI, EGL und GLES3 mit Originaldaten: **774 Prüfungen bestanden**. Teststubs und GPU-Fehlerinjektion werden ausschließlich im Hostziel gebaut.

- Direkter r101-Katalogstart erzeugt das vollständige Spieler- und Interaktionsprofil. Echte A-/LS-Eingaben öffnen die Tür, bewegen Leon durch den Durchgang und wechseln beim zweiten A-Druck von der Rückseite zum gespiegelten Originaltritt.
- Die tatsächlich ausgeführten Draws erhalten die Live-Scharniermatrix; ausschließlich das aktive originale Fenster-BIN wird gezeichnet. Keine Ersatztextur und kein Weglassen der Welt-Tiefenprüfung wird dafür verwendet.
- Pause friert beide Motionowner ein. Eine tatsächlich frische EGL-Oberfläche behält dieselben CPU-Objekte, Frames, Türkollision, Flags und im pausierten Zustand identische Framebufferpixel.
- Der Produktionsrouter übernimmt Originalschloss/-fensterhits, Cues und Serienbegrenzung. Diese neuen Strahlen werden synthetisch an den echten Router übergeben; LT/RT/X-Eingaben, Schussentstehung und Munition sind Gegenstand der bisherigen Waffenregressionen.
- Absichtlich fehlgeschlagener Ziel-GPU-Upload behält Spieler, Interaktionszustand, Flags und Darstellung des bisherigen Raums. Nach erfolgreichem r301-Ausflug stellt die r101-Rückkehr Fenster und Schloss aus denselben Raumwörtern wieder her. Eine ausdrücklich neue Sitzung setzt den Prototypzustand zurück.

Die bestehende umfassende Produktions-JNI/GLES-Regression besteht weiterhin mit **2918 Prüfungen** für r301↔r300, Waffen/Griff/Messer/Tritt, RS-Kameraachsen, Originalobjekte, sichtbare Tiere, Munition/Kräuter, aktive/inaktive Snapshots und CPU-/GPU-Rückfälle. Der M26-Fix1-Ablauf besteht auch nach Einführung der Tür-/Fensterlaufzeit mit **788 Prüfungen**; der direkte r101-Messerstart bleibt funktionsfähig. Das vollständige `tools/test_host.sh` besteht zusätzlich mit Exit0: optimierte C++-Builds, neue Raumflag-/Wandverträge und die vorhandenen Java-Disc-/Controller-/Repositoryverträge.

## Android-Artefakt

Frischer Projektordner, Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1. `clean assembleDebug lintDebug` besteht: **BUILD SUCCESSFUL, 50 Tasks, 46 ausgeführt/4 aktuell**. Der abschließende Stand besteht zusätzlich mit `assembleDebug lintDebug`: 47 Tasks, 6 ausgeführt/41 aktuell. **Lint: 0 Fehler, 6 bestehende Warnungen** (`UnusedAttribute`, `VectorRaster`, `ChromeOsAbiSupport`, `DataExtractionRules`, zweimal `UsableSpace`).

AAPT2 bestätigt `com.ast.re4decomp`, VersionCode 28/`0.27.0-dev`, API21/35, GLES3 und ausschließlich `arm64-v8a`. APK-CRC, der exakte native Payload gegenüber dem Strip-Ergebnis und die neuen Interaktionsdiagnosen sind geprüft. Alle drei ELF-LOAD-Segmente sind 16 KiB-ausgerichtet. Die Android-Appquellen, Ressourcen und Gradleversion des unabhängigen Builds entsprechen bytegenau den ausgelieferten Dateien.

Hier wurde kein physisches Android-Gerät und kein Android-Emulator ausgeführt. Tatsächliches Controllerlayout, Adreno-Verhalten, AudioTrack-Ausgabe und Leistung benötigen den kurzen kombinierten Gerätetest aus [UPDATE-M27.md](UPDATE-M27.md). Host-Mesa-Framebuffer und erfolgreiche ARM64-Kompilation ersetzen diesen Test nicht.

## Reproduktion und ZIP

```text
ctest --test-dir HOST_BUILD --output-on-failure
bash tools/test_host.sh
room_interactions_check R101.dat PL00.drs WEP01.drs CORE.das
m27_runtime_check R101.dat PL00.drs WEP01.drs roominfo.dat CORE.das EM12.drs emleon00.esl R301.dat
m26_runtime_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat R101.dat
android_bridge_gl_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat
```

JNI/GLES-Ziele benötigen `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR`; die echten Disc-Dateien werden selbst bereitgestellt. Die vorhandenen Brückentools erwarten die originalen Geschwisterpfade von PL00, darunter CORE und EM28/omake01.esl.

ZIP ausschließlich mit vollständigen geänderten/neuen Dateien über dem vollständigen M26-Fix1-Stand, unter `RE4-Android-Decomp/`. CRC, Pfade, unverändertes Upstream und exakte Rekonstruktion sämtlicher Quelldateihashes nach dem Overlay werden geprüft. Originalassets, SDK, APK, native Buildprodukte und lokale Rechnerpfade sind nicht enthalten. Kampagnen-EVD/Tasks, Fenstersprünge/Leitern, mehrere Ganados, Glockenende und dauerhafte Saves sind keine fertig erklärten Funktionen.

---

# M26-Fix1 — historische Validierung

Prüfdatum: 2026-10-04. VersionCode27 / `0.26.1-dev`, Paket `com.ast.re4decomp`, ARM64, minSdk21, targetSdk35, GLES3. Diese Korrektur betrifft den gemeinsamen Spielerstart und die Reihenfolge der Java-Datenkonfiguration. Die historischen M26-Ergebnisse folgen darunter.

## Reproduzierter Fehler und Regression

Der neue Ablauf in `tools/test_m26_runtime.cpp` öffnet r101 unmittelbar über `createScene`, ohne Dorf- oder Ganado-Testbutton. Auf dem unveränderten M26-Produktionscode scheitert er mit `Ordinary room start enables original knife/kick/grab clips without a test encounter`. Nach Korrektur besteht derselbe Ablauf mit Originaldaten und echter Produktions-JNI/GLES-Brücke.

**788 Laufzeitprüfungen bestanden:** Direkter r101-Start mit vollständigem Spielerprofil ohne Gegner; L1-Ziehen und sichtbare Klinge; L1+RT-Schnitt und originale Trefferframes ohne Munitionsverbrauch; L1-Abbruch und Wegstecken; Messer→Pistole→Schuss→X-Nachladen; Pause und tatsächliche EGL-Neuerstellung mit erhaltenem Spielerowner und Pose. Eine frische r300-Session erhält ebenfalls das vollständige Profil ohne Gegner. Die bisherigen Dorf-/Fass-/Splitter-/Audio-/Rollback-Fälle bleiben Teil desselben Ablaufs.

Die umfassende bestehende Produktionsbrücke besteht mit **2918 Prüfungen**, einschließlich Waffen-/Griff-/Messer-/Trittaktionen, RS-Achsen/Kameraübergängen, echten r301↔r300-Türen, Originalobjekten, Tieren, Item-/Krautaktionen und CPU-/GPU-Rückabwicklung. **29/29 CTest-Gruppen bestehen**. Es wurden keine weiteren Gameplay-Systeme als fertig erklärt und keine Originalassets ersetzt.

## Android-Artefaktprüfung

`assembleDebug lintDebug` wurde zusätzlich in einem frischen Projektordner ohne vorhandene generierte Ausgaben ausgeführt: **BUILD SUCCESSFUL,47/47 Tasks ausgeführt**. Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2, Gradle8.11.1 wie M26. Lint: **0 Fehler,6 bestehende Warnungen**.

AAPT2 und das verpackte Manifest bestätigen VersionCode27/`0.26.1-dev`, Paket `com.ast.re4decomp`, API21/35 und ausschließlich `arm64-v8a`. APK-CRC ist fehlerfrei; die enthaltene native Bibliothek trägt die neue Spieler-Diagnose. Alle drei ELF-LOAD-Segmente sind mit16KiB ausgerichtet. Appcode, Gradle- und Ressourcenversion dieses frischen Builds entsprechen den ausgelieferten Dateien.

Die unabhängige Prüfung war erforderlich, weil das vorhandene lokale Buildverzeichnis trotz neuem Versionsinput veraltete zusammengeführte Manifestdateien lieferte. Ein bloßes `BUILD SUCCESSFUL` wurde deshalb nicht als Nachweis der richtigen Paketversion übernommen. Die Ursache dieser lokalen Artefaktabweichung wurde nicht als allgemeiner Gradlefehler bewiesen; maßgeblich ist der geprüfte frische Build.

Hier wurde kein physisches Android-Gerät oder Android-Emulator ausgeführt. Das Benutzerlog belegt den vorherigen M26-Gerätelauf; die neue Messerinitialisierung ist durch den Originaldaten-/Host-GLES-Ablauf geprüft und benötigt den kurzen Gerätetest aus [UPDATE-M26-FIX1.md](UPDATE-M26-FIX1.md).

## Auslieferung

ZIP enthält ausschließlich acht vollständige geänderte/neue Dateien über dem vollständigen M26-Stand. ZIP-CRC, Pfade, unverändertes Upstream und exakte Hash-Rekonstruktion des Projekts werden geprüft. Disc-/SDK-/APK-/Buildprodukte sind nicht enthalten. Die bisherigen Kampagnen-, Effekt-, Inventar-, Ashley-/Karten- und Speichergrenzen bleiben offen.

---

# M26 — historische Validierung

Prüfdatum: 2026-10-04. Version26 / `0.26.0-dev`, Paket `com.ast.re4decomp`, ARM64, minSdk21, targetSdk35, GLES3. Historische Abschnitte darunter behalten ihre damaligen Ergebnisse und Grenzen.

## Android

Echter Kaltbuild `clean assembleDebug lintDebug`: **BUILD SUCCESSFUL,50/50 Tasks ausgeführt**. Die abschließenden Änderungen wurden zusätzlich erfolgreich mit `assembleDebug lintDebug` gebaut:47 Tasks,13 ausgeführt/34 aktuell. Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2, Gradle8.11.1. Lint: **0 Fehler,6 bestehende Warnungen**.

AAPT2 bestätigt Paket/Version26, API21/35, GLES3 und ausschließlich ARM64. APK-CRC und natives Payload sind geprüft; alle drei LOAD-Segmente haben **16KiB-Ausrichtung**. APK/SDK/Buildprodukte werden nicht ausgeliefert.

Kein Android-Gerät oder Emulator wurde hier ausgeführt. AudioTrack-Playback, Controller-UI, Adreno-Laufzeit und Leistung sind deshalb noch Gerätetests; gebaut wurde die echte Android-App, gerendert wurde auf Host-Mesa. Ein kurzer kombinierter Test steht in [UPDATE-M26.md](UPDATE-M26.md).

## Gemeinsame CPU-/Originaldatenprüfungen

**29/29 CTest-Verträge bestanden.** Das vollständige `tools/test_host.sh` besteht ebenfalls mit Exit0, einschließlich Java-Disc8, ControllerPolicy11, RoomRepository64, vorhandener Bewegungs-/Aktions-/Griff-/Drehregeln und neuer Audioverträge.

- Audio: **353 CPU-Verträge** für DSP-Hardware-Rundung, Sättigung/Endframe, Schleifenkontext, ungültige DVD/ISS/DLS/STB-Daten, Zufallsgruppen, Verkettung,64-Stimmenbudget, Pausen, Raumwechsel und nebenläufigen Producer/Consumer.
- Ausgewählte Originalbanken CORE/PL00/WEP01/r101/r400 und STB: **698 spielbare Cues,17081 Prüfungen**. Das gesamte vorhandene Raumkorpus ist automatisch geprüft: **142/142 DVD-Dateien,12764 Cues,76726 Prüfungen**; acht Räume besitzen absichtlich keine lokale Bank. Die142 dekodierten Raum-STB-Dateien bestehen ebenfalls.
- Objektaktionen: r400 **186** Originaldatenprüfungen mit16 Behältern, r101 **157** mit13 Behältern und sieben zufälligen ITA-Zuordnungen. Zwei-Takt-Verzögerung, Kettenreaktion, EAT-Nah-/Fernregeln, einmalige Ereignisse, RandomItemCk und Snapshots sind geprüft.
- EFM: r400 **74623**, r101 **77385** Originaldatenprüfungen; zusätzlich **85** analytische Physikverträge. Alle31 bzw.20 Teile sind native Modelle mit echten Texturen. Replay, Flug, Boden/Reflexion, Fade, ruhende Überreste und16-Bit-Überlauf sind geprüft.
- Explosionskampf: **265030** Prüfungen mit tatsächlich posierten Originalclips für lebenden Fall/Aufstehen, äußeres Stolpern und tödlichen Fall; Source-Leben/RNG, Waffenklasse13, Kopf-/Schadensgrenzen, EAT und einmalige30-Hz-Callbacks. Die vorhandenen35 Behälter-/Ganado-Nahkampfprüfungen bestehen weiterhin.
- r101-Geometrie:25 sichtbare ETS plus2 Skriptmodelle,10705 vollständig texturierte Dreiecke ohne Fallback-/Alpha-Fehler. 13 Itemowner mit62 Varianten/10 Modellen/26844 texturierten Dreiecken. Das Vorausladen der Varianten verändert keinen Drop-RNG/Snapshot.
- Welt-Effekte: **1024465 Originaldatenprüfungen**,42 Unit-Verträge und1701 Umgebungsprüfungen. r101 zeigt alle30 Flammenbilder und aufsteigenden Originalrauch; ET2D erzeugt endliche Originalfeuer-/Rauchsprites mit gemeinsamem Budget.

Frische **ASan+UBSan** bestehen für Audio-CPU/Originalbanken, Objektaktionen, echte r101-Geometrie, Fragmentphysik einschließlich Metallreflexion und die neu geänderten Kampfmodule. Beim Kampf sind die drei geänderten Module und das Prüftool instrumentiert, nicht alle statischen Basisarchive. LeakSanitizer war wegen der Executor-Einschränkung deaktiviert. Es wird kein Leak-/Android-Sanitizer-Ergebnis behauptet.

## Tatsächliche Produktions-JNI/GLES/PCM

Der neue `m26_runtime_check` inkludiert die Produktionsbrücke und benutzt echte EGL/GLES3 sowie Originalassets: **163 Prüfungen bestanden**. Host-JNI-Stubs und GPU-Fehlerinjektion sind ausschließlich im Testziel.

- Der wirkliche JNI-Dorfbutton lädt den geprüften r101-Punkt1 samt Feuer, Objekt-/Beutelaufzeit und Originalbanken. Ein Original-Zufallsbehälter löst seinen Inhalt genau einmal auf; nur seine gewählte ITM-Variante wird gezeichnet.
- Tatsächliche Objektstrahlen treffen ein ET2D-Fass. Der After-Tick-Pfad verbindet Ereignis, Originalsound, Explosionssprites und echte Metall-EFM44-Teile; der Schaden folgt genau nach zwei späteren Spielticks. Splitter verändern **10747 Framebufferpixel bei15 Produktionsdraws**.
- Start-Pause, echte EGL-Wiederherstellung, absichtlich fehlgeschlagener Ziel-GPU-Upload und neue Session erhalten bzw. erneuern die richtigen CPU-/Effekt-/Audioowner. Ein GPU-Rückfall ersetzt die bisherige Audiobank nicht.
- Tatsächliches `renderAudio(short[])` liefert hörbares Nichtnull-PCM und Pausen-Nullen. Null/ungerade/zu große Arrays, fehlgeschlagener JNI-Elementzugriff, maximale erlaubte Größe, Guardbytes und Release-Modus sind geprüft. Die beiden Audio-Gates verhindern Fortschritt bei UI-Pause.

**1383 Kamera-GLES-Prüfungen** bestehen:360 tatsächlich gerenderte Richtungen, gleiche Bilder bei30/60/120Hz, unveränderte entfernte Ansicht, richtige Cull-State-Rückgabe,15 Bewegungs-/Waffen-Wandpaare und Wandverdeckung. Im engsten Fall bleiben72119 Körper-/Waffenpixel erhalten; störende Kopfattachments werden gezielt ausgeblendet.

Actor-/Waffen-/Gegner-/Griff-/Messer-/Tritt-GLES-Regressionen bestehen mit Originaldaten. Zusätzliche **39 Fragment-/Item-GLES-Verträge** prüfen Materialbatches, genau eine Itemvariante, echte affine Welt-/Normalenmatrizen, Blend/Depth/Fade, Pause und Kontextaufbau. Die Effekt-GLES-Prüfung besteht mit **117 Verträgen** für reale Feuer-/Explosionssichtbarkeit, Verdeckung und Snapshot-Wiederherstellung.

Die vorhandene umfassende Produktions-JNI/GLES-Regression besteht ebenfalls mit **2918 Prüfungen** auf dem endgültigen M26-Stand: r301↔r300, echte Waffen-/Ganadoaktionen, sichtbare Tiere/Fundstücke, Kräuter, aktive/inaktive Raumsnapshots und CPU-/fünffache Actor-GPU-Rückabwicklung. Sie ergänzt den neuen163-Verträge-Ablauf und wird nicht als Gerätetest ausgegeben.

## Reproduktion und Umfang

```text
ctest --test-dir HOST_BUILD --output-on-failure
bash tools/test_host.sh
object_actions_check R400.dat 400
object_actions_check R101.dat 101
destruction_effects_check R400.dat CORE.das 400
destruction_effects_check R101.dat CORE.das 101
blast_combat_check EM12.drs emleon00.esl PL00.drs WEP01.drs
audio_check CORE.das PL00.drs WEP01.drs R101.das R101.dat R400.das R400.dat
m26_runtime_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat R101.dat
```

GLES/JNI-Ziele benötigen `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR`. Disc-Dateien werden selbst bereitgestellt. Die Brückentools erwarten originale Geschwisterpfade von PL00.drs, insbesondere CORE und bei der umfassenden Regression EM28/omake01.esl. Fixture-/Testdaten sind kein Ersatz für einen Vergleich der vollständigen GC-Engine.

Vollständige geänderte/neue Dateien über M25, ohne Originalassets oder generierte Produkte. Upstream-Commit bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`. ZIP-CRC, Pfade und Hash-Overlay werden vor Ausgabe geprüft.

Grenzen: vollständige Szenarioflags/Ereignisse/Kampagne, weitere KI/Abtrennungen, Musik/Schritte/Stimmen/Hall, sämtliche Effektfamilien, dynamische Fass-Laufkollision, Inventarkoffer und dauerhafte Saves. Kamera-Coverage und Fade-Tiefenschreiben sind deklarierte Renderanpassungen. Die im Update aufgeführten Source- und Clock-Adaptergrenzen gelten weiterhin.

---

# M25 — historische Validierung

Prüfdatum: 2026-10-04. Version25 / `0.25.0-dev`, Paket `com.ast.re4decomp`, ausschließlich ARM64, minSdk21, targetSdk35, GLES3. Historische Abschnitte darunter behalten ihre damaligen Ergebnisse und Grenzen.

## Android

Tatsächlich ausgeführter `clean assembleDebug lintDebug` mit Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1: **BUILD SUCCESSFUL, 50/50 Tasks ausgeführt**. Lint: **0 Fehler,6 bestehende Warnungen**. `aapt2 dump badging` bestätigt Paket/Version/minSdk/targetSdk. Das APK enthält nur `lib/arm64-v8a/libre4decomp.so`; alle drei LOAD-Segmente haben **0x4000/16KiB-Ausrichtung**.

Kein Android-Gerät/Emulator wurde hier ausgeführt. Die vollständige App einschließlich Java-UI/JNI wurde mit dem echten SDK gebaut; die GLES-Ausführung erfolgte auf Host-Mesa. Controller-Menübedienung, Treiber und Leistung benötigen weiterhin den Gerätetest in [UPDATE-M25.md](UPDATE-M25.md).

## CPU, Originaldaten und Sanitizer

**27/27 CTest-Verträge bestanden.** `tools/test_host.sh` besteht mit Exit0; darin Java Disc8, ControllerPolicy11 und RoomRepository64 Prüfungen sowie die native Daten-/Bewegungs-/Aktionskette. Neue Quellmodule sind auch in dessen eigenständige Verknüpfung aufgenommen.

- **4.315 Kamera-Prüfungen:** drei Nahflächenlinien, SEE-/Ausschlussmaske, schmale Wandränder, verschiedene Seitenverhältnisse,360 Körperrichtungen, gleichbleibender Augen-/Zielzusammenhang,30/60Hz, Waffenübergänge, Pause und begrenzte Rückführung. Der ganze Actor wird nicht mehr aufgrund der Kameraentfernung ausgeblendet.
- Physisch RS rauf liefert positives Y und bewegt freien Schulterblick, Pistolen- und Messeransicht nach oben. Das bestehende horizontale Mapping und LS-Gegenwehr bleiben erhalten. **496.275** vorhandene Original-PAD-/Drehregelprüfungen bestehen.
- **145 Original-r400-ObjectActions-Prüfungen:** alle13 originalen rotierenden Treffervolumen, ursprüngliche ITA-Bindungen und Mengen, Schuss-/Messerserien, einmalige Aufnahme, Aufnahmepriorität/Bereichsgrenzen, Inventartransfer, Wiederöffnung und validierte aktive/inaktive Raumsnapshots. Die CPU-Verträge prüfen zusätzlich alle ITA-Kürzungen, ungültige Floats/Slots, EAT-Verdeckung und ursprüngliche Messer-Ausnahme sowie die dreifache Pickup-Sichtprüfung.
- **Original-ITM-Geometrie:** 13 Platzierungen,5 BIN/TPL-Modelle,2.844 vollständig texturierte Dreiecke,7 Texturen; null Fallback-/Alpha-Ressourcenfehler. ETS-/Item-Identitäten bleiben beim Anhängen und Materialbatches erhalten. Parser-Verträge prüfen unter anderem abgeschnittene Header/Tabellen, doppelte IDs/Indizes, fehlende echte Modelle und NaN.
- **35 zusätzliche Original-Kampfprüfungen:** r400-Behälter vor/hinter tatsächlich posiertem EM12, nächster Treffer, kein Ganado-Durchschuss, wiederholte Schussmeldung, echte PL00-Messerstrahlen, EAT-Ausnahme, Pause, Munition/Überlauf, Heilung600/voll/tot und fortgesetzte Messer-Angriffsserie bei Playerersatz.
- Bestehende Original-Regressionen ohne Objektbindung bestehen: Griff9.315, Messer/Tritt26.425, Encounter3.383. Die sourcefreien Griffregeln behalten4.587.520 Original-Leben/RNG-Fälle.

Frisch kompilierter **ASan+UBSan** besteht für vollständige originale r400-ETS/ETM-/ITM-Geometrie, ObjectActions145 und den Kampfadapter35. Neue sourcefreie Item-/Objektparserverträge bestehen ebenfalls mit Sanitizern. Keine Sanitizerdiagnose. LeakSanitizer war wegen der Containerumgebung deaktiviert; keine Leak-Abnahme wird behauptet.

## Tatsächliche Produktions-GLES/JNI

**2.918 Prüfungen mit der echten Produktions-JNI-Brücke bestanden.** Der Test inkludiert `android_bridge.cpp` und führt SceneGL/ActorGL mit Originalarchiven auf surfaceless EGL/GLES3 aus. Host-Stubs sind nicht im Android-Target enthalten.

- Physische RS-Achsen und ursprüngliche Waffen-/Kamerapose, r301↔r300, Griff-/Messer-/Trittpfade, Neutralisierung, Pause und Szene/Actor-Kontextwiederherstellung bestehen weiterhin.
- Ein echter Encounter-Schuss trifft einen Originalbehälter. Dessen echtes ITM-Munitionsmodell verändert **2.862 Framebufferpixel**. Physisches A nimmt einmal auf, entfernt den sichtbaren Gegenstand und ergänzt exakt20 Patronen. Halten erzeugt keine zweite Aufnahme.
- Kräuteranforderungen heilen600, verbrauchen bei voller Gesundheit nichts, werden zwischen Renderbildern nicht verdoppelt und funktionieren bei Start-Pause ohne Gegnerfortschritt. Eine während des tatsächlichen Raumladens angeforderte Verwendung wartet, verbraucht dann ein Kraut aus dem inaktiven r400-Cache in r301; die Rückkehr zu r400 validiert dessen aktualisierte Prüfsumme.
- Behälter-/Fundzustände und Vorräte überstehen echten EGL-Kontextverlust, erneuten Kampftest und CPU-/GPU-Rückabwicklung. Ein gezielter Bone-UBO-Uploadfehler stellt den früheren Raum samt gebundenem Objektzeiger wieder her; ein nachfolgender Encounter-Schuss trifft dessen tatsächlichen Behälter.
- Vorhandene fünf Actor-GPUs, sechs Originalhühner und Effekt-/Kamerazustände werden weiterhin geprüft. Neue Session und Prozesspersistenz sind ausdrücklich getrennt: dauerhafte Savegames werden nicht behauptet.

**173 zusätzliche Wandkamera-GLES-Prüfungen** mit originalem PL00/WEP01: neun nahe Wandstellungen, jeweils Bewegung und Waffenansicht. Der bisherige vollständige Ausblendbereich wird tatsächlich gerendert; mindestens108.043 sichtbare Actorpixel. Opaque Wandverdeckung bleibt korrekt, pausierte Wiederholung pixelidentisch. Nahe Schulter-/Kopfausschnitte und die Originalraum-Munitionsaufnahme wurden visuell geprüft.

**24 eigene Objekt-GLES-Prüfungen:** gemeinsames Material mit unterschiedlichen Objektidentitäten, Behälterbruch/Itemanzeige/Aufnahme, unveränderte Nachbargeometrie, Inspektionsmodus, Snapshot-Rückkehr und echter Kontextwechsel. Keine neue GPU-Geometrie wird bei jeder Aufnahme aufgebaut.

## Umfang und Paket

M25 liefert vollständige geänderte/neue Dateien über M24 unter `RE4-Android-Decomp/`. Upstream-Commit bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`; ZIP-CRC und ein Hashvergleich des vollständigen Overlaystands werden vor der Ausgabe geprüft. Keine Disc-/Originalassets, APK, SDK-Pfade oder Builddateien werden ausgeliefert.

Dies ist ein cEmBox-/ITA-/ITM-Teilport mit einem begrenzten Vorratsadapter. Kistenbruch erfolgt unmittelbar im Player-Callback statt beim nächsten originalen EmMgr-Poll. Dynamische Fass-Laufkollision, Splitter/Audio, originaler Inventarkoffer/Kombination, Gewehr-/Granatenverwendung, Szenario-KI, vollständige Kampagne und dauerhafte Saves fehlen. Die Holzplanken-/r400-/r101-Zuordnung und der belegte Originalfeuerpfad stehen in [SOURCE-NOTES-M25.md](SOURCE-NOTES-M25.md). Die Welt-Effekt-Produktionsdateien sind byteidentisch zu M24.

Reproduktion mit konfiguriertem Hostbuild und eigenen Disc-Dateien:

```text
ctest --test-dir HOST_BUILD --output-on-failure
object_actions_check R400.dat
room_items_check R400.dat CORE.das
object_combat_check EM12.drs R400.dat emleon00.esl PL00.drs WEP01.drs
object_actions_gl_check
follow_camera_gl_check PL00.drs WEP01.drs OUTPUT-PREFIX
android_bridge_gl_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat
```

GL-Werkzeuge benötigen `RE4_BUILD_GL_CHECK=ON` sowie EGL/GLES3. Die JNI-Prüfung benötigt `RE4_JNI_INCLUDE_DIR` und erwartet CORE.das, EM28.drs und omake01.esl in ihren Original-Geschwisterpfaden zu PL00.drs.

---

# M24 — historische Validierung

Prüfdatum: 2026-10-04. Version24 / `0.24.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Die folgenden historischen Abschnitte behalten ihre damaligen Prüfergebnisse.

## Android-Build

Der tatsächliche abschließende Kaltbuild `clean assembleDebug lintDebug` mit Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1 ist erfolgreich: **50/50 Tasks ausgeführt**. Das originale zur AGP-Version gehörige AAPT2 wurde verwendet. Lint: **0 Fehler,6 bestehende Warnungen**. APK-Metadaten bestätigen Paketname, Version24, minSdk21, targetSdk35 und ausschließlich ARM64. Die drei nativen LOAD-Segmente haben **0x4000/16KiB-Ausrichtung**. Keine APK, Werkzeuge oder generierten Builddateien sind im Quellpatch enthalten.

Ein Android-Gerät oder Emulator wurde hier nicht ausgeführt. Die tatsächlichen Shader-/Framebufferprüfungen verwenden Host-Mesa/GLES3. Geräte-Treiber, UI und Laufzeitleistung müssen mit dem Gerätetest aus `UPDATE-M24.md` geprüft werden.

## CPU und Originaldaten

**25/25 CTest-Verträge bestanden.** Die eigenständigen Java-/Hostprüfungen aus `tools/test_host.sh` bestanden ebenfalls; der anschließend ergänzte Kameraübergang wurde separat mit dem endgültigen CMake-Stand geprüft.

- **253 Kamera-Verträge:** Originale obere/neutrale/untere Punkte, gemeinsame Bewegung-/Waffenkomposition mit FOV50, begrenzter gehaltener Rechtsstick, originale 0,9-Rückführung, 30/60Hz, Teil-Takt, verbleibender Waffenwinkel, endliche Zustände und nahe SEE-Wandkollision. Die aufgelöste optische Achse trifft immer den tatsächlich berechneten Zielpunkt.
- **496.275 Drehregelprüfungen:** ursprüngliche PAD-Sektoren/Schwelle, Abbildung durch die Android-Totzone, feste Geh-/Laufdrehung, Winkelüberlauf und vierframe-lange Kamerawinkelübergabe. Original-PL00-/WEP01-Bewegungs- und Waffenaktionsprüfungen bestanden.
- **r400-ETS/ETM:** alle33 Platzierungen,6234 vollständig texturierte Dreiecke,6 Türen,5 Fenster,4 Leitern,18 Behälter/Regale. Modelle, Originalpaletten, Originalpositionen, Indizes und Szenenzusammenführung geprüft; keine fehlenden Ressourcen. Parser-Verträge prüfen Kürzungen, NaN, ungültige Slots/IDs und Doppeleinträge. ASan/UBSan bestanden; LeakSanitizer war deaktiviert, daher keine Leak-Abnahme.
- **70.003 Hühnerprüfungen mit Originaldaten:** sechs ESL-Platzierungen, beide Texturvarianten,19 Gelenke,1022 Dreiecke je Tier; originale Wait-/Walk-Kurven, echtes Root-Movement, fünf SAT-Bodentiere/ein animierter erhöhter Sitzplatz, gleiche Posen bei30/60Hz, isolierter RNG und Pause.
- **28 sourcefreie und86.439 Originaldatenprüfungen für Welteffekte:** globale SST3/13,13 Records,4 Texturanimationen,200-Takt-Vorlauf, originale Weltorientierung, UV/Farb-/Blendzustände, Rückspielbarkeit, Ressourcen- und Zustandsgrenzen. Bereichsabhängige SST5 wird nicht global aktiviert.
- Die Original-Griffprüfung mit PL00/WEP01/EM12 besteht mit9315 Checks; die Messer-/Trittprüfung mit26425 Checks. Der ursprüngliche r400-Terrain-Kampftest endet nach gekoppeltem Griff und erfolgreicher LS-Gegenwehr bei Leon990HP/Ganado900HP.

## Tatsächliche GLES3-Ausgabe und Lebenszyklus

**2723 Prüfungen mit der echten Produktions-JNI-Brücke bestanden.** Der Test führt `android_bridge.cpp`, SceneGL und ActorGL mit den Originalarchiven aus; die Android-App enthält keine Host-Stubs.

- Aktueller RS-Blick wird bei gleichzeitigem L2 vor dem Aktionstakt übernommen und genau einmal fortgeschrieben. Vierframe-Draw, frühes Loslassen und vollständiges Ablegen behalten den optischen Augen-/Zielpunkt beim Modusübergang. Pistole und Messer verwenden dieselbe Kamerapunkttabelle.
- Kamera inklusive separater Körperrichtung, Originalmodelle, Waffen-/Ganado-Pose, HP, Munition, RNG und beide Effektzustände bleiben bei Pause und echtem EGL-Kontextwechsel exakt erhalten.
- r301→r300→r301, CPU-/GPU-Ladefehler, veraltete Raumtokens und Rücknahme in den bisherigen Raum bestehen weiterhin. r400 enthält alle33 neuen Originalobjekte und sechs animierte Tiere. Gezielt fehlende EM28-Daten sowie Fehler beim vierten/fünften Bone-UBO erhalten den vorherigen Raum und stellen alle fünf Actor-GPUs wieder her.
- Tatsächlich gerendertes Originalhuhn:930 veränderte Bildpunkte. Tatsächlich gerenderter Ziel-Ganado:24697 Bildpunkte; pausiert22819. Bilder mit Tür, Wagen, Zaun, Huhn und rechter Schulterkomposition wurden visuell geprüft.
- SAT-/Einzelansicht nach36001 Szenenframes erhält die Kamera und startet eine inkompatible Effektsource mit neuer Uhr statt eines unbeschränkten Nachholens. Kontextwiederherstellung derselben Source erhält dagegen deren genauen Effektzustand.

**65 eigene Welt-GLES-Prüfungen bestanden:** originale r400-Texturen, animierte Bildänderung, vollständige Tiefenverdeckung, erhaltenes Zielalpha, alle17 geprüften Caller-GL-Zustände, eingefrorene Bilder, Rückwärtsuhr und echter EGL-Kontextverlust.

**96 Himmel-/Wasser-GLES-Prüfungen bestanden:** r100/r300/r301 erzeugen nach1800 Takten4096/16359/16324 veränderte Wasserzielpixel. Originale Follow- und Inspektionsbilder bleiben bei Pause und Kontextneuerstellung exakt erhalten. Die bestehenden Sicht-/Verdeckungsschwellen wurden beibehalten und die synthetischen Geometrien auf die neue Originalkamerapose ausgerichtet. Die Szenenprüfung bestätigt stabile actorlose Followansicht, exakte Normalansicht-Rückkehr, Originaltexturen/Masken/Vertexfarben/Mips und UV-Scroll.

## Umfang und Reproduktion

Die Kamerapunkttabelle, PAD-Regeln, Objektmodelle und Tierkurven stammen aus den eingefrorenen Originalquellen/Daten. Die gemeinsame Transition-Kamera statt unterschiedlicher Original-Waffenprofile ist die ausdrücklich gewünschte Anpassung; SAT-Kugelsweep, native Laufzeitadapter und die ergänzte Winkelrückführung ersetzen noch keine vollständige Original-Kamerazustandsmaschine. Der Patch aktiviert keine komplette Kampagne oder r400-Szenario-KI. Die neuen Objekte sind zunächst visuell; Tierflug/Schaden/Eier/Audio und vollständige Fackel-/Scheiterhaufenflammen/Rauch fehlen. Die neuen Weltlagen sind Glut und Umgebungslichtstrahlen.

Upstream-Commit unverändert: `9b76ff003081d6ece8b838453fb8310743a972c7`. Alle42 Patchdateien sind vollständige geänderte/neue Dateien über dem kompletten M23-Reparaturstand. Keine Disc-/Originalspieldaten werden ausgeliefert.

Zusätzliche Hostwerkzeuge nach CMake-Konfiguration mit `RE4_BUILD_GL_CHECK=ON`:

```text
room_objects_check R400.dat CORE.das
ambient_animals_check EM28.drs OMAKE01.esl R400.dat
world_effects_check CORE.das R400.dat
world_effects_gl_check CORE.das R400.dat
environment_gl_check CORE.das R100.dat R300.dat R301.dat OUTPUT-PREFIX
scene_gl_check R300.dat OUTPUT.ppm R100.dat
android_bridge_gl_check PL00.drs WEP01.drs R300.dat R301.dat roominfo.dat OUTPUT.ppm EM12.drs emleon00.esl R400.dat
```

Die Originaldateien werden nur aus dem eigenen vollständigen Disc-Import gelesen. Die JNI-Prüfung erwartet CORE.das, EM28.drs und omake01.esl in den ursprünglichen Geschwisterpfaden zu PL00.drs.

# M23 — historische Validierung

Prüfdatum: 2026-10-04. Version23 / `0.23.0-dev`, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Die folgenden historischen Abschnitte behalten ihre damaligen Prüfergebnisse.

- **20/20 CTest-Verträge bestanden.** Eigenständige `tools/test_host.sh`-Prüfungen mit Java17/native Tests bestanden. Neue Inputprüfungen: kleine LS-Wechsel, sechs/mehrere Flanken zwischen Renderbildern, einmaliger Verbrauch, Halten, Hysterese, LS/Dpad-Alias, Face-Button-Alias, Lebenszyklusreset und rechte vertikale Achse unabhängig von Bewegung.
- Original PL00/WEP01/EM12: **9.315 gekoppelte Griffprüfungen** bestanden, einschließlich Analog-Gegenwehr bei gehaltenem L2 nach Suspend, Bruchteiltakt, Originalschaden, Befreiung,30/60-Hz-Verlauf und RNG.
- Neuer `terrain_combat_check` mit originalem r400 SAT/EAT und Originalfiguren: Hanghöhen2575,77/2273,52, Annäherung, gekoppelter Griff, sechs Links/Rechts-Flanken=12 Original-Gegenwehrpunkte, erfolgreiche Befreiung und Footwork-Rückkehr. End-HP990/900. Der Test prüft reale r400-Geometrie, nicht eine synthetische Bodenplatte.
- **2.184 Produktions-JNI/GLES3-Prüfungen bestanden**: kompletter r301/r300/r400-Lauf; sichtbarer Ganado,162-HP-Kopftreffer, pausierte Originalpose, Lade-/GPU-/Kontext-Rückfälle und Bestandsraum-Erhaltung. Zusätzliche vertikale Prüfungen für freie Kamera und Messeransicht; Pistolenprüfung folgt der invertierten Achse. Sichtkontrolle der gerenderten r400-Startansicht bestätigt Gelände und horizontalen erhöhten Schulterblick.
- Kamera-Verträge: horizontaler900-Versatz bei freier Sicht, SAT-Verkürzung vor Wand ohne Höhenabsenkung, vorhandener Boden-/SEE-Kugelsweep. Transparenzsortierung benutzt denselben tatsächlichen Kameraort wie die Viewmatrix.

Der echte Android-Build `clean assembleDebug lintDebug` mit Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1 ist erfolgreich. Generierte `app/.cxx`/`app/build` wurden vorher entfernt; der native Quellstand wurde neu übersetzt. Lint: **0 Fehler,6 bestehende Warnungen**. APK-Metadaten bestätigen Paketname, Versioncode23 / `0.23.0-dev`, minSdk21 und ausschließlich ARM64; LOAD-Segmente sind auf16KiB ausgerichtet.

Die unveränderten Upstream-Quellen bleiben auf Commit `9b76ff003081d6ece8b838453fb8310743a972c7`. Der Patch enthält vollständige Änderungen über M22 und keine Disc-, entpackten Spiel- oder Builddaten. Host-Mesa/GLES3 ersetzt keinen Android-Gerätetest; UI, Treiber und tatsächliche Leistung müssen dort geprüft werden.

# M22 — historische Validierung

Prüfdatum: 2026-10-04. Version22/0.22.0-dev, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Die folgenden älteren Abschnitte dokumentieren frühere Stände und deren damalige Grenzen.

## Tatsächlicher Android-Build

Finales `clean assembleDebug lintDebug` mit Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1: **BUILD SUCCESSFUL**,50 Tasks (49 ausgeführt,1 bereits aktuell). Lint: **0 Fehler,6 bestehende Warnungen**. Vorher wurden `app/.cxx` und `app/build` entfernt. Die neuen Produktionsdateien, Shaderintegration, JNI-Deklaration und Java-Core-Prüfung sind im echten Android-Build enthalten.

`aapt2 dump badging` bestätigt Version22, `0.22.0-dev`, Paketname und minSdk21. Das APK enthält ausschließlich `lib/arm64-v8a/libre4decomp.so`; dessen LOAD-Segmente sind auf0x4000/16 KiB ausgerichtet. Die bereits bestehenden Compilerwarnungen im ursprünglichen Motion-/FDlibm-Code bleiben bestehen. **Kein Android-Gerät oder Emulator wurde hier ausgeführt.** Shader-/Framebuffer-Ausführung erfolgte auf Host-Mesa/GLES3; Geräte-Treiber, Leistung und die tatsächliche App-UI brauchen weiterhin den Gerätetest.

## Originaldaten, Grenzen und Effektzustand

**20/20 CTest-Verträge bestanden.** `tools/test_host.sh` prüft weiterhin zusätzlich die Java-Disc-, ControllerPolicy- und RoomRepository-Verträge sowie die eigenständig kompilierten nativen Tests. Neue sourcefreie Umwelt-Verträge: **1.701 Prüfungen**. Mit bereitgestellten Originaldaten: **389.378 Prüfungen**.

- Primärer EFF bei mehrfachen Archiv-Tags, Tabellen-/Ressourcen-Grenzen, TPL-/ANM-Zuordnung, NaN, zu großer Textur-/Scrollbereich und Basistextur-Rückfall geprüft.668 aufeinanderfolgende Kürzungen und1.000 gezielte Byte-Mutationen werden begrenzt verarbeitet.
- r100:1 Himmel,1 Wasserziel,2 Flächen; r300:2 Himmel,2 Wasserziele,2 Flächen; r301:2 Himmel,1 Wasserziel,1 Fläche. Alle Originalsequenzen werden ohne Warnung/Fallback geladen. r300 EST4 enthält nur4 zugehörige Ziel-Effekte; seine18 Welt-Effekte landen nicht in diesem Texturkanal.
- Jeder Raum wurde10.800 Originaltakte/sechs Minuten simuliert. Maximal gleichzeitig26/81/2 Sprites in r100/r300/r301, unter dem512-Poollimit. Muster-/Texturgrenzen und alle erzeugten Quadpositionen sind gültig/endlich.
- Strikte ANM-Grenze, direkte Musterwahl, verzögerte30-/70-Takt-Emitter, Set_num+1, Intervallabstand, CPU-Snapshot, fremde Raum-Signatur, ungültige Snapshotreferenz, OT-Bucketreihenfolge und Erhaltung des Himmels-Gelenkpivots geprüft.
- Derselbe Originaldaten-/Parserlauf besteht mit frisch kompiliertem **AddressSanitizer und UndefinedBehaviorSanitizer**, ohne Diagnose. LeakSanitizer war deaktiviert (`detect_leaks=0`); keine Leak-Abnahme wird behauptet.

## Unabhängiger Originalcode und tatsächliche GPU

`tools/check_environment_reference.py` extrahiert unverändert die vollständigen Originalmethoden `cEsp::CommonMove`, `ColorUpdate`, `AnmMove` aus dem eingefrorenen `upstream/re4/src/game/esp_sub.cpp` und kompiliert sie separat. Nur benötigte SDK-Vektor-/Hostobjekt-Hüllen werden gestellt. **399.768 Prüfungen** gegen den Adapter: maximale Floatabweichung **0,0**. Verzögerung, Dämpfung, Größen-/Farbverlauf, Todesframe, drei ANM-Schleifenmodi und die ursprünglichen8-/16-Bit-Zähler einschließlich Überlauf wurden variiert. Dies validiert diese CPU-Zweige; daraus folgt keine bitgenaue GX-Pixel- oder globale Kampagnen-RNG-Reproduktion.

Neuer `environment_gl_check` auf echtem surfaceless EGL/GLES3 (Mesa25.2.8): **78 Prüfungen bestanden**. Alle Shader, Texturen, FBOs,2×-Rasterziele und der Resolve wurden tatsächlich ausgeführt.

- Synthetische Szene: Himmel hinter600.000 Welt-Einheiten bleibt sichtbar; opake Vordergrundgeometrie verdeckt das dahinter gezeichnete Wasser. Eine zweifarbige Verlaufstextur erkennt die korrekte Y-Ausrichtung zwischen Effektziel und Raumtextur.
- Originale Wasserziele ändern nach1.800 Takten4096/16359/16324 Pixel für r100/r300/r301. Originale Debug-Raumstarts wurden zusätzlich als Follow-Ansicht gerendert und die Aufnahmen visuell geprüft.
- Mehrfaches Zeichnen mit eingefrorener Uhr behält das gesamte Bild. Nach tatsächlicher EGL-Kontextzerstörung/Neuerstellung erzeugen CPU-Snapshots exakt dasselbe Raum- und Wasserzielbild.
- Produktions-JNI/SceneGL/ActorGL mit CORE-EFF und Originalarchiven: **2.074 Prüfungen bestanden**. r301↔r300-Türen, asynchrone Fertigstellung ohne aktuellen EGL-Kontext, tatsächliche Waffen-/Nahkampf-/Ganado-Pfade, Pausen-Neutralisierung, Grafik-Kontextwechsel und gezielt ausgelöste CPU-/GPU-Fehler mit Rücknahme bleiben gültig. Erwartete Fehlerlogs stammen aus absichtlich fehlenden Dateien/gescheiterten Uploads dieses Tests.

Reproduktion aus dem Projektordner, `HOST_BUILD` ist ein bereits konfiguriertes CMake-Hostverzeichnis:

```sh
cmake -S app/src/main/cpp -B "$HOST_BUILD" -DRE4_BUILD_GL_CHECK=ON
cmake --build "$HOST_BUILD"
ctest --test-dir "$HOST_BUILD" --output-on-failure
python3 tools/check_environment_reference.py --build "$HOST_BUILD"
"$HOST_BUILD/environment_tests" CORE.das R100.dat R300.dat R301.dat
"$HOST_BUILD/environment_gl_check" CORE.das R100.dat R300.dat R301.dat OUTPUT-PREFIX
```

GLES3-/EGL-Header und Bibliotheken müssen auf dem Host vorhanden sein. Der optionale Produktionstest benötigt zusätzlich `RE4_JNI_INCLUDE_DIR` auf das JDK-`include`-Verzeichnis. Originaldaten bleiben außerhalb des Quellpakets; `environment_gl_check` verwendet zusätzlich `Debug/roominfo.dat` aus demselben Disc-Verzeichnis wie CORE.

## Paket und verbleibende Grenzen

M22 verteilt ausschließlich vollständige neue/geänderte Dateien über M21 mit Projektstruktur `RE4-Android-Decomp/`. Keine Discs, Original-Assets, APK, Buildausgaben oder SDK-/Maschinenpfade sind enthalten. Der eingefrorene Upstream ist byteidentisch. ZIP-CRC, vollständige Overlaydateien und Zeitstempel in der Vergangenheit sind geprüft. Die Update-Anleitung verlangt die Entfernung von `app/.cxx` und `app/build`, damit ältere Objektdateien nicht weiterverwendet werden.

Die getesteten Profile decken r100/r300/r301 und die benötigten Esp00/08/47-/Emitter00-Bildpässe ab. Welt-Effekte, alle weiteren Flüssigkeiten und vollständige GX-TEV/EFB-Filterung sind nicht allgemein umgesetzt. r100-Brechung, Pool-/OT-Anbindung und isoliertes visuelles RNG sind dokumentierte GLES/Testadapter. Kampagne, komplette Gegner-/Szenariozustandsmaschine, Inventar, Audio und Speichern fehlen weiterhin. Anleitung und Grenzen: [UPDATE-M22.md](UPDATE-M22.md).

---

# Validierung — M21, aktueller Stand

Prüfdatum: 2026-10-04. Version0.21.0-dev, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Die älteren Abschnitte darunter dokumentieren frühere Stände und deren damalige Prüfgrenzen.

## Android und UI

Tatsächlich ausgeführt: vollständiger `clean assembleDebug lintDebug` mit Java17, SDK35, NDK28.2.13676358, CMake3.22.1, AGP8.9.2 und Gradle8.11.1. **BUILD SUCCESSFUL**,50 ausgeführte Tasks. Lint: **0 Fehler,6 bestehende Warnungen**. APK-Metadaten bestätigen Version21/0.21.0-dev, minSdk21 und Paketname. Die native Bibliothek hat16-KiB-LOAD-Ausrichtung. APK und Geräte-Spieldaten gehören nicht zum Patch-ZIP.

Die neue Vollbild-UI, Ressourcen und alle Java-Klassen wurden gegen das echte SDK gebaut. GLSurfaceView füllt den FrameLayout-Viewport; Zurückpfeil, HUD und optionale Diagnose liegen darüber. Der lokale Menüwechsel pausiert dieselbe Surface und setzt Eingaben zurück, ohne einen Activity-Neustart oder einen neuen Szenenbesitzer zu verlangen. **M21 wurde hier nicht auf einem Android-Gerät oder Emulator ausgeführt.** Insbesondere Pfeil/Touch, Insets, Rotation und Geräteleistung benötigen den beschriebenen Gerätetest.

## Native Regressionen und echte Spieldaten

Alle19 CMake-/CTest-Verträge und `tools/test_host.sh` bestanden. Der Hostscript prüft zusätzlich die Java-Disc-, ControllerPolicy- und RoomRepository-Verträge. Originalarchive wurden aus den bereitgestellten G4BE08-Revision0-Discs wiederhergestellt; die benutzten PL00/WEP01/EM12-Hashes sind in den Referenzscripts festgehalten.

- Korrigierte Zieldrehung: physischer RS rechts/links über den echten Mapper und die gelöste Waffen-/Handrichtung relativ zur Kamera; frühe Hüft- und spätere Körperdrehung sowie gewickelte Blickrichtungen bestanden. Der entsprechende Test erkennt das alte Vorzeichen.
- Messer-/Tritt-Integration mit Originalarchiven: **26.425 Prüfungen**. Eigene gültige synthetische Hermite-Fixtures: **26.512 Prüfungen**. Lebenspunkte, Sweep-Strahlen und SEQ-Flags,17-Takt-Messerschutz,17.Handler-Tritt, RNG-Reihenfolge, EAT-Masken, sichtbare Klinge, Munition, Kontakt, Aktionsaufforderungen, Pause/Flanken, Griffe, geschützte Reaktionen und Boden-Treffer/Tod wurden geprüft. Gekoppelte Posen, HP und RNG stimmen bei30/60 Render-FPS überein.
- Beide vollständigen Integrationsläufe bestanden zusätzlich mit frisch gebautem AddressSanitizer + UndefinedBehaviorSanitizer einschließlich der neuen FDlibm-Dateien. Keine Sanitizerdiagnosen. LeakSanitizer war deaktiviert (`detect_leaks=0`); keine Leak-Abnahme wird behauptet.
- Unabhängig kopierte Original-Regeln:32.768 animierte Kugel-/Kapsel-Geometriefälle,65.536 Messer-RNG-Fälle und1.048.576 Blow-Auswahlen einschließlich finalem RNG-Zustand bestanden. Bestehende Griff-Regeln behalten4.587.520 unabhängige Lebenspunkte-/RNG-Fälle.
- M20-Griff mit echten Archiven:9.307 gekoppelte Prüfungen bestanden. Der ursprüngliche Ganado-/Schuss-Test mit echten r100-/ESL-/Modell-Daten:3.383 Prüfungen bestanden.

Der `melee_encounter_tests`-Aufruf ohne Assetargumente prüft absichtlich nur zwei kleine Metadaten-Verträge. Die obigen vollständigen Integrationszahlen stammen ausdrücklich aus den Aufrufen mit allen drei DRS-Dateien.

## Unabhängige Original-Animation und tatsächliche GLES-Brücke

Die Referenzen bauen separat den unveränderten Originalcode aus `upstream/re4`; sie vergleichen keine zweite Kopie unseres Android-Adapters mit sich selbst.

- **699 Messer-/Tritt-Posen**,119 Gelenke, mit Original-FCV, cMot3, IK, Hokan und Waist: maximale Abweichung **0,00073242**, unveränderte Testgrenze0,005. Zustände, SEQ und Frame-Crossing-Ereignisse für Ziehen, Ablegen, Messer→Waffe und Tritt bestanden.
- **1.943 Ganado-Posen**,34 Gelenke, und24 Root-/SEQ-/Free-/Hokan-/Spiegelungstraces: maximale Abweichung **0,0**. Acht beendete Fall-/Bodentod-/DownIdle-Posen bestanden.
- Originale M20-Griffkurven zusätzlich erneut geprüft:685 Posen und6 Traces, maximale Posen-/Rootabweichung0,0. Der historische NoSEQ-Endframe-Test unterscheidet jetzt korrekt die Loop-Metadaten vom One-shot-Frame `maxFrame`; Laufzeitcode wurde dafür nicht verändert.
- Produktions-ActorGL auf tatsächlichem surfaceless GLES3/Mesa: **143 vollständige Transform-Feedback-Aufnahmen**,26.957.740 Prüfungen. Alle27.603 Leon- und10.341 Ganado-Render-Ecken einschließlich Klinge/offener Hände wurden kontrolliert; maximale CPU-/GPU-Abweichung0,000521296. Verdeckte Teile, Zieh-/Schlag-/Ablegphasen, Pitchraten, Spiegelungen, neue Boden-/Körperkurven, Kontextwechsel und unveränderte Munition bestanden. Die Aufnahme wurde visuell geprüft.
- Tatsächliche Produktions-JNI-/SceneGL-/ActorGL-Brücke mit Originalräumen r300/r301/r100: **2.073 Prüfungen**. Echte Türwege, Waffensteuerung, horizontale Kamera-/Zielrichtung, Messeransicht, Pause/Neutralisierung, asynchrone Raumfertigstellung, EGL-Kontextwiederherstellung und absichtlich ausgelöste CPU-/GPU-Fehler mit Rücknahme bestanden. Die absichtlichen Fehlerlogs dieses Tests sind keine gefundenen App-Abstürze.

Die Original-FDlibm-Funktionen für Sinus, Kosinus und Arkuskosinus sind jetzt im Motion-Namespace erhalten. Dadurch wird eine ursprüngliche IK-Verzweigung bei fast gestreckten Beinen reproduziert, die mit System-Trigonometrie um eine Float-ULP abwich. Es werden keine globalen Standard-Mathematiksymbole ersetzt. Generator-Reproduktion und Symbolprüfung bestanden.

Bei der neuen Tritt-Referenz musste der unveränderte Originalaufruf `MotionClear` vor Idle0 ergänzt werden, um dieselbe Initialisierung wie der native Reset zu prüfen. Die einzelne frühere Knieabweichung verschwand mit identischem Ablauf; weder Produktionspose noch Toleranz wurden dafür angepasst.

## Grenzen und Paket

Originalclips und ausgewählte Kampfzweige sind geprüft; dies ist weiterhin ein begrenzter Testadapter mit einem ausdrücklich aktivierten Ganado. Vollständige Szenario-KI, bewaffnete/multiple Gegner, Arm-/Knie-Spezialreaktionen, dynamische Kollision und Abgrund-Fallzustand, Kampagneninventar, Beute, Audio, Speichern und Original-Kameraschnitte fehlen. Die resultierenden Hostprüfungen beweisen keinen vollständigen ersten Kampagnenlevel und ersetzen keinen Android-Gerätetest.

Das M21-ZIP enthält nur vollständige neue/geänderte Projektdateien. Upstreamdateien bleiben byteidentisch; keine ISO/GCM, Original-Assets, SDK-/Maschinenpfade, Buildausgaben oder APK werden verteilt. Die ZIP-Zeitstempel liegen ausdrücklich in der Vergangenheit; vor dem Update-Build `app/.cxx` und `app/build` entfernen. Anleitung: `UPDATE-M21.md`.

---

# Validierung — Milestone 01

Prüfdatum: 2026-10-03. Quellstand und tatsächliche Tests, keine Gameplay-Zusage.

## Android-Build

`clean assembleDebug lintDebug` mit AGP 8.9.2, Gradle 8.11.1, Java 17, SDK 35,
NDK 28.2.13676358 und CMake 3.22.1: **BUILD SUCCESSFUL**.
Java/Dex, ARM64-C++/JNI, Ressourcen und APK-Verpackung wurden tatsächlich gebaut.
Die native Bibliothek verwendet 16-KiB-Linkerausrichtung; Geräte mit 16-KiB-Seiten wurden nicht praktisch geprüft.

Lint: **0 Fehler, 6 Warnungen**, ohne Baseline oder deaktivierte Fehlerprüfungen.
Offene Hinweise: Android ignoriert `appCategory` vor API 26; TV-Vektorbanner über 200 dp;
kein x86_64/ChromeOS-Ziel; zusätzliche Android-12-Backupregeln fehlen; zwei konservative
`getUsableSpace`-Abfragen statt API-26-Cachefreigabe. Importdaten dürfen nicht als Backup eingeplant werden.
Der Gradle-Wrapper enthält die SHA-256-Prüfsumme der verwendeten Distribution.

## Host-Tests

`tools/test_host.sh` baut und prüft die unabhängig ausführbaren nativen Teile und Disc-Klassen.

- Native Eingaben: Deadzone, Grenzen, NaN, Triggerhysterese, D-Pad-Gegenrichtungen,
  wiederholte Tasten, kurze Press-/Release-Flanken, Reset.
- Original-RNG: deterministische Folge, 10.000 Vergleichsschritte, Wertebereiche.
- TPL: sämtliche unterstützten Textur- und Palettenformate, CMPR-Transparenz,
  verkürzte Daten und Grenzen.
- Synthetische Disc-Dateien: **8 Prüfungen bestanden**, einschließlich ungültiger
  Header, Dateitabellen und Pfade sowie abgebrochener Extraktion.
- Beide bereitgestellten GCM-Dateien: **4.510 Prüfungen bestanden**. Originalcode-
  Referenzen geprüft, vollständige Extraktion durchgeführt und SHA-256 jeder Ausgabe
  erneut berechnet. Disc 1: 1.155 Ausgaben inkl. main.dol; Disc 2: 1.091.
  Wiederherstellung und Abbruch bei bereits installiertem Datenbestand geprüft.
- AddressSanitizer + UndefinedBehaviorSanitizer: native Verträge und drei echte TPLs
  (`pl010a.tpl`, `moji8.tpl`, `itm00.tpl`) bestanden. LeakSanitizer war wegen der
  Ausführungsumgebung deaktiviert; damit wird keine Leak-Prüfung behauptet.

Echte Disc-Tests selbst ausführen:

```sh
tools/test_host.sh /pfad/Disc1.gcm /pfad/Disc2.gcm /pfad/leerer-testordner
```

Originaldaten sind im Quellcode-ZIP nicht enthalten. Tests erzeugen große Datenmengen
und verändern den angegebenen Testordner; einen eigenen Testordner verwenden.

## Noch am Android-Gerät zu prüfen

Kein Android-Gerät oder Emulator war für einen Laufzeittest verfügbar.
Android-Dateiauswahl/Import, Rotation/Prozessneustart, Fokusverlust, USB-/Bluetooth-
Controller, Controller-Trennung, TV-Launcher und GLES-Kontextverlust sind deshalb
noch nicht praktisch abgenommen. Das Quellprojekt enthält die Implementierung,
aber die Controller-Mapping-Tests ersetzen diesen Gerätetest nicht.

Erster Gerätetest: App installieren, beide Discs importieren, Texturvorschau öffnen;
alle Tasten und Achsen kontrollieren, Trigger langsam bewegen, beide Sticks diagonal
bewegen, pausieren/wiederaufnehmen und Controller trennen. Mit und ohne A/B-X/Y-
Umschaltung testen. Zweiten Controller anschließen und Exklusivität des ersten prüfen.

## Engine-Status

Es gibt noch keinen Titelbildschirm, kein Gameplay, keinen GX-/TEV-Renderer,
kein Audio, kein Speichern und keinen RetroTouch. Das komplette Upstream liegt
als Referenz bei; nur ausdrücklich portierte Teile gelangen in die Android-Bibliothek.
API 21/GLES 3.0 ist die Mindestanforderung dieser Basis, keine gemessene Mindest-
Hardware oder zugesicherte Mindestversion für das spätere fertige Spiel.

## Milestone 02 — nachfolgende Prüfung am 2026-10-03

- Android `clean assembleDebug lintDebug`: BUILD SUCCESSFUL; **0 Fehler, 6 Warnungen** wie oben.
- Yz2: **alle 142 Raumarchive** beider Discs vollständig entpackt und byteweise mit
  dem unveränderten Original-PowerPC-Decoder verglichen; sämtliche Ausgaben identisch.
- Neuer Decoder-Test: alle 256 Einzel-Literale, Containergrenzen, Header, Budgets,
  uninitialisierte Referenzen und 2.000 deterministische veränderte Streams bestanden.
- Controller-Auswahl: **11 Host-Prüfungen** für virtuelle Navigation, Vorrang echter
  Stickgeräte, gleichrangige Zweitcontroller und Reset bestanden.
- Bestehende native Eingabe-/RNG-/TPL-Tests und 8 synthetische Disc-Prüfungen bestanden.
  Der Disc-Importer wurde in M02 nicht verändert; die 4.510 Vollimportprüfungen sind der M01-Nachweis.
- Yz2-Test unter AddressSanitizer/UndefinedBehaviorSanitizer bestanden; Leak-Prüfung
  weiterhin wegen der Ausführungsumgebung deaktiviert.
- Der vom Nutzer gelieferte Screenshot bestätigt den M01-Gerätestart und die echte
  32×32-CMPR-Textur unter GLES 3.2; kein Fatal-/ANR-Marker im beigefügten Log.
  Damit sind weder sämtliche Controllerfunktionen noch die NEUE M02-App praktisch abgenommen.

Version: 0.2.0-dev / versionCode 2. Mindest-API, ABI, Paketname und Disc-Datenformat unverändert.
Neue Geräteprüfung: r100/r300 müssen jeweils PASS melden. Danach tatsächliche
Controllerachsen und Trigger sowie den angezeigten Gerätenamen prüfen.

## Milestone 03 — nachfolgende Prüfung am 2026-10-03

- Nutzer-Screenshot: r100 und r300 bestanden auf Android die M02-Ausgabehashprüfung.
- Archivleser: 142 Räume, 5.005 Einträge und 13.075 Abfragen mit dem tatsächlichen
  Original-PowerPC-GetDataExt verglichen. Alle stimmen überein.
- Alle 335 Bilder der eingebetteten TPL-Teil-Dateien mit sämtlichen Bildindizes dekodiert.
- Archivverträge für wiederholte Tags, ignoriertes viertes Tagbyte, unsortierte Offsets,
  Aliases, Endposition, Header-/Offsetgrenzen und 3.000 veränderte Eingaben bestanden.
- CMake/CTest: native Verträge, Yz2-Verträge und Archiv-Verträge bestanden.
- Bestehende Host-Tests einschließlich Java-Disc-Verträge und Controller-Auswahl bestanden.
- Archivtest unter AddressSanitizer/UndefinedBehaviorSanitizer bestanden; Leak-Prüfung
  wie zuvor deaktiviert. Der Screenshot prüft nicht die NEUE M03-App.

Android-Quellstand inklusive ARM64/JNI, Java/Dex und Ressourcen tatsächlich gebaut:
`assembleDebug lintDebug --rerun-tasks --no-build-cache` bestanden, Lint 0 Fehler / 6
unveränderte Warnungen. Stale generierte R-Dex-Ausgaben wurden im Build-Verzeichnis
entfernt; keine Fehlerprüfung deaktiviert und keine Quellcode-Baseline angelegt.

Neue Android-Geräteprüfung: beide Raumprüfungen plus Archivliste und 256×256-
Raumtextur. Keine 3D-/Gameplay-Abnahme. Version 0.3.0-dev / Code 3.


## M03 Fix01 — 2026-10-03

Der neue Geräte-Log belegt die erfolgreiche r100-Archivprüfung und Vorbereitung
von TPL[0], Bild 6. r300 scheitert dagegen an der zusätzlichen JNI-Annahme,
dass EMI in jedem geprüften Raum enthalten sein müsse. Diese Annahme war falsch;
die bisherigen Host-Archivtests deckten diese zusätzliche JNI-Bedingung nicht ab.

Die unveränderte Originalausgabe von r300 hat 49 Einträge und kein EMI;
SHA-256 a0fccfc6bd1a608f7ed5868eff1d1f9869903234d288a35b261c4ff15bd94788.
Die pauschale Pflichtteilprüfung wurde entfernt. Die JNI-Inhaltsprüfung verwendet
nun die bestehende geprüfte Archiv-Inhaltsliste ohne zusätzliche Taganforderung.

Erneuter Vergleich aller 142 Räume, 5.005 Einträge, 13.075 Original-PowerPC-Abfragen
und 335 TPL-Bilder bestanden. Android assembleDebug und lintDebug mit
--rerun-tasks --no-build-cache bestanden; Lint 0 Fehler / 6 Warnungen.
Die korrigierte JNI-Funktion ist für ARM64 gebaut. Ein Lauf dieser Fix-Version
auf dem Android-Gerät steht noch aus: beide Räume erneut prüfen.


## M04 — 2026-10-03

Der Nutzer-Log nach M03-Fix01 bestätigt auf Android r100/r300 PASS, die
Vorbereitung der 256×256-Raumtextur und den Start der Vorschau ohne Fatal/ANR.
Die sichtbare Darstellung lässt sich allein aus dem Log nicht bestätigen.

M04: alle 142 Original-Raumarchive über Quelldatei-/Ausgabe-SHA-256 geprüft;
139 SMD-Dateien unterstützt, 4 ältere ausdrücklich noch nicht unterstützt.
22.922 Basis-Platzierungen, 11.852 lokale BIN-Tabelleneinträge, 4.979.572
Vertices und 29.751 Displaylist-Blöcke erfolgreich gelesen/geprüft.
11.991 lokale Resource-Verweise stimmen mit originalen PowerPC-Accessors überein.
Work-Start und logische Anzahl gegen getWorkPtr(0)/getWorkNum geprüft;
Vertexgrenzen gegen eine unabhängige Python-s16-Referenz. Einzelheiten: SMD.md.
Ein versuchter Original-getBoundingBox-Emulatorlauf endete mit UC_ERR_EXCEPTION;
kein bestandener binärer Bounding-Box-Nachweis wird daraus abgeleitet.

SMD-Verträge: Gruppentabelle, relative Tabellenbasen, Vertex-Skalierung,
Unveränderlichkeit, Common-Verweise, Trunkierungen, NaN, Shift-Limit,
relocierte Zeiger, Grenzen zwischen BIN/TPL und übergroße Streams bestanden.
Zusätzlich 3.000 deterministisch veränderte SMD-Eingaben; ASan/UBSan bestanden
(Leak-Prüfung wegen Hostumgebung deaktiviert). Assertions des neuen CMake-Tests
bleiben auch im Release-Build aktiv. Bestehende Host-/Java-Verträge bestanden.

Android ARM64/JNI, Java/Dex und Ressourcen: assembleDebug lintDebug mit
--rerun-tasks --no-build-cache bestanden, Lint 0 Fehler / 6 Warnungen.
Version 0.4.0-dev / versionCode 4. M04 noch nicht auf dem Nutzergerät geprüft.
Keine GX-Kommandodekodierung, Welttransformationen oder 3D-Raumdarstellung.


## M05 — 2026-10-03

Der M04-Gerätelog bestätigt r100/r300 PASS und alle drei SMD-Zusammenfassungen
mit den erwarteten Platzierungs-/Vertex-/Blockzahlen. Keine Fatal-/ANR-Marker.

M05: vollständiger Bytevergleich des nativen GX-Exports mit einer unabhängigen
Referenz aus beiden Originaldiscs, einschließlich Materialheader und Blockzuordnung:
142 Räume, 11.852 BIN-Tabelleneinträge, 696.593 Primitive, 12.340.412
Vertexreferenzen und 7.497.538 Dreiecke inklusive Degeneraten bestanden.
Quelldatei-/Yz2-Ausgabehashes unverändert geprüft. Keine Bild-/GPU-Abnahme daraus abgeleitet.
Die vier älteren SMD-Dateien bleiben ausdrücklich noch nicht unterstützt.

Fünf CMake-Vertragstests bestanden. GX-Verträge prüfen 6-/8-Byte-Referenzen,
Quad-Diagonale, Strip-Parität einschließlich Degeneraten, Fan-/Linien-/Punktfälle,
getrennte Attribute, Grenzen, Trunkierungen, VAT-/Kommando-Ablehnung und Budgets.
5.000 veränderte GX-Eingaben und 3.000 veränderte SMD-Eingaben; beide unter
ASan/UBSan bestanden. Leak-Prüfung weiterhin wegen Hostumgebung deaktiviert.
Bestehende native/Java-/Controller-Verträge bestanden; GX- und SMD-Assertions
bleiben auch im Release-CMake-Build aktiv.

Android ARM64/JNI, Java/Dex und Ressourcen: assembleDebug lintDebug mit
--rerun-tasks --no-build-cache bestanden, Lint 0 Fehler / 6 Warnungen.
Version 0.5.0-dev / versionCode 5. Neue Geräteprüfung siehe UPDATE-M05.md.
Noch keine Welttransformation, Modellhierarchie, Materialumsetzung oder 3D-Ansicht.


## M06 — 2026-10-03

Die nachgereichte M05-Geräteausgabe bestätigte r100 und r300 mit den erwarteten
GX-Zahlen und PASS. M06 ist noch nicht auf dem Gerät bestätigt.

- tools/test_host.sh: sämtliche nativen und Java-Verträge PASS, einschließlich
  neuer Matrix-, Ruhepose-, starrer Geometrie- und Auswahltests.
- Scene-Verträge zusätzlich mit AddressSanitizer und UndefinedBehaviorSanitizer
  bestanden (ASAN detect_leaks=0; keine Aussage über LeakSanitizer).
- Unabhängiger Datenvergleich aller 142 Räume: 139 SMD, 11.852 Gelenkzentren,
  22.922 Matrizen/Platzierungen und 11.653.738 Welt-Dreiecke PASS. Absolute
  Floattoleranz 0,125 Original-Welteinheiten; Details und Grenzen in SCENE.md.
- Optionaler CMake-Hosttest scene_gl_check erfolgreich gebaut und ausgeführt:
  echte GLES-3-Zeichnung auf Mesa OpenGL ES 3.2, Basis-Modell 29, 6.399 Dreiecke,
  16.107 Geometriepixel; GL-Fehlerprüfung und GPU-Aufräumen/Modellwechsel PASS.
- Android ARM64: assembleDebug und lintDebug BUILD SUCCESSFUL, 24 Sekunden,
  47 ausgeführte Tasks. Abschließender inkrementeller Build nach Korrektur
  der Rückwärts-Modellwahl aus der Gesamtansicht: ebenfalls erfolgreich,
  11 Sekunden (6 Tasks ausgeführt, 41 aktuell). Lint: keine Fehler, sechs Warnungen.

Der Datentest vergleicht die originale Ruhepose und Platzierung mathematisch,
nicht animierte Gelenkmatrizen, Common-Modelle oder Original-GPU-Bilder.
Die neue Vorschau verarbeitet Basis-Platzierungen und ist kein vollständig
sichtbarkeitskorrekter Raumrenderer. Texturen, TEV, Audio und Spielsimulation
bleiben ausstehend. Version 0.6.0-dev / versionCode 6; Gerätetest: UPDATE-M06.md.


## M06 — nachgereichte Geräteprüfung

Eingefügter Text(8).txt vom 2026-10-03 und Nutzerbestätigung: originale
untexturierte 3D-Objekte sichtbar. r100/r300 SHA-Prüfungen PASS, starre Zähler
13/6.020, 18/24.351 und 48/59.559 korrekt. Basis-Modell 29 mit 6.399 Dreiecken
und zahlreiche Modellwechsel protokolliert. Keine FATAL EXCEPTION, Fatal signal,
ANR der App oder 3D-Fehlermeldung im Mitschnitt. Dies bestätigt nicht separat
alle Hintergrund-/Kontextverlustfälle.

## M07 — 2026-10-03

- tools/test_host.sh: alle nativen/Java-Verträge PASS, einschließlich neuer UV-
  Grenzwerte, Nähte, Basistextur-Deduplizierung, Wrap-Override und Fehlpfade.
- Scene-/Materialtests zusätzlich mit ASan/UBSan PASS (Leakprüfung deaktiviert).
- Unabhängiger Vergleich aller 142 Räume PASS: 11.852 Modelle, 7.631.616 UV-Paare,
  1.156.057 primitiveweise Bereiche, 11.647.718 lokal texturierte und 6.020
  ungelöste Dreiecke. Werte und Zuordnungen exakt geprüft; siehe MATERIALS.md.
- Echter Mesa GLES-3.2-Test: Modell 29 mit 16 Originaltexturen, 6.399 texturierten
  Dreiecken, null ungelöst; 16.107 veränderte Pixel gegenüber gleicher
  untexturierter Ansicht. Visuelle Kontrolle und Modellwechsel/GPU-Aufräumen PASS.
- ARM64 assembleDebug/lintDebug erfolgreich; abschließender Build 15 Sekunden
  (6 Tasks ausgeführt, 41 aktuell). Lint: keine Fehler, sechs Warnungen.
  M07-Geräteprüfung ausstehend.

Version 0.7.0-dev / versionCode 7. Importdaten bleiben verwendbar. Diese Prüfung
belegt den Basistexturnachweis, kein vollständiges GameCube-TEV-Endbild.


## M07 — nachgereichte Geräteprüfung

Eingefügter Text(9).txt, 2026-10-03: r100/r300 mit erwarteten SHA256 PASS;
Modell 29 mit 6.399 texturierten Dreiecken und null ungelöst; Modellwechsel
protokolliert. Nutzer bestätigt die Darstellung. Kein FATAL EXCEPTION,
Fatal signal, App-ANR oder 3D-Fehler im Mitschnitt. Kontextverlust nicht
separat bestätigt.

## M08 — 2026-10-03

- Gesamte Hostverträge einschließlich I4/I8-Intensitätsalpha PASS.
- Scene-/Materialverträge zusätzlich mit ASan/UBSan bestanden
  (Leakprüfung deaktiviert).
- Unabhängiger Vergleich aller 142 Räume: Alpha-Zuordnung exakt geprüft;
  792.166 aufgelöste Alpha-Dreiecke, 3.543 ungelöst. Zusätzlich 1.458
  I4/I8-Alpha-Bilder direkt aus Original-Tile-Bytes geprüft.
- Echter Mesa-GLES-3.2-Test: strikte Grenzwerte, Basis-/Maskenalpha-Trennung,
  SourceAlpha-Blending und Erhalt unterschiedlicher Batch-Referenzen PASS.
  Modell 29: 21 Texturen, 6.399 Basis-Dreiecke, 304 Alpha-Dreiecke, null
  ungelöst. Visuelle Prüfung, Modellwechsel/Ressourcenabbau PASS.
- ARM64 assembleDebug/lintDebug erfolgreich: abschließender Build 15 Sekunden,
  6 Tasks ausgeführt und 41 aktuell. Lint: keine Fehler, sechs Warnungen.
  Native Input-/Texturverträge ebenfalls mit ASan/UBSan PASS (Leaks deaktiviert).
  Geräteprüfung ausstehend.

Version 0.8.0-dev / versionCode 8. Dies belegt einen statischen Alphamaskenzweig
und Formatkorrektur; kein vollständiges Runtime-TEV-Endbild.


## M09 — 2026-10-03

- M08-Geräterückmeldung: Original-Modell 29 mit 304 Alpha-Dreiecken und
  null ungelösten Masken; sichtbare Transparenzen vom Nutzer bestätigt.
- Hostverträge einschließlich gespeicherter Stufen, Dimensions-/LOD-Grenzen,
  abgeschnittener Daten, Nachbarbildschutz, verkürzter Originalketten und
  CMPR-Interpolation PASS. Native Input-/Texturtests zusätzlich mit
  AddressSanitizer/UndefinedBehaviorSanitizer PASS (Leaks deaktiviert).
- Unabhängiger Python-Vergleich aller 142 Originalraumarchive PASS:
  11.852 Modelle, 7.631.616 UV-Paare, 1.156.057 Material-Draws,
  11.647.718 Basis-Dreiecke, 6.020 ungelöste Basis-Dreiecke,
  792.166 Alpha-Dreiecke, 3.543 ungelöste Alpha-Dreiecke.
  1.458 I4/I8-Basisalphabilder sowie 2.501 Texturen mit gespeicherten Mipmaps,
  12.502 zusätzliche Stufen und 37.404.218 RGBA-Pixel aus Originalbytes
  unabhängig geprüft. Alle dabei verwendeten Mipmap-Formate sind I4/I8/CMPR;
  keine Behauptung über hier nicht auftretende Original-Mipmap-Formate.
- Echter Mesa-GLES-3.2-Framebuffer-Test PASS: ausdrücklich gelieferte grüne
  Zusatzstufe einer roten Basis wird bei Minifikation genutzt; ohne diese
  Stufe wird wieder rot gerendert. Die künstlich verkürzte Kette bleibt
  vollständig, keine GL-Fehler. M08-Alpha-Grenzwerte/Blending weiter PASS.
- r300-Modell 29: 6.399 texturierte Dreiecke, 21 Texturen,
  304 Alpha-Dreiecke, jeweils null ungelöst; vier Mipmap-Texturen mit
  20 Zusatzstufen. 14.500 sichtbare Mesh-Pixel; Modellwechsel/Abbau PASS.
- Abschließender ARM64 assembleDebug/lintDebug BUILD SUCCESSFUL:
  16 Sekunden, sechs Tasks ausgeführt, 41 aktuell. Lint: null Fehler,
  sechs Warnungen. M09-Gerätetest ausstehend.

Version 0.9.0-dev / versionCode 9. Originalstufen und begrenzte LOD-Ketten
sind belegt; keine vollständig portierte dynamische GX-Filtersteuerung,
TEV-Pipeline, Animation oder spielbare Engine. Details: MIPMAPS.md.


## M10 — 2026-10-03

- M09-Gerätelog Eingefügter Text(20261003-172126).txt bestätigt
  Modell 29 mit vier Mipmap-Texturen/20 Zusatzstufen, null begrenzten
  Ketten und 304 Alpha-Dreiecken ohne ungelöste Masken. Mehrere Modellwechsel
  protokolliert; Nutzer bestätigt Darstellung. Kein FATAL EXCEPTION/Fatal
  signal/ANR der App im übermittelten Ausschnitt. Externes Force-stop am Ende
  ist keine native Crashmeldung. Beim Öffnen erscheint eine verworfene
  SurfaceView-Buffer-Meldung; daraus folgt keine bestätigte dauerhafte
  Renderstörung. Keine Behauptung, Pause/Resume sei damit vollständig geprüft.
- Hostverträge PASS: RGBA-Bytefolge, Farbarraygrenzen, unabhängige Farbindizes,
  neutraler Registerzweig, aktive Farbquelle und ungültige Indizes.
- SMD- und Scene-Verträge zusätzlich mit ASan/UBSan PASS (Leaks deaktiviert).
- Unabhängiger Originaldatenvergleich aller 142 Raumarchive PASS:
  11.852 Modelle, 96.480 begrenzte RGBA-Einträge einschließlich Polster,
  52.135 Vertexfarb-Draws und 562.251 Vertexfarb-Dreiecke. Native Array-Hashes,
  getrennte Farbindexauflösung und RGBA-Hash jedes Szenen-Draws gegen
  direkte Originalbytes geprüft. Bestehende UV-, Basistextur-, Masken- und
  Mipmap-Prüfungen bleiben PASS: 12.502 Zusatzstufen, 37.404.218 Mipmap-Pixel,
  drei begrenzte Originalketten. Gemeinsame Materialzuordnung unverändert.
- Echter Mesa-GLES-3.2-Framebuffer PASS: RGB-Modulation, neutraler Registerzweig,
  Vertexalpha-Blending, kombinierte Masken-/Vertexalpha vor Grenzwertprüfung,
  getrennte Batches mit unterschiedlichen Farbquellen. M08/M09-Tests PASS.
- r300-App-Platzierung 18 / BIN 12: 180 aktive Vertexfarb-Dreiecke;
  gegenüber neutralem Farbzweig 14.929 veränderte Bildpunkte.
  Modell 29 unverändert: 6.399 texturierte, 304 Alpha-Dreiecke,
  null ungelöst; 14.500 sichtbare Mesh-Pixel, Modellwechsel/Abbau PASS.
- ARM64 assembleDebug/lintDebug BUILD SUCCESSFUL: 22 Sekunden,
  24 Tasks ausgeführt, 23 aktuell; Lint null Fehler/sechs Warnungen.
  M10-Gerätetest ausstehend.

Version 0.10.0-dev / versionCode 10. Belegter Farb-/Materialpfad in einer
Ambientvorschau mit weißem Registermaterial und ohne dynamische Lichter;
keine vollständige Runtime-Beleuchtung oder spielbare Engine.
Details: VERTEX-COLORS.md, UPDATE-M10.md.


## M11 — 2026-10-03

- M10-Gerätelog Eingefügter Text(20261003-181844).txt bestätigt Modell 18
  mit 180 Vertexfarb- und 180 Alpha-Dreiecken, null ungelöst. Gesamtansicht:
  48 Platzierungen, 59.559 Basis-Dreiecke, 9.201 Alpha-Dreiecke,
  2.454 Vertexfarb-Dreiecke, fünf Mipmap-Texturen/25 Zusatzstufen.
  Modell 29 unverändert. Kein FATAL EXCEPTION/Fatal signal/ANR der App im
  übermittelten Ausschnitt. Sichtbare Qualität von M10 wurde mit diesem
  Upload nicht ausdrücklich kommentiert; keine zusätzliche Sichtprüfung behauptet.
- Hostverträge PASS: S8-/S16-Grenzwerte, signed Umwandlung und implizite
  Faktoren, Arraystride/Polster, getrennte Normalindizes, inverse Transposition,
  ungleichmäßige/negative Skalierung und exakt singuläre Matrix.
  SMD-/Scene-Verträge mit ASan/UBSan PASS (Leaks deaktiviert).
- Unabhängiger Originaldatenvergleich aller 142 Räume PASS:
  139 unterstützte SMD, 11.852 Ruhegelenke, 22.922 Platzierungsmatrizen,
  22.922 starre Platzierungen, 11.653.738 Welt-Dreiecke;
  7.190.666 Original-S8-Normalen, 34.961.214 Welt-Normalenecken,
  3.961 Nullnormalen, null singuläre Platzierungen. Keine Original-S16-Normalen
  im Raumkorpus; S16 wird durch synthetische Werte geprüft.
  NumPy-Matrixinversion/Originalindizes unabhängig vom nativen Cofaktorweg;
  Normalentoleranz relativ 1e-4/absolut 1e-5, Positionsgrenze 0,125 unverändert.
- M10-Materialvergleich aller 142 Räume erneut PASS: UVs, Farbarrays und
  Farbindizes, Basis-/Maskenzuordnung, 12.502 Mipmap-Zusatzstufen und
  37.404.218 Mipmap-RGBA-Pixel. Bekannte ungelöste Common-Verweise unverändert.
- Echter Mesa-GLES-3.2-Framebuffer PASS: Originalnormal-Attribut,
  Nullnormalen-Markierung, Erhalt der Alpha-Grenzwerte, exakte Rückkehr zur
  Texturansicht; alle bisherigen Masken-, Farb- und Mipmap-Tests PASS.
- r300-Modell 29: 19.197 Normalenecken, null Nullnormalen/singuläre
  Platzierungen. Normalenansicht verändert 16.024 Pixel und stellt das
  ursprüngliche Textur-Framebuffer beim Rückwechsel exakt wieder her.
- Abschließender ARM64 assembleDebug/lintDebug BUILD SUCCESSFUL:
  15 Sekunden, sechs Tasks ausgeführt, 41 aktuell. Lint null Fehler,
  sechs Warnungen. Visuelle Hostprüfung der Normalenansicht erfolgt;
  M11-Geräteprüfung ausstehend.

Version 0.11.0-dev / versionCode 11. Kein neues künstliches Lichtmodell;
Originalattribute/Welttransformation plus umschaltbare Prüfansicht.
Weiterhin keine vollständige Beleuchtungs-/Spielengine. Details: NORMALS.md.


## M12 — gemeinsame SMD-Ressourcen und r100-Vorschau

Ausgangsbasis: M11. Das übermittelte Gerätelog Eingefügter Text(20261003-184536).txt bestätigt für Modell 29 19.197 Normalenecken, null Nullnormalen/singuläre Platzierungen und mehrfach Normalenansicht EIN/AUS. Im Ausschnitt kein FATAL EXCEPTION/Fatal signal/ANR für die App. Der Nutzer bestätigt dazu: „ja sieht alles gut aus soweit“. Diese Bestätigung betrifft M11, nicht M12.

Originalquellen: game.cpp Raumstart, scroll.cpp::SmdSetParam, model.cpp::addTplAddr, trans.cpp::commonModelTrans. Primäre Platzierungen und gemeinsame Ressourcen werden getrennt gehalten; Palette 0 der gemeinsamen SMD wird hinter den lokalen Deskriptoren angehängt. Gemeinsame BINs verwenden die leere lokale DmyZeroTpl-Semantik. Details: ROOM-RESOURCES.md.

Prüfungen auf dem Entwicklungsrechner:

- Vollständiger Disc-Katalog: 142 Räume; 138 unterstützte primäre SMDs. Nur r100 besitzt eine zweite SMD. Die übrigen vier primären Versionen bleiben außerhalb der geometrischen Vorschau.
- Unabhängiges Python-Originaldatenreferenzmodell: 1.152.480 gezeichnete primitive Gruppen, 11.629.387 aufgelöste Basisdreiecke. Platzierungs-/Quellwahl, lokale/gemeinsame Indexgrenzen und TPL-Bildmetadaten jeder Gruppe stimmen überein.
- r100: 13 primäre starre Platzierungen; 6.020 Dreiecke; alle Basisbilder aufgelöst; 6.020 gemeinsame Basisdreiecke und 3.543 gemeinsame Maskendreiecke. 552 gezeichnete primitive Gruppen. Gemeinsame SMD-Records werden nicht als zusätzliche Raum-Szene behandelt.
- r300: 48 starre Platzierungen, 59.559 Dreiecke, null ungelöste Basisbilder; keine Common-Quelle. 6.300 primitive Gruppen.
- Bestehender lokaler Materialreferenztest PASS: alle 142 Räume / 11.852 Modelle / 7.631.616 UV-Paare / 1.156.057 primitive Gruppen. Seine 6.020 ungelösten Basis- und 3.543 ungelösten Maskendreiecke sind absichtlich das unveränderte **lokale** Referenzverhalten ohne gemeinsame Quelle; der neue Raumtest löst genau diese auf. Die 18 Records der Ressourcen-SMD werden im lokalen Regressionstest separat getestet, im neuen Raumtest nicht zusätzlich gerendert.
- 2.501 Miptexturen, 12.502 Zusatzstufen, 37.404.218 unabhängig geprüfte RGBA-Mippixel, drei begrenzte Ketten PASS. 96.480 RGBA-Einträge, 52.135 Farbdraws, 562.251 Farbdreiecke PASS.
- Vollständige Host-Verträge einschließlich Java-Disc-/Controller-Tests PASS. Zusätzliche quellfreie Verträge für extern verlangte gemeinsame Tabellen, Common-BIN, leere lokale Palette, Quelle/Index-Cachetrennung, separate Masken und nicht auflösbare Renderziel-IDs PASS.
- Szenen-/Materialtests unter AddressSanitizer und UndefinedBehaviorSanitizer PASS (Leakprüfung deaktiviert).
- GLES-3-Hosttest mit Mesa: sämtliche bestehenden r300-, Alpha-, Mipmap-, Vertexfarben- und Normalenprüfungen PASS. Gemeinsame r100-Texturen ändern bei Basis-Modell 6 9.611 Pixel gegenüber der ungelösten Vorschau; Normalenansicht stellt das Texturbild anschließend exakt wieder her. Host-Framebuffer visuell geprüft: texturierte Brücke.
- ARM64/API21 `clean assembleDebug lintDebug` BUILD SUCCESSFUL in 34 Sekunden, alle 50 Tasks ausgeführt. Lint null Fehler, sechs vorhandene Warnungen. Vorheriger inkrementeller Versuch scheiterte an doppelten generierten R-Dex-Klassen; sauberer Build behebt dies ohne Quellcode-Workaround.

Gemeinsame BIN-Platzierungen mit Flag 0x10 existieren im geprüften Raumkorpus nicht; für diesen Zweig liegen quellfreie Tests, kein aktiver Originaldaten-Gerätenachweis vor. M12-Geräteprüfung ausstehend. Version 0.12.0-dev / versionCode 12; weiterhin statische Teilansicht ohne Spielsimulation.


## M13 — vollständige Raumwahl und SMX-Materialregister

Ausgangsbasis M12. Das neue Gerätelog Eingefügter Text(20261003-191155).txt bestätigt r100-Modell 6 mit 2.108 texturierten Basisdreiecken, 1.104 aufgelösten Alphamaskendreiecken, zwei Miptexturen/zehn Zusatzstufen, 6.324 Normalenecken ohne Nullnormalen/singuläre Platzierungen. Modellwechsel und Normalenansicht EIN/AUS sind protokolliert; im übermittelten Ausschnitt kein App-FATAL EXCEPTION/Fatal signal/ANR. Eine explizite visuelle M12-Bestätigung enthält die begleitende Nutzeranfrage nicht.

M13 portiert das SMX-Dateilayout und die belegte Material-RGB-Initialisierung. Work-ID-Zuordnung, erster Treffer, Null-RGB auf weiß und Blendbyte getrennt vom Material-Alpha entsprechen scroll.cpp::smxInit. Die Raumwahl umfasst den gesamten vorhandenen 142-Raum-Katalog mit Quell-/Ausgabe-SHA256, nativer Vorschau-Vorprüfung und atomarer Cache-Aktivierung. Kein Schreiben in installierte Disc-Daten. Details: SMX.md.

Entwicklungsrechner-Prüfungen:

- Vollständiger unabhängiger Originaldatenvergleich für 142 Räume / 138 unterstützte primäre SMDs PASS: 1.152.480 gezeichnete Primitive, 11.629.387 texturierte Basisdreiecke, darunter 6.020 gemeinsame Basis- und 3.543 gemeinsame Maskendreiecke. Primitive-Work-ID und daraus abgeleitetes Material-RGB stimmen einzeln überein.
- Alle 7.777 SMX-Records des Katalogs PASS: ID-/Typ-/Order-/Cull-Bytes, Masken, Flags, Material-/Specularworte, UV-Floatbits und sämtliche 120 Objekt-Work-Bytes. 1.131.104 gezeichnete Dreiecke erhalten nichtweiße Materialfarben.
- Vollständige Host-Verträge PASS. Neue quellfreie SMX-Verträge prüfen erste Treffer, IDs über 0xf9/unregistriertes Work, Null-RGB, Material-/Blend-Trennung, abgeschnittene Records und nicht endliche UV-Werte.
- Abschließende Szenentests mit AddressSanitizer/UndefinedBehaviorSanitizer PASS; Leakprüfung deaktiviert.
- GLES/Mesa: alle bestehenden Basistextur-/Alphamasken-/Mip-/Vertexfarb-/Normalenprüfungen PASS. Neue getrennte rote/blaue Materialregister bleiben trotz benachbarter Batches getrennt; der Textur-Alpha-Nullwert wird weiterhin nicht als Basis-Deckkraft benutzt.
- Reales r100-Work 0: 94 Dreiecke, Original-SMX RGB42/42/42; Materialfarbe verändert 4.070 Framebufferpixel. Textur-/Materialansicht wird nach Normalen-EIN/AUS exakt wiederhergestellt.
- r100-Brücke (Modell 6): 85 SMX-Records im Raum, 234 passende Primitive im ausgewählten Modell, null zusätzlich getönte Dreiecke (weißes Register). Common-Texturprüfung weiterhin 6.020/3.543 Gesamtzähler und 9.611 gegenüber der ungelösten Vorschau veränderte Pixel PASS.
- Abschließender ARM64/API21 `clean assembleDebug lintDebug`: BUILD SUCCESSFUL, 27 Sekunden, alle 50 Tasks ausgeführt. Lint null Fehler/sechs vorhandene Warnungen. Inkrementelle Zwischenprüfung nach Ressourcenänderung traf wieder auf doppelte generierte R-Dex-Klassen; sauberer Build besteht ohne Quellcode-Workaround.

Die Android-Raumwahl und ihr Activity-/Surface-/Controller-Lebenszyklus wurden gebaut und statisch geprüft, aber noch nicht auf dem Nutzergerät getestet. SMX-Lichtmasken, Objekttypen, Sortierung, Culling, Blend-/TEV-Sonderzustände und UV-Geschwindigkeiten sind keine aktive Original-Runtime. Gruppen-/Skriptinstanzen, Animation und Gameplay fehlen weiterhin. Version 0.13.0-dev / versionCode 13.

## M14 — SAT-Kollisionsdaten (2026-10-03)

- Das vorgelegte M13-Log bestätigt erfolgreich geladene r100-/r111-Ansichten und Modellwechsel; der Ausschnitt enthält keine FATAL EXCEPTION/Fatal signal/ANR. Daraus wird keine zusätzliche visuelle Gerätebestätigung abgeleitet.
- Unabhängiges Python-BE-Orakel gegen C++: alle 142 referenzierten Raumarchive, 152 SAT-Vorkommen, jeweils Variante 0. 95.310 Polygone, 53.268 räumliche Blöcke, 417.445 Blattverweise. SHA-256 der gepackten/entpackten Archive geprüft; alle Polygonindizes, Attribute, reservierte Polygonfelder und referenzierten Position-/Normalen-Floatbits stimmen im deterministischen Prüfwert überein. Parser mit zusätzlicher Bereichs-/Überlappungsprüfung bestanden.
- CTest: 7/7 bestanden. Host-Skript: native, Disc-, Controller-, Archiv-, SMD-, GX-, Scene- und neue SAT-Verträge bestanden. SAT prüft alle Dateikürzungen, NaN, ungültige Indexwerte, Gruppen, Kantenflags, Blockbudget und Teil-Dateigrenzen. ASan+UBSan für SAT bestanden; LeakSanitizer deaktiviert.
- GLES3 mit Software-EGL: alle bestehenden Renderer-Regressionen bestanden. SAT r300: 890 Dreiecke, Gruppenpixel 321/0/17.878; r100: 1.507 Dreiecke, Gruppenpixel 53/10/3.645. Farben in Boden/Neigung/Wand-Reihenfolge. Die fehlenden gelben Pixel im r300-Startwinkel sind keine fehlende Neigungsgeometrie. Beide Ansichten: keine GL-Fehler, Normalen verändern den Framebuffer, anschließende Gruppenansicht identisch. r100-Bild visuell auf Geometrie/Farben geprüft.
- ARM64 Android: `clean assembleDebug lintDebug --offline` erfolgreich in 29 Sekunden; 50 Tasks, 38 ausgeführt, 12 unverändert. JNI/C++, Ressourcen, Dex und APK gebaut. Lint 0 Fehler, 6 bestehende Warnungen.
- Kein Geräte- oder Gameplaytest des neuen M14. Keine Boden-/Sweep-Abfragen, Spielerkollision, dynamischen SAT-Varianten oder originale Physik behauptet.

## M15 — Begehbare SAT-Diagnose (2026-10-03)

- Das M14-Gerätelog vom Nutzer (`Eingefügter Text(20261003-200125).txt`) zeigt erfolgreiche SAT-Umschaltungen von r100 und Rückkehr zum Basis-Modell. Die Prozessbeendigungen sind Force-Stops durch Installation beziehungsweise Launcher; kein FATAL EXCEPTION/Fatal signal/ANR im vorgelegten Ausschnitt. Das bestätigt die protokollierten M14-Umschaltungen, nicht schon M15 auf dem Gerät.
- Unabhängige Originaldatenprüfung über beide Discs: SHA-256 jedes gepackten/entpackten katalogisierten Raumarchivs; **142 Räume, 4.750 nächste Linienabfragen** stimmen bezüglich Polygon, Hit-Attribut, Segmentanteil und Weltpunkt mit der Python-Referenz überein. **141 freie Startpunkte / 141 viersekündige Bewegungsspuren** enden auf gültigen, nicht für Spieler ausgeschlossenen Bodenpolygonebenen. Ein SAT-Archiv liefert mit den Diagnosemaßen keinen freien Start. Die bestehende SMD-Unterstützungsgrenze bleibt getrennt davon bestehen.
- CTest: **8/8 bestanden**. Host-Tests einschließlich Java-Disc-Tests (8), Controller-Policy (11), aller bisherigen nativen Verträge, SAT und neuen Kollisionsverträgen bestanden. Neue analytische Fälle: Bodenhöhe, nächster Wandtreffer, Null/parallel, dünne Wand bei hoher Weglänge, Kapselberührung/Rückzug, Wandgleiten, Randbegrenzung, 30-/60-Hz-Weggleichheit, Attribute, NaN und degenerierte/gesperrte Startflächen.
- ASan + UBSan für SAT/Kollisionsabfragen/Bewegung bestanden. LeakSanitizer deaktiviert; keine Leak-Prüfung behauptet.
- Software-EGL/GLES3: frühere Textur-, Alpha-, Mip-, Vertexfarben-, Normalen-, Common- und SMX-Regressionen bestanden. Neue Followkamera und Marker mit SAT sowie kompletter unterstützter statischer Texturgeometrie für r300/r100: **1.694 / 2.852 gelbe Kapselpixel**, kein GL-Fehler, exakte Rückkehr nach Normalenmodus. Textur-Framebuffer visuell geprüft; r300 zeigt Gebäude/Boden/Masken und die Drahtkapsel, r100 kann an den diagnostischen Bodenpunkten sehr dunkel sein. Neue Kamera-, Kapsel- und Fußmarkengeometrie sind Diagnoseelemente.
- Finales ARM64 `clean assembleDebug lintDebug --offline`: **BUILD SUCCESSFUL in 29s**, 50/50 Tasks ausgeführt. Java/Dex, JNI/C++, Ressourcen und APK gebaut. Lint: **0 Fehler, 6 bestehende Warnungen**. Android-Gerätetest des neuen M15 noch ausstehend.
- Keine originale Leon-/Animationsruntime, originaler Spawn, dynamische SAT/EAT-Objekte, Tür-/Raumwechsel, Gegner, Audio oder fertiges Gameplay. Boden-/Kapsel-/Kameraverhalten ist gemäß WALK-PROBE.md klar abgegrenzt.

## M16 — Original-Leon, Motion64 und Rendererzustände (2026-10-03)

- Das aktuelle Nutzerlog `Eingefügter Text(20261003-203725).txt` zeigt r300-Bewegung über mehrere Boden-/Neigungspolygone und Wandkontakt, ohne Fatal-/ANR-Marker im vorliegenden Ausschnitt. Der Nutzer bestätigt die begehbare Ansicht und meldet Durchsicht bei einigen Flächen. Eine einzelne Ursache sämtlicher Lücken ist damit nicht bewiesen. Belegte Korrekturen: bisherige Kameraabfrage ohne Böden/Schrägen und pauschales Blending unabhängig vom originalen SMX-Modus.
- PL00/WEP01 aus der Disc-Dateitabelle extrahiert, Größe und SHA-256 geprüft. **119 Gelenke, sieben Modelle, 9.096 Dreiecke, elf Texturen** vollständig aufgelöst; keine Originalassets im Patch.
- Unabhängiger Vergleich mit der Upstream-Hostauswertung: **18 Posen × 119 Gelenke**, Idle/Walk/Run, originale Kurven-IK und Mischgelenke. Größte Matrixabweichung **0,0006104 Spieleinheiten**. Floatberechnung ist keine zugesagte Bitgleichheit mit PowerPC/Original-libm. Zusätzlich 300 endliche Posen über echte SEQ-Loops und Hokan-Wechsel; diese Spur ist kein vollständiger unabhängiger SEQ-Goldenvergleich.
- Unabhängiger Python-BIN-Vergleich: **27.288 Modellecken**, Position/Normalen und jeweils separate Gelenkgewichte stimmen überein. Generator reproduziert alle vier eingecheckten Motion64-Dateien exakt. Native Zeiger und Dateioffsets bleiben getrennt; keine Low-Address-Allokation im Android-Code.
- Alle **142 Raumarchive**, 138 unterstützte primäre SMDs: **7.777 SMX-Records / 1.152.480 Draw-Primitiven** geprüft. Darunter **11.629.387 texturierte Dreiecke** sowie die bisherigen gemeinsamen Textur-/Maskenquellen. Jeder Draw wird zusätzlich auf Blend-Rohwert, OT-Typ, Alpha-Grenzwert und UV-Floatbits geprüft. Drei Records mit unbelegtem Blendmodus 5 behalten den Rohwert und verwenden den dokumentierten Vorschau-Rückfall; nicht als vollständige Originalfunktion gewertet.
- **CTest 9/9**, Host-Skript einschließlich Java-Disc- und Controller-Verträgen, neuen Animation-/Skinningverträgen und Kugelkameratreffern gegen Böden bestanden. Neue Fälle prüfen auch SEE-Flächen, die Spielerfilter auslassen, sowie FCV/SEQ-Kürzungen, ungültige Formate/Offsets/Frames/Gelenke/Gewichte.
- **ASan + UBSan 9/9** mit Abbruch bei Befund bestanden. Die echte Leon-Auswertung mit 18 Stichproben und 300 SEQ-Schritten ebenfalls ohne Befund; vollständige Ausgabe 3.999.608 Byte. LeakSanitizer weiterhin deaktiviert; keine Leak-Prüfung behauptet.
- Tatsächlicher GLES3-Vertexshader mit Software-EGL: drei Posen, alle **27.288 Modellecken**, separate Positions-/Normalengewichte per Transform Feedback gegen unabhängige Double-Auswertung. Größte Abweichung **0,000336869 Spieleinheiten**. Original-Leon ist in r300 mit **8.327 abweichenden sichtbaren Pixeln** nachgewiesen. Texturen/Normalenwechsel, exakte Rückkehr, GPU-Neuaufbau, 120 bewegte Raumframes und Ausblenden bei Kamera unter 1.000 Einheiten bestanden. Framebuffer visuell geprüft. Das ist kein Adreno-/Android-Gerätetest.
- Raum-GLES-Regressionen bestanden: alle fünf belegten Blendmodi, maskierter Modus 4 ohne Blend-Durchsicht, ursprüngliche Textur-/Alpha-/Mip-/Vertex-/Normalen-/Common-Prüfungen, r300/r100 mit Followkamera und pixelgenauer Rückkehr bei eingefrorener Zeit. r100-UV-Scroll verändert tatsächlich den Framebuffer und bleibt nach Normalenwechsel erhalten.
- Android ARM64 **`clean assembleDebug lintDebug --offline` erfolgreich in 29 s**; anschließender Build der letzten Actor-Batch-/GPU-Rückfalländerung **erfolgreich in 18 s**. Lint **0 Fehler / 6 bestehende Warnungen**. Finale APK enthält ausschließlich ARM64 `libre4decomp.so`, ELF64/AArch64 bestätigt; alle PT_LOAD-Segmente 16-KiB-ausgerichtet. Keine APK im Quellpatch mitgeliefert.
- M16-Gerätetest steht bei Auslieferung aus. Original-Spawn, vollständige cPlayer-/Kamera-/Szenarioruntime, Waffenmodell/Kampf, Gegner, Audio, Inventar, Speichern, dynamische Raumobjekte, Lichter/Schatten und vollständige Sichtbarkeit fehlen weiterhin. Die eigene M15-Bewegung und die dokumentierten Vorschau-Rückfälle werden nicht als fertiges Spiel ausgegeben.

## M17 — Originale Bewegungswege und nativer Türlauf (2026-10-03/04)

- Neues Gerätelog `Eingefügter Text(20261003-221022).txt`: M16-Leon mit **119 Gelenken, sieben Modellen und elf Texturen**, Stand/Gehen/Laufen protokolliert. Der Nutzer bestätigt die sichtbaren korrekten Animationen nach sauberem Neubau. Das belegt M16 auf seinem Gerät; daraus folgt keine zusätzliche Bestätigung sämtlicher zuvor gemeldeter Material-/Sichtbarkeitsprobleme.
- Sieben echte WEP01-FCV/SEQ-Paare: Root-Zyklen, **20 Schleifenübergänge**, volle/halbe analoge Wiedergabe, **30/60-Hz-Gleichheit von Position und Pose**, Gehen/Laufen-Phasenwechsel, vollständige Quickturn-Auswertung, Press-Flanke bei zunächst null Zeit, gehaltene Taste ohne Neustart, Suspend und Wandblockierung/Rückzug bestanden. Physisches Links/Rechts ist zusätzlich gegen Original-`pad.cpp` und den rechten Vektor der nativen Hinterkamera geprüft; irreführende Original-JOY-Namen werden nicht als Achsenvorzeichen übernommen. Terrain-Fuß-IK ist weiterhin nicht angebunden.
- Erneuter unabhängiger M16-Upstream-/BIN-Vergleich bestanden: **18 Posen × 119 Gelenke**, maximale Matrixabweichung **0,0006104 Spieleinheiten**, 300 endliche SEQ-Schritte und **27.288 Modellecken**. Die unabhängige Pose-Referenz deckt die ersten drei Clips ab; die sieben neuen Laufzeit-Zustände sind kein vollständiger cPlayer-Goldenvergleich.
- Neue RoomInfo-/AEV-Validierung gegen unabhängige Python-BE-/Geometrieauswertung und die zwei Originaldiscs: **142 Räume, 289 Türen, 5.202 Bereichs-/Richtungsabfragen, 222 exakte Debug-Startdatensätze**; beide RoomInfo-Dateien identisch nach SHA-256. Genau die zwei geprüften Tür-0-Übergänge r300/r301 sind ausführbar. Andere Türen werden als Diagnosedaten behalten und nicht pauschal freigegeben.
- Host-Skript vollständig bestanden, einschließlich **64 synthetischer Java-RoomRepository-Checks**. Mit echtem RoomInfo-Fixture und 142-Zeilen-Katalog **68 Java-Checks** bestanden. Cache-/Quellen-/Ausgabehashes, Pfadgrenzen/Symlinks, fehlende Quellen, inkonsistenter Katalog, Unterbrechung und schlechte Decoderausgabe mit Erhalt des bisherigen Caches geprüft.
- **CTest 11/11** bestanden. **ASan + UBSan 11/11** mit Abbruch bei Befund bestanden; die tatsächliche Leon-Bewegung ebenfalls ohne Befund, einschließlich korrigierter Links-/Rechtssteuerung. LeakSanitizer bleibt deaktiviert; keine Leak-Prüfung behauptet.
- Echter Produktions-JNI-/GLES-Bridge-Test mit Host-JNI-Strings und Software-EGL: **236 Checks** bestanden. Tatsächliche Controlleraktionen B/zurück und A bewirken **r301 → r300 → r301**, einschließlich originaler AEV-Zielposition/-richtung und gehaltenem A ohne neue Aktion. Worker-Ergebnis ohne EGL, anschließender neuer GL-Kontext, Pause, aktueller Raum statt ursprünglicher Activity-Intent, veraltete Session-/Anfrage-Tokens und vollständige Kameraparameter/UV-Zeit geprüft.
- Der gleiche Test erzwingt **einen realen GLES-Fehler beim Actor-Uniformbuffer-Upload** ausschließlich über eine Host-Linkerhülle. Der GPU-Fehler wird als Fehler protokolliert, behält den alten CPU-Raum, dieselbe Leon-Instanz, Position, Richtung, Pose, Sequenzphase, Pause und Kamera und stellt seine GPU wieder her. Ein anschließender neuer Raumwechsel gelingt. Keine Fault-Injection-API wird in Android eingebaut. Fehlende Disc-Daten und fehlende Raumdatei behalten ebenfalls den laufenden Zustand.
- Bestehende reale GLES3-Shader-/Framebufferprüfungen bestanden: **drei Posen / 27.288 Modellecken**, maximale Skinningabweichung **0,000336869 Spieleinheiten**, **8.327 sichtbare Actorpixel**, 120 bewegte Raumframes, Masken/Texturen/Normalen/GPU-Neuaufbau. Raumtests für Blendmodi, Common-Ressourcen, UV-Scroll, Followkamera und SAT bestehen. Der r301-Bridge-Framebuffer wurde visuell geprüft; Kamera und statische Raumdarstellung bleiben die dokumentierte Vorschau.
- Finaler Android-ARM64-Build **`clean assembleDebug lintDebug --offline`: BUILD SUCCESSFUL in 31 s, 50/50 Tasks ausgeführt**. Nach ausschließlicher Quellformatierung erneuter Build/Lint **erfolgreich in 26 s**. **Lint: 0 Fehler / 6 bestehende Warnungen**. APK ausschließlich `lib/arm64-v8a/libre4decomp.so`, ELF64/AArch64; sämtliche PT_LOAD-Segmente **0x4000 / 16 KiB** ausgerichtet.
- Version **0.17.0-dev / Code 17**. M17-Test auf dem Nutzergerät steht aus. Originale Debug-Startdaten, Root-Bewegung und zwei direkte Raumwechsel sind jetzt angebunden; vollständige cPlayer-/Kamera-/Szenario-/Leiter-/Waffen-/Gegnerruntime, Audio, Inventar, Speichern, dynamische Sichtbarkeit, Raumlicht und Schatten bleiben offen.

## Milestone 18 — Waffenmodell und native Spieleraktionen

- Der neue Nutzerbericht und `Eingefügter Text(20261003-225513).txt` bestätigen **M17 r301 → r300 → r301** auf dem Android-Gerät. Die beiden ursprünglichen Zielpositionen erscheinen als PASS im Log. Damit wird weder M18 auf dem Gerät noch die vollständige Behebung früherer Durchsichten behauptet.
- Die normale originale WEP01-Handfeuerwaffe besitzt **605 Dreiecke / 1.815 Modellecken / 93 Draws / eine Textur / vier eigene Gelenke**. Der unabhängige BIN-/TPL-Vergleich prüft sämtliche Ecken und **8.192 RGBA-Pixel bytegenau**. Die separat kompilierten Original-MotionMove-/IK-Prüfungen bestätigen **66 Waffenposen**, maximale Matrixabweichung **0,0000001 Originaleinheiten**, sowie **594 Anbindungs-/Normalenvergleiche** an der rohen Leon-Handmatrix 10 mit abweichender Platzierung/Ausrichtung, maximal **0,0003802**.
- Die neue unabhängige Original-Aktionsreferenz umfasst **612 Posen**, 119 Körper- und vier Waffengelenke: draw/down/aim/fire/reload, cMot3-Endpunkte/Mittelwerte/Vorzeichenwechsel, Hokan, Taille und SEQ. Maximale Matrixabweichung **0,00097656 Spieleinheiten**; Curve-/SEQ-/Zustands- und Crossframe-Werte stimmen exakt. Es handelt sich um einen Animations-/Adaptervergleich, keinen vollständigen Original-cPlayer-/Inventar-/Kampf-Goldenlauf.
- M16-Baseline erneut bestanden: **18 Originalposen**, 119 Gelenke, maximale Abweichung **0,0006104**, 300 endliche SEQ-Schritte, **27.288 Modellecken**. Die sieben M17-Bewegungsclips bestehen weiterhin Root-Zyklen, 20 Loopübergänge, 30/60-Hz-Gleichheit, analoge Wiedergabe, Phase, Quickturn-Kanten und Wandblockierung.
- Reale Handgun-Aktionstests bestehen stationäres Anlegen/Zielen/Ablegen, echte Handrichtung oben/unten, Taille und endliche Posen, **14-Tick-Schusskadenz**, 30/60-Hz-Gleichheit, Nullzeitflanken, Trigger-Neutralfreigabe, Mindest-Recoilzeit, voller/leerer Magazinbestand und einmaliger Reloadtransfer nach Waffenframe 31. **70 Testpatronen** werden über alle Schüsse und Reloads erhalten, einschließlich Reserve 0 und Placementwechsel.
- **CTest 12/12**, das vollständige Host-Skript einschließlich 64 synthetischer Java-RoomRepository-Prüfungen und **ASan/UBSan 12/12** bestanden. Der reale Handgun-Aktionslauf besteht zusätzlich unter ASan/UBSan mit Abbruch bei Befund. LeakSanitizer bleibt für diese Umgebung deaktiviert; keine Speicherleck- oder Android-Sanitizer-Prüfung behauptet.
- Tatsächlicher GLES3-Waffenshader: **90 Posen / 1.815 Ecken**, maximale Weltpositions-/Normalenabweichung **0,0001072**, **795 sichtbare Waffenpixel**. Die Prüfung verwendet getrennte CPU-Eltern-/Skinningauswertung und Transform Feedback des Produktionsshaders; Textur-/Normalenwechsel, Cleanup/Neuaufbau und ein realer Waffen-Uniformbufferfehler erhalten beide bestehenden GPU-Instanzen. Bestehender Leon-GPU-Lauf weiterhin bestanden: drei Posen, 27.288 Ecken, maximal **0,000336869**, 8.327 sichtbare Pixel und 120 bewegte Raumframes.
- Echte Produktions-JNI-/GLES-Brücke: **1.108 Checks bestanden**. Sämtliche M17-Fälle bleiben erhalten: reale A-Türen, Worker-Completion ohne EGL, veraltete Tokens, schlechte Quelldatei, Actor-GPU-Fehler und Kontextneuanlage. Neue Fälle bedienen LT/RT/X über tatsächliche JNI-Eingaben, prüfen Mündungsrichtung, Pause mit gehaltenem RT, Trigger-Neutralfreigabe nach Kontextverlust, exakt erhaltene Pose beim Inspektorwechsel, Ammo über echte Türen und Rollback nach dem zweiten Uniformbuffer-Upload (Waffe). Körper-/Hand-/Waffenmatrizen, Motionframes, Aktion, Magazin/Reserve/Schusszahl, Füße, Ausrichtung und Kamerasicht bleiben beim Fehler erhalten; die nächste gute Anfrage gelingt. Fehlerausgaben in diesem Hostlauf sind gezielt injizierte Testfälle, keine Fehler des normalen Android-Builds.
- Der Integrationstest fand einen echten Resume-Fehler: ein Grafik-Kontextwechsel allein setzte zuvor die Triggerfreigabe nicht zurück. `createScene` erhält jetzt Pose/Bestand und fordert physische Neutralstellung vor neuen Aktionen. Der anschließende gesamte Integrationstest besteht. Die Zielkamera wurde anhand echter r301-/r300-Framebuffer kontrolliert: Schulterhöhe bleibt auch bei verkürztem SAT-Abstand erhalten. Bei sehr kurzem Abstand bleibt die frühere dokumentierte Modellausblendung aktiv.
- Android **`clean assembleDebug lintDebug --offline`: BUILD SUCCESSFUL in 34 s**, 50/50 Tasks ausgeführt. Nach der letzten Kamerakorrektur **`assembleDebug lintDebug --offline`: BUILD SUCCESSFUL in 16 s**, alle betroffenen Ziele aktualisiert. **Lint 0 Fehler / 6 bestehende Warnungen**. APK ausschließlich `lib/arm64-v8a/libre4decomp.so`, 2.267.112 Byte, ELF64/AArch64; alle drei PT_LOAD-Segmente **16 KiB** ausgerichtet. Das APK wurde gebaut, aber nicht auf einem Android-Gerät ausgeführt.
- Version **0.18.0-dev / Code 18**. ZIP enthält vollständige geänderte/neue Quelldateien mit Projektordnerstruktur, keine Spielassets, APKs oder lokalen SDK-Pfade. Native Handgun-Aktionen und Testmunition sind implementiert; Gegner, Treffer/Schaden, Originalinventar, Audio, Szenario, Speichern, dynamische Objekte/TEV und vollständige Player-/Kameraruntime bleiben offen.


## Milestone 19 — Prüfung am 2026-10-04

M18-Gerätelog `Eingefügter Text(20261003-234535).txt` bestätigt originale Leon-/Waffenanimation, Schüsse, Nachladen und die Testmunitionsbilanz. Nutzer bestätigt die Animationen. M19 ist davon getrennt: **noch kein Android-Gerätetest dieser Version**.

- Android ARM64/API21: abschließendes `clean assembleDebug lintDebug`, gleiche gepinnte Werkzeuge; Java/JNI/C++/Ressourcen/APK gebaut. Lint weiterhin 0 Fehler und 6 bekannte Warnungen.
- Eigenes Hostskript mit strikten Warnungen und Assertions sowie CMake-Verträge: **15/15 bestanden**. Der Asset-freie CTest-Lauf ersetzt keine Originaldatenprüfung; diese wurde zusätzlich separat ausgeführt.
- ASAN/UBSAN: 15/15 Verträge und echter EM12/r100/ESL/PL00/WEP01-Kampftest bestanden. LeakSanitizer wegen der Containerumgebung deaktiviert; keine Leak- oder Android-Sanitizer-Prüfung behauptet.
- Beide originalen ESL00-Dateien identisch: 8.160 Byte und SHA65cd90…5126. BE-Layout, 255 Datensätze, Rohdatenbesitz, Winkelgrenzen und verkürzte/falsche Dateien geprüft. Auditiert: unveränderter Datensatz0x25, ID12, Typ0, HP1000, Listenindex37. Originaler Szenario-Spielerstart und beide SAT-Kapseln sind gültig; freier Laufweg und EAT-Sichtlinie geprüft.
- EM12-Referenz: **933 rohe Gelenkposen**, maximal0,00057983 Originaleinheiten; sieben originale Root-/SEQ-/Free-/Mirror-Spuren einschließlich drei Gehschleifen, Rootdelta exakt gleich. Vier BIN-Modelle, 34 gemeinsame Gelenke, 3.013 Dreiecke, 9.039 Ecken und drei aufgelöste Texturen.
- Trefferreferenz: **5.103 Vergleiche** mit getrennt extrahiertem Originalspiel-/SDK-/fdlibm-Code; 5.021 bitgleich, größte Schnittpunktabweichung0,0205078 Einheiten, relative quadratische Distanzabweichung2,59e-6. Fünf nicht endliche Originaldegenerationen werden sicher verworfen. Kapsel-/Boxgrenzen, Achsen, rohe einsbasierte Gelenke, Handgun-Ausschluss, Bereich und spätere Zone bei Abstandsgleichheit geprüft.
- Treffer-/EAT-Verträge:137 synthetische Prüfungen; mit echten r300/r301-EAT-Daten **273 Prüfungen**, inklusive Quell-Ausschlussregeln und Raumstrahlen. EAT nutzt den bisherigen nativen Dreieck-BVH; kein bitgenauer kompletter EatMgr-Port behauptet.
- Echter Encounter: **3.383 Prüfungen** für Quelle/HP/Scale, Root/SAT-Annäherung, Schaden im Folgetakt und kurze Sperre, Körper135/Kopf162/kritisch1350, zwei originale LifeDown-RNG-Bytes, verbleibenden Schaden55 beim achten Körpertreffer, gehaltene finale Todespose, echte EAT-Wandbegrenzung, ungültige Schüsse ohne partiellen Serienzustand, Akteurkontakt und Streif-/Rückzugsfälle. Tatsächliches LT/RT-Schießen erhält die Munition; Gegnerpose/Platzierung/HP/RNG sind bei30/60Hz exakt gleich.
- Tatsächlicher Gegner-Vertexshader: **56 Posen mit9.039 Ecken**, maximale lokale/Weltpositionsabweichung0,00409796 Einheiten; sichtbarer Framebuffer, ausgeschalteter Originalalphatest, finale Leiche, drei getrennte Akteurinstanzen, UBO-Fehler/Rollback, Ressourcenersatz und EGL-Kontextwechsel geprüft. Leon-/Waffen-GPU-Regressionen bestehen weiter.
- Echte Produktions-JNI/GLES-Brücke: **1.699 Prüfungen** mit PL00/WEP01, r300/r301, Rauminfo, EM12, ESL00 und r100. Tatsächlicher RT-Körpertreffer reduziert HP1000→865. Pause, Inspektor, Triggerneutralisierung, Kontextneuerstellung, CPU-Fehler, gezielter dritter UBO-Fehler, Wiederholung und Türtest-Rückkehr erhalten relevante CPU/GPU-Zustände und Munition.
- Visuelle Abschlussprüfung fand verdeckten Gegner durch Leons Körper bei wandverkürzter Kamera. Der SAT-geprüfte Schulteroffset900 statt450 korrigiert die eigene Zielkamera. Zusätzlicher Framebuffervergleich ohne Gegner misst **3.331 sichtbare Gegnergeometrie-Pixel**, rote Diagnoselinie ausgenommen, über der neuen Mindestprüfung2000. Initialstand, Zielansicht, Treffer, Leiche und Rollback wurden visuell geprüft.
- Originalquellprüfung bestätigt Aim-only-Laseranforderung: Bereitschaft erneuert sie, Feuer nicht. Die spätere Original-Rückstoßpose (etwa42° nach oben) wird nicht als fortdauernde Ziellinie gezeichnet; das Schussereignis benutzt weiterhin den ursprünglichen gelösten Fire-Einstiegsframe.

Neue reproduzierbare Originaldatenprüfungen (Dateien selbst aus den geprüften Discs gewinnen):

```sh
python3 tools/test_enemy_data.py .host-cmake/enemy_tool /pfad/em12.drs
python3 tools/test_hit_geometry.py .host-cmake/hit_geometry_tool
.host-cmake/enemy_spawn_tests /pfad/emleon00.esl
.host-cmake/enemy_encounter_tests /pfad/em12.drs /pfad/r100.dat /pfad/emleon00.esl /pfad/pl00.drs /pfad/wep01.drs
.host-cmake/enemy_gl_check /pfad/pl00.drs /pfad/wep01.drs /pfad/em12.drs /pfad/bild.ppm
.host-cmake/android_bridge_gl_check /pfad/pl00.drs /pfad/wep01.drs /pfad/r300.dat /pfad/r301.dat /pfad/roominfo.dat /pfad/bild.ppm /pfad/em12.drs /pfad/emleon00.esl /pfad/r100.dat
```

GPU-Ziele benötigen `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3-Entwicklungspfade; die JNI-Brückenprüfung zusätzlich `RE4_JNI_INCLUDE_DIR` zum JDK-Includeordner. Keine Teststubs, Fault-Injection oder Referenzprogramme werden in die Android-App gelinkt. Die historischen sechs Fixtureargumente der Brückenprüfung bleiben gültig.

Die Gesamtspiel-/KI-/Szenario-/Griff-/Inventar-/Audio-/Speichergrenzen stehen in ENEMY-ENCOUNTER.md und UPDATE-M19.md. Originalgameplay wird mit diesem begrenzten Gegner-/Treffertest nicht als fertig erklärt.

## M20 — Frontgriff, Spielerleben und Stickrichtung (2026-10-04)

Der Nutzer bestätigt den M19-Test. Sein Log `Eingefügter Text(20261004-073044).txt` enthält sieben Treffer: fünf Körper- und zwei Kopftreffer ergeben 999 Schaden, Gegner-HP1 und Magazin3/10 mit Reserve60. Im vorgelegten Ausschnitt ist kein Fatal-/ANR-Marker. Das ist eine Bestätigung von M19, noch kein M20-Gerätetest.

- Das vollständige Hostskript besteht: **17 native Tests**, Java-Disc-Verträge (8), Controller-Policy (11), RoomRepository (64). Alle geänderten nativen Laufzeitdateien werden mit C++17 und strikten Warnungen kompiliert. Produktions-JNI, Szene- und Akteurrenderer sind mit offiziellen JNI-/GLES-Headern zusätzlich kompiliert und als Host-Brückenprogramm gelinkt. Der datenabhängige Brückenlauf wurde diesmal nicht ausgeführt.
- HP/RNG-Regeln: **4.587.520 Fälle** über alle 65.536 RNG-Startwerte, sieben HP-Werte, fünf Schäden und beide keep-one-Werte stimmen mit dem Prüfsummenorakel der exakt extrahierten Originalfunktion überein. **4.369 Ausrichtungsfälle** stimmen mit dem originalen `EmCatchPLSet` ohne Differenz überein. Fangboxgrenzen und die originalen Gegenwehrgruppen sind ebenfalls geprüft. Diese Prüfungen benötigen keine Disc-Assets.
- Neue, ausdrücklich **synthetische** DRS-Fixtures werden durch den unveränderten Upstream-Parser geprüft: drei DRS-, 36 FCV- und 22 SEQ-Roundtrips sind byteidentisch. Die Kurven enthalten tatsächlich wechselnde endliche Gelenkposen, Free2-Fangfenster, offene/geschlossene Handmodelle und alle benötigten Clip-Schnittstellen. Sie enthalten keine kopierten Spielassets.
- Gekoppelte Laufzeit: **9.307 Prüfungen** mit diesen synthetischen Archiven bestehen. Dazu gehören fehlgeschlagener Griff mit HP1200→990→810 und anschließendem Aufstehen, erfolgreiche Gegenwehr mit Gegner-HP1000→900 und Down/Wake, niedriger Lebensstand/Tod, kritische Gegenwehr mit zusätzlichem RNG-Verbrauch, Pause, einmalige Eingabeflanken, Kontakt und Munitionsübernahme. **600 Simulationstakte pro Spur** sind für erfolgreichen und fehlgeschlagenen Griff bei 30/60 Renderbildern pro Sekunde exakt gleich. Die Datenprüfung belegt die implementierten Zustandsverträge; sie ist kein Goldenvergleich der vollständigen Original-cPlayer-/Gegnerruntime.
- **ASan/UBSan** bestehen für die exhaustive HP/RNG-Prüfung und den vollständigen gekoppelten synthetischen Lauf, einschließlich der korrigierten FCV-Fixtures. LeakSanitizer ist deaktiviert; keine Leak- oder Android-Sanitizer-Prüfung behauptet.
- Produktions-GLES auf Software-EGL: **4.619 Prüfungen, 25 vollständige GPU-Pose-Aufnahmen**, maximale Weltpositions-/Normalenabweichung **0,0000609913** Spieleinheiten gegenüber unabhängiger Double-Auswertung. Geprüft werden beide Handvarianten, exklusive Drawauswahl, ausgeblendete/zurückkehrende Waffe, Hold/Fail/Recover/Escape, gespiegeltes Wake sowie erhaltene CPU-Zustände und Framebuffer bei EGL-Neuaufbau. Die synthetische Gegnergeometrie besitzt 18 Modellecken; dies ersetzt keinen Vergleich der zusätzlichen Originalhandmodelle oder neuen Originalposen.
- Rechter Stick: physisches Rechts wird genau einmal im nativen Mapper horizontal invertiert, nach radialer Totzone. Asset-freie Eingabetests prüfen beide Richtungen, Totzone, Null/NaN und Flanken. Freie Kamera und Zielrichtung verwenden denselben gemappten Wert. Die zusätzlich erweiterten Originaldaten-/Brückenprüfungen für die tatsächliche Kamerasicht und Handrichtung bleiben in diesem Durchlauf unausgeführt.

**Grenzen dieses Durchlaufs:** Die gespeicherten Original-Discs waren nicht abrufbar. Android-SDK, NDK und die frühere Gradle-Buildumgebung standen nicht zur Verfügung. Es gibt daher **keinen neuen Android-APK-/Lint-/Gerätelauf** und **keinen erneuten Vergleich der neuen Clips oder offenen Hände mit Originalassets**. Frühere M16–M19-Ergebnisse oben sind historische Prüfungen und werden nicht als neuer M20-Lauf ausgegeben. Die separate Original-EM12-Referenz ist mit `tools/test_enemy_grab_data.py` vorbereitet; der Referenzcode kompiliert und Python-Syntaxprüfung besteht.

Version **0.20.0-dev / Code20**. Der Frontgriff ist ein begrenzter, quellgestützter Kampfzweig; vollständige KI, Szenario, Audio, Inventar, Speichern und Originalkamera bleiben offen. Installation und Gerätetest stehen in `UPDATE-M20.md`, die Quellen und Adaptergrenzen in `GRAB-COMBAT.md`.

### M20-BuildFix — Verpackung (2026-10-04)

Das neue Nutzerlog `Eingefügter Text(20261004-082054).txt` enthält einen Buildabbruch nach 100 CMake-Neuerzeugungen, vor der C++-Kompilation. Die ursprüngliche ZIP-DOS-Zeit 11:18:20 lag gegenüber der tatsächlichen Auslieferung 10:18:20 Europe/Vienna eine Stunde voraus. Im Projekt gibt es keine konfigurationszeitliche Dateiänderung, die diese Schleife erklären würde. Der fehlerhafte Archivzeitstempel ist somit die belegte Verpackungsursache; die tatsächlichen entpackten Windows-Dateizeiten sind nicht im Log enthalten.

Der Ersatz enthält dieselben 34 vollständigen M20-Dateien mit vergangenen ZIP-Zeitstempeln; nur diese zwei Dokumente wurden inhaltlich ergänzt. Alle übrigen Inhalte, insbesondere sämtliche Spielquellen und Tests, werden byteidentisch zum ersten M20-ZIP geprüft. ZIP-CRC, vollständige Dateiliste und Overlay-Inhalte sind geprüft. Ein vollständiger Neuaufbau nach dem Entpacken ist vorgeschrieben. Keine geänderten Gameplay-Tests oder erfolgreiche Windows-/Android-Kompilation durch diese Verpackungsprüfung behauptet.
