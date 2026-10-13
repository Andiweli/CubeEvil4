# Phase 5a: vollständige registrierte Leon-Hierarchie und originale Materialpfade

Fortsetzung: [r100, gruppierte Original-SMD-Erstellung und nachgewiesene
Spieler-/Licht-/Kameraabhängigkeiten](phase5a-r100-status.md). Der unten
beschriebene gruppierte SMD-Abbruch ist dort behoben; die übrigen Grenzen
bleiben als historischer Stand dieses Berichts erhalten.

Geprüfte Fortsetzung vom 11. Oktober 2026 auf `android-port`, ausgehend von
`af1cafa33dd35adaa49d962ffef478c73a77a061`. Windows x86/x64 und Retroid
Pocket Flip 2 `dbdcacc`, Android 13/API 33, Adreno 650, Vulkan 1.1.128.

## Ergebnis und Abnahmegrenze

Leons originale registrierte Körper-/Anbauhierarchie wird über die gemeinsame
GX-/Vulkan-Pipeline ausgegeben. Der bisherige Abbruch an Materialflag 3 ist
beseitigt. Vollständige Originalbodies erzeugen die Specular-/Bump- und übrigen
Materialzustände; skalare GX-Adapter und Vulkan-Shader verarbeiten sie.
Es gibt keine Leon-, Raum- oder Material-ID-Sondershader.

**Nachgewiesen ist ein isolierter Draw, kein vollständiges Spiel oder
vollständig wiedergegebenes Intro.** Der gewählte Cut wird aus neutralem
Modellzustand ausgewertet. Vorherige Cuts, aktive Morphs, Raum-/Eventcallbacks,
Sichtbarkeitsänderungen und sämtliche anderen Eventobjekte werden nicht als
vollständige Szene abgespielt. Eine Pose aus originalen FCV-Daten ist deshalb
nicht automatisch der identische vollständige GameCube-Eventframe.

Die GPU-Bilder zeigen Leons Körper, Kopf und registrierte Anbauten. Sie sind
deutlich dunkel. Eine passende GameCube-Aufnahme mit identischem Modell, Cut,
Motionframe, Kamera und Eventzustand fehlt. Bytegleiche Readbacks beweisen
Wiederholbarkeit und ABI-Gleichheit, keine vollständige visuelle Originaltreue.
Phase 5a bleibt in dieser Hinsicht ein geprüfter Teilstand. Phase 5b wurde
nicht begonnen.

## Neu ausgeführter Originalcode

| Quelle | Tatsächlich verwendeter Umfang |
|---|---|
| `src/game/trans.cpp:1323`, `shaderSetup` | Vollständiger Body statt des bisher allein übernommenen finalen Scale-Segments; originale Material-, Specular-/Bump-, Alpha-, Stage-/Texgen- und Indirect-Reihenfolge. |
| `src/game/trans.cpp:1798`, `specularSetup` | Vollständiger Body; RGB-/Alpha-Inputs, Typen 0/1, normale oder externe Textur, Zerfall und sekundäre Materialfarbe. Nur `static` entfällt im generierten Übersetzungsmodul. |
| `src/game/trans.cpp:1898`, `specularSetup2` | Vollständiger alternativer mehrstufiger Warp-Pfad. |
| `src/game/trans.cpp:2334`, `bumpSetup` | Vollständiger Body einschließlich originaler Bump-Textur-/Indirect-Verknüpfung. |
| `src/game/trans.cpp:1993`, `GlobalIlluminationSetup` | Vollständiger Body. Der Zweig ist durch isolierte Zustandsprüfungen belegt; kein neuer separater GPU-Nachweis eines aktiven GI-Eventzustands. |
| `src/game/trans.cpp:2641`, `updateMatrices` | Vollständiger Body; Model-View, inverse Transponierte, originale 0,4-Skalierung und Indirect-Matrix. Nur der native Name der Kameramatrix wird angepasst. |
| `src/lib/GXBump.c:286`, `GXSetTevIndBumpXYZ` | Vollständiger SDK-Body mit dem geprüften nativen Indirect-Zustandsadapter. |
| `src/lib/GXBump.c:186`, `GXSetTevIndWarp` | Vollständiger SDK-Body für Signed-Offset und Replace/Warp. |

Die vorhandenen Material-/Alpha-/TextureBlend-, OT-, LightSet- und Lichttyp-
Bodies bleiben erhalten, ebenso Weight-Paletten, skalare PPC-Quantisierung,
FK-`MotionMove` und Gelenk-/Bindmatrizen. `generate_gx.py` prüft normalisierte
Quellhashes, darunter nun `GXBump.c`. `gx-render-manifest.json` im Build enthält
Bodies, Anpassungen und die weiterhin begrenzte `Render`-Dispatch-Slice.
`Render` und `Trans` werden nicht als vollständig integriert behauptet.
Originaldateien unter `src/`, `include/` und `tools/motion/host` bleiben unverändert.

## GX-Zustände und Plattformadapter

- TEX0-/Normalen-Texgen, 2×4-/3×4-Matrizen, ST und STQ. Rohe geskinte Normalen
  bleiben zusätzlich zu World-/View-Normalen erhalten. Die Specular-Matrix
  basiert wie `commonModelTrans` auf `m->mat`, nicht auf der ersten
  Gelenkmatrix, die für den abschließenden Weighted-Draw gebraucht wird.
- Indirect-Format 8, A/B/G-Kanäle, STU-Bias, Stage-Order/-Scale, Matrizen 1–3,
  Wrap OFF/0. Koeffizienten werden gemäß `GXBump.c` auf vorzeichenbehaftete
  11-Bit-Werte quantisiert; der Shader verarbeitet Offset und vorzeichenbehaftete
  24-Bit-Texturkoordinaten. Andere Formate, S-/T-Matrizen, AddPrevious,
  OriginalLOD, AlphaBump und andere Wrap-Modi werden ausdrücklich abgelehnt.
- Vorhandene TEV-ADD/SUB-, Bias-/Scale-, Register-/Konst-, Alpha-Test-, Blend-,
  Fog-, Depth- und Culling-Pfade bleiben erhalten. Fehlende TEV-Vergleichs-
  operationen, weitere Texgen-Quellen/Postmatrizen und sonstige GX-Formen
  lösen weiterhin Fehler aus.
- `ThermoShaderSetup`, `RefractShaderSetup`, `SelfShadowSetup` und
  `ShadowCastSetup` sind fehlende Verbraucher mit Exceptions, keine erfolgreichen
  Ersatzfunktionen. Der isolierte Capture-Kontext besitzt keine aktiven
  Runtime-Schatten-/Thermo-/Refraction-Zustände. Er ersetzt keinen echten
  GameTask-Renderkontext.
- Vulkan-Texelfilterung und LOD sind native GPU-Operationen; bitgenaues
  GameCube-Sampling oder ein PPC-GPU-Tracevergleich wurden nicht nachgewiesen.
  Die bestehenden Decoder unterstützen I4, I8, IA4, IA8, RGB565, RGB5A3,
  RGBA8, C4, C8, C14X2 und CMPR einschließlich der zuvor getesteten Paletten
  und Mips. Die neuen konkreten Orakel prüfen ihre tatsächlich verwendeten
  Modell-/Core-Formate und Hashes.

Zur Prüfung der Texgen-/Indirect-Arithmetik wurden zusätzlich die primären
Dolphin-Quellen
[UberShaderVertex.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/UberShaderVertex.cpp)
und
[UberShaderPixel.cpp](https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/UberShaderPixel.cpp)
herangezogen. Für RE4-Aufrufe und Strukturen bleiben die lokalen Originalquellen
maßgeblich. Kein separater Emulator-Renderer wurde übernommen.

### Originale Core-Texturen

`EngineLoader` lädt `etc/core.das`; geprüfte Entries werden direkt dekodiert.
Die Slots stammen aus `include/arc/core.h`, ihre Semantik aus `CoreDataRead`,
`SpecularInit` und `GlobalIlmTexInit`.

| Originalslot | Verwendung | Tatsächliche Anzahl |
|---|---|---:|
| `CORE_SPECULAR_0`, 4 | `Specular[]` | 5 |
| `CORE_SPECULAR_1`, 17 | Erster Teil `IndTex[]` | 1 |
| `CORE_SPECULAR_2`, 18 | Zweiter Teil `IndTex[]` | 1 |
| `CORE_GLOBAL_ILLUMINATION`, 16 | `GlobalIlmTex[]` | 1 |

Die Originalinitialisierung wurde bereits im Message/Core-Runtime-Scope
portiert. Der neue Rendering-Brückencode liefert Vulkan-Bindungen mit deren
Wrap-/Nicht-Mip-Semantik, kein originales GPU-`GXTexObj`-Speicheroverlay.
Die Indirect-Paletten werden ausdrücklich sequenziell angefügt. Das unabhängige
Orakel entdeckte zuvor eine durch unbestimmte C++-Auswertungsreihenfolge
vertauschte Reihenfolge; nach Korrektur bestanden alle finalen Hashprüfungen.

### Speicher und ABIs

`GxVertex` ist auf allen ABIs 48 Byte, `GxTevStage` 112 Byte und
`GxMaterialState` 3312 Byte. Statische Assertions prüfen die std140-Offsets;
Vulkan berücksichtigt `minUniformBufferOffsetAlignment`. Shaderzustände
transportieren keine nativen Pointer. Native Modelle und BE-Dateistrukturen
bleiben getrennt. `ResourceLease` hält BIN-/TPL-Speicher über die CPU-
Modellfreigabe hinaus bis zur Scene-/GPU-Freigabe.

## Modell- und GPU-Nachweise

Die universelle Auswahl übernimmt den registrierten Root und originale
`SetPartsSub`-Anbauten in der Reihenfolge der `pModelInfo`-Liste. Gemeinsame
OT-Callbacks sowie originale Part-/Texturindizes bleiben erhalten. Keine
manuell festgelegte Leon-Liste steuert den Renderer.

Beide Leon-Fälle verwenden **119 Gelenke und 70 erfasste und über OT ausgegebene
Draw-Gruppen**, darunter sämtliche 14 Körper-BIN-Gruppen. Original registriert:
`pl0000`, `ev0003`, `ev000e`, `ev0002`, `ev0001`, `pl000d`, `pl000l`;
sechs Weighted-BINs und ein statischer Anbau. Originale Motion und Rootbewegung
bleiben erhalten, im begrenzten FK-/Neutral-Morph-Scope des ausgewählten Cuts.

| Fall | Originalkamera / Frame | Zusätzliche Draws | Veränderte Pixel zur identischen Kamera ohne Modell |
|---|---|---:|---:|
| r120, `r120s00`, Cut 1, `pl0000` | `cam_s00_001.fcv`, 30 | 70/70 | 17.202 |
| r120, `r120s00`, Cut 1, `ev0703a` | `cam_s00_001.fcv`, 30 | 4/4, echter Flag-0x83-Warp | 89 |
| r119, `r119s10`, Cut 0, `pl0000` | `cam_s10_000.fcv`, 30 | 70/70 | 2.957 |
| r119, `r119s10`, Cut 0, `em2b00` + `em2b01` | `cam_s10_000.fcv`, 30 | 11/11, zwei Weighted-Hierarchien | 18.997 |

Je Fall: zwei Activity-Erstellungen pro ABI mit echten Vulkan-Frames,
Readback und Präsentation. Alle vier Readbacks desselben Falls sind bytegleich;
die originalen OT-/TEV-/Lichtzustände ebenfalls. Das unabhängige CPU-Orakel
prüft Archive, EVD-Registrierungen, Display-List-Primitive, Partheader,
TPL-Mips/Core-Hashes, Materialprogramme, Matrizen und Kamerakanäle.

Screenshots und Berichte bleiben ausschließlich privat unter
`E:\Development\Android\RE4-Data`:

- `phase5a-material-leon-device\gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`
- `phase5a-material-warp-device\gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`
- `phase5a-material-r119-leon-device\gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`
- `phase5a-material-em2b00-device\gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`

Die zugehörigen `.json` und `.rgba` enthalten Kamera, Materialprogramme,
Hash und Prozess-/Activity-Belege. r120-Leon-RGBA-SHA256:
`248718bbbef050b332cbe9f5d6c4d8cfc325a42f592e574d94a79c06a95acf75`.
`geometry_frames` zählt Geometrie-Präsentationen; der ältere Clear-Zähler
`presents` ist dafür nicht maßgeblich.

## Visuelle Grenzen und r100-Startposition

Der vom Nutzer gelieferte Screenshot zeigt den ersten spielbaren Waldabschnitt
**r100**. Er passt nicht zu obigen r120-/r119-Eventkameras. Die frühere pauschale
Zuordnung des aktuell angezeigten Retroid-Bilds zum Intro war falsch: Der
tatsächlich laufende zusätzliche Modelltest nutzte r119. Die Diagnose-Activity
zeigt nun Raum, Originalkamera, Frame und Modell direkt an. Unterschiede
zwischen Bildern verschiedener Räume sind kein Qualitätsbeweis.

Die Quellen unterscheiden zwei Positionen:

1. `src/st1/r120.cpp:99`: Wechsel nach r100 mit `(-109450, -515, 820)`,
   Rotation `(0, 0, 0)`.
2. `src/st1/r100.cpp:582`, `r100_StartEvent`: danach Spielerposition
   `(-99685, -484, -1343)`, Y-Winkel `2.246` (`:588`).

Die spielbare Schulterkamera ist keine beliebig auswählbare EVD-Kamera.
`CameraControl::roomInit` (`cam_ctrl.cpp:1017`) ruft `bindDefaultCamera`,
`CameraQuasiFPS::init`, `Check`/`Move` und `CameraMove` auf. Ihre Ausgabe hängt
von Spielerzustand/-matrizen, Raum-/Core-Kameradaten, Areas und Kollision ab.
`cPlayer::startUp` (`player.cpp:277`) benötigt echte `move`-/Motion-/SatMgr-
Ausführung. Keine nachgestellte Kamera oder verschobene Eventpose wurde verwendet.

**r100 wurde auf allen vier CPU-ABIs angefordert, aber nicht gerendert.**
Der direkte native `scene`-Aufruf stoppt vor der Kamera reproduzierbar bei
`SMD old version/grouped works need original group creation`. Der originale
r100-SMD besitzt Version `0x40`, Flag 1, 13 im Header deklarierte Top-Level-
Modelle und fünf Gruppenangaben `11, 99, 61, 52, 74`. Die Originalformel
`getWorkNum` ergibt 310; dies belegt keine 310 initialisierten Modelle.
Group-/Block-Besitzer und Datenlayout müssen über den Originalpfad verbunden
werden; ein flaches 72-Byte-Array ist hier nicht ausreichend.
`St1/r100.das` liegt laut FST auf Disc 1 (`disc_number=0`), nicht auf Disc 2.

Der Startpositions-Test bleibt ein konkreter offener Meilenstein: originale
Group-/Block-Scroll-Erstellung, `SmdSetParam` samt gemeinsamen BIN/TPL-/Motion-
Referenzen, originale Spieler-Startpose und Schulterkamera, anschließend echter
GPU-Nachweis und Vergleich mit derselben Originalreferenz. Der Materialauftrag
hat dafür keinen Spielstart künstlich fortgesetzt.

## Tatsächliche Tests

| Prüfung | Ergebnis |
|---|---|
| Windows x64 / x86 Build und CTest | Je 27/27 bestanden einschließlich GX, Sourceguards, Scheduler, Module, Ressourcen, Loader, Runtime. |
| Android Dual-ABI-Build mit Validation-Layers | CLI/JNI, Debug-/Test-APK und Lint erfolgreich. Bekannte CXX5304-SDK-XML-Warnung; kein Buildfehler. |
| Vier Szenenfälle oben | Host x86/x64 je zweimal; Retroid ARM32/ARM64 je zweimal. Daten-/Material-/Kameraorakel sowie identische Readbacks bestanden. |
| Skinning, beide Discs, vier ABIs, zwei Runden | Je ABI 18 Ausführungen, 244 Modellberechnungen, 1.739.820 skalare Prüfungen und 503 isolierte GX/SDK-Adapterprüfungen. Quantisierte Positionen/Normalen gleich; ARM32-/ARM64-Floatdelta 0. |
| GameRuntime, beide Discs, vier ABIs, zwei Runden | Originale Player-, SubScreen-/Inventar-, Message-/Core-, Save-/ID-, Licht-, Modell-/Motion-, Callback- und Loaderprüfungen bestanden; 37 r120-BIN-Vorkommen bleiben geprüft. |
| Ressourcen, beide Discs, vier ABIs, zwei Runden | 157/151 Ressourcen und 85/57 Räume (Disc 1/2), native Speicher-/Transfer-/Freigabeprüfungen bestanden. Keine vollständige Darstellung dieser Räume behauptet. |
| Module, beide Discs, vier ABIs, zwei Runden | 110/4 RELs (Disc 1/2), geprüfte partielle native Registrierung/Init bestanden; kein vollständiger REL-/Raumcode-Replay behauptet. |
| Retroid Core/Scheduler, zwei Runden je ABI | ARM32 je 13.386, ARM64 je 13.387 Prüfungen, 0 Fehler; identische Scheduler-/Background-/Yz2-Traces. Die zusätzliche Prüfung betrifft den breiteren Pointer-Scope, keine fehlende ARM32-Gameplayprüfung. |
| Finale APK-Prozesse ARM32 und ARM64 | Je 20/20 Instrumentationstests bestanden: OriginalCode, GameRuntime, ModuleCode, LoaderFrame einschließlich zentraler Eingabe-/Vulkanprüfungen. |
| Direkter r100-Probeaufruf, alle vier ABIs | Exit 1 am dokumentierten Original-Group-Verbraucher; kein GPU-Startbild. |

Skinning-Maxima gegen das unabhängige Double-Orakel bleiben: Weight-Linear
`1,19e-7`, Weight-Translation `0,011290427`, FCV-Position `0,004801790`,
Rotation `1,58e-6`, Scale `2,4e-8`. Keine PPC-Bitgleichheit behauptet.
Specular-Texgen: `rtol=3e-6`, `atol=2e-6`; Indirect-Koeffizienten höchstens
eine Quantisierungseinheit Differenz durch Float-Auswertung.

Khronos-Validation 1.3.290 ist auf dem Gerät aktiv: **0 Fehler**, je Surface
die bekannte Warnung `SwapchainPreTransform` (IDENTITY statt ROTATE90).
Geprüft wurden Ressourcen/Descriptors, Fence-/Surface-Abbau und erneute
Activity-Erstellung. `last_frame_cpu_us` umfasst Aufzeichnung, Fence, Readback
und Hashprüfung, keine reine GPU-Zeit oder Gameplay-FPS. Der r120-Leon-Nachweis
liegt in der Größenordnung von 50 ms pro Diagnoseframe; kein Performanceziel
für den späteren Renderer wird daraus abgeleitet.

### Fehlgeschlagene bzw. begrenzte zusätzliche Fälle

- `em2100`, r119 Cut 0: echte Initialisierungs-/Bind-Pose und Flag-2-Material
  bestehen auf Host x86/x64. In diesem Cut kein Mot-Paket; keine Animation
  erfunden. Original-OT nimmt das Modell bei dieser Kamera nicht auf. ARM32-
  GPU-Test scheitert deshalb am Sichtbarkeitsnachweis; keine ARM64-GPU-
  Erfolgsaussage für diesen Fall und keine zur Umgehung versetzte Pose.
- `em2100`, r119 Cut 1: Abbruch an fehlender originaler modellgebundener
  Lichtregistrierung. Keine Freischaltung durch Ersatzlichter.
- r100 mit `wep0200`: Orakel außerhalb seines statischen lokalen SMD-Scope.
  Der direkte native Probeaufruf bestätigt den Group-Blocker auf allen ABIs.
- Aktive Shape-Pakete im gewählten Cut werden abgelehnt, etwa `ev0001` in
  r120 Cut 0. Der neutral begonnene spätere Cut belegt keine noch fehlende
  Morph-Zustandsübernahme aus früheren Cuts.

## Reproduktion

Host-Builds wie in `port/README.md`: hier `port/build/host-ninja` (x64),
`port/build/host-x86` (x86), CTest jeweils mit `--output-on-failure`.
Android-Build: `port/scripts/build-android.ps1` mit SDK-/JBR-/Python-Pfaden,
`-Offline` und `-ValidationLayers` auf
`port/build/validation/layers/android-binaries-1.3.290.0`.

`port/tools/test_gx_scene.py` benötigt `--data <import-phase3a>`, separaten
privaten `--output`, `--repeat 2` sowie `--host-exe <re4_gx_tool.exe>` oder
`--adb <adb.exe> --serial dbdcacc`. Szenenargumente:

```text
--room 120 --event Evd/r120s00.evd --cut 1 --frame 30 --model pl0000
--room 120 --event Evd/r120s00.evd --cut 1 --frame 30 --model ev0703a
--room 119 --event Evd/r119s10.evd --cut 0 --frame 30 --model pl0000
--room 119 --event Evd/r119s10.evd --cut 0 --frame 30 --model em2b00
```

Zuerst dieselben Argumente ohne `--model` für den Kamera-Baseline-Output
ausführen, danach `--baseline-output <Baseline-Verzeichnis>` beim Modelltest
angeben. Ungleiche Räume, Kameras, Frames und Matrizen werden abgelehnt.
Kein Vergleich unterschiedlicher Raum- oder Kamerabilder als Qualitätsbeweis.

Regressionssuiten `test_skinning.py`, `test_game_runtime.py`, `test_resources.py`,
`test_modules.py` ebenfalls mit `--data`, privatem `--output`, `--repeat 2`,
jeweiliger Host-EXE oder ADB/Serial ausführen. Zusätzlich:

```powershell
.\port\scripts\test-device.ps1 -Adb <adb.exe> -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb <adb.exe> -Serial dbdcacc
```

Der r100-Blocker ist direkt reproduzierbar:

```text
re4_gx_tool scene <privater Root> <Katalog mit St1/r100.das> 0 100 Evd/r100s03.evd 0
```

Erwarteter Exit 1: `SMD old version/grouped works need original group creation`.
Die EVD-Kamera wird vorher nicht ausgewertet; dies ist kein r100-Startkameraframe.
`test_gx_scene.py --room 100 --event Evd/r100s03.evd` erstellt vor seinem
Scope-Abbruch einen separaten hashgeprüften Katalog für diesen Probeaufruf.

Logs: `port/build/material-*.log`. Private Rohberichte:
`E:\Development\Android\RE4-Data\phase5a-material-*` mit Suffix
`host64`, `host32` bzw. `device`, insbesondere `skin`, `game`, `resource`,
`module`, `leon`, `warp`, `r119` und `em2b00`.

## Vollständig gespeicherte geänderte Dateien

- `port/config/gx-sources.json`
- `port/include/re4/port/{event_skinning,gx_pipeline,gx_scene,model_types}.h`
- `port/src/{event_skinning,gx_pipeline,gx_scene}.cpp`, `gx_render_original.h`
- `port/platform/android/vulkan_bootstrap.cpp`
- `port/shaders/{gx_diagnostic.vert,gx_diagnostic.frag,gx_state.glsl}`
- `port/tools/{generate_gx.py,test_gx_scene.py}`, `port/tests/test_gx_sources.py`
- `port/android/app/src/main/java/org/re4/port/GeometryActivity.java`
- `port/docs/phase5a-material-status.md`, Verweise in den beiden vorherigen
  Phase-5a-Berichten sowie `port/README.md`

Generierte Originalmodule, Builds, temporäre Probe-/Auswertungsskripte und Logs
bleiben im ignorierten `port/build`. Originaldaten und GPU-Bilder bleiben
außerhalb des Repositorys; weder Git noch APK enthalten GameCube-Assets.

## Verbleibende Grenzen und nächster Schritt

`GameTask` bleibt bei `MerchantGameInit()`, `R120Event` bei
`Sofdec.Initialize("movie/opening.sfd", 0)`. Renderingdiagnosen ändern keinen
dieser Gameplay-Zustände. Offen: vollständige `Trans`-/`Render`-Orchestrierung,
Eventobjekt-/Sichtbarkeitszustände, modellgebundene Lichter und SAR-Laufzeit,
Morphs, IK, Cloth, Hokan/sekundäre Motion, vollständige SEQ-Aktionen,
EFB-Refraction/Mirror, Schatten und Postprocessing.

Nach Nutzersteuerung ist **r100 an der originalen Startposition** die nächste
geeignete Referenz: zuerst Group-/Block- und Player-/Kamera-Verbraucher
verbinden, danach identische Pose/Kamera aufnehmen und die dunkle Beleuchtung
prüfen. Keine erfundene Schulterkamera, Light-Korrektur, Eventbeendigung oder
Ersatzpose als Zwischenschritt. Phase 5b bleibt außerhalb dieses Auftrags.

Controller und zentrale `PadRead`-Anbindung bleiben unverändert. Für diese
Materialphase werden keine neuen physischen Tastendrücke behauptet.
[graphics-settings.md](graphics-settings.md) bleibt verbindlich: MSAA
Aus/2×/4×, AF 2×/4×/8×, DoF mit Nah-/Zielfokus, persistente Einstellungen
und hardwarevalidierter effektiver Zustand. Aktuell: ein Sample, AF 1,
kein DoF. Andere Optionen werden nicht als aktiv angezeigt.

Lokaler `master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`; dessen
aktualisierte README wurde berücksichtigt. Kein Merge, Rebase oder Push.
