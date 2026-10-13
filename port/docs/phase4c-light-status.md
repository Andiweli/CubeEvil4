# Phase 4c: originales Lichtsystem und GameTask-Fortsetzung

Fortsetzung am 10. Oktober 2026 auf `android-port`, Ausgangscommit
`5c6d94daa34a9058fcc2f7c7f7385669682df4a4`.

## Nachgewiesener Fortschritt

Der originale `GameTask` passiert jetzt `LightMgr.init(LightFuncTbl)` und
`LightMgr.initPath(CORE_ARC(CORE_LIGHT_PATH))`. Beide Aufrufe führen die
extrahierten Originalfunktionskörper aus. Danach wird der vorhandene
`ScenarioInit`-Adapter erreicht. Der nächste tatsächliche Abbruch lautet
`Original consumer unavailable: PlayerInit()` (`src/game/game.cpp:264`).
`Rno0` bleibt 0. Es wurde weder zum Raumzustand weitergeschaltet noch ein
Spielstart simuliert.

Mit privaten Originaldaten nachgewiesene Reihenfolge:

```text
Bestehender Bootadapter mit original belegten Voraussetzungen
  -> CoreDataRead -> MessageControl::init
Original GameTask -> gameDebug -> gameInit
  -> ClothInit -> GXSetCopyClear -> MessageControl::gameInit
  -> SubScreenGameInit -> SubScreenAramRead -> SubScreenRoomInit
  -> Cockpit::gameInit -> IDSystem::roomInit
  -> ObjMgr.warnDiv = 100
  -> cLightMgr::init
       -> cManager<cLight>::init -> cManager<cLight>::roomInit
  -> cLightMgr::initPath mit dem tatsächlichen Core-Light-Path-Block
  -> bestehender ScenarioInit-Adapter
  -> PlayerInit(): ausdrücklich noch nicht gebunden
```

`cLightMgr::init` (`src/game/light.cpp:61`) registriert die originale
17-Einträge-Tabelle aus `src/game/game.cpp:173`, löscht `LightEnv`, setzt
`ElecPower` auf 1, alle acht Kind-Wörter auf `0xFFFFFFFF` und die Pool-/Path-
und Debugreferenzen zurück. Diese konkreten Zustände und die Aufrufreihenfolge
werden im echten GameTask geprüft. `initPath` (`light.cpp:1075`) bindet den
originalen Core-Block. Startup-Initialisierung und Rauminitialisierung sind
verschiedene Schritte: Im Startup existiert noch kein Lichtpool.

Der spätere originale `gameRoomInit` bindet bei `game.cpp:413–416` Core-LIT,
Raum-LIT 0 und Raum-LIT 1, allokiert nach `CONS_R_NLIGHT` und bindet den
Core-Path erneut. Diese Kette ist anhand echter geladener Archive als
**isolierte Ausführung echter Originalfunktionen** geprüft. Der vollständige
`gameRoomInit` wird in diesem Stand noch nicht ausgeführt.

## Originalcode, Adapter und Ausführungsgrenzen

`port/tools/generate_lights.py` extrahiert 72 vollständige Originalkörper.
21 Originalquell-/Headerdateien sind durch SHA-256 in
`port/config/light-sources.json` gesichert. Generiert werden im jeweiligen
Buildverzeichnis `original/light_control.cpp`,
`original/include/re4_light_types.h` und `original/light-manifest.json`.
Das Manifest benennt Signaturen, Quellenzeilen und explizite Anpassungen.
Originaldateien unter `src/` und `include/` bleiben unverändert.

Neu tatsächlich ausgeführt im GameTask:

- `cLightMgr::init` und `initPath`.
- Die dabei aufgerufenen `cManager<cLight>::init` und `roomInit`.

Zusätzlich mit Originaldaten isoliert ausgeführt:

- `cLightMgr::roomInit`, `cLit::versionUp`, `getCut`, `getMaxLight` und
  `cLightEnv::getLightWork`.
- Originale `ConsInitRoom`/`ConsGetRoomValue` samt Defaulttabelle für die
  Lichtpoolkapazität.
- Originale Poolallokation, Front-/Back-Erstellung, Verkettung, Entfernung
  und Freigabe; `cLightMgr::create(cLightWork*)`, `createBack`,
  `cLight::operator=(cLightWork&)`, `setParent(u8,u32)`, `calcParent`, `move`.
- `Light00_Move`, `Light01_Move`, `Light02_Move`, `Light04_Move` und
  `Light07_Move` mit tatsächlich vorhandenen Datensätzen. Auch der leere
  `Light04_Move`-Körper ist der unveränderte Originalkörper.
- `setElecPower`, `onKind`, `offKind`, `checkKind`, `beginEvent`, `endEvent`,
  `roomLitSet`, `roomLitCheck`, `setTune`, `setFogStart`, `setFogEnd`.
- `getPathPtr`, `cLightPath::setPath` und `movePath`; die originale
  Vektoraddition und die native Fassung von `LIMIT_ANGLE` werden von den
  Typfunktionen verwendet.

Weitere extrahierte Körper sind mitkompiliert, aber damit nicht automatisch
ausgeführt oder vollständig integriert. Insbesondere sind die Handler
03/05/06/08/10 nicht durch passende Lichtdatensätze dieser Raumstichprobe
belegt. Der originale Path-Cursor ist separat mit echten Core-Bytes geprüft;
das ist kein Nachweis von `Light05_Move` als Raumlicht. Auch
`cLightWork::operator=(cLight&)`, `saveLit`/`loadLit` und der Export eines
pointertragenden Type-5-Workbereichs sind kein unterstützter Speicherpfad.

Notwendige native Adapter sind klar getrennt:

- `LightRuntime` besitzt CPU-Manager, Pool, Resource-Leases und generationale
  Handles auf dem Frame-Owner-Thread. Kopierte Zustandsansichten bilden die
  Schnittstelle zu künftigen Renderingverbrauchern; native Modellpointer
  gelangen nicht in diese Ansichten.
- GameCube-Adressbereichsprüfungen werden durch Besitz-/Bereichsprüfungen der
  nativen Ressourcen und des Pools ersetzt. Pointerarithmetik verwendet
  native Pointer beziehungsweise `size_t`, Dateioffsets bleiben `u32`.
- Die Pool-Konstruktion erzeugt einen gemeinsamen nativen `LightCpu`.
  Die originalen Unterklassen 01/02/06/07/08 haben laut Originalheader und
  Konstruktoren keine zusätzlichen Felder oder Initialisierungsanweisungen;
  das Verhalten wird durch die originale Typfunktionstabelle dispatcht.
  Dieser Konstruktionsadapter ist nicht der unveränderte Körper von
  `cLightMgr::construct`.
- Das originale Pool-`delete` wird in einen Lebenszeit-erhaltenden Retire-
  Vorgang mit den ursprünglichen `0x601`-Flagbits übersetzt. Die Allokation
  gehört weiterhin dem gemeinsamen Gameplay-Heap, nicht einzelnen Lights.
- Die PowerPC-Assemblerfassung von `LIMIT_ANGLE` wird durch dieselbe
  `f32`-Additions-/Subtraktionsschleife mit den Original-PI-Konstanten
  ersetzt. Es gibt keine vereinfachte Ersatz-Lichtberechnung.
- Em-/SMD-/Etc-/Obj-Parent-Lookups benötigen einen echten Managerresolver.
  Ohne ihn wird vor Veröffentlichung einer Lichtinstanz explizit
  `MissingGameConsumer` geworfen. Es wurden keine Parentmodelle erfunden.

## ABI, Endianness und Lebenszeiten

Die Originaldefinitionen von `cLit`, `cLightEnv`, `cLightWork`, Path- und
Typarbeitsstrukturen werden aus den Originalheadern gewonnen.
Die pointerfreien Wirelayouts sind mit `static_assert` auf beiden ABIs
abgesichert: `cLit`/`LightPathHeader` jeweils 4 Bytes, `cLightEnv` 260 Bytes,
`cLightWork` 300 Bytes. Farben und Byteflags bleiben byteweise erhalten.
Benannte BE-u16/u32/f32-Felder werden in eine geprüfte native Kopie umgesetzt;
unbekannte Bytes werden erhalten, nicht interpretiert oder neu erfunden.

Der originale BE-Unionzugriff auf `ParentNo` wird durch explizite
`u16(ParentNo)`-/`u16(ParentNo >> 16)`-Zugriffe ersetzt. Pointertragende
Live-Objekte werden nicht auf das GameCube-Speicherlayout gepackt.
`cLightPath` benötigt nativ 12 Bytes auf ARM32 und 24 auf ARM64. Der
Workbereich erhält die nötige native Ausrichtung; bei Type 5 werden die
belegten Wire-ID-/Flagbytes in diesen nativen Zustand expandiert. Ein
GameCube-Pointer wird niemals als native Adresse übernommen.

LIT-Cut-Anzahl, Offsettabelle, Headerüberlappung, Ausrichtung, Work-Anzahl,
Ressourcengrenzen und überlappende Cuts werden vor Zugriff geprüft. Exakt
geteilte Cut-Offsets bleiben zulässig. Die Allokationsrundung wird in `u64`
berechnet und gegen die native `size_t`-Grenze geprüft. Neuere Versionen als
die vom Original unterstützte Version `0x2C` werden abgewiesen.

`cLit::versionUp` läuft auf der nativen Kopie. Das Original aktualisiert nur
Core und erstes Raum-LIT; das dritte LIT wird unverändert gebunden. Genau
dieses Verhalten wird geprüft. Archiv- und Blockhashes belegen, dass
Quelldaten auch nach Upgrade, Freigabe und Wiederinitialisierung unverändert
bleiben.

Handles enthalten Laufzeitidentität, Epoch, Slot und Generation. Gelöschte,
wiederverwendete und nach Reinitialisierung veraltete Handles werden
abgewiesen. Modellreferenzen verwenden vorhandene `ConsumerHandle` und
deren Lebenszeitprüfung. Freigabe beziehungsweise neuer Raumbindung geht
eine Prüfung auf ruhende Schedulerstacks voraus. Im GameRuntime werden
zuerst Tasks beendet, anschließend Lights und erst danach Modelle freigegeben.
Ein noch gehaltener Resource-Lease überlebt die Managerfreigabe.

Für die spätere echte Raumwechselanbindung muss die Bindung an einer sicheren
Frame-Grenze erfolgen. Der gegenwärtige quiescente `bindArchives`-Vertrag
wurde nicht stillschweigend in einen bereits schlafenden vollständigen
`gameRoomInit` eingebaut.

## Originaldatenabdeckung

Quelle ist ausschließlich der private Import unter
`E:\Development\Android\RE4-Data\import-phase3a` mit vorhandenen FST-
Identitäten, CAS-/Archiv-/Blockhashes und Originalnamen. Core wird über die
bestehende DVD-Anforderung geladen, Räume über den gemeinsamen
`loadRoom`-/`ReadAreaData`-Pfad inklusive Hintergrundtask und Yz2.
Es gibt keine neuen raumspezifischen Laderegeln.

`CORE_LIGHT` ist laut `include/arc/core.h` Slot `0x0B`, im indizierten
Archivbody Eintrag 7. Der ursprüngliche Path-Slot `0x0F` ist Body-Eintrag 11
und trägt tatsächlich den Tag `BIN`. Er ist keine zusätzliche LIT-Datei.
Es sind elf Path-Indizes vorhanden; Index 7 ist null, zehn enthalten
terminierte Helligkeitspfade. Alle zehn werden mit den vier originalen
Flagkombinationen gegen eine unabhängige Auswertung der tatsächlichen
Bytes geprüft: Fortschritt, Schleife, Endzustand und Inversion.

Die Tabelle zeigt je Raum und je LIT ein einzelnes Prüfdurchlaufen.
Jeder Raum wird pro CLI-Lauf zweimal initialisiert; pro ABI werden zwei
CLI-Runden ausgeführt. Cuts umfassen die ganze Offsettabelle, auch null-
Einträge; Records zählen die tatsächlich referenzierten Workeinträge.

| Disc / Archiv | LIT 0: Cuts / Records / erstellt / blockierter Parent | LIT 1: Cuts / Records / erstellt / blockierter Parent |
|---|---:|---:|
| 1 / `St1/r120.das` | 30 / 93 / 93 / 0 | 1 / 0 / 0 / 0 |
| 1 / `St1/r100.das` | 16 / 289 / 273 / 16 | 1 / 2 / 2 / 0 |
| 1 / `St1/r101.das` | 20 / 262 / 242 / 20 | 2 / 4 / 4 / 0 |
| 2 / `St4/r400.das` | 20 / 262 / 242 / 20 | 2 / 4 / 4 / 0 |
| 2 / `St4/r405.das` | 25 / 206 / 186 / 20 | 1 / 2 / 0 / 2 |
| 2 / `St4/r402.das` | 1 / 37 / 22 / 15 | 1 / 2 / 0 / 2 |

Der jeweils mitgeprüfte Core-Hauptblock enthält 46 Cutslots, 13 vorhandene
Cuts und 26 Records: 15 werden erstellt, elf benötigen fehlende Modellmanager.
Pro ABI ergeben sich über beide Runden 24 isolierte originale `roomInit`-
Aufrufe, 4.632 erfolgreiche Record-Erstellungen und 644 ausdrücklich
blockierte Modellauflösungen. Diese Summen enthalten wiederholte Core-Records;
es sind keine 4.632 unterschiedlichen Lichtquellen im Spiel.

Nachweisbare Besonderheit von r120: In 78 der 93 Records stehen nach dem
Originalheaderlayout Parent-Typen 191, 188 beziehungsweise 62, jeweils 26-mal.
Der echte `calcParent`-Defaultzweig meldet einen ungültigen Typ und setzt
das Live-Objekt auf ParentType 0 / ParentNo 1. Die Quelldaten bleiben erhalten.
Diese 78 Ausführungen sind **kein Nachweis einer erfolgreichen ursprünglichen
Modellreferenz**. Die Ursache dieser Debugdaten-/Versionsauffälligkeit ist
offen und verlangt einen Abgleich mit der GameCube-Referenzausführung.
Es wurde keine vermeintliche Formatkorrektur eingeführt.

Die beobachteten Wireversionen sind `0x25`, `0x28`, `0x2B` und `0x2C`.
Die zusätzlichen Core-LIT-Vorkommen 18–22 werden vom bestehenden Import
inventarisiert, sind hier jedoch nicht als aktive Raumlichtblöcke geprüft.
Der Lichtnachweis umfasst sechs Räume, nicht das gesamte Lichtgameplay
aller 142 inventarisierten Raumvorkommen.

## Tatsächlich ausgeführte Verifikation

Windows-MSVC-Builds für x86/x64 und der Android-NDK-/Gradle-Build für
`armeabi-v7a`/`arm64-v8a`, Debug-APK, Test-APK und Lint bestehen.
Verwendet wurden MSVC 19.51, SDK-CMake 4.1.2, NDK 28.2.13676358/Clang 19,
Gradle 8.13, AGP 8.13.2 und die vorhandene Android-Studio-JBR.

Echtes Gerät: Retroid Pocket Flip2, Serial `dbdcacc`, Android 13 / API 33.
ADB meldet `arm64-v8a,armeabi-v7a,armeabi`. Die nativen CLI-Prozesse melden
32 beziehungsweise 64 Pointerbits; die APK wird für jede ABI separat mit
`--abi` installiert und `primaryCpuAbi` wird geprüft.

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
|---|---:|---:|---:|---:|
| vollständiges Host-CTest | 25/25 | 25/25 | entfällt | entfällt |
| Lichtprüfungen Disc 1, pro Lauf | 11.049 | 11.049 | 11.049 | 11.049 |
| Lichtprüfungen Disc 2, pro Lauf | 8.399 | 8.399 | 8.399 | 8.399 |
| privater Core-/GameTask-Boot, je Disc/Lauf | 1.913 | 1.913 | 1.913 | 1.913 |
| vorhandene assetfreie GameRuntime-Prüfungen | 634 | 634 | 634 | 634 |
| native Core-Suite, zwei Geräteläufe | über CTest | über CTest | 13.386 | 13.387 |
| Ressourcen / Raumvorkommen je Runde | 308 / 142 | 308 / 142 | 308 / 142 | 308 / 142 |
| Original-REL-Dateien je Runde | 114 | 114 | 114 | 114 |
| APK-Instrumentierung | entfällt | entfällt | 20/20 | 20/20 |

Originaldaten-CLI, Ressourcen und Module laufen pro Plattform zweimal.
Die Abweichung von einer Core-Prüfung auf ARM64 ist die vorhandene zusätzliche
Prüfung hoher Pointeradressbits (`port/src/scheduler_verification.cpp:96`),
kein Unterschied des Taskverhaltens. Alle sieben Scheduler-/Hintergrund-/Yz2-
Spuren stimmen zwischen den Geräteläufen und ABIs überein.

Die Lichtprüfungen umfassen Record-/Cut-/Pathreferenzen, unabhängige BE-
Scalar-Prüfsummen für Position, Radius, Intensität, ParentNo, HitRadius und
Spotnormalen, echte Typ-Callbacks, Farben/Integerzustände vor und nach dem
weiteren Move sowie einen geometrischen Invariantentest für Richtungslicht.
Die Integer-/Farbzustands-Prüfsummen werden zwischen allen vier ABIs und
beiden Wiederholungen verglichen. Gleitkomma-Trigonometrie wird nicht als
bitidentische GameCube-Ausgabe behauptet.

Identische abschließende Licht-Semantikhashes einschließlich der berechneten
Integer-/Farbzustände:

- Disc 1: `f1cbbdd0b1be316b3c36289ec5f6e4ca97c35648ac9ffc5a692ca7e6f73fbf97`
- Disc 2: `d6d76eefa0b331353f497c9e476c51d1251723ed80f752218e066d5158e189f1`

Weitere Prüfungen: Event-Kind `0x7F`, Kindbit 255, Power-Clamping, aktive
Archivwahl, Tune-Farben, Fog-Skalare, Freigabe, mehrfaches Reinitialisieren,
stale Handles, gehaltener Lease nach Freigabe, tatsächlich schlafender
Taskstack und abgewiesene Release-Anforderung, beschädigte Cut-/Pathoffsets
und abgeschnittene Header. Kein neuer Sanitizerlauf wurde ausgeführt;
Bounds-/Pointer-/Lebenszeittests sind kein Ersatz für ASan/UBSan.

Die früheren Modell-/Motion-/Event-, Message-/Font-, SubScreen-/Inventar-/Map-,
Cockpit-/ID-/Save-, Modul-, Scheduler- und Controllerprüfungen bleiben
enthalten. Pro ABI sind erneut 180 originale `modelInit`-Aufrufe in EVDs,
48 isolierte Inventar-/Map-Modellinitialisierungen, 76 `SetPartsSub`,
132 Event-`MotionSetCore` und 264 Player-`MotionSetCore` belegt.
Alle 37 r120-BIN-Vorkommen bleiben über die vorhandenen Ressourcenbindungen
abgedeckt; das ist kein vollständiger `PlayerInit`-Nachweis.
Die Ressourcenregression prüft 157/151 Ressourcen und 85/57 Raumvorkommen
je Disc, außerdem 63 isolierte Adapterprüfungen. Die Modulregression prüft
110/4 REL-Dateien und 216 isolierte Adapterprüfungen. Ein vollständiger
nativer Lauf aller REL-Funktionen wird nicht behauptet.

Die assetfreie APK stoppt GameTask weiterhin bei `cMes.gameInit`; der
weiterführende Originaldaten-Startnachweis stammt aus den nativen CLI-Läufen.
`R120Init`, TexRender und der Original-Sleep-/VI-Pfad von `R120Event` bestehen
weiter bis zur unveränderten Grenze `Sofdec.Initialize`. Das Event wird
nicht übersprungen oder künstlich abgeschlossen.
Controllerprüfungen dieser Sitzung sind automatisierte Adapterregressionen;
der frühere physische Tasten-/Stick-/Triggernachweis wurde nicht wiederholt.

Vulkan-Nachweise der APK: Adreno 650, EFB 512×448, 17 verifizierte GPU-Clears
und 17 Presents auf ARM32, 16/16 auf ARM64. Die unterschiedlichen Framezahlen
folgen dem Display-/Lifecycle-Timing. Effektiv Samples 1, AF 1, DoF aus.
MSAA Aus/2×/4×, AF 2×/4×/8×, DoF mit Nah-/Zielfokus, Persistenz und
hardwarevalidierte effektive Optionen bleiben verbindlich; dieses Update
ändert weder den Vulkan-Bootstrap noch die zentrale `PadRead`-Anbindung.

Fehlgeschlagene Zwischenversuche wurden korrigiert und sind nicht als Erfolg
gezählt: Die erste Sandbox-CTest-Ausführung scheiterte bei acht Gruppen an
Datei-/Tempzugriffen; die zugelassenen vollständigen Wiederholungen bestehen.
Frühe Compile-/Linkversuche erkannten native Alignment-/Typdeklarationen,
fehlende Vektor-/LIMIT_ANGLE-Abhängigkeiten und eine ARM64-Overflow-Prüfung.
Ein Lebenszeittest hatte zunächst nur einen wartenden statt bereits
ausgeführten/schlafenden Task; jetzt wird der belegte Stack geprüft.
Der erste letzte Host-Buildaufruf scheiterte an Shell-Quoting und wurde mit
separaten MSVC-Batchaufrufen ausgeführt. Alle abschließenden Nachweise sind
aus den korrigierten Builds.

## Verbleibende konkrete Grenzen

- `PlayerInit` (`src/game/player.cpp:107`) ist der nächste gemeinsame
  GameTask-Verbraucher. Dafür müssen die tatsächlichen Player-/Manager-
  Abhängigkeiten untersucht und gemeinsam angebunden werden. Seine
  umfangreichere Integration wurde nicht in diesen Lichtauftrag vorgezogen.
- EmMgr/SMD/Etc/Obj-Identitäten, `cLight::setParent(cModel*)`, Parent-Parts-
  Welttransformation und Parenttodbehandlung sind noch offen. Auch die
  bisherige Event-Facade für `getKindLight`/Modellbindung ist noch nicht
  durch einen vollständigen Managerverbund ersetzt.
- `cLightMgr::move`, `lightMove` mit Kollision/Modelltod, `update`,
  `registCut`, der vollständige `setEnv`-/Interpolations-/Filter-/Wind-/View-
  Ablauf, Fog-Ausgabe, GX-Lichtbindung und Geometrieausgabe fehlen.
  `moveType` führt einzelne Original-Callbacks aus, keinen vollständigen
  Gameplay-Frame. Fog-Skalare und Tune-Werte sind belegter CPU-Zustand,
  kein Nachweis vollständiger Environment-Anwendung.
- Raum-Lichtbereiche liegen im originalen `SAR`-Block:
  `src/game/light_area.cpp` definiert Header und `LightAreaData` (0xD8,
  `AREA_HIT_DATA`) und benötigt Area-/Kollisionsprüfung, lebende Em-/Player-
  und Waffenobjekte sowie `cModelState`. Der bestehende Import erhält diese
  Daten; `LightAreaUpdate` wird hier noch nicht ausgeführt. Es gibt keine
  erfundenen Bereiche oder Ersatzfaktoren.
- r120-Parenttypen und weitere ungenutzte alte Workfelder benötigen
  Referenzverifikation. Full-LIT-Export und Save-/Load-Lichtzustände sind
  nicht fertig. Nicht geprüfte Handler benötigen reale passende Daten.
- Sofdec/CRI/Audio/Video sowie sichtbares Inventar, HUD und vollständige
  GX-Geometrie bleiben die zuvor dokumentierten Abhängigkeiten. Die
  eigentliche Vulkan-Beleuchtung gehört zur gesondert beauftragten
  Renderingphase. Phase 5 wurde nicht begonnen.

## Reproduktion, Nachweise und vollständige Dateien

Vorhandene Tools werden erweitert; es gibt keine separate Testpipeline.
In einer jeweils passenden MSVC-Developer-Shell:

```powershell
cmake --build port/build/host-ninja --parallel
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
ctest --test-dir port/build/host-x86 --output-on-failure

$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskAdb = 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$taskData = 'E:\Development\Android\RE4-Data\import-phase3a'
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python $taskPython -Offline
.\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc
& $taskPython port/tools/test_game_runtime.py --data $taskData --output E:\Development\Android\RE4-Data\phase4c-light-host --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data $taskData --output E:\Development\Android\RE4-Data\phase4c-light-host --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data $taskData --output E:\Development\Android\RE4-Data\phase4c-light-device --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/summarize_consumers.py --host E:\Development\Android\RE4-Data\phase4c-light-host --device E:\Development\Android\RE4-Data\phase4c-light-device --output E:\Development\Android\RE4-Data\phase4c-light
```

Zusätzlich `test_resources.py` und `test_modules.py`, jeweils mit
`--data $taskData --output <privates Ziel> --host-exe <passendes Tool> --repeat 2`
für beide Hosts, dann `--adb $taskAdb --serial dbdcacc --repeat 2`.
Die zugehörigen Host-Tools heißen `re4_resource_tool.exe` und
`re4_module_tool.exe`.

Vollständige Messberichte mit Executable-Hashes, Disc-/Ressourcenidentitäten
und Zustandsspuren liegen außerhalb von Git in
`RE4-Data/phase4c-light-host`, `-device`, `-resources`, `-modules` und
`phase4c-light/consumer-runtime-summary.json`. Die Zusammenfassung verlangt
identische relevante Semantik über alle vier ABIs und Wiederholungen.
Build-/Testlogs liegen im ignorierten `port/build/light-*.log`,
`device-<ABI>-round<N>.txt`, `instrumentation-<ABI>.txt` und
`graphics-<ABI>.json`.

Neu, vollständig gespeichert, relativ zu `port/`:

- `config/light-sources.json`
- `include/re4/port/lights.h`
- `src/lights.cpp`, `src/light_original.h`
- `tools/generate_lights.py`
- `docs/phase4c-light-status.md`

Verändert, vollständig gespeichert:

- `CMakeLists.txt`, `include/re4/port/game_runtime.h`
- `src/game_runtime.cpp`, `src/runtime_original.h`
- `tests/game_runtime_tool.cpp`, `tests/test_consumer_sources.py`
- `tools/generate_runtime.py`, `tools/test_game_runtime.py`,
  `tools/summarize_consumers.py`

Die vollständigen generierten Originalkörper/Typdateien stehen zusätzlich in
den Buildverzeichnissen. Keine GCM-Dateien, extrahierten Assets oder privaten
Messdateien werden in Git übernommen. Die Originalimages wurden nicht verändert.

`master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`;
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die aktualisierte README wurde berücksichtigt. Kein Merge, Rebase oder Push,
keine README- oder Originalcodeänderung. Der geprüfte Stand wird ausschließlich
in einem lokalen Commit auf `android-port` gesichert.
