# Hybrid H1: Cloth, Body und Leon-Konstruktion — geprüfter Teilstand

Stand: 12. Oktober 2026. Ausgangspunkt `b772bacc` auf `android-port`.

**H1 ist nicht abgeschlossen.** Die Cloth-/Body-Blockade ist beseitigt.
Die erhaltene Konstruktorsequenz führt `init1()` vollständig aus und betritt
den originalen `cPlayer::startUp()`-Body. Dieser erreicht seinen ersten
`move()`-Aufruf. Vollständiger virtueller Leon-Dispatch, erfolgreicher Abschluss
von `startUp()` und tatsächliche Player-Ticks fehlen weiterhin.

## Tatsächlich ausgeführter Pfad

Der bestehende `PlayerRuntime::constructLeon` führt erhaltene Original-Bodies
auf seinem typisierten Konstruktionseigentümer aus:

```text
cPlayer::cPlayer
  → cPlLeon::cPlLeon
  → init0 / setModel / weaponRelease / weaponLoad / weaponInit
  → init1
      → Neck / LightInfo / atari / Yarare
      → PlClothSetLeon / PenClothSet
      → cPlBody::makeSpaeData
      → native ATTACH_CAMERA-Allokation
  → cPlLeon::setMotion
  → EspDataLoad(PL_ARC_PTR(..., 0x1A), EFF_PL00, 0)
  → cPlayer::startUp
      → be_flag / ang.y / cModel::setPos / cModel::matUpdate
      → Frame / Hokan / setRno(0, 0, 0, 1)
      → move: explizite fehlende Actor-Laufzeit
  → vollständige Rückabwicklung der erworbenen nativen Ressourcen
```

Die Versuche laufen in einem echten Scheduler-Task, **isoliert von
`gameRoomInit`**. `PlayerDomain` bleibt ein Besitzer benannter nativer Felder,
`ActorModelCpu` ein cEm-Konstruktor-/Modellunterbau. Sie sind keine vollständige
native cEm/cPlayer/cPlLeon-Vererbungshierarchie. Es wird kein Actor veröffentlicht.
Die echten virtuellen Waffenklassen Fn57/Ruger/Mauser bleiben erhalten.

`startUp_entered=true` dokumentiert den tatsächlich ausgeführten Einstieg.
Das bisherige JSON-Feld `startUp_executed=false` bedeutet weiterhin **kein
erfolgreich abgeschlossener startUp-Pfad**. `returned_actor=false` und
`em_mgr_dispatch_bound=false` bleiben ausdrücklich bestehen.

Der Einstieg verwendet `pG->pl_pos` und `pG->pl_ang_y` des vorhandenen
Initialisierungstests. Das beweist die Original-Zuweisung, keine vollständige
Raumstartposition oder laufende r100-Szene.

## Originalfunktionen und Adapter

Neue beziehungsweise zusätzlich erreichte Original-Bodies:

| Quelle | Funktion / tatsächlicher Umfang |
| --- | --- |
| `src/game/pl_cloth.cpp:289` | `PlClothSetLeon`, originale Jacket-/Holster-/Hair-Setupfunktionen und Tabellen |
| `src/game/pendulum.cpp:128` | vollständiges `PenClothSet`, Restlängen, Richtungen, Kind-/Elternreferenzen, Nachbarschaftsabstände und Anfangsgeschwindigkeit |
| `src/game/pendulum.cpp:205` | `PenClothFixSet` / `PenClothFixClear`; ausgeführt in ausdrücklich numerischen Adaptertests |
| `src/game/pl_body.cpp:63` | vollständiges `cPlBody::makeSpaeData` |
| `src/game/model.cpp:494` | vollständiges `cModel::setPos` einschließlich aller Parts und Kollisions-Moved-Flag |
| `src/game/model.cpp:1181` | `cModel::matUpdate` mit originalem Koordinaten-, Parts- und Lichtmatrixpfad |
| `src/game/lightInfo.cpp:73` | vollständiges `cLightInfo::updateMatrix` |
| `include/model.h:559` | originale `setRno`-Zuweisungen |
| `src/game/player.cpp:277` | kompletter Body generiert; ausgeführt bis zum ersten, noch fehlenden virtuellen `move` |
| `src/game/cMotBase.cpp:16` | beide `set`-Varianten, `adjust`, `move`; mit echten pl00-FCV-/SEQ-Daten geprüft |
| `src/game/sub2.cpp:114` | originales `Muku2` für den cMotBase-Winkelübergang |
| `src/game/cam_ctrl.cpp:2418` | originale Attach-Camera-Tabelle: Clear, Register, Delete, Model-/Camera-Lookup |

Die Generatoren prüfen Originalquellen per SHA-256, übernehmen ganze Bodies
und protokollieren Namens-, Pointer-, Speicher- und SDK-Anpassungen.
`cloth-manifest.json` entsteht im Buildverzeichnis; keine extrahierten Assets
werden generiert oder versioniert. Der Cloth-Generator enthält sieben Bodies,
der Player-Generator jetzt 68, der Consumer-Generator 126.

Cloth benötigt zur Initialisierung keine separat erfundene Datei: Die
originalen Gelenk-, Nachbarschafts-, Wind- und Kollisionsvolumenlisten stehen
in `pl_cloth.cpp`; ihre Gelenke stammen aus den importierten pl00-BINs.

Native Adapter:

* `ModelPart::Pen` besitzt benannte, typisierte PEN_INFO-Felder. GC-Adressen
  werden weder als native Pointer interpretiert noch in 32 Bit abgeschnitten.
* Jeder Konstruktionseigentümer besitzt drei CLOTH_INFO-Works. Die originale
  Namens-/Aufrufreihenfolge ist absichtlich unverändert: `leonHair` ist hier
  der Jacket-Work, `leonJacket` der Holster-Work, `leonHolster` der Hair-Work.
  Die Kostümbedingung stammt aus `include/player.h:321`.
* Gelenk- und Kollisionsvolumenindizes werden vor dem originalen Pendelloop
  geprüft. Die bestehende skalare SDK-Matrix-/Vektorarithmetik wird verwendet.
  Die ursprünglichen vorzeichenverschiedenen Loop-Typen bleiben im Body;
  ausschließlich diese generierte Quelle erhält eine entsprechende Warnungsfreigabe.
* `makeSpaeData` erstellt originale native Records mit Header 257/2,
  Shape-IDs 0/1, Offsets 0x18/0x38 und komplementären Keys 1→0 / 0→1 über
  256 Frames. Dies ist Initialisierung, keine bereits laufende Morphsimulation.
* Attach-Camera-Speicher kommt aus Heap 13 mit **`sizeof(MotionCamera)`**;
  es gibt keinen ARM64-Overlay über die ursprünglichen 0x98 Bytes.
* Modellinitialisierung bindet die Attach-Camera-Tabelle des jeweiligen
  `EngineConsumers`. Modellfreigabe entfernt ihre Referenzen. Ein allokierter
  Camera-Work mit `type=0` folgt dem originalen inaktiven Motionpfad.
  Aktive Kamerapräsentation wird weiterhin vor `MotionMove` explizit abgelehnt.
* Konstruktorabbruch gleicht genau die neu erworbenen EFF_PL00-Referenzen
  mit originalem Count-Check aus, zusätzlich zu den bisherigen Waffenressourcen.
  Native Kamera-, Tabellen-, Modell- und Subwork-Besitzer werden freigegeben.

## Originaldaten-Nachweise

Quelle bleibt `E:\Development\Android\RE4-Data\import-phase3a`, beide
G4BE08-Debug-Discs. Prüfergebnisse liegen ausschließlich außerhalb von Git.

Jeder Start-CLI-Test führt zehn echte Konstruktorversuche aus: Ausrüstung
`(2,0), (1,0), (3,0), (2,1), (2,0)`, jeweils zweimal. Das sind ausdrücklich
isolierte Ausrüstungseingaben. Die Auswahl, Ressourcen, Modelle und
Motionreferenzen bestimmt der Originalcode.

Alle vier Plattformen führen acht Start-CLI-Läufe aus: beide Discs, normaler
und New-Game-Initialisierungszweig, zwei Wiederholungen. Insgesamt wurden
320 Konstruktorversuche bis zum dokumentierten Move-Blocker geprüft.

Nachgewiesen sind:

* Originales pl00-Modell mit 119 Gelenken und acht Modell-/Materialanbindungen.
* Kostüm 0: 24 Jacket- und 21 Hair-Links, sechs plus vier originale
  Kollisionsvolumendefinitionen. Der Holster-Work bleibt gemäß Originalbedingung
  leer. **Der Kostüm-1-Holster-Zweig wurde in diesen Realdateifällen nicht ausgeführt.**
* Längen, Kindpositionen, Offsets, Elternvererbung, normalisierte Richtungen,
  Nachbarabstände, Motionflags und Null-Anfangsgeschwindigkeit.
* Beide vollständigen Body-SPAE-Records, originale LightInfo-Matrix und
  originale startUp-Routinenzuweisung vor dem fehlenden Move.
* pl00-Effektquelle aus Archivslot 0x1A mit Referenzhash und ausgeglichenem Ladezähler.
* Unveränderte wep01/wep02-Bindung, wep03-Daten mit Modul 4, virtuelle
  Waffeninitialisierung, Hand-Parent 10, Motion-/BIN-/TPL-Referenzhashes.
* Freigabe ohne Heap-/Modell-/Objektverlust und erneute Konstruktion.
* Sieben originale pl00-FCV-/SEQ-Paare: bestehende MotionMove-/IK-Ticks und
  zusätzlich vier cMotBase-Blendticks pro Paar und Modellinitialisierung.
  Diese sind **Motion-Consumer-Ticks, keine vollständigen Player-Ticks**.

Numerischer Vergleich mit unabhängiger skalarer Referenz auf allen vier ABIs:

| Messung | Größte gemessene absolute Abweichung |
| --- | --- |
| Pendel-Kindlängen | 0 |
| Normalenkomponenten | 5.96046448e-8 |

Das ist kein Nachweis bitidentischer PowerPC-Paired-Single-Ausführung.
Semantische Referenzen und Zustandsübergänge stimmen über alle ABIs und
Wiederholungen überein. Geringe numerische Referenzresiduen werden separat
begrenzt und aus dem semantischen Vergleichshash entfernt.

## Tatsächlich ausgeführte Builds und Tests

| Umgebung / Prüfung | Ergebnis |
| --- | --- |
| Windows MSVC x86 und x64 | Builds erfolgreich; je 27/27 CTest-Prüfungen |
| Android NDK 28.2.13676358 | armeabi-v7a und arm64-v8a erfolgreich gebaut |
| Debug-/Test-APK, Android Lint | erfolgreich; zusätzlich APK mit vorhandenen Khronos-Layers 1.3.290.0 gebaut |
| Originaldaten-Startprüfung | je Architektur acht CLI-Läufe; 392195 Checks normal, 392204 im New-Game-Zweig; keine Fehler |
| Zusätzliche Modelle / Events / Licht | beide Discs und Wiederholungen, r120/r100-Eventdaten und r317s15-Modelle, bestehende Raum-/Lichtprüfungen bestanden |
| SAT/EAT | x86/x64 und beide Retroid-ABIs, r100/r102/r119/r120/r300, zwei Wiederholungen, bestehender unabhängiger Triangle-Oracle bestanden |
| Retroid native Basistests | ARM32 zweimal 13386, ARM64 zweimal 13387 Checks, jeweils null Fehler |
| Retroid APK-Instrumentation | je ABI 20/20 Tests, einschließlich Controller, Runtime und Vulkan |
| GX-CPU-Regression | x86/x64, Original-SMD-/Primitive-/TPL-/Material-/Kamera-Oracle bestanden |
| Echte Vulkan-Regression | je Android-ABI zwei GPU-Läufe, aktive Validation, null Fehler; Readbacks bytegleich |

Gerät: **Retroid Pocket Flip2**, Serial `dbdcacc`, Android 13/API 33.
Gemeldete ABIs: arm64-v8a, armeabi-v7a, armeabi. Die geprüften Prozesse
haben tatsächlich 64 beziehungsweise 32 Bit Pointerbreite.

Die unterschiedliche Basistestzahl stammt weiterhin vom zusätzlichen
64-Bit-Pointer-Nachweis oberhalb UINT32_MAX. Die Originaldaten-Startprüfzahlen
sind für dieselbe Eingabe auf allen vier Plattformen gleich. Die neun
zusätzlichen New-Game-Checks stammen aus dem Inventarzweig.

Hardwaresequenzen verwenden unverändert `Global\Retroid_ADB_Exclusive`,
PowerShell/System.Threading.Mutex, 60000 ms Timeout und try/finally-Freigabe.
Python-Prüfer verwenden den bestehenden PowerShell-Mutexbroker und prüfen
vor jedem ADB-Aufruf dessen Besitz sowie `-s dbdcacc`. Es gab keine globalen
ADB-/Geräteoperationen. Jede Installation, Ausführung, Prüfung und zugehörige
Logerfassung war durch die jeweilige vollständige Testsequenz geschützt.

Vulkan meldet pro GPU-Aufnahme die bestehende Performancewarnung
`WARNING-Swapchain-PreTransform` / MessageID `0x9e11f9f9`: IDENTITY statt
aktueller Surface-Transformation ROTATE_90. Keine Validationfehler.
Die Warnung wurde aus dem RE4-Log unter erneut erworbenem Mutex verifiziert.
Sie wurde in diesem Cloth-Auftrag nicht durch einen Rendererumbau verändert.

Die neuen GPU-Aufnahmen sind die **bestehende isolierte r120-Renderdiagnose**,
keine Cloth-Simulation, kein konstruierter Leon-Actor und kein GameTask-Spielframe.
ARM32 meldet 66/30, ARM64 13/16 Geometrieframes; diese zeitabhängigen Zähler
zeigen mindestens zwölf wiederholte Frames je Lauf. Alle vier erfassten
512×448-Readbacks sind identisch und enthalten 229376 Nicht-Clear-Pixel.

Private Ergebnisse:

* `E:\Development\Android\RE4-Data\h1-cloth-host64`
* `E:\Development\Android\RE4-Data\h1-cloth-host32`
* `E:\Development\Android\RE4-Data\h1-cloth-retroid`
* `E:\Development\Android\RE4-Data\h1-cloth-collision-{host64,host32,retroid}`
* `E:\Development\Android\RE4-Data\h1-cloth-gx-{host64,host32,retroid}`

GPU-Dateien: `h1-cloth-gx-retroid/gpu-{armeabi-v7a,arm64-v8a}-{1,2}.png`
mit gleichnamigen JSON-/RGBA-Dateien und zusätzlichen Display-Aufnahmen.
Build-, CTest-, App- und Geräteprotokolle: `port/build/h1-cloth-*`.
Lock-Journale: `port/build/adb-coordination/`. Keine dieser Dateien ist Git-Inhalt.

Reproduktion: Hostbuilds in einer passenden MSVC-x64- beziehungsweise
x86-Developer-Shell mit `port/scripts/test-host.ps1 -BuildName host-ninja`
beziehungsweise `-BuildName host-x86`. Python muss für CMake verfügbar sein.
Die bereits konfigurierten Builds lassen sich mit `cmake --build` und
`ctest --test-dir port/build/<BuildName> --output-on-failure` wiederholen.

Die Originaldatenprüfung verwendet diesen vorhandenen gemeinsamen Runner:

```powershell
& $taskPython port/tools/test_game_runtime.py `
  --data E:\Development\Android\RE4-Data\import-phase3a `
  --output E:\Development\Android\RE4-Data\h1-cloth-host64 `
  --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
```

`$taskPython` war
`C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe`.
Für x86 wird das x86-Executable und ein separates Ausgabeverzeichnis verwendet.
Für Retroid ersetzt `--adb C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe
--serial dbdcacc` das Hostargument; der Runner erwirbt selbst den Mutex.
Keinen zweiten äußeren Mutex um einen solchen brokerbasierten Runner halten.

Android: `port/scripts/build-android.ps1` mit dem vorhandenen SDK, JBR,
Python und `-Offline`; für die Validation-APK zusätzlich
`-ValidationLayers E:\Development\Android\RE4-Fixed\port\build\validation\layers\android-binaries-1.3.290.0`.
Danach `test-device.ps1 -Repeat 2` und `test-app-device.ps1`, jeweils mit
`-Adb <SDK-adb.exe> -Serial dbdcacc`. `test_collision.py` und
`test_gx_scene.py` verwenden dieselben Daten-/Ausgabe-/Host- oder ADB-Argumente
und `--repeat 2`; alle Hardware-Runner sperren selbst exklusiv.

Anfängliche reale Fehlprüfungen wurden behoben: Original-u32/int-Loopwarnung,
gesicherte Generator-Occurrences, alter Consumer-Funktionszähler 118 statt 126
und der bisherige pauschale MotionSetCore-Fehler für jede allokierte Kamera.
Danach wurden die betroffenen vollständigen Prüfungen erfolgreich wiederholt.

## Exakte verbleibende H1-Abhängigkeiten

Der nächste tatsächliche Fehler ist:

```text
Original consumer unavailable: cPlayer::move/live virtual actor, DmgMgr,
actionSelect, EmAtCheck and camera owners
```

Dies ist keine fehlende GPU-Cloth-Funktion als Ersatz für den Actor. Erforderlich
ist die zusammenhängende native Original-Actor-Laufzeit:

1. **Actor-Pool und Dispatch:** `src/game/em.cpp:76::cEmMgr::construct`, die
   Original-cManager-Erstellung, typisierte cEm/cPlayer/cPlLeon-Klassen,
   GUIDs, Work-/Statusfelder und native Objektgrößen statt GC-Stride 0xDE0.
   Keine Casts zwischen `PlayerDomain` und `cPlayer` oder Einbau von LeonPreview.
2. **Player-Move-Kette:** `src/game/player.cpp:294` braucht DmgMgr/ShapeMove,
   Binocular-/Partnerzustände, originale Routinentabellen, Neck/Eye/Shadow,
   EmAtCheck, WeaponMove, ClothMove, Sound-/Wasser-/Kameraconsumer. Schon Idle
   (`pl_R1_Footwork`, Zeile 405) ruft Motionauswahl, Neckinit, actionSelect und
   checkCtrl auf. `pl_class.cpp:547::actionSelect` braucht originale Push-,
   Wand-, Fenster-, Sprung- und ActBtn-Bezüge. Diese wurden nicht umgangen.
3. **Cloth-Simulation:** `PenClothMove` (225), `PenClothMove3` (823) und
   `penClothAtMake` (1144) brauchen PenAtWork, Originalwind, Körpervolumen,
   Nachbarschaft und Kollisionsmathematik. Der GC-Locked-Cache-Pointer
   `0xE0000000` muss durch einen echten nativen Work-Besitzer ersetzt werden.
   Setup und Pinverwaltung behaupten weder Simulation noch GPU-Cloth.
4. **Weitere Darstellung/Animation:** tatsächliche Morphwiedergabe,
   `seqSeCtrl`, Fußschatten-Consumer (`pl_fs_tbl` nach startUp), aktive
   Attach-Camera-Auswahl/Präsentation und QuasiFPS-Actor-Kamera bleiben offen.
5. **gameRoomInit:** `src/game/game.cpp:354` enthält weiterhin die komplette
   originale Manager-, Sound-, Heap-, Ressourcen-, SAT/EAT-, Effect-,
   Scenario-/Event-Initialisierung. `port/src/runtime_original.h:320` bindet
   diesen Body noch nicht. Realer GameTask bleibt bei **Rno0=2** mit
   `gameRoomInit/model/light/player`; kein künstlicher Übergang nach Rno0=3.

Der nächste Integrationsschritt innerhalb H1 ist daher die echte native
EmMgr-/cPlayer-/cPlLeon-Publikation zusammen mit dem vollständigen Idle-Move-
Consumerpfad. Erst dessen funktionierende Besitzer erlauben den Abschluss
von startUp, mehrere echte PadRead-/Player-/Motion-/Kollisionsticks und die
anschließende Einbindung in die unveränderte Room-Initialisierungsreihenfolge.

## Vollständig gespeicherte Projektdateien

Neu: `port/config/cloth-sources.json`, `port/src/cloth.cpp`,
`port/src/cloth_original.h`, `port/tools/generate_cloth.py` und dieser Bericht.

Geändert:

* `port/CMakeLists.txt`, `port/config/player-sources.json`
* `port/include/re4/port/consumers.h`, `model_types.h`, `motion.h`, `player.h`
* `port/src/consumers.cpp`, `model_original.h`, `model_runtime.cpp`,
  `motion.cpp`, `motion_original.h`, `object_original.h`, `player.cpp`, `player_original.h`
* `port/tests/game_runtime_tool.cpp`, `game_runtime_verification.cpp`,
  `test_consumer_sources.py`, `test_runtime_sources.py`
* `port/tools/generate_consumers.py`, `generate_objects.py`,
  `generate_player.py`, `test_game_runtime.py`

Der Spenderport wurde nur gelesen. Keine Builds oder Schreiboperationen dort.
Originalquellen, importierte Daten und `master` sind unverändert. README
`origin/master` bei `15cf64f80e08bbc64712c39be330b75d1512a0d0` wurde gelesen;
kein Merge/Rebase/Push. `master` bleibt bei
`9b76ff003081d6ece8b838453fb8310743a972c7`.
Controller und Vulkan bleiben erhalten; MSAA Aus/2×/4×, AF 2×/4×/8×,
DoF mit Nah-/Zielfokus, Persistenz und Hardwarevalidierung bleiben verbindlich.
Keine nächste Hybridphase wurde begonnen.
