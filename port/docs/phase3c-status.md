# Phase 3c – MRAM/ARAM und gemeinsame Ressourcenlaufzeit

Stand: 9. Oktober 2026. Grundlage: `1eace9c55eb6f8a757852a15064921ac1e7ce22d`.
Implementierung ausschließlich auf `android-port`, ausschließlich unter `port/`.
Keine Originaldateien oder GameCube-Images verändert, keine Assets in Git.

Die Datencontroller-Zustände laufen jetzt mit tatsächlichen MRAM-Reservierungen,
einem begrenzten nativen ARAM-Bereich und kontrollierten Transfers. Echte
Originaldaten sind auf Windows x86/x64 und Android ARM32/ARM64 geprüft. Dies ist
eine Ressourcenlaufzeit, weiterhin kein spielbares RE4 und kein vollständiger
Modell-, Sound-, Heap- oder REL-Verbraucher.

## Ausgangslage und Git

Der Arbeitsbaum war sauber. `master` bleibt bei
`9b76ff003081d6ece8b838453fb8310743a972c7`; nach `git fetch origin` steht
`origin/master` weiterhin bei `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die Differenz zwischen diesen Referenzen betrifft nur 25 neue README-Zeilen.
Die README zu Architektur/Zielen wurde gelesen und in der Port-Dokumentation
berücksichtigt. Ein Merge ist für diese Implementierung nicht erforderlich.
Es gab keinen Merge, Rebase, Push oder Wechsel von `master`.

Die Entwicklungshistorie bleibt erhalten: Phase 1 `ec1b162a`, Phase 2 `3c2dcfbc`,
Phase 2b `31940b6e`, Phase 3a `50b33b56`, Phase 3b `1eace9c5`.
Der erfolgreiche Ausgangsstand bestand zu Beginn erneut alle 18 CTest-Gruppen.

## Tatsächliche Originalcode-Abdeckung

`cmake/LoaderSources.cmake` extrahiert vollständige, fingerprintgeschützte
Definitionen aus den Originaldateien. Explizite ABI-/Sicherheitsänderungen stehen
dort zentral; `#line` verweist weiterhin auf die Originaldefinition. Die
Originaldateien selbst bleiben unverändert.

Aus `src/game/datactrl.cpp` laufen jetzt zusätzlich zu Phase 3b:

- `cDataUnit::setLoadToAram`, `checkLoadToAram`, `checkAramToMram`,
  `checkMramToAram`, `checkAramToAram`, `checkMramToMram`, `checkCondition`,
  `waitUseOk`, `waitLoadOk`;
- `cDataCtrl::getAramFree`, `initDataUnit`, `deleteAll`, `setAramSort`,
  `checkAramSort`, `initDummyId`, `setDummyId`, `checkDummyId`, `check`.

Die bestehenden `setCommand/checkCommand`, MRAM-Lade-/Freigabefunktionen,
`setData/getNewUnit`, `isUseOk/isLoadOk` und Zustandszugriffe bleiben erhalten.
Der Frame arbeitet mit dem vollständigen `DC.check` über alle 32 Datenunits,
statt ausschließlich Raumblock-Units einzeln zu pollen. Die Controllerflags
für deaktivierte Verarbeitung und gehaltene Kommandos werden berücksichtigt.

Aus `src/game/dvd.cpp` sind jetzt auch `cAram::DmaTransReq`, `pullAramQueue`
und `TransCheck` mit expliziter ABI-/Boundsanpassung kompiliert. FIFO, 16 Slots,
In-use-/Done-Flags und konsumierender Fertigstellungspoll stammen aus dem
Original. `cAram::DmaTrans/DmaCancel/DmaCancelAll` und ARQ-Callback/Hardware sind
klar getrennte native Adapter, keine behauptete Dolphin-Implementierung.

Aus `src/game/block.cpp` laufen jetzt `cBlockUnit::setBlockCommand`,
`setBlockLoadToMram`, `checkBlockLoadToMramSet`, `checkBlockLoadToAramSet`,
`checkBlockLoadToAram`, `checkBlockDelete`, zusätzlich zum bisherigen
`cBlock::getBlockWork`.

`src/game/read.cpp:ReadAreaData`, der bestehende Original-DVD-Anforderungspfad,
die originalen Yz2-Setupfunktionen und der geprüfte Decoder bleiben eingebunden.
`decodeData/GetDataExt`, Dateizugriff, Besitzregeln, Cache und BE-Bounds sind
native Adapter; ihre Bezeichnungen bedeuten nicht, dass die gesamten
GameCube-Quellcodedateien unverändert kompiliert werden.

Nicht kompiliert sind `cDataCtrl::init/dispDebug` mit Debugdarstellung sowie
modellabhängige Blockfunktionen und die gesamte Dolphin-Heapverwaltung aus
`main_mem.cpp`. Developer-Console-Dummyfunktionen sind kompiliert; die reale
Laufzeit bleibt wie zuvor bei `IsDevConsole == 0`, ohne behauptete Prüfung eines
kompletten Debug-Console-Modus.

## Zustände, Transfers und Reihenfolge

`include/re4/port/resources.h:ResourceState` verwendet die Originalwerte:

| Originalzustand | Wert | Native Bedeutung |
| --- | --- | --- |
| NO_DATA | 0 | Registriert, keine veröffentlichte Residenz |
| MRAM_LOAD | 1 | Original-DVD-Anforderung aktiv |
| MRAM_OK | 2 | MRAM-Puffer vollständig nutzbar |
| ARAM_LOAD | 3 | DVD-Daten und anschließender ARAM-Transfer noch nicht abgeschlossen |
| ARAM_OK | 4 | Vollständige Ressource im ARAM-Cache |
| ARAM_TO_MRAM | 5 | Transfer in reserviertes MRAM läuft |
| MRAM_TO_ARAM | 6 | Transfer läuft, MRAM-Quelle bleibt bis Abschluss erhalten |
| ARAM_TO_ARAM | 7 | Original-Zweistufenpfad über MRAM-Staging |
| MRAM_TO_MRAM | 8 | Originalhandler ist leer; der reale MRAM-Move erfolgt synchron in `setLoadToMram` |

Belegte Zustandsfolgen sind `0→1→2`, `2→6→4`, `4→5→2`, `0→3→4` und
`4→7→6→4`. MRAM-Move, Clear/Delete, explizite Waits, Kommandopause, Abbruch und
erneuter Start sind geprüft. Die Native-API erfindet keinen zusätzlichen
erfolgreichen Zustand für einen fehlenden Verbraucher.

Der Datenbereich folgt tatsächlichen Quellen:
`src/game/snd.cpp:SndInit` setzt `ARAM_FREE_BASE = 0x800000`;
`datactrl.cpp` definiert `ARAM_END = 0xD00000`. Die normale native Datenarena
umfasst damit 5 MiB, getrennt von MRAM. Der Soundbereich unterhalb dieser Basis
wird nicht als fertig portierte Audioverwaltung ausgegeben.

`src/resource_runtime.cpp` ersetzt Hardware-DMA durch begrenzte CPU-Kopien auf
dem Engine-Ownerthread. Standardbudget: 64 KiB pro Frame, konfigurierbar. Eine
Anforderung bleibt bis zur tatsächlichen letzten Kopie pending; nur dann wird
das Done-Bit gesetzt. Es gibt einen aktuellen Transfer und eine FIFO-Liste.
Die einzige vorhandene I/O-Workerthread liest weiterhin ausschließlich in
eigene Puffer; er verändert weder Datencontroller noch ARAM/MRAM-Zielpuffer.

Die Frame-Reihenfolge bleibt Main-Tasks → DVD-Watcher → begrenzter Slot 4 →
Transferfortschritt → Blockkommandos → `DC.check` → Block-Datenbedingungen.
Das ist der ressourcenbezogene Ausschnitt des Originals, kein vollständiger
Render-/Sound-/Gameplayframe. Explizite Original-Waits dürfen blockieren und
fördern dann ihren I/O-/Transferpfad; gewöhnliche Frames warten nicht auf alle
Transfers. Android-Displaytakt und 30-Hz-Gameplaytakt bleiben über den
vorhandenen VI-Adapter getrennt.

Die Transferdauer entspricht nicht der GameCube-DMA-Dauer. Es werden keine
Hardwareinterrupts nachgebildet. Nachgewiesen sind Reihenfolge, Zustandsfolgen,
Bereitschaft, Daten und Besitzregeln; ein framegenauer Vergleich gegen eine
ausgeführte GameCube-Hardwaremessung wurde nicht durchgeführt.

## Speicher, Cache und Referenzen

MRAM wird 32 Byte ausgerichtet reserviert. Der Standard-Gesamtbetrag für diesen
Allocator ist 64 MiB und berücksichtigt auch bereits aus einer Unit entfernte,
aber noch durch Leases gehaltene Reservierungen. Allocationfehler werden als
Fehler sichtbar; ein nicht vorhandener Puffer gilt nicht als geladen.

ARAM-Adressen bleiben logische 32-Bit-Offsets. Originalfelder, die ARAM als
`void*` transportieren, werden nur über geprüfte Offsetadapter verwendet und
niemals dereferenziert. DMA-Adapter erhalten MRAM-Adressen als `uintptr_t`;
native Pointer werden nicht nach `u32/int` verkürzt. Explizite ARAM-Ziele werden
gegen Grenzen, Alignment und fremde Ressourcenreservierungen geprüft.
Logische Nutzdaten werden kopiert; DMA-Padding wird separat reserviert und
genullt, ohne hinter einem ungeraden Dateiende zu lesen.

Die Disc-Dateien und Archivfelder bleiben unveränderte Big-Endian-Bytes.
Nur kontrollierte Scalar-/Indexleser interpretieren sie. Native
`cDataUnit`-/Queue-Layouts sind keine serialisierten PPC-Layouts. IDs, Pfade,
Disc-Zugehörigkeit, Archivindizes, Occurrences und Originalreferenzen werden
nicht durch native Adressen ersetzt.

`ResourceHandle` enthält Runtime, Slot und Generation. `ResourceReference`
ergänzt Offset/Größe. Ein Reference lässt sich nach MRAM-Rückladen oder Move
erneut auflösen; während ARAM-Residenz oder einer laufenden Übertragung wird
MRAM-Zugriff abgelehnt. Ein `ResourceLease` hält die frühere Reservierung bei
Clear/Move am Leben. Delete/Slotwiederverwendung invalidiert alte Handles.
`releaseMemory` lehnt die Freigabe einer noch von einer Datenunit referenzierten
externen Reservierung ab. Besitzfreigaben und Framezustände erfolgen auf dem
Ownerthread; der Besitz-Bytezähler ist atomar.

Es gibt zwei begrenzte LRU-Caches: 16 MiB für verifizierte Originaldateien und
16 MiB für dekodierte Raumkörper. Originaldateien werden beim ersten Lesen
gegen den Phase-3a-SHA-256 geprüft. Cachehits verwenden diese bereits geprüften
unveränderlichen Bytes und behalten Disc-/Pfad-/Range-/Headerkontext bei.
Die CAS-Eingabe muss unveränderlich bleiben; es wird nicht bei jedem Cachehit
eine geänderte Festplattendatei erneut geöffnet.

Der I/O-Puffer wird nach erfolgreichem Hashcheck in `LoadedFile` verschoben.
Der Yz2-Raumkörper wird über `sharedOutput` ohne zweite Ergebniskopie geteilt;
Decoderwork und komprimiertes Staging werden anschließend freigegeben. Auch
`RoomArchive::blocks` teilen die bereits geprüfte Datei, statt aus MRAM eine
weitere Archivkopie anzulegen. MRAM bleibt für den Original-Datenconsumer eine
eigene veränderbare Reservierung. Cacheeviktion/`trimCaches` entfernt
Cachebesitz; lebende Archive/Leases bleiben gültig.

Cachelimits sind **keine Gesamt-RSS-Grenze**: aktuelle Archive, Verbraucher,
I/O, Decoderstaging und externe Besitzer können zusätzlich Speicher halten.
Ein vollständiges Android-Speicherprofil unter späterem Gameplay ist offen.
Die ARAM-Backingarena wird beim ersten Transfer angelegt und bis zum Ende der
Laufzeit behalten. `memoryStats` zeigt getrennte MRAM-/Cache-/ARAMbeträge und
Transferzahlen; nach den geprüften Clear/Cancel-Sequenzen sind MRAM und aktive
Transfers vollständig freigegeben.

## Explizite Original-/Plattformkorrekturen

Zusätzlich zu den dokumentierten Phase-3b-Anpassungen:

1. Die ARAM-Raumtabelle benötigt bis zu 64 Einträge, weil jede der 32 Units bei
   Zustand 7 zwei Bereiche reserviert. Die Original-Hilfstabelle mit 32
   Einträgen wird nativ sicher erweitert. 36 gleichzeitig reservierte Spannen
   sind tatsächlich getestet.
2. Ausrichtung/Größenaddition werden vor Überläufen geprüft; ein leerer ARAM-
   Allocator akzeptiert keine Ressource größer als die gesamte Arena.
3. Der Original-Abbruch durchsucht nur `p->next` und übersieht den Listenanfang.
   Der native Abbruch entfernt auch den Kopf sowie den aktuellen CPU-Transfer,
   bevor eine Quelle freigegeben werden kann. Es gibt keinen noch schreibenden
   Hardwarevorgang, der ein erzwungenes Thread-Suspend erfordernte.
4. Eine volle Transferqueue darf die noch gültige MRAM-Quelle nicht freigeben.
   Der negative Original-Submitpfad wird hierfür abgesichert; dieser Fall ist
   mit 17 Anfragen auf 16 Slots getestet.
5. Eine bereits ARAM-residente Unit ohne neues Ziel konsumiert ihr redundantes
   Loadkommando. Fehler werden nativ pro Unit gehalten, statt andere Units durch
   ein HALT oder wiederholte Fehlerpolls zu stranden.
6. Wiederverwendete native Units werden vollständig initialisiert; alte fixe
   Adressen/Wait-/Allocatorfelder dürfen nicht in eine neue Generation gelangen.
7. Datenbereitschaft wird nach dem ersten Blockcontroller-Schritt gepollt. Die
   Raumadapter-Warteschleife schläft davor einmal, statt eine noch nicht gestartete
   Unit als Originalfehler 5 zu behandeln.

Diese Änderungen sind ABI-, Speicher- und kontrollierte Fehlerpfadkorrekturen.
Es gibt keine neu erfundenen Raum-, Gegner-, Waffen- oder Eventregeln.

## Block-Datenbereitschaft und Grenzen

Die fünf echten Blockdateien von `r100` werden durch den Originalpfad
`getBlockWork → DC.setData` registriert. Aktivierung bedeutet hier vollständige
MRAM-Daten. Der originale Blockzustand bleibt dabei `BLOCK_MRAM_LOAD` mit
`COND_MRAM_OK`; `BLOCK_CREATE` wird nicht vorgetäuscht.

`resourceBlockCommands/resourceBlockConditions` bilden die explizite
Daten-only-Grenze. Sie benutzen die oben aufgeführten Original-Load-/Check-
Funktionen und ermöglichen ARAM-Cache, Rückladen und Clear. Die modellabhängigen
`BlockCreate/BlockDestroy`, Sichtbarkeit, `recalcModelAddr`, `cSmd::slide`,
`checkBlockMemSort`, Bereichstrigger und Stopflags des gesamten Gameplay sind
noch nicht implementiert. Der native Pfad verwendet Einzelreservierungen wie
der originale `noMemCtrl == 1`-Pfad. Ein Aufruf des noch ungebundenen Modellpools
scheitert ausdrücklich; es wird kein Dummy-Speicher zurückgegeben.

## Verifikation und tatsächliche Testabdeckung

Privater Eingang: `E:\Development\Android\RE4-Data\import-phase3a`.
Neue Berichte liegen ausschließlich in `RE4-Data\phase3c`,
`phase3c-all-host` und `phase3c-all-device`. Die historischen Phase-3a-/3b-
Berichte werden nicht überschrieben. Androiddaten liegen in jeweils neu
angelegten `/data/local/tmp/re4-resources-*`, `re4-engine-*` und
`re4-port-import/*`-Verzeichnissen; keine Assets befinden sich in der APK.

Das Gerät ist `dbdcacc`, **Retroid Pocket Flip2**, Android **13**, API **33**.
ADB meldet `arm64-v8a`, `armeabi-v7a`, `armeabi`. Die tatsächlich gestarteten
Programme melden 32 beziehungsweise 64 Bit, die APKtests erzwingen getrennte
Installations-ABIs. Ein vorhandenes ARM32-Laufzeitsystem ist hier nachgewiesen.

| Prüfung | Host x86 | Host x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| CTest-Gruppen | 19/19 | 19/19 | nicht CTest; identische native Programme | nicht CTest; identische native Programme |
| Isolierte Controller-/Speicherchecks | 63 je Lauf | 63 je Lauf | 63 je Lauf | 63 je Lauf |
| Neue echte Ressourcencases, 2 Runden | 616 | 616 | 616 | 616 |
| Neue Raum-/Cachecases, 2 Runden | 284 | 284 | 284 | 284 |
| Bisherige vollständige Engine-Ladetests | 142 | 142 | 284 | 284 |
| Vollständiger Import-/Yz2-Korpus | 758 | 758 | 2×758 | 2×758 |
| Bestehende Core-/Schedulertests, 2 Runden | durch CTest abgedeckt | durch CTest abgedeckt | 2×13.386 | 2×13.387 |
| Debug-APK-Instrumentierung | – | – | 14/14 | 14/14 |

Die neuen resource-/room-Cases sind 3.600 Prüfungen über alle vier Plattformen.
Die gemeinsame Abschlussprüfung bestätigt zusätzlich 852 bestehende Engine-
Ladefälle und 4.548 Import-/Decoderfälle: **9.000 erfolgreiche Korpusfälle,
0 fehlgeschlagen**. Der vollständige Android-Archiv-/Decodertrace ist auf beiden
ABIs und beiden Wiederholungen
`d3bead1d4b296186cdb2e7b1f2075019c7c54217437da2455274884cad5741a8`.
Ein Raum-/Cachecase lädt den Raum tatsächlich zweimal, prüft gleiche Bodybytes
und Wiederverwendung des gemeinsamen dekodierten Körpers. Die 308 ausgewählten
Dateiinstanzen entsprechen 164 einzigartigen Dateien. Die Auswahl entsteht aus
dem echten Inventar und den tatsächlichen Blockreferenzen, ohne feste Raumregeln.

Darunter sind `.bnr/.das/.dat/.drs/.eff/.evd/.fnt/.hed/.rel/.sfd/.str/.sym/.tpl/
.txt/.uwf`. Dies ist **Datenresidenz-/Integritätsabdeckung**, kein Nachweis eines
portierten Interpreters oder Players für jede Endung. Große Dateien oberhalb der
5-MiB-ARAM-Datenarena und Nullgrößen werden von dieser Auswahl ausgeschlossen;
Datei-/Archivgrenzen und Nullgrößen sind separat im bestehenden Loaderkorpus
abgedeckt. Streaming großer Videos/Audio ist weiterhin eine spätere Aufgabe.

Die automatischen Controllerprüfungen umfassen echte Original-Zustandsfunktionen
gegen synthetische Dateimuster: asynchrones MRAM/ARAM, Waits, Rückladen,
Zweistufen-Repack, MRAM-Move, gepinnte Leases, 16-Slot-Queue, Kopfabbruch,
Partialabbruch, Gameplayparallelität, Kommandopause, Generation, Fehlerpersistenz,
Alignment, Überlappung, Überlauf, Speichergrenzen und 36 Reservierungsspannen.
Sie sind ausdrücklich isolierte Originalcontroller-/Adaptertests und kein
erfundenes Gameplay.

Die echten Prüfungen vergleichen jede ausgewählte Ressource nach Umlagerung und
Abbruch/Reload mit dem Inventar-SHA. Alle 142 Raumarchive werden zusätzlich über
den bisherigen Enginepfad gegen vollständige Archivindizes, Größen, Blockhashes
und English-MDT-Referenzen geprüft. 35 unvollständige englische MDT-Blöcke bleiben
unverändert; tatsächlicher Message-/Eventzugriff wird weiterhin nicht behauptet.

Die neuen normalisierten Ausgaben sind auf allen ABIs und Wiederholungen gleich:

| Discnummer im Image | Ressourcen-Trace SHA-256 | Raum-/Cache-Trace SHA-256 |
| --- | --- | --- |
| 0 | `b9bc40a39a586d52e19dab59153fa15b0382a18588b0f0fdeb979d41958a4e05` | `917ff3199b36b042374adf2f664ec76868793155e722369c6d23baa39df1d243` |
| 1 | `5e9a0cc368466580e77001d3d34507d9c237fefe1e2df6421350922dcc492849` | `a69131df5006d521fbda52aaf2b2f8b7cc54cc2dda9e6f0661d9e8d3b5b3c275` |

Zeit-/Pointerwerte und Cachehit-Messwerte sind keine zugesicherte identische
Hardwarelaufzeit. Der normale Corekorpus hat unter ARM64 eine zusätzliche
High-Pointer-Prüfung; daher 13.387 statt 13.386. Die 63 Ressourcenprüfungen und
ihre relevanten Zustandsfolgen unterscheiden sich nicht.

### Während der Entwicklung behobene Fehlschläge

MSVC `/W4 /WX` beanstandete die originale `int→u8`-Blockargumentzuweisung und
unerreichbaren Code hinter einer inline geworfenen Modellpool-Ausnahme.
Argumentgrenzen/Cast wurden explizit gemacht, die noch offene Poolgrenze in
eine eigene native Definition verschoben. Der erste echte Blocktest meldete
Fehler 5 vor dem ersten Controllerbesuch; die oben beschriebene Reihenfolge ist
jetzt durch echte Räume geprüft. Ein direkt im Sandboxkontext gestarteter
Fixturetest scheiterte an Dateizugriff; die autorisierten Builds/Testläufe
außerhalb dieser Sandboxgrenze bestehen. Dies waren keine erfolgreich
ausgegebenen Platzhaltertests.

Der Android-Build besteht für beide NDK-ABIs sowie Debug-APK,
AndroidTest-APK und `lintDebug`. Die bekannte SDK-XML-3/4-Warnung bleibt
nichtfatal. ASan/UBSan bleibt wegen der noch fehlenden verifizierten Fiberhooks
gesperrt; es wird kein ausgeführter Sanitizerlauf behauptet.

## Reproduzierbare Befehle

Werkzeuge: MSVC 19.51, Ninja, CMake; Android NDK `28.2.13676358`, SDK-CMake
`4.1.2`, Gradle `8.13`, AGP `8.13.2`, JBR 21. `python` muss eine reale
Python-3.12-Installation sein; hier wurde der gebündelte Runtimepfad verwendet.

```powershell
# Host: jeweils aus dem passenden VS x64-/x86-Developer-Terminal
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-x86 --output-on-failure

.\port\scripts\build-android.ps1 -Offline `
  -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' `
  -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr'

python port/tools/test_resources.py `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --output 'E:\Development\Android\RE4-Data\phase3c' `
  --host-exe port/build/host-ninja/re4_resource_tool.exe --repeat 2
# Derselbe Aufruf für port/build/host-x86/re4_resource_tool.exe
python port/tools/test_resources.py `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --output 'E:\Development\Android\RE4-Data\phase3c' `
  --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' `
  --serial dbdcacc --repeat 2

python port/tools/test_engine_loading.py `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --output 'E:\Development\Android\RE4-Data\phase3c-all-host' `
  --host-exe port/build/host-ninja/re4_loader_tool.exe --all-rooms --repeat 1
# x86 entsprechend; Android mit --adb/--serial, eigenem
# --output ...\phase3c-all-device und --repeat 2.

python port/tools/import_original.py verify-native `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --native-tool port/build/host-ninja/re4_asset_tool.exe `
  --report 'E:\Development\Android\RE4-Data\phase3c\host-x64-import-tests.json'
# x86 entsprechend mit --report ...\host-x86-import-tests.json
python port/tools/test_import_device.py `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --output 'E:\Development\Android\RE4-Data\phase3c' `
  --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' `
  --serial dbdcacc --repeat 2

.\port\scripts\test-device.ps1 -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Serial dbdcacc
python port/tools/summarize_resource_tests.py `
  --resources 'E:\Development\Android\RE4-Data\phase3c' `
  --engine-host 'E:\Development\Android\RE4-Data\phase3c-all-host' `
  --engine-device 'E:\Development\Android\RE4-Data\phase3c-all-device'
```

Driver und Zusammenfassung prüfen reale Prozess-ABIs, aktuelle Binärhashes,
Kataloghashes, die Referenzen und semantische Wiederholungs-/ABIgleichheit.
`resource-coverage.csv` und `verification-summary.json` entstehen automatisch
im privaten Berichtverzeichnis. Ein neuer Build darf nicht ungeprüft alte
Berichte als seinen Nachweis verwenden.

## Vollständige geänderte Projektdateien

Alle Änderungen liegen als vollständige Dateien im Projekt:

- `port/CMakeLists.txt` und `port/cmake/LoaderSources.cmake`;
- `port/include/re4/port/resources.h`, `engine_loader.h`, `async_file.h`,
  `yz2.h`, `contracts.h`;
- `port/src/resource_runtime.cpp`, `engine_loader.cpp`, `loader_original.h`,
  `async_file.cpp`, `yz2.cpp`;
- `port/tests/resource_tool.cpp`;
- `port/tools/test_resources.py`, `summarize_resource_tests.py`,
  `import_original.py`, `test_import_device.py`;
- `port/docs/phase3c-status.md`, `phase4-boundaries.md`, `runtime-boundaries.md`;
- `port/README.md`.

Die beiden bestehenden Importtreiber erhalten getrennte private Berichtziele,
damit Regressionen historische Importberichte nicht überschreiben. Keine
GameCube-Bytes, GCMs, extrahierten Assets oder Buildprodukte werden committet.

## Verbleibende Probleme und Voraussetzungen für Phase 4

Die geprüfte gemeinsame Datenverwaltung ist abgeschlossen. Offen sind echte
Modellaktivierung/-fixups und Blockpoolsteuerung, alle 13 Gameplay-Heap-
Lebenszeiten, native REL-Registry samt explizitem Modulzustand, vollständige
`GLOBAL_WK`-/Manager-/Core-/Playerbindung und die tatsächlichen Gameplay-
Entryfunktionen. Weiterhin fehlen Renderer, Sound-/Videowiedergabe und die
physische Controllerimplementierung. Ressourcenbytes allein führen diese
Systeme nicht aus.

Auch die bisherigen DVD-Grenzen bleiben bestehen: Header-/Sounddaten werden
begrenzt indexiert und erhalten, aber nicht als aktive Soundbanken angelegt;
der vollständige hardwarebezogene Header-/SND-/NEST-/ARAM-Mischpfad ist nicht
portiert. Feste Heap-End-Stagingadressen aus `ReadAreaData` sind durch native
Besitzer ersetzt. Weitere Verbraucher mit besonderen DVD-/Heap-/Endpointer-
Modi müssen anhand ihrer Originalaufrufe angepasst werden; ihre vollständige
Kompatibilität ist durch reine Byte-Residenztests nicht bewiesen.

[phase4-boundaries.md](phase4-boundaries.md) benennt die tatsächlichen
Abhängigkeiten von `GameTask`, `StageSet`, `ScenarioRoomInit`, `R120Init` und
`R100Init`, die REL-Prolog/Epilog/Stop/Restart-Reihenfolge sowie die verbindliche
Retroid-/Xbox-/PlayStation-Eingabearchitektur. Die neuen abstrakten
Modul-/Block-/Padgrenzen sind vorbereitet und ausdrücklich nicht simuliert.

Der konkrete nächste Schritt ist nach separatem Phase-4-Auftrag die native
Modulregistrierung mit Game/Stage/Modul/Room-Lebenszeiten und geprüften
Ressourcenreferenzen. Erst danach können Originalmanager und Raumcallbacks
systematisch eingebunden werden. Es wurde kein `GameTask` durch Dummy-Manager
kompilierbar gemacht und Phase 4 wurde nicht begonnen.
