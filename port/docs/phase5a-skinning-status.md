# Phase 5a: originales Weight-/Skinning-System

Geprüfte Fortsetzung vom 11. Oktober 2026 auf `android-port`, ausgehend von
`bbeef30a74a7ebe85acd18c6f589a38011766e4f`. Windows x86/x64 sowie Retroid
Pocket Flip 2 `dbdcacc`, Android 13/API 33, ARM32/ARM64, Adreno 650.

## Ergebnis und Abnahmegrenze

Die originale Weight-Kette berechnet native S16-Positionen sowie S8-/S16-Normalen.
Die vollständigen Originalbodies von `calcWeightMat`, `MakeWeightPalette` und
`MakeWeightPaletteExt` laufen mit nativen Besitzern. Die PPC-Quantisierung und
Matrix-Lane-Anordnung sind ausdrücklich gekennzeichnete skalare CPU-Adapter.
Originale FK-Motion, Rootbewegung und Doppelgelenke laufen zusätzlich über
`MotionMove`. NEON und GPU-Skinning wurden nicht eingeführt.

Ein echter Vulkan-Nachweis zeigt **Leons originale Anbauhierarchie `pl000d`**
in der originalen r120s00-Cut-1-Pose, Frame 30. Die gemeinsame BIN-/TPL-,
Display-List-, Material-, Licht- und OT-Pipeline verarbeitet die Skinning-Ausgabe.
Es gibt keinen Leon-Spezialshader und keine selbst festgelegte Kamera.
Ein Kontrollframe desselben Raums mit identischer Kamera, Matrix und Frame,
aber ohne das zusätzliche Modell, weist **524 veränderte Pixel** nach.
Alle vier Readbacks aus zwei Activity-Initialisierungen je ABI sind byteidentisch.

**Der vollständige Leon-Körper wird noch nicht ausgegeben.** Er bricht an
Materialflag 3 ab, weil die originalen Specular-/Bump-Pfade fehlen. Aktive
Gesichtsmorphs, IK, Cloth, sekundäre Motion/Hokan und vollständige Event-
Wiedergabe bleiben offen. Das sichtbare Teilmodell ist klein und dunkel;
eine passende GameCube-Aufnahme desselben Modells, Cuts und Frames fehlt.
Die Prüfung belegt eine originale geskinte Pose und deren GPU-Fragmente,
keine vollständige visuelle Originaltreue oder spielbare Leon-Steuerung.
Phase 5a bleibt daher ein geprüfter Teilstand. Phase 5b wurde nicht begonnen.

## Originalcode, native Anpassungen und tatsächliche Ausführung

| Originalquelle | Umfang |
|---|---|
| `src/game/trans.cpp:787` | Vollständiger Body `calcWeightMat`: inverse erste Gelenkmatrix × Gelenkweltmatrix × originale Bindmatrix. Native Parts-Liste statt PPC-Overlay. |
| `src/game/trans.cpp:808` / `:855` | Vollständige Bodies `MakeWeightPaletteExt` / `MakeWeightPalette`; originale Prozentwerte, letzte Gewichtung als Rest, Matrixakkumulation und Einflusszählung. `uintptr_t` statt Pointer-Trunkierung, eigener Speicher statt `0xE0000000` Locked Cache. |
| `src/game/trans.cpp:2485` / `:2521` | `CalcSk1_x` / `CalcSk1_x2`: skalare Umsetzung der drei Single-FMAs, GQR-Skalierung, Saturierung und Trunkierung. Kein unzulässiger letzter Assembly-Prefetch über das Arrayende. |
| `src/lib/psmtx.c` | `PSMTXReorder`: explizite Umsetzung der `ps_merge`-Lanes als vier Spalten zu je drei Floats. |
| `src/game/motion.cpp:622` | Vollständiger `MotionMoveCore`-Body, BE-Leseadapter und native Referenzen; originale Hermite-Kanäle und Parts-Zustände. |
| `src/game/motion.cpp:943`, `:1030`, `:1088` | Vollständige Bodies `MotionGetSpeed`, `MotionAddSpeed`, `MotionSequenceCtrl`. Die drei globalen Speed-Lesezugriffe beziehen den begrenzten nativen Frame-Step. |
| `src/game/motion.cpp` | Vollständiger `MotionMove`-Body mit **vorab eingeschränktem FK-Scope**: aktive IK, Hokan, Motion B und AttachCamera werden abgelehnt. Der IK-Aufruf prüft, dass die originale Schleife in diesem Scope keine aktive Arbeit hätte. Keine erfolgreiche IK-Ersatzfunktion. |
| `include/model.h`, `src/game/model.cpp` | Originales `cCoord::matCalc` und vollständiges `cModel::matBlend`, zusätzlich zu den erhaltenen Parts-/Bind-/World-Matrixbodies. Nur C++-Namen und codelose Compileranker angepasst. |
| `src/lib/quat.c`, `mtx.c`, `mtxvec.c`, `vec.c` | Tatsächliche SDK-C-Bodies für Quaternionkonvertierung/-interpolation, Quaternionmatrix und erforderliche Vektor-/Matrixhelfer. |
| `src/game/shape.cpp` | Originales `ClrShape` und `ResetShape` für die neutrale Vertexbasis. **Nicht** die aktive Shape-Wiedergabe. |
| `src/game/event.cpp:960`, `:1503` | Bereits portierte originale SetPartsSub-/SetMod-/GetMod-Bodies und sechs externe pl00-Registrierungen werden tatsächlich verwendet. |
| `src/game/trans_ot.cpp`, `camera.cpp` | Zusätzlich originale `AddOtModelPosRadius`, `AddOtWorldPosRadius`, `CameraGetLookVecInverse` und SDK-DotProduct für originale Tiefenprioritäten der OT-Typen 0/1. |

`generate_consumers.py`, `generate_skinning.py` und `generate_gx.py` prüfen
Originalquellhashes und erzeugen vollständige Dateien mit `#line`-Provenienz.
Die Buildverzeichnisse enthalten `consumer-manifest.json`,
`skinning-manifest.json` und `gx-render-manifest.json`. Vollständige Bodies,
Quellsegmente und Assembly-/Plattformadapter sind getrennt ausgewiesen.
`src/`, `include/` und `tools/motion/host` wurden nicht verändert.

Die vorhandene Linux-Motion-Portierung unter `tools/motion/host` wurde als
Grundlage für native Feldzugriffe und Original-SDK-Auswahl untersucht.
Ihr `MAP_32BIT`-/Raw-Pointer-Ansatz wird nicht übernommen; ihre Stand-ins
für noch fehlende Engine-Zustände sind kein Laufzeitnachweis dieses Ports.

`event_skinning.cpp` ist eine **isolierte Originalfunktionsdiagnose**:
EngineLoader/Dvd/Read/Yz2 laden echte EVD-/pl00-Daten; originale Modell- und
Eventregistrierung erstellt Besitzer; Original-Motion-Bodies berechnen die
Pose. Ein begrenzter BE-Packetadapter liest Namen, Pos- und Mot-Felder.
Der komplette `Event::Run`, `ExePacket_SetOm`, `SetObj18` und Event-Zeitleben
werden damit nicht ersetzt. Insbesondere werden parentgebundene Pos-Pakete
und aktive Shape-Pakete ausdrücklich als Abhängigkeiten gemeldet. Der Pfad
setzt ruhende Modellbesitzer voraus und räumt seine Domäne anschließend auf.

## Formate, Matrizen, ABI und Normalen

- `cWeight`: 8 Bytes, drei u8-Gelenkindizes, Einflusszahl und Prozentwerte.
  `cWeightExt`: 12 Bytes, drei BE-u16-Indizes und BE-u16-Einflusszahl.
  **`weight_ext_num > 255` wählt das erweiterte Format**, nicht die Knochenzahl.
  Ein bis drei Einflüsse; höchstens 248 originale Bone-Matrixslots.
- S16-Positionen: Eingang vier S16-Wörter inklusive Paletteindex, Ausgang
  drei S16-Wörter. S8-Normalen: drei signed Bytes plus unsigned Paletteindex;
  S16-Normalen: drei signed Wörter plus signed Paletteindex.
- Positions-GQR folgt dem originalen BIN-Shift. Normals verwenden die
  tatsächlichen Konstanten **`0x32073207`** bzw. **`0x20062006`**: signed
  6-Bit-Skalierung −14 bzw. −32. Das sind keine normalen Positions-Shifts.
  Die Berechnung behält diese ursprüngliche Quantisierung bei, einschließlich
  des dadurch unterdrückten Translationseinflusses und der Trunkierung.
- Die CPU-Kette normalisiert die Gewichtssumme oder Normallänge **nicht
  zusätzlich**. Ein solcher Schritt würde die Originalberechnung verändern.
  GX-Decoder lesen S16-Normalen mit 1/16384 bzw. S8 mit 1/64; anschließend
  folgt wie im Original die inverse transponierte Modell-/View-Transformation.
- Bindmatrizen kommen aus originalem `setPartsOffset`/SetPartsSub. Es werden
  keine glTF-Inverse-Bind-Matrizen oder neue Rest-Posen erfunden.
- `commonModelTrans:1104` unterscheidet die Ausgabe: gewichtete Positionen
  verwenden `m->pList->mat × info->mat`; nur der statische Ein-Part-Zweig
  verwendet `getPartsPtr(d->pHead->partsNo)`. Die native Ausgabe und der
  unabhängige Test prüfen genau diese Unterscheidung.
- Der ursprüngliche statische Bypass gilt nur ohne Shape-Flag. Aktive
  Morphs sowie das noch nicht integrierte unskinned-Override `0x4000` führen
  zu einem konkreten Fehler, nicht zu einer ersatzweise erfolgreichen Pose.
- Originalbytes bleiben unverändert. BE-Felder werden begrenzt gelesen;
  Palette, Quaternions, Parts und Zeiger liegen in nativen Strukturen.
  `ResourceLease` hält BIN/TPL/FCV-Besitzer bis zum letzten CPU-/GPU-Verbrauch.
  Output-Vektoren ersetzen Locked Cache, GQR-Register und Prim-Puffer;
  der Renderer lädt sie in seine vorhandenen Vertex-/Indexbuffer.
  Die Reorder-Ziele sind tatsächliche native `float[4][3]`-Objekte mit
  48 Bytes, keine als mehrdimensionale Arrays umgedeuteten flachen Objekte.

Die Quantisierungsreferenz wurde zusätzlich anhand der primären
[Dolphin-PSQ-Implementierung](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/Interpreter/Interpreter_LoadStorePaired.cpp)
und der
[Paired-Single-FMA-Implementierung](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/Core/PowerPC/Interpreter/Interpreter_Paired.cpp)
abgeglichen. Das ist kein gemessener Gekko-Trace. Denormals, FPSCR-Sondermodi
und bitgenaue PPC-Matrixakkumulation sind dadurch nicht generell verifiziert.

## Originaldatenabdeckung

Verbindliche unveränderte Quelle ist `RE4-Data/import-phase3a` mit Manifest/CAS.
`test_skinning.py` verwendet Disc-IDs aus dem Manifest; r317s15 gehört zu
Disc 2 (Index 1). Beide pl00-Archive bleiben getrennt adressierbar.

| Tatsächlicher Testfall je Runde | Berechnete Modellinstanzen / Motion-Zuordnungen |
|---|---|
| r120s00, Cut 0, Frames 0 / 100 / 400 | Je 22 / 15; Leon/zweite Instanz, Eventanbauteile, Polizei und Auto. Gesichtsausgabe neutral, aktive Morphs gemeldet. |
| r120s00, Cut 1, Frame 30 | 29 / 15; originale FK-Pose, zusätzliche registrierte neutrale Modelle. |
| r120s01, Cut 0, Frame 30 | 23 / 15. |
| r317s15, Cut 0, Frame 0, Disc 2 | `em3900g`, 97 Bones, originale `em3900g_s03_000.fcv`. |
| r10c und r22a, globale BIN-Archiveinträge 27 | Je 66 Bones, 275 WeightExt-Paletteneinträge, 547 Einflüsse; tatsächlich identische BIN-Hashes. Bind-Pose-/Formatprüfung, keine Behauptung eines vollständigen Raumrenderings. |
| Vollständige externe `em/pl00/pl000a.bin` + TPL | 119 Bones, 107 Paletteneinträge, 198 Einflüsse; originale native Bind-Pose. |

Die 275er Tabellen liegen **in globalen DAS-BIN-Einträgen**, nicht im SMD-
BIN-Pool. Private rohe BIN-/TPL-Fixtures werden aus geprüften Archiven gewonnen
und als solche gekennzeichnet. Die übrigen Eventtests verwenden den echten
EngineLoader. Die bisherigen GameRuntime-Regressionen prüfen weiterhin alle
37 r120-BIN-Vorkommen und zusätzliche r100-/r317-Ressourcen. Daraus folgt
kein Nachweis, dass sämtliche 37 Hierarchien bereits vollständig animiert
oder gerendert werden.

## Numerische Ergebnisse

`test_skinning.py` prüft tatsächliche Original-FCV-Locals, die Draw-Rootmatrix,
Bone-/Bind-Weight-Komposition und sämtliche quantisierten Positionen/Normalen.
Die Python-Matrixreferenz rechnet mit höherer Präzision; die PSQ-Referenz
benutzt explizite Single-FMAs und die ausgelesene Single-Palette. Die Methoden
und Toleranzen stehen im Skript und werden nicht als Hardware-Bitvergleich
ausgegeben.

Alle vier Architekturen: **18 Läufe / 244 Modellberechnungen / 1.739.820
Einzelvergleiche**, jeweils zwei Runden. Die 486 GX-Isolationschecks enthalten
112 neue Skinning-Arithmetik-/Formatchecks; diese sind keine echten Spieldaten.

| Vergleich | Größte gemessene Abweichung |
|---|---|
| PSQ-Referenz vs. alle ausgegebenen S16-/S8-Integerwerte | **0** |
| Weight-Matrix linearer Anteil vs. Double-Referenz | 1,19 × 10⁻⁷ |
| Weight-Matrixtranslation vs. Double-Referenz | 0,011290427 Original-Längeneinheiten |
| FCV-Positionskurven vs. höherpräziser Hermite-Referenz | 0,004801790 Original-Längeneinheiten |
| FCV-Winkel | 1,579 × 10⁻⁶ rad |
| FCV-Scale | 2,4 × 10⁻⁸ |
| ARM32 vs. ARM64: quantisierte Werte und Modell-/Motion-Identitäten | byteidentisch |
| ARM32 vs. ARM64: erfasste Bone-/Bind-/Palette-/Draw-/Local-Matrizen und Werte | max. absolute Differenz **0** |

Diese Werte messen die native Referenz und die unabhängige Hostrechnung.
**Eine numerische Abweichung zur echten PPC-Ausführung ist noch nicht
gemessen.** Matrix-SDK-C-Bodies und Host-libm bleiben von PPC-Paired-Single-
Matrixroutinen und der originalen libm zu unterscheiden.

## GPU-Nachweis und visuelle Grenzen

Private Ausgabe: `E:\Development\Android\RE4-Data\phase5a-skinning-gpu`.
`gpu-armeabi-v7a-1.png` / `gpu-arm64-v8a-1.png` sind echte unveränderte
512×448-GPU-Readbacks; pro ABI existiert auch eine zweite Aufnahme,
der vollständige JSON-Bericht, `.rgba` und ein Android-Displayscreenshot.
Keine dieser Dateien liegt in Git oder der APK.

Die Szene enthält 242 Modellhierarchien, 329 Part-Draws, 351.777 Dreiecke
und 38 Texturen. Davon ist **eine** zusätzliche Hierarchie gewichtet:
`em/pl00/pl000d.bin`, über Leons echte Motion
`event/r120/s00/pl0000/pl0000_s00_001.fcv`. Die Kamera ist
`event/r120/s00/cam/cam_s00_001.fcv`, Cut 1, Frame 30.

EFB-Hash aller vier finalen Readbacks:
`f7ec27e594e7c633011b5823379b4698b223f015a08a272e1cf80ade16bb90d3`.
Gegen die identische modellfreie Kameraaufnahme unterscheiden sich 524
Pixel im Bereich **x 207..232, y 185..211**. Der Pixeltest verlangt identische
Raum-ID, Kamera, Frame, Kameraparameter und View-/Projektionsmatrix; andere
Kameras können nicht versehentlich als Vergleich akzeptiert werden.
Die vier finalen Durchläufe erreichten 18/58 ARM32- und 12/12 ARM64-Frames.

Vulkan-Validation 1.3.290.0 wurde tatsächlich geladen: **0 Fehler**, je
initialisierter Surface **1 Warnung** `WARNING-Swapchain-PreTransform`
(`IDENTITY` statt aktuellem `ROTATE_90`, zusätzliche Präsentationstransformation).
Es wurden keine Lebenszeit-, Descriptor- oder Synchronisationsfehler gemeldet.
Die Warnung bestand bereits vor dem Skinning-Schritt; sie bleibt offen.
Die letzten erfassten CPU-Framezeiten liegen bei 50,1–51,7 ms einschließlich
Fence-Wait, GPU-Readback und SHA. Das sind keine reinen Skinning-Zeiten und
kein optimierter Gameplay-FPS-Nachweis.

Die Ausgabe wurde visuell angesehen. Das Teilmodell ist bei dieser echten
Originalkamera zu klein/dunkel für einen belastbaren Gelenk- oder Material-
Qualitätsvergleich. Die bekannte Dolphin-Aufnahme vom 10. Oktober zeigt
r100 nach dem Intro; sie wird **nicht** als direkter Vergleich zu dieser
r120-Pose verwendet. Normale, Bind-Matrix- und FCV-Werte sind numerisch
geprüft; fehlende Specular-/Bump-, Morph-/Cloth- und dynamische Lichtpfade
begrenzen die sichtbare Originaltreue weiterhin.

## Builds und Regressionen

| Tatsächlich ausgeführte Prüfung | Ergebnis |
|---|---|
| MSVC x64 und x86, CMake/Ninja | Gebaut; je **27/27 CTests** bestanden. Original-Quelländerungen werden zurückgewiesen. |
| NDK 28.2.13676358, ARM32/ARM64, Gradle offline + Lint | CLI, JNI, Debug-APK und Test-APK gebaut; Build erfolgreich. |
| Originaldaten-Skinning auf x86/x64/ARM32/ARM64 | Obiger voller Umfang je ABI bestanden. |
| Originalpose-GX-Referenz auf allen vier ABIs | Zwei Runden je ABI; Primitive, TPL-Mips, Materialien, Kamera, originale OT-/TEV-/Lichtzustände bestanden. |
| Retroid Originalpose-GPU, zwei Initialisierungen je ABI | Sichtbare Modellfragmente, identische Readbacks, Surface-Retirement und Validation bestanden. |
| Bestehende statische r120- und r119s10-Ausgabe | Host x86/x64 und Retroid ARM32/ARM64, jeweils zwei Runden; Originaldatenreferenz und wiederholte GPU-Ausgabe bestanden. |
| GameRuntime, beide Discs, zwei Runden auf allen vier ABIs | Originale Player-, SubScreen-/Inventar-, Save-/ID-, Licht-, Modell-/Motion-, Event-/Callback- und Loaderprüfungen bestanden; 37 r120-BIN-Vorkommen bleiben geprüft. |
| Ressourcen, beide Discs, zwei Runden auf allen vier ABIs | Je Disc 157/151 Ressourcen und 85/57 Räume; MRAM/ARAM, Freigabe, Abbruch und Neuinitialisierung bestanden. |
| Module, beide Discs, zwei Runden auf allen vier ABIs | 110/4 originale REL-Dateien und bisherige partielle native Registrierung/Dispatches bestanden. Keine Behauptung vollständiger REL-Ausführung. |
| Retroid Core, zwei Runden je ABI | ARM32 **13.386**, ARM64 **13.387** Checks, 0 Fehler; Scheduler-Traces identisch. Ein zusätzlicher ARM64-Pointerbreitencheck erklärt den Zählunterschied. |
| Retroid APK-Instrumentation | **20/20 je ABI**; zentrale Controller-/PadRead-Anbindung, echte Vulkan-Clears und Surface-Prüfungen erhalten. Keine erneuten physischen Tastendrücke behauptet. |

Die ersten Host-CTestläufe scheiterten teilweise an Sandbox-Dateirechten,
nicht am Originalcode. Der vollständige Wiederholungslauf außerhalb der
Sandbox bestand. Der erste Android-Modullauf konnte den lokalen ADB-Daemon
nicht erreichen; der separat wiederholte Lauf bestand. Ursprüngliche Logs
bleiben neben erfolgreichen Wiederholungen unter `port/build/skin-*.log`.
Bei der Entwicklung wurden Disc-Zuordnung und DAS/SMD-Fixtureauswahl sowie
Cull-/Draw-Root-Zuordnung korrigiert; die obigen Werte stammen aus den finalen
Prüfungen. Keine vorbereitenden Fehlversuche werden als erfolgreiche Tests gezählt.
Der Android-Build meldet außerdem die bekannte `CXX5304`-Warnung zum
SDK-XML-Versionsunterschied (Version 4 vorhanden, Parser bis Version 3).
Build, Packaging und Lint liefen trotzdem erfolgreich durch.

Bewusste Negativprüfung: der komplette `pl0000`-Renderaufruf meldet
`Original consumer unavailable: GX unsupported material flags 3`. Kein
Ersatzmaterial oder als fertig ausgegebener Gameplay-Start wurde eingeführt.

## Geänderte vollständige Projektdateien

Alle Dateien wurden vollständig im Projekt gespeichert. Generierte Original-
Bodies liegen nur im ignorierten Buildbaum; Originaldaten und GPU-Bilder privat.

- Build/Provenienz: `port/CMakeLists.txt`, `port/config/consumer-sources.json`,
  `port/tools/generate_consumers.py`, `generate_skinning.py`, `generate_gx.py`.
- Native Typen/API: `port/include/re4/port/model_types.h`, `motion.h`,
  `skinning.h`, `event_skinning.h`, `gx_scene.h`.
- Native Ausführung: `port/src/model_runtime.cpp`, `motion.cpp`,
  `motion_original.h`, `skinning.cpp`, `skinning_original.h`,
  `event_skinning.cpp`, `gx_scene.cpp`, `gx_pipeline.cpp`, `gx_render_original.h`.
- Android: `port/platform/android/graphics_jni.cpp`,
  `port/android/app/src/main/java/org/re4/port/GeometryActivity.java`,
  `GraphicsBootstrapView.java`.
- Tests/CLI: `port/tests/gx_tool.cpp`, `gx_verification.h`,
  `skinning_verification.h`, `test_consumer_sources.py`,
  `port/tools/test_skinning.py`, `test_gx_scene.py`.
- Dokumentation: dieser Bericht, `port/docs/phase5a-pipeline-status.md`,
  `port/README.md`.

## Reproduktion

Vorhandene Host-CMake-Konfigurationen `port/build/host-ninja` (x64) und
`port/build/host-x86` (x86) bauen; dann jeweils `ctest --test-dir <build>
--output-on-failure`. Die Privatskripte benötigen Python mit NumPy/Pillow,
das Phase-3a-Manifest/CAS und für Android das autorisierte USB-Gerät.

```powershell
.\port\scripts\build-android.ps1 -SdkRoot '<Android SDK>' -JavaHome '<JBR>' -Python '<Python>' -ValidationLayers '<android-binaries-1.3.290.0>' -Offline
python port/tools/test_skinning.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-skinning-host64' --host-exe port/build/host-ninja/re4_gx_tool.exe --repeat 2
python port/tools/test_skinning.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-skinning-host32' --host-exe port/build/host-x86/re4_gx_tool.exe --repeat 2
python port/tools/test_skinning.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-skinning-device' --adb '<SDK>/platform-tools/adb.exe' --serial dbdcacc --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-skinning-camera-baseline' --cut 1 --frame 30 --adb '<adb>' --serial dbdcacc --repeat 1
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-skinning-gpu' --model pl000d --cut 1 --frame 30 --baseline-output 'E:\Development\Android\RE4-Data\phase5a-skinning-camera-baseline' --adb '<adb>' --serial dbdcacc --repeat 2
```

Für Host-GX-Prüfungen denselben Modell-/Cut-/Frame-Aufruf mit `--host-exe`
statt `--adb` und ohne Pixel-Baseline verwenden. Original-GameRuntime-,
Ressourcen- und Modulläufe behalten ihre vorhandenen `--data`, `--output`,
`--host-exe` bzw. `--adb --serial`, `--repeat 2` Argumente. Die beiden
Device-PowerShellskripte bleiben unverändert. GPU-/APK-Läufe nacheinander
ausführen, weil sie dieselbe App installieren bzw. neu initialisieren.

## Konkrete verbleibende Abhängigkeiten

1. Vollständiger Leon-/Polizei-/Auto-Draw: `shaderSetup`, `specularSetup` und
   `bumpSetup` einschließlich Normal-/Position-TexGen, Original-Specular-/Bump-
   Texturen und erforderlicher TEV-/indirekter Stages. Die CPU-Pose ist verfügbar;
   fehlende Materialfunktionen müssen vor einem vollständigen GPU-Actor gelöst sein.
2. Gesichter: originale `ShapeSet/ShapeMove/CalculateShape_new`-Ausführung.
   r120-Pakete für ev0001/ev0011 bzw. ev0701/ev0701a werden gemeldet;
   Neutral-Skinning ist keine aktive Gesichtsmorph-Wiedergabe.
3. Eltern-/Eventobjekte: vollständiges `ExePacket_Pos/OyaSetObj18`, lebende
   Actor-/Objektbesitzer und Cloth. Die sieben parentgebundenen r120-Objekte
   werden in der FK-Diagnose nicht als korrekt initialisierte Posen ausgegeben.
4. Vollständige Spieler-Motion: `InverseKinematics/ikCalc` benötigt echte
   Actor-Kollisions-/`SatMgr::getFloor`-Zustände. Hokan, zweite Motion,
   AttachedCamera und die vollständige reale SEQ-Laufzeit bleiben gesondert
   zu integrieren. Der getestete Event-FK-Pfad nutzt benannte Original-FCVs
   ohne SEQ und prüft den entsprechenden Original-SequenceCtrl-Zweig.
5. Modellgebundene Lichter und dynamische SAR-/Sichtbarkeitszustände,
   vollständiges `Trans/Render`, Schatten und passende Original-PPC-/Dolphin-
   Referenzen desselben Modells/Frames/Kameras bleiben erforderlich.
6. `GameTask` bleibt am echten `MerchantGameInit()`-Blocker; `R120Event`
   erreicht weiterhin `Sofdec.Initialize("movie/opening.sfd", 0)`. Die
   Renderdiagnose verändert oder überspringt diese Originalsequenz nicht.

MSAA Aus/2×/4×, AF 2×/4×/8× und DoF mit Nah-/Zielfokus einschließlich
persistenter, hardwarevalidierter Einstellungen bleiben verbindlich.
Die bestehende Renderziel-/Samplerarchitektur und zentrale PadRead-Anbindung
wurden erhalten. Kein Merge/Rebase/Push wurde durchgeführt. `master` bleibt
`9b76ff003081d6ece8b838453fb8310743a972c7`; die README von `origin/master`
`15cf64f80e08bbc64712c39be330b75d1512a0d0` wurde berücksichtigt.
