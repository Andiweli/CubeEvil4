# Phase 5a: originale OT-, Material- und Lichtsteuerung über Vulkan

Geprüfter Teilstand, ausgehend von `b0071bda3a4a97737c486d7df353970a013ccb26`
auf `android-port`. Prüfung am 10. Oktober 2026, Windows x86/x64 und
Retroid Pocket Flip2 `dbdcacc`, Android 13/API 33, Adreno 650/Vulkan 1.1.128.

## Ergebnis und Abnahmegrenze

Die statische Originalgeometrie von **r120 und r119** wird mit originalen
TPL-Texturen und Eventkameras ausgegeben. Neu laufen originale Ordering
Tables, Material-/Alpha-Bodies, Lichtauswahl und Lichttyp-Callbacks in einem
abgegrenzten nativen Capture-Kontext. Vulkan verarbeitet deren Zustände;
es gibt keine raumspezifische Sortierung oder erfundene Lichtquellen.
Alle vier CPU-Architekturen liefern für jeden geprüften Raum denselben
OT-/TEV-/Lichtzustands-Hash. Je Raum sind vier echte GPU-Readbacks aus
ARM32/ARM64 und Activity-Wiederinitialisierung byteidentisch.

**Phase 5a ist noch nicht vollständig abgeschlossen.** Der Renderpfad bleibt
ein isolierter Originaldaten-Nachweis. Vollständiges `Trans/Render`, dynamische
Event-Sichtbarkeit, modellgebundene Lichter, lebende SAR-Zustände und sichtbare
Leon-/Eventmodell-Animation mit Skinning fehlen. Die Bilder sind dunkel;
eine passende GameCube-Referenz derselben Szene/Kamera fehlt. Bytegleichheit,
Originaldaten und fehlerfreie Vulkan-Validierung beweisen keine vollständige
visuelle Originaltreue. Es wurden keine fehlenden Events übersprungen und
keine Gameplay-Zustände künstlich weitergeschaltet. Phase 5b wurde nicht begonnen.

## Originalbodies und tatsächlich ausgeführte Pfade

`port/tools/generate_gx.py` erzeugt nach Quellhash-Prüfung vollständige
Funktionsbodies mit `#line`-Quellzuordnung. Die zusätzliche Render-Provenienz
steht in `gx-render-manifest.json` unter dem jeweiligen CMake-Buildverzeichnis.
Sie unterscheidet vollständige Bodies von zwei ausdrücklich markierten
Quellsegmenten. `port/config/gx-sources.json` schützt die maßgeblichen Dateien.
Die Dateien in `src/` und `include/` wurden nicht verändert.

| Originalquelle | Neu integrierte Funktionen / Umfang |
|---|---|
| `src/game/trans_ot.cpp` | `InitOt`, `ClearOt`, `AddOtDirect`, `ExecOt`, `OtGetPrevKind`, `DeleteOtData`, `CrearOtMirrorWork` und originale Tabellengrößen. |
| `src/game/trans.cpp:1441` | `TextureBlend`, `TextureBlend2`, `TextureBlend3`, `materialSetup`, `alphaSetup` samt tatsächlichen Register-/TexMap-/TexCoord-Helfern. |
| `src/game/trans.cpp:1323` | **Nur** der vollständige abschließende Scale-/Stage-Registrierungsabschnitt von `shaderSetup`, als `finishModelScale`. Schatten-/Specular-/Bump-Zweige des Gesamtbodys fehlen weiterhin. |
| `src/game/trans.cpp:905` | Tatsächliche `ExecOt`-Aufrufreihenfolge aus `Render` extrahiert; **nicht** der vollständige `Render`-Body. |
| `src/game/light.cpp:1036` | `cLightMgr::loadLit` erzeugt echte native Lichtobjekte aus den geprüften Original-Cut-Records, über den bereits vorhandenen Originalpool. |
| `src/game/light.cpp:482` | `cLightMgr::checkKind`, `setModel2`, `lightHitCheck` und Sphere-/Cylinder-/BBox-Zweige. |
| `src/game/lightInfo.cpp` | Originaler Konstruktor, `init2`, `updateMatrix`, `getPos`. Die statischen SMD-Bounds/Rootmatrizen stammen aus der tatsächlichen Modellinitialisierung. |
| `src/game/trans_lit.cpp:49` | `LightSetInit`, `LightSetModel`, `lightSetAmbient`, alle acht Typ-Callbacks und `lightSetColor`. `LightDisable` zusätzlich kompiliert, im neuen Nachweis nicht gesondert aufgerufen. |
| `src/game/model.cpp` / `include/model.h` | Originaler `cModelState`-Konstruktor und Zustandsmethoden; SAR-Farbverbraucher isoliert geprüft. |
| `src/lib/GXLight.c` | Vollständige SDK-Bodies `GXInitLightSpot`, `GXInitLightDistAttn`, `GXInitLightAttnK`. |
| SDK-/Math-Dateien | Zusätzlich tatsächliche `C_MTXMultVecSR`, `C_VECScale`, `GetDistance3`, `RootSumSquare3`, `RotVector`, `low_RotMatrix`; bisherige Original-Kamera-/Matrixfunktionen bleiben erhalten. |

Native Anpassungen betreffen Typ-/Owner-/Allokationszugriff, sichere
Pointerbreite und SDK-Zustandserfassung. Der GameCube-Adressenbereichstest
wird durch geprüfte native Owner-/Nullreferenzen ersetzt. RGB-Nulltests
lesen Farbbytes explizit, anstatt einen Big-Endian-`u32` auf Little-Endian
neu zu interpretieren; Bitmasken verwenden unsigned Shifts. Ein OT-Listen-
abschluss bildet keinen Pointer vor dem Anfang der nativen Allokation;
Reihenfolge und Nullende bleiben erhalten. Unverstandene Funktionen erzeugen
einen konkreten Fehler. Der Capture-Namespace ist kein
fälschlich vollständiger allgemeiner GX-Entry für die gesamte Engine.
Die echten Raumläufe führen die unten aufgeführte Load-/Model-/OT-/Material-
und Lichtauswahlkette aus. OT-Löschen, TextureBlend-Varianten, acht Lichttypen
und SAR-Farbverbraucher werden zusätzlich mit isolierten Ownern gegen echte
Originalbodies geprüft. Daraus folgt keine Behauptung, dass jeder kompilierte
Helfer oder jeder Zweig bereits in einem sichtbaren Originalframe vorkommt.

## Originale Renderreihenfolge und Materialzustände

SMD-Works werden in Originaldateireihenfolge registriert, entsprechend
`Smd::setObj -> ObjMgr.createBack` und dem aktiven Listenlauf von `Trans`.
Ein Work erhält einen originalen OT-Knoten; seine Parts bleiben in
ursprünglicher Reihenfolge. `AddOtDirect/ExecOt` bestimmen Bucket-Reihenfolge
und LIFO innerhalb eines Buckets. Es wird keine zusätzliche r120-Sortierung
verwendet. Native OT-Strukturen tragen echte pointerbreite Referenzen.

Für die nachgewiesenen statischen Modelle werden die tatsächlichen
`ModelTrans`-Zweige aus `src/game/trans.cpp:466` abgebildet:

| ot_type | OT / Bucket |
|---|---|
| 2 | 11 / 4 |
| 3 | 11 / 2 |
| 4 | 11 / 1 |
| 5 | 16 / 0 |

Diese Zuordnung ist ein ausdrücklich abgegrenzter Adapter für `ModelTrans`,
kein vollständig kompilierter Ersatzbody. Typen 0/1 brauchen originale
Tiefen-/Projektionspriorität, Typ 6 Itemkamera und Typ 7 Mirror und brechen
derzeit ab. Originale Render-OT-Dispatches 0..21 laufen; Shadow-, Background-,
Filter- und ClearZ-Pässe des vollständigen `Render` fehlen. Die geprüften
statischen Ausgaben liegen in OT 11/16 nach dem initialen Depth-Clear.
Frustumprüfung verwendet sechs aus der originalen SDK-Projektions-/Viewmatrix
abgeleitete Clipplanes mit GX-Depth [-w,0]; sie ist ein nativer Adapter,
**nicht** der Originalbody von `collision_sphere_hexahedron`.

Die neuen Shader verarbeiten pro Draw den tatsächlich erfassten Materialzustand:

- Bis 16 TEV-Stages, acht Texturbindungen, vier TEV-Register; ADD/SUB,
  Bias, Scale 1/2/4/halb, Clamp bzw. signed Register, Color-/Alpha-Inputs.
- Originale Mehrstufen-TextureBlend-Bodies, K0..K3 RGB-/Alpha-Werte;
  Texture Generation aus TEX0, MTX2x4 Identität bzw. UV-Scale/Translation.
- Alle acht Alpha-Vergleiche und AND/OR/XOR/XNOR. Originaler später
  Alpha-/Depth-Test, drei Cull- und fünf bereits vorhandene Blendmodi.
- Acht GX-Lichtobjekte mit View-Position/-Richtung, A-/K-Koeffizienten,
  Farbe, Diffuse-Clamp und Spotattenuation. Originale BIN-Flags wählen
  Vertexambient/-alpha oder Material-/Ambientregister.
- LIT-Fog aus/linear/exp/exp2/rev-exp/rev-exp2 und ursprüngliche Farben,
  Original-TEV-Scale und LOD-Umgebungswerte für den ausgewählten Cut.

Integerkombination und Rundung im TEV-/Light-Shader wurden anhand der
Original-SDK-Zustände und der primären
[Dolphin-TEV-Implementierung](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/PixelShaderGen.cpp)
und [Dolphin-Lichtimplementierung](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/LightingShaderGen.cpp)
überprüft; es wurde keine neue Spiellogik übernommen. Die GLSL-Implementierung
ist ein Plattformadapter, kein dekompilierter Originalshader. Fog-BP-
Koeffizientenquantisierung und Range-Adjustment sind noch nicht exakt
nachgebildet; daher wird keine bitgenaue GX-Fog-Emulation behauptet.

Explizit offen: TEV-Vergleichsoperationen, zusätzliche Konst-Selektoren,
Normal-/Position-/Postmatrix-TexGen, indirekte Texturen, Specular/Bump,
Thermo/GI/Shadow, zusätzliche Kanalmodi und frühes ZCompLoc. Der aktuell
verwendete BIN-Materialumfang erlaubt nur Flag 4 (separate Alphatextur);
andere Flags werden mit Ressourcen-/Funktionskontext abgebrochen.
`phase5a-gx-inventory.md` erfasst alle benannten GX-Referenzen der drei
angeforderten Originaleinheiten mit dem jeweiligen Unterstützungsumfang.

Die bisherigen Decoder für I4/I8/IA4/IA8/RGB565/RGB5A3/RGBA8/C4/C8/C14X2/CMPR,
TLUT IA8/RGB565/RGB5A3, Swizzling, Originalmips und TPL-Sampler bleiben erhalten.
Die beiden realen Raumprüfungen benötigen keine CLUT; Palettenpfade sind
isolierte Formatprüfungen. Die unabhängige Python-Referenz vergleicht
sämtliche tatsächlich verwendeten RGBA-Mipdaten. Bisherige VAT0-POS/NRM/
CLR0/TEX0-INDEX16-Display-Lists (NOP/Quads/Triangles/Strips) bleiben der
nachgewiesene Geometrieumfang. Direkte FIFO-Ausgabe und andere VATs fehlen.

## Licht, SAR und Motion: belegter Umfang

`captureOriginalRoomLighting` lädt `etc/core.das` über den bestehenden
Original-DVD-/Read-/Yz2-/CAS-Pfad, bindet Core-/Raumarchive an `LightRuntime`
und lädt den LIT-Cut der ersten echten Kameraanweisung im angegebenen EVD.
`loadLit -> create` und danach `cLightInfo::init2/updateMatrix -> setModel2 ->
LightSetModel -> Typ-Callback -> GXLoadLightObjImm` laufen tatsächlich.
Referenzen halten die unveränderten Datenowner. Ein erneuter Bulk-Lichtload
invalidiert alte Handles; Ende/Exception entfernen Capture-Referenzen vor
Pool-/Ownerfreigabe. Eine lebende Managerbelegung wird nicht überschrieben.

Unterstützt sind **freistehende** Originallichter. ParentType != 0 führt
zu einer konkreten fehlenden Model-/Actor-Abhängigkeit. Die ursprünglichen
Positions-/Normalpfade `cLight::getCoord/calcPos/getNormal` benötigen dafür
noch reale Enemy-/Object-/Scroll-Managerreferenzen. Es werden keine Eltern
oder Ersatzpositionen erfunden.

Originales `lightSetColor` verarbeitet jetzt auch originale `cModelState`
LightIgnore/LightNo/LightPow-Zustände. Tests prüfen eine passende und eine
nicht passende Lichtnummer gegen den tatsächlichen Originalbody. Das ist
ein **isolierter SAR-Zustandsverbrauchertest**, keine Integration des
SAR-Regionslaufs aus `src/game/light_area.cpp:102`. `LightAreaUpdateSub`
braucht lebende Actorpositionen, Raum-SAR-Referenzen und deren tatsächliche
Lebenszeiten. Der Diagnose-Cut bleibt eingefroren, einschließlich Flags;
Eventcallbacks und laufende Cut-Interpolation werden nicht vorgetäuscht.

Die vorhandenen 37 r120-BIN-Vorkommen, originale pl00-Hierarchie mit 119
Gelenken und Motion-Initialisierung bestehen weiterhin in den CPU-Tests.
**Sie werden noch nicht als skinnierte/animierte GPU-Modelle gezeichnet.**
Der statische SMD-Renderpfad lehnt Motion-/Shape-/Weightpalette explizit ab.
Neue Leon-Animationen wurden weder erfunden noch als fertig gemeldet.

Konkrete Originalkette für den fehlenden Nachweis:
`commonScreenMatSub` (`trans.cpp:671`) -> `calcWeightMat` (:787) ->
`MakeWeightPaletteExt/MakeWeightPalette` (:808/:855) -> `CalcSk1_x/CalcSk1_x2`
(:2485/:2521). Die letzten Kernel verwenden PPC-Paired-Single/GQR6 und
Locked-Cache-Adressen; native Matrix-/Palettenowner, S16/S8-Quantisierung
und geprüfte Rechenpräzision müssen erst sicher angebunden werden.
Das ist der nächste konkrete Blocker für sichtbare Leon-/Eventmodelle.

## GPU-Nachweise und visueller Vergleich

Alle privaten Eingaben, Bilder und Originaldatenberichte liegen außerhalb
von Git unter `E:\Development\Android\RE4-Data`. Quellen sind die geprüften
G4BE08-Debug-Discs über `import-phase3a/manifest.json` und dessen SHA256-CAS.

| Nachweis | r120 / `Evd/r120s00.evd` | r119 / `Evd/r119s10.evd` |
|---|---:|---:|
| CPU-initialisierte statische Modelle | 241 | 180 |
| Dekodierte Parts / OT-sichtbare Parts | 290 / 18 | 290 / 51 |
| Dekodierte Dreiecke vor OT/Frustum | 349703 | 137095 |
| Primitive / Originaltexturen | 49921 / 33 | 16798 / 49 |
| Original-LIT-Cut / Records | 0 / 3 | 0 / 13 |
| ARM32 wiederholte Frames | 12, 30 | 14, 15 |
| ARM64 wiederholte Frames | 25, 20 | 13, 25 |
| Readback-Auflösung | 512×448 RGBA | 512×448 RGBA |
| Shader-/OT-Zustand SHA256 | `8a234b1b9a0e8718955b1f8d050d521f027a5eb0aad0dff7507b445e12154621` | `d5ed167b1ecb630b7ad848173b3b9644dec09b18c85e3aae621c4b1ec9a11764` |
| GPU-RGBA SHA256, je vier gleiche Bilder | `15401308c0eab45195164bf6ad4318b826cc8e15b7342bbfebfcf536f87bdfc7` | `9f0773766ee9529829cbff6f95225582bd62538307ee6d6d221281ed618b08ac` |

Bildpfade: `phase5a-pipeline/gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png` und
`phase5a-pipeline-r119s10/gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`.
Daneben liegen originale GPU-RGBA-Dateien, JSON-Status und echte
ADB-Displayscreenshots. Pro ABI bleiben beide Activity-Runden im selben
Prozess; native Graphics-Domainzahl ist jeweils 1. Jede Runde erhält
eine eindeutige Evidence-ID und eigene Bild-/Statusdateien.

Der erste Wiederholungslauf fand einen **Testablauffehler**: Ein alter
Activity-Bericht wurde mit einer noch fehlenden neuen Bilddatei gepaart.
Die 55 gelesenen Bytes waren ADBs `cat: ... No such file or directory`,
kein beschädigtes GPU-Bild. Der fehlgeschlagene Nachweis bleibt privat als
`gpu-armeabi-v7a-2.failed-readback.bin` erhalten. Eindeutige Aufnahme-IDs
beseitigen die Zuordnungsrace; Hash-/ABI-/Frame-/Ownerassertions wurden
nicht abgeschwächt. Die abschließenden acht GPU-Nachweise bestehen.

Der vom Nutzer aufgenommene Dolphin-Screenshot
`C:\Users\andiw\AppData\Roaming\Dolphin Emulator\ScreenShots\G4BE08\G4BE08_2026-10-10_23-42-29.png`
wurde gelesen und privat als `phase5a-pipeline/dolphin-reference-r100.png`
gesichert. Größe 583×448, SHA256
`48e8473fa8ef99d989d65b541ae33224d9daed46f73c86f5f0f1dfa15c1df825`.
Er zeigt Leon, Waldboden, Vegetation, Cockpit und Debugtext zu r100s43.
Die tatsächliche Raumfolge belegt `src/st1/r120.cpp:99`:
Intro/Autofahrt r120 -> `SceAtExecRoomJump(0x100, ...)` -> begehbarer Wald r100.

Die neue r120-Aufnahme zeigt Baumkronen und entfernte Berg-/Waldsilhouetten
aus `event/r120/s00/cam/cam_s00_000.fcv`, Frame 0. r119 zeigt Originalfelsen,
Weg und Bauteile, ebenfalls mit eingefrorener echter Eventkamera. Beide
Ausgaben sind deutlich dunkel. Leon, Eventobjekte, Cockpit, Schatten,
Spiegel und Originalpostprocessing fehlen. Unterschiedliche Räume und
Kameras erlauben **keinen** Pixelvergleich, keine bestätigte Farb-/Fogtreue
und keine behauptete identische räumliche Bildkomposition. Eine r120-
Referenz derselben Intro-Kamera/Cut/Frame und eine r119-Referenz stehen aus.
Der spätere Vergleich muss auch Dolphin-Backend/Enhancements, VI-Crop und
Original-EFB-Auflösung dokumentieren. Der allgemeine G4BE08-Screenshot
ersetzt diese offenen Nachweise nicht.

## Vulkan-Validierung, Lebenszeiten und Messwerte

Die optionalen Debug-Libraries aus dem offiziellen
[Khronos-Release 1.3.290.0](https://github.com/KhronosGroup/Vulkan-ValidationLayers/releases/tag/vulkan-sdk-1.3.290.0)
sind für beide ABIs geladen. Archiv-SHA256:
`8eb74be5403f72caac9502c2d5eb5a9353dfcb10695f198374d6e481445036c1`.
Sie liegen ausschließlich unter ignoriertem `port/build/validation` und
werden nur über `-ValidationLayers` in den Debug-Build eingebunden,
entsprechend der [Android-Validierungsanbindung](https://developer.android.com/ndk/guides/graphics/validation-layer).
Instance-Layer/Debug-Messenger zählen echte Warnungen/Fehler; sie werden
zusätzlich mit Logtag `RE4Validation` protokolliert.

Alle acht neuen GPU-Läufe: Validation aktiv, **0 Fehler**, jeweils **1
Performance-Warnung** `WARNING-Swapchain-PreTransform`. Die Surface meldet
ROTATE_90, der bestehende Adapter präsentiert mit IDENTITY über den
Compositor. Es wurde keine VUID-Ressourcen-/Descriptor-/Layoutverletzung
beobachtet. Die Performancewarnung ist offen: Vorrotation müsste zusammen
mit VI-/Touch-/Readback-Koordinaten geprüft werden. Synchronization-
Validation und GPU-Assisted Validation wurden nicht zusätzlich aktiviert;
keine vollständige Racefreiheit wird aus Core-Validation allein abgeleitet.

Neue Material-UBOs tragen ausschließlich pointerfreie feste Typen, geprüftes
std140-Layout (2368 Bytes, Lightoffset 1696), deviceabhängige Alignment-
Grenzen und pro Draw acht Texturdescriptoren plus UBO. Descriptorindizes
sind shaderseitig konstante Bindungen; ARM32 braucht keine optionale
SampledImageArrayDynamicIndexing-Funktion. Scenewechsel/Freigabe warten
auf die tatsächliche GPU; alte Descriptor-/Buffer-/Textureowner werden
danach freigegeben. Readbacks verwenden Transfer-/Hostbarrieren und Fence.
Surface-/Activity-Neuerstellung ist tatsächlich wiederholt geprüft.

Gemessene letzte vollständige Diagnose-Frame-Aufrufe:
r120 ARM32 42.289/42.297 ms, ARM64 43.159/42.485 ms;
r119 ARM32 41.006/47.781 ms, ARM64 41.170/38.949 ms.
Das sind **CPU-Wandzeiten inklusive Acquire/Submit/Present, Queue-/Fence-
Warten, GPU-Readback, Pixelzählung und SHA256**, mit aktiven Validation-Layers.
Sie sind keine isolierten GPU-Timestamps, kein FPS-Benchmark und keine
Leistungsprognose für die vollständige Engine.

MSAA Aus/2×/4×, AF 2×/4×/8×, DoF Nah-/Zielfokus und persistente Einstellungen
bleiben verbindlich. EFB-/Depth-/Present-Stufen bleiben getrennt, Sampler
werden pro Originaltextur erstellt; spätere Resolve-/Depth-Sampling- und
Postprocessing-Stufen können daran anschließen. Heute werden korrekt nur
1 Sample, AF 1, DoF aus gemeldet; kein Menü behauptet aktive Optionen.
Die Retroid-Capabilities bleiben Samplemaske 7 (1/2/4), Max-AF 16.

## Tatsächlich ausgeführte Tests

| Lauf | Ergebnis / Einordnung |
|---|---|
| Windows MSVC x64 / x86 | Beide native Buildbasen erfolgreich; je **27/27 CTest**. |
| GX-Checks alle vier ABIs | Je **374** erfolgreiche Prüfungen: bisherige Decoder-/SDK-Checks plus **37** neue Original-OT-/Material-/Lichtbodyprüfungen mit isolierten nativen Ownern. |
| Android ARM32/ARM64 / Gradle | Native CLI/JNI, Debug-APK/Test-APK und Lint erfolgreich. SDK-XML-Versionswarnung des vorhandenen Werkzeugmixes bleibt ohne Buildfehler. |
| Retroid Core/Scheduler, je zwei Runden | ARM32 **13386**, ARM64 **13387** Checks, 0 Fehler; originale Scheduler-/Background-/Yz2-Traces identisch. ABI-bedingte Prüfzahldifferenz aus bestehenden Typprüfungen, kein Ablaufunterschied. |
| APK im tatsächlichen Prozess | **20/20 je ABI**, nach der Aufnahme-ID-Korrektur erneut bestanden; zentrale Controller-/PadRead-/Lifecycle-Anbindung und Vulkan-Clears erhalten. Physische Tasten wurden diesmal nicht neu angefordert. |
| GameRuntime, beide Discs, je zwei Runden, alle vier ABIs | Tatsächliche Original-Consumer-/Message-/SubScreen-/Checkpoint-/ID-/Player-/Modell-/Motion-/Lichtläufe bestanden, einschließlich 37 r120-BINs, pl00 und weiterer Eventmodelle. Licht-CPU-Checks Disc 0 **11049**, Disc 1 **8399**. |
| Ressourcen, Host x86/x64, je zwei Runden | Disc 0 **157 Ressourcen / 85 Räume**, Disc 1 **151 / 57**; Originalreferenzen/Transfers/Freigaben bestanden. |
| Module, Host x86/x64, je zwei Runden | Disc 0 **110 RELs**, Disc 1 **4 RELs**, bestehender Umfang partieller nativer Stage-Registrierung/Original-Init bestanden. Kein PPC-REL-Code wird als nativ ausgeführt ausgegeben. |
| Ressourcen und Module, Retroid ARM32/ARM64, je zwei Runden | Nach erneuter erfolgreicher Übertragung dieselben **157/151 Ressourcen, 85/57 Räume und 110/4 RELs** beider Discs geprüft; wiederholte ABI-Traces identisch. |
| r120/r119, Host x86/x64 + Retroid ARM32/ARM64 | Je zwei CPU-Läufe mit realem CAS; unabhängig geprüfte Primitive, Material-IDs/Referenzen, Originalkamera, Miphashes, LIT und OT. Alle nativen Zustands-Hashes über Plattformen gleich. |
| GPU, beide Räume, beide Android-ABIs | Je zwei Activity-Runden, tatsächliche Draws/Readbacks/Präsentation, kein Clear-Ersatz; acht Nachweise bestanden. |

Private Rohberichte liegen in `phase5a-pipeline*`, ignorierte Konsolenlogs
in `port/build/pipeline-*.log`. Temporäre Host-Sandboxzugriffe auf CAS/Test-
Fixture-Verzeichnisse wurden außerhalb der Sandbox wiederholt und bestanden;
Umgebungsfehler wurden nicht als Engine-Erfolg gezählt. Der zusätzliche
Android-Ressourcenlauf brach zunächst bei Blobübertragung wegen des lokalen
ADB-Daemons ab; der erneute Lauf und der anschließende Modullauf bestanden
vollständig. Die erste Fehlermeldung bleibt in
`pipeline-resources-device.log`, der erfolgreiche Lauf in
`pipeline-resources-device-retry.log` und `pipeline-modules-device.log`.

Explizite Negativ-/Grenzprüfungen mit Originaldaten: r100 scheitert an
Motion/shared-SMD-Scope, r101/r117/r11f an modellgebundenen Lichtreferenzen,
r102/r104/r105 an Specular-Materialflag 2. Das erste EVD von r119s00 benötigt
noch FCC15-Interpolation mit mehreren Keys. r119s10 besitzt eine andere
**echte** erste Kamera und funktioniert ohne Raumsonderregel. Die fehlende
Rollspur ist gemäß originalem `CameraMotion::move` zulässig (Default 0);
Position/Target/FOV bleiben verpflichtend. Diese Fehler sind Grenzen,
keine ersatzweise erfolgreich gerenderten Räume.

GameTask erreicht weiterhin den echten Blocker `MerchantGameInit()`
(`src/game/game.cpp:271`, neuer Spielzweig außerdem `ItemMgr` :267).
`R120Event` endet im geprüften Lauf weiterhin an
`Sofdec.Initialize("movie/opening.sfd", 0)` (`src/st1/r120.cpp:74`).
Die Renderdiagnose löst diese Gameplay-/Video-Abhängigkeiten nicht und
ändert deren Reihenfolge nicht.

## Reproduktion und nächste begrenzte Abhängigkeiten

Vorhandene CMake-Hostkonfigurationen `port/build/host-ninja` (MSVC x64) und
`host-x86` (MSVC x86) bauen, danach jeweils `ctest --test-dir <build>
--output-on-failure`. Private Daten benötigen das unveränderte Phase-3a-
Manifest/CAS. Ohne Daten können nur Isolationstests laufen.

```powershell
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python '<Python-3.9+-mit-Pillow>' -ValidationLayers '<lokales-android-binaries-1.3.290.0>' -Offline
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-pipeline' --host-exe port/build/host-ninja/re4_gx_tool.exe --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-pipeline' --host-exe port/build/host-x86/re4_gx_tool.exe --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-pipeline' --adb '<SDK>/platform-tools/adb.exe' --serial dbdcacc --repeat 2
python port/tools/test_gx_scene.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase5a-pipeline-r119s10' --room 119 --event Evd/r119s10.evd --adb '<SDK>/platform-tools/adb.exe' --serial dbdcacc --repeat 2
.\port\scripts\test-device.ps1 -Adb '<SDK>/platform-tools/adb.exe' -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb '<SDK>/platform-tools/adb.exe' -Serial dbdcacc
python port/tools/inventory_gx.py
```

Für r119 dieselben beiden Hostaufrufe mit `--room 119 --event Evd/r119s10.evd`
und dessen privatem Ausgabeordner ausführen. Bestehende
`test_game_runtime.py`, `test_resources.py`, `test_modules.py` verwenden
`--data`, privaten `--output`, Host-Tool bzw. `--adb --serial`, `--repeat 2`.
Das Skript kopiert erforderliche Originalblobs nur in private Gerätedaten;
keine APK enthält GameCube-Assets. Ohne optionalen Layerpfad ist der Build
weiter möglich, der GPU-Bericht meldet dann den tatsächlichen Layerstatus.

Nächste Schritte bleiben innerhalb noch offener Phase-5a-Abhängigkeiten:

1. PPC-Weight-/Skinning-Kette für vorhandene pl00-/Eventmodellowner anbinden
   und mit Originalpaletten/Motions überprüfen; danach sichtbarer Actor-Nachweis.
2. Modell-Elternreferenzen und originale SAR-Regionenauswahl mit lebenden
   Actor-/Objektmanagern verbinden, Cut-/Event-Änderungen statt eingefrorenem
   Snapshot ausführen.
3. Vollständige `ModelTrans/commonModelTrans/shaderSetup/Render`-Pfadbindung,
   fehlende Specular-/Bump-/indirekte TEV-/TexRender-/Shadowzustände ergänzen;
   keine offenen GX-Aufrufe stillschweigend überspringen.
4. Passende G4BE08-Referenzframes aufnehmen und Fog/Kanal-/TEV-/Render-
   Reihenfolge quantitativ prüfen; danach PreRotation und GPU-Timestamps.

## Vollständig gespeicherte Projektdateien und Git

Neu: `port/include/re4/port/gx_pipeline.h`, `port/src/gx_pipeline.cpp`,
`port/src/gx_render_original.h`, `port/shaders/gx_state.glsl`, dieser Bericht.

Geändert: `port/CMakeLists.txt`; `port/config/gx-sources.json`;
`port/include/re4/port/gx_scene.h`, `lights.h`;
`port/src/gx_scene.cpp`, `gx_original.h`, `lights.cpp`, `light_original.h`;
`port/tools/generate_gx.py`, `generate_lights.py`, `inventory_gx.py`,
`test_gx_scene.py`; `port/tests/gx_verification.h`, `test_gx_sources.py`;
`port/shaders/gx_diagnostic.vert`, `gx_diagnostic.frag`;
`port/platform/android/vulkan_bootstrap.cpp`, `graphics_jni.cpp`;
`port/android/app/build.gradle.kts`;
`port/android/app/src/main/java/org/re4/port/GeometryActivity.java`,
`GraphicsBootstrapView.java`; `port/scripts/build-android.ps1`;
`port/docs/phase5a-gx-inventory.md`, `phase5a-status.md`.

Alle Dateien liegen vollständig im Projekt, keine bloßen Patch-Snippets.
Buildprodukte, Validation-Libraries, Bilder und Assets bleiben ignoriert
oder außerhalb des Repositories. Die README von `origin/master` wurde
berücksichtigt. Lokaler `master` bleibt
`9b76ff003081d6ece8b838453fb8310743a972c7`, `origin/master` bleibt
`15cf64f80e08bbc64712c39be330b75d1512a0d0`. Alle bisherigen Entwicklungs-
commits bleiben erhalten. Der geprüfte Teilstand wird ausschließlich lokal
auf `android-port` committed; keine Pushes, Merges, Rebases oder Phase 5b.
