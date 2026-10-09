# Verbindliche Integrationsgrenzen nach Phase 3c

Dieses Dokument bereitet Phase 4 vor. Die hier beschriebenen Gameplay-, Modul-,
Modell- und Eingabesysteme werden durch Phase 3c nicht ausgeführt. Die vorhandenen
abstrakten Schnittstellen in `include/re4/port/contracts.h` liefern keine
erfolgreichen Ersatzfunktionen.

## Nachgewiesene Originalabhängigkeiten

| Einstieg | Tatsächliche Abhängigkeiten | Nächste Integrationsgrenze |
| --- | --- | --- |
| `src/game/game.cpp:GameTask` | Schleife über `gameInit`, `gameStageInit`, `gameRoomInit`, `gameMainLoop`, Türdemo, Ending, Option; `TaskSleep(1)` | Originalzustände des vollständigen `GLOBAL_WK`, Core-/Player-Ressourcen und Manager zuerst portieren; keine Ersatzschleife mit Raumattrappen |
| `src/game/stage.cpp:StageSet` | Reload anhand der Systemflags/Stage; `RoomData.stopRelData`; Heapwechsel 1→2 und 2→3; `checkRelRead/linkRelData`; `readEmList(1)` | Native Lebenszeiten für Game/Stage/Modul und vollständig registrierte native Module |
| `src/game/game.cpp:gameRoomInit` | `ReadAreaData`, Archive/CNS, Modelle, Parts/Em/Obj-Manager, Licht, SAT/EAT, SceAt, Eventmanager, Kamera, BLK, Texturen, Sound, Messages, `ScenarioRoomInit` | Tatsächliche Formate und Pointerfixups je gemeinsamem Manager; fehlende Verbraucher müssen Fehler melden |
| `src/game/sce_sys.cpp:ScenarioRoomInit` | Scenario-Slots 5–17 beenden, Event-/Item-/Savezustände, `RoomData.execInitFunc`, Schedulerpass, Ashley, `EmSetFromList`, `SceAtRoomSet` | Originale Manager und globale Zustände binden; Scheduler ist bereits vorhanden |
| `src/st1/r120.cpp:R120Init` | Raumwork, zwei benannte Eventcallbacks, `R120Event`, `TexRenderInit` | EvtMgr, TexRender, Fade, SubScreen und Sofdec bereitstellen; Film-/Eventpfad nicht durch sofortigen RoomJump ersetzen |
| `src/st1/r100.cpp:R100Init` | Raumwork, `EmReadSearch(0x12)`, Modulressourcen, Original-Archivindizes, SMD-/Obj-/SceAt-/Eventfunktionen, TexRender, `EmSetEvent` | Modell-/Enemy-Modul und Eventsystem portieren; sämtliche Positionen/Flags bleiben im Originalcode |

`src/game/main_mem.cpp` verwaltet 13 Dolphin-Heaps, Suspend/Signal und
`MemReplaceHeap`. Die Datenlaufzeit hat jetzt einen begrenzten nativen MRAM-
Allocator mit echten Besitzregeln. Sie bildet noch nicht diese gesamten Heaps
nach. Für Gameplay müssen die Lebenszeiten erhalten werden: System/Game/Stage/
Modul, Room-Heap 4 und temporäre Bereiche. `src/game/sscrn.cpp` suspendiert Heap 4,
benutzt Heap 12 und setzt Heap 4 danach fort. `game.cpp` benutzt zusätzlich Heap
10 und `MemorySwap` mit festen GameCube-Adressen. Diese Adressen dürfen niemals
als Android-Pointer ausgeführt werden.

Der nächste notwendige Arbeitsschritt ist daher eine native Modul- und
Heap-Lebenszeitintegration mit den bereits geprüften Ressourcenhandles. Ein
komplettes `GameTask` kann vorher nicht seriös als funktionsfähig gelten.

## REL-Zustände und native Module

Die maßgeblichen Funktionen sind
`src/game/roomdata.cpp:cRoomData::checkRelRead/linkRelData/stopRelData/restartRelData`
und `src/game/main_sub.cpp:DLL_Link/DLL_Unlink`:

1. `ROOM_DATA::rel_file` ist ein **FileTbl-Index**, nicht die REL-Modul-ID.
   `linkRelData` lädt diesen FileTbl-Eintrag mit `DvdRead(..., 0x104)`, wartet mit
   `TaskSleep`, reserviert BSS und ein Backup, verlinkt und ruft den Prolog auf.
2. `stopRelData` setzt `CTRL_STOP`, kopiert den Zustand **vor** dem Epilog und
   ruft `DLL_Unlink` auf. Die geladenen Modul-/Backupdaten werden dabei erhalten.
3. `restartRelData` löscht `CTRL_STOP`, verlinkt erneut und stellt danach das
   Backup wieder her. Es ruft **keinen zweiten Prolog** auf.
4. `StageSet` unterscheidet diese vorübergehende Unterbrechung von einem echten
   Stage-/REL-Wechsel und löscht `m_RelNo` vor dem neuen Laden.

Native ARM-Funktionen werden aus den Original-REL-Quellen kompiliert und über
eine generierte Registry aktiviert. PPC-Instruktionen aus der importierten REL
können nicht als native ARM-Funktionen aufgerufen werden. Registryeinträge müssen
aus tatsächlichen Modullisten, FileTbl und Raumregistrierungen entstehen.

Die erweiterten abstrakten Methoden `activate/deactivate/stop/restart/lifetime`
bilden diese Grenze ab. Die vorhandenen Scheduler-Modultokens verhindern bereits
die Invalidierung eines aktiven Aufrufstapels. Der Registry muss zusätzlich die
transitiven Abhängigkeiten von Callbacks, Events und Ressourcen bekannt sein.
Native Codebereiche bleiben zunächst resident; eine logische Deaktivierung ist
kein unsicheres `dlclose` unter einem schlafenden Task.

Native C++-BSS mit Pointern, Vtables und Besitzobjekten darf nicht byteweise wie
eine PPC-BSS gesichert werden. Explizite modulbezogene Zustandsobjekte müssen
Originalzustand erhalten und Konstruktoren/Prologe korrekt unterscheiden.
Diese Zustandsabbildung ist noch zu implementieren und zu testen.

## Modellaktivierung und Umlagerung

Phase 3c meldet `MRAM_OK` für echte Daten. Es setzt **nicht** `BLOCK_CREATE`,
solange `BlockCreate`, `cSmd` und `ObjMgr` nicht laufen. `BlockModelBinding` ist
eine abstrakte Integrationsgrenze für echte Create/Destroy/Rebind-Aufrufe.

`src/game/block.cpp:moveBlockData/recalcModelAddr/checkBlockMemSort` verändert
auch Modellpointer und ruft `cSmd::slide`. Diese Funktionen sind noch nicht
kompiliert: ihre Pointerarithmetik über `int/u32`, die modellabhängigen Fixups
und die Poolkompaktierung benötigen native Anpassungen. Bis dahin wird der
originale `noMemCtrl == 1`-Datenpfad mit Einzelreservierungen verwendet.

Ressourcenreferenzen sind jetzt `(Runtime, Slot, Generation, Offset, Größe)`.
Sie können nach einer Umlagerung erneut aufgelöst werden. Ein bereits erhaltenes
Lease hält die frühere MRAM-Reservierung bis zu seiner Freigabe am Leben. Echte
Modelle müssen zusätzlich ihre internen Referenzen korrekt binden; ein gültiges
Lease allein ist kein Nachweis für portierte Modell-Fixups.

## Integrierte und externe Gamepads

Verbindliche Zielgeräte bleiben Retroid Pocket Flip 2 sowie Android-kompatible
Xbox- und PlayStation-Controller. Eine einzige Eingabeschicht muss Sticks,
analoge Trigger, D-Pad, ABXY, Schultern und Menütasten für Gameplay, Inventar,
Menüs und QTEs bereitstellen. Diese Phase implementiert keinen Inputtreiber.

Die Android-Grenze erfasst Gerätequellen und Fähigkeiten, verarbeitet digitale
`KeyEvent`- und analoge `MotionEvent`-Ereignisse und normalisiert unterschiedliche
Achsen-/Triggerlayouts. Die offiziellen Hinweise beschreiben auch die C++ Game
Controller Library als mögliche Normalisierungsschicht. Backendwahl und
gerätespezifische Zuordnung werden erst bei der Inputintegration nach Tests
festgelegt. [Android-Primärquelle](https://developer.android.com/games/sdk/game-controller/controller-input)

`PlatformGamepads` liefert eine pro Gameplay-Tick stabile Probe. Die skalaren
Felder von `OriginalPadSample` stammen aus `include/dolphin/pad.h:PADStatus`:
`u16 button`, vier `s8`-Stickachsen, vier `u8`-Analogwerte und `s8 err`.
Ihre natürliche Größe wird auf 12 Byte geprüft. Dies ist eine native
Schnittstelle, keine Beschreibung eines vermuteten Disc-Datenformats.

Der spätere Adapter ersetzt `PADRead/PADClamp/PADControlMotor`. Der eigentliche
`src/game/pad.cpp:PadRead`-Ablauf soll erhalten bleiben: `Joy.on/old/trg/rel`,
Stickrichtungen, Triggerbits, Repeatzeiten 24/6 und 18/3, `Key_type_tbl` und
Vibration. Bei `GetSystemVcnt == 2` laufen diese Zähler weiterhin nach dem
Originaltakt, unabhängig von 60/90/120-Hz-Anzeigen.

Zuordnungen werden einheitlich als Profil gepflegt, statt in einzelnen Räumen,
QTEs oder Menüs neu geschrieben zu werden. Gerätewechsel, Entkopplung und
App-Pause müssen gehaltene Tasten/Achsen und Rumble zuverlässig zurücksetzen.
Stick-/Triggerbereiche und Deadzones werden anhand tatsächlicher Fähigkeiten
validiert; GameCube-Clamping und Mapping dürfen nicht versehentlich doppelt
angewandt werden. Xbox-/PlayStation-Symbole dürfen die logische Originalaktion
nicht verändern. Externe Controller sind hier noch nicht am Gerät getestet.

## Weiterhin verbindliche Grafikarchitektur

[graphics-settings.md](graphics-settings.md) bleibt unverändert verbindlich:
bedienbares Ingame-Menü, MSAA Aus/2×/4×, AF 2×/4×/8×, DoF Ein/Aus mit Fokus auf
nahe oder anvisierte Objekte, persistente Speicherung und hardwareabhängige
Validierung. Angefordert, unterstützt und tatsächlich aktiv bleiben getrennt.
Menü/Persistenz bleiben backendunabhängig; Vulkan kann die primäre Implementierung
werden. Phase 3c implementiert weder diese Effekte noch einen Renderer.
