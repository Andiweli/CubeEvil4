# Phase 3b – Originale Datei- und Raumladefunktionen

Stand: 9. Oktober 2026. Grundlage: `50b33b5635ff0f7088db3cd493a34b6b31961bf8`.
Die vollständigen geänderten Dateien stehen unter `port/`; Originaldateien werden
weder überschrieben noch mit SDK-Stubs ergänzt.

## Erreichter Lademeilenstein

Der native **MRAM-Ladepfad** verbindet die originalen DVD-Steuerfunktionen,
`ReadAreaData`, den stackerhaltenden Scheduler, den Dateiworker, Yz2 und die
originale Registrierung der Blockdateien. Der Test wartet auf einen ausgeführten
Engine-Ladevorgang und kontrolliert danach Daten und Freigabe. Ein Import allein
erfüllt diesen Nachweis nicht.

Die vollständige GameCube-DVD-/ARAM-/Sound-Laufzeit ist damit noch nicht portiert.
Insbesondere werden Modelle und REL-Code noch nicht initialisiert und Sounddaten
noch nicht an eine Audioengine übergeben. Das Spiel ist weiterhin nicht spielbar.

## Git und aktualisierte README

Beim Start war `android-port` sauber und auf `50b33b56`. Lokal `master` und der
bisherige Remote-Tracking-Stand lagen auf
`9b76ff003081d6ece8b838453fb8310743a972c7`.
`git fetch origin master` aktualisierte ausschließlich `origin/master` auf
`15cf64f8`. Dieser Commit ergänzt 25 README-Zeilen zu Android-Zielen, bisherigem
Teststand, noch fehlender Spielbarkeit, Grafikoptionen und den Grenzen der
ursprünglichen Lizenz. Die Entwicklung und dieser Bericht berücksichtigen diese
Aussagen. Es besteht keine technische Merge-Abhängigkeit.

Kein Merge, Rebase, Push oder Wechsel auf `master`. Alle vier lokalen
Entwicklungscommits vor dieser Phase bleiben Vorfahren des neuen lokalen Commits.
Die lokale README auf `android-port` enthält das Remote-Update weiterhin nicht;
eine spätere Übernahme ist eine ausdrücklich abzustimmende Git-Aktion.

## Tatsächlich verwendeter Originalcode

`port/cmake/LoaderSources.cmake` erzeugt vollständige Definitionen aus den
fingerprintgeschützten Originaldateien. `#line` verweist auf deren Quellzeilen.
Die folgenden Definitionen laufen im nativen Ladepfad:

| Originaldatei | Funktionen / Daten |
|---|---|
| `src/game/dvd.cpp` | Originale `FileTbl` mit erhaltenen Indizes, `DvdRead`, `DvdReadN`, `DvdReadProc`, `cDvdQueue::LinkQueue`, `PushQueue`, `cDvd::ReadReq`, `ReadProc`, `readProcMain`, `pullReadQueue`, `readCheckMain` |
| `src/game/read.cpp` | `ReadAreaData`, mit expliziten Änderungen an Staging-Adresse, Fehlerprüfung und erneutem Versuch bei belegtem Hintergrundslot |
| `src/game/datactrl.cpp` | `setCommand`, `getCommand`, `setCondition`, `getCondition`, `checkMallocRelease`, `setMallocInfo`, `fixMramAddr`, `isUseOk`, `isLoadOk`, `setLoadToMram`, `setClear`, `setDelete`, `checkLoadToMram`, `checkCommand`, `cDataCtrl::setData`, `getNewUnit` |
| `src/game/block.cpp` | `cBlock::getBlockWork`: originaler Dateiname `st%x/r%03x_%02x.dat`, Datencontroller-Registrierung, Blocknummer und Flags |
| `src/game/yz2code.cpp`, `src/game/yz2asm.cpp` | Bereits geprüfter Original-C-Setup und der aus PPC-Anweisungen portierte Decoderkern aus Phase 2b, unverändert weiterverwendet |

Die Originalfunktionen werden gegen native, eng begrenzte Strukturen gebunden.
Die Originalgrößen und Offsets von `cDataUnit` in Kommentaren beschreiben PPC und
sind **kein** Serialisierungsvertrag für Android. Pointerfelder wachsen nativ;
Dateioffsets, Flags, Indizes und Größen bleiben explizite Originalskalare.

`observedFunctions()` enthält tatsächlich aufgerufene, instrumentierte
Originaldefinitionen und Adapter. Der Test prüft ihre Präsenz zusätzlich zu den
Hashes. Die Ausgabe ist eine Funktionsmenge, kein behaupteter hardwaregenauer
Frame-Trace und keine Zeitmessung.

## Abgegrenzte Plattformadapter

`port/src/engine_loader.cpp` implementiert:

- `ImportedFiles`: versionierter privater Katalog aus Phase-3a-FST-Einträgen,
  explizite Discwahl, erhaltene Schreibweise/Dateiidentität und ASCII-Caseless-
  Auflösung der im Original verwendeten Pfade; kein pauschales `.das/.drs`-Alias.
- `cDvdQueue::Initialize/Read`: asynchrones Dateilesen, Größen- und SHA-256-Prüfung,
  begrenzte Header-/Archivspannen und native Adresstabellen.
- `cDvd::Watcher/ReadCancel/ReadNblk2Blk/blockRead/ReadCheck/FileExistCheck`:
  native Taskanbindung, sichere Abbruchbestätigung, Polling und explizites
  synchrones Anfordern. Das Dateisystem ersetzt das physische DVD-Laufwerk.
- `decodeData`-Anbindung: besitzender `Yz2Decoder` statt Heap-End-Adressen,
  `0x80350000`-Scratch oder angenommener MRAM-Geometrie. Der Decoder erhält den
  exakt geprüften komprimierten Originalspan und verarbeitet ihn in Slot 4.
- `GetDataExt`: vorhandener BE-/Bounds-Adapter aus Phase 3a mit den originalen
  ersten drei Tagbytes und dem n-ten Vorkommen. Kein `(u32)`-Pointer-Cast.
- `cDataUnit`-MRAM-Heapadapter: 32-Byte-ausgerichtete, erfasste native Allokationen.
  Freigabe fremder Pointer oder doppelte Freigabe wird zurückgewiesen.
- BLK-Registrierung: Bounds-Prüfung des Originalheaders und der 12-Byte-Link-
  Einträge, dann echte `getBlockWork`-/DC-Aufrufe für aktive Blockreferenzen.

Die vollständige Quelldatei `datactrl.cpp` oder `block.cpp` wird ausdrücklich
nicht als vollständig portiert ausgegeben. ARAM-Kommandos und DMA-Übergänge werfen
eine konkrete Unsupported-Fehlermeldung. Es gibt keine erfolgreich meldende
ARAM-Ersatzfunktion und kein vorgetäuschtes `BlockCreate`.

## Warteschlangen, Speicher und Fehler

Die 16 DVD-Slots und 32 Datencontroller-Einheiten bleiben erhalten. Das originale
`LinkQueue` sortiert **kleinere Prioritätswerte zuerst**, mit FIFO bei gleichen
Werten; sein Kommentar behauptet irrtümlich das Gegenteil. Ein laufender Auftrag
wird durch einen neu eingereihten Auftrag nicht verdrängt. Normale DVD-Aufträge
laufen in Slot 3, Mode `0x100` in Slot 4. Yz2 und DVD nutzen denselben exklusiven
Hintergrundslot. Ein belegter Slot lässt den Watcher warten.

Eine Dateioperation pro Engine ist aktiv. Der einzelne Dateiworker besitzt seine
eigenen Puffer und ruft niemals Engine-Funktionen auf. Nach acquire-veröffentlichter
I/O-Fertigstellung prüft der Engine-Owner die Datei in 64-KiB-Schritten, bevor er
Erfolg veröffentlicht. Die regulären Ladeframes warten oder joinen den Worker
nicht. Explizite synchrone Mode-1-Anfragen dürfen blockieren und erhalten eine
bereits laufende asynchrone Queue samt Kontext und Puffern.

Der Adapter liest derzeit eine vollständige Datei und wendet anschließend die
originalen `offset/length`-Spannen an. Das prüft den vollständigen Phase-3a-Hash,
kann bei sehr großen Streamingdateien aber mehr I/O/Speicher benötigen als die
GameCube-Segmentzugriffe. Pro Eingabe und Yz2-Ausgabe gilt die vorhandene
64-MiB-Schutzgrenze. Große SFD-/PCM-Streams erfordern später einen dedizierten
Segmentadapter; sie werden nicht als bereits unterstütztes Movie-/Audiostreaming
ausgegeben.

`readCheckMain` liefert weiterhin 0 während Verarbeitung, 1 bei vollständigem
Erfolg, -4 bei Abbruch und -5 für eine freigegebene/unbekannte Anfrage. Native
Dateifehler verwenden -1; Integritäts-/Bounds-Fehler verwenden den zusätzlichen
Adaptercode -6. Erfolg konsumiert die Queue. Mode `0x40` folgt dem tatsächlichen
Originalcode und gibt die Queue nach Abschluss automatisch frei; die
Originalkommentare dazu sind widersprüchlich. Pending-Abbruch setzt auch das
Fertig-Bit, damit ein Poll nicht dauerhaft an einer ausgehängten Queue wartet.

Native `LoadRequest` enthält Laufzeitidentität, 64-Bit-Generation, Slot und die
originale 1..255-ID. Recycelte oder fremde Referenzen können keinen neuen Auftrag
abbrechen. Bei neu vergebenen Legacy-IDs werden aktive IDs übersprungen.

Die beiden erkannten Dekompilationsprobleme werden explizit korrigiert:

1. Dem Originalüberlad `cDvd::ReadCheck(int,int*,int*,void**)` fehlt der Return;
   der native Adapter gibt das Ergebnis von `readCheckMain` zurück.
2. Der MRAM-Move-Pfad in `setLoadToMram` gibt die Quelle vor `memcpy` frei.
   Die erzeugte Definition kopiert mit `memmove` vor der Freigabe.

Außerdem beendet ein negativer DVD-Poll den `ReadAreaData`-Wartepfad mit einem
Fehler, statt wie im vorliegenden C-Code unbegrenzt auf `ret == 1` zu warten.
Ein besetzter Hintergrundslot wird mit `TaskSleep(1)` erneut versucht.

Stackabbruch ruft wie beim Original keine lokalen C++-Destruktoren auf.
Daher liegen wartende Raum-/Decoder-/Block-Eigentümer im Engine-Kontext bzw.
Task-Capture, nicht als allein besitzende Smartpointer auf einem verwerfbaren
Taskstack. `RoomArchive` hält besitzende Referenzen: Seine Spannen bleiben auch
nach `cancelRoom` gültig, solange ein Verbraucher das Archiv hält. DC-Blockpuffer
und fertige Taskstacks werden trotzdem freigegeben.

Modulcode wird in dieser Phase nicht aktiviert. Die bestehenden Modul-Token-
Tests bleiben erhalten. Vor späterem REL-/Modulwechsel muss der Host
`cancelRoom/cancelAll` ausführen und die vorhandene Scheduler-Lebensdauerprüfung
bestehen; dieser Loader behauptet keine bereits ausgeführte REL-Integration.

## Frame-/VI-Anbindung

`FrameViAdapter` verwendet monotone Nanosekunden und rationale 60-Hz-VI-Ticks.
`systemWorkInit` setzt im Original `SetSystemVcnt(2)`; daraus entstehen 30
Gameplay-Frames pro Sekunde. Die vorhandene Parametrisierung erlaubt auch Vcnt 1,
wie für den späteren Sofdec-Pfad erforderlich. Displayfrequenz und
Gameplaybesuche sind getrennt; 60, 90 und 120 Displaycallbacks ergeben in den
deterministischen Tests jeweils 60 VI-Ticks und 30 Gameplay-Frames.

Ein Ladeframe führt Main-Tasks, DVD-Watcher, den begrenzten Hintergrundslot und
danach die Datencontroller-Zustände aus. Das entspricht dem relevanten Ablauf
in `main.cpp` (TaskScheduler → Dvd.Watcher → iTaskScheduler → DC.check).
Renderer, Audio-Watcher und Block-Modellerzeugung werden dabei nicht simuliert.
Die bestehende kooperative Slot-4-Budgetierung ersetzt die Hardwareunterbrechung;
die Zahl der Ladeframes bis zur Fertigstellung hängt von Budget und I/O ab.
Ein hardwaregenauer DVD-Durchsatz oder dieselbe Fertigstellungs-Framezahl wird
nicht behauptet.

`LoaderFrameLoop.java` bindet reale Android-Choreographer-Callbacks an diese
native API. Alle Calls einschließlich Schließen erfolgen auf dem Owner-/UI-
Thread. JNI verwendet IDs statt auf `int` verkürzter Pointer. `stop/reset`
verwirft Lebenszyklus-Pausenzeit; es gibt kein Gameplay-Nachholen nach Resume.
Die APK hat keine Assets oder automatische Auswahl eines Originaldatenordners.
Der konfigurierte Katalog und die Blobs müssen für den App-Prozess lesbar sein.
Die echte Raumprüfung läuft separat als nativer ADB-Prozess in privatem
`/data/local/tmp`; diese Zugriffsmöglichkeit wird nicht mit App-Sandbox-Zugriff
gleichgesetzt.

## Englische MDT-Ressourcen

Die Phase-3a-Dateien werden nicht verändert. Die Pipeline erzeugt privat
`english-mdt-reachability.json` mit allen 35 unvollständigen englischen Blöcken,
Dateiidentitäten, Entry-Indizes, Hashes und statischen Referenzen.

Nachgewiesene Zugriffskette:

- `ReadAreaData` bestimmt den ersten Raum-`MDT` mit `GetDataExt`.
- `MessageControl::roomInit` registriert `pG->RoomMes` als MessageData-Typ 1.
- `MessageData::getAddr/getMesNum` dereferenziert den Sprachblock. Die originale
  englische Auswahl ist Index 1, also BE-Headerwort bei Offset 8.
- Events können durch `Event::ExePacket_SetMdt` einen benannten Bin-Eintrag mit
  `EvtMgr.GetBin` als Typ 1 registrieren. `MessageControl::MesSet` benötigt später
  tatsächlich eine Nachricht und ruft `getAddr` auf.

Eine Registrierung ist noch kein Nachrichtenzugriff. Der Bounds-Adapter
`englishMessageCount/englishMessage` prüft diesen Zugriff erst bei Bedarf und
interpretiert weder Kontrollcodes noch erfundene Textterminatoren. Die 32-Byte-
MDTs mit Blockoffset 0x1c reichen nicht für den acht Byte großen Blockheader.
Sie bleiben erhalten und werden als `open` ausgewiesen. Keine Ersatztexte,
keine andere Sprache als stiller Fallback. Die konkrete Gameplay-/Event-
Erreichbarkeit einzelner Nachrichten ist ohne laufende Originalevents weiter
offen. Alle gefundenen Raum-MDTs werden beim nativen Archivtest zusätzlich gegen
die Phase-3a-Sprachstatus-/Count-Referenzen geprüft.

## Reproduzierbare Tests und private Nachweise

`re4_loader_tool self-test DIR` verwendet ausdrücklich synthetische Adapterdaten.
Geprüft werden SHA-256-Referenzvektoren/Chunks, Disc-/Pfadauflösung, reale
Dateiworker, Original-Queueprioritäten/FIFO, Gameplay während I/O, Slot 3/4,
Pending-/Active-Abbruch, Suspend/Resume, Worker-Barriere ohne Join beim Abbruch,
Neuanforderung, 16-Slot-Grenze, mehr als 255 ID-Recyclings, fremde Generationen,
Range-/Overflow-Prüfung, leere Datei, Fehler statt Endlosschleife, explizite
Synchronität einschließlich paralleler bestehender Queue, automatisches
Freigabeflag, originale FileTbl-Nummer, MDT-Bounds und Display-/Gameplay-Takt.

`re4_loader_tool rooms ROOT CATALOG DISC HEX_ID...` verwendet echte Originaldaten.
Es startet `ReadAreaData` als stackerhaltenden Main-Task, lässt einen zweiten
Gameplay-Testtask während des Ladens schlafen/fortsetzen und wartet auf den
kompletten Engine-Zustand. Es kontrolliert Raw-/Yz2-Hashes, alle Tag-Indizes,
Offsets, Größen und Vorkommen, Blockdateien, englische MDTs, besitzende
Archivlebensdauer, Taskstack-/MRAM-Freigabe sowie Abbruch während Lesen,
Dekomprimieren und Blockladen. Keine Positionen oder Gameplayobjekte werden
erzeugt. Isolierte Adaptertests und reale Datenprüfungen bleiben erkennbar getrennt.

Der private Katalog umfasst alle **2.244 FST-Dateieinträge**. Alle **142
Raumarchive**, 85 auf Disc 0 und 57 auf Disc 1, werden als generische Fälle aus
`rooms/*.json` abgeleitet. Die fokussierte Folge lautet auf Disc 0
`120,100,101,120,100`; auf Disc 1 `400,405,402,400,405`.

Reproduktionsbefehle ab Repositoryroot, nach dem vorhandenen Host-/NDK-Build:

```powershell
$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
& $taskPython port/tools/test_engine_loading.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase3b --host-exe port/build/host-ninja/re4_loader_tool.exe --repeat 2
& $taskPython port/tools/test_engine_loading.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase3b --host-exe port/build/host-x86/re4_loader_tool.exe --repeat 2
& $taskPython port/tools/test_engine_loading.py --data E:\Development\Android\RE4-Data\import-phase3a --output E:\Development\Android\RE4-Data\phase3b-all-device --adb C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe --serial dbdcacc --all-rooms --repeat 2
.\port\scripts\test-device.ps1 -Adb C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe -Serial dbdcacc
```

Der breite Hosttest verwendet `--all-rooms --repeat 1` und das getrennte
Ausgabeverzeichnis `phase3b-all-host`. Die Python-Hilfe dokumentiert sämtliche
Optionen. Alle Berichte/Kataloge/Originalbytes liegen außerhalb von Git unter
`E:\Development\Android\RE4-Data`. Keine Images oder Assets gelangen in die APK.
Die Berichte enthalten EXE-/Katalog-Hashes, ABI-Nachweis, tatsächliche
Funktionsbeobachtungen, vollständige Archivresultate und normalisierte Ergebnis-
Hashes. Unterschiedliche I/O-Framezahlen sind absichtlich kein Gleichheitskriterium.

## Ausgeführte Prüfungen

| Ziel | Tatsächlicher Nachweis |
|---|---|
| Windows MSVC x64 und x86 | Je 18/18 CTests erfolgreich; einschließlich 597 isolierter Loaderprüfungen und 14 Originaldatei-Negativproben |
| Windows x64 und x86 | Je zweimal die 10 fokussierten Raumladungen und einmal sämtliche 142 Raumarchive |
| Retroid Pocket Flip2, USB `dbdcacc`, Android 13 / API 33 | Unterstützte ABIs mit ADB geprüft: `arm64-v8a,armeabi-v7a,armeabi` |
| Retroid `armeabi-v7a` | Zweimal 142 Archive und zweimal die fokussierten 10 Raumladungen, tatsächlicher 32-Bit-Prozess, je Runde 597 isolierte Loaderprüfungen |
| Retroid `arm64-v8a` | Derselbe Testumfang, tatsächlicher 64-Bit-Prozess, 597 Loaderprüfungen |
| Retroid bisherige native Suiten | Zweimal pro ABI erfolgreich: ARM32 13.386, ARM64 13.387 Prüfungen; identische sieben semantische Traces |
| APK / APK-Testpaket | Beide ABIs gebaut, installiert und jeweils 14/14 Instrumentierungstests bestanden; zwei neue Frame-/Choreographer-Tests |
| Gradle | `assembleDebug`, `assembleDebugAndroidTest`, `lintDebug` erfolgreich, offline mit vorhandenen Tools |

ARM64 hat in der bisherigen Scheduler-Suite einen zusätzlichen Test für Pointer
oberhalb von 4 GiB. Die 597 neuen isolierten Loaderprüfungen sind auf allen vier
Architekturen gleich gezählt. Die breiten Raumtests weisen zusätzlich denselben
MDT-Status nach: 116 vorhandene englische Raumblöcke, ein leerer und 25
unvollständige. Die übrigen zehn der 35 unvollständigen Instanzen gehören zu
Eventarchiven; deren konkrete Eventausführung wurde nicht getestet.

Die breiten normalisierten Ergebnisse sind pro Disc auf allen vier Architekturen
identisch (Hashes umfassen reale Daten-/Index-/Block-/MDT-Ergebnisse und beobachtete
Funktionsmengen):

```text
Disc 0: aeaf6ab550bed60757e2d6c8055bd8abcea720c23392de7adf76ad5238df00ee
Disc 1: c93a22b69cfe04b7e253fd08b1c5304b5635f3d5530d057cca237acb3d6932d4
```

Ergebnisdateien: `phase3b/host-32-engine-tests.json`,
`host-64-engine-tests.json`, `android-engine-tests.json` für die fokussierten
Folgen; `phase3b-all-host/host-32-engine-tests.json` und
`host-64-engine-tests.json` für die breite Hostprüfung;
`phase3b-all-device/android-engine-tests.json` für die wiederholte Geräteprüfung.
Die Dateien nennen den Hash des konkret ausgeführten Testprogramms. ADB prüft
übertragene Kataloge, Originaldateien und Executables zusätzlich mit SHA-256.
`summarize_engine_tests.py` validiert die aktuell gebauten EXE-Hashes gegen diese
Berichte, prüft jedes Ergebnis erneut gegen seine Fälle und vergleicht alle
Architekturen. Der private `phase3b/verification-summary.json` fasst 80 fokussierte
und 852 breite, insgesamt **932 erfolgreiche Raumladungen** zusammen. Zusätzliche
abgebrochene Probeanforderungen sind nicht in dieser Erfolgszahl enthalten.

```powershell
& $taskPython port/tools/summarize_engine_tests.py --focused E:\Development\Android\RE4-Data\phase3b --all-host E:\Development\Android\RE4-Data\phase3b-all-host --all-device E:\Development\Android\RE4-Data\phase3b-all-device
```

Während der Entwicklung schlugen Compilerprüfungen wegen explizit zu korrigierender
int/u8-Konvertierung, MSVC-CRT-Diagnosen und unzugänglicher Returns nach dem
Fehlerlogger fehl. Der erste isolierte Ladetest versuchte mehrere Scheduler auf
demselben Owner-Thread gleichzeitig zu konstruieren; er wurde auf getrennte
Lebensdauern korrigiert. Keine dieser Prüfungen ist als Erfolg gezählt. Alle oben
genannten finalen Prüfungen bestanden. Die bekannte SDK-XML-Version-3/4-Warnung
bleibt ohne Build-/Lintfehler bestehen.

## Geänderte Dateien

Neu:

- `port/include/re4/port/engine_loader.h`, `sha256.h`
- `port/src/engine_loader.cpp`, `loader_original.h`, `sha256.cpp`
- `port/cmake/LoaderSources.cmake`
- `port/tests/loader_tool.cpp`
- `port/tools/test_engine_loading.py`
- `port/tools/summarize_engine_tests.py`
- `port/platform/android/loader_jni.cpp`
- `port/android/app/src/main/java/org/re4/port/LoaderFrameLoop.java`
- `port/android/app/src/androidTest/java/org/re4/port/LoaderFrameTest.java`
- dieser Bericht

Erweitert:

- `port/CMakeLists.txt`: Loaderbibliothek, Testtreiber/CTest, JNI-Verknüpfung
- `port/cmake/OriginalSources.cmake`: zusätzliche Fingerprints und Erzeugung
- `port/tests/source_guard_negative.cmake`: saubere Basiskopie und 14 Negativproben
- `port/scripts/test-app-device.ps1`: 14 statt 12 tatsächlich erwartete Tests
- `port/README.md`: Phase-3b-Einstieg und Grenzen

## Verbleibende Arbeiten vor vollständiger Engine-Nutzung

1. Native MRAM-/ARAM-Adressdomänen und der originale ARAM-Allocator/DMA-Verbund,
   einschließlich `checkAramSort` und vollständigem DC-Zustandsautomaten.
   Gegenwärtige ARAM-Aufrufe werden ausdrücklich abgewiesen.
2. Speicher-/FileTbl-Anbindung der übrigen Verbraucher, Heaplevels und
   DLL-abhängige `.drs`-Umschaltung anhand tatsächlich aktivierter Module.
3. SND_BLK-/PCM-Registrierung und gestreamte Segmente; das vollständige rohe
   Soundbank-Bytebild ist bereits erhalten, Audioinitialisierung/-ausgabe fehlt.
4. Originale `cBlock::roomInit/check`, Modellpool, Sichtbarkeits-/Area-Steuerung
   und Modell-/Kollisionsdateninitialisierung. Der heutige Nachweis lädt alle
   aktiven Originalblockdateien zur Prüfung; er simuliert keine Spielerposition.
5. REL-/native Modulaktivierung, Resource-Initializer und tatsächlich laufende
   Raum-/Eventcallbacks, dann dynamischer Nachweis benötigter MDT-Nachrichten.
6. Größere FileRead-Segmente und weniger Kopien für Streaming-/Spitzenbedarf.
   ASan/UBSan mit verifizierten Fiber-Hooks sind weiterhin nicht verfügbar;
   getestete Bounds-/Generation-/Ownership-Prüfungen ersetzen diesen Nachweis
   nicht. Kein hardwaregenauer Retrace-/DVD-Timing-Nachweis.

Die dokumentierten Grafikoptionen bleiben verbindlich: MSAA Aus/2×/4×,
anisotrope Filterung 2×/4×/8×, DoF Ein/Aus mit Nah-/Zielfokus, persistente und
hardwarevalidierte Einstellung, backendunabhängiges Menü. Keine dieser
Renderingfunktionen wird durch Phase 3b als bereits implementiert ausgegeben.
Phase 4 wurde nicht begonnen.
