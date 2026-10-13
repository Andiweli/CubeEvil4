# Phase 4c – Eventmodelle, pl00, Motion und Grafik-Bootstrap

Stand: 10. Oktober 2026. Grundlage: `f3680b89` auf `android-port`.
Dies ist eine Fortsetzung von [Modell/TexRender](phase4c-model-status.md), keine
vollständige Phase 5. Alle nachfolgend als ausgeführt bezeichneten Funktionen
wurden tatsächlich getestet. Unvollständige Subsysteme liefern keine Erfolgs-Stubs.

## Ergebnis und Reichweite

Alle **37 r120-BIN-Vorkommen** lassen sich jetzt mit ihren originalen TPLs
initialisieren. Die bisher fehlenden beiden Bindungen gehören zu
`event/model/ev0000/ev000e.bin` in `Evd/r120s00.evd` und `Evd/r120s01.evd`:
Beide referenzieren `em/pl00/pl000a.tpl`. Die Ressource liegt im originalen
`em/pl00.drs` und wird anhand der tatsächlichen Event-Registrierung gebunden.
Es gibt keine Ersatztextur, Ersatzgeometrie oder Änderung an den Daten.

Originale Event-Datentabellen, BIN-/EVD-Registrierung, Modelltabellen und
`ExePacket_SetPartsSub` laufen über native, lebenszeitgesicherte Ansichten.
Die Körpermodelle werden noch durch einen klar abgegrenzten CPU-Bindungsadapter
bereitgestellt; **`ExePacket_SetOm`/`SetObj18` und vollständige Eventwiedergabe
sind nicht implementiert**. Die Registrierung belegt keine fertigen Actorobjekte.

Originale Motion-Initialisierung und Kurvenauswertung laufen für die benötigten
Event-Motions und alle 66 FCV-Einträge des pl00-Archivs. Der Android-Bootstrap
führt echte Vulkan-Clears mit Farb-/Tiefenrücklesen und Surface-Präsentation aus.
`GameTask` erreicht `cMes.gameInit`; das separat gestartete `R120Event` erreicht
mit diesem Präsentationsadapter die fehlende Sofdec-Initialisierung.
Der vollständige Spielstart und ein spielbarer Raum sind weiterhin offen.

## Git, Quellen und private Daten

- Arbeitsbranch ausschließlich `android-port`; Ausgangsarbeitsbaum war sauber.
- `master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`.
- Lokal vorliegendes `origin/master` bleibt
  `15cf64f80e08bbc64712c39be330b75d1512a0d0`. Die README-Ergänzung über G4BE08,
  zwei Debug-Discs und 114 RELs ist berücksichtigt; kein Merge/Rebase/Push.
- Alle Projektänderungen liegen unter `port/`. Originalcode, Originalkonfiguration,
  die beiden GCM-Images und Importblobs bleiben unverändert. Keine Assets in Git/APK.
- Die verbindlichen Ressourcen stammen aus `RE4-Data/import-phase3a`. Discs werden
  über das vorhandene Inventar unterschieden; Hashes werden vor und nach Transfer
  sowie nach nativer Verwendung geprüft. Andere Sprachinhalte werden nicht ersetzt.

`em/pl00.drs` ist auf beiden Discs byteidentisch:
`5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090`,
2.215.392 Bytes. Der Archivkörper hat SHA-256
`7171022d741e11d8d06f1cbe522ca33cbe07e897379d336343e1b2420bc3e583`.
Die 133 Originaleinträge enthalten 25 BIN, 15 TPL, 66 FCV, 23 SEQ, 1 EFF und
3 leere Einträge. Das ist gemessener Bestand, keine neue Formatannahme.

## Neu ausführbare Originalfunktionen

Die Generatoren prüfen Originalfingerprints und extrahieren vollständige Körper.
Die Buildmanifeste führen Quellzeilen und Ersetzungen pro Funktion auf.
Consumer: 87 Körper; Runtime: 41; TexRender: weiterhin 7.

| Originalfunktion | Nachgewiesene Quelle / native Anpassung |
| --- | --- |
| `DatTbl::SetDat/GetDat/ChkDat/DelDat/DelDatWkNo` | `src/game/event.cpp:3143`, `:3184`, `:3218`, `:3338`, `:3313`; native Tabellenfelder, Überlauf-/Indexschutz |
| `EventMgr::SetBin/GetBin/SetEvd` | `src/game/event.cpp:2641`, `:2655`, `:2698`; gültige Lease-/Paketreferenzen statt GC-Adressbereich, begrenzte EVD-Ansicht |
| `Event::SetMod/GetMod` | `src/game/event.cpp:1966`, `:1984`; echte native Pointer und aktueller originaler Debugflag, kein PPC-Registeranker |
| `Event::ExePacket_SetPartsSub` | `src/game/event.cpp:960`; Originalfolge GetBin → GetMod → optionale Teileoffsets → ModInfoMgr.create → addModel → SetMod(type 3) |
| `MotionClear`, `MotionSetCore` | `src/game/motion.cpp:91`, `:134`; native FCV-Referenzen statt Inplace-u32-Pointerrelokation |
| `IKInit` | `src/game/ik.cpp:22`; echte Modellteile, Bindematrizen und ursprüngliche IK-Flags/Längen |
| `HermiteInterpolation`, zehn `Fcc_get_data_*`, `Fcc_next_axis_addr`, `dummy` | `src/game/motion.cpp:1239` und Folgefunktionen; explizite BE-/unaligned-Reads |
| `SetOrientationZY`, `VecRadLimit`, `hermite` | `src/game/math_sub.cpp`; unveränderte Rechenausdrücke |
| SDK `C_MTXInverse/Transpose`, `C_VECSubtract/Normalize/CrossProduct/Mag/SquareMag` | `src/lib/mtx.c`, `src/lib/vec.c`; portable Original-C-Funktionen statt PPC-Paired-Single |
| `GXSetCopyClear` | `src/lib/GXFrameBuf.c:308`; vollständige originale BP-Registerpackung, tatsächlich gespeicherter Zustand |
| `systemVISetBlack` | `src/game/main_sub.cpp:221`; ursprüngliche SYS_SET_BLACK-Flagsemantik mit nativer VI-Schwarzschaltung |

`dummy` stammt aus dem ursprünglichen FCC-Dispatch, ist kein neu erfundener
Erfolgsersatz. Nicht unterstützte FCC-Formate werden vor Ausführung abgelehnt.
Event-Diagnosen sind begrenzte, nicht werfende Meldungen, damit die ursprünglichen
GetBin-/GetMod-/SetPartsSub-Fehlerreturnpfade erhalten bleiben. Fehlende operative
Consumer und ungültige Speicherzugriffe bleiben ausdrücklich fehlschlagend.

## pl00-Referenzen und Eventmodelle

Die sechs Bindungen werden aus den tatsächlichen Statements in
`Event::ExeBeginEvt`, `src/game/event.cpp:1487`, generiert. Der vollständige
BeginEvt-Körper wird **nicht** als ausgeführt behauptet: Szenariostart, Font,
Sound und CORE_DUMMY-Bindungen bleiben seine offenen Abhängigkeiten.
PL_ARC_PTR greift ab Wort 4 auf die Original-Offsettabelle zu; der generische
Archivindex ersetzt diesen Zugriff durch geprüfte native Spans.

| Originalname | PL_ARC-Wort | Body-Eintrag |
| --- | ---: | ---: |
| `em/pl00/pl000a.bin` | 4 | 0 |
| `em/pl00/pl000a.tpl` | 5 | 1 |
| `em/pl00/pl000d.bin` | 9 | 5 |
| `em/pl00/pl000b.tpl` | 7 | 3 |
| `em/pl00/pl000e.bin` | 0xA | 6 |
| `em/pl00/pl000l.bin` | 0x10 | 12 |

Der originale Basis-BIN hat 119 Teile und 1.484 Vertices; sein TPL hat acht
Deskriptoren. Die beiden ev000e-Vorkommen haben je 113 Teile / 797 Vertices und
verwenden dasselbe originale TPL. `pl000d.bin` enthält 33 sparse Teile mit
globalen Leon-Teilenummern; `pl000l.bin` enthält einen Teil. Diese Anhänge werden
über den echten SetPartsSub-Körper dem Elternmodell hinzugefügt. Ihre Indizes
werden nicht umnummeriert und sie werden nicht als eigenständige Skelette behandelt.

| Paket | BIN-Vorkommen | Mit echten BIN/TPL initialisiert | Registrierte Körper / Anhänge | Event-Motion-Initialisierungen |
| --- | ---: | ---: | ---: | ---: |
| `Evd/r120s00.evd` | 18 | 18 | 12 / 17 | 26 |
| `Evd/r120s01.evd` | 19 | 19 | 13 / 17 | 38 |
| Gesamt r120 | 37 | 37 | 25 / 34 | 64 |

Die Registrierungszahlen zählen originale Modellnamen, nicht BIN-Vorkommen.
TPLs, Body/Attachment-Referenzen, Parentindizes, Original-ModTbl-Slotreihenfolge,
Typen, Matrizen und Boundingboxen werden unabhängig mit Importdaten verglichen.
Zusätzlich laufen dieselben Consumer für die inventarbasiert ausgewählten
kleinsten weiteren Modellpakete beider Discs: `Evd/r100s00.evd` und
`Evd/r317s15.evd` (sechs weitere BIN/TPL-Paare). Keine neuen Raumcallbacks.

DatTbl erhält erste Bindung, Referenzzählung und temporäre Freigabesemantik.
BIN-/TPL-/Motion-Bytes bleiben über Leases gültig, bis nach Taskquieszenz Tabellen,
Modelle und Modellinfos freigegeben sind. EVD-Registrierung rollt bei Fehlern
zurück. Aktive Modelltabellen verhindern verfrühte Einzelfreigabe. Raumretirement,
Abbruchcallbacks und alte Handles werden mit der vorhandenen Engine überprüft.

## Motion: ausgeführt und offen

`MotionAsset` prüft das tatsächliche FCV-Headerlayout, BE-Kinds/Teilnummern,
Offsettabelle, Größenwort, alle Achsbereiche, Keyreihenfolge, endliche Werte und
IK-/Modellreferenzen. Eigene native Arrays ersetzen die ursprüngliche Veränderung
des Originalfiles durch 32-Bit-Pointer. `MotionClear` und `MotionSetCore` arbeiten
anschließend auf echten Modellteilen und originalen Motionzuständen.

Alle 66 pl00-FCVs werden auf dem originalen, durch EVD referenzierten Leonmodell
initialisiert. **Eintrag 23 hat null Joints**: der originale Initialisierungskörper
läuft, es werden dafür keine Kurvensamples erfunden. Bei den 65 nichtleeren
Einträgen werden sämtliche Joints an Anfang/Mitte/Ende mit Original-Hermite
ausgewertet; die drei Achsen des ersten Joints werden zusätzlich unabhängig
mit `tools/motion/fcv.py` und der Hermiteformel verglichen. Das gleiche Verfahren
prüft die tatsächlichen Event-Motionreferenzen. Wiederholte Initialisierung,
Freigabe sowie beschädigte Offsets/Jointreferenzen werden geprüft.

**Offen:** die 23 originalen SEQ-Dateien sind inventarisiert und erhalten, aber
noch nicht als native SEQUENCE_DATA gebunden; `MotionMove/MotionMoveCore`,
Blend-/Flip-Poseausführung und angehängte Kameras laufen noch nicht. Die Prüfung
belegt Motionzustände/Kurven, keine vollständige Animation. FCC15 mit mehreren
Keys wird abgelehnt, weil der ursprüngliche Reader keine benötigten Tangenten
liefert. Vollständiges `ReadPlayerData`, `PlayerInit` und die pl00-Gameplaymodul-
Initialisierung sind ebenfalls noch offen. Die Datenanforderung nutzt dieselbe
original belegte Flagkombination 0x8100 wie `ReadPlayerData` (`read.cpp:578`);
deren vollständiger Kontrollfluss ist damit nicht portiert.

## Echter Android-Grafik-Bootstrap

`GXSetCopyClear` **setzt Zustand**; der Aufruf allein löscht noch kein EFB.
Der native Zustand speichert die originalen BP-Wörter 0x4F/0x50/0x51 mit RGBA
und 24-Bit-Tiefe. Das nachfolgende diagnostische Present führt GPU-Befehle aus.

Der Android-Adapter lädt Vulkan global/instanz-/geräteabhängig korrekt, erzeugt
eine echte Android-Surface, Graphics/Present-Queue und FIFO-Swapchain. Er besitzt
separate RGBA8-UNORM- und D32-Float-EFB-Bilder mit 512×448 Pixeln, entsprechend
`Render_init` → `ScreenReSize(512,448)` in `src/game/main_sub.cpp:123`.
Tiefe ist die normierte originale 24-Bit-Cleartiefe. Der Swapchain-Clear zeigt
bei VI-black Schwarz, sonst die Clearfarbe; Ausgabealpha ist immer 1.

Layoutübergänge, Acquire-/Submit-/Present-Semaphoren, Fence, Image-to-Buffer-Copy
und Host-Read-Barrier werden tatsächlich ausgeführt. Farbbytes müssen exakt und
die gelesene Tiefe mit Toleranz 1e-6 zum Clearzustand passen. OUT_OF_DATE beim
Acquire führt nach Devicequieszenz zur Swapchain-Neuerstellung. Fehler werden
gemeldet, ohne eine unterstützte GPU-Funktion vorzutäuschen.

Gemessen auf **Retroid Pocket Flip 2, dbdcacc, Android 13 / API33**, in echten
32- und 64-Bit-Appprozessen: **Adreno 650, Vulkan 1.1.128**, EFB 512×448,
Surface 1848×270 (Diagnoseansicht). Surface-Komposition ist INHERIT (8); mit
vollständig opaken Ausgabepixeln korrekt unterstützt. Abschließende Messwerte:

| APK-ABI | Originalruntime | GPU-Clears mit Rücklesen | erfolgreiche Presents |
| --- | ---: | ---: | ---: |
| armeabi-v7a | 634 Checks, 0 Fehler | 15 | 15 |
| arm64-v8a | 634 Checks, 0 Fehler | 17 | 17 |

Die Zahl zusätzlicher Choreographer-Diagnoseframes hängt vom Displaytakt ab.
Der Unterschied 15/17 ist keine abweichende Gameplay-Taskreihenfolge. Originale
Kontrollfluss-/Modell-/Motiontraces stimmen überein. Die bestehenden Controller-
und Frameadapter behalten ihre Trennung von Displaytakt und 30-Hz-Gameplay.

**Grenze:** Das ist ein synchroner Clear-/Readback-/Present-Bootstrap mit
QueueWaitIdle pro Diagnoseframe, noch kein produktiver Renderer. GX-Primitives,
Material-/Displaylistausgabe, XFB/GXCopyDisp, TexRender-EFB-Kopie, Movie-Drawing,
Pixel-/Depthformatkonversionen und performante Frames-in-flight fehlen. Es gibt
keine GPU-Validierungslayer-/Sanitizer-Behauptung. Der normale Controller-
GameDomain ist noch nicht automatisch mit dieser Diagnose-Surface verbunden;
der Originalruntime-GPU-Test bindet den echten Presenter ausdrücklich.

MSAA-/AF-/DoF-Menü und Persistenz bleiben verbindlich, siehe
[graphics-settings.md](graphics-settings.md). Aktiv sind **1 Sample, AF1,
DoF aus**. Globale Samplemaske 7 und AF-Maximum16 sind nur Hardwarebeobachtungen;
formatspezifische 2×/4×-Renderziele, Resolvepfad und reale Sampler sind noch nicht
validiert. Das Backend meldet keine unimplementierte Option als aktiv.

## Tatsächlicher Originalstart und nächste Blocker

| Pfad | Erreicht | Genau nächste fehlende Abhängigkeit |
| --- | --- | --- |
| `GameTask` / `gameInit` | original gameDebug, ClothInit, GXSetCopyClear; RGBA=0, Tiefe=0xFFFFFF | `cMes.gameInit()` → `MessageControl::gameInit`, `src/game/mes.cpp:406`: CORE_MESSAGE/CORE_MESSAGE_3, Font/common_p.fnt, Message-/Fontzustand |
| separater `R120Init` mit echten Paketressourcen | beide originale Callbackregistrierungen, SceExec, vollständiger CPU-TexRenderInit; normale Rückkehr | der gestartete Openingtask hat die nachfolgenden Abhängigkeiten |
| `R120Event`, Android-GPU-Test | erster SceSleep, originale Fortsetzung, SCF-Operation, original systemVISetBlack(1) / SYS_SET_BLACK | `Sofdec.Initialize("movie/opening.sfd",0)`; `src/game/sofdec.cpp:534` → initSub/ThreadMove/Decoder-/Audio-/Drawpfad |
| `R120Event`, Headless-Host/CLI | erster SceSleep und Fortsetzung | explizit fehlender VI-Presenter; keine simulierte Präsentation |
| Event-Consumer mit Original-EVD | SetEvd/SetBin, SetMod/GetMod, SetPartsSub, Motion-Initialisierung, Cancel-/Retirementpfade | vollständige ExePacket_SetOm/SetObj18, ExeBeginEvt, Event::Run/Packet-Timing, Kameras/Licht/Cloth |

Der GPU-Originaltask-Test verwendet die vorhandenen isolierten Runtimefixtures;
er behauptet keine EVD-Ladung durch den noch vor Sofdec blockierten Openingtask.
Die Modell-/EVD-/pl00-Nachweise laufen getrennt durch den echten EngineLoader mit
Originaldaten. Es werden weder Moviecancel noch Erfolgsflags injiziert, um den
Spielstart vorzutäuschen. Rno0 wird nicht künstlich zum nächsten Zustand gesetzt.
Die 35 unvollständigen englischen Raum-MDT-Blöcke bleiben unverändert offen.

## Abschließende Tests

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| Build | MSVC/Ninja erfolgreich | MSVC/Ninja erfolgreich | NDK/Clang + APK erfolgreich | NDK/Clang + APK erfolgreich |
| Host CTest | 25/25 | 25/25 | nicht Host | nicht Host |
| gemeinsame Runtime | 634/0 Fehler | 634/0 Fehler | 634/0 Fehler | 634/0 Fehler |
| reale EVD/Modelle/Motions/Räume | 2 Durchläufe, bestanden | 2 Durchläufe, bestanden | 2 Durchläufe, bestanden | 2 Durchläufe, bestanden |
| Module/RELs | 216 Checks; 114 RELs ×2 | identisch | identisch | identisch |
| MRAM/ARAM | 63 Adapterchecks; 308 Ressourcen und 142 Räume ×2 | identisch | identisch | identisch |
| APK/Controller/Frame/GPU | nicht Android | nicht Android | 20/20 | 20/20 |
| separate Kern-/Scheduler-/Yz2-Tests | in CTest | in CTest | 13.386/0 ×2 | 13.387/0 ×2 |

Der eine zusätzliche ARM64-Kerncheck ist der bestehende Test gegen Trunkierung
eines Pointers oberhalb 4 GiB. Die sieben relevanten Scheduler-/Background-/Yz2-
Traces sind über beide ABIs und Wiederholungen identisch. Die Controller-APK-
Regression prüft JNI/Lifecycle/Original-PadRead; **neue physische Tastendrücke
wurden in dieser Sitzung nicht angefordert oder als gemessen behauptet**.
Die vorherigen physischen Retroid-Nachweise bleiben erhalten.

Pro ABI erfasst der reale Consumerlauf 20 Raumvorbereitungen, vier Opening-
Paketverwendungen, 180 überprüfte Standalone-modelInit-Aufrufe, 76 SetPartsSub-
Aufrufe, 132 Event-/Zusatzpaket-MotionSetCore-Aufrufe und 264 pl00-MotionSetCore-
Aufrufe. Die 180 sind die expliziten Zwei-Runden-Modellprüfungen, nicht die
zusätzlich beim Registrierungsdurchlauf erstellten Körpermodelle.
`summarize_consumers.py` bestätigt identische semantische Ergebnisse über alle
vier ABIs und Wiederholungen. Eventtrace-SHA:
`de94465e844abffb70083d90788970503074d63cf2c39b3a1457fe5069ed6d41`.

### Fehlgeschlagene Zwischenprüfungen und Korrekturen

- Clang-Werror in extrahierten Originalkörpern wegen unbenutzter Parameter/
  Variablen: nur für die generierte Consumerdatei gezielt deaktiviert; native
  Adapter behalten die strikten Warnungsregeln.
- Surface zunächst nicht bereit: Displayzustand/Fehlermeldung diagnostiziert,
  Testscript weckt das Display, Activity hält es sichtbar. Vulkan `vkCreateInstance`
  wurde fälschlich erneut als Instanzfunktion geladen: globale, Instanz- und
  Gerätefunktionen getrennt. Keine gefälschte Surfacebereitschaft.
- Gerät unterstützt keine OPAQUE-Komposition für diese Surface: INHERIT ist
  tatsächlich unterstützt; Ausgabealpha1 gewährleistet opake Farbausgabe.
- Die zusätzliche pl00-Kurvenprüfung traf den echten null-Joint-FCV-Eintrag:
  Originalinitialisierung erhalten, unzulässiges Sampling entfernt.
- Ein Ressourcen-Gerätelauf scheiterte beim Blobtransfer am ADB-Server. Derselbe
  vollständige Lauf wurde anschließend separat erfolgreich wiederholt.

### Reproduktion und Belege

Host x86/x64 aus entsprechend initialisiertem VS-Developer-Prompt:

```powershell
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-x86 --output-on-failure
.\port\scripts\build-android.ps1 -SdkRoot "$env:LOCALAPPDATA\Android\Sdk" -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python 'C:\path\to\python.exe' -Offline
.\port\scripts\test-device.ps1 -Adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" -Serial dbdcacc
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-event-host --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-event-host --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-event-device --adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" --serial dbdcacc --repeat 2
python port/tools/summarize_consumers.py --host E:/Development/Android/RE4-Data/phase4c-event-host --device E:/Development/Android/RE4-Data/phase4c-event-device --output E:/Development/Android/RE4-Data/phase4c-event
```

`test_modules.py` / `test_resources.py` verwenden dieselben data/output/repeat-
und Host-/ADB-Argumente, mit `re4_module_tool.exe` / `re4_resource_tool.exe`.
Private Evidenz liegt in `RE4-Data/phase4c-event{,-host,-device,-modules,
-modules-device,-resources,-resources-device}`. Dort sind alle Originalhashes,
ABIbreiten, Slot-/Motionzustände und Testexecutable-Hashes gespeichert.
Lokale ignorierte Logs: `port/build/event-*.log`, GPU-Berichte
`graphics-armeabi-v7a.json` / `graphics-arm64-v8a.json`, Instrumentation- und
Kern-Testlogs pro ABI. Die GPU-Berichte werden nach jeder ABI-Ausführung gelesen,
bevor eine Installation sie ersetzen kann.
Abschließender Debug-APK-SHA-256:
`e5101e1b63e377fb5783e956fbc76acd228fdf096a2329f5f7ae67e7ea8f4749`.

## Vollständig gespeicherte Projektdateien

Alle folgenden Dateien liegen vollständig im Projekt; keine bloßen Diffs und
keine Originalassets:

```text
port/CMakeLists.txt
port/README.md
port/android/app/src/androidTest/java/org/re4/port/GameRuntimeTest.java
port/android/app/src/main/java/org/re4/port/FoundationActivity.java
port/android/app/src/main/java/org/re4/port/GraphicsBootstrapView.java (neu)
port/config/consumer-sources.json
port/docs/graphics-settings.md
port/docs/phase4c-event-status.md (neu)
port/include/re4/port/consumers.h
port/include/re4/port/event_resources.h (neu)
port/include/re4/port/game_runtime.h
port/include/re4/port/graphics.h (neu)
port/include/re4/port/model_types.h
port/include/re4/port/motion.h (neu)
port/platform/android/graphics_jni.cpp (neu)
port/platform/android/vulkan_bootstrap.cpp (neu)
port/platform/android/vulkan_bootstrap.h (neu)
port/scripts/test-app-device.ps1
port/src/consumer_original.h
port/src/consumers.cpp
port/src/event_resources.cpp (neu)
port/src/game_runtime.cpp
port/src/graphics.cpp (neu)
port/src/model_original.h
port/src/model_runtime.cpp
port/src/motion.cpp (neu)
port/src/motion_original.h (neu)
port/src/runtime_original.h
port/tests/consumer_verification.h
port/tests/game_runtime_tool.cpp
port/tests/game_runtime_verification.cpp
port/tests/model_verification.h
port/tests/test_consumer_sources.py
port/tools/generate_consumers.py
port/tools/generate_runtime.py
port/tools/summarize_consumers.py
port/tools/test_game_runtime.py
```

## Nächste begrenzte Phase-4c-Arbeit

**Nachgewiesene Blocker:** MessageControl gameInit/Core-/Fontbesitz, Sofdec-
Video-/Audio-/Drawpfad, echte Obj18-/SetOm-Erstellung, vollständige Eventbegin-/
Run-End-Semantik, SEQ und MotionMove, Kamera-/Licht-/Cloth-Verbraucher sowie
TexRender-/GXCopyDisp-/Geometrieausgabe. Der komplette ExePacket_SetParts-Wrapper
mit Kostümwahl ist ebenfalls noch nicht gebunden; die Messung verwendet die
unveränderten Paketreferenzen und den originalen SetPartsSub-Körper.
Produktionspoolgrößen müssen weiterhin
aus Original-CONS/SMD/RoomInit hergeleitet werden; die Consumerprüfung konfiguriert
ausdrücklich ausreichend große Testpools und behauptet keinen Produktionspool.

**Technische Annahme, noch zu verifizieren:** Die native Lease-/Managerstruktur
trägt die nächsten Font-/Obj18-/MotionMove-Consumer. Deren reale Layouts, virtuelle
Schnittstellen und Freigabe-/Callbacks müssen erneut untersucht werden; der
erfolgreiche CPU-BIN-Nachweis reicht dafür nicht aus.

**Konkreter nächster Schritt:** MessageControl::gameInit samt echten CORE_MESSAGE-
und Font/common_p.fnt-Abhängigkeiten anbinden und den originalen Start erneut bis
zum nächsten realen Consumer ausführen. Für vollständige Eventmodelle danach
SetObj18/ExePacket_SetOm einschließlich originaler Cloth-/Lichtinitialisierung
portieren; kein Überspringen über simulierte Actorflags. Sofdec muss seine echte
Task-/Cancel-/Mediensemantik erhalten. Ein vollständiger Renderer oder Phase 5
beginnt erst nach gesondertem Auftrag.
