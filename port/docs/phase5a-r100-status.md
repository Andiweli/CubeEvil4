# Phase 5a: r100 und originale gruppierte SMD-Erstellung — geprüfter Teilstand

Dieser Bericht dokumentiert den Stand vor der Fortsetzung von `c5c988d0`.
Der [Folgebericht](phase5a-scene-ownership-light-diagnostics.md) beschreibt
originale CNS-Pools, Modell-Borrows, korrigierte Event-LIT-Referenzen und echte
unbeleuchtete r100-GPU-Diagnosen. Die originale Spieler-Startszene bleibt offen.

Fortsetzung auf `android-port`, Ausgangscommit
`e285a6d6d6e37fcfefed3ed10e7f350c833e3ac2`, 11. Oktober 2026.
Windows x86/x64 und Retroid Pocket Flip 2 `dbdcacc`, Android 13/API 33,
`armeabi-v7a` und `arm64-v8a`, Adreno 650, Vulkan 1.1.128.

## Ergebnis und Abnahmegrenze

Die ursprüngliche r100-Ladeblockade an gruppierten SMD-Works ist beseitigt.
Die echten Originalfunktionen erstellen alle **310 Modellobjekte**, einschließlich
der **fünf Originalblöcke**. Ihre BIN-Hierarchien, Platzierungen, gemeinsamen
TPL-Referenzen und SMX-Initialzustände werden über die bestehende Modellverwaltung
aufgelöst. Die gemeinsame Implementierung besteht zusätzlich die Prüfungen
mit r102 und r119 auf allen vier Architekturen.

**Phase 5a ist damit noch nicht abgeschlossen.** Der vollständige r100-Renderpfad
stoppt jetzt bei `original model-bound light registration`. Seine LIT-Daten
referenzieren den echten Gameplay-Spieler. Dessen vollständiger Konstruktor,
Startzustand und die davon abhängige Schulterkamera laufen noch nicht nativ.
Es gibt deshalb **keinen r100-GPU-Startframe, keine ausgeführte originale
Gameplay-Startpose und keinen Schulterkamera-Nachweis** in diesem Stand.
Die neue Diagnose `scroll` ist ausdrücklich nur ein CPU-Registrierungs- und
Geometrienachweis; der Vulkan-Adapter lehnt solche unvollständigen Szenen ab.

Die vorhandene r120-GPU-Ausgabe mit Leons registrierter Hierarchie wurde erneut
geprüft. Das ist eine Regression des bestehenden isolierten EVD-Renderpfads,
kein Ersatz für r100 und kein Nachweis vollständiger Intro- oder Gameplayausführung.
`GameTask` bleibt bei `MerchantGameInit()`, `R120Event` bei
`Sofdec.Initialize("movie/opening.sfd", 0)`.

## Tatsächliche Original-Aufrufkette

`port/tools/generate_scroll.py` übernimmt 22 vollständige Funktionsbodies aus
den unveränderten Originaldateien. Normalisierte Quellfingerprints werden
vor jeder Generierung geprüft; Änderungen am Original werden abgewiesen.
`scroll-manifest.json` im jeweiligen ignorierten Buildverzeichnis verzeichnet
Quellpfad, Funktionssignatur, Zeile und jede notwendige Anpassung.

- `src/game/scroll.cpp`: `SmdInit`, `SmdClear`, `SmdSetup`, `setObj`, `workInit`,
  `SmdSetParam`, `SmxSetFlag`, beide `smxInit`-Overloads, `BlockCreate`,
  `BlockDestroy`, `SmdGetGroupNext`, `cSmd::getWorkNum`, `getWorkPtr`,
  `getBinPtr`, `getTplPtr`, `getMotPtr`.
- `include/cManager.h`: `createBack`, `getPrevWork`.
- `src/game/model.cpp`: `cModel::setNoSuspend`.
- `src/game/obj.cpp`: `cObj::cObj`; `src/game/obj02.cpp`: `cObjScr::cObjScr`.
  Die beiden Konstruktorbodies werden als benannte Initialisierungsmethoden
  des nativen Feldviews ausgeführt.

Die Kette lautet `SmdInit -> SmdSetup -> setObj -> createBack ->` originale
Objektinitialisierung `-> workInit -> SmdSetParam -> cModel::modelInit ->`
optionale `MotionSetCore`-Initialisierung `-> smxInit -> matUpdate`.
Die bestehende Originalimplementierung von `cModInfoMgr::create`,
`cModel::modelInit`, Joint-/Parts-Erstellung, `cCoord::matCalc`,
`partsMatCalc/partsWorldCalc`, `cLightInfo::init2/updateMatrix` und CPU-Skinning
wird wiederverwendet. Es gibt keine neue Raumgeometrie oder Platzierungstabelle.

Die Originalfunktionen `BlockCreate/BlockDestroy` werden auf die tatsächlich
geladenen Block-SMDs angewendet. Die Poolreihenfolge von `createBack` und
die anschließende Listenreihenfolge bleiben erhalten. Gemeinsame BINs werden
über das originale Flag `0x10` im gemeinsamen SMD aufgelöst. Die originale
leere `DmyZeroTpl`-Palette und angehängte gemeinsame TPL ersetzen keine Texturen;
sie bilden genau diesen Original-Ressourcenpfad ab.

## Gruppierte Daten und Ressourcenabdeckung

Quelle ausschließlich `E:\Development\Android\RE4-Data\import-phase3a` und
deren hashgeprüfter Katalog von Disc 1, `disc_number=0`.
`St1/r100.das` hat den SHA-256
`d80eee4b9d0b7abb1b7737f69cae16060f10ecb5b931ee91564ea7e27fb334da`.
Die vorhandene Disc-2-Abdeckung bleibt in den Ressourcen-/GameRuntime-Tests geprüft.

r100: SMD-Version `0x40`, Flag `1`, 13 primäre Works, fünf Gruppen mit
`11, 99, 61, 52, 74` Works. Der originale `getWorkNum()` ergibt 310.
Die Gruppenzahlen beschreiben zusätzliche Block-SMDs; sie sind keine
310 zusammenhängenden Records im primären Archiv. Das gemeinsame SMD hat
18 Works. Die realen `St1/r100_00.dat` bis `r100_04.dat` stimmen jeweils
mit den Gruppenzahlen überein. 198 Block-Works verwenden gemeinsame BINs.

| Raum | Erstellte Modelle | Draw-Gruppen | Dekodierte Vertices | Dreiecke | Dekodierte TPL-Texturen |
|---|---:|---:|---:|---:|---:|
| r100 | 310 | 434 | 364.926 | 337.138 | 483 |
| r102 | 46 | 146 | 27.569 | 25.649 | 104 |
| r119 | 180 | 290 | 164.757 | 137.095 | 49 |

Texturzahlen enthalten die tatsächlich aufgelösten lokalen/gemeinsamen
Palettenbindungen; sie sind keine Zahl weltweit eindeutiger Disc-Dateien.
Die unabhängige Python-Referenz prüft Archiv-/Blockhashes, Modell-/BIN-/TPL-IDs,
Materialgruppen, Primitive, Texturhashes, SMX-Farben, Alpha, Culling, OT-Typen
und UV-Zustände. Ihre vorhandenen Dekoder wurden um den auf r100 tatsächlich
auftretenden RGBA8-Zweiebenenpfad als unabhängige Referenz erweitert;
der native RGBA8-Dekoder war bereits vorhanden.

Der Katalog wird aus tatsächlichen FST-Pfaden erzeugt. Die Engine lädt Blöcke
über die vorhandenen BLK-/ReadAreaData-Schnittstellen. Für r100 gibt es
keinen speziellen Parser, Renderer, fest eingebauten Blockzähler oder Modellpfad.
Die zwei weiteren Räume besitzen in diesem geprüften FST keine zusätzlichen
bare `.dat`-Blockdateien; sie prüfen den gemeinsamen nicht gruppierten Pfad.
Ein zweiter realer Raum mit diesem fünfteiligen Gruppentyp wird nicht behauptet.

**Blockaktivierung bleibt abgegrenzt:** Die Renderdiagnose erstellt alle
geladenen Originalblöcke gleichzeitig. Das ist der ausdrücklich bezeichnete
All-resident-Diagnosezustand. Die originale spielerabhängige Auswahl in
`cBlock::check`, `checkBlockArea` und `checkBlockConnect`
(`src/game/block.cpp:243/306/350`) ist noch nicht angeschlossen.
Ein erfolgreicher ARAM-/MRAM-Datentransfer bedeutet weiterhin nicht automatisch
einen vollständigen Live-Blockzustand.

## Native Anpassungen und Lebenszeiten

- Die Big-Endian-Records (SMD 72 Bytes, SMX 144 Bytes) werden in geprüfte native
  Feldviews dekodiert; niemals auf native 32-/64-Bit-Klassen überlagert.
  Gruppen, Tabellenoffsets, IDs, endliche Koordinaten und Ressourcenbereiche
  werden vor Verwendung geprüft.
- Pointertabellen verwenden `sizeof(cObj*)` und native Pointerarithmetik.
  Originale GameCube-Adressbereichsprüfungen werden durch Ownership-Prüfungen
  ersetzt. Keine Pointer-Trunkierung nach `u32`.
- Das originale `setObj` liest Slot `0xFE` vor seiner Sentinelprüfung, obwohl
  die ID-Tabelle nur 250 Einträge hat. Der Adapter prüft den Sentinel zuerst;
  dies verhindert den unbeabsichtigten nativen Out-of-bounds-Lesezugriff,
  ohne registrierte IDs oder die Originalentscheidung zu verändern. Der
  Generator-Regressionstest schützt diese explizite Anpassung. Die realen
  Testdaten führen sie für 183 r100-, sieben r102- und 62 r119-Modelle aus.
- Der Objekttyp und die originalen Constructor-/`workInit`-Flags werden **vor**
  `cModel::modelInit` an den bestehenden ModelRuntime übergeben. Andernfalls
  würde dessen originale `kindid == 0`-Abzweigung Raumobjekte als Actor behandeln.
  Die Standardparameter anderer vorhandener Modellnutzer bleiben erhalten.
- Die vollständigen 120 von `smxInit` kopierten Arbeitsbytes bleiben in
  Original-Byteordnung. Der enthaltene Specular-Farbwert besitzt zusätzlich
  einen separat dekodierten nativen Wert; seine Little-Endian-Repräsentation
  darf den Roharbeitsbereich nicht verändern.
- BIN/TPL/FCV/Blockdaten besitzen gemeinsame immutable Owners. Modelle nutzen
  bestehende ConsumerHandles und Modell-/Parts-Pools. CPU-Modelle werden erst
  an einer quieszenten Diagnosegrenze freigegeben; Task-Borrower werden nicht
  künstlich übergangen. Die Geometriesnapshots halten ihre Ressourcen weiterhin.
- Pro Diagnoseaufruf werden fünf Blöcke erstellt, zerstört, nochmals erstellt
  und zerstört; danach werden Primärmodelle freigegeben und beide aktiven
  Modell-/Objektzähler müssen null sein. Derselbe Runtime-Owner lädt anschließend
  den Raum nochmals. Zwei CLI-Runden pro ABI ergeben für r100 2.428 tatsächliche
  Modellinitialisierungen und Freigaben, einschließlich Blockwiedererstellung.

Das native Obj-Pool-/Lease-Ownership ist ein Plattformadapter, kein vollständig
portierter `cObjMgr` mit sämtlichen Spielobjektklassen und Live-Destroy-Modi.
Die originalen rotierenden/schwingenden `cObjScr::move`-Callbacks, SMX-Zeitverlauf
und FCV-/SEQ-Wiedergabe werden in dieser statischen Diagnose nicht ausgeführt.
Die geprüften SMD-Works dieser Räume enthalten `MotId=0xFF`; eine bewegte
Scroll-Motion wird deshalb aus diesen Daten nicht als nachgewiesen behauptet.
`SmdGetGroupNext/getPrevWork` sind quellgeprüft eingebunden, aber ein vollständiger
Live-Raumcallback mit dieser Zugriffskette wird hier nicht als ausgeführt gezählt.
Die zusätzliche TPL-Bindung ist ein geprüfter nativer Lease-/Palettenadapter
nach `cModelInfo::addTplAddr` (`src/game/model.cpp:750`), kein weiterer der
22 erzeugten Originalbodies.

Für eine Live-Szene müssen SMD-/Actor-/Eventmodelle einen gemeinsamen,
aus den Originalkapazitäten abgeleiteten Pool behalten. Die aktuelle Diagnose
konfiguriert Pools für die tatsächlich verwendeten SMD-Modelle und gibt ihre
CPU-Objekte vor der nachfolgenden Licht-/GPU-Snapshot-Auswertung frei.
Diese Grenze darf nicht zur späteren Auflösung von ParentType 2/4 über lebende
Objekte verwendet werden. Gemeinsame persistente Pools, echte Actorhandles und
die zugehörige Raumfreigabereihenfolge sind daher weitere konkrete Integrations-
voraussetzungen; vorhandene Borrower-Prüfungen dürfen dafür nicht deaktiviert werden.

## Nächster tatsächlicher r100-Renderblocker

Nach SMD-/Block-Erstellung und originaler EVD-Kameraauswertung scheitert
`captureOriginalRoomLighting -> LightRuntime::loadRenderCut`
(`port/src/gx_pipeline.cpp`, `port/src/lights.cpp:229`) ausdrücklich an
modellgebundenen Lichtern. Die vollständige Renderanforderung wird auf
Windows x86/x64 und Android ARM32/ARM64 mit Exit 1 und demselben Consumerfehler
geprüft. Der hierfür gewählte EVD-Cut ist **keine Gameplay-Schulterkamera**.

Alle 16 LIT-Cuts von r100 enthalten ein ParentType-1-Licht mit ParentNo 0.
Im Cut 0 ist dies Lichtrecord 17. Die tatsächlichen BE-Felder liegen bei
Recordoffset `0x1c` (`ParentType`) und `0x20` (`ParentNo`). Der Originalpfad
`cLight::calcParent()` (`src/game/light.cpp:1236`) löst diesen Parent über
`EmMgr.getEmPtr(0,0)` auf; `getCoord`, `calcPos` und `getNormal` verwenden seine
realen Parts-/Weltmatrizen (`1268/1288/1340`).

Das bereits registrierte EVD-Modell `pl0000` ist ein Event-Objekt. Es darf
nicht als Gameplay-Player ID 0 eingesetzt werden. Der Originalcode trennt
`pPL` und `PPl` sowie die SetPl-/SetOm-Pakete (`src/game/event.cpp`).
Der Lichtrecord wird weder gelöscht noch deaktiviert; seine Position und
Beleuchtungswirkung werden nicht erfunden. Das vorhandene SAR-System und
live modellgebundene Lichter bleiben offen.

## Originale Startpose und Schulterkamera: Fakten und fehlende Ausführung

Nachgewiesene Quellfakten:

- `cPlLeon::cPlLeon()` (`src/game/pl_leon.cpp:25`) führt `init0`, `setModel`,
  `weaponRelease`, `weaponLoad`, `weaponInit`, `init1`, `setMotion`,
  `EspDataLoad`, `startUp` und die Fußschattentabellenzuordnung in dieser
  Reihenfolge aus. Die vorhandene native Assetinitialisierung führt echte
  `setModel/setMotion`-Bodys aus, aber noch nicht diesen vollständigen Konstruktor.
- `cPlayer::startUp()` (`src/game/player.cpp:277`) übernimmt `pG->pl_pos` und
  `pl_ang_y`, setzt die Rnos, führt `move`, zweimal `motionMove`,
  `SatMgr.check` und nochmals `matUpdate` aus. Ein direkt gesetzter Transform
  mit ausgewähltem FCV-Frame würde diese Sequenz nicht ersetzen.
- `r100_StartEvent()` (`src/st1/r100.cpp:550`) lädt im tatsächlichen Code
  `readEvent(9,1,&evt)`; die Tabelle bei Zeile 411 ordnet Index 9
  `evd/r100s40.evd` zu. Nach dem Originalevent setzt es die Spielerposition
  auf `(-99685,-484,-1343)` und Y-Winkel auf `2.246` (`582/588`).
  Der Quellenkommentar „s01“ ist hierfür kein belastbarer Ersatz für die Tabelle.
  Diese Konstanten wurden **nicht** in einen künstlichen Startframe kopiert.
- `CameraControl::roomInit()` (`src/game/cam_ctrl.cpp:1017`),
  `CameraQuasiFPS::bindDefaultCamera/init/move`
  (`src/game/cam_qfps.cpp:1050/1152/1189`) und `CameraMove`
  (`src/game/camera.cpp:73`) benötigen den tatsächlichen Spielerzustand.
  `setPlayerLocation/calcBaseMatrix` (`cam_qfps.cpp:354/372`) verwenden dessen
  Matrix, Bodennormale, Suchzustand und gegebenenfalls Crouch-Korrektur.
- `checkCameraType` und `hitCheck` (`cam_qfps.cpp:496/705`) benötigen Waffen-,
  Ziel-/Bewegungszustände und echte Kollision. `cameraHitCheck`
  (`cam_ctrl.cpp:354`) ruft `EmHitCheck`, `ObjHitCheck` und
  `SatMgr.hitCheck(...,0x8000,0x1C2810)` auf und wählt den nächsten Treffer.

**Offene technische Abhängigkeiten:** vollständige `cPlayer/init0/init1`,
Actor-Registrierung als echter Gameplay-Player, originale Waffen- und
MotionBase-Initialisierung, notwendige SEQ-/State-Funktionen, SAT-/Actor-/Objekt-
Kollision, originale StartEvent-Ausführung und Kamera-Runtime mit ihrer
Framehistorie. Das ist keine bereits getestete Native-Startpose.
Die originale PadRead-/Controlleranbindung bleibt unverändert; es gibt keine
zweite Bewegungs- oder Kamerasteuerung.

## Tatsächlich ausgeführte Tests

| Prüfung | Windows x64 / x86 | Retroid ARM32 / ARM64 | Umfang |
|---|---|---|---|
| CMake/NDK/Gradle | Beide Hostbuilds erfolgreich | Beide CLI/JNI-ABIs, Debug-APK/Test-APK und Lint erfolgreich | Kein Asset im APK |
| CTest | Je 27/27 bestanden | Entsprechende native Geräteprüfungen separat | Original-/Adapter-/Generator-/Yz2-Suiten |
| GX-Adapter | Je 617 Checks, 0 Fehler | Je 617 Checks, 0 Fehler | SMD-Grenzen, Originalkonstruktoren, OT/Material/Skinning, SDK-Matrizen |
| Private SMD-Daten | r100/r102/r119, je zwei Runden | Dieselben Räume/ABIs, je zwei Runden | Echte Originalerstellung, Freigabe, Blockwiedererstellung, unabhängige Referenz |
| GameRuntime | Beide Discs, je zwei Runden | Beide Discs/ABIs, je zwei Runden | 927 isolierte Runtimechecks; echte Player-/Modell-/MDT-/SubScreen-/Save-/ID-/Licht-/Abbruch-/Taskprüfungen |
| Ressourcen | 157/151 Ressourcen; 85/57 Räume pro Disc, zwei Runden | Gleicher Umfang, beide ABIs | ARAM/MRAM/Lesen/Dekomprimierung/Lebenszeiten |
| Module | 110/4 Original-RELs pro Disc, zwei Runden | Gleicher Umfang, beide ABIs | Native Teilregistrierung, tatsächliche Init-Aufrufe; keine vollständige REL-Emulation |
| Weight/Motion | Je 18 Ausführungen, 1.739.820 Checks | Gleicher Umfang, beide ABIs | Originalalgorithmen und unabhängige skalare Referenz |
| Scheduler/Background/Pad | Bestehende CTests | Je zwei Runden, ARM32 13.386 / ARM64 13.387 Checks | Ein zusätzlicher Hochpointer-Check auf ARM64; gleiche Schedulertraces |
| APK/Surface | APK gebaut, kein Windows-GPU-Claim | Je 20/20 Instrumentationstests | Controller/JNI, reale Clears/Präsentation und Surface-Lebenszeiten |
| r120 + Leon GPU | Je zwei CPU-Referenzrunden | Je zwei GPU-Aufnahmen/Activity-Neuerstellungen | Originaler Cut 1, Frame 30, `pl0000`; nicht r100/Gameplay |
| Vollständige r100-Renderanforderung | Erwarteter Consumerabbruch | Erwarteter Consumerabbruch | Modellgebundenes Licht nicht stillschweigend ausgelassen |

Die vielen Ressourcen-/Runtimeprüfungen behalten ihre bisherigen expliziten
Grenzen. Insbesondere zählen geladene Assets, erfolgreiche Scalar-PlayerInit
und ein importierter Raum nicht als ausgeführter kompletter Spielstart.
ASan/UBSan wurden nicht ausgeführt; Speicherbelege sind hier Ownership-/Bereichs-
und Freigabeprüfungen sowie Vulkan-Validation, kein vollständiger Sanitizerlauf.
Neue physische Controllerbetätigungen wurden nicht verlangt oder behauptet;
die Controllerregressionen sind reproduzierbare synthetische Adapterprüfungen.

### Numerische ABI-Abweichungen

Raw-Float-Geometriehashes sind nicht als plattformübergreifendes Bestehenskriterium
geeignet. Alle ausgegebenen Vertices und Indizes werden privat binär erfasst.
Indizes, Quellfarben, UVs und Quellnormalen stimmen bytegleich überein.
Gegen Windows x64 betragen die gemessenen Maximalabweichungen:

| Raum / Vergleich | Vertexposition | Transformierte Normale |
|---|---:|---:|
| r100: Windows x86 | 0,00006103515625 | 0,000000119209289551 |
| r100: beide ARM-ABIs | 0,015625 | 0,000000715255737305 |
| r102: Windows x86 | 0 | 0 |
| r102: beide ARM-ABIs | 0 | 0 |
| r119: Windows x86 | 0,001953125 | 0,000000357627868652 |
| r119: beide ARM-ABIs | 0,00048828125 | 0,000000238418579102 |

Positionen sind Originaleinheiten, keine angenommene Meterangabe. Der Test
verlangt endliche Werte, bytegleiche Quellattribute/Indizes sowie Delta
maximal 0,05 für Position und 0,000002 für Normalen. Diese Grenzwerte und
Messwerte betreffen **native SDK-Transformationen**, keinen aufgenommenen
PPC-Trace und keinen visuellen Originaltreuenachweis. Die Quantisierung der
separaten Weight-Referenz ergibt weiterhin null abweichende quantisierte Werte.

### GPU-Bilder und Validation

Die neuen GPU-Readbacks liegen außerhalb von Git unter
`E:\Development\Android\RE4-Data\phase5a-r100-regression-device`:

- `gpu-armeabi-v7a-1.png`, `gpu-armeabi-v7a-2.png`
- `gpu-arm64-v8a-1.png`, `gpu-arm64-v8a-2.png`
- Jeweils passende `.json`/`.rgba` sowie `display-*.png` der tatsächlichen Anzeige.

Alle vier RGBA-Readbacks sind byteidentisch, SHA-256
`248718bbbef050b332cbe9f5d6c4d8cfc325a42f592e574d94a79c06a95acf75`,
220.600 nicht leere Pixel. Validation-Layers sind aktiviert: **0 Fehler**,
je Aufnahme **1 bekannte Warnung** zur `IDENTITY`- gegenüber `ROTATE_90`-
Surface-Pretransform. Die Output-/Activity-Lebenszeiten werden durch
Neuerstellung im selben Prozess geprüft. Dies ist keine Messung der
vollständigen Gameplay-Framerate; keine GPU-Timestamps wurden ergänzt.
Die erfassten letzten CPU-Framezeiten liegen für ARM32 bei 50,594/50,509 ms
und für ARM64 bei 50,898/46,553 ms. Sie enthalten den synchronisierten
Diagnose-/Readbackpfad und sind keine reine GPU-Zeit oder Durchschnittsframerate.

Die visuell inspizierte bestehende r120-Diagnose bleibt deutlich dunkel.
Ein identischer Originalzustand mit gleicher Kamera, Pose, Eventhistorie
und Animationsframe ist nicht nachgewiesen. Der vom Nutzer bereitgestellte
r100-Start-Screenshot ist kein direkter Vergleich zu dieser r120-Diagnose.
Der verlangte visuelle r100-Startvergleich bleibt ausdrücklich **offen**.

## Reproduktion und Belege

Reproduzierbare SMD-Prüfung; `100` kann durch `102` oder `119` ersetzt werden:

```powershell
python port/tools/test_gx_scene.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase5a-r100-smd-host64 --host-exe port/build/host-ninja/re4_gx_tool.exe --room 100 --smd-only --repeat 2
python port/tools/test_gx_scene.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase5a-r100-smd-host32 --host-exe port/build/host-x86/re4_gx_tool.exe --room 100 --smd-only --geometry-reference E:/Development/Android/RE4-Data/phase5a-r100-smd-host64 --repeat 2
python port/tools/test_gx_scene.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase5a-r100-smd-device --adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe --serial dbdcacc --room 100 --smd-only --geometry-reference E:/Development/Android/RE4-Data/phase5a-r100-smd-host64 --repeat 2
```

Für die GPU-Regression dasselbe Tool ohne `--smd-only`, mit
`--room 120 --event Evd/r120s00.evd --cut 1 --frame 30 --model pl0000` und
jeweils eigenem privaten Output aufrufen. Die vollständige r100-Probe:

```text
re4_gx_tool scene <privater Root> <Katalog mit Raum/5 Blöcken/Core/EVD> 0 100 Evd/r100s03.evd 0
```

Erwarteter Exit 1: `Original consumer unavailable: original model-bound light registration`.
Die EVD-Kamera dieses Aufrufs wird nicht als Startkamera bezeichnet.

Regressionssuiten `test_game_runtime.py`, `test_resources.py`, `test_modules.py`
und `test_skinning.py` verwenden `--data`, getrennten privaten `--output`,
`--repeat 2`, jeweilige `--host-exe` oder `--adb/--serial` wie zuvor.
Zusätzlich `port/scripts/test-device.ps1` und `test-app-device.ps1`.
Builds über die vorhandenen Host-CMake-Verzeichnisse und
`port/scripts/build-android.ps1 -Offline -ValidationLayers <private Layers>`.

Logs liegen ignoriert unter `port/build/r100-*.log`. Private JSON-/Binärbelege
unter `RE4-Data/phase5a-r100-*`, darunter `smd`, `crossroom-102`, `crossroom-119`,
`game-host64/host32`, `game_runtime-device`, `resources`, `modules`, `skin-host*`,
`skinning-device` und `regression`. `geometry-comparison.json` verzeichnet die
tatsächlich verglichenen Vertex-/Indexzahlen und Maximalabweichungen.

Anfangs scheiterten ADB-Tests wegen eines getrennten USB-Geräts; nach der
Nutzerbestätigung der erneuten Verbindung wurden sie wiederholt. Temporäre
Datei-/Gradle-Cachezugriffe außerhalb der Sandbox benötigten die vorgesehene
Freigabe. Diese Umgebungsfehler werden nicht als Engine-Erfolg gezählt.
Die erste unabhängige r100-Texturprüfung scheiterte außerdem an ihrem fehlenden
RGBA8-Referenzpfad. Dieser wurde anhand des tatsächlichen GX-Layouts ergänzt;
die nachfolgenden Originaldaten-/Texturprüfungen bestehen. Die vollständige
r100-GPU-/Gameplay-/Schulterkamera-Abnahme bleibt dagegen weiterhin offen.

## Vollständig gespeicherte Dateien

- `port/CMakeLists.txt`, `port/config/scroll-sources.json`
- `port/include/re4/port/{consumers,gx_scene}.h`
- `port/src/{consumers,gx_scene,model_runtime,scroll_scene}.cpp`
- `port/src/{model_original,scroll_original}.h`
- `port/platform/android/vulkan_bootstrap.cpp`
- `port/tests/{gx_tool.cpp,gx_verification.h,test_gx_sources.py}`
- `port/tools/{generate_scroll.py,test_gx_scene.py}`
- `port/docs/{phase5a-r100-status,phase5a-material-status}.md`, `port/README.md`

Generierte C++-Bodys, Fingerprintmanifest, Builds und Logs bleiben im ignorierten
Buildverzeichnis. Alle Originaldaten, dekodierten Geometrien und GPU-Bilder
bleiben ausschließlich im privaten Datenverzeichnis. Keine Originaldatei
unter `src/`, `include/` oder `tools/` wurde verändert.

## Nächster konkreter Schritt innerhalb der noch offenen Phase 5a

Die echte `cPlLeon`-/Gameplay-Actor-Erstellung entlang ihres Originalkonstruktors
anschließen: `init0 -> weaponLoad/weaponInit -> init1 -> startUp`, mit den
benötigten vorhandenen pl00-/Waffen-/Motion-/SAT-Ressourcen und echten
Actorreferenzen. Danach originale modellgebundene Lichttransformationen
anschließen, `r100_StartEvent` ohne Eventüberspringen ausführen und die
ursprüngliche `CameraQuasiFPS` einschließlich Kollisionsprüfung und Historie
verwenden. Erst dieser Zustand eignet sich für r100-Startframe und Vergleich.
Der vollständige Aufgabenabschluss wird nicht aus dem jetzigen CPU-Meilenstein
abgeleitet. Keine Phase 5b wurde begonnen.

Die zentrale Controller-/PadRead-Schicht, bisherigen GPU-Clears, Präsentation
und Draw-Pfade bleiben erhalten. MSAA Aus/2×/4×, AF 2×/4×/8×, Nah-/Zielfokus-DoF
und persistente hardwarevalidierte Einstellungen bleiben verbindlich gemäß
[graphics-settings.md](graphics-settings.md); aktuell ein Sample, AF 1, kein DoF.

Lokaler `master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die aktualisierte README auf `origin/master` wurde gelesen und berücksichtigt.
Keine automatischen Merges, Rebases oder Pushes.
