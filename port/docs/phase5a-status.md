# Phase 5a: sichtbare originale r120-Geometrie über Vulkan

Historischer Bericht zum Stand `b0071bda`. Der folgende Entwicklungsschritt
mit originaler OT-, TEV- und Lichtausführung ist in
[phase5a-pipeline-status.md](phase5a-pipeline-status.md) dokumentiert.

Geprüfter Teilstand vom 10. Oktober 2026 auf `android-port`, ausgehend von
`81fa552a517418f7bbeac8117e8db36f6f23dcd3`.

## Ergebnis und Abnahmegrenze

**Originale statische r120-Geometrie, originale TPL-Texturen und die erste
originale r120-Eventkamera werden tatsächlich auf dem Retroid Pocket Flip2
über Vulkan dargestellt.** Beide Prozess-ABIs wurden installiert und geprüft.
Vier GPU-Readbacks sind bytegleich. Die Bilder enthalten die Wald- und
Bergkulisse des Raums; es handelt sich weder um einen Clear-Test noch um
nachgebaute Geometrie.

Dies ist ausdrücklich ein **isolierter, unbeleuchteter Rendering-Nachweis**.
Die vollständige Phase-5a-Abnahme ist noch offen: Der originale allgemeine
GX-Renderpfad, seine OT-Reihenfolge, Beleuchtung und TEV-Auswertung laufen
noch nicht. Eventactors und Leon werden noch nicht auf der GPU dargestellt.
Eine originalgetreue vollständige Szene oder ein spielbarer Raum wird nicht
behauptet. Der reguläre GameTask wurde nicht künstlich fortgesetzt.

## Originalquellen und tatsächliche Ausführung

Die Originalquellen in `src/` und `include/` bleiben unverändert. Das neue
Quellenmanifest prüft normalisierte SHA-256-Fingerprints; der Generator
extrahiert vollständige Funktionskörper mit Quellenprovenienz. Die folgende
Tabelle unterscheidet Integration von tatsächlicher Nutzung im r120-Nachweis.

| Originalfunktion | Quelle | Status |
|---|---|---|
| `CameraMotion::move` | `src/game/cam_motion.cpp:60` | Originalkörper ausgeführt; ausschließlich Klassenname auf `DiagnosticCameraMotion` angepasst. Native, besitzende FCV-Referenzen ersetzen GC-Zeigerlayouts. |
| `CameraSequenceCtrl` | `src/game/cam_motion.cpp:117` | Vom Original-Move aufgerufen; pausierter originaler Frame 0, keine erfundene Kamerabewegung. |
| `CameraSetOrientationRoll` | `src/game/cam_sys.cpp:26` | Mit dem originalen Roll-Wert ausgeführt. |
| `CameraSetOrientationUp` | `src/game/cam_sys.cpp:12` | Vollständiger Körper integriert; der relative Basiskamera-Zweig ist im gewählten ersten Cut nicht erforderlich. |
| `getColumn`, `MTXSetColumns` | `include/math_sub.h` | Vollständige Originalhelfer integriert; `MTXSetColumns` im Orientierungspfad ausgeführt, `getColumn` ist dessen Sonderfall für vertikale Blickrichtungen. |
| `C_MTXRotAxisRad`, `C_MTXLookAt` | `src/lib/mtx.c` | Original-SDK-Körper ausgeführt. |
| `C_MTXPerspective` | `src/lib/mtx44.c` | Original-SDK-Körper ausgeführt. |

Wiederverwendet werden die bereits geprüften Originalkörper für
`cModInfoMgr::create`, `cModel::modelInit`, `partsMatCalc`, `partsWorldCalc`,
`RotMatrix`, `TransMatrix`, `ScaleMatrix`, Matrix-/Vektoroperationen und
Motion-/Hermite-Abtastung. Für jeden der 241 verwendeten SMD-Works wird ein
echtes natives Modellobjekt erstellt, mit seiner Originaltransformation
berechnet und am kontrollierten Ressourcenübergang freigegeben. Das beweist
keine bereits laufende Scroll-/Actorverwaltung für 241 gleichzeitige Objekte.

`EngineLoader::loadRoom` und die bestehenden DVD-/ReadAreaData-/Yz2-Pfade
laden das originale DAS; die EVD-Datei wird ebenfalls über den bestehenden
Loader angefordert. Die CPU-/GPU-Daten behalten ihre `ResourceLease`-Besitzer.
Disc-Bytes werden nicht in native Zeigerstrukturen umgeschrieben.

Der originale `commonModelTrans` aus `src/game/trans.cpp:1019` wird **noch
nicht ausgeführt**. Seine untersuchten Vertex-, Sampler-, Cull-, Alpha- und
Blendkonfigurationen liefern die belegte Grundlage des begrenzten nativen
Adapters. `shaderSetup` (`:1323`), `materialSetup` (`:1739`), `alphaSetup`
(`:2374`), `smxInit` (`src/game/scroll.cpp:297`) sowie die Referenzen in
`model.cpp` und `TexRender.cpp` wurden analysiert, nicht als vollständig
portierte Renderfunktionen ausgegeben.

Das automatisch erzeugte [GX-Inventar](phase5a-gx-inventory.md) führt alle
66 benannten GX-Referenzen der drei angeforderten Originaleinheiten samt
Quellstellen und Grenzen auf. Ein Diagnose-Äquivalent ist kein allgemeiner
nativer GX-Entry. Nicht unterstützte Opcodes, VATs, Materialflags, Tabellen-
oder Texturreferenzen führen zu expliziten Fehlern.

## Geometrie-, Textur- und Kameraverarbeitung

Der BIN-Adapter verarbeitet die tatsächlich verwendete VAT0-Konfiguration:
INDEX16 für Position, Normal, CLR0 und TEX0; S16-Positionen mit originalem
Fraction-Shift, S8/6- oder S16/14-Normalen, RGBA8-Farben und S16/8-UVs.
NOP, Quads, Triangles und Triangle Strips werden mit geprüftem Winding zu
32-Bit-Indizes assembliert. Index-Tupel werden dedupliziert; Display-List-
Grenzen, Arraygrenzen und Materialreferenzen werden geprüft. Direkte FIFO-
Vertexausgabe, weitere VATs/Attribute/Primitive und allgemeine GX-Register-
Display-Lists bleiben offen.

Originale Modellhierarchien und SMD-Worktransformationen bestimmen die
Weltpositionen. Die inverse transponierte Weltmatrix wird für die erhaltenen
Normalen berechnet; der unbeleuchtete Shader verwendet diese noch nicht.
Die CPU berechnet Weltpositionen, die GPU multipliziert mit originaler View-
und Projektionsmatrix. Das ist mathematisch dieselbe Transformation; die
Rundung gegenüber einem originalen `view * model` pro Draw und die Genauigkeit
bei großen Weltkoordinaten sind noch gegen eine Originalrenderreferenz zu prüfen.

Der TPL-Decoder unterstützt I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8,
C14X2 und CMPR, einschließlich gekachelter Speicherung, Alpha, CLUT-Formaten
IA8/RGB565/RGB5A3 und aller deklarierten Mip-Level. **Im echten r120-Pfad
wurden I4 und CMPR benutzt und sämtliche Mip-Bytes unabhängig verifiziert.**
Die übrigen Formate/Paletten haben isolierte Adaptertests; eine bereits
umfassend geprüfte Bindung aller Formate an den vorhandenen Modellmanager
wird nicht behauptet. Dessen bisheriger eigener Formatumfang bleibt erhalten.

Textur- und Alpha-Texturindizes werden aus den originalen BIN-Materialteilen
übernommen. TPL-IDs, BIN-IDs, Work-IDs, Partindizes, SMX-Cull/Blend/Material-
farbe, Alpha-Grenzen, OT-Typen, Flags und UV-Scrollgeschwindigkeiten bleiben
im Diagnosebericht nachvollziehbar. Der S16-Modellpfad übernimmt die in
`commonModelTrans` belegten Repeat- und Linear/Trilinear-Vorgaben. Globale
Environment-LOD-Overrides sind noch nicht angeschlossen. UV-Scroll bleibt
auf dem initialen Offset 0; die erhaltene Geschwindigkeit wird nicht erfunden
oder durch neue Animationen ersetzt.

`r120` hat im DAS-CAM-Block null Kameraeinträge und vier Bereiche. Daher
verwendet der Nachweis die tatsächlich referenzierte erste Type-6-Kamera
des Originalevents: `event/r120/s00/cam/cam_s00_000.fcv`, Cut 0, Frame 0.
Die vier Originaltracks für Position, Ziel, Roll und FOV werden über den
Original-Move abgetastet. Parent-Camera-Pakete werden geprüft; nicht erfüllte
Basiskamera-Abhängigkeiten werden ausdrücklich abgebrochen.

Gemessene Kamera, gerundet: Position `(-118953.531, 1420.571, -590.926)`,
Ziel `(-119554, 1612.507, -1367.203)`, Roll `-0.0127 rad`, FOV `9.711634°`.
Aspect `1.3333334` stammt aus `camera.cpp:98`; Near `100` und Far `1000000`
aus `main.cpp:380–381`. Es gibt keine manuell gewählte freie Kamera.
Der originale SDK-Projektionsbereich Z `[-1,0]` wird im Vertexshader auf
Vulkan `[0,1]` abgebildet; Y wird für die Vulkan-Viewportkonvention korrigiert.

## Vulkan-Ausgabe und Lebenszeiten

Echte Vertex-/Indexbuffer, GPU-Texturen mit Mip-Leveln, Descriptor-Sets,
Sampler, Renderpass, Framebuffer und 15 Pipelines werden erstellt. Die
Pipelines kombinieren drei Cull-Modi mit den fünf Blendfaktor-Zeilen aus
`commonModelTrans`; Depth-Test/Write verwenden LEQUAL. Für Alpha-Material-
flag 4 wird die originale separate Alpha-Textur und Greater-Grenze verwendet.
Materialflag 0/4 ist der explizite Umfang; weitere Flags werden abgewiesen.
Diffuse RGB wird mit originalem SMX-RGB multipliziert. RASA-Alpha startet
opaque; das ersetzt ausdrücklich keine Lichtkanal- oder TEV-Berechnung.

Das Cull-Winding berücksichtigt den FRONT/BACK-Tausch in
`src/lib/GXGeometry.c:107` und Vulkan-Y-Korrektur. Als unabhängige technische
Referenz wurde auch die [Vulkan-Pipeline von Dolphin](https://raw.githubusercontent.com/dolphin-emu/dolphin/master/Source/Core/VideoBackends/Vulkan/VKPipeline.cpp)
für `VK_FRONT_FACE_CLOCKWISE` geprüft; keine Implementierungskörper wurden
aus Dolphin übernommen.

Der bestehende RGBA8-/D32-EFB hat 512×448 Pixel. Die Ausgabe wird tatsächlich
per GPU-Blit in eine zentrierte 4:3-VI-Fläche der Android-Swapchain übertragen.
IDENTITY-Surface-Transform wird verlangt; eine Plattform ohne diese Option
meldet die noch erforderliche Prerotation ausdrücklich. Die Darstellung
ist auf dem Retroid aufrecht und mit korrektem Seitenverhältnis sichtbar.

Upload-/Frame-Fences, Queue-/Device-Idle an Freigabegrenzen und gepinnte
Besitzer schützen Buffer und Images. Der diagnostische Framepfad arbeitet
serialisiert mit vollständigem Readback; er ist noch kein optimierter
mehrfach gepufferter Produktionsrenderer. Fehler beim Aufbau geben bereits
erstellte Ressourcen frei. Wiederholtes Schließen/Öffnen der Surface wurde
im selben Prozess auf beiden ABIs geprüft: jeweils genau eine native
Graphics-Domain nach dem erneuten Aufbau.

Der Szenenbericht wird beim Upload berechnet und anschließend wiederverwendet.
Der bestehende `presents`-/`verified_gpu_clears`-Zähler gehört zum früheren
Clear-Pfad; für den Geometriepfad sind `geometry_frames`, Readback und echte
Displayscreenshots die Nachweise.

## Reales r120-Ergebnis und private Nachweise

| Messgröße | Wert |
|---|---|
| Verwendete SMD-Works / tatsächlich initialisierte Modelle je Ladevorgang | 241 / 241 |
| Deduplizierte Vertices | 358563 |
| Triangles / ursprüngliche Primitive | 349703 / 49921 |
| Ursprüngliche Strips / Triangles / Quads | 49461 / 252 / 208 |
| Draws / eindeutige Texturdeskriptoren | 290 / 33 |
| Materialflag 0 / 4 | 213 / 77 |
| Nicht schwarze GPU-Pixel | 204045 von 229376 |

Prüfsummen der unveränderten Eingaben bzw. Ergebnisse:

- `St1/r120.das`: `97fbcf085571a687beadc5ba7239d610a313dccdc58c408246d6a31ae36717fb`
- Dekomprimierter SMD: `66b9d17de5c6cecb398d6563cc90cb71db523efcf691638819166560dc5a48c5`
- `Evd/r120s00.evd`: `ecf46d255e69415449d8f5245f14ec1815635fd13207fba34df2259d27838513`
- Native Geometrie, alle vier Architekturen: `eb025f23d918f063685b6f707b73d1d12a05aeacd12a72076c7497f045dbb0df`
- Vier tatsächliche RGBA-GPU-Readbacks: `e9dbab203189674993aee9772f9c4b12334c73ee3cba16d996eacd687813cac7`

Alle Originaldaten und abgeleiteten Bilder liegen ausschließlich unter
`E:\Development\Android\RE4-Data`, insbesondere `phase5a-gx`:
`gpu-{armeabi-v7a,arm64-v8a}-{1,2}.{rgba,png,json}` sind echte EFB-Readbacks;
`display-{abi}-{1,2}.png` sind ADB-Displayscreenshots der sichtbaren Ausgabe.
Die PNGs wurden visuell geprüft. Sie zeigen eine dunkle, texturierte Wald-
und Bergkulisse; Helligkeit/Beleuchtung, Fog, originale OT-/Transparenzordnung,
Eventobjekte, Schatten und Postprocessing sind noch nicht originalgetreu.
Ein vollständiger GameCube-Bildvergleich wurde nicht durchgeführt.

| Gerätelauf | Frames bis Nachweissicherung | Letzter gemessener Frame | Prozess |
|---|---|---|---|
| ARM32, Runde 1 | 22 | 40283 µs | 30491 |
| ARM32, Runde 2 | 13 | 36905 µs | 30491 |
| ARM64, Runde 1 | 15 | 46916 µs | 31076 |
| ARM64, Runde 2 | 21 | 39197 µs | 31076 |

Diese Zeiten umfassen natives Command-Recording, Submission, Synchronisation,
Readbackprüfung und SHA-256; sie sind **keine GPU-Timestamps, keine FPS-
Messung und kein repräsentativer Gameplaybenchmark**. Java-/Berichtsausgabe
und Choreographer-Abstände sind nicht eingeschlossen. Die Diagnose ist
bewusst auf überprüfbare Daten ausgelegt.

## Ausgeführte Builds und Tests

Retroid: Seriennummer `dbdcacc`, Android 13, `armeabi-v7a` und `arm64-v8a`
jeweils durch Installation, Paket-ABI und native Pointerbreite bestätigt;
Adreno 650, Vulkan 1.1.128. Windows: tatsächliche MSVC-x86- und x64-Builds.
Android: NDK 28.2.13676358, Offline-Gradle-Build für beide ABIs, Debug-/Test-
APK und Lint erfolgreich; GLSL wird mit `glslc` für Vulkan 1.0 kompiliert.

| Prüfung | Tatsächlicher Umfang | Ergebnis |
|---|---|---|
| Host CTest | Windows x86 und x64 | je 27/27 bestanden |
| Neue GX-Adapterprüfung | Host x86/x64, Android ARM32/ARM64 | je 337 Checks, null Fehler |
| Quellenextraktion | reproduzierbare neun Körper, absichtlich veränderte Kameraquelle | zwei Tests bestanden; Veränderung korrekt abgewiesen |
| Originaler r120-CPU-Pfad | zwei Läufe auf jeder der vier Architekturen | Primitive/Materialreferenzen, Kamera und alle Mip-Hashes gegen unabhängige Python-Parser bestanden |
| Echter Vulkan-Frame | zwei Activity-/Surface-Läufe je Android-ABI | echte GPU-Bilder, gleiche Hashes, Prozesswiederverwendung und eine Graphics-Domain bestanden |
| Bestehende Core-/Scheduler-/Hintergrund-/Yz2-Tests | Retroid, zwei Läufe je ABI | 13386 Checks ARM32 / 13387 ARM64, null Fehler; gleicher Schedulertrace; zusätzlicher High-Pointer-Check bei ARM64 |
| Bestehende APK-Instrumentierung | reale App-Prozesse beider ABIs | je 20/20 bestanden, einschließlich Originalruntime-/Controller-/Vulkan-Clear-Regressionen |
| Private Game-/Consumer-/Model-/Motion-/Light-Prüfung | Windows x86/x64 und beide Android-ABIs; beide Discs, zwei Runden | bestanden; bestehender Architekturvergleich ebenfalls bestanden |
| Ressourcen-/Raumregression | Host x64 und beide Android-ABIs; beide Discs, zwei Runden | 157 Ressourcen/85 Räume Disc 1 und 151/57 Disc 2 je Runde bestanden |
| REL-/Modulregression | Host x64 und beide Android-ABIs; beide Discs, zwei Runden | 110 bzw. 4 Original-RELs und bestehende teilweise native Registrierung bestanden |

Der neue Python-Oracle verwendet die bestehenden unveränderten
`tools/motion/archive.py`, `gxtex.py` und `fcv.py`. Er vergleicht tatsächliche
Primitive-/Dreieckszahlen, sämtliche Material- und SMX-Zuordnungen, originale
Kameratrackwerte und SHA-256 aller dekodierten Mips. Die synthetischen 337
Adapterchecks sind getrennt als Isolationstests gekennzeichnet; sie enthalten
Trunkierung, ungültige Indizes/Opcodes, bekannte Texel, Palette und SDK-
Projektionsgrenzen. Sie sind keine erfundenen Spielressourcen.

Der Vulkanloader meldet **null verfügbare Khronos-Validierungslayer** auf dem
Gerät. Es wurde kein Layer aktiviert; ein Nachweis „null Vulkan-
Validierungsfehler“ liegt deshalb nicht vor. Alle ausgeführten Vulkanaufrufe,
Readbacks und Präsentationen waren erfolgreich. Ein vollständiger API-
Validierungslauf bleibt Voraussetzung für die weitere Rendererarbeit.

Private Ergebnisse: `phase5a-gx`, `phase5a-gx-x86`,
`phase5a-regression-host64`, `phase5a-regression-host32`,
`phase5a-regression-host-combined`, `phase5a-regression-device`,
`phase5a-regression-resources`, `phase5a-regression-modules` und
`phase5a-regression-summary`. Letzteres enthält die automatisch überprüften
Architekturvergleiche. Build-/Testlogs und erzeugte Provenienz liegen im
ignorierten `port/build`; Originalbytes, GPU-Bilder und private JSON-Dateien
werden nicht in Git aufgenommen.

Während der Entwicklung fehlgeschlagene Prüfungen wurden behoben:
API-21-Lint wegen `java.util.function.Consumer` durch eigenen Callback ersetzt;
CRLF-Katalog auf dem Gerät durch festes LF korrigiert; SMD-Offsettabellen-
Alignment mit Nullwörtern korrekt begrenzt; verkehrtes Cull-Winding korrigiert;
falsche Surface-Prerotation korrigiert. Abschließende Builds und die oben
genannten Tests haben null Fehler. Ein Sandbox-ADB-Serverstart scheiterte;
die autorisierte Geräteprüfung lief anschließend außerhalb dieser Einschränkung.

## GameTask, Event und Leon bleiben ehrlich abgegrenzt

Der Architekturvergleich bestätigt weiterhin den vollständigen originalen
`PlayerInit()` innerhalb des GameTask und den nächsten echten Blocker
`MerchantGameInit()` (`src/game/game.cpp:271`); der separate New-Game-Zweig
hat zusätzlich die noch offene `ItemMgr.gameInit()`-Abhängigkeit (`:267`).
`R120Event` bleibt an der realen Sofdec-Initialisierung/Decoder-/Audio-/
Videoausgabe begrenzt (`src/st1/r120.cpp:74`). Keine Events werden übersprungen.

Alle bisherigen 37 r120-Event-BIN-Vorkommen sowie zusätzliche Modelle aus
r100 und r317 bestehen ihre CPU-Initialisierungsregressionen. Diese 37
Eventmodelle sind von den 241 statischen SMD-Works zu unterscheiden.
Originale Leon-Ressourcen und isolierte Original-`setModel`-/`setMotion`-
Helfer bleiben geprüft: im bisherigen Standard-pl00-Pfad 119 Joints,
acht ModelInfos und 14 Motion-Einträge aus sieben FCV/SEQ-Paaren.
Ein vollständiger cPlayer/cPlLeon-Actor wird weiterhin nicht behauptet.
Die neue Pipeline kann diese Modelle noch nicht korrekt ausgeben: Hierarchie,
Skinning/CalcSk1, Shape-/Motionpalette und weitere Materialflags müssen
entlang der Originalfunktion integriert werden; keine Ersatzpose/-animation
wurde eingesetzt.

Die GeometryActivity verwendet die bestehende `ControllerLoop` mit ihrer
zentralen nativen PadRead-Anbindung. Sie fügt keine Tastenbelegung, freie
Kamera oder Gameplaysteuerung hinzu. Die automatisierten Eingabe- und
Lifecycle-Regressionen bestehen; physische Tastendrücke wurden in diesem
Auftrag nicht erneut angefordert oder als neu geprüft ausgegeben.

## Konkrete offene Abhängigkeiten und nächster begrenzter Schritt

1. `commonModelTrans -> shaderSetup/materialSetup/alphaSetup` an einen
   expliziten allgemeinen GX-Zustandsadapter anschließen; originale
   Lichtkanäle, TEV-Operationen/Register/Stage-Reihenfolge einschließlich
   Alpha und Color-/Alpha-Write-States abbilden. Das ist der konkrete Blocker
   für originalgetreue Materialien statt der klar begrenzten Unlit-Diagnose.
2. Originale OT-/Trans-Reihenfolge und Light-/Fog-Zustände aus der vorhandenen
   CPU-Engine übernehmen. SMD-Dateireihenfolge ist kein Ersatz für OT.
3. Originale Skinning-/Hierarchiepfade für Leon/Eventmodelle anbinden, bevor
   ihre GPU-Ausgabe behauptet wird. Gruppen, gemeinsame SMD-Tabellen und
   animierte Scroll-Works brechen aktuell ausdrücklich ab.
4. Erweiterte GX-Funktionen aus dem Inventar implementieren und jeweils
   gegen Originaldaten prüfen: weitere Vertexkonfigurationen, direkte FIFO-
   Ausgabe, Texturmatrizen/Generators, TLUT-Managerbindung, indirektes TEV,
   TexRender-Copy-/Mirror-Ziele, Scissor/Viewport, Fog, Shadows und weitere
   Materialflags. Originale Camera-/SMD-Workreferenzen allgemein anbinden.
5. Layerfähigen Android-Validierungslauf und Vergleich mit einem tatsächlichen
   G4BE08-Referenzframe herstellen; danach GPU-Timestamps und gepufferte
   Ausgabe prüfen. Wiederholte bytegleiche Frames allein beweisen keine
   vollständige GameCube-Bildtreue.

MSAA Aus/2×/4×, AF 2×/4×/8× und persistentes DoF mit Nah-/Zielfokus bleiben
verbindlich. Aktuell sind 1 Sample, AF 1 und DoF aus aktiv und werden so
gemeldet. Das Retroid meldet gemeinsame Color-/Depth-Samplemaske 7 (1/2/4)
und Max-AF 16. Spätere Mehrfachsample-Farb-/Depth-Ziele mit Resolve, pro-
Sampler hardwarevalidierte AF und ein Depth-basiertes Postprocessing vor
der VI-Präsentation können den vorhandenen EFB-/Swapchain-Aufbau erweitern.
Für DoF ist ein tatsächlich samplbares/aufgelöstes Depth-Ziel erforderlich;
es ist heute nicht als Postprocessing implementiert. Backendunabhängiger
gewünschter/effektiver Zustand und persistente Einstellungen bleiben in der
bisherigen Architektur dokumentiert; kein Menü zeigt unimplementierte
Optionen als aktiv. Phase 5b wurde nicht begonnen.

## Reproduktion

Vorhandene Host-Konfigurationen `port/build/host-ninja` und `host-x86`
mit MSVC-x64 bzw. x86 bauen, danach jeweils
`ctest --test-dir <build> --output-on-failure`. `re4_gx_tool self-test`
führt die isolierten Checks aus. Die Original-Szene wird ausschließlich
mit privatem Datenroot/Katalog geladen.

```powershell
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -Offline
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-gx' --host-exe port/build/host-ninja/re4_gx_tool.exe --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-gx-x86' --host-exe port/build/host-x86/re4_gx_tool.exe --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-gx' --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' --serial dbdcacc --repeat 2
.\port\scripts\test-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
python port/tools/inventory_gx.py
```

Für die privaten Regressionen werden die unveränderten
`test_game_runtime.py`, `test_resources.py`, `test_modules.py` jeweils mit
`--data`, externem `--output`, Host-Tool oder `--adb --serial` und
`--repeat 2` verwendet. Python benötigt die bereits vorhandenen Import-
Abhängigkeiten einschließlich Pillow für Bildnachweise. Im Gerätetest
werden nur die benötigten unveränderten CAS-Dateien kopiert; keine Assets
werden in eine APK oder in Git eingebettet.

## Vollständige Projektdateien und Git

Neu gespeichert:

- `port/include/re4/port/gx_scene.h`
- `port/src/gx_scene.cpp`, `port/src/gx_original.h`
- `port/config/gx-sources.json`
- `port/shaders/gx_diagnostic.vert`, `port/shaders/gx_diagnostic.frag`
- `port/android/app/src/main/java/org/re4/port/GeometryActivity.java`
- `port/tests/gx_tool.cpp`, `port/tests/gx_verification.h`, `port/tests/test_gx_sources.py`
- `port/tools/generate_gx.py`, `port/tools/embed_spirv.py`,
  `port/tools/test_gx_scene.py`, `port/tools/inventory_gx.py`
- `port/docs/phase5a-gx-inventory.md`, dieser Bericht `port/docs/phase5a-status.md`

Geändert und vollständig gespeichert:

- `port/CMakeLists.txt`
- `port/platform/android/vulkan_bootstrap.h`, `port/platform/android/vulkan_bootstrap.cpp`
- `port/platform/android/graphics_jni.cpp`
- `port/android/app/src/main/AndroidManifest.xml`
- `port/android/app/src/main/java/org/re4/port/GraphicsBootstrapView.java`

Die aktualisierte README von `origin/master` wurde gelesen und berücksichtigt.
Lokaler `master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Alle bisherigen Entwicklungscommits bleiben erhalten. Ausschließlich der
geprüfte Teilstand wird lokal auf `android-port` committed; keine Merges,
Rebases, Pushes oder Änderungen an Originalassets bzw. Originalquellen.
