# Phase 5b: GameTask, Stage und aggregierter Save/Load

Geprüfter Teilstand vom 11.10.2026, ausgehend von
`7a978abbfc6b5db51b3f4e75cf362827d2336cc0` auf `android-port`.

**Die zusammenhängende Spielszene ist noch nicht erreicht.** Der originale
GameTask führt jetzt `gameInit` und `gameStageInit` aus und erreicht selbst
`Rno0=2`. Der Originalkörper von `gameRoomInit` ist noch vollständig unportiert.
Sein explizit fehlschlagender Adapter verhindert die weitere Ausführung.
`gameMainLoop`, ein vollständiger Leon-Actor, die lebende Schulterkamera und
`Event::Run` werden dadurch weiterhin nicht ausgeführt. Es wurden keine
Raumzustände, Events oder fehlenden Manager mit Erfolgs-Stubs überbrückt.

Die neuen CPU-Nachweise laufen auf Windows x86/x64 und Retroid ARM32/ARM64.
Die GPU-Bilder dieses Teilstands sind erneut geprüfte **Renderdiagnosen**,
keine Bilder aus einer laufenden Spielsimulation. Der Umfang der noch offenen
Actor-/Room-/Eventintegration gehört weiterhin zu Phase 5b.

## Tatsächlich ausgeführte Originalfunktionen

Die Generatoren übernehmen vollständige geprüfte Funktionskörper aus den
Originaldateien. SHA-256-Quellfingerprints, eindeutige Ersetzungsstellen und
`#line`-Zuordnungen bleiben erhalten. `src/` und `include/` wurden nicht verändert.

| Originalquelle | Neu angebundener Körper | Nachweis und Grenze |
| --- | --- | --- |
| `src/game/game.cpp:292` | `gameStageInit` | Normale englische Startpfade vollständig; originale Discentscheidung, Zeit, Save, StageSet und `Rno0=2`. Kostüm-Auswahldialog mit `EXT_COSTUME` bleibt eine explizite Message-Abhängigkeit. |
| `src/game/game.cpp:858` | `cGameSave::save` | Tatsächliche globale, Room-, SubScreen-, Merchant- und Item-Sektionen. Der `Rno0=3`-Zweig benötigt den lebenden Spieler und schlägt vor einer Mutation fehl. |
| `src/game/game.cpp:824` | `cGameSave::load` | Tatsächlicher gewöhnlicher Restore mit denselben Managern und originalem `PlSetCostume`. New-Round (`SaveKind=3`) wird vor Restore explizit zurückgewiesen. |
| `src/game/game.cpp:735` | `GameLoad` | Separater Restore-Test mit originalen Ladeflags, `RoomNo_next`, `Part_next` und `pl_flag`; kein gestarteter Raum dadurch simuliert. |
| `src/game/game.cpp:1320`, `:1368` | `GamePointInit`, `GameAddPoint` | Der Original-New-Game-Zweig berechnet `point=5500`, `Game_level=5`. |
| `src/game/sce_at.cpp:3607` | `SceAtInitSaveItem` | Originales Löschen der gespeicherten Itemzustände; noch keine Erzeugung von Item-Actors. |
| `src/game/mes.cpp:454`, `:485` | `MessageControl::stageInit`, `loadStageFont` | Originale Dateigrößenprüfung und Stage-Font-Allokation. Englisch lädt im Original keinen japanischen Stage-Font: diese Bedingung bleibt erhalten. |
| `src/game/item.cpp:1196`, `:261` | `cItemMgr::gameInit`, `set_game` | Vollständiger normaler Leon-New-Game-Inventarpfad aus dem Original; Ada/Extra-Spielzweige bleiben explizit ungebunden. |
| `src/game/item.cpp:2866`, `:2916` | `cItemMgr::save`, `load` | Native Original-Itemdatensätze und Original-Reihenfolge; PPC-Registerbindung entfernt, Algorithmus erhalten. |
| `src/game/item.cpp`, `include/item.h` | `arm`, `weaponId(cItem*)`, beide `dump`, `erase`, `num`, `bareHand`, `itemCombine` | Abhängigkeiten des ursprünglichen Inventarpfads mit Originaltabellen; keine neue Waffen- oder Bewegungslogik. |
| `src/game/sscrn.cpp:60`, `:66` | `SscrnDataSave`, `SscrnDataLoad` | Originaler persistenter SubScreen-Wert. |
| `src/game/merchant.cpp:945`, `:956` | `MerchantDataSave`, `MerchantDataLoad` | Originaler Merchant-Datenrestore einschließlich Friendship; weiterhin kein Shop-Actor. |
| `src/game/pl_sub.cpp:66` | `PlSetCostume` | Originale skalare Auswahl im New-Game-/Load-Pfad; kein Ersatz für `cPlLeon`-Konstruktion. |

Der originale New-Game-Inventarpfad erzeugt in Originalreihenfolge die IDs
`0x7c, 0x23, 4, 5, 0xac, 0xad`: Case, Handgun, Munition, Spray und Dokumente.
Tests prüfen den bewaffneten Slot 1, 10 Handgun-Patronen, 20 Reservepatronen
und die vom Original gesetzten Case-Koordinaten `(2,1)`.
Diese Daten werden durch `set_game` erzeugt, nicht durch einen Testadapter.

Die CLI hat zwei klar abgegrenzte Starteingaben: der bisherige Defaultpfad und
ein ausdrücklich gesetztes `SYS_NEW_GAME`-Eingabeflag. Das Flag ersetzt weder
`Title_task` noch dessen Menü. Sämtliche daraus folgenden Inventar-, Schwierigkeits-,
Save- und Stagezustände berechnen die Originalkörper. Der verwendete
`CpuPresentationProbe` bleibt ein isolierter VI-Beobachter, ohne GPU-Spielausgabe.

## Native Daten- und Speicheradapter

- `GameSaveBlock` enthält jetzt den vollständigen pointerfreien Save-Bereich
  von `GLOBAL_WK`, einschließlich unbekannter Paddingbytes und bisher unbenutzter
  Flags. Alle ABIs prüfen `sizeof==0x36F8`, `room_id`-Offset `0x1c` und
  `SaveKind`-Offset `0x33d8`. Die beiden BE-Room/Stage-Unions sind native
  `u16`-IDs; Stagewerte werden durch Shift berechnet. Die originalen rohen
  Gegner-/Item-Save-Workbytes bleiben unverändert erhalten.
- `NativeGameState` besitzt diesen Save-Wert als Basissubobjekt. Originales
  Save/Load verwendet dieses typisierte Subobjekt, keinen Cast vom alten
  GameCube-Adresslayout und keine abgeschnittenen Pointer.
- `SAVE_DATA_HEAD` enthält echte native Pointer: 28 Bytes auf x86/ARM32 und
  56 Bytes auf x64/ARM64; seine erste Sektion bleibt bei Offset `0x40`.
  `ID_UNIT` hat 312 beziehungsweise 344 Bytes. Diese Unterschiede sind erwartete
  native Layouts, keine überlagerbaren GameCube-Strukturen.
- Der native Save ist ein **interner Speicherwert**, kein GameCube-Cardformat
  und kein ABI-übergreifend serialisiertes Dateiformat. Die bestehenden originalen
  Sectionoffsets, Owner-Leases und Generationen bleiben geprüft. Externe Card-I/O,
  Checksummen und Dateipersistenz werden damit nicht als implementiert behauptet.
- Save/Load bindet für die Dauer des Originalaufrufs die tatsächlichen Room-,
  SubScreen-, Merchant- und Itemmanager. Fremde globale Besitzer, ungültige
  Arm-Slots/Item-IDs, Sectiongrenzen und veraltete Handles werden zurückgewiesen.
  Scoped Cleanup entfernt Bindungen auch bei Ausnahmen und bei fehlgeschlagenem
  Callback-Aufbau. Offsetförmige Images liefern wie im Original `false`.
- Stage-Font-Adressen gehören dem Message-Runtime-Besitzer. Überzählige
  Stageinitialisierung kann kein noch referenziertes Fontbuffer überschreiben.
  Der native Stage-Heapwechsel retiert alte Slots/Allokationen nach geprüftem
  Read-Abschluss; externe Leases verhindern weiterhin unsichere Heapwiederverwendung.
  Vollständiger GameRuntime-Reset gibt die Manager erneut in ihrer bestehenden
  Reihenfolge frei.
- `Dvd.GetDiscNo`/`DiscChange` wird auf die beiden unveränderlichen importierten
  Disc-Kataloge abgebildet. Aktive oder queued Reads verhindern einen Wechsel;
  bestehende Ressourcen behalten ihre Discidentität. Bei Boot von Disc 2 schaltet
  das originale `gameStageInit` für Stage 1 tatsächlich auf Discindex 0 zurück.
  Der Adapter simuliert kein Disc-Lid oder GameCube-Discwechselmenü.

Die Original-Stage-Fontinitialisierung fragt selbst bei Englisch Größen von
`Font/stageN_j.fnt` und `Font/eventN_j.fnt` ab. Diese Dateien bleiben als tatsächliche
Abhängigkeiten im privaten Testkatalog. `loadStageFont` lädt sie nur bei
Originalsprache 0; Englisch bleibt Sprache 1 mit den bestehenden englischen
Common-/System-Fonts. Die 35 unvollständigen englischen MDT-Blöcke werden weder
ersetzt noch umgeschrieben.

## Reproduzierbare Testergebnisse

Alle angegebenen Ergebnisse wurden in diesem Auftrag erneut ausgeführt.

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | --- | --- | --- | --- |
| CMake-Build / Host-CTest | 27/27 bestanden | 27/27 bestanden | Dual-ABI-NDK-Build bestanden | Dual-ABI-NDK-Build bestanden |
| GameTask-Starts, 2 Discs × 2 Eingaben × 2 Runden | 8 | 8 | 8 | 8 |
| Originales `gameStageInit` bis Rno0=2 | 8/8 | 8/8 | 8/8 | 8/8 |
| Originales New-Game-Inventar | 4/4 | 4/4 | 4/4 | 4/4 |
| Checks je Default-/New-Game-Boot | 350210 / 350219 | 350210 / 350219 | 350210 / 350219 | 350210 / 350219 |
| Bestehender isolierter Scheduler-/Runtime-Consumer-Test | 1073 Checks/Lauf | 1073 Checks/Lauf | 1073 Checks/Lauf | 1073 Checks/Lauf |
| Originale SAT-/EAT-Abfragen, 2 Runden | 13930 | 13930 | 13930 | 13930 |
| Weight-/FCV-/SEQ-Regressionsprüfungen, 2 Runden | 1739820 + 617 Adapterchecks | gleich | gleich | gleich |
| Ressourcen pro Disc und Runde | 157 / 151 | gleich | gleich | gleich |
| Ressourcen-/Raumfälle pro Disc und Runde | 85 / 57 | gleich | gleich | gleich |
| Geprüfte RELs pro Disc und Runde | 110 / 4 | gleich | gleich | gleich |
| Android-Core, je 2 Runden | Host-CTest | Host-CTest | 13386 Checks/Lauf, 0 Fehler | 13387 Checks/Lauf, 0 Fehler |
| APK-Instrumentierung inkl. Controller/Surface/Vulkan | keine Windows-APK | keine Windows-APK | 20/20 | 20/20 |

Das zusätzliche ARM64-Corecheck prüft einen Pointer oberhalb von 32 Bit.
Die Scheduler-/Background-/Yz2-Traces stimmen über beide ABIs und Wiederholungen
überein. Die automatische Zusammenfassung vergleicht alle vier Architekturen,
getrennt nach Default- und New-Game-Eingabe, einschließlich Originalzuständen
und Tracehashes. Es gibt keinen abschließenden unerwarteten Testfehler.

Der Retroid meldet Modell `Retroid Pocket Flip2`, Android 13/API 33,
Serial `dbdcacc`, ABIs `arm64-v8a, armeabi-v7a, armeabi`. ARM32- und ARM64-ELFs
liefen als tatsächliche Geräteprozesse. Die APK wurde mit `install --abi`
jeweils explizit installiert; Instrumentierung prüft auch die Prozesspointergröße.
Die vorhandene zentrale `PadRead`-Anbindung bleibt erhalten. Die Controllertests
dieses Auftrags sind automatisierte Adapter-/APK-Regressionen; es wurde keine
erneute physische Tastenkalibrierung behauptet.

Die numerischen Regressionen ergeben weiterhin:
SAT-Schnittpunktfehler zur unabhängigen Double-Referenz maximal `0.015125`,
PSQ-Quantisierung exakt gleich, Weight-Linearfehler maximal `1.19e-7`,
Weight-Translationsfehler maximal `0.011290427`, FCV-Positionsfehler maximal
`0.004801790`. ARM32-/ARM64-Skinning-Ergebnisse sind bytegleich. Das sind
Algorithmus-/Ressourcentests, keine Nachweise vollständiger Actor- oder Gameplayausführung.

Die bestehenden Ressourcen-/Modul-/Modelltests bleiben teilweise isoliert:
180 `modelInit`-Aufrufe je ABI für Event-/weitere EVD-Pakete, zusätzliche SubScreen-
und Leon-Modelle sowie 20 Room/ESL-Vorbereitungen je ABI. Vollständige REL-
Gameplayausführung wird aus dem erfolgreichen REL-Parser nicht abgeleitet.

Während der Entwicklung aufgetretene und korrigierte Fehler:

1. Ein Generator-Regressionsassert erwartete noch die alten Funktionszahlen.
   Die neuen geprüften Körper ergeben 33 ID/Save-, 40 SubScreen- und
   35 Message-Körper; die Quelle bleibt fingerprintgeschützt.
2. Der erste echte New-Game-Lauf scheiterte am nur für `Em_list` begrenzten
   `memclr_asm`. Der Adapter erlaubt jetzt zusätzlich ausschließlich den
   tatsächlichen `item_save`-Bereich mit exakt passender Größe.
3. Der erste Gesamthashvergleich vermischte zwei absichtlich unterschiedliche
   Starteingaben. Der Vergleich prüft jetzt jede Eingabe über sämtliche ABIs,
   Discs und Wiederholungen separat; die vollständigen Läufe wurden wiederholt.
4. Ein Host-Regressionsaufruf nannte ein nicht vorhandenes Kollisions-Executable.
   Der reale vorhandene Einstieg ist `re4_game_tool collision`; alle betroffenen
   Läufe wurden damit erneut vollständig ausgeführt.

## GPU-Regression und Helligkeit

Private echte GPU-Aufnahmen liegen unter
`E:\Development\Android\RE4-Data\phase5b-scene-gpu-r100`:

- `gpu-armeabi-v7a-1.png`, `gpu-armeabi-v7a-2.png`
- `gpu-arm64-v8a-1.png`, `gpu-arm64-v8a-2.png`
- Entsprechende `.json`, `.rgba` und `display-*.png` als Nachweise.

Die Aufnahme verwendet weiterhin den **isolierten Textur-Debugpfad**:
Raum r100, Original-EVD `r100s40`, Cut 0, Kamera-/Motionframe 30, Modell `pl0000`,
`debug-view=1`. Die View-Matrix stammt aus
`event/r100/s40/cam/cam_s40_000.fcv`. Dies ist weder die r100-Startkamera noch
ein von GameTask erzeugter aktueller Eventframe.

Alle vier Readbacks sind bytegleich:
`7c2676d8361694ceb5f9db106b8c3d8bec3126caa032b96be519b4ceb1cf5cd6`.
512×448 Pixel, 317 Modelle, 372702 Vertices, 347330 Dreiecke, 504 Draws,
510 Texturen, 226966 Nicht-Clear-Pixel. Je Wiederholung wurden 12–22
Geometrieframes mit Activity-/Surface-Neuerstellung überprüft.
Die Bildprüfung zeigt die bisherige Brücke mit Leon am linken Bildrand;
ein Auto oder eine vollständig gestartete Originalsequenz ist damit nicht belegt.
Die PNG-Ausgabe wurde angesehen, ohne sie als passenden GameCube-Referenzframe
zu verwenden.

Encoded-RGB8-Helligkeit, ausdrücklich keine photometrische Messung:
Mittelwert `49.616669`, Median `48.1352`, P05 `9.0722`, P95 `92.2848`,
Anteil unter 16 `0.088109`. Die Werte sind zum vorigen identischen Debugframe
unverändert. Es wurde kein globaler Helligkeitsfaktor, keine neue Lichtquelle
und keine Gamma-/Shaderkorrektur eingeführt. Der Textur-Debugpfad belegt nicht
die korrekte normale Ambient-/Diffuse-/Specular-/Fog-Ausgabe.

Die normalen r100-Lichtpfade brauchen weiterhin den echten Player-Besitzer
für modellgebundene LIT-Parentreferenzen (ParentType 1, ID 0), lebende SAR-
Zuordnungen und Original-Eventupdates. Dieser Teilstand behebt die dunkle
normale Szene deshalb **nicht**. Ein passender GameCube-Gegenframe mit
identischem Spiel-, Kamera- und Animationszustand steht weiterhin aus.

Khronos-Validation ist in der APK und in allen vier GPU-Läufen aktiviert:
0 Fehler, jeweils 1 Performancewarnung `WARNING-Swapchain-PreTransform`.
Die Swapchain fordert IDENTITY, das Display meldet ROTATE_90; der Presentation-
Engine-Transform bleibt gültig, verursacht aber die dokumentierte Warnung.
Die GPU läuft als Adreno 650, Vulkan 1.1.128. Auch Deskriptor-/Lifetime-/Sync-
Fehler wurden bei den tatsächlich ausgeführten Pfaden nicht gemeldet.

Gemessene letzte CPU-Framezeiten: ARM32 `47.935/90.033 ms`, ARM64
`37.887/37.443 ms`. Zeit bis zum Test-Readback: ARM32 `13.078/11.140 s`,
ARM64 `9.375/9.875 s`. Das sind einzelne Diagnosemessungen einschließlich
Test-/Upload-/Pollingkosten, kein FPS-Benchmark einer laufenden Spielsimulation.

r100-Material-/OT-/Texturorakel wurden auch auf beiden Hosts wiederholt.
Zusätzlich lief der allgemeine originale SMD-/Blockpfad für r102 und r119:
r102 27569 Vertices/76947 Indizes, r119 164757 Vertices/411285 Indizes,
Erstellung und Freigabe je zweimal pro Host, mit identischer Wiederholung.
Neue Gerätescreenshots aus `gameMainLoop` existieren ausdrücklich nicht.

## Exakte verbleibende Abhängigkeiten

Der aktuelle Fehler lautet
`Original consumer unavailable: gameRoomInit/model/light/player`.
Er stammt aus `port/src/runtime_original.h`, **vor** dem noch unportierten
Originalkörper `src/game/game.cpp:354`. Er ist kein nachgewiesener Abbruch
innerhalb einer schon laufenden originalen Rauminitialisierung.

Der Originalkörper benötigt gemeinsam und in Originalreihenfolge:

1. `SndReadAddrInit`, `gameRoomMemInit`, Player-/Area-Lader und Room-DC-Zustand;
   Room-CNS-basierte native Managerpools einschließlich Actor/Obj/Ctrl/Esp/Dmg.
   Ein GC-Pool mit festen 32-Bit-Workgrößen kann native vtables/Pointer nicht
   korrekt aufnehmen. Native Typen und Destruktorpfade müssen vollständig sein.
2. `cEmMgr::construct` (`src/game/em.cpp:76`) → `cPlLeon`-Konstruktor
   (`pl_leon.cpp:26`) → `cPlayer` (`player.cpp:202`), Leon-init0/init1,
   Weapon-/Equipment-/Workbesitzer und `startUp` (`player.cpp:277`).
   Bestehendes `PlayerInit` und isoliertes `setModel/setMotion` ersetzen diese
   Actor-Kette nicht. `cPlayer::move` (`:294`) muss die zentrale Original-Key/
   Pad-Verarbeitung verwenden.
3. Vollständige `cSatMgr::check`-/Sphere-/Wall-Adjust-Kette (`atari.cpp:54` und
   weitere Körper). Die vorhandenen SAT/EAT-Segment-/Floor-Orakel liefern keine
   vollständige Actor-Kollision oder Kamera-Hindernisauflösung.
4. Gemeinsame AEV/ITA-, Effekt-, Shadow-, Room-Texture-, Item-/Etc-/Cloth- und
   Controlregistrierung sowie originale Objekt-/Light-/SAR-Lebenszeiten.
   Erst der lebende Actor kann als Besitzer der Player-Lichter registriert werden.
5. `CameraRoomInit/CameraMove` (`camera.cpp:65/73`), CamCtrl/View und
   `CameraQuasiFPS::init/move/hitCheck` (`cam_qfps.cpp:1152/1189/705`) aus
   tatsächlichem Player-/Roomzustand. Kein vorberechneter EVD-Diagnoseframe
   darf diese Schulterkamera ersetzen.
6. Vollständige `ScenarioRoomInit`-Abhängigkeiten (`sce_sys.cpp:49`), insbesondere
   `SceAtSetSaveItem` (`sce_at.cpp:3562`) mit echten Item-Actors. Native
   Modulregistrierung und isoliert ausführbares R120Init bestehen bereits;
   die gesamte Roomkette ist dadurch noch nicht angebunden.
7. `Event::init/Run/ExePacket` (`event.cpp:152/205/732`) mit lebenden Actors,
   Objects, Motion-, Audio-, Fade- und Callbackbesitzern. `EventCarInit`
   (`st1/r120.cpp:462`) initialisiert weiterhin die originale 24-Gelenk-Car-
   Ressource nur CPU-seitig. Das Auto wird nicht statisch in den Debugframe
   eingefügt. R120Event benötigt weiterhin reale Sofdec-/CRI-Abhängigkeiten;
   weder Video noch Events werden übersprungen.
8. Erst danach der vollständige `gameMainLoop` (`game.cpp:569`) mit originaler
   Actor-/Scenario-/Object-/Camera-/Light-/Collision-/OT-/Soundreihenfolge und
   einer Vulkan-Präsentation aus diesen tatsächlichen Managerzuständen.

MotionMove, SEQ und komplette originale Leg/Heel/Toe-IK bleiben über mehrere
Ticks auf persistenten Modellen nachgewiesen. Ihr isolierter Actor-Statusview
und leerer SAT-Besitzer sind keine Leon-Spielsimulation. Hokan/Morph/Cloth und
SEQ-Sound-/Eventdispatch müssen vor ihrer tatsächlichen Verwendung angebunden
werden. New-Round-Load benötigt zusätzlich den vollständigen globalen Reset,
die originale BGM-Tabelle und weitere New-Round-Verbindungen. Der gewöhnliche
Save/Load-Pfad wird deswegen nicht mit dem New-Round-Pfad gleichgesetzt.

## Reproduktion und private Nachweise

Hostbuild: in jeweils initialisierter VS-x86-/x64-Compilerumgebung mit Ninja,
CMake/CTest und Python im PATH:

```powershell
.\port\scripts\test-host.ps1 -Generator Ninja -BuildName host-ninja
# In der x86-Compilerumgebung:
.\port\scripts\test-host.ps1 -Generator Ninja -BuildName host-x86
```

Die tatsächlichen lokalen Builds nutzten die vorhandenen ignorierten
`light-host64.cmd`/`light-host32.cmd` zur VS-Umgebungsinitialisierung.
Android wurde offline über `port/scripts/build-android.ps1` mit SDK,
Android-Studio-JBR, Python und Validation-Layers gebaut. Build, Debug-APK,
Test-APK, Unitprüfungen und Lint liefen erfolgreich; die bereits vorhandene
SDK-XML-Version-3/4-Warnung bleibt dokumentiert.

```powershell
$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskAdb = 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$taskData = 'E:\Development\Android\RE4-Data'
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-scene-host64" --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-scene-host32" --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-scene-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/summarize_consumers.py --host "$taskData\phase5b-scene-host64" --host32 "$taskData\phase5b-scene-host32" --device "$taskData\phase5b-scene-device" --output "$taskData\phase5b-scene-summary"
.\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc
& $taskPython port/tools/test_gx_scene.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-scene-gpu-r100" --adb $taskAdb --serial dbdcacc --room 100 --event Evd/r100s40.evd --cut 0 --frame 30 --model pl0000 --debug-view 1 --repeat 2
```

Zusätzlich `test_collision.py`, `test_skinning.py`, `test_resources.py`,
`test_modules.py` jeweils mit `--repeat 2` auf allen vier ABIs. Host-Executables
sind der Reihe nach `re4_game_tool.exe`, `re4_gx_tool.exe`,
`re4_resource_tool.exe`, `re4_module_tool.exe`. Private Ergebnisse liegen unter
`phase5b-scene-{host64,host32,device}-test_{collision,skinning,resources,modules}`.
Die GX-Hostdiagnosen liegen unter `phase5b-scene-gx-host{64,32}-r{100,102,119}`.
`consumer-runtime-summary.json` im Summaryordner enthält die automatisch
verifizierten ABI-/Trace-/Ressourcenmessungen. Ausführungs-, Build-, CTest-,
APK- und Validation-Logs liegen ignoriert unter `port/build/phase5b-scene-*`;
APK/Core-Nachweise zusätzlich in den bestehenden `graphics-*`,
`instrumentation-*` und `device-*-round*.txt`.

## Vollständig gespeicherte Projektdateien und Git

Neue Datei: `port/docs/phase5b-scene-status.md`.
Geänderte vollständige Dateien:

```text
port/README.md
port/config/runtime-sources.json
port/config/subscreen-sources.json
port/include/re4/port/engine_loader.h
port/include/re4/port/game_runtime.h
port/include/re4/port/merchant.h
port/include/re4/port/messages.h
port/include/re4/port/save_state.h
port/include/re4/port/subscreen.h
port/src/engine_loader.cpp
port/src/game_runtime.cpp
port/src/id_save.cpp
port/src/id_save_original.h
port/src/merchant.cpp
port/src/merchant_original.h
port/src/message_original.h
port/src/messages.cpp
port/src/runtime_original.h
port/src/subscreen.cpp
port/src/subscreen_original.h
port/tests/game_runtime_tool.cpp
port/tests/test_consumer_sources.py
port/tools/generate_consumers.py
port/tools/generate_id_save.py
port/tools/generate_merchant.py
port/tools/generate_runtime.py
port/tools/generate_subscreen.py
port/tools/summarize_consumers.py
port/tools/test_game_runtime.py
```

`master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`, `origin/master`
bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`. Dessen README-Aktualisierung
wurde gelesen und berücksichtigt. Keine Zusammenführung, kein Rebase, kein Push.
Nur Portquellen und Dokumentation werden im lokalen getesteten Commit gesichert;
Images, extrahierte Originaldaten, GPU-Bilder und Buildprodukte bleiben außerhalb
des Git-Inhalts. Native ARM32/ARM64-, Controller- und Vulkanpfade bleiben erhalten.
MSAA Aus/2x/4x, AF 2x/4x/8x, Nah-/Zielfokus-DoF sowie persistente,
hardwarevalidierte Einstellungen bleiben verbindliche spätere Anforderungen.
Es wurde keine weitere Hauptphase begonnen.
