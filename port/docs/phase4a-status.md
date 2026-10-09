# Phase 4a – Native Modulregistrierung und Originalcode-Ausführung

Stand: 9. Oktober 2026. Grundlage: Phase 3c,
`d56196f04d101bd2e2e36eaf3fc4de682f484877`. Ausschließlich `android-port` und
`port/`; vollständige Dateien liegen im Projekt. Keine Originalquellen,
Disc-Images oder importierten Assets verändert. Keine Assets im APK oder Git.

Die native Modullaufzeit registriert geprüfte Originalfunktionen, verwaltet ihre
Lebenszeiten und ruft sie über die bestehende MRAM-/Scheduler-Laufzeit auf.
Ein vollständiger spielbarer Raum, `GameTask`, Gegner-/Waffenlogik und Renderer
gehören weiterhin nicht zum nachgewiesenen Umfang.

## Git-Ausgangslage

Der Arbeitsbaum war sauber, Phase 3c bereits lokal gesichert. Zu Beginn bestand
der Ausgangsstand erneut alle 19 CTest-Gruppen. Nach `git fetch origin`:

| Referenz | Unveränderter Stand |
| --- | --- |
| `master` | `9b76ff003081d6ece8b838453fb8310743a972c7` |
| `origin/master` | `15cf64f80e08bbc64712c39be330b75d1512a0d0` |
| Ausgangspunkt `android-port` | `d56196f04d101bd2e2e36eaf3fc4de682f484877` |

Die Differenz `master..origin/master` betrifft ausschließlich 25 README-Zeilen
zu Projektziel und Portierungsstrategie. Sie wurden berücksichtigt. Kein Merge,
Rebase oder Push; alle bisherigen lokalen Entwicklungscommits bleiben erhalten.
Der lokale Phase-4a-Commit enthält diesen Bericht und ausschließlich die unten
gelisteten Port-Dateien. Seine ID steht in `git log -1` auf `android-port`;
der Ausgangscommit bleibt sein direkter Vorfahr.

## Nachgewiesene Originalbeziehungen

Die maßgeblichen Quellen sind `config/G4BE08/config.yml`, `modules.py`, die
114 Modulkonfigurationen mit `rel.json/splits.txt/sym_map.tsv`,
`src/game/dvd.cpp:FileTbl`, `roomdata.cpp`, `src/st1..st4/stN.cpp` und die
tatsächlichen REL-Dateien aus dem unveränderten Phase-3a-Import.

- 114 konfigurierte Modul-IDs und ihre originalen externen REL-Import-IDs.
- 216 Zeilen der fünf `St0..St4_data_tbl`-Tabellen, einschließlich leerer bzw.
  nicht durch die vier Stage-Prologe registrierter Einträge. Keine erfundenen
  Funktionen für solche Zeilen.
- 122 `set(no, RxxxInit, RxxxMain)`-Zuordnungen der vier Original-Stagequellen.
  IDs, Saveflags, Funktionsnamen, primäre Module und Quell-/Symbolvarianten bleiben
  erhalten. Das sind Registrierungen, keine 122 spielbaren nativen Räume.
- `ROOM_DATA::rel_file` ist der FileTbl-Index. `r100/r120` gehören beide zu
  `st1_0`, Modul-ID 74, FileTbl 146. `r10d` gehört zu `st1_2`, ID 78, FileTbl 152;
  `r328` zu `st3_3`, ID 101, FileTbl 203.
- Mehrfachvarianten werden dokumentiert, beispielsweise `r102` in `st1_1/st1_3`;
  die primäre Zuordnung folgt der Original-Raumtabelle. Ein Raumname allein reicht
  nicht als Schlüssel für eine überschreibbare globale C++-Registrierung.
- 644 globale Symbolnamen besitzen mehrere Modulbesitzer. Die generierten
  Teilkomponenten liegen deshalb in separaten C++-Namespaces, mit eigenem State
  statt mehrfacher globaler `_prolog/_epilog/set/setTbl`-Definitionen.

`tools/gen_rel_config.py` gewinnt `links` aus den tatsächlichen REL-Imports.
Diese Referenzen bilden einen residenten Codegraphen, teilweise mit Zyklen;
`st1_2` importiert unter anderem die IDs 73, 74 und 86. Sie dürfen nicht blind
als rekursive Aufforderung interpretiert werden, alle anderen Stage-Prologe
auszuführen. Die Registry trennt originale Symbolimporte von ausdrücklich
deklarierten nativen Initialisierungsabhängigkeiten.

Der deterministische Katalogfingerprint lautet:

`a70cb2595722b48ba11c025ccc77f5443d251547d2212c45807d404bf7448ed4`.

## Tatsächlich ausgeführter Originalcode

`tools/generate_modules.py` erzeugt die vollständige Metadatenregistrierung und
14 ausdrücklich **partielle** Stage-Komponenten. Die Ausgaben liegen nur unter
`port/build/*/original/`; Änderungen an Originaldateien sind nicht erforderlich.
`config/module-sources.json` schützt die acht zur Ausführung ausgewählten
Originalquellen per normalisiertem UTF-8/SHA-256. Vollständige Funktionskörper
werden übernommen; Funktionsnamen, Statezugriff und Native-Adapter sind sichtbar
abgegrenzt. Die Generierung ist kein Parser für beliebige ungeprüfte C++-Quellen.

| Teilkomponente | Nachgewiesene Ausführung | Bewusste Grenze |
| --- | --- | --- |
| `st1_0..3`, `st2_0..4`, `st3_0..3`, `st4_0` | Original `set`, `setTbl`, Prolog und Epilog; je Modul eigene native Tabellen/State | Nur Registrierungsanteil; vollständige Gameplay-Units und ihre Imports sind nicht kompiliert |
| `src/game/roomdata.cpp` | `execInitFunc`, `execMainFunc`, `checkRoomRange`, `checkRelRead` | Native skalare Tabellen/Funktionszeiger; explizite Byte-Narrowings, kein Cast des 12-Byte-PPC-Layouts |
| `src/st1/r10d.cpp` | Vollständige `R10dInit` und `R10dMain`, `R10dWork` | Init reserviert original ein Byte; der originale Main ist leer |
| `src/st3/r328.cpp` | Vollständige `R328Init` und `R328Main`, `R328Work` | Init reserviert original ein Byte; der originale Main ist leer |

Die leeren Main-Funktionen sind im vorhandenen Originalcode tatsächlich leer.
Dieser kleine Nachweis prüft Registrierung, Originaldispatch und reale MRAM-
Allokation/Freigabe; er beweist keine Modelle, Events oder Gegner dieser Räume.
Unvollständig portierte Funktionen besitzen keine callable Ersatzimplementierung.
Ihre internen Registrierungsfelder bleiben null und `room()` liefert keine
ausführbare Bindung. Die Metadaten behalten trotzdem ihre Originalnamen.

Bei allen 14 tatsächlichen Stage-RELs bestehen `.ctors/.dtors` jeweils aus einem
vier Byte langen Nullterminator. Der Original-Prolog/Epilog läuft deshalb korrekt
mit leeren Listen. Der Datentest überprüft das vor der nativen Aktivierung.
Andere Module besitzen nichtleere Listen, beispielsweise `Sscrn`; deren PPC-
Konstruktorzeiger werden weder gecastet noch ausgeführt. Vollständige native
Konstruktor-/Destruktorbindungen solcher Module bleiben offen.

Die Tests mit `ProbeState` prüfen pointerhaltige C++-Objekte, Konstruktion,
Destruktion und Snapshots ausdrücklich als **isolierte Adaptertests**. Sie
implementieren weder `Sscrn` noch eine originale Gameplayfunktion.

## Implementierte Modul-, Speicher- und Taskverwaltung

`ModuleRuntime` besitzt ein natives Zustandsobjekt pro aktivierter Teilkomponente,
explizite Snapshottypen und Zustände `Inactive/Active/Stopped/Failed`.
Initialisierungsabhängigkeiten werden vollständig geprüft, bevor Factory/Prolog
laufen. Fehlende Komponenten und Zyklen scheitern; neu aktivierte Abhängigkeiten
werden bei Fehlern zurückgerollt. Bereits aktive Komponenten bleiben erhalten.
Mehrfache Aktivierung einer aktiven Komponente führt zu keinem zweiten Prolog.

Originalbezug: `roomdata.cpp:linkRelData/stopRelData/restartRelData`,
`main_sub.cpp:DLL_Link/DLL_Unlink` und `sscrn.cpp:430/565`:

1. Aktivierung erstellt den nativen State und führt seinen Original-Prolog aus.
2. Temporärer Stop pausiert eigene und verschachtelt aufrufende Tasks, sichert
   State **vor** dem Epilog und erhält MRAM-Owner, Stacks und Sleep-Zähler.
3. Restart restauriert explizit die nativen Pointer/Owner, ohne einen zweiten
   Prolog oder Konstruktor. Die Aktivierungsidentität bleibt gleich; vorher
   schlafende Aufrufe setzen an derselben Stelle fort.
4. Endgültige Deaktivierung bzw. expliziter Raumreset invalidiert Callback- und
   Tasklebenszeiten, beendet schlafende eigene/entliehene Stacks und gibt danach
   State und Reservierungen frei. Aktuell ausgeführter Code darf nicht freigegeben
   werden. Der Code selbst bleibt statisch resident.

Der Inventarpfad beendet die Raumtasks beim temporären Stop nicht. Ein
grundsätzliches Cancel auch bei `stop()` würde dieses beobachtbare Verhalten
verletzen. Die neue Modulpause wird vor der Sleep-Zählerdekrementierung geprüft.
Taskprioritäten, Slotreihenfolge und Szenario-OT bleiben im vorhandenen Scheduler.

Callbackreferenzen enthalten Runtime-ID, Aktivierungsepoch, Modul-ID, Raum-ID
und Init/Main-Art. Vor jedem Aufruf wird erneut geprüft. Alte, fremde, gestoppte
oder zerstörte Bindungen werden abgewiesen; nach einem echten Reset erhält der
Raum eine neue Epoch. Es gibt keinen öffentlichen rohen Funktionszeiger, der die
Prüfung umgehen müsste. Init läuft pro Raumlebenszeit genau einmal.

Verschachtelte Modulaufrufe können auf einem Task eines anderen Moduls schlafen.
Ihre Lebenszeiten und Adapterkontexte stehen in Scheduler-Metadaten, nicht in
RAII-Objekten auf einem abrupt verworfenen Stack. Cancel/Chain räumen diese
Referenzen ab. Ein zusätzlicher Test prüft die Freigabe der TaskChain-Captures:
libc++ darf einen kleinen `std::function` beim Move kopieren, deshalb wird der
Parameter vor dem Verwerfen des Stacks ausdrücklich geleert.

Raumwork verwendet den vorhandenen MRAM-Allocator und `MemoryReservation` mit
vollständigen nativen Pointern und 32-Byte-Ausrichtung. Der geprüfte `MEM_CALLOC`
Mode `1, 0xd` ist gebunden; andere Heap-/Flagvarianten werden abgewiesen. Das ist
keine behauptete Implementierung aller 13 Dolphin-Heaps. Ein `ResourceLease`
behält alte Daten auch nach Raumreset/Deaktivierung bis zu seiner Freigabe.
Der Snapshot kopiert weder native BSS noch Vtables oder C++-Objektbytes.

ARM32 behält bei den geprüften nativen Tabellen die 32-Bit-Funktionszeigerbreite;
das vereinfacht einzelne Layoutprüfungen gegenüber ARM64. Es löst jedoch weder
Big Endian, PPC-Code, Compilerklassenlayouts noch feste GameCube-Adressen.
`RoomFunction` ist auf beiden ABIs typisiert, Reservierungen verwenden volle
native Pointer, Identitäten feste Integerbreiten. Dateidaten werden getrennt als
begrenzt gelesene Big-Endian-Strukturen behandelt. Keine Plattform wird über ein
erzwungenes 32-Bit-Pointerlayout ausgeschlossen. ASan/UBSan bleiben bis zu
geprüften Fiber-/Stackwechselhooks ausdrücklich nicht verfügbar.

Alle Lifecycle-Übergänge verlangen den Engine-Ownerthread und dessen
Frame-Aufrufstack. Auch ein direkt auf diesem Stack laufender Modulcallback
darf keinen neuen Lifecycle-Übergang beginnen; ein gezielter Negativtest prüft
diese zusätzliche Sperre. Die Integration in das echte `StageSet` aus einem Gameplay-
Task erfordert noch eine zentrale Übergabe am Frame-Rand mit Yield/Bestätigung.
Das ist eine benannte Integrationsgrenze, kein vorgetäuschter fertiger Loader.

## Prüfung tatsächlicher REL-Daten

`module_rel_index.cpp` liest begrenzt Big-Endian-Header, Sektionstabelle,
Entry-Offsets, Importliste, Relocation-Streamgrenzen und BSS-Größen. Es führt
keine PPC-Relocations aus. IDs und externe Imports müssen mit dem generierten
Originalkatalog übereinstimmen.

Ein beim ersten Gesamttest gefundenes Problem wurde anhand des Original-SDK-
Headers korrigiert: `include/dolphin/os/OSModule.h:53` kennzeichnet `bssSection`
als zur Laufzeit gesetzt. Das Disc-Feld ist bei den geprüften SN-Dateien null;
beispielsweise liegt Sscrn-BSS tatsächlich in Sektion 22. Der native Parser nutzt
die nachgewiesene `.bss`-Sektion aus `rel.json` und überprüft sie gegen die
tatsächlichen Nulloffset-/Größenwerte. Er erfindet keine alternative Headerform.

`test_modules.py` findet alle 114 Quellen inventargesteuert. Bei Enemy-Modulen
gilt die belegte `tools/extract_orig.py`-Regel `em/*.rel` in `em/*.drs`;
`tools/drs.py:Drs` extrahiert die Original-REL für die unabhängige Referenzprüfung.
Die native Seite lädt reale Dateien über `EngineLoader`, den bestehenden DVD-
Anforderungs-/Framepfad und bei DRS den begrenzten Archivindex. Lose Stage-RELs
nutzen den originalen `linkRelData`-Lesemode `0x104`.

Je Datei werden Dateihash, exakte REL-Länge/-Hash, Modul-ID, BSS-Größe und sieben
fehlerhafte/trunkierte Varianten geprüft. Die 14 Stage-Registrierungen werden
danach nativ ausgeführt, einschließlich der beiden freigegebenen Init/Main-Paare.
Alle anderen Module bleiben **Metadatenprüfung**, nicht native Gameplayausführung.

## Builds und reproduzierbare Verifikation

Werkzeuge: Windows/MSVC 19.51, CMake 4.1.2/Ninja; Python 3.12. Python 3.9+ ist
jetzt für die Generierung verpflichtend. Android NDK 28.2.13676358/Clang 19,
Gradle 8.13, AGP 8.13.2, JBR 21. Keine zusätzlichen Pythonpakete.
Android-Native-Builds, Debug-APK, Test-APK und `lintDebug` erfolgreich.
Die bestehende nichtfatale SDK-XML-3/4-Warnung bleibt; keine native Buildwarnung
oder vorgetäuschte Verfügbarkeit fehlender Werkzeuge.
Android-Lint meldet 0 Fehler und 8 Warnungen zu gepinnten älteren Werkzeug-/SDK-
Versionen, fehlendem ChromeOS-x86_64, Backup-Regeln, App-Icon und einem Text der
Diagnose-Activity. Diese Warnungen wurden nicht durch ungeprüfte Upgrades oder
eine zusätzliche Ziel-ABI verdeckt.

Die vollständige abschließende Verifikation ist erfolgreich. Bilanz:

| Prüfung | x86 | x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| Vollständige CTest-Suite | 21/21 | 21/21 | native Einzelwerkzeuge | native Einzelwerkzeuge |
| Modul-/Lebenszeittest | 215 Checks | 215 Checks | 215 Checks, zweimal | 215 Checks, zweimal |
| Alle 114 echten RELs | zweimal | zweimal | zweimal | zweimal |
| Native bisherige Kernprüfungen | durch CTest | durch CTest | 13.386, zweimal | 13.387, zweimal |
| APK-Instrumentierung | entfällt | entfällt | 15/15 | 15/15 |

| Originaldatenkorpus | Erfolgreiche Ausführungen | Umfang |
| --- | --- | --- |
| Neue REL-/Modulprüfung | 912 | 114 Module × 2 Durchläufe × 4 Plattformen |
| Ressourcen-/MRAM-/ARAM-Prüfung | 3.600 | 308 ausgewählte Ressourcen + 142 Raumarchivvorkommen, zweimal auf 4 Plattformen |
| Bestehender vollständiger Engine-Ladepfad | 852 | 142 Raumarchive, einmal je Host-ABI und zweimal je Android-ABI |
| Bestehender Import-/Yz2-Referenzkorpus | 4.548 | 758 Fälle je Host-ABI, zweimal je Android-ABI |
| Summe dieser Originaldatenfälle | **9.912** | Wiederholungen zählen als Ausführungen, nicht als zusätzliche einzigartige Assets |

Hinzu kommen 6.384 korrekt abgewiesene fehlerhafte/trunkierte REL-Eingaben,
16 reale Init/Main-Lebenszeitprüfungen im REL-Datentreiber und die separaten
215 Modul-/63 Ressourcen-Adapterchecks je isoliertem Lauf. Die vier Module-
Berichte decken je beide Discgruppen und zwei Runden ab: 110 konfigurierte
Module von Disc 1 und vier weitere von Disc 2. Ein discübergreifend identisches
Modul wird für diese Konfigurationsabdeckung einmal anhand seines verifizierten
kanonischen Disc-Vorkommens ausgewählt; das ersetzt keine zusätzliche
Behauptung, jedes doppelte physische REL-Vorkommen erneut ausgeführt zu haben.

`module-verification-summary.json` und `verification-summary.json` bestätigen
0 Fehler, aktuelle Binärhashes und identische relevante ABI-Traces. Der
Import-/Decodertrace aller vier Android-Durchläufe lautet
`d3bead1d4b296186cdb2e7b1f2075019c7c54217437da2455274884cad5741a8`.
`module-coverage.csv` enthält alle 114 Module; `room-module-coverage.csv` alle
122 originalen Registrierungszuordnungen. 14 Module besitzen die ausführbare
partielle Stage-Registrierung; die übrigen 100 besitzen einen geprüften REL-
Metadateneintrag, aber noch keine native Gameplaykomponente. Zwei Raum-Paare
sind vollständig kompiliert; die übrigen 120 bleiben ausdrücklich offen.

ADB-Gerät `dbdcacc`: Retroid Pocket Flip2, Android 13/API 33;
`ro.product.cpu.abilist = arm64-v8a,armeabi-v7a,armeabi`.
Die APK wurde für jede ABI mit `adb install --abi` installiert;
`primaryCpuAbi` und die tatsächliche JNI-Pointerbreite wurden geprüft.
Der Unterschied eines Kernchecks ist der vorhandene zusätzliche 64-Bit-Pointer-
Test; die sieben Scheduler-/Hintergrund-/Decodertraces bleiben identisch.
Die 215 neuen Modulchecks und ihre drei semantischen Traces stimmen ebenfalls
plattformübergreifend überein.

Die Generatorprüfung enthält fünf Tests: echte Zuordnungen/Varianten,
deterministische Ausgabe, Kollisionsauflösung, Ablehnung von Änderungen an allen
acht freigegebenen Quellen sowie unbekannten Imports/doppelten Modul-IDs.
Die Lifecycleprüfung enthält unter anderem Save vor Epilog, Restore ohne zweiten
Prolog, Konstruktor-/Destruktorbesitz, Fehlerrollback, fremde/stale Referenzen,
Pause/Resume eines verschachtelten Stacks, finalen Cancel und Capturefreigabe.

Während der Entwicklung scheiterten zunächst die isolierte leere Testkatalog-
Fixture (der echte Loader verlangt einen lesbaren Katalog), eine Testannahme
`r328 → st3_2` und der Gesamttest am oben beschriebenen BSS-Headerfeld.
Die Fixture enthält jetzt eine echte dreibytegroße synthetische Datei; der Test
verwendet die nachgewiesene Zuordnung `r328 → st3_3`; der REL-Parser wurde anhand
des Originalheaders korrigiert. Eine falsche Testwerkzeug-API (`result` statt
`readCheck`) und ein Generator-Syntaxfehler wurden vor den endgültigen Builds
behoben. Die tatsächlich ausgeführten finalen Modul-, Host- und APK-Tests haben
keine Fehler. Negative Eingaben werden absichtlich abgewiesen und zählen nicht
als erfolgreiche Ausführung fehlender Enginefunktionen.

Reproduzierbare Befehle aus dem Repository, nach Initialisierung einer passenden
x86/x64-MSVC-Umgebung (Pythonpfad bei Bedarf mit `Python3_EXECUTABLE` setzen):

```powershell
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-x86 --output-on-failure
.\port\scripts\build-android.ps1 -SdkRoot <SDK> -JavaHome <JDK> -Python <Python> -Offline
.\port\scripts\test-device.ps1 -Adb <adb> -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb <adb> -Serial dbdcacc

python port/tools/test_modules.py --data <import-phase3a> --output <phase4a> --host-exe port/build/host-ninja/re4_module_tool.exe --repeat 2
python port/tools/test_modules.py --data <import-phase3a> --output <phase4a> --host-exe port/build/host-x86/re4_module_tool.exe --repeat 2
python port/tools/test_modules.py --data <import-phase3a> --output <phase4a> --adb <adb> --serial dbdcacc --repeat 2
```

Die vollständige Regression und die endgültigen Abdeckungsmatrizen lassen sich
mit folgenden Aufrufen erneut erzeugen (Hostwerkzeuge zuvor bauen):

```powershell
$taskData = 'E:\Development\Android\RE4-Data\import-phase3a'
$taskEvidence = 'E:\Development\Android\RE4-Data'
$taskAdb = 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'
foreach ($taskBuild in @('host-ninja', 'host-x86')) {
    python port/tools/test_resources.py --data $taskData --output "$taskEvidence\phase4a-resources" --host-exe "port/build/$taskBuild/re4_resource_tool.exe" --repeat 2
    python port/tools/test_engine_loading.py --data $taskData --output "$taskEvidence\phase4a-all-host" --host-exe "port/build/$taskBuild/re4_loader_tool.exe" --all-rooms --repeat 1
    $taskLabel = if ($taskBuild -eq 'host-ninja') { 'x64' } else { 'x86' }
    python port/tools/import_original.py verify-native --data $taskData --native-tool "port/build/$taskBuild/re4_asset_tool.exe" --report "$taskEvidence\phase4a-resources\host-$taskLabel-import-tests.json"
}
python port/tools/test_resources.py --data $taskData --output "$taskEvidence\phase4a-resources" --adb $taskAdb --serial dbdcacc --repeat 2
python port/tools/test_engine_loading.py --data $taskData --output "$taskEvidence\phase4a-all-device" --adb $taskAdb --serial dbdcacc --all-rooms --repeat 2
python port/tools/test_import_device.py --data $taskData --output "$taskEvidence\phase4a-resources" --adb $taskAdb --serial dbdcacc --repeat 2
python port/tools/summarize_resource_tests.py --resources "$taskEvidence\phase4a-resources" --engine-host "$taskEvidence\phase4a-all-host" --engine-device "$taskEvidence\phase4a-all-device"
python port/tools/summarize_modules.py --modules "$taskEvidence\phase4a" --resources "$taskEvidence\phase4a-resources"
```

Die vorhandenen vollständigen Ressourcen-, Engine-Lade- und Importprüfungen
wurden mit den aktuellen Binärdateien wiederholt. Die zusätzliche finale
Callback-Sperre änderte nur die Modulkomponente; deren Host-/Android-Builds,
vollständige REL-Prüfungen und APK-Prüfungen wurden anschließend nochmals
ausgeführt. Die Binärhashes aller übrigen Korpustests bleiben gültig und wurden
in der Zusammenfassung erneut geprüft. Private Daten/Nachweise:

- `E:\Development\Android\RE4-Data\import-phase3a`: unveränderte Eingaben.
- `...\phase4a`: Module, Original-REL-Hashes, Disc-Zuordnung, Katalog,
  vier ABI-Berichte und automatisch erzeugte Modul-/Raumabdeckungsmatrizen.
- `...\phase4a-resources`: MRAM/ARAM-, Import-/Yz2-Berichte und Gesamtregression.
- `...\phase4a-all-host` / `...\phase4a-all-device`: vollständiger Engine-Ladepfad.
- `port/build/device-*-round*.txt` / `instrumentation-*.txt`: nicht versionierte
  tatsächliche Kern-/APK-Laufzeitlogs mit Binär-/APK-Hashes.

`summarize_resource_tests.py` und `summarize_modules.py` prüfen diese Nachweise
gegen die aktuellen Executable-Hashes, Originalreferenzen, Katalogidentität,
Wiederholungen und ABI-Traces; alte erfolgreiche Reports reichen nicht aus.

## Offene gemeinsame Engine-Abhängigkeiten

| Einstieg/Originalquelle | Nachgewiesene Voraussetzung | Status und nächster Nachweis |
| --- | --- | --- |
| `game.cpp:GameTask`, `stage.cpp:StageSet` | Echter GLOBAL_WK, Heap-/Game-/Stage-Lebenszeiten, Frame-Owner-Übergabe, `readEmList` | Vollständige Funktionen nicht kompiliert; gemeinsame Zustände/Transitionsadapter vor Gameplaystart |
| `sce_sys.cpp:ScenarioRoomInit` | Event-/Item-/Savezustände, ursprüngliche Manager, Ashley, `EmSetFromList`, SceAt | Scheduler vorhanden; Funktion als Ganzes offen |
| `r120.cpp:R120Init` | EvtMgr.SetFunc für zwei tatsächliche Callbacks, `SceExec(R120Event)`, `TexRenderInit` | Metadaten/Dateien verfügbar, Init/Main nicht callable |
| `r120.cpp:R120Event` | Fade, Sofdec-Filme, SubScreen, EventRead/Packet-Ausführung und originaler RoomJump | Kein sofortiger RoomJump als Ersatz; gemeinsame Verbraucher müssen zuerst laufen |
| `r100.cpp:R100Init` | `EmReadSearch(0x12)`, SMD-/Obj-/Event-/SceAt-Manager, Original-Archivindizes und TexRender | Keine manuelle Gegner- oder Eventrekonstruktion; vollständiger Code noch offen |
| `r100.cpp:193` | Store eines `PlArc*` bei PPC-Offset `0x378` in ursprünglichem Playerobjekt | Native typisierte Memberbindung erforderlich; nicht als ARM64-Objektoffset verwenden |
| `read.cpp:setEmModule` | Einmaliges DLL-Link, Prolog erneut bei Auswahl, Export `EmInitFunc` nach MODULE_DAT | Anders als Stage-Aktivierung; typisierte Auswahl-/Exportbindung noch offen |
| `read.cpp:ReadWepData/ReleaseWepData` | Charakter-/Waffentabellen, DRS, Playerzustand, `WeaponInitFunc/MoveFunc`, Obj-Exports | Vollständiger Waffenverbraucher und typed exports erforderlich |
| `em18.cpp:Em18Init`, `pl02.cpp:Pl02Init`, `wep36.cpp:_prolog/_epilog` | Originale `cEm18/cPlAda`-Placementkonstruktion, Weapon-/ObjInit-Slots | Inventarisiert, nicht durch `ProbeState` implementiert |
| `main_mem.cpp`, `sscrn.cpp` | 13 Heapmodi, Room-Heap 4, Heap 12/10, MemorySwap und Suspend/Signal | Nur geprüfte MRAM-Workreservierung gebunden; feste GameCube-Adressen niemals native Pointer |

**Nachgewiesen** sind die oben genannten Quellenbeziehungen, Originaldatenhashes,
zwei originalen Init/Main-Paare und die geprüften nativen Lebenszeiten.
**Architekturentscheidung** ist statisch residentes C++ mit unabhängigen State-
Objekten, typed exports/snapshots und Frame-Owner-Transitions. **Offen** bleiben
vollständige States/Snapshots der übrigen Gameplay-Module, nichtleere native
Konstruktorlisten, globale Actor-/Eventexports, vollständige Heapmanager und die
Startsequenz. Diese Abdeckung wird nicht aus einem erfolgreichen Build abgeleitet.

Die 35 unvollständigen englischen MDT-Blöcke bleiben unverändert und als offen
dokumentiert. Laden/Registrieren bedeutet noch keine tatsächliche Message-
Dereferenz. `english-mdt-reachability.json` benennt RoomMes-/Eventpfade; erst deren
echte Ausführung kann die benötigten Texte nachweisen. Keine erfundenen Inhalte
und kein stiller Sprachfallback.

## Verbindliche spätere Eingabe und Grafik

Eine zentrale `PlatformGamepads`-Grenze liefert einen Snapshot je Gameplaytick an
die originale `PadRead`-Logik. Retroid sowie Android-Xbox-/PlayStation-/andere
Gamepads nutzen dieselbe Zuordnung für Gameplay, Inventar, Menüs und QTEs.
Gerätefähigkeiten, Sticks, Trigger, D-Pad, ABXY, Schultern, Menütasten und
Disconnectzustände gehören zum Plattformadapter; Module dürfen keine eigenen
Android-Eingabepfade erhalten. Der Treiber ist noch nicht implementiert.

Das backendunabhängige Ingame-Menü bleibt verbindlich: MSAA Aus/2×/4×,
AF 2×/4×/8×, DoF Ein/Aus mit nahen/anvisierten Objekten, persistente Speicherung
und hardwarevalidierter tatsächlich aktiver Zustand. Anforderungen bleiben in
`graphics-settings.md`; diese Phase implementiert weder Menü noch Effekte.

## Vollständige geänderte Projektdateien

Alle Dateien sind vollständig im Arbeitsbaum bereitgestellt; generierte
Buildausgaben und private Referenzberichte werden nicht eingecheckt.

Neue Dateien:

- `port/config/module-sources.json`
- `port/include/re4/port/modules.h`
- `port/include/re4/port/module_verification.h`
- `port/src/modules.cpp`
- `port/src/module_original.h`
- `port/src/module_rel_index.cpp`
- `port/tests/module_tool.cpp`
- `port/tests/module_verification.cpp`
- `port/tests/test_modules.py`
- `port/tools/generate_modules.py`
- `port/tools/test_modules.py`
- `port/tools/summarize_modules.py`
- `port/platform/android/module_jni.cpp`
- `port/android/app/src/androidTest/java/org/re4/port/ModuleCodeTest.java`
- `port/docs/phase4a-status.md`

Veränderte Dateien:

- `port/CMakeLists.txt`
- `port/include/re4/port/contracts.h`
- `port/include/re4/port/scheduler.h`
- `port/src/scheduler.cpp`
- `port/android/app/build.gradle.kts`
- `port/android/app/src/main/java/org/re4/port/NativeChecks.java`
- `port/scripts/build-android.ps1`
- `port/scripts/test-app-device.ps1`
- `port/README.md`
- `port/docs/runtime-boundaries.md`
- `port/docs/phase4-boundaries.md`

## Voraussetzungen für eine spätere Gameplayintegration

Zuerst den originalen Game-/Stage-/Room-State und die erforderlichen Heap-
Lebenszeiten typisiert anbinden; Modulübergänge aus schlafenden GameTasks sicher
an den Frame-Owner übergeben. Danach originale exportierte Actor-/Eventfunktionen
und gemeinsame EvtMgr/SceAt/Modellverbraucher portieren, mit gezielten Tests gegen
Originalressourcen. Erst dann `R120Init/R120Event` und den ursprünglichen Übergang
zu `r100` tatsächlich ausführen. Keine nächste Phase in dieser Sitzung begonnen.

Beim Kompilieren weiterer vollständiger Units müssen gemeinsame Engineklassen
und wiederverwendete Quellunits konsistent einmal gebunden werden; modulinterne
Symbole/States bleiben getrennt. Der Namespace-Nachweis der Stage-Teilkomponenten
ist keine automatische Lösung sämtlicher ODR-/Klassenlayoutprobleme der übrigen
100 Module. Exportprototypen werden aus ihren tatsächlichen Deklarationen und
Aufrufstellen gewonnen, einschließlich der erneut laufenden Enemy-Prolog-
Registrierung, statt globale `void*`-Exports oder pauschale Funktionscasts zu übernehmen.
