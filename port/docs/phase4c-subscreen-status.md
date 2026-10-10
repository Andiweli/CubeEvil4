# Phase 4c: originale SubScreen-, Inventar- und Map-Initialisierung

Fortsetzung vom 10. Oktober 2026 auf `android-port`, Ausgangscommit
`4a3edfe92b7ff1ca56ab860220dbf83984268644`.

## Ergebnis und Ausführungsgrenze

**Nachgewiesen:** Der originale `GameTask` führt `SubScreenGameInit()` vollständig
aus und erreicht anschließend den Aufruf `Cckpt.gameInit()`.
Die abschließenden Originaldaten-Läufe bestehen unter Windows x86/x64 und auf
dem Retroid Pocket Flip 2 unter Android ARM32/ARM64. Es wird kein spielbarer
Raum, geöffnetes Inventarmenü oder abgeschlossenes Opening behauptet.

Die tatsächliche Reihenfolge ist:

```text
Native Bootvorbereitung: Original cItemMgr::init (Title_task-Voraussetzung)
Original CoreDataRead -> MessageControl::init
Original GameTask -> gameDebug -> gameInit
  ClothInit -> GXSetCopyClear -> MessageControl::gameInit
  SubScreenGameInit
    sscrnSetLanguage
    SubScreenAramRead
      DvdReadN("rel/Sscrn.rel", mode 9) -> DVD/DMA -> ReadCheck
      sscrnDataFilename("ss_cmmn.dat") -> DVD/DMA -> ReadCheck
      sscrnDataFilename("ss_pzzl.dat") -> DVD/DMA -> ReadCheck
    board/map reset -> originales 0x44-Byte-Ope-memset
    SubScreenRoomInit
      vier originale ItemMgr.search-Aufrufe
      originale Statusflags -> cManager<cMap>::roomInit
  Cckpt.gameInit(): explizit noch nicht angebunden
```

Originalbelege: `src/game/game.cpp:237` (`gameInit`),
`src/game/sscrn.cpp:73` (`SubScreenAramRead`), `:143` (`SubScreenGameInit`),
`:159` (`SubScreenRoomInit`), `src/game/title.cpp:90` (`Title_task`),
`include/cManager.h:220` (`roomInit`).

`pG->IsMessageInit` ist nach dem realen GameTask-Lauf 1;
`pG->Rno0` bleibt 0, weil die gesamte Spielinitialisierung noch nicht fertig ist.
Es gibt keine künstliche Fortschaltung zur Stage-/Room-Phase.

## Originalcode und Plattformadapter

`generate_subscreen.py` extrahiert 25 vollständige Originalfunktionskörper.
Die SHA-256-Fingerprints stehen in `config/subscreen-sources.json`;
Quelle, Signatur, Zeile und jede Anpassung werden unter
`build/<build>/original/subscreen-manifest.json` aufgezeichnet.
Die Originaldateien in `src/` und `include/` bleiben unverändert.

Neu eingebundene Originalfunktionen:

- `SubScreenGameInit`, `SubScreenAramRead`, `SubScreenRoomInit`,
  `SubScreenWait`, `sscrnSetLanguage`, `sscrnDataFilename`.
- `cItemMgr::init`, `clear`, `flagclear`, `roomInit`, `search`, `at`,
  `searchAt`, `construct`, `get`, `countFiles`.
- `itemInfo`, die drei Original-Inlines `itemType`, `itemDefNum`, `itemMaxNum`,
  `setBullet`, `WeaponId2ChargeNum`, `addMoney` mit der originalen `wep_info`-Tabelle.
- `cManager<T>::roomInit`, nativ als `MapManagerCpu::roomInit` qualifiziert.
- `mapAreaFilename` aus `src/Sscrn/ss_map.cpp:2296`.

Die unmittelbar benötigten SubScreen-Funktionen, Inventarallokation und Suche
werden im echten Startup benutzt. Registrierung, Dokument-/Waffenkonstruktion,
Treasure-Stacking und alle 256 `itemInfo`-Abfragen werden zusätzlich als
**isolierte Tests echter Originalfunktionen** ausgeführt. Ihre Testgegenstände
werden gelöscht und nicht als Startinventar in GameTask eingeschleust.
Der ungewöhnliche Originaltest `!p->isAlive(m_char)` in `countFiles()` bleibt
unverändert; diese Phase korrigiert keine vermuteten Originalbugs.

Die native Bootvorbereitung erfüllt den belegten `Title_task`-Aufruf
`ItemMgr.init()`. Sie führt noch nicht den vollständigen Titel-/Save-/Optionpfad
aus. Die Startgegenstände aus `cItemMgr::gameInit()` werden im Original erst
**nach** `PlayerInit()` angelegt (`game.cpp`). Dieser spätere Pfad ist weiterhin
offen und wurde nicht vorgezogen.

Notwendige Adapter:

- `SubScreenRuntime` besitzt Inventartabellen, Zustandsansicht und Ressourcen.
  `SubScreenWorkCpu` enthält nur die tatsächlich benutzten Originalfelder;
  es ist kein vollständiger `SUB_SCREEN`-Layoutnachbau.
- Native Objekte beginnen ihre C++-Lebenszeit per Placement-new in den
  originalen Heapallokationen. `cItem` bleibt 14 Byte. `ITEM_ORDER` verwendet
  echte native Pointer und dadurch ABI-abhängig 8 beziehungsweise 16 Byte.
  Anzahl, IDs, Flags und Suchreihenfolge bleiben original.
- `at()` weist negative Indizes ab; die Originalfunktion konnte dabei vor die
  Tabelle greifen. Die Originalaufrufstellen verwenden nichtnegative Indizes.
- Die Ope-Felder einschließlich Originalpadding sind aus `GLOBAL_WK` kopiert.
  Ein Compile-time-Test beweist den zusammenhängenden, pointerfreien
  **68-Byte-Bereich** des originalen `memset`; es wird kein GC-Offset als
  nativer Speicheroffset benutzt.
- `p_module` ist in diesem Pfad eine logische ARAM-Adresse. Sie bleibt `u32`;
  native Pointer werden weder darin gespeichert noch darauf verkürzt.
  Das Laden von REL-Bytes gilt nicht als Ausführung des PPC-Moduls.
- Inventarallokationen und Readback-Leases pinnen ihre Speicherbesitzer.
  Ein voller nativer Neustart gibt zuerst sämtliche Taskstacks frei und danach
  diese Besitzer. Raum-/Modulretirement behält die gemeinsamen Startupdaten.
  Von `findItem()` geliehene Slotpointer gelten bis zum vollen Neustart;
  die bestehende Taskquieszenz verhindert lebende Callbacks auf freigegebenen Slots.
- Native Diagnoselogger geben Originalmeldungen aus; fehlende Gameplayfunktionen
  werfen weiterhin `MissingGameConsumer`.

Es gibt kein erfolgreiches Ersatz-`cItemMgr::gameInit`, keine leere Map-Modellerstellung
und keine simulierte Sscrn-Prologausführung. Mercenaries-Zeitfunktionen im
übernommenen `get()`-Pfad bleiben ausdrücklich nicht verfügbar; sie melden einen
konkreten fehlenden Consumer statt erfolgreicher Verarbeitung.

## ARAM-/MRAM-Pfad und Originalressourcen

Der frühere Datencontrollerbereich endet bei `0xD00000`.
`sscrn.cpp` reserviert darüber `SS_ARAM=0xD00000`, `SS_ARAM_SIZE=0x300000`.
Der native DVD-/DMA-Adapter unterstützt jetzt diesen separaten Bereich bis
`0x1000000`. Allgemeine DC-Allokation, Kompaktierung und explizite DC-Ziele
bleiben auf den bisherigen Bereich beschränkt. Grenzübertritt und nicht
ausgerichtete ARAM-Ziele werden abgewiesen.

| Datei, tatsächlicher FST-Name | Nutzbytes | gemeldete DMA-Bytes | ARAM-Adresse |
|---|---:|---:|---:|
| `Rel/Sscrn.rel` | 310156 | 310176 | 13631488 (`0xD00000`) |
| `ss/Eng/ss_cmmn.dat` | 531040 | 531040 | 13941664 |
| `ss/Eng/ss_pzzl.dat` | 912704 | 912704 | 14472704 |

Diese drei Dateien sind auf beiden Discs byteidentisch. Ihre SHA-256-Werte sind:

```text
Sscrn.rel   1b55e8a211c592c0ec571bc757145210136bf9a3f2a4482802b61e83f99107f1
ss_cmmn.dat d581d82ecdf1d23520e4f908ec3f5a7dcc6618adf7039c07248b3460c648c4c8
ss_pzzl.dat 7dec5da8fbb7fb97dbaea40b059b095557bd15cca31117d1b6cb0378e0048fd7
```

`pCommonOffs=310176`, `pSwitchOffs=841216`, `pFreeOffs=1753920`.
Die Rundung ist durch `cDvdQueue::trans2aram` / `trans2aram_cb` in
`src/game/dvd.cpp:427–445` belegt: der Callback zählt `ALIGN32(size)`.
Hashvergleiche prüfen die FST-Nutzbytes; native DMA-Auffüllbytes sind keine
Originalressourcen und werden nicht als verifizierte Discbytes ausgegeben.

`EngineLoader::request` erhält einen typisierten ARAM-Zielparameter. Die
SubScreen-Aufrufe benutzen tatsächlich den vorhandenen synchronen Mode 9,
`cDvd::ReadReq`, `blockRead`, Dateiworker und `cAram::DmaTransReq`.
`EngineLoader::readAram` führt den tatsächlichen synchronen ARAM→MRAM-Transfer
aus; die Tests vergleichen dessen Ergebnis mit dem Importkatalog.
Das ist kein Vergleich zweier Zeiger auf den originalen Eingabepuffer.

Mode 9 blockiert wie im Original und wird innerhalb seines Aufrufs vollständig
abgearbeitet. Es wird kein angeblicher Abbruch mitten in diesem synchronen
Aufruf behauptet. Der ARAM-Watcher wird dabei weitergepumpt. Die bestehenden
asynchronen DC-/Background-Pfade behalten ihre Scheduler- und Abbruchsemantik.

## Inventar-/Map-Abdeckung und Modelle

Pro Disc werden über die vorhandene gemeinsame Ressourcenverwaltung
**44 Archive** geladen und mit dem Phase-3a-Katalog verglichen:
26 tatsächliche `ss/Eng/*.dat` sowie 18 `ss/Cmn/map_obj*.dat`.
Darunter befinden sich `ss_item.dat`, `ss_map.dat`, `ss_cap.dat`, `ss_file.dat`,
`ss_shop.dat`, `ss_term.dat`, Kapitel-/Result-/Option-/Titeldaten.
Diese breitere Inventur behauptet nicht, dass alle Dateien schon beim Startup
benötigt oder ihre Widgets ausgeführt werden.

Die Originalfunktion `mapAreaFilename` liefert für 18 Area-IDs 17 unterschiedliche
Dateinamen: Area 0 und 1 benutzen beide `map_obj1.dat`. Die zusätzlich vorhandene
`map_obj2.dat` wird in dieser Funktion nicht ausgewählt und bleibt erhalten.

Alle **868 Tabellenblöcke** werden mit originalen Indizes, Tags, Offsets,
Größen und Hashes gegen `original_import.formats.body_index` überprüft:

| Originaltag | Anzahl | nachgewiesener Umfang |
|---|---:|---|
| TPL | 78 | begrenzte native TPL-/CLUT-/Pixelreferenzen geprüft; keine GPU-Ausgabe behauptet |
| BIN | 107 | Bytes/Referenzen erhalten; sechs belegte BIN/TPL-Paare zusätzlich initialisiert |
| MAP | 142 | Dateigrenzen und Integrität geprüft; Map-Geometrie-/Roomtabellenconsumer noch offen |
| UWF | 83 | unverändert erhalten; Widgetinterpretation offen |
| EFF | 30 | unverändert erhalten; Effektconsumer offen |
| MDT | 30 | unverändert erhalten; diese SS-Messagepfade noch nicht vollständig ausgeführt |
| LIT | 4 | unverändert erhalten; Lichtconsumer offen |
| FNT | 3 | unverändert erhalten; zugehöriger SS-Fontpfad noch offen |
| FCV | 2 | unverändert erhalten; kein neuer Animationsablauf erfunden |
| leeres Tag | 389 | nachgewiesen sämtlich Nullgrößen; originale leere Slots bleiben vorhanden |

Die 78 TPL-Blöcke werden mit dem bereits vorhandenen nativen Parser geprüft.
Für die restlichen 790 Einträge bedeutet der Diagnosezustand `opaque-retained`
Integrität und Erhalt, keine vollständige Formatimplementierung.

**Isolierte Modellconsumer mit echten Originaldaten:**

- Vier Koffer-BINs: `ss_pzzl.dat` Slots `0x1A6..0x1A9`, TPL `0x1A5`;
  die Zuordnung stammt aus `pieceModelInit` in `src/Sscrn/ss_pzzl.cpp:1059`.
  Jeder BIN hat zwei Parts.
- Gemeinsames Türmodell: `ss_map.dat` BIN `0xC`, TPL `0xD`, gemäß `doorModelInit`.
- Gemeinsames Markermodell: `ss_map.dat` BIN `0xF`, TPL `0x10`, gemäß Map-Markerfunktionen.
  Beide Modelle haben jeweils einen Part.

Jedes Paar wird zweimal über die vorhandenen originalen
`cModInfoMgr::create` / `cModel::modelInit` initialisiert. Elternreferenzen,
Motion-Defaultzustand und Freigabe werden geprüft. Hierbei laufen nicht
`pieceModelInit`, `mapModelInit`, `doorModelInit` oder vollständige Sscrn-Widgets.
Es werden keine Ersatzobjekte für diese noch fehlenden Funktionen erzeugt.

Das originale `cManager<cMap>::roomInit` setzt den Manager leer; es lädt selbst
keine Kartenmodelle. `sscrnModelAlloc` / `mapModelAlloc`, die eigentliche
Raumzuordnung, Item-Piece-Tabellen, Item-Modelpack-Registrierung, GUI-Widgets und
`SubScreenExec` / `SubScreenExit` bleiben nächste gemeinsame Abhängigkeiten.

Englisch ist die gemeinsame Referenzsprache. Die vorhandenen vollständigen
Core-Itemnamen mit 256 Einträgen bleiben angebunden. `font/sscrn_p.fnt`
(525792 Byte, auf beiden Discs identisch) ist als spätere SS-Fontabhängigkeit
belegt, aber nicht als ausgeführter neuer Fontconsumer gewertet.
Die 35 bekannten unvollständigen englischen Raum-MDTs bleiben unverändert;
es gibt keinen Ersatztext oder Wechsel auf eine andere Sprache.

## Sofdec-/CRI-Analyse und verbleibende Startupgrenzen

`R120Init` und die vorhandenen Event-/Modellconsumer bleiben erfolgreich.
Mit echter VI/GPU-Anbindung erreicht `R120Event` weiterhin
`Sofdec.Initialize("movie/opening.sfd", 0)` und stoppt dort ausdrücklich.
Im reinen Host-/CLI-Lauf fehlt die Displaypräsentation; dieser Headless-Grenzfall
bleibt vom APK-VI-/Vulkan-Nachweis getrennt.

Die konkrete Sofdec-Kette in `src/game/sofdec.cpp`:

1. `SofdecInit:55`: 59.94-Hz-Parameter, `ADXM_SetCbErr`, `mwPlyInitSfdFx`.
2. `Initialize:534` / `initSub:547`: Task 0 suspendieren, Slot 1 mit
   `ThreadMove` starten (oder Inlinepfad bei Flag `0x200`), `SndAllStop`.
3. `initWork:508`: Datei/Fallback prüfen, Stop-/Disp-Flags sichern,
   5 MiB an ARAM-Adresse `0x740000` auslagern, Heap 11 erstellen.
   Dieser Moviebereich liegt unter der bisher gebundenen Daten-ARAM-Basis
   `0x800000` und wird durch die neue SubScreen-Bank nicht implementiert.
4. `initApp:356`: Draw-/Camera-Zustand und `0x5000`-Byte-Headerread;
   `mwPlyGetHdrInf` benötigt `SFD_AnalyCreInf` und
   `MWSFFRM_AnalyTotalFrmNum` (`src/lib/mwsfdcre.c:234`).
5. `startApp:380`: CRI-Workgrößen, Sofdec-Handle und echte Dateistreams
   (8-Mbit/s-Konfiguration, vier Framepuffer, zwei Streams).
6. `Move` / `appMain` / `drawTex`: Demultiplexer/MPV, Framebesitz und Decoder,
   Audioanlage/PCM-Ausgabe, Y-/UV-/RGBA-Texturen, TEV- beziehungsweise native
   Farbkonvertierung, Videozeitbasis und VI-Präsentation.
7. `finishMovie`: Decoder/Buffer freigeben, Heap/ARAM und Stop-/Disp-Flags
   zurücksetzen; erst danach darf `ThreadMove` Task 0 fortsetzen.

`mwPlyInitSfdFx` (`src/lib/mwsfdlib.c:195`) benötigt bereits MWSFSVM, ADXT,
SJRBF, SJMEM, SJUNI, MWSTM, `mwPlySfdInit` und Servercallbacks.
Es ist keine isoliert fertige GPU-unabhängige Erfolgsinitialisierung.
`MWSFD_INIT_PRM` ist 0x20 Byte (`src/lib/cri/mwsfd.h:292`);
die zusätzlichen Originalparameter und ihre ABI-/Lebenszeitregeln sind vor
einer vollständigen Bindung zu überprüfen.
Konkrete Pointer-Trunkierungsstellen sind beispielsweise
`mwsfdcre.c:946`, `:993` und `sfd_ply.c:482`.

`Movie/opening.sfd` ist tatsächlich nur auf Disc 1 vorhanden:
29169664 Byte, SHA-256
`5810a094749f023aeba86ecc29a363951aa932fc5ff25875f7b131a2d25ccabe`.
`movie/dmy.sfd` wurde in keinem der beiden Importinventare gefunden.
Codec-/Streamparameter werden nicht aus dem Dateinamen angenommen.

**Entscheidung in dieser Phase:** Keine neue Sofdec-Funktion wird als lauffähig
ausgegeben. Die zusammenhängenden Anforderungen an Streaming, Movieheap,
Decoder, Ausgabe und Fehler-Rollback sind noch nicht erfüllt. Es gibt weder
eine künstliche Movie-Fertigmeldung noch eine übersprungene Originalszene.
Ein späterer Header-/Streamingadapter muss Teilreads ermöglichen; der aktuelle
Hash-verifizierende Loader liest auch bei Teilanforderungen die komplette Datei.

Der nächste reale GameTask-Blocker ist unabhängig davon
`Cockpit::gameInit` (`src/game/cockpit.cpp:21`): es ruft das originale
`IDSystem::roomInit` (`src/game/id_sys.cpp:55`) auf. Der vollständige native
ID_UNIT-Pool mit eigenen Pointer-/Textur-/Widgetlebenszeiten fehlt noch.
Die vorhandene begrenzte Event-ID-Fassade ist dafür kein Erfolgsersatz.
Danach bleiben unter anderem LightMgr, PlayerInit, Startinventar,
Merchant-/Save-/GamePointpfade und die vollständige Eventausführung offen.

## Ausgeführte Tests

Alle unten genannten abschließenden Läufe bestehen. Ein Import allein wird
nicht als Engine-Ladevorgang gewertet.

| Plattform | Host-Testgruppen | echte Core/Font/SubScreen-Läufe | Prüfungen pro echtem Lauf |
|---|---:|---:|---:|
| Windows x86 | 25/25 | 2 Discs × 2 Wiederholungen | 1130 |
| Windows x64 | 25/25 | 2 Discs × 2 Wiederholungen | 1130 |
| Retroid `armeabi-v7a` | native Tests + APK | 2 Discs × 2 Wiederholungen | 1130 |
| Retroid `arm64-v8a` | native Tests + APK | 2 Discs × 2 Wiederholungen | 1130 |

Retroid: Serial `dbdcacc`, Android 13/API 33, unterstützte ABIs
`arm64-v8a,armeabi-v7a,armeabi`. Die Testprozesse bestätigen zusätzlich ihre
Pointerbreite; APK-Installationen erzwingen die jeweilige ABI.

Weitere Regressionen auf jeder der vier Plattformen, jeweils zwei Runden:

- 634 Prüfungen der vorhandenen assetfreien GameRuntime-Fixtures.
- 20 reale Raum-/ESL-Vorbereitungen; alle 37 r120-BIN-Vorkommen,
  externe pl00/TPL-Bezüge und zusätzliche Modelle anderer Räume erhalten.
- 180 bisherige Original-Modellinitialisierungen und 48 zusätzliche,
  ausdrücklich isolierte Koffer-/Map-Modellinitialisierungen pro ABI.
- 76 `SetPartsSub`, 132 Event-/Additional-MotionSetCore,
  264 Player-MotionSetCore-Aufrufe pro ABI.
- Ressourcenpipeline: 308 Ressourcen (157/151 je Disc) und
  142 Räume (85/57 je Disc) **pro Runde**; MRAM/ARAM, Grenzen,
  Transfers, Abbruch, Wiederanforderung und Besitzprüfungen bestehen.
- Modulpipeline: alle 114 REL-Metadaten (110/4 je Disc) pro Runde,
  bestehende partielle native Originalregistrierung; keine PPC-REL-Ausführung behauptet.
- Android-Core-Tests: ARM32 13386, ARM64 13387 Prüfungen pro Runde,
  null Fehler, gleiche Schedulertraces. Die zusätzliche ARM64-Prüfung
  beweist einen Pointer oberhalb 4 GiB; die Differenz ist beabsichtigt.
- Debug-APK einschließlich Lint gebaut; jeweils 20 Instrumentierungstests
  unter ARM32 und ARM64. Controller, Lifecycle, Frameadapter und echte Vulkan-
  Clears/Readback/Präsentation bestehen.

Die assetfreie APK-Diagnose stoppt GameTask weiterhin vor dem privaten
Message-Bootpfad. Der neue Startupnachweis mit echten Daten stammt aus den
nativen Android-CLI-Prozessen. Dies sind unterschiedliche Tests derselben
Enginebausteine, kein behaupteter vollständiger APK-Spielstart mit Assets.
Es wurden keine neuen physischen Tastendrücke angefordert; der vorhandene
physische Controllernachweis bleibt erhalten. Synthetische Eingaben gelten
weiterhin nur als Adaptertests. Es gibt keine separate Inventar-Tastenverarbeitung.

Vulkan: Adreno 650, Vulkan 1.1.128, EFB 512×448. Messberichte mit den tatsächlichen
GPU-Clear-/Präsentationszählern liegen unter `build/graphics-<ABI>.json`.
Die Zähler können wegen Display-/Lifecycle-Timing zwischen den ABIs abweichen;
pro Lauf müssen GPU-verifizierte Clears und Präsentationen übereinstimmen.
Aktiv bleiben Samples 1, Anisotropie 1, DoF aus. Das ist keine Implementierung
des geplanten Menüs. Backendunabhängige MSAA Aus/2×/4×, AF 2×/4×/8×,
DoF Nah-/Zielfokus, persistente Speicherung und validierte effektive Einstellungen
bleiben verbindliche Anforderungen. Keine neue GX-Geometrieimplementierung.

Fehlversuche wurden nicht als Erfolg gezählt: Sandbox-Pfad-/Tempverzeichnisfehler
(`WinError 5`) wurden außerhalb der Sandbox wiederholt. Der erste MSVC-Build
beanstandete originale schmale Inline-Zuweisungen; die Warnungsbehandlung ist
auf die kopierten Originaldefinitionen beschränkt. Clang beanstandete einen
unvollständigen Aggregatinitialisierer und eine durch das Original-Enum ersetzte
Konstante; beide Ursachen wurden korrigiert. Ein Diagnose-JSON enthielt originale
NUL-Tagauffüllbytes; die Ausgabe entfernt jetzt ausschließlich diese Auffüllung,
und sämtliche Tags/Offsets/Hashes werden gegen den unabhängigen Bodyparser geprüft.

## Reproduktion, Dateien und Git

Vorhandene Skripte, keine neue separate Testumgebung:

```powershell
# MSVC Developer-Shell, jeweils passend zur Architektur
cmake --build port/build/host-ninja --parallel
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
ctest --test-dir port/build/host-x86 --output-on-failure

.\port\scripts\build-android.ps1 -SdkRoot <SDK> -JavaHome <JBR> -Python <Python> -Offline
.\port\scripts\test-device.ps1 -Adb <ADB> -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb <ADB> -Serial dbdcacc

<Python> port/tools/test_game_runtime.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase4c-subscreen-host --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
# Danach derselbe Befehl mit host-x86/re4_game_tool.exe.
<Python> port/tools/test_game_runtime.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase4c-subscreen-device --adb <ADB> --serial dbdcacc --repeat 2
<Python> port/tools/summarize_consumers.py --host E:\Development\Android\RE4-Data\phase4c-subscreen-host --device E:\Development\Android\RE4-Data\phase4c-subscreen-device --output E:\Development\Android\RE4-Data\phase4c-subscreen
```

Zusätzlich wurden `test_resources.py` und `test_modules.py` mit beiden Host-
Executables und anschließend `--adb <ADB> --serial dbdcacc --repeat 2` ausgeführt.
Die privaten vollständigen Berichte liegen in `RE4-Data/phase4c-subscreen-host`,
`-device`, `-resources`, `-modules` und `phase4c-subscreen`.
Sie enthalten Executable-Hashes, jeden Archivblock, Quellenzuordnungen und Traces.
Build-/Testlogs liegen ausschließlich im ignorierten `port/build/subscreen-*.log`.

Neu, jeweils als vollständige Datei gespeichert:

- `config/subscreen-sources.json`
- `include/re4/port/subscreen.h`
- `src/subscreen.cpp`, `src/subscreen_original.h`
- `tools/generate_subscreen.py`
- `docs/phase4c-subscreen-status.md`

Verändert, jeweils vollständig gespeichert:

- `CMakeLists.txt`
- `include/re4/port/engine_loader.h`, `include/re4/port/game_runtime.h`
- `src/engine_loader.cpp`, `src/resource_runtime.cpp`, `src/game_runtime.cpp`,
  `src/runtime_original.h`
- `tests/game_runtime_tool.cpp`, `tests/test_consumer_sources.py`
- `tools/generate_runtime.py`, `tools/test_game_runtime.py`, `tools/summarize_consumers.py`

Alle diese Pfade sind relativ zu `port/`. Generierte vollständige Original-
Funktionsdateien und native Typdefinitionen befinden sich zusätzlich lokal in
den jeweiligen Buildverzeichnissen. Es werden keine Assets dort als Gitdateien
übernommen. Private Daten bleiben im separaten Datenverzeichnis; GCM-Images
werden nicht verändert.

`master` bleibt bei `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` bei `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die README-Ergänzung mit 25 Zeilen wurde gelesen und berücksichtigt:
früher, nicht spielbarer Android-Port, getrennte ABIs und keine Assets im Repository.
Keine Änderung an README/master, kein Merge, Rebase oder Push.

**Nächster konkreter Auftrag:** gemeinsame IDSystem-/Cockpit-Initialisierung
mit echten ID_UNIT- und Texturlebenszeiten anbinden, anschließend den nächsten
Original-GameTask-Blocker prüfen. Menü-Widgets, Videowiedergabe und ein vollständiger
GX-Renderer benötigen eigene anschließende Aufträge. Phase 5 wurde nicht begonnen.
