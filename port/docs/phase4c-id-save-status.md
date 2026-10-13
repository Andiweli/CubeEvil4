# Phase 4c: Cockpit-/ID-System und gemeinsame Save-Initialisierung

Fortsetzung am 10. Oktober 2026 auf `android-port`, Ausgangscommit
`d3d6624b2b0a463aa13b2bd3b0b3b84ebb1b0ef8`.

## Nachgewiesener Fortschritt

Der originale `GameTask` führt jetzt `Cckpt.gameInit()` und damit
`IDSystem::roomInit()` aus. Danach wird die originale Zuweisung
`ObjMgr.warnDiv = 100` ausgeführt. Der nächste tatsächliche Abbruch ist
`LightMgr.init(LightFuncTbl)` in `src/game/game.cpp:261`.
`Rno0` bleibt 0; es gibt keine künstliche Fortschaltung zum Raum oder Gameplay.

Die Bezeichnung ist entscheidend: `Cckpt` ist laut `include/cockpit.h` und
`src/game/cockpit.cpp` die globale **Cockpit-/HUD-Instanz**. Ihre
`gameInit()`-Funktion (`cockpit.cpp:21`) enthält ausschließlich
`IdSys.roomInit()`. Sie ist keine Checkpoint-Verwaltung.
Die Save-/Continue-Funktionen liegen separat in `cGameSave` und
`cRoomData`. Die hierfür unmittelbar gemeinsamen, überprüfbaren Grundlagen
wurden ebenfalls integriert; vollständiges Gameplay-Speichern wird nicht behauptet.

Tatsächliche Reihenfolge im neuen Originaldaten-Nachweis:

```text
Native Bootadapter mit belegten Original-Voraussetzungen:
  systemWorkInit
  IDSystem::gameInit(0x80) -> IDSystem::roomInit, Systemheap
  Heap-Tail-Handoff 0 -> 1
  cRoomData::init, originale Stage-/Room-Save-Tabellen
  cGameSave::alloc -> Größenfunktionen -> calcAddr
  cItemMgr::init
Original CoreDataRead -> MessageControl::init
Original GameTask -> gameDebug -> gameInit:
  ClothInit -> GXSetCopyClear -> MessageControl::gameInit
  SubScreenGameInit -> SubScreenAramRead -> SubScreenRoomInit
  Cockpit::gameInit -> IDSystem::roomInit
  ObjMgr.warnDiv = 100
  LightMgr.init(LightFuncTbl): ausdrücklich noch nicht gebunden
```

Der Bootadapter führt nicht den vollständigen `systemInit`, `systemRestartInit`
oder `Title_task` aus. Er bindet deren belegte Voraussetzungen in ihrer relativen
Reihenfolge: `main.cpp:303` allokiert den Haupt-ID-Pool vor dem Gameheap-Handoff;
`systemRestartInit` initialisiert die Room-Save-Daten im Gameheap;
`title.cpp:98` allokiert das Save-Image vor `ItemMgr.init()` und `CoreDataRead()`.
Audio, Sofdec, Titelmenü und weitere noch fehlende Manager werden dadurch nicht
als erfolgreich initialisiert ausgegeben.

## Originalfunktionen und klare Ausführungsgrenzen

`tools/generate_id_save.py` extrahiert 31 vollständige Originalfunktionskörper.
15 Quell-/Headerdateien sind durch SHA-256 in `config/id-save-sources.json`
gesichert. Die generierten Dateien liegen unter
`build/<build>/original/id_save_control.cpp`, `include/re4_id_save_types.h`
und `id-save-manifest.json`. Jede Signatur, Quellenzeile und Anpassung ist
im Manifest nachvollziehbar. Originaldateien in `src/` und `include/` wurden
nicht verändert.

Tatsächlich ausgeführt, im Startup oder ausdrücklich isoliert über die vorhandene
Originaldaten-Diagnose:

- `Cockpit::gameInit`.
- `IDSystem::gameInit`, `roomInit`, `free`, `dispSw`, `unitPull`, `unitPush`,
  `unitLevel`, `unitParent`, `kill`, `beMove`, `setTime`.
- `cRoomData::init`, `getSaveDataSize`, `checkRoomRange`, `getRoomSavePtr`,
  `save`, `load`, `clear`, `checkPassed`, `setPassed`.
- `cGameSave::alloc`, `calcOffset`, `calcAddr`, `checkAddr`.
- `SscrnDataSize`, `MerchantDataSize`, `cItemMgr::saveDataSize`.

`IDSystem::setCk`, `unitPtr` und `stop` sind zusätzlich aus dem Original
mitkompiliert, haben in diesem Meilenstein jedoch noch keinen eigenen
Laufzeitnachweis. Die Liste oben umfasst 28 tatsächlich aufgerufene neue
Originalfunktionen; 31 eingebundene Körper sind nicht gleichbedeutend mit
31 nachgewiesenen Ausführungen.

Die Pool-/Hierarchie-, Save-/Load- und Pointertests sind **isolierte Tests echter
Originalfunktionen**. Die kurz angelegten ID-Gruppen und gesetzten Passed-Bits
dienen der Prüfung von Referenzen und Zustandsübergängen. Sie erzeugen keine
HUD-Grafik und werden nicht als Checkpoint oder Gameplayzustand eingeschleust.

## Native ID-Verwaltung

`IdRuntime` besitzt den tatsächlichen Haupt-Pool von 128 `ID_UNIT`-Objekten.
Sämtliche Originalfelder einschließlich Matrizen, Parent-, Path- und Curve-Pointer
werden übernommen. Speicher stammt aus `GameplayHeaps`; Placement-new beginnt
die nativen C++-Objektlebenszeiten. Der ursprüngliche Byteclear eines Units wird
durch Zuweisung eines wertinitialisierten nativen `ID_UNIT` ersetzt. Flags und
Default-UVs werden danach im unveränderten Originalkörper gesetzt.

`IDSystem::roomInit` setzt `m_nId = 0`, leert sämtliche Units und setzt
`be_flag = 0xFF`; beide acht Worte großen Klassenflag-Tabellen werden gelöscht.
Der Originalkörper setzt `m_levelMax` nicht zurück; diese Semantik bleibt erhalten.
Ein geräumter Pool ist keine erfolgreiche Instanziierung von HUD- oder Menütabellen.

Die ursprüngliche Macro-Arithmetik mit `(u32)m_IdUnit` wird durch typisierte
Pointeraddition ersetzt. Parentbezüge liegen ausschließlich im nativen Pool.
Die Adapter prüfen Owner-Thread, lebende Slots und Zyklen vor dem originalen
rekursiven Parent-/Level-Aufruf. Handles enthalten Slot, Generation und eine
über Poolinstanzen hinweg eindeutige Runtime-ID. Reset, Slotwiederverwendung
und fremde Pools werden abgewiesen, ohne native Pointer zu verkürzen.

Vollständige Freigabe verlangt freigegebene Schedulerstacks. Im Original-Startup
darf `roomInit` selbstverständlich innerhalb des laufenden GameTask ausgeführt
werden. Neue Generationen schützen danach vor ungültigen öffentlichen Handles.
Ein geliehener `ID_UNIT&` bleibt ein Owner-Thread-Borrow bis zum nächsten
Reset/Free; er ist kein dauerhaft gültiger Ressourcenhandle.

`OriginalContext::IdFacade::dispSw` verwendet jetzt dieselbe gemeinsame
ID-Verwaltung. Die bisherige separate Missing-Consumer-Grenze dafür entfällt.
`ObjMgr.warnDiv` ist ein aus `cManager` belegtes originales Skalarfeld;
die Feldzuweisung behauptet keine fertig gebundene Objektverwaltung.

## Raumzustand und Save-Image

Originalbelege: `src/game/roomdata.cpp:80` und `include/room_data.h`,
`src/game/game.cpp:878` bis `:969`, `include/game.h`,
`src/game/item.cpp:2859`, `sscrn.cpp:54`, `merchant.cpp:939`.

Die fünf vollständigen originalen `St<n>_data_tbl` und `Room_data_tbl` werden
aus dem Quellcode übernommen. Es werden keine Raumlisten manuell erfunden.
Sie umfassen 216 Tabelleneinträge und 121 Einträge mit `save_flg == 1`.
Diese Zahl ist keine Disc-Archivabdeckung: Stage 0 enthält beispielsweise
67 Einträge ohne Room-Save-Record. Der Originalstart r120 hat ebenfalls
`save_flg == 0`; er bekommt keinen erfundenen Checkpointdatensatz.

`RoomSaveHdr` bleibt 16 Byte, jeder `ROOM_SAVE_DATA` 216 Byte.
Der lebende Room-Save-Block hat damit 26152 Byte. Seine IDs, Passed-Bits,
Save-/Item-/Find-Flags, EtcModel-, BGM- und Streamfelder bleiben original.
Die Source-Funktionen speichern den ganzen Block, laden nach Room-ID und
leeren passende Records unter Erhalt ihrer IDs.

Die originale Big-Endian-Union `Stage/Room/RoomNo` wird intern als nativer
`u16 RoomNo = stage<<8|room` geführt. Das originale Stempeln der ersten zwei
BE-Bytes wird entsprechend angepasst. Alle späteren Source-Vergleiche benutzen
denselben numerischen Room-ID-Wert; Passed-Bits liegen weiterhin bei Byte 2.
Native Room-Save-Blöcke sind keine direkt serialisierten GameCube-Bytes.

`cGameSave::alloc` erzeugt das originale, 45792 Byte große Save-Image mit diesen
unveränderten relativen Section-Offsets:

| Section | Offset | Originalgröße, vor ALIGN32 |
|---|---:|---:|
| Global | 0x40 / 64 | 0x36F8 / 14072 |
| Room | 0x3740 / 14144 | 26152 |
| SubScreen | 40320 | 4 |
| Merchant | 40352 | 0x304 / 772 |
| Items | 41152 | 0x1204 / 4612 |

`calcOffset`, `calcAddr` und `checkAddr` behalten die Originalreihenfolge der
Pointerfelder bei. Nur temporäre Adressen und das Baseargument werden von
`u32` auf `uintptr_t` erweitert. File-/Sectiongrößen und Offsets bleiben `u32`.
Die ursprüngliche Darstellung von Offsets in den Pointerfeldern wird lediglich
für das kontrollierte native In-Memory-Image verwendet; vor Dereferenzierung
wird es wieder auf seine eigene Base rebasiert und geprüft.

`SaveStateRuntime` prüft Ownership, Generation, Imagegröße, alle fünf Sections
sowie Roomheader und Recordgrenzen. Kopien werden mit dem originalen
`checkAddr` auf ihre neue Adresse rebasiert. Das Clear/Reset gibt den logischen
Besitz frei; vorhandene Leases halten die alten Speicherbereiche noch sicher am
Leben, während weitere Operationen mit alten Images abgewiesen werden.
Die diagnostischen Kopien gehören bis zum vollständigen Service-Reset zum
aktuellen Boot; es gibt noch keine Gameplay-Checkpoint-Copy-Warteschlange.

**Offen:** `cGameSave::save/load`, `GameLoad`, `GameContinue`, Memory-Card-Dateiformat
und dauerhafte Save-Dateien sind nicht implementiert. Das Original kopiert
14072 zusammenhängende Globalbytes und bindet außerdem Spielerposition,
Room-, Item-, SubScreen- und Merchant-Speicher sowie Kostüm-/BGM-/Second-Round-
Zustände. Der bislang partielle `NativeGameState` darf dafür nicht als vollständiges
GLOBAL_WK-Overlay verwendet werden. Die Größenfunktionen und die Allokation
initialisieren weder Händlerbestand noch Startinventar. Diese Systeme erhalten
keine Erfolgs-Stubs. Leerer, original allokierter Speicher ist kein gültiger,
vollständig gespeicherter Spielstand.

## Tatsächliche Tests

| Plattform | Host-Testgruppen | echte Originaldaten-Bootläufe | Checks je Lauf |
|---|---:|---:|---:|
| Windows x86 | 25/25 | beide Discs × zwei Runden | 1909 |
| Windows x64 | 25/25 | beide Discs × zwei Runden | 1909 |
| Retroid armeabi-v7a | native Core + APK | beide Discs × zwei Runden | 1909 |
| Retroid arm64-v8a | native Core + APK | beide Discs × zwei Runden | 1909 |

ABI-Differenzen sind explizit geprüft:

| Nativer Typ | x86 / ARM32 | x64 / ARM64 |
|---|---:|---:|
| ID_UNIT | 312 Byte | 344 Byte |
| SAVE_DATA_HEAD | 28 Byte | 56 Byte |
| ROOM_SAVE_DATA | 216 Byte | 216 Byte |

Der größere native Saveheader passt weiterhin vor den ersten Originaloffset 0x40.
Semantische JSON-Traces vergleichen keine Pointeradressen oder native Typgrößen;
die Größen werden separat als ABI-Metadaten geprüft. Prüfzahlen, IDs, Offsets,
Referenzen, Flags, Source-Traces und Datenhashes müssen übereinstimmen.

Geprüft sind GameTask/Cockpit-Reset, gelöschte Klassentabellen, 128 Poolslots,
Original-UV-Defaults, voller Pool, Slotwiederverwendung, Parent-/Child-Level,
rekursive Timer/Move-Flags, rekursive Gruppenfreigabe, Kill-all, Zyklusabwehr,
stale Handles nach Roomreset und zwei isolierte Originalpools mit **identischen
Slots/Generationen** zur Überprüfung der Runtime-ID. Freigabe eines Pools darf
den anderen nicht beschädigen. Live Taskstacks verhindern Full-Free.

Alle 121 Original-Room-IDs werden gegen die Source-Tabellen geprüft;
Save/Load/Clear und Passed-Bits werden über alle Records getestet.
Native Imagekopie/Rebase, zweimalige Offset-/Address-Konvertierung,
falsche Sections, übergroße Recordanzahl, invalidierte Images nach Reboot,
Wiederinitialisierung und gepinnte Alt-Speicherbereiche sind geprüft.
Es wurde kein AddressSanitizer-Lauf ausgeführt; Pointer-/Speichernachweise hier
sind die konkreten Bounds-, Ownership-, Generation- und Lebenszeittests.

Retroid: `dbdcacc`, Android 13/API 33, ABIs `arm64-v8a,armeabi-v7a,armeabi`.
Jeder native Prozess bestätigt seine tatsächliche Pointerbreite.
APK-Installationen erzwingen die geprüfte ABI.

Weitere abschließende Regressionen:

- Android Core: je zwei Runden, ARM32 13386 und ARM64 13387 Checks, null Fehler;
  identische sieben Scheduler-/Background-/Yz2-Traces. Ein zusätzlicher ARM64-
  Check prüft einen Pointer oberhalb 4 GiB.
- 634 assetfreie GameRuntime-Fixturechecks bleiben unverändert. Alle 37 r120-
  BIN-Vorkommen, pl00-/TPL-Abhängigkeiten und weitere r100-/r317-Modelle bleiben
  nachgewiesen; pro ABI 180 bestehende Original-Modellinitialisierungen,
  48 isolierte SubScreen-Modellinitialisierungen, 76 SetPartsSub,
  132 Event-/Additional- und 264 Player-MotionSetCore-Aufrufe.
- Original SubScreen: pro ABI vier echte Bootläufe, acht GameInit-Ausführungen,
  176 Archivloads, 44 Archive mit 868 Blöcken pro Disc/Lauf; Hashes und Referenzen
  unverändert. Die 35 unvollständigen englischen Raum-MDTs erhalten keinen Ersatz.
- MRAM-/ARAM-Pipeline: jede ABI, zwei Runden, pro Runde 308 Ressourcen
  (157/151 pro Disc) und 142 Räume (85/57), einschließlich Transfers,
  Referenzhashes, Abbruch, erneuten Anforderungen und Lebenszeiten.
- Modulpipeline: jede ABI, zwei Runden, je Runde 114 Original-REL-Metadaten
  (110/4 pro Disc), bisherige partielle native Registrierung und Originalcallbacks.
  Das ist keine Ausführung von PPC-REL-Maschinencode.
- Android Debug-/Test-APK und Lint gebaut; jeweils 20 Instrumentierungstests
  unter ARM32 und ARM64 einschließlich Controller/Lifecycle/Frameadapter und
  echten Vulkan-Clears, GPU-Readback und Präsentation.

Die assetfreie APK stoppt GameTask weiterhin vor dem privaten Message-Bootpfad.
Der neue Startnachweis mit G4BE08-Daten ist aus den nativen Android-CLI-Läufen.
Die APK beweist separat die echte Vulkan-/VI-Anbindung und den bisherigen
R120Event-Fortschritt bis `Sofdec.Initialize`. Das Opening wird nicht abgeschlossen
oder übersprungen; ein spielbarer Raum oder sichtbares HUD wird nicht behauptet.
Keine erneuten physischen Tastendrücke wurden angefordert. Controllerregressionen
hier sind Adaptertests; der frühere physische Nachweis bleibt erhalten.

Vulkan bleibt unverändert: Adreno 650, EFB 512×448, echte GPU-Clears und
Präsentation. GPU-Zähler stehen in `port/build/graphics-<ABI>.json` und können
durch Display-/Lifecycle-Timing variieren; Clears und Presents müssen pro Lauf
übereinstimmen. Effektiv Samples 1, AF 1, DoF aus. MSAA Aus/2×/4×, AF 2×/4×/8×,
DoF Nah-/Zielfokus, Persistenz und hardwarevalidierte effektive Einstellungen
bleiben verbindliche spätere Anforderungen. Keine GX-Geometrie implementiert.

Frühere fehlgeschlagene Versuche sind nicht als Erfolg gezählt: ADB ließ sich
in der Sandbox nicht starten; die expliziten Gerätetests wurden außerhalb wiederholt.
Der erste Hostbuild hatte einen falschen Scheduler-Typnamen, der korrigiert wurde.
ARM64 Clang wies fünf originale u32-Offset-zu-Pointer-Casts ab; sie sind jetzt
pointerbreit. Ein isolierter Fremdpooltest versuchte einen zweiten Scheduler auf
demselben Owner-Thread und wurde von der bestehenden Single-Domain-Sicherung
abgewiesen; die Prüfung verwendet nun zwei Pools in der vorhandenen Domäne.
Alle abschließenden Tests wurden mit den korrigierten Dateien durchgeführt.

## Reproduktion und Dateien

Vorhandene Build-/Testumgebung, keine neue separate Testinfrastruktur:

```powershell
# Jeweils passende MSVC Developer-Shell
cmake --build port/build/host-ninja --parallel
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
ctest --test-dir port/build/host-x86 --output-on-failure
.\port\scripts\build-android.ps1 -SdkRoot <SDK> -JavaHome <JBR> -Python <Python> -Offline
.\port\scripts\test-device.ps1 -Adb <ADB> -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb <ADB> -Serial dbdcacc
<Python> port/tools/test_game_runtime.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase4c-id-save-host --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
# Danach ebenfalls mit host-x86/re4_game_tool.exe
<Python> port/tools/test_game_runtime.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase4c-id-save-device --adb <ADB> --serial dbdcacc --repeat 2
<Python> port/tools/summarize_consumers.py --host E:\Development\Android\RE4-Data\phase4c-id-save-host --device E:\Development\Android\RE4-Data\phase4c-id-save-device --output E:\Development\Android\RE4-Data\phase4c-id-save
```

Zusätzlich `test_resources.py` und `test_modules.py`, beide Host-Executables
und danach `--adb <ADB> --serial dbdcacc --repeat 2`. Private Berichte liegen in
`RE4-Data/phase4c-id-save-host`, `-device`, `-resources`, `-modules` und
`phase4c-id-save`; sie enthalten Executable-Hashes und vollständige Messtraces.
Logs liegen im ignorierten `port/build/id-*.log`. Der Summary-Generator verlangt
identische semantische Ergebnisse über alle vier Architekturen und beide Discs.

Neu, vollständig gespeichert, relativ zu `port/`:

- `config/id-save-sources.json`
- `include/re4/port/id_system.h`, `include/re4/port/save_state.h`
- `src/id_save.cpp`, `src/id_save_original.h`
- `tools/generate_id_save.py`
- `docs/phase4c-id-save-status.md`

Verändert, vollständig gespeichert:

- `CMakeLists.txt`, `include/re4/port/game_runtime.h`
- `src/game_runtime.cpp`, `src/runtime_original.h`
- `tests/game_runtime_tool.cpp`, `tests/test_consumer_sources.py`
- `tools/generate_runtime.py`, `tools/test_game_runtime.py`, `tools/summarize_consumers.py`

Generierte vollständige Originalkörper und Typdateien sind zusätzlich in den
Buildverzeichnissen vorhanden. Keine GCM-Datei oder extrahierte Assetdatei wird
in Git übernommen; die verbindlichen Images und privaten Imports bleiben getrennt.

`master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`;
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die README-Aktualisierung wurde gelesen und berücksichtigt. Kein Merge,
Rebase oder Push, keine README- oder Originalcodeänderung.

## Nächste konkrete Abhängigkeiten

Der nächste gemeinsame GameTask-Schritt ist `cLightMgr::init` (`light.cpp:61`):
originaler cManager-/Typfunktionszustand, LightEnv, Power, Kindflags, native
Pointer und anschließendes `initPath` (`light.cpp:1075`) mit dem echten
Core-Light-Path-Block. Danach folgen PlayerInit und die belegten weiteren
Originalinitialisierungen; sie wurden nicht vorgezogen.

Für tatsächliche Checkpoints müssen die vollständigen Global-Save-Felder und
Item-/SubScreen-/Merchant-/Player-/Continue-Verbraucher gemeinsam gebunden und
GC-Wireformat beziehungsweise native Persistenz explizit getrennt werden.
Der Save-Record-Teil der bestehenden RoomFacade kann dann dieselbe bereits
gemeinsame SaveStateRuntime verwenden; die REL-Verwaltung bleibt bei ModuleRuntime.

Für sichtbare IDs fehlen die Original-ID-Tabelleninstanziierung und deren
BE-/Offsetauflösung, FuncPath/Hermite, IdTex/TexAnm, OT und GX-Zeichenausgabe.
`Cockpit::roomInit` und die HUD-Subdisplays sind deshalb noch nicht lauffähig.
Für Phase 5 verbleiben die vollständige GX-Geometrie, Material-/Textur- und
ID-Ausgabe sowie die bereits dokumentierten Sofdec-/Audio-/Videofunktionen.
Phase 5 wurde nicht begonnen.
