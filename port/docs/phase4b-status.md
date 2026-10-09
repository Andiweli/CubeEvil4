# Phase 4b – Gemeinsame native Laufzeit und Android-Controller

Stand: 10. Oktober 2026. Ausgangspunkt ist der lokale Phase-4a-Commit
`aec54e4e3abc7b1fc1691e2146fde0fe833d217c` auf `android-port`.
Alle geänderten Dateien liegen vollständig unter `port/` im Projekt.

Der überprüfte Meilenstein verbindet originale skalare Initialisierung,
PadRead, Stage-/Szenario-Orchestrierung, stackerhaltende Tasks, Modulverwaltung,
Dateilader und eigene Gameplay-Heaps. Physische Retroid-Eingaben wurden mit
ARM32 und ARM64 ausgeführt. Der vollständige GameTask, die gemeinsame
Raumfreigabe und die r120-Eventausführung bleiben offen. Deshalb ist dies ein
teilweise integrierter Phase-4b-Stand; das Spiel ist weiterhin nicht spielbar.

## Git und Datenquellen

| Referenz | Geprüfter Stand |
| --- | --- |
| `master` | `9b76ff003081d6ece8b838453fb8310743a972c7` |
| `origin/master` | `15cf64f80e08bbc64712c39be330b75d1512a0d0` |
| vorheriger `android-port` | `aec54e4e3abc7b1fc1691e2146fde0fe833d217c` |

Der Arbeitsbaum war anfangs sauber. Die README-Ergänzungen in `origin/master`
wurden gelesen und berücksichtigt: Wiederverwendung des Originalcodes,
getrennte Portstruktur, kein spielbarer Port und keine enthaltenen Assets.
Die 25 README-Zeilen wurden nicht durch einen Merge übernommen. Master und
origin/master bleiben unverändert; es gab keinen Merge, Rebase oder Push.
Alle bisherigen Entwicklungscommits bleiben Vorfahren des neuen lokalen Commits.
Die neue Commit-ID ist nach Abschluss mit `git log -1` sichtbar.

Originalquellen, Originalheader, Disc-Images und der Phase-3a-Import wurden
nicht verändert. Die echten Testdaten stammen ausschließlich aus
`E:\Development\Android\RE4-Data\import-phase3a`. Private Berichte liegen
außerhalb von Git in `phase4b`, `phase4b-host`, `phase4b-device`,
`phase4b-modules-host` und `phase4b-modules-device` unter demselben Datenverzeichnis.
Es sind keine GameCube-Assets im Commit oder APK enthalten.

## Tatsächlich übernommener Originalcode

`tools/generate_runtime.py` prüft 16 Quell-/Header-Fingerprints aus
`config/runtime-sources.json`. Es extrahiert 28 vollständige Funktionskörper
und die Nintendo-Padclamp-Einheit in das jeweilige Buildverzeichnis.
`original/runtime-manifest.json` dokumentiert Originalpfad, Signatur, Zeile und
jede textuelle Anpassung. Ein negativer Test verändert eine Originalkopie und
belegt die Ablehnung durch den Fingerprint. Die kompletten Engine-Translation-
Units werden nicht als bereits nativ kompiliert bezeichnet.

| Originalquelle und Funktionen | Ausführung / Grenze |
| --- | --- |
| `src/game/main.cpp`: `systemScreenInit`, `systemWorkInit`, `SetSystemVcnt`, `GetSystemVcnt` | Originale skalare Startwerte und Bildschirmmaße; Bootraum 0x120, Leon, Englisch, Modus 5. Systemnamen-Allokation im nativen Heap. Kein originales SystemMemInit/GX/VI-Setup. |
| `src/game/main_sub.cpp`: `SecToTime`, `InitGameTime`, `GetGameTime`, `SetGameTime` | Originale Sekunden-/Spielzeitberechnung; Plattformtimer liefert Sekunden. Pause akkumuliert Spielzeit, Resume setzt die Zeitbasis neu. |
| `src/game/game.cpp:194`: `DoorFlagInit` | Originale Szenarioflags mit gleicher Bitnummerierung; native Pointer werden vollständig erhalten. |
| `src/game/game.cpp:214`: `GameTask` | Originaler Rno-Reset und Dispatchkörper; Abbruch beim Aufruf des fehlenden `gameDebug`. Die sieben Dispatchziele sind ausdrücklich fehlschlagende Abhängigkeiten. |
| `src/game/stage.cpp`: `emListVillage`, `checkEmListNo`, `getEmListName`, `getEmListDbgName`, `getEmListNum`, `StageSet`, `readEmList` | Originale universelle ESL-Auswahl, Heap-/Modulübergänge und Dateianforderung. Message-Consumer bei StageSet derzeit erforderliche externe Abhängigkeit. |
| `src/game/sce_sys.cpp:49,121`: `ScenarioRoomInit`, `ScenarioMove`, `scenarioLoopBeforeInit`, `scenarioLoopAfterInit` | Originale Reset-/Aufrufreihenfolge; echte Raumcallbacks und Scheduler können ausgeführt werden. Item-, Condition-, Actor-, Area-, Event- und Mission-Consumer fehlen produktiv. |
| `src/game/sce_com.cpp:440`: `SceInitItemEvent` | Originaler Reset der 16 Itemevent-Slots. Die Funktion registriert keine vollständigen Raumtrigger. |
| `src/game/pad.cpp`: `PadInit`, `PadRead`, `KeyStop`, `KeyClear`, `VibControl`, `PadCheckStatus` | Originale JOY-/KEY-Zustände, Flanken, Stickrichtungen, Trigger, Wiederholungen und Stops. PAD-Hardwarezugriff ist ein Android-Adapter. |
| `src/lib/Padclamp.c`: `PADClamp`, `PADClampCircle`, `ClampStick`, `ClampCircle`, `ClampTrigger` | Originale SDK-Geometrie, Deadzones und Sättigung. Hauptstick/C-Stick/Trigger werden tatsächlich über PADClamp verarbeitet. Circle-Variante wird mitgebaut. |
| vorhandene Phasen 1–4a | RNG, Mathematik, Flags, Scheduler/SceSleep, Hintergrundarbeit, Yz2, DVD/DataCtrl/ReadAreaData, MRAM/ARAM, Block- und Modulverwaltung bleiben vorhanden. |

Die Anpassungen beschränken sich auf native Kontextbesitzer, Funktionsbindungen,
benannte Plattformgrenzen und sichere Datenrepräsentation. PPC-Registerbindung
und Scheduling-Barriere im PadRead werden entfernt; dessen Debug-Overlay
`Pad_test` wird nicht ausgeführt. Diagnostische eprintf/OSReport-Aufrufe sind
Ausgabesenken und ersetzen keine Spielentscheidungen.

`JOY`, `KEY`, `PADStatus`, `PADClampRegion`, `Vec`, Flagenumerationen und Pad-
Tabellen stammen aus den geprüften Originalheadern. Der überraschende originale
`u32 bit`-Cursor in der 64-Bit-KEY-Schleife bleibt erhalten. Er wird nicht ohne
PPC-Referenzmessung auf 64 Bit geändert. Die neue Flagadressierung führt dieselbe
32-Bit-Maskenoperation ohne die ursprünglichen Pointer-Trunkierungen aus.

## Gemeinsame Runtime, Frame und Speicher

`GameRuntime` besitzt EngineLoader, ModuleRuntime, GameplayHeaps und einen
OriginalContext. `NativeGameState` ist eine Auswahl nachgewiesener originaler
Feldnamen/-typen; es ist kein binär kompatibles GLOBAL_WK-Overlay. Original-
Assetbytes verbleiben in ihren Formaten und werden über vorhandene BE-Parser
gelesen. Die Stage-ID entsteht aus `room_id >> 8`, damit die originale
Big-Endian-Union nicht auf einer Little-Endian-CPU falsch interpretiert wird.
Loader-eigene Raumdatenansichten und der skalare Spielzustand besitzen noch
keine vollständige gemeinsame GLOBAL_WK-/Modell-/Kameraansicht.

Ein aktiver Tick führt am Besitzthread aus:

1. aus dem vorigen Tick angeforderte Moduloperationen am Frame-Rand;
2. Abbruch-/Freigabeprüfung verwaister ESL-Anforderungen;
3. originalen PadRead, Framezähler und originale Suspend-/Diedemo-Gates;
4. vorhandenen EngineLoader-Frame: normale Tasks, DVD-Watcher,
   Hintergrundbudget, ARAM-Transfer, Blockbefehle und DataCtrl-Prüfung.

ScenarioMove wird von der gemeinsamen Spiel-/Testschnittstelle aufgerufen;
der vollständige originale `gameMainLoop` läuft noch nicht. Die Reihenfolge
aller späteren IdSys-, Trans-, Render-, Audio- und Gameplay-Consumer ist daher
noch kein verifizierter vollständiger Originalframe.

FrameViAdapter akzeptiert nun einen Besitzer-Tickcallback und behält den
vorhandenen Loader-Konstruktor. Tests mit 60/90/120 Displayaufrufen pro Sekunde
ergeben jeweils 30 Gameplay-Ticks. Pause und Fokusverlust entfernen den
Choreographer-Callback, löschen gehaltene Plattformeingaben und setzen die
Displayzeitbasis zurück. Resume holt pausierte Wandzeit nicht nach. Hintergrund-
I/O kann im privaten Worker weiterlaufen; abgeschlossene Daten werden weiterhin
vom Besitzthread übernommen. Die 30-Hz-Regel belegt das getestete Framegerüst,
keine allgemeine Umschreibung der Original-Spielgeschwindigkeit.

ModuleOperation-Tickets werden über Runtime/Gegenration besitzende TaskHandles
kontrolliert. Getötete Anforderer dürfen ihre Operation nicht später auslösen.
Auf dem wartenden Taskstack liegt lediglich ein geliehener Ticketpointer;
der Runtime-Ledger hält den Besitzer bis zur Freigabe. Stop/Restart,
erneute Prologaktivierung, stale Callbacks und RoomReset sind getrennte
Operationen. Direkte ModuleRuntime-Zugriffe bleiben eine interne Schnittstelle;
Laufzeitwechsel des Spiels müssen queueModule/awaitModule verwenden.

GameplayHeaps verwendet 13 logische Heaps 0–12; 13 bedeutet aktuelle Allokation.
Der native Standardarenaumfang von 8 MiB ist ein konfigurierbarer Portwert und
keine Behauptung über das vollständige GameCube-Speicherbudget. Die nach
`src/game/main_mem.cpp:187,305` abgeleiteten Operationen verwenden size_t,
32-Byte-Ausrichtung, begrenzte Bereiche, Tail-Handoffs und Speicherleases.
Die originale OSAlloc/MAD-Tag-Verwaltung wird nicht binär nachgebildet.
Nicht verfügbare/suspendierte Heaps, Überläufe und belegte Zielbereiche scheitern
ausdrücklich. Konstruktorhaltige native Objekte werden durch typisierte
Zuweisung zurückgesetzt; memclr wird nur auf nachgewiesene rohe Bytes angewandt.

ModuleContext kann seine Allokationen nun aus demselben Gameplay-Heapanbieter
beziehen. Ohne Anbieter verwendet die bisherige eigenständige Modullaufzeit
weiter ihren MRAM-Allocator. Echte R10dInit-/R328Init-Allokationen verwenden den
gemeinsamen Anbieter. Raum-/Callback-Lebenszeit endet beim Reset, während eine
extern gehaltene ResourceLease die Speicherbytes sicher weiter pinnt.

**Offene gemeinsame Raumfreigabe:** Ein vollständig initialisierter Raum besitzt
noch Work-Reserven. Stop hält diese für Restart fest. Das Original StageSet
ignoriert den Rückgabewert von MemReplaceHeap. Der neue Adapter wirft bei einer
nicht möglichen nativen Handoff stattdessen
`MemReplaceHeap: native live-resource boundary prevents handoff`.
Ein Test führt diesen Fall mit originalem R328Init und gehaltenem Work-Lease aus.
Die vollständige originale RoomEnd/Modul-/Stage-Freigabekette muss vor
allgemeinen Wechseln initialisierter Räume portiert werden. Das bisher erfolgreiche
StageSet-Relinken zwischen noch nicht vollständig initialisierten Stage-
Komponenten beweist diesen weitergehenden Übergang nicht.

## Szenario- und Eventgrenzen

Die vorhandene native SceExec-Ausführung übernimmt TaskEntry-Bindungen mit
nativen Argumenten und ModuleToken-Lebenszeiten. Originales SceSleep erhält
verschachtelte Stacks und lokale Variablen. ScenarioTaskAllOff entfernt die
originalen Slots 5–17; ein vollständiger Lifecycle-Reset entfernt alle Slots.

SharedScenarioConsumers besitzt ausschließlich erforderliche abstrakte
Operationen. Es gibt keine produktive Default-Erfolgsausführung. Die
ConsumerOrder-Klasse lebt nur in der Verifikation und protokolliert die
originale Aufrufreihenfolge. Sie implementiert keine Actor-/Trigger-/Eventlogik.
Unter dieser ausdrücklich isolierten Fixture laufen originale ScenarioRoomInit
und ScenarioMove sowie die echten R10d-/R328-Init/Main-Paare. In einer echten
Datenlaufzeit scheitert ScenarioRoomInit beim fehlenden SceAtSetSaveItem.

Produktiv offen sind insbesondere:

- `src/game/sce_at.cpp`: SceAtSetSaveItem, SceAtRoomSet, SceAtCheckMoveScrAt
  und das zugehörige originale Item-/Area-/Raumsystem;
- `src/game/sce_sys.cpp:384–455`: SceExecInitCondition, SceExecCheckCondition,
  SceExecCheckCondition_sub und SceExecLinkCondition. Originale Bedingungen
  benötigen Em_flg, CamCtrl.CurrentAreaNo, cEm-Zustände, gebrochene Modelle und
  Itemareas. Integer-OT-Adressen und `(u32) em + 0x320` brauchen typisierte
  native Referenzen; sie dürfen nicht auf native Objekte kopiert werden;
- `src/game/sce_sys.cpp:574`: scenarioCheckEventCancel;
- `src/game/sce_com.cpp:82,133`: SceEventStart/SceEventEnd und deren Stop-,
  Player-, Kamera-, Status- und Audioabhängigkeiten;
- die tatsächliche EventMgr-Funktionsregistrierung, EVD-Ladevorgänge und Run-
  Ausführung. Eventzähler, Dummytrigger und künstliche Event-Endmeldungen sind
  nicht als Ersatz implementiert.

## Originaler Spielstart und r120

Der Startversuch mit dem originalen GameTask führt den Rno0–3-Reset aus und
erreicht den Aufruf gameDebug. Der fehlende Consumer wird ausdrücklich als
`gameDebug/CamCtrl.CurrentAreaNo` gemeldet. Der originale gameDebug-Körper
selbst läuft noch nicht; er benötigt neben der Diagnose echte Kamera-/Player-
Zustände und Debugmenüs. Es wird weder eine erfundene AreaNo zurückgegeben
noch der GameTask künstlich in einen späteren Zustand gesetzt.

Weitere echte Abhängigkeiten aus `src/game/game.cpp`:

| Originale Funktion | Nächste benötigte gemeinsame Komponenten |
| --- | --- |
| gameInit (239) | ClothInit; GXSetCopyClear; cMes.gameInit; SubScreen/Cockpit; LightMgr und Core-Lightdaten; PlayerInit; ItemMgr, Save-/Merchant-/GamePoint-Systeme |
| gameStageInit (292) | cMes-Selektion; Original-Disc-Wechsel; GameSave; StageSet |
| gameRoomInit (354) | Modelle, RoomData, Licht/Kamera/Player/Collision, originale gemeinsame Ressourcen- und Szenarioinitialisierung |
| gameMainLoop (563) | Originale IdSys-/Spiel-/Trans-/Audio-/Renderabfolge |

Die r120/r100-Archive beider verwendeter Referenzfälle werden über echte
Engine-Ladefunktionen verarbeitet; zusätzlich sind r101 und die Disc-2-Fälle
r400/r405/r402 geprüft. Ihre Originalbytes sind intakt. Es wurde keine
raumspezifische Ersatzinitialisierung geschrieben.

Das st1_0-Teilmodul 74 mit Original-FileTbl 146 kann am Frame-Rand aktiviert
werden. Seine r120-Registrierung ist bekannt; ein ausführbarer R120Init fehlt.
Mit isolierten gemeinsamen Consumern wird dieses Fehlen ausdrücklich gemeldet.

`src/st1/r120.cpp:45` belegt die nächsten Blocker genau: originales R120Work,
EvtMgr.SetFunc für Evt_R120S00_Func/Evt_R120S01_Func, SceExec(R120Event) und
TexRenderInit. `R120Event` ab Zeile 63 benötigt Fade/SubScreen,
Sofdec.Initialize(`movie/opening.sfd`), echte SceEventStart/End und
EvtReadAram/EvtReadExec für `event/evd/r120s00.evd` und `r120s01.evd`.
Erst danach erfolgt der originale SceAtExecRoomJump nach 0x100.
Diese Pfade sind Quellenbefunde; sie wurden noch nicht als native Eventsequenz
ausgeführt. Auch der leere R120Main beweist keine gelungene Initialisierung.

Englisch bleibt die Referenz. Die 35 bereits inventarisierten unvollständigen
englischen MDT-Blöcke sind unverändert. Die aktuellen Archive-/ESL-Prüfungen
fordern keine cMes-Texte an. gameInit/gameStageInit und spätere Event-/Menü-
Consumer benötigen echte Message-Ressourcen; konkrete dynamische Zugriffe auf
die 35 Blöcke sind bei diesem Ausführungsstand noch nicht belegt. Es werden
keine Inhalte ersetzt und keine zusätzlichen Sprachen entfernt.

## Android-Controller und physische Messung

ControllerLoop nutzt InputManager, InputDevice/MotionRange und Activity-dispatch
für KeyEvent/MotionEvent. Choreographer ruft ein einziges natives Pad-/Game-
Framegerüst auf. Die konfigurierte gemeinsame GameRuntime und die reine
Pad-Diagnose teilen dieselbe Originalimplementierung, ohne einen doppelten
PadRead pro Tick. Game-JNI-Handles sind 64-Bit-IDs mit Besitzthread-/Closed-
Prüfung; zwei Game-Domänen im selben Eingabethread werden abgewiesen.

Normierung verwendet die tatsächlich gemeldeten Achsenbereiche und Flat-Werte;
danach folgen originale PADClamp und PadRead. Der native Stickrohbereich ist
-100..100, Y wird zur GC-Richtung invertiert, Trigger verwenden 0..255 vor
dem SDK-Clamp. Rechts werden Z/RZ oder RX/RY gewählt; Trigger verwenden
LTRIGGER/RTRIGGER beziehungsweise BRAKE/GAS. Digitale L2/R2-Ereignisse
übersteuern vorhandene Analogachsen nicht; bei fehlender Analogfähigkeit
steht eine digitale Triggerfallback zur Verfügung.

| Android-Eingabe | Originale GC-JOY-Zuordnung |
| --- | --- |
| ABXY | gleichnamige JOY_A/B/X/Y-Bits |
| D-Pad-Tasten und Hat | JOY_LEFT/RIGHT/DOWN/UP, zusammengeführt |
| L1 / R1 | JOY_L / JOY_Z |
| L2 / R2 | analoge GC-L-/R-Trigger beziehungsweise digitaler Fallback |
| Start | JOY_START |
| Select und Stickklicks | bislang keine erfundene GC-Aktion |

Originale Key_type_tbl und Gameplay-Zuordnung bleiben erhalten. Rumble-
Anforderungen werden erfasst; tatsächliche Android-Haptikausgabe ist noch offen.
Xbox, PlayStation und weitere standardkonforme Android-Gamepads verwenden
dieselbe API-basierte Schicht. Physisch getestet wurde ausschließlich der
Retroid. Weitere Geräte sowie genaue GC-gegen-Android-Stickkalibrierung sind
offen. Androids dokumentierte Eingabegrenzen wurden anhand der
[offiziellen Controller-Dokumentation](https://developer.android.com/games/sdk/game-controller/controller-input)
geprüft.

Retroid Pocket Flip 2, Serial `dbdcacc`, Android 13/API 33, meldet
`arm64-v8a,armeabi-v7a,armeabi`. InputDevice 9 heißt Retroid Pocket Controller;
der Deskriptor lautet `dc75afea56e3c3a269b97967aa26b8c93c0bd3fb`.
X/Y/Z/RZ und Hats melden -1..1, BRAKE/GAS 0..1. Die tatsächlichen Android-
MotionRanges für Sticks zeigen Flat `4.5777764E-4`; die Hat-/Trigger-Flats sind 0.
Die gelesene Keylayout heißt `/system/usr/keylayout/Vendor_2022_Product_3001.kl`.

Der Nutzer hat die Eingaben für beide ABIs ausdrücklich mit „fertig“ bestätigt.
Die Logauswertung prüft ABXY/L1/R1/Start Down/Up, beide Stick- und Hat-Richtungen,
alle benötigten originalen JOY-Bits sowie Trigger-Neutral-, Zwischen- und Vollwerte.
ADB-Keyinjektionen wurden nicht als physische Eingaben verwendet.

| Messlauf | Actual primaryCpuAbi / Pointer | Raw / native Snapshots | Triggerstufen links / rechts |
| --- | --- | --- | --- |
| korrigierter ARM64, PID 27980 | arm64-v8a / 64 Bit | 379 / 266 | 20 / 30, jeweils 0..150 |
| korrigierter ARM32, PID 28500 | armeabi-v7a / 32 Bit | 361 / 221 | 11 / 9, jeweils 0..150 |

ARM64-Sticks erreichten ±72 und C-Sticks ±59. ARM32 erreichte Haupt-X ±72,
Haupt-Y -72..68 und C-Sticks ±59. Unterschiedliche Handbewegungen und die
Abtastung machen diese Aufzeichnungen nicht zu identischen Testvektoren.
Die kontrollierten nativen Vektoren und Zustandsabläufe sind auf allen vier
Architekturen gleich. Physische Verbindungs-/Trennungstests eines externen
Gamepads und einer Bluetooth-Verbindung wurden nicht durchgeführt; deren
Adapterpfade besitzen automatisierte Verifikation.

Die physischen Messläufe verwendeten APK-SHA256
`2d8a34e5d99a4471167e30076cdfe77da4a6a3fb14c847860b8eed1f2841c2a3`.
Danach kamen der zusätzliche JNI-Domänentest und der Heap-Sicherheitsfall hinzu;
die Controllerzuordnung und übernommenen Pad-Funktionen blieben unverändert.
Die abschließend mit 19 App-Tests geprüfte APK hat SHA256
`34b6cd1e5c7cb39cc970c53cf291ca24f996641fb5a4c66b6d0e5cf72710299c`.
Die final installierte App-ABI ist ARM64. Beide physischen Logs und JSON-
Auswertungen sind separat im privaten `phase4b`-Verzeichnis gespeichert.

## Tatsächlich ausgeführte Verifikation

| Prüfung | Windows x86 / x64 | Retroid ARM32 / ARM64 |
| --- | --- | --- |
| CMake-Build und CTest | jeweils 23/23 Gruppen bestanden | beide native Builds bestanden |
| neue native Runtime-Suite | jeweils 176 Prüfungen, 0 Fehler | jeweils 176 Prüfungen, 0 Fehler |
| echte Archiv-/Block-/ESL-Vorbereitung | je 2 Runden × 2 Discs × 5 Ladevorgänge = 20 | je ABI ebenfalls 20; Hashes/Metadaten gleich |
| REL-Regressionslauf | je 114 echte REL-Dateien, 110 Disc 1 + 4 Disc 2 | je ABI 114, Metadaten-/Lebenszyklus-Trace gleich |
| bestehende RNG-/Scheduler-/Background-/Yz2-CLI-Suite | durch CTest geprüft | 2 Runden je ABI; ARM32 13.386, ARM64 13.387 Prüfungen, 0 Fehler |
| APK-Prozess / JNI / Android-Range-/Lifecycle-Adapter | nicht anwendbar | 19/19 Instrumentationstests pro erzwungener ABI |
| physischer Controller | nicht anwendbar | beide ABIs, oben dokumentierte Nutzeraktionen |
| APK/Test-APK/Lint | gebaut über Gradle 8.13, AGP 8.13.2, JBR 21, NDK r28c | Lint 0 Fehler, 9 Warnungen |

Die neue Suite prüft Original-Startwerte, Doorflags, Zeit/Pause, Pad-/KEY-
Mapping/Clamps/Repeat, Heapgrenzen/Lease-Pinning, originale Szenarioaufrufordnung
mit isolierten Consumern, SceSleep-lokale Variablen, echte R10d-/R328-Callbacks,
Frame-Rand-Module, Stop/Restart, Cancel/Stale-Handles, Wiederinitialisierung,
StageSet-Relinken und den verhinderten belegten Heapwechsel. 20 nacheinander
abgebrochene asynchrone readEmList-Tasks plus erneutes erfolgreiches Lesen
prüfen die Freigabe der begrenzten DVD-Warteschlange.

Die Original-ESL-Dateien sind 8.160 Byte; der Originalpuffer ist 8.192 Byte.
Der Adapter übernimmt nur vorhandene Bytes. Ein Canary-Test und echte Hash-
Vergleiche belegen den unveränderten Restpuffer. `readEmList(1)`/StageSet
verwendet original Mode 0x11, also synchrones Warten. Der Abbruchtest benutzt
original `readEmList(0)`, Mode 0x10. Eine pauschale Umstellung von StageSet
auf asynchrones Verhalten wurde nicht vorgenommen. Beim Testneustart wird die
ESL-Auswahlbedingung bewusst neu gesetzt; die Originalfunktion setzt ihr
em_list_no vor dem Lesen und macht diesen Zustand nach Task-Abbruch nicht
automatisch rückgängig.

Die echten Datenfälle sind Disc 1: 120,100,101,120,100; Disc 2:
400,405,402,400,405. Sie nutzen dieselbe Pipeline. Initiale Testzustände werden
offen als diagnostische Voraussetzungen gesetzt. Dies sind originale Daten-
Ladevorgänge innerhalb der gemeinsamen Runtime; eine vollständig erreichte
GameTask-/Event-Rauminitialisierung wird daraus nicht abgeleitet.

`summarize_game_runtime.py` bestätigt vier gleiche ABI-Prüfzahlen, gleiche
isolierte Traces, genau zwei Runden je Disc/ABI und gleiche Daten-Traces:

- Disc 1: `fb560095c44ed28833a11042f09649d2d1a1a65b456fa74057945b55f1f0adc4`
- Disc 2: `0678366825d0c75049ea86cd04b73b1181f6627a0e4bd5c0acf4fdbb7cfe02ac`

Die alte ARM64-CLI-Suite hat eine zusätzliche 64-Bit-High-Address-Prüfung im
Scheduler-Stacktest. Das erklärt 13.387 gegenüber 13.386; ihre sieben
semantischen Traces sind identisch. Die vollständigen Ressourcen-Korpusläufe
aus Phase 3c wurden in Phase 4b nicht vollständig wiederholt. Ihre vorhandenen
CTest-Regressionsgruppen und die genannten echten Datenfälle wurden erneut
ausgeführt. Sanitizer stehen weiterhin ohne geprüfte Fiber-Hooks nicht bereit;
die bestehenden bounds-/owner-/generation-/canary-Prüfungen ersetzen keine
zukünftige Sanitizer-Verifikation.

Zwischenstände mit Fehlern wurden korrigiert: falsche Annahme eines 8.192-Byte-
ESL-Files; digitales L2/R2 übersteuerte Analogwerte im ersten physischen Lauf;
Abbruchtest benutzte zunächst den synchronen Modus; der neue JNI-Test benutzte
zunächst einen vom Parser korrekt abgewiesenen leeren Katalog. Weitere anfängliche
Compile-/Fixture-Probleme bei der GX-Typgrenze, ResourceLease-Zugriffen und
doppeltem RoomInit sind behoben. Ein Python-Temp-Verzeichnis scheiterte in der
Sandbox; die Wiederholung mit zulässigem Dateizugriff und beide CTest-Läufe
bestanden. Die finalen gemeldeten Prüfungen haben 0 Testfehler.

MSVC meldet ursprüngliche Konversions-/Shadow-/unerreichbarer-Code-Warnungen
in den zwei extrahierten Originaleinheiten. Dort wird /WX gezielt aufgehoben;
eigene Runtime-Adapter bleiben mit /W4 /WX beziehungsweise Clang -Werror gebaut.
Die bekannte SDK-XML-v3/v4-Meldung bleibt ein Toolchain-Warnhinweis. Lint nennt
gepinnte ältere Tool-/SDK-/Testversionen, fehlendes Diagnoseicon und englische
Diagnoseliterale. Es wurde kein unbelegtes Toolchain-Upgrade vorgenommen.

## Reproduktion

Vom Repositorywurzelverzeichnis, mit eingerichteter x86- beziehungsweise x64-
Visual-Studio-Developer-Umgebung:

```powershell
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
# Entsprechend separat für port/build/host-x86.
.\port\scripts\build-android.ps1 -SdkRoot 'C:\path\to\Android\Sdk' -JavaHome 'C:\path\to\jdk' -Python 'C:\path\to\python.exe' -Offline
.\port\scripts\test-device.ps1 -Adb 'C:\path\to\adb.exe' -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb 'C:\path\to\adb.exe' -Serial dbdcacc
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4b-host' --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4b-host' --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4b-device' --adb 'C:\path\to\adb.exe' --serial dbdcacc --repeat 2
python port/tools/test_modules.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4b-modules-host' --host-exe port/build/host-ninja/re4_module_tool.exe --repeat 1
# Entsprechend mit Host-x86 und mit --adb/--serial plus separatem device-Output.
python port/tools/summarize_game_runtime.py --host 'E:\Development\Android\RE4-Data\phase4b-host' --device 'E:\Development\Android\RE4-Data\phase4b-device' --controllers 'E:\Development\Android\RE4-Data\phase4b' --output 'E:\Development\Android\RE4-Data\phase4b'
```

Für neue physische Logs: passende APK-ABI installieren, Activity starten,
primaryCpuAbi und PID erfassen, Nutzeraktionen ausführen lassen, anschließend
`adb logcat -d -v threadtime --pid=<PID>` mit den Tags RE4ControllerRaw,
RE4ControllerNative und RE4ControllerDevice separat speichern. Nicht alte
ABI-Läufe zusammenmischen. Danach beispielsweise:

```powershell
python port/tools/analyze_controller_capture.py --log 'E:\Development\Android\RE4-Data\phase4b\controller-arm64-corrected.log' --bits 64 --output 'E:\Development\Android\RE4-Data\phase4b\controller-arm64-corrected.json' --human-confirmed --require-standard-controls
```

`--human-confirmed` ist nur bei tatsächlicher Nutzerbestätigung zulässig.
Die neue zusammengeführte Evidenz liegt in `phase4b/game-runtime-summary.json`.
CTest-Protokolle liegen im jeweiligen Build unter Testing/Temporary;
App-/CLI-Protokolle in `port/build/instrumentation-*.txt` und `device-*-round*.txt`.

## Vollständige geänderte Dateien

| Bereich | Dateien unter `port/` |
| --- | --- |
| Build / Übersicht | `CMakeLists.txt`, `README.md` |
| Originalprovenienz | `config/runtime-sources.json`, `tools/generate_runtime.py`, `tests/test_runtime_sources.py` |
| gemeinsame Runtime | `include/re4/port/game_runtime.h`, `src/runtime_original.h`, `src/game_runtime.cpp`, `src/gameplay_heaps.cpp`, `src/controller_input.cpp` |
| bestehende Integrationspunkte | `include/re4/port/engine_loader.h`, `src/engine_loader.cpp`, `include/re4/port/modules.h`, `src/modules.cpp`, `include/re4/port/scheduler.h`, `src/scheduler.cpp` |
| native Prüfungen / echte Daten | `tests/game_runtime_verification.cpp`, `tests/game_runtime_tool.cpp`, `tools/test_game_runtime.py` |
| physische Auswertung / Abgleich | `tools/analyze_controller_capture.py`, `tools/summarize_game_runtime.py` |
| Android-JNI / Eingabe | `platform/android/game_jni.cpp`, `android/app/src/main/java/org/re4/port/ControllerLoop.java`, `FoundationActivity.java`, `NativeChecks.java` im selben Java-Verzeichnis |
| App-Prüfung | `android/app/src/androidTest/java/org/re4/port/GameRuntimeTest.java`, `scripts/test-app-device.ps1` |
| Bericht | `docs/phase4b-status.md` |

Build-generierte Quellen/Typheader sowie APKs und Binärdateien bleiben ignoriert.
Das Original-`src`, `include`, `tools`, `docs` und `config` außerhalb von `port/`
haben gegenüber dem Ausgangscommit keine Änderungen.

## Konkrete Voraussetzungen für Phase 4c

1. Gemeinsame originale Raumende-/Modul-/Heap-Freigabe aus game.cpp/roomdata.cpp
   integrieren. Einen initialisierten R10d/R328 mit noch gepinntem externem
   Lease verlassen; Callback invalidieren, Original-State freigeben, sichere
   Stage-Handoffs und danach erneute Init prüfen. Erst dann allgemeine
   initialisierte Stage-/Raumwechsel als unterstützt bezeichnen.
2. Vollständige native Ansichten für originale Room-/Player-/Model-/Camera-
   Consumer verbinden; benötigte Originalinitialisierer entlang gameInit und
   gameRoomInit übernehmen. Ein ehrlicher Headless-Grenzpunkt darf fehlende
   Kamera-/GX-/Audiooperationen melden, aber keine Erfolgszustände erzeugen.
3. Originale Condition-OTs, Trigger-/Itemareas, Eventstart/-ende/-abbruch und
   EventMgr-Registrierung mit nativen Argument-/Objektlebenszeiten portieren.
   Originale EVD-/MDT-Referenzen und Ressourcenzugriffe dynamisch protokollieren.
4. Mit diesen gemeinsamen Abhängigkeiten R120Init/R120Event übernehmen, echte
   r120s00/s01-EVDs und Opening-Verbraucher verbinden und den originalen
   Übergang nach r100 verifizieren. Keine alternativen Raumprogramme erstellen.
5. Spätere Renderer-/Audioarbeit bleibt gesondert. Das Laufzeit-Grafikmenü mit
   persistentem MSAA Aus/2×/4×, AF 2×/4×/8× und DoF Ein/Aus für nahe/anvisierte
   Objekte bleibt gemäß `graphics-settings.md` verbindlich: backendunabhängig,
   hardwarevalidiert und tatsächlich aktive statt bloß angeforderte Zustände.

Dies sind Voraussetzungen und nächste überprüfbare Aufgaben. Phase 4c und die
vollständige Grafikengine wurden in dieser Sitzung nicht begonnen.
