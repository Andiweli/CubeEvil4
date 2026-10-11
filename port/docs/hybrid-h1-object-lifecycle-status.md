# Hybrid H1: Gemeinsamer Objektpool und Weapon-/Effect-Freigabe

Ausgangspunkt: `41ec803b668fff505ea0a7f84a3d9c07dd7b29da`, Branch
`android-port`. Prüfungen am 11./12. Oktober 2026. Dieser Bericht ergänzt
`hybrid-h1-leon-actor-status.md`; frühere Berichte bleiben historische Nachweise.

## Ergebnis und Abnahme

**Geprüfter Teilstand. H1 ist weiterhin offen.** Der bisherige Fehler in
`cPlayer::weaponRelease → ObjMgr.getActiveWork` ist im Originaldatentest
überwunden. Der erhaltene Leon-Konstruktorbody führt jetzt die vollständige
`weaponRelease`-Funktion, `endCamera` und `weaponLoad` aus. Die Konstruktion
stoppt anschließend ausdrücklich an:

```
Original consumer unavailable: ReadWepData/owned weapon archive and native WeaponInitFunc module binding
```

`ReadWepData` wurde in diesem Schritt analysiert, aber noch nicht nativ
ausgeführt. Es wird kein Actor veröffentlicht. Der originale virtuelle
`cPlLeon`-Dispatch, `cPlayer::startUp` und vollständige Player-Ticks sind nicht
nachgewiesen. Der tatsächliche GameTask bleibt nach `gameStageInit` bei
`Rno0=2` am ungebundenen `gameRoomInit`-Consumer. Es gibt keinen künstlichen
Übergang zu `Rno0=3` und keinen neuen Nachweis einer laufenden Spielszene.

| H1-Kriterium | Tatsächlicher Stand |
| --- | --- |
| Gemeinsamer ObjMgr-Pool | Originale Listen-, Allokations- und Freigabebodies integriert; sechs benötigte Konstruktionsarten |
| Weapon-/Effect-Freigabe | Originale Löschbodies integriert; aktive Objektliste auch mit nichtleeren Testfällen geprüft |
| Vollständiger Leon-Actor | Offen; weiterhin typisierter Konstruktor-Ausführungskontext mit gemeinsamer Modellwurzel |
| Originales startUp | Nicht erreicht |
| Mehrere vollständige Player-Ticks | Nicht ausgeführt; vorhandene isolierte Motion-/Kollisionsprüfungen bleiben getrennt |
| Wiederholte Konstruktion/Freigabe | Konstruktorpräfix, Rückabwicklung und Objekt-/Modellbesitzer geprüft; kein kompletter Actor-Lebenszyklus |
| ARM32 und ARM64 | Dieser Teilstand tatsächlich auf Retroid getestet |

## Originalfunktionen und native Anpassungen

`generate_objects.py` extrahiert 39 vollständige Original-Funktionsbodies.
`object-sources.json` schützt 21 Originaldateien mit Prüfsummen. Änderungen
am Original führen zu einem Generatorfehler. Das generierte
`object-manifest.json` nennt Quellen, Signaturen, Zeilen und textuelle
Anpassungen; es liegt jeweils im ignorierten Buildverzeichnis. Der bestehende
Player-Generator enthält jetzt 53 Bodies, einschließlich `endCamera` und
`weaponLoad`.

| Originalquelle | Erhaltene Funktion bzw. Semantik |
| --- | --- |
| `include/cManager.h` | `roomInit`, `arrayAlloc`, `arrayFree`, drei `create`-Varianten, `createBack`, aktive Listen, `countActiveWork`, `destroy`, `destroyAll`, `dieCheck`, `cUnit`-Destruktor |
| `src/game/obj.cpp` | Beide `cObjMgr::construct`-Overloads, vollständige Auswahl der Original-IDs, `cObj::cObj`, `cObjMgr::destroy` |
| `src/game/event.cpp` | `cManager<T>::destroyNow` |
| `src/game/model.cpp` | `cModel::push` mit Freigabe durch den vorhandenen Modellbesitzer |
| `src/game/objWep.cpp` | `cObjWep::cObjWep` einschließlich initialer Parent-, Motion-, Sound- und Displayfelder |
| `src/game/lightInfo.cpp` | `cLightInfo::cLightInfo`, `init2`, `getLightNum` |
| `src/game/esp.cpp`, `est.cpp`, `esp_sub.cpp` | `PushEsp`, `EspDelete`, die drei `Effect*Delete`-Wrapper und originale Basiskonstruktor-/Destruktorbodies |
| `src/game/espgen.cpp` | `GetEspgenIdMax`, `PushEspgen`, `EspgenDelete` |
| `src/game/esp_efm.cpp` | `EfmDelete`, `EfmDeleteSub` mit den typisierten FREE-Strukturen für IDs 4, 5 und 9 |
| `src/game/pl_wep.cpp`, `pl_class.cpp` | Bereits extrahiertes `weaponRelease` jetzt angebunden, zusätzlich `weaponLoad` und `endCamera` |

Der Objektpool ist ein gemeinsamer Besitzer in `GameRuntime`, kein
Leon-spezifischer Ersatzpool. Aktive Listen und Suchreihenfolge stammen aus
`cManager`. `create` durchsucht Slots von vorne und fügt vorne ein;
`createBack` durchsucht von hinten und fügt hinten ein. GUIDs werden durch
den Originalbody vergeben und bei `roomInit` nicht zurückgesetzt.

Mit dem originalen Managerflag 2 entfernt `destroy` ein Objekt sofort aus
der aktiven Liste und setzt `0x200`. Der erste `dieCheck` ergänzt `0x400`,
der zweite zerstört die Zelle. `destroyNow` verwendet vorübergehend Flag 0.
Der Adapter stellt dieses Flag auch bei einer Ausnahme wieder her.

Native Poolzellen haben einen von den unterstützten nativen Typgrößen
abgeleiteten Stride. Rohes Löschen einer Arena-Zelle wurde durch typgerechte
Destruktion und eine konstruierte freie Basiszelle ersetzt. Ein Iterator
liest daher kein zerstörtes C++-Objekt. Handles prüfen Runtime, Raum-Epoche,
Slot und Generation. Kurzlebige interne Pointer dürfen nicht über Freigabe
oder Wiederbelegung hinweg gespeichert werden; langfristige Referenzen
verwenden Handles. Diese Zusatzprüfungen ersetzen keine Original-GUIDs.

Nativ unterstützt sind Konstruktorfelder/Work für `cObj00`, `cObj04`,
`cObj05`, `cObj09`, `cWepItem` und den `cObjWep`-Basiskonstruktor
(IDs `00/04/05/09/0a/0b`). Die übrigen Fälle der Original-Auswahl bleiben
konkrete Fehlergrenzen. Ebenso sind die konkreten `move`-Methoden dieser
Objekte noch nicht angebunden und werfen bei Aufruf. Eine erfolgreiche
Poolkonstruktion ist kein Nachweis ihrer Gameplay-Ausführung.

`ModelCpu::guid` und die benannten `cLightInfo`-Felder sind native Felder,
keine Überlagerung des GameCube-Layouts. Pointer bleiben pointerbreit;
GC-Offsets werden weiterhin über die vorhandenen geprüften BIN-/TPL-Views
aufgelöst. `init2` führt die originale Bereichsberechnung aus; der
Weapon-Basiskonstruktor nutzt damit seine originale Lichtdefinition.
Die neue Struktur allein implementiert keine fehlende Lichtregistrierung.

### Effect-Lebenszyklus und Grenzen

Sprite- und Generatorpools verwenden typisierte native Lifecycle-Views der
tatsächlich von den Löschfunktionen gelesenen Felder. Die GC-Spritegröße
`0x150` wird nicht als native Pointerarithmetik benutzt. Ownerpointer,
Core-Flags, Kind, Modellpointer und Modell-GUID bleiben erhalten. Die
`EfmDeleteSub`-Zugriffe prüfen den tatsächlichen nativen Objekttyp; es gibt
keinen Cast zwischen unvereinbaren Modellstrukturen.

Die originalen `cEsp`-Basiskonstruktoren und `Destruct` sind tatsächlich leer
in `esp_sub.cpp`; hier wird kein fehlender konkreter Consumer durch eine
leere Funktion ersetzt. Die Original-Generator-Destruktortabelle enthält
für die Haupt-IDs NULL-Einträge; die Anwendungstabelle beginnt mit zwei
NULL-Einträgen. Der Generator überprüft diese Quellcodebasis. Die konkreten
ressourcenbesitzenden Destruktoren der IDs `0x42..0x45` bleiben explizit
unverfügbar. Eine passende Löschanforderung wird vor Teilfreigaben abgelehnt.
Nicht passende Owner werden dabei nicht irrtümlich blockiert.

**Nicht integriert:** vollständiges `EspRoomInit`, `EspgenRoomInit`,
`EspDataLoad/Release`, `EstSet`, konkrete Effect-Erzeugung und deren Bewegung
oder Darstellung. Die öffentlichen `*Fixture`-Zugriffe dienen ausschließlich
den numerischen Lifecycle-Prüfungen. Sie werden nicht zum Erzeugen von
Gameplay-Effects verwendet. Die neuen Pools simulieren diese Systeme nicht.

## Originaldaten-Ausführung und GameTask

Die vorhandene Originaldatensuite lädt `em/pl00.drs` von beiden Discs über
den bestehenden DVD-/Yz2-Lader. Sie führt die erhaltenen Player- und
Leon-Konstruktorbodies auf einem echten Scheduler-Stack aus. Alle 32
Startup-Ergebnisdatensätze der vier Plattformen dokumentieren:

* 119 originale Gelenke und acht Modell-/Materialinfos aus pl00.
* 109 native Motionpointer-Slots und zwölf registrierte Slots; elf originale
  Debugcallback-Registrierungen.
* Ausgeführte Player-/Subwork-Konstruktoren und vollständiges `weaponRelease`.
* Nächster Fehler an `ReadWepData`; `returned_actor=false`,
  `em_mgr_dispatch_bound=false`, `startUp_executed=false`, `rolled_back=true`.

Jeder Startup-Datensatz enthält zwei Konstruktorversuche. Vor jedem Versuch
werden ein original konstruiertes ID-`0x0a`-Objekt und ein anderes Objekt
registriert. `weaponRelease` entfernt ausschließlich das vorgesehene Objekt
und hält es bis zur originalen verzögerten Destruktion reserviert. Der
Player-Modell- und Workbesitz wird bei der folgenden Ausnahme vollständig
zurückgenommen; aktive Modellzahl und Heapverbrauch kehren auf den vorigen
Stand zurück. Anschließend wird auch der Objektpool vollständig entleert.

Der erste Originalstart setzt `F_NO_WEP_EFF`; deshalb wird in diesen
Konstruktorversuchen kein bislang ungebundenes `EspDataRelease` ausgeführt.
Ebenso sind keine Thermo-/Scope-/Binocular-/PushObject-Kameramodi aktiv:
`endCamera` kehrt auf seinem ursprünglichen inaktiven Zweig zurück. Die
aktiven Spezialkamerazweige bleiben explizite Consumergrenzen. Vollständige
Ausführung des Funktionsbodys bedeutet hier nicht Abdeckung aller Zweige.

Die Poolkapazitäten dieses Consumer-Probes stammen aus originalem r120-CNS
über `originalRoomConstants` (NOBJ/NESP/NESPGEN). Es ist ausdrücklich nicht
`gameRoomInit`: Scrollobjekte wurden dort nicht registriert, deshalb wird
auch nicht behauptet, dass NOBJ allein die vollständige Raumkapazität sei.
Der Originalbody in `game.cpp` benötigt **NOBJ + SmdGetObjNum** und die
originalen Debugverdopplungen. Die Produktionsadapter selbst enthalten
keine r120-Regel und keinen fest programmierten Leon-Pool.

Zusätzlich initialisiert der gemeinsame Objektmodellbesitzer zweimal die
tatsächlichen pl00-BIN-/TPL-Slots 4/5 und gibt sie über `cModel::push` innerhalb
eines Scheduler-Tasks frei. Das prüft Originalmodellinitialisierung und
Besitz, ohne pl00 als erfundene Waffenressource zu deklarieren. Die private
Modellfreigabe prüft exakten Besitzer, Eventreferenzen und Borrower. Die
öffentliche Quieszenzprüfung wurde nicht abgeschaltet.

Die zentrale Raumretirement-Kette gibt konfigurierte Objekt-/Effectpools
vor dem Modellmanager frei und invalidiert Handles. Die bestehenden zwanzig
R10d-/R328-Modulwechsel prüfen das nun mit echten Original-Raumeinträgen
und numerischen Objektpools. Lebende Modellborrower oder nicht abgewickelte
Effect-Ressourcen werden weiterhin abgelehnt; vollständige Actor-/Effect-
Raumwechsel sind damit noch nicht behauptet.

Die ursprüngliche GameTask-Sequenz führt weiterhin `gameInit` und
`gameStageInit` aus. Danach gilt unverändert:

```
Rno0 = 2
Original consumer unavailable: gameRoomInit/model/light/player
```

Der neue Konstruktorprobe ersetzt diesen Consumer nicht. `ScenarioRoomInit`
und die vollständige `gameMainLoop` sind weiterhin nicht aus dem
GameTask-Raumstart erreicht. R120Init-/SceSleep-/Event-Abbruchregressionen
bleiben erhalten; die vollständige Introausführung ist weiterhin durch
Sofdec und weitere Event-Consumer begrenzt.

## Tatsächlich ausgeführte Prüfungen

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| Bestehendes CTest-Set | 27/27 | 27/27 | Nicht über CTest ausgeführt | Nicht über CTest ausgeführt |
| Native GameRuntime-Suite | 1.197 Checks | 1.197 Checks | 1.197 Checks, mehrfach | 1.197 Checks, mehrfach |
| Kern-/Scheduler-/Background-/Yz2-Suite | Teil des CTest-Sets | Teil des CTest-Sets | 13.387 Checks, zweimal | 13.387 Checks, zweimal |
| APK-Instrumentierung | Nicht ausgeführt | Nicht ausgeführt | 20/20 | 20/20 |
| Beide Discs, Messages/NewGame/Model/Licht/Motion/Raumconsumer | Zwei Wiederholungen | Zwei Wiederholungen | Zwei Wiederholungen | Zwei Wiederholungen |
| Original-SAT/EAT-Abfragen | 13.930 | 13.930 | 13.930 | 13.930 |
| GPU-Diagnose mit r100/pl00 | Kein Windows-Vulkanlauf | Kein Windows-Vulkanlauf | Zwei Aufnahmen | Zwei Aufnahmen |

Retroid Pocket Flip2, Serial **dbdcacc**, Android **13**, API **33**.
Unterstützte und tatsächlich ausgeführte ABIs: `armeabi-v7a`, `arm64-v8a`.
Die nativen Executable- und APK-Prüfsummen sowie die installierte ABI sind
in den Testprotokollen erfasst. Es gab keinen neuen physischen Tastentest;
die bestehenden zentralen PadRead-/Controllerregressionen wurden automatisiert
ausgeführt. Keine zweite Spielersteuerung wurde eingeführt.

Die neuen Lifecycle-Prüfungen umfassen volle Pools, Front-/Back-/Index-
Erstellung, GUIDs, verzögerte und sofortige Destruktion, Wiederbelegung,
veraltete Handles, abgelehnte Konstruktoren, Owner- und GUID-Filter,
konkrete Generatorfehler sowie zwanzig Allokations-/Freigabezyklen.
Diese Fixtures führen Original-Verwaltungsbodies aus, aber keine originalen
Player-Ticks. Source-Provenienz, deterministische Generierung und die
Ablehnung geänderter Originalquellen bestehen ebenfalls.

Die beiden Startup-Tracehashes (je nach ursprünglichem NewGame-Testinput)
sind über x86/x64/ARM32/ARM64 und die Wiederholungen identisch:

```
424c6f37f7629775011d3a1767985843da2e52a45b69b5045f62796c7b550130
aa8d6a8b542917baf965f742066cec72944b1e163ac24ba453afa5e68931eff6
```

Die SAT/EAT-Prüfung verwendet zehn Originalressourcen aus r100, r102, r119,
r120 und r300. Die maximale Punktabweichung gegenüber dem vorhandenen
unabhängigen Orakel bleibt `0.015125000005355105` Engineeinheiten. Das ist
keine auf GameCube gemessene PPC-Bitgleichheit. Die vorhandenen numerischen
Toleranzen und Originalalgorithmen wurden nicht verändert.

### GPU-Regression

Der bestehende Diagnosepfad nutzt `Evd/r100s40.evd`, Modell `pl0000`, Cut 0,
Frame 30 und **Debugview 1 (Texturen)**. Er ist kein startUp-/gameMainLoop-
Frame und kein Beweis originaler Spielerposition oder Schulterkamera.

Alle vier aktuellen RGBA-Readbacks sind bytegleich zum bisherigen Nachweis:
`7c2676d8361694ceb5f9db106b8c3d8bec3126caa032b96be519b4ceb1cf5cd6`.
Jeweils 226.966 Pixel weichen vom Clear ab. Die beobachteten Framezähler
sind ARM32 82/20 und ARM64 14/23; das sind asynchrone Testzähler, keine FPS.
OT-, TEV- und Lichtzustände sind ABI- und wiederholungsübergreifend gleich.
Die Bilder wurden auch visuell als vorhandene r100-Brücken-/Leon-Diagnose
kontrolliert; sie zeigen keine neue vollständige Originalspielszene.

Khronos-Validation 1.3.290.0 ist auf Adreno 650 aktiviert: jeweils **null
Fehler und eine Performancewarnung**. Das aktuelle gefilterte Geräteprotokoll
weist `WARNING-Swapchain-PreTransform` nach: IDENTITY statt ROTATE_90,
zusätzliche Transformation durch die Presentation-Engine. Diese bestehende
Rendering-Abweichung wurde in H1 nicht durch einen allgemeinen Rendererumbau
kaschiert. Mittlere codierte RGB8-Luma 49.6166688, Median 48.1352,
P05 9.0722, P95 92.2848. Debugview 1 ist kein Beleuchtungsnachweis; die
Originalhelligkeit bleibt offen. Kein pauschaler Aufhellungsfaktor wurde ergänzt.

Aufnahmen außerhalb Git:
`E:\Development\Android\RE4-Data\hybrid-h1-objects-gpu-r100\gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`.

### Fehler während der Entwicklung

Ein neuer Sofortfreigabetest fand einen echten Heapfehler: Eine Generator-
Anpassung hatte das GC-`delete p` im Flag-0-Zweig nicht erfasst. Der native
Pfad verwendet jetzt typgerechte Arena-Destruktion. Die Source-Prüfung
verbietet verbliebenes `delete p` und rohe Arena-Stride-Ausdrücke im
generierten Body. Effect-Zellen werden einzeln per Placement konstruiert,
ohne Abhängigkeit von Placement-Array-Cookies.

Clang lehnte den ursprünglichen anonymen Strukturtyp in einer anonymen
Union mit `-Werror` ab. Dieselben Call-Bytefelder sind jetzt ein benannter
Typ außerhalb der Union. MSVC unterdrückt originale beabsichtigte
Integerverengungen nur für `object_control.cpp`; Adapter behalten `/WX`
beziehungsweise `-Werror`. Sandbox-Zugriffsfehler auf Test- und Gradlecaches
wurden mit freigegebenen Wiederholungen behoben. Fehlgeschlagene Läufe
wurden nicht als bestandene Tests gezählt. Der Gradlelauf war erfolgreich,
einschließlich APK-Paketierung und `lintDebug`; bestehende SDK-XML-
Versionswarnungen bleiben dokumentiert.

Die abschließende Logerfassung verwendete zunächst einen aktuellen PID,
obwohl der GPU-Runner die App regulär bereits beendet hatte. Der korrigierte
Lauf filtert mit dem im GPU-Bericht erfassten PID. Logerfassung und Entfernung
des validierten privaten `/data/local/tmp/re4-game-<UUID>`-Stagings waren
danach erfolgreich; dies war kein fehlgeschlagener GPU-Test.

## Reproduktion und Gerätekoordination

Hostbuilds erfolgen aus einer passenden MSVC-x86-/x64-Developer-Shell mit
dem bestehenden `port/scripts/test-host.ps1` (`host-x86` bzw. `host-ninja`).
Android verwendet weiterhin NDK `28.2.13676358`, CMake `4.1.2`, Gradle
`8.13` und `build-android.ps1` für beide ABIs.

```powershell
$taskPython='C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskData='E:\Development\Android\RE4-Data\import-phase3a'
$taskAdb='C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'

.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' `
    -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python $taskPython -Offline `
    -ValidationLayers 'E:\Development\Android\RE4-Fixed\port\build\validation\layers\android-binaries-1.3.290.0'
.\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc

& $taskPython port/tools/test_game_runtime.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-objects-host64' `
    --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
# x86: host-x86/re4_game_tool.exe, separater *-host32-Ausgabeordner.
& $taskPython port/tools/test_game_runtime.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-objects-device' `
    --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_collision.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-objects-collision-device' `
    --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_gx_scene.py --data $taskData `
    --output 'E:\Development\Android\RE4-Data\hybrid-h1-objects-gpu-r100' `
    --adb $taskAdb --serial dbdcacc --room 100 --event Evd/r100s40.evd `
    --model pl0000 --cut 0 --frame 30 --debug-view 1 --repeat 2
```

Alle Hardwarefolgen halten `Global\Retroid_ADB_Exclusive` über
`System.Threading.Mutex` in PowerShell, mit 60 Sekunden Timeout und
`try/finally`-Freigabe. Pythonrunner nutzen den vorhandenen PowerShell-Broker;
kein zusätzlicher sperrender Elternprozess umschließt sie. Der Mutex deckt
ADB-Serialprüfung, Transfers, Installation, Start, Tests, Auswertung und die
jeweilige Bereinigung ab. ADB verwendet ausschließlich `-s dbdcacc`.
Es gab keinen Serverkill, Reboot oder fremden Paket-/Dateizugriff.

Nachweise liegen in `port/build/h1-obj-{tests-x86,tests-x64,core-device,
app-device,original-x86,original-x64,original-device,collision-x86,
collision-x64,collision-device,gpu-device,evidence}.log`, den zugehörigen
Buildlogs und `h1-obj-validation-logcat.txt`. Private Ergebnisdateien liegen
in den oben genannten externen Verzeichnissen. `port/build/adb-coordination`
enthält die Brokerjournale mit Erwerb/Freigabe und den ADB-Aufrufen.

## Vollständig gespeicherte Projektdateien

Neu:

* `port/config/object-sources.json`
* `port/tools/generate_objects.py`
* `port/include/re4/port/objects.h`, `effects.h`
* `port/src/object_original.h`, `objects.cpp`, `effect_original.h`, `effects.cpp`
* `port/tests/object_verification.h`
* `port/docs/hybrid-h1-object-lifecycle-status.md`

Geändert:

* `port/CMakeLists.txt`
* `port/include/re4/port/consumers.h`, `game_runtime.h`, `model_types.h`, `player.h`
* `port/src/consumers.cpp`, `game_runtime.cpp`, `player.cpp`, `player_original.h`
* `port/tests/consumer_verification.h`, `game_runtime_tool.cpp`, `game_runtime_verification.cpp`, `test_runtime_sources.py`
* `port/tools/generate_player.py`, `test_game_runtime.py`

Buildprodukte, Diagnosehilfen, private Datenberichte und GPU-Aufnahmen werden
nicht eingecheckt. Keine Originalassets wurden übernommen. Das alte Projekt
`RE4-Android-Decomp` blieb ausschließlich Lesereferenz; seine LeonPreview-
Steuerung wurde nicht übernommen. `master` bleibt bei `9b76ff00`,
`origin/master` bei der berücksichtigten README-Aktualisierung `15cf64f`.
Kein Merge, Rebase oder Push fand statt.

## Nächste konkrete gemeinsame Abhängigkeiten

1. **ReadWepData und Waffenmodulbindung.** `src/game/read.cpp:802` wählt über
   die originalen Charaktertabellen Dateieinträge, lädt per DVD/Yz2, prüft
   Größen und führt den Modulprolog aus. Leons Startwaffe 2 verwendet
   `wep_data_leon[2] = {9,10,0}`. `FileTbl` in `dvd.cpp` weist auf
   `em/wep02.das` und `rel/wep02.rel`; die Originalfunktion schaltet den
   Datennamen bei vorhandenem DLL auf DRS um. Native Archivbesitzer,
   Rückabwicklung und native Modulcallbacks müssen diese festen GC-Adressen
   und PPC-Prologaufrufe ersetzen. Ein validierter REL ist kein ausführbarer
   ARM-Prolog.
2. **Wep02-Konstruktion und Effectbesitz.** `src/wep02/wep02.cpp::_prolog`
   registriert `Wep02_init`, `PlHandgunMove` und `ObjInitFunc[0x21/0x27]`.
   `Wep02_init` erstellt Ruger/Mauser über `createBack`, ruft ihre virtuellen
   `init`-/`setMotion`-Funktionen auf und lädt `EFF_WEP02`. Diese konkreten
   Klassen, Attachment-/Motionbindungen und Effectdaten fehlen weiterhin;
   der vorhandene Basiskonstruktor ersetzt sie nicht.
3. **Vollständiger Player-Actor.** Nach Waffeninitialisierung benötigt die
   erhaltene Leon-Konstruktorfolge `init1` (unter anderem Yarare, Cloth,
   Licht/Kollision), Effectdaten und `startUp`. Ein kompatibler nativer
   cPlayer/cPlLeon-Typ samt originalem virtuellem Dispatch und vollständiger
   EmMgr-Konstruktion muss diesen Kontext ablösen. Erst dann sind mehrere
   echte Controller-/Player-/Motion-/Kollisionsticks ein sinnvolles
   Abnahmekriterium.
4. **gameRoomInit anbinden.** Die tatsächliche Reihenfolge umfasst
   `gameRoomMemInit`, ReadPlayerData/ReadAreaData, DC/ActBtn, CNS/SMD,
   Model-/Parts-/Em-/Objpools, Effectdaten und -pools, Ctrl-/Licht-/Shadow-/
   Dmg-/Filter-/Item-/Render-Consumer, SAT/EAT und schließlich Actor-/Event-
   Registrierung. Die vorhandenen Funktionen müssen als gemeinsame
   Consumer zusammengeführt werden; die neue Pool-API allein erfüllt diesen
   Body nicht. Insbesondere sind aktive Light-/Effectborrower vor Modell-
   oder Heapwechseln in der ursprünglichen Reihenfolge abzumelden.

Die späteren MSAA-Optionen Aus/2x/4x, AF 2x/4x/8x, Depth of Field mit
Nah-/Zielfokus und persistente, hardwarevalidierte Einstellungen bleiben
verbindlich. Vulkan, zentrale Controlleranbindung, MotionHokan und die
vorhandenen Originalsysteme wurden erhalten. Keine weitere Hybrid- oder
Engine-Hauptphase wurde begonnen.
