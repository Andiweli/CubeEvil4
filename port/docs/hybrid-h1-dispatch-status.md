# Hybrid H1: nativer Leon-Dispatch – geprüfter Teilstand

Ausgangsstand: `7246252e3a9ecff3c8f9f6624f1e933442e363fa` auf
`android-port`. Prüfung am 12. Oktober 2026. **H1 bleibt offen.**

Der originale Aufruf von `startUp()` erreicht jetzt über eine native
C++-Hierarchie die tatsächliche `cPlayer::move`-Implementierung, deren
Idle-Routine, primäre Motion und Hals-Motion. Die Konstruktion endet weiterhin
mit einer ausdrücklichen Ausnahme an der Interaktionsverwaltung. Es gibt noch
keinen vollständig zurückgekehrten originalen Leon-Konstruktor, keinen
erfolgreich zurückgekehrten `startUp()` und keine vollständigen Player-Ticks.

Die 27 Hosttestgruppen, Android-Builds und nachstehend aufgeführten
Hardwareprüfungen bestehen. Diese Prüfungen unterscheiden erfolgreich
ausgeführte Originalfunktionen von der erwarteten, geprüften Abbruchgrenze.
Ein bestandener Teilstandtest ist keine H1-Abnahme.

## Nachgewiesene Originalausführung

Die bestehende Originalkonstruktion läuft mit unveränderten importierten
G4BE08-Ressourcen und den bereits gebundenen Waffenmodulen. Der zusätzliche
Pfad lautet:

```text
originale cManager::create(0, 0)-Spezialisierung
  -> originale cEmMgr::construct, Leon-Fall
  -> native typisierte Platzierung und erhaltene cPlayer-/cPlLeon-Konstruktorbodies
  -> originale init1 / Cloth- und Body-Initialisierung
  -> originales cPlayer::startUp
  -> virtueller NativeLeonActor::move [originaler cPlLeon::move-Body]
  -> NativePlayerActor::move [Besitzerprüfung und Delegation]
  -> originaler cPlayer::move-Body
  -> originale Damage-/Key-/Shape-/Binocular-/Partner-Prüfungen
  -> originales pl_R0_Move / pl_R1_Footwork
  -> originale motionSet / dmMotCk / MotionSetCore
  -> originale cPlNeck::init / motSet / move / getTarget
  -> originales MotionMove / MotionSequenceCtrl
  -> ausdrücklicher Abbruch vor noch ungebundenem actionSelect
```

`cManager::create` veröffentlicht den Actor erst nach erfolgreicher
Konstruktion in der Alive-Liste. Diese originale Reihenfolge bleibt erhalten.
Der fehlgeschlagene Actor wird nicht veröffentlicht und nicht als fertiger
Spieler zurückgegeben. Die physische native C++-Instanz während dieses
Konstruktionsversuchs darf nicht mit einem vollständig initialisierten
originalen Gameplay-Actor gleichgesetzt werden.

| Originalquelle und Funktion | Tatsächlicher Nachweis |
|---|---|
| `src/game/em.cpp:76`, `cEmMgr::construct` | Leon-Zweig betreten; vollständige Rückkehr noch ausstehend |
| `include/cManager.h`, `create`, `arrayAlloc`, Listen- und Freigabefunktionen | Native Speicher-/Listen-Spezialisierung; fehlgeschlagene Konstruktionen rückabgewickelt |
| `src/game/pl_leon.cpp:63`, `cPlLeon::move` | Tatsächlicher virtueller Leon-Dispatch |
| `src/game/em.cpp:301`, `setStatus`, `clearStatus`, `checkStatus` | Originale Statusoperationen; IK-Status im nativen Motionkontext |
| `src/game/player.cpp:294`, `cPlayer::move` | Originalkörper betreten; nur der Prefix bis zum Interaktionsblocker kehrt aus seinen Unteraufrufen zurück |
| `src/game/player.cpp:395`, `pl_R0_Move`; `:406`, `pl_R1_Footwork` | Originale Routinen- und Idle-Motion-Ausführung |
| `src/game/pl_class.cpp:646`, `dmgCheck` | Originale Abfrage des konfigurierten Damage-Pools; keine Gameplay-Treffer simuliert |
| `src/game/pl_class.cpp`, `moveBinocular`, `subCharLiveCheck` | Originale neutrale Zweige; aktive Kamera-/Partnerpfade bleiben explizit ungebunden |
| `src/game/pl_class.cpp:513`, `motionSet`, `dmMotCk` | Originale Auswahl der tatsächlichen Waffen-/Spieler-Motionpaare |
| `src/game/pl_class.cpp`, `cPlNeck::init`, `motSet`, `move`, `getTarget`, `setMode` | Originalalgorithmen auf typisierten Besitzern und unveränderten FCV-Daten; keine Zielgegner erzeugt |
| `src/game/motion.cpp`, `MotionSetCore`, `MotionMove`, `MotionSequenceCtrl` | Primäre und sekundäre Work initialisiert; ein originaler Motionaufruf innerhalb des fehlgeschlagenen Starts |

Die Generator-Manifeste enthalten 21 Actor-, 20 Damage- und 27
Player-Funktionskörper beziehungsweise Templatespezialisierungen. Diese
Anzahl bezeichnet nicht 68 vollständig ausgeführte Gameplay-Systeme.

Zusätzlich übernommen und kompiliert, aber im Leon-Start noch nicht erreicht:
`shadowCtrl`, `visibleCtrl`, `seqSeCtrl`, `checkCtrl`,
`cModel::partsFixAdjust`, `PartsWorldPosCalc`, `cPlWep::move`,
`CameraQuasiFPS::setPlayerLocation` und `cPlLeon::checkXbutton`.
`ShapeMove` ist gebunden; vollständige aktive Morph-Verarbeitung ist damit
nicht nachgewiesen. Die drei leeren beziehungsweise mit 0 antwortenden
`cPlayer`-Basisvirtuals werden direkt aus ihren tatsächlich so definierten
Inline-Bodies in `include/player.h` generiert. Leon-Cloth wird weiterhin
ausdrücklich abgewiesen, nicht durch den leeren Basisbody ersetzt.

## Native Grenzen und Speicherbesitz

`NativeEmActor -> NativePlayerActor -> NativeLeonActor` verwendet regulären
C++-Dispatch. Es werden keine GameCube-VTables importiert, keine nativen
Objekte auf `EM_WORK_SIZE` gelegt und keine GameCube-Pointer als native
Objektadressen verwendet. Die originalen Konstruktorbodies laufen nach Beginn
der erforderlichen nativen Objektlebenszeiten, damit ihre Ausnahmen sicher
rückabgewickelt werden können. Die Source-Fingerprints verhindern eine
ungeprüfte Änderung dieses Konstruktorpfads.

Der Actor-Pool verwendet `sizeof(NativeLeonActor)`, 32-Byte-Ausrichtung und
geprüfte Slotzugriffe. Die Modelle und bestehenden Player-Unterobjekte behalten
ihre gemeinsamen Ressourcenbesitzer. Die zweite Motion-Work `m_SubMot`
liegt am nativen Actor. `initializeAuxMotion` akzeptiert einen typisierten
Actor und prüft ihn gegen den Besitzer des Modellhandles; eine fremde
Motion-Work oder ein Stack-Temporär wird nicht als Modellwork übernommen.
FCV-/SEQ-Leases bleiben während der Originalaufrufe erhalten.

Der Actor-Motionwechsel verwendet `MotionSetCore` ohne den zusätzlichen
`MotionClear` einer isolierten Reset-Probe. Vorherige Datenbesitzer werden
erst nach dem Wechselaufruf freigegeben. Aktive `cMot3`-Übergänge müssen über
ihren originalen Blendpfad erfolgen und werden an dieser Schnittstelle
ausdrücklich abgewiesen.

`Pl_func_tbl` und `Pl_func_move_tbl` werden einschließlich Reihenfolge und
Null-Einträgen aus `src/game/player.cpp` gelesen. Aktuell ausführbar sind
R0/0 und R1/0. Andere Routinen liefern ihre tatsächlichen Originalnamen als
fehlende Bindung. Es gibt keine Ersatzbewegung und keine LeonPreview-Logik.

Die pointerfreie originale `CAMERA`-Wertstruktur ist im transienten nativen
Globalzustand vorhanden. Sie gehört nicht zum serialisierten Save-Tail.
Das bindet noch keine vollständige Schulter- oder Eventkamera.

## Damage-Pool und Kollisionsgrenze

`src/game/dmg.cpp` ist mit seinen originalen Cylinder-/Quad-Predikaten,
Registrierungen, Lebensdauern, Eventabbruch und virtuellen Hit-Aufrufen
angebunden. Native Slots werden nach verzögerter Destruktion erneut typisiert;
der originale `move`-Iterator wird danach auf die aktuelle Slotlebenszeit
aktualisiert. Fremde Referenzen werden vor dem ersten Zugriff zurückgewiesen.
Die `DMG_TYPE`-Felder und virtuellen Signaturen folgen `include/dmg.h`.

Numerische Zusatztests prüfen diese echten Originalkörper. Insbesondere wird
dem originalen Quad-Predikat keine erfundene Höhenprüfung hinzugefügt: Sein
tatsächlicher Code prüft XZ, obwohl der Datensatz ein Höhenfeld besitzt.
Das sind isolierte Damage-Adaptertests, keine Player-Damage-Gameplaytests.

Der konfigurierte Player-Pool verwendet die echte CNS-Actorzahl. Die 20
Damage-Works sind durch `DmgMgr.arrayAlloc(20)` in
`src/game/game.cpp:419` belegt. SAT-Segment-, Floor- und Actor-Scenery-Zugriffe
sind an die bestehende CollisionRuntime vorbereitet. `EmAtCheck` mit der
vollständigen Actor-/Objektliste bleibt ungebunden; der Leon-Start erreicht
die betreffende Stelle noch nicht.

## Gemessener Konstruktionsstand

Auf jeder der vier Plattformen wurden acht Engine-Bootläufe geprüft:
zwei Disc-Zuordnungen, zwei ursprüngliche Boot-/New-Game-Eingaben und zwei
Wiederholungen. Jeder Lauf enthält zehn echte, ausdrücklich isolierte
Konstruktionsversuche mit Ausrüstung `(2,0)`, `(1,0)`, `(3,0)`, `(2,1)`,
`(2,0)`, jeweils zweimal. Das ergibt 80 Versuche je Plattform und 320 insgesamt.

Die Original-Waffenbindung bleibt unverändert: wep01, wep02 und die originale
wep03-Zuordnung zum Mauser-Modul. BIN-/TPL-/FCV-/SEQ-/EFF-Identitäten werden
weiter mit Disc-Hashes und Originaltabellen geprüft.

| Beobachtung beim Abbruch | Ergebnis |
|---|---:|
| Originale pl00-Hierarchie | 119 Gelenke, 8 Modellinfos |
| Originale Motiontabellen | 109 Einträge, 12 registrierte Einträge |
| Originale Hitboxregistrierung | 5 |
| Originale Debugregistrierung | 11; erfolgreiche spätere Live-Callbacks nicht nachgewiesen |
| Originales `init1` | abgeschlossen |
| Eintritt in `startUp` | ja |
| Native virtuelle Leon-Move-Aufrufe | 1 pro Versuch |
| Zurückgekehrte originale Motionaufrufe im Prefix | 1 pro Versuch |
| Primäre Motion / Actor-eigene Hals-Motion | beide referenzgültig initialisiert |
| Idle-SEQ des abschließenden Ruger-Falls | 90 Keys; Index 0 -> 1 |
| Originale Routinen beim Abbruch | `(0,0,1,1)` |
| Vollständiger `move` / vollständiger Player-Tick | nein |
| `startUp` zurückgekehrt / Actor veröffentlicht | nein / nein |
| Rückabwicklung | Modelle, Work, Waffenmodule und Effect-Leases freigegeben |

Der isolierte Test folgt zuvor unabhängigen Reset-/Assetproben. Sein Tickfaktor
ist der Wert 1, der im selben Test tatsächlich durch originale `gameInit()`
gesetzt und gemessen wurde. Er ist ausdrücklich eine Testeingabe nach dem
Reset, kein künstlicher GameTask-Zustandsübergang. Die Runtime respektiert
weiter `pG->Speed`, einschließlich 0. Originale Raumstartposition,
Schulterkamera oder mehrere erfolgreiche Player-Ticks werden dadurch nicht
behauptet. Controllerregressionen prüfen weiterhin die zentrale `PadRead`-
Anbindung; ihre erfolgreiche Verarbeitung durch einen vollständigen Leon-Tick
bleibt offen.

Der gespeicherte Licht-Inversen-Bezugspunkt wird beim originalen `setPos`
vor der Motionausführung erfasst. Eine anschließend bewegte Gelenkposition
darf nicht gegen diesen früheren Lichtcache als identischer Zeitpunkt geprüft
werden. Es wurde keine Beleuchtung korrigiert oder künstlich aufgehellt.

## Tatsächlich ausgeführte Tests

| Umgebung | Ergebnis und Grenze |
|---|---|
| Windows x64 / MSVC, `host-ninja` | Build; 27/27 CTest-Gruppen |
| Windows x86 / MSVC, `host-x86` | Build; 27/27 CTest-Gruppen |
| Originalcode-Generierung | 8 Python-Tests; Wiederholbarkeit und Ablehnung geänderter Originalquellen |
| Originaldaten auf x86/x64 | je 8 Bootberichte; 80 Konstruktorversuche; erwartete Abbrüche und Freigabe geprüft |
| Android NDK 28.2.13676358 | beide ABIs gebaut; Gradle Debug-/Test-APK und Lint erfolgreich |
| Retroid `dbdcacc`, Android 13 / API 33, ARM32 | native Suite zweimal: je 13.386 Prüfungen, 0 Fehler |
| Derselbe Retroid, ARM64 | native Suite zweimal: je 13.387 Prüfungen, 0 Fehler |
| APK ARM32 / ARM64 | je 20 Instrumentationtests bestanden; zentraler Controller, Module und GPU-Clears/Präsentation |
| Originaldaten auf Retroid ARM32 / ARM64 | je 8 Bootberichte, 80 Konstruktorversuche; Originalmodelle, Motion, Ressourcen und Rückabwicklung |
| SAT/EAT, x86/x64 und Retroid beide ABIs | r120 und r300, jeweils zweimal; originale Ray/Floor/Sphere-/Actor-Scenery-Prüfungen, keine vollständigen Leon-Ticks |
| GX-CPU-Oracle x64 | Originalgeometrie, TPL, Material-, Kamera- und OT-Zustände zweimal geprüft |
| Vulkan auf Retroid beide ABIs | bisheriger r120-Diagnosepfad je zweimal; echte Geometrie/Readbacks, identische RGBA-Daten |

Die 32 vollständigen Bootberichte besitzen auf allen vier Plattformen
identische normalisierte semantische Hashes. Native Layoutgrößen und separat
begrenzte numerische Referenzresiduen sind dabei ausdrücklich ausgenommen.
Die eine zusätzliche ARM64-Prüfung der nativen Basissuite ist ABI-bezogen;
sie ist kein zusätzlicher Player-Tick. Assertionzahlen sind keine Framezahlen.

Für reale SAT-/EAT-Abfragen bleiben die gemessenen maximalen absoluten
Referenzabweichungen erhalten: r120 `0,00025`, r300 SAT `0,00332307`,
r300 EAT `0,00641391`. Diese Werte gelten für die isolierten Kollisionsoracles.

Jede Hardwarefolge lief unter `Global\Retroid_ADB_Exclusive`, PowerShell
`System.Threading.Mutex`, Timeout 60 Sekunden und `try/finally`-Freigabe.
Installation, ABI-Prüfung, Start, Ergebnisprüfung, Logs und zugehörige
Bereinigung lagen innerhalb der jeweiligen Sperre. Alle ADB-Aufrufe verwendeten
`-s dbdcacc`. Kein Kill-Server, Neustart oder globales Loglöschen wurde benutzt.

Die finale Test-CLI ist über folgende SHA-256 identifiziert:

```text
host-32       32e6ef1f4b1cffcb81b849a0e5d24d2231dd5d777d4c6a4a9fca1211c105677f
host-64       342af9009228c7a1ff41f66db4f8d84127038c1bfac475d6e60695b98f2a3779
armeabi-v7a   777a6c1328b4ec6c0581ab12aa072ccdff5d63f2751008bf8c0a003e173c2fa7
arm64-v8a     fdfb9b6c3f633a5b694eda92164113d432b32b4367489ee2efde5126266c7947
```

## Vulkan- und Fehlertestbefunde

Die vier finalen Diagnoseaufnahmen haben aktive Khronos-Validation,
**0 Validation-Errors und 1 Performance-Warnung je Bericht**:
`WARNING-Swapchain-PreTransform` (Identity gegen aktuelle 90-Grad-
Surface-Transformation). Es wird keine Warnungsfreiheit behauptet.
Die GPU-Ausgabe umfasst 229.376 von Clear abweichende Pixel und 26/20
ARM32- beziehungsweise 22/29 ARM64-Geometrieframes in den Prüfaufnahmen.
Alle vier RGBA-Readbacks haben SHA-256
`b46d87280a1701af2992f04a55b1aed31a409dbc293313c7cec4eabff02ae3bf`.

Im gefilterten Log des letzten ARM64-Prozesses stehen zusätzlich zwei
`AdrenoUtils`-E-Meldungen `Memory Layout input parameter validation failed!`.
Sie sind keine gemeldeten Khronos-Validation-Errors; die GPU-Nachweise bestehen
trotzdem. Ihre konkrete Treiberursache ist nicht nachgewiesen und bleibt offen.
Dieser Bericht bezeichnet die Treiberlogs daher nicht pauschal als fehlerfrei.

Die Aufnahmen sind Regressionen der bestehenden **isolierten Renderdiagnose**.
Sie sind keine vom vollständigen Actor/GameTask erzeugten Spielframes und
kein neuer visueller Originaltreuenachweis. Screenshots und Rohdaten bleiben
außerhalb von Git unter
`E:\Development\Android\RE4-Data\h1-dispatch-gpu-retroid`.

Vor der finalen erfolgreichen Prüfung gab es folgende korrigierte Befunde:

* falsche private Heap-Headerreferenz und Compilerdiagnosen während der Integration;
* fehlende Fremdreferenzprüfung im nativen Damage-Destruction-Pfad, vom neuen Negativtest erkannt;
* zunächst falsche Testannahme zum SEQ-Fortschritt nach einem isolierten Runtime-Reset.
  Die kurzzeitig angenommene Ein-Key-Erklärung war falsch. Die Disc-SEQ hat
  90 Keys; der Reset hatte den globalen Taktfaktor auf 0 gesetzt. Die Testeingabe
  und unabhängige SEQ-Referenz sind nun ausdrücklich abgegrenzt;
* früherer Lichttest verglich den Cache vor Motion mit einem Gelenk nach Motion;
  die Beobachtungszeit wurde korrigiert, nicht die Originalbeleuchtung.

## Noch fehlende zusammenhängende Verbraucher

Der erste tatsächlich erreichte Blocker ist:

```text
Original consumer unavailable: cPlayer::actionSelect/ActBtn and live interaction owners
```

`src/game/pl_class.cpp` benötigt dort neben den originalen Waffenabfragen
`joyKamae`, `joyLKamae`, `keyReload` und `reloadable` die gemeinsamen
`actWallCheck`, `fanceCheck`, `fallCheck_80172E38`, `jumpCheck`,
`windowCheck`/`ChkWindow`/`EmRackCk` und `cPlPush::catchCheck`.
Die originalen `cEmRack`-/`cEmWindow`-Referenzen, Actioncallbacks und ihre
Lebenszeiten müssen gemeinsam gebunden werden.

`src/game/act_btn.cpp` und `include/act_btn.h` zeigen dafür acht Work-Einträge,
16 reverse-OT-Slots, normale Callbacks, `SceExec` und `SCE_AT`-Callbacks,
Button-/Playerstatusprüfung und Cockpit-/Message-Präsentation. Die 32-Bit-
Pointertags und Integer-Callbackargumente müssen durch native typisierte Links
und referenzgültige Argumente ersetzt werden. Nur eine leere `actionSelect`
oder eine always-false Interaktionsprüfung würde diese Semantik umgehen.

Weitere bereits konkret abgegrenzte Lücken der Updatekette:

* nicht gebundene Bewegungs-, Damage-, Event- und Waffenroutinen der Originaltabellen;
* aktive `setDamage`-/`LifeDownSet2`-Pfade, Weapon-Interrupts und lebende Partnerreferenzen;
* originaler Actor-vs-Actor/-Objekt-Push über `EmAtCheck` und `__em_at_core`;
* `PlClothMoveLeon` / `PenClothMove` einschließlich nativer Collision-Work statt PPC-Locked-Cache;
* Augen-/aktive Morph-Verarbeitung und die originalen Debugverbraucher;
* aktive `seqSeCtrl`-Sound-/Sandanforderungen sowie originale Water-/Effect-Registry;
* später im Leon-Konstruktor die native Foot-Shadow-Tabellenbindung;
* erfolgreiche Actor-Publikation, vollständige Destruktor-/Callback-Lebenszyklen
  und Controllerverarbeitung über mehrere vollständige Player-Ticks.

Die entsprechenden Grenzen werfen ausdrückliche Fehler. Sie sind keine
implementierten Ersatzsysteme. Erfolgreiche Freigabe bisher fehlgeschlagener
Konstruktionen beweist noch keinen erfolgreichen Live-Actor-Lebenszyklus.

## GameTask und H1-Abnahme

Die echte GameTask-Ausführung bleibt bei `Rno0=2` und
`gameRoomInit/model/light/player`. Es wurde kein künstlicher Übergang nach 3
und kein Einstieg in `gameMainLoop` eingeführt. Die neue Actor-Verwaltung ist
in der vorhandenen Player-/Modul-/Ressourcenruntime nutzbar; der vollständige
Originalkörper von `gameRoomInit` ist weiterhin nicht als Consumer gebunden.

`src/game/game.cpp:348` belegt die gemeinsame Abhängigkeitsfolge: Sound-/Heap-
und Datenunit-Setup, `ActBtn.init`, CNS/SMD- und Managerpools, Light-/Shadow-
und Damage-Setup, SAT/EAT und EffInfo, AEV/ITA, Eventmanager, anschließend
`EmMgr.create(0,0)` und Raum-/Block-/Kamera-/Szenarioinitialisierung.
Die Actor-Isolation darf diese Reihenfolge nicht als bereits ausgeführt melden.

| H1-Kriterium | Status |
|---|---|
| Vollständiger originaler cPlLeon erzeugt | nicht erfüllt; Konstruktorbody bricht ab |
| Virtueller Actor-Dispatch | Leon-Move nachgewiesen; gesamter Actor-Dispatch noch unvollständig |
| startUp kehrt erfolgreich zurück | nicht erfüllt |
| Mehrere originale Player-/Motion-/Kollisionsticks | nicht erfüllt; ein Motion-Prefix pro fehlgeschlagenem Start |
| Wiederholte Konstruktion/Freigabe | fehlgeschlagene Versuche sicher geprüft; erfolgreicher Live-Lebenszyklus offen |
| ARM32 und ARM64 | Teilstand tatsächlich geprüft; vollständige H1-Abnahme offen |

Nächstes gemeinsames H1-Paket: Action-/Weapon-/Push-/Window-/Rack-Bindung mit
nativen Callbacklebenszeiten, danach verbleibende Actor-Kollision,
Cloth-/Body-/Eye- und benötigte Effect-/Soundverbraucher im vollständigen
`move`-Pfad. Erst nach erfolgreicher Rückkehr von Konstruktion und `startUp`
dürfen mehrere Originalticks, Live-Retirement und die Einbindung in den echten
`gameRoomInit` als bestanden gemeldet werden. Keine neue Hybridphase wurde begonnen.

## Dateien, Reproduktion und Git

Alle folgenden Dateien sind vollständig im neuen Projekt gespeichert:

| Gruppe | Dateien unter `port/` |
|---|---|
| Native Actorverwaltung | `src/actor_original.h`, `src/actors.cpp`, `tools/generate_actors.py`, `config/actor-sources.json` |
| Native Damageverwaltung | `src/damage_original.h`, `src/damage.cpp`, `tools/generate_damage.py`, `config/damage-sources.json` |
| Originaler Player-Updatepfad | `src/player_live_original.h`, `src/player_live.cpp`, `tools/generate_player_live.py`, `config/player-live-sources.json` |
| Bestehende Player-/Model-/Motionbesitzer | `src/player.cpp`, `src/player_original.h`, `src/object_original.h`, `include/re4/port/player.h`, `include/re4/port/model_types.h`, `src/consumers.cpp`, `include/re4/port/consumers.h`, `tools/generate_player.py` |
| Transiente originale Kamerawerte | `tools/generate_runtime.py`, `config/runtime-sources.json` |
| Build und Tests | `CMakeLists.txt`, `tests/game_runtime_tool.cpp`, `tests/game_runtime_verification.cpp`, `tests/test_runtime_sources.py`, `tools/test_game_runtime.py` |
| Bericht | `docs/hybrid-h1-dispatch-status.md` |

Reproduktion mit vorhandenen, eingerichteten Buildverzeichnissen:

```powershell
cmake --build port/build/host-ninja --parallel 4
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel 4
ctest --test-dir port/build/host-x86 --output-on-failure
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/h1-dispatch-host64 --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
.\port\scripts\build-android.ps1 -SdkRoot C:/Users/andiw/AppData/Local/Android/Sdk -JavaHome 'C:/Program Files/Android/Android Studio 2025/jbr' -Python C:/Users/andiw/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe -ValidationLayers port/build/validation/layers/android-binaries-1.3.290.0 -Offline
.\port\scripts\test-device.ps1 -Adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe -Serial dbdcacc
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/h1-dispatch-retroid --adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe --serial dbdcacc --repeat 2
```

Die konkreten Host-Builds liefen mit dem eingerichteten VS-Developer-
Environment. Auf einem neuen Rechner ist dieses ebenfalls einzurichten.
Hardwaretools enthalten bereits den Mutexbroker; sie dürfen nicht in einen
zusätzlichen prozessfremden Lock mit demselben Namen eingeschlossen werden.

Vollständige private Nachweise: `h1-dispatch-host32`, `h1-dispatch-host64`,
`h1-dispatch-retroid`, `h1-dispatch-collision32`, `h1-dispatch-collision64`,
`h1-dispatch-collision-retroid`, `h1-dispatch-gx64`,
`h1-dispatch-gpu-retroid` unter `E:\Development\Android\RE4-Data`.
Die zugehörigen Build-/CTest-/ADB-Protokolle liegen ignoriert in
`port/build/h1-dispatch-*.log`.

Die Originalquellen, Disc-Images und der alte Port
`E:\Development\Android\RE4\RE4-Android-Decomp` wurden nicht verändert.
Es erfolgten keine Builds oder Git-Schreiboperationen im alten Port.
Keine Original-Assets oder GPU-Aufnahmen werden eingecheckt.

`master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`;
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Dessen aktualisierte README wurde gelesen und berücksichtigt, ohne Merge,
Rebase oder Änderungen an master. Der geprüfte Teilstand wird ausschließlich
lokal auf `android-port` gesichert. Keine Pushes.

Die spätere Vorgabe MSAA Aus/2x/4x, AF 2x/4x/8x, DoF mit Nah-/Zielfokus und
persistenten hardwarevalidierten Einstellungen bleibt erhalten. Es wurden
weder ein neues Grafikmenü noch ein Ersatzrenderer integriert.
