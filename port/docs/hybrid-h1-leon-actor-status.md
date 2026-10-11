# Hybrid H1: Leon-Konstruktorfolge und Actor-Szenariokollision

Ausgangspunkt: `8ed4d15c8cc9fbec995b8b7a6dfe66111df9f15d`, Branch
`android-port`, 11. Oktober 2026. Dieser Bericht ergänzt `hybrid-h1-status.md`.

## Abnahme

**Geprüfter H1-Teilstand; H1 ist nicht abgeschlossen.** Die erhaltenen
Originalbodies der Player-/Leon-Konstruktorfolge werden mit importierten
G4BE08-Daten auf dem Stack eines nativen Scheduler-Tasks ausgeführt. Die
Konstruktion erreicht `cPlayer::weaponRelease()` und scheitert konkret am
fehlenden gemeinsamen, lebenden Objektpool:

```
Original consumer unavailable: cPlayer::weaponRelease/ObjMgr.getActiveWork (live generic object pool)
```

Es gibt weiterhin keinen zurückgegebenen vollständigen `cPlLeon` mit originalem
virtuellem Actor-Dispatch, keinen erfolgreich ausgeführten `cPlayer::startUp`
und keine vollständigen Player-/Weapon-/Kollisionsticks. Eine erfundene leere
Objektliste, ein übersprungenes `weaponRelease` oder ein erfolgreicher
Effect-Stub würden diesen Fehler verbergen und wurden nicht eingesetzt.

| Abnahmekriterium | Nachgewiesener Stand |
| --- | --- |
| Vollständiger originaler cPlLeon-Actor | Offen; EmMgr-Dispatch und vollständige native Actorklasse fehlen |
| Originales startUp vollständig | Offen; die vorherige Konstruktorfolge bricht beim Objektpool ab |
| Mehrere vollständige Actor-/Motion-/Kollisionsticks | Offen; Motion-/SAT-Prüfungen sind ausdrücklich separate Originalconsumerprüfungen |
| Wiederholte Konstruktion/Freigabe | Der ausgeführte Konstruktorpräfix wird wiederholt aufgebaut und vollständig zurückgerollt; vollständige Actorfreigabe bleibt offen |
| ARM32-/ARM64-Verifikation | Der Teilstand wird auf beiden ABIs geprüft; daraus folgt keine vollständige H1-Abnahme |

## Tatsächlich integrierter Originalcode

`port/tools/generate_player.py` prüft sämtliche beteiligten Originaldateien
gegen `port/config/player-sources.json` und extrahiert ganze Funktionsbodies.
Das Player-Manifest enthält jetzt 51 Funktionen, zuvor 21. Neu sind:

* `src/game/player.cpp`: `cPlayer::cPlayer`, `cPlayer::init0`.
* `src/game/pl_leon.cpp`: der erhaltene Body von `cPlLeon::cPlLeon`.
* `src/game/pl_wep.cpp`: `cPlWep::cPlWep`, `cPlayer::weaponRelease`.
* `src/game/pl_body.cpp`: `cPlBody::waistSet`, `waistMove`, `move`.
* `src/game/pl_class.cpp`: `cPlWaist::cPlWaist`, `set`.
* `include/pl_push.h`: `cPlPush::cPlPush`.
* `src/game/cMotBase.cpp`: `cMotBase::cMotBase`; dessen Bewegungsfunktionen
  werden in diesem Schritt noch nicht als integriert ausgegeben.
* `src/game/pl_debug.cpp`: `cPlayer::debugInit`, `cPlMaho`-Konstruktor,
  `reset`, `regist` und die elf registrierten Cheatcallbacks. Die Registrierung
  wird ausgeführt; die Cheats werden im Test nicht ausgelöst.
* `src/game/em.cpp`: beide `cDmgInfo::set`-Überladungen und `move`.

Die Konstruktorbodies laufen in `PlayerRuntime::constructLeon` auf einem
typisierten `PlayerDomain`, mit demselben `ActorModelCpu` als Modellwurzel wie
bei der bereits integrierten Modellverwaltung. Dies ist noch keine native
vollständige cPlayer-/cPlLeon-Klassenhierarchie. Insbesondere wird der lokale
Konstruktionsverweis nicht als globaler, spielbereiter `pPL` veröffentlicht.

Die tatsächlich erreichte Folge ist:

```
cDmgInfo::cDmgInfo / cEm::cEm / initWork
→ cPlayer-Konstruktorbody
  → stat.reset, Leben, Archivbesitzer, Motiontabellen, debugInit
→ cPlLeon-Konstruktorbody
  → init0: Weapon, Body, Push, Waist, MotionBase
  → setModel: ursprüngliche BIN-/TPL- und Attachmentzuordnungen
  → weaponRelease
    → ObjMgr.getActiveWork: ausdrücklicher fehlender Consumer
→ geprüfter nativer Rollback
```

Mit `em/pl00.drs` beider Debug-Discs wurden vor diesem Fehler 119 Gelenke und
acht Modell-/Materialzuordnungen aufgebaut. Die Body-Waistfunktionen werden
zusätzlich auf diesen echten Gelenken ausgeführt: der Originalcode verteilt
die Testdrehung auf die Spine-Teile 1/2 und gegenläufig auf Teil 3. Dieser
definierte numerische Input ist kein originaler Gameplayzustand und keine
erfundene Animation.

## Plattform- und Lebenszeitanpassungen

Die Originaltabellen haben 109 beziehungsweise 12 Einträge. Die GC-Bytezahlen
`0x1B4` und `0x30` werden durch `Einträge * sizeof(void*)` ersetzt. Speicher
kommt aus den vorhandenen `GameplayHeaps`, mit der originalen Heapauswahl 13.
Placement-Array-New startet native Pointerlebenszeiten; das anschließende
Clear setzt typisierte Nullpointer. Es gibt keine vier Byte breite
Pointerüberlagerung unter ARM64. Die GC-Adresse `0x807EC000` wird durch den
bereits geprüften Besitzer des originalen Playerarchivs ersetzt.

Weapon, Body, Push, Waist, MotionBase und Maho besitzen getrennte typisierte
RAII-Besitzer. Die Reihenfolge ihrer Erstellung folgt `init0`; bei einem
Fehler verschwinden alle Besitzer einschließlich der Modellregistrierung.
Ein zweiter Versuch benötigt weder ein künstliches Flag noch eine andere
Spielerposition. Die vorhandenen Leon-Assethelfer bleiben verfügbar.

Für den Rollback auf dem Konstruktor-Taskstack besitzt `EngineConsumers` einen
privaten, ausschließlich `PlayerRuntime` zugänglichen Freigabepfad. Er prüft
den exakten Modellbesitzer, die vorhandenen Borrower und originale
Event-ModTbl-Referenzen. Die normale Modellfreigabe fordert weiterhin ruhende
Taskborrower. Das ist keine allgemein zugängliche Umgehung der Freigabeprüfung.
Die neuen Kollisionslinks werden beim Retirieren eines Modellbesitzers
gelöscht und die Collision-Enablebits deaktiviert.

Maho registriert native besitzergeschützte Callables. Kein Funktionspointer
wird über einen GC-u32 transportiert. Eine spätere Callbackausführung benötigt
einen gültigen Konstruktionsbesitzer; die Tabelle wird beim Rollback zerstört.
Die vollständige gameplayseitige Callbackregistrierung steht noch aus.

## Originale Actor-Szenariokollision

Das Collision-Manifest umfasst 45 ganze Originalbodies, einschließlich:

* `src/game/atariInfo.cpp`: Konstruktor, `init0`, `init`, `set`, `move`,
  `getSpeedVector`, `getPos`, `setPriority`.
* `src/game/atari.cpp`: `cSatMgr::check`, `checkRect`, `checkAir`,
  `scrAtCheckSphere`, `scrAtCheckSphereAir`, `wallAdjust`, `adjust`, `atck`,
  `at_pos_calc` und die bereits integrierten SAT-/EAT-Kernel.
* `src/game/sub2.cpp` und `math_sub.cpp`: `RotVector`, `low_RotMatrix`.

`ActorCollisionInfo` übernimmt die benannten Felder aus `include/atariInfo.h`.
Das originale `x48/m_pList`-Union wird als vollständiger nativer Pointer
geführt. `init0` nullt den gesamten Pointer. Die Model-/Motionfelder werden
direkt angesprochen; die originalen Casts `((cEm*)pMod)` sind nicht nötig.

`CollisionRuntime::checkActor` verlangt Besitzer aller verknüpften Modelle
und Zusatzinfos, erkennt zyklische beziehungsweise unbesessene Links vor der
Dereferenzierung und prüft Partindizes und endliche Eingaben. Floor-Normalen
werden vor dem Ende der SAT-Abfrage in stabilen Modellspeicher kopiert.
Das Freigeben oder Ersetzen eines SAT-Assets hinterlässt dadurch keinen
Pointer auf dessen Normaltabelle. Threadlokale Querykontexte werden auch bei
Fehlern restauriert. Fehlende Kollisions-Debugdarstellung bleibt ein Fehler;
PPC-Profiling wird nicht als ausgeführte Hardwarefunktion simuliert.

Dies schließt **Szenariokollision** an die native Modellverwaltung an.
`EmAtCheck`, dynamische Actor-vs-Actor-Listen und die vollständige Player-
Kollisionsroutine benötigen weiterhin den echten EmMgr. SAT-/EAT-Testprobes
sind keine vollständigen Player-Ticks.

## GameTask und verbleibende gemeinsame Consumer

Der tatsächliche GameTask-Stand bleibt `Rno0=2`, nach den bereits integrierten
originalen `gameInit`- und `gameStageInit`-Bodys. Der Adapter in
`port/src/runtime_original.h` verweigert weiterhin das gesamte
`gameRoomInit/model/light/player`. `gameMainLoop` ist nicht erreicht.

`src/game/game.cpp::gameRoomInit` verlangt vor `EmMgr.create(0,0)` unter anderem
Room-Heaps, ReadPlayerData/ReadAreaData, CNS/SMD, die Model-/Parts-/Em-/Obj-Pools,
Esp-/Espgen-/Ctrl-Pools, Light-/Shadow-/Dmg-Initialisierung, SAT/EAT und
Eventmanager. Einzelne Datenconsumer sind bereits geprüft; ihre gemeinsame
spielseitige Ausführung wurde nicht durch einen Diagnoseframe ersetzt.

Der nächste konkrete gemeinsame Block ist:

1. Den lebenden generischen `cObjMgr` anbinden: `src/game/obj.cpp::construct`,
   aktive Listen, `destroy`/`destroyNow` und unterschiedliche native Objektgrößen.
   Der vorhandene Scroll-Diagnosemanager unterstützt nur Scrollobjekte und ist
   kein gültiger Ersatz für Weapon-/Effectobjekte.
2. `cPlayer::endCamera`, Esp-/Espgen-/Efm-Besitzer und originalen
   `ReadWepData`-/Weapon-Modul-Dispatch verbinden. Der Startzustand bestimmt die
   Waffe; Tests oder Adapter dürfen keine andere Waffe einsetzen.
3. Danach `init1` mit Neck, Yarare, Licht, Cloth und Attachmentkamera sowie
   die vollständigen Player-Virtuals/Body-/Weaponroutinen für `startUp` schließen.
4. Erst nach diesen Voraussetzungen die originalen EmMgr-Pools, erfolgreiche
   `construct`-Rückkehr und deren Guid/Flags/Listen in `gameRoomInit` veröffentlichen.

Die Folgefunktionen werden ausdrücklich als fehlend gemeldet und liefern
keinen leeren Erfolg. Dies ist der nächste H1-Arbeitsumfang, keine begonnene
weitere Hybridphase.

## Geänderte vollständige Projektdateien

* `port/CMakeLists.txt`
* `port/config/collision-sources.json`, `port/config/player-sources.json`
* `port/include/re4/port/actor_collision.h` (neu), `collision.h`, `consumers.h`,
  `model_types.h`, `player.h`
* `port/src/collision.cpp`, `collision_original.h`, `consumers.cpp`,
  `model_runtime.cpp`, `player.cpp`, `player_original.h`
* `port/tools/generate_collision.py`, `generate_player.py`, `test_collision.py`,
  `test_game_runtime.py`
* `port/tests/game_runtime_tool.cpp`, `game_runtime_verification.cpp`,
  `model_verification.h`, `test_runtime_sources.py`
* `port/docs/hybrid-h1-leon-actor-status.md` (dieser Bericht)

Die Dateien sind vollständig im Projekt gespeichert. Buildoutputs,
Originalressourcen, JSON-Prüfberichte und GPU-Aufnahmen bleiben außerhalb Git.
Der alte Android-Port wurde ausschließlich gelesen. `master` bleibt
`9b76ff003081d6ece8b838453fb8310743a972c7`, `origin/master` bleibt
`15cf64f80e08bbc64712c39be330b75d1512a0d0`; dessen README wurde berücksichtigt.
Kein Merge, Rebase oder Push. Vulkan und die zentrale PadRead-Anbindung bleiben
erhalten. MSAA Aus/2×/4×, AF 2×/4×/8×, Depth of Field mit Nah-/Zielfokus und
persistente Einstellungen bleiben Anforderungen der weiteren Grafikarbeit.

## Tatsächlich ausgeführte Tests

Alle nachstehenden Resultate wurden mit dem endgültigen Code ausgeführt.
Windows nutzt MSVC 18 mit getrennten x86-/x64-Builds. Android nutzt NDK
28.2.13676358, CMake 4.1.2 und die vorhandene Debug-/Test-APK. Das reale Gerät
war `dbdcacc`, Retroid Pocket Flip2, Android 13; unterstützte ABIs waren
`arm64-v8a,armeabi-v7a,armeabi`. Die Prozess-Pointerbreite wurde zusätzlich
im Test beziehungsweise bei der ABI-spezifischen APK-Installation geprüft.

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| Host-CTest | 27/27 | 27/27 | nicht zutreffend | nicht zutreffend |
| Native Core-Suite, zwei Gerätewiederholungen | in CTest enthalten | in CTest enthalten | 13.386 Checks, 0 Fehler pro Lauf | 13.387 Checks, 0 Fehler pro Lauf |
| Final installierte APK/Instrumentation | nicht zutreffend | nicht zutreffend | 20/20 | 20/20 |
| Originaldaten-/GameRuntime-Suite, beide Discs, zwei Wiederholungen | bestanden | bestanden | bestanden | bestanden |
| Erhaltene Konstruktorfolge, tatsächlicher Scheduler-Stack | 16 Versuche | 16 Versuche | 16 Versuche | 16 Versuche |
| Originale SAT-/EAT-Rays, zwei Wiederholungen | 13.930 Abfragen | 13.930 Abfragen | 13.930 Abfragen | 13.930 Abfragen |
| Originale Szenariokollision, zwei Wiederholungen | 60 Probes mit je 3 Checks | 60 Probes mit je 3 Checks | 60 Probes mit je 3 Checks | 60 Probes mit je 3 Checks |
| Wiederholter echter Vulkan-Readback | kein Host-GPU-Lauf | kein Host-GPU-Lauf | 2/2 | 2/2 |

Der zusätzliche ARM64-Corecheck prüft einen Pointer oberhalb des 32-Bit-
Adressbereichs. Scheduler-/Background-/Decodertraces stimmen überein; die
unterschiedliche Prüfzahl bedeutet keine unterschiedliche Gameplaysemantik.
Die Bootzustände einschließlich Konstruktorblocker und Rollback sind auf
allen vier Architekturen und Wiederholungen semantisch identisch.

Die Originaldaten-Suite führt vorhandene Message-, Font-, Menü-, Save-,
Checkpoint-, Licht-, Modell-, FCV-/SEQ-, MotionHokan-/cMot3- und IK-Prüfungen
weiter aus. Die neuen Konstruktorprüfungen verwenden beide originalen
`em/pl00.drs` und beide vorhandenen Startup-Testzweige. Das sind echte
Originalconsumer-Ausführungen auf kontrollierten Testeingaben; sie sind
weiterhin keine zusammenhängende `gameRoomInit`-/Player-Simulation.

Die Kollision prüft zehn originale SAT-/EAT-Dateien aus Disc-0-Räumen r100,
r102, r119, r120 sowie Disc-1-r300. Hitattribute, Normalen, Rayresultate und
die Endpositionen der Szenarioprobes stimmen auf allen vier Architekturen
überein. Die maximale Abweichung der Raypunkte vom unabhängigen
Triangle-/Floor-Orakel beträgt **0,015125000005355105 Engineeinheiten**.
Beim synthetischen horizontalen Floorfixture beträgt die Toleranz 0,02
Einheiten: Der originale lange Floor-Ray verursacht Rundung in Float32.
Der Originalalgorithmus wurde dafür nicht verändert. Diese Zahlen sind
kein Vergleich mit auf GameCube ausgeführten PPC-Rechnungen. `std::sin`/
`std::cos` ersetzen die PPC-Trigonometrie; deren Bitgleichheit ist offen.

Die Szenarioprobes benutzen aus den vorhandenen SAT-Rayprüfungen abgeleitete
Positionen und Originalmasken. Sie prüfen `cAtariInfo::move` und die originale
Szenariokollision mehrfach, nicht Leons originale Startposition,
Controllerbewegung, dynamische Actorlisten oder vollständige Player-Ticks.
Die numeric Damage-/Waist-/Spherefixtures sind ausdrücklich isolierte Tests.

### GPU-Regression und Grenzen

Der bestehende r100-Diagnosepfad lief auf Adreno 650 mit aktiver
`VK_LAYER_KHRONOS_validation` (Android-Binaries 1.3.290.0). Die Aufnahmen
verwenden `Evd/r100s40.evd`, Cut 0, Frame 30, `pl0000` und **Debugview 1
(Texturen)**. Sie sind kein Ergebnis von `startUp` oder `gameMainLoop`.
Die Dateien bleiben unter
`E:\Development\Android\RE4-Data\hybrid-h1-actor-gpu-r100` außerhalb Git.

* ARM32: `gpu-armeabi-v7a-1.png`, `gpu-armeabi-v7a-2.png`.
* ARM64: `gpu-arm64-v8a-1.png`, `gpu-arm64-v8a-2.png`.
* Alle vier RGBA-Readbacks:
  `7c2676d8361694ceb5f9db106b8c3d8bec3126caa032b96be519b4ceb1cf5cd6`.
* Je 226.966 vom Clear abweichende Pixel; OT-/TEV-/Lichtzustände ebenfalls
  ABI- und wiederholungsübergreifend identisch.
* Je **0 Validationfehler, 1 Performancewarnung**. Das aktuelle Retroid-
  Protokoll bestätigt `WARNING-Swapchain-PreTransform`: `IDENTITY` statt
  Surface-`ROTATE_90`; die Presentation-Engine transformiert zusätzlich.
* Mittlere codierte RGB8-Luma 49,6166688; Median 48,1352, P05 9,0722,
  P95 92,2848. Das ist keine photometrische Luminanz und kein Nachweis
  korrekter Originalbeleuchtung. Die dunkle endgültige Darstellung bleibt
  offen; es wurde kein Aufhellungsfaktor hinzugefügt.

Die sichtbare Ausgabe ist gegenüber dem bestehenden H1-Diagnosepfad
unverändert. Kein identischer GameCube-Spielzustand, keine neue vollständige
Actorpose und keine originale Player-Schulterkamera wurden dadurch bewiesen.
Die beobachteten 15/16 beziehungsweise 16/31 gerenderten Frames sind
asynchrone Testlaufzähler und keine FPS-Messung. Controllerregressionen wurden
automatisiert geprüft; ein neuer physischer Tastentest fand nicht statt.

### Reproduktion

Aus dem Projektroot und einer passenden MSVC-x86- beziehungsweise
MSVC-x64-Developer-Shell lassen sich die beiden Hostbuilds mit dem vorhandenen
Skript prüfen:

```powershell
# Jeweils in der passenden Compilerumgebung; CMake/Ninja im PATH.
.\port\scripts\test-host.ps1 -BuildName host-x86
.\port\scripts\test-host.ps1 -BuildName host-ninja

$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskData = 'E:\Development\Android\RE4-Data\import-phase3a'
$taskAdb = 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'

.\port\scripts\build-android.ps1 `
    -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' `
    -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' `
    -Python $taskPython -Offline `
    -ValidationLayers 'E:\Development\Android\RE4-Fixed\port\build\validation\layers\android-binaries-1.3.290.0'

.\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc

& $taskPython port/tools/test_game_runtime.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-actor-host64' `
    --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_collision.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-actor-collision-host64' `
    --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
# Für x86: host-x86/re4_game_tool.exe und getrennte *-host32-Ausgabeverzeichnisse.

& $taskPython port/tools/test_game_runtime.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-actor-device' `
    --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_collision.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-actor-collision-device' `
    --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_gx_scene.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-actor-gpu-r100' `
    --adb $taskAdb --serial dbdcacc --room 100 --event Evd/r100s40.evd `
    --model pl0000 --cut 0 --frame 30 --debug-view 1 --repeat 2
```

Die Geräte-Skripte erwerben selbst den Windows-Mutex
`Global\Retroid_ADB_Exclusive` mit 60 Sekunden Timeout. Die Pythonrunner
halten ihn über den vorhandenen PowerShell-Broker während ihrer kompletten
Hardwarefolge; ein zusätzlich sperrender Elternprozess darf sie nicht
einschließen. ADB verwendet immer `-s dbdcacc`. Bei Fehlern geben die
`try/finally`- beziehungsweise Brokerkontexte den Mutex frei. Keine
projektübergreifende Bereinigung, kein `kill-server` oder Geräte-Neustart
fand statt. Die abschließende zusätzliche Loglesung wurde ebenfalls gesperrt.

Ignorierte lokale Protokolle: `port/build/h1-actor-tests-x64.log`,
`h1-actor-tests-x86.log`, `h1-actor-core-device.log`,
`h1-actor-app-device-final.log`, `h1-actor-original-{x64,x86,device}.log`,
`h1-actor-collision-{x64,x86,device}.log`, `h1-actor-gpu-device.log`,
`h1-actor-validation-logcat.txt`, `h1-actor-evidence.log`.
Die automatisch generierten Originaldaten-JSONs bleiben in den jeweils
genannten externen Ergebnisverzeichnissen.

### Während der Entwicklung korrigierte Prüfprobleme

Die Prüfungen fanden einen echten Lebenszeitkonflikt: Die öffentliche
Modellfreigabe verweigerte den Rollback auf einem laufenden Scheduler-Stack.
Der private, unveröffentlichte Konstruktor-Rollback prüft jetzt exakten
Besitzer, Modellborrower und Eventreferenzen; die öffentliche Quieszenzprüfung
bleibt erhalten. Die wiederholten Originaldatentests liefen danach auch
im Scheduler. Generator-Mehrfachersetzungen, native Deklarationen und
Source-Provenienzprüfzahlen wurden korrigiert. Die ursprüngliche gewollte
16-Bit-Verengung im Kollisionscode hat eine eng begrenzte MSVC-Warnungsregel;
die nativen Adapter behalten `/WX` beziehungsweise `-Werror`.
Ein anfänglicher Sandbox-Zugriffsfehler wurde mit freigegebenem Testzugriff
wiederholt; kein fehlgeschlagener Lauf wurde als bestanden gezählt.

## Konkreter Entwicklungsgewinn und offene Abnahme

Die bisher getrennte native Modellwurzel und die Player-Konstruktorbodies
benutzen jetzt denselben Besitzer. Die echte pl00-Referenzauflösung,
pointerbreitenabhängigen Motiontabellen, ursprünglichen Work-Konstruktoren
und Rückabwicklung müssen für den nächsten gemeinsamen Actor-Schritt nicht
erneut entwickelt werden. Das alte Projekt blieb technischer Lesespender;
seine LeonPreview-Steuerung und Raumlogik wurden nicht übernommen.

Der verbleibende `ObjMgr`-Blocker erfordert den vollständigen gemeinsamen
Objekt-/Weapon-/Effect-Lebenszyklus und danach den echten Actor-Dispatch.
Eine leere Objektliste würde eine unbelegte Gameplayannahme einführen.
`startUp`, vollständige Player-Ticks, Actor-vs-Actor-Kollision und der
gemeinsame `gameRoomInit`-Pfad sind daher offen. **H1 ist nicht abgeschlossen.**
Keine folgende Hybridphase wurde begonnen.
