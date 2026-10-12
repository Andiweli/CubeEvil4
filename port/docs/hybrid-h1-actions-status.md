# Hybrid H1: Originale Action-Auswahl – geprüfter Teilstand

Historischer Stand. Die Fortsetzung
[Originale Cloth- und Player-Laufzeit](hybrid-h1-cloth-status.md) weist jetzt
die erfolgreiche Rückkehr von `startUp` und neutrale Originalticks nach;
der aktive Walk-Pfad bleibt am Original-Soundverbraucher unvollständig.

Ausgangsstand: `7de4a23c098219a8b2061085e4cff80ca55953d5`, Branch
`android-port`, Prüfung am 12. Oktober 2026. **H1 bleibt offen.**

Die originale Leon-Konstruktion erreicht mit echten G4BE08-Ressourcen jetzt
ActionSelect, Augensteuerung, Actor-Kollision und SAT-Bodenkontakt. Der nächste
Actor-Abbruch liegt bei `PlClothMoveLeon/PenClothMove native simulation owner`.
`startUp()` kehrt noch nicht zurück; es gibt keinen vollständig konstruierten,
veröffentlichten Leon und keinen vollständigen Player-Tick.

Der echte GameTask erreicht unverändert `Rno0=2`, nach abgeschlossenem
`gameInit` und `gameStageInit`. Sein `gameRoomInit`-Consumer ist weiterhin nicht
gebunden. Die Actor-Prüfungen erfolgen auf dem bestehenden Scheduler als
isolierte Originalcode-Ausführung, außerhalb dieses noch fehlenden Körpers.

## Spendervergleich vor der Implementierung

Das Referenzprojekt `E:\Development\Android\RE4\RE4-Android-Decomp` wurde
ausschließlich gelesen. Dort wurden keine Builds, Änderungen, Formatierungen
oder Git-Schreiboperationen ausgeführt. Nachstehende Pfade sind relativ zu
diesem Referenzprojekt.

| Spenderpfad / Funktion | Befund und Entscheidung |
|---|---|
| `app/src/main/cpp/room_interactions.cpp`: `RoomInteractions::windowOffer`, `pushOffer`, `interact`, `tickR100Scripts` | Fenster-, Rack- und Türzustände gehören `RoomInteractions`/`LeonPreview`; Angebote und Raumskriptzustände werden teilweise separat abgebildet. Keine Übernahme als zweite Interaktionsverwaltung. |
| `app/src/main/cpp/leon_preview.cpp`, `tools/audit_player_actions.py`: Mapping von `actionSelect`, Footwork, Walk und Kontextaktionen | Der Audit benennt ausdrücklich den fehlenden originalen Actor-/Scenario-Prioritätsmanager und beliebige Callbacks. Kein vollständiger nativer `cActionButton`-Body in `app/src/main/cpp` gefunden. |
| `app/src/main/cpp/collision_query.cpp`: `sourceSphere`, `SatCollision::resolveScenerySphere` | Brauchbare originale Triangle-/Sphere-Berechnungen, bereits durch die vorhandene native SAT-/EAT-Portierung und deren Tests abgedeckt. Keine erneute konkurrierende Kollisionswelt. |
| `app/src/main/cpp/walk_probe.cpp`: `WalkProbe` | Eigenständiger Positions-, Kontakt- und Traversalbesitzer. Nicht als `cAtariInfo`-/Actor-Ersatz übernommen. |
| `app/src/main/cpp/hit_geometry.cpp`: `emLineCapsuleCrossCk`, `HitLineCapsule`, `HitLineCube` | Originale Hit-Geometrie mit nativen Datenprüfungen; geeigneter späterer Spender für Schadens-/Waffentraces. Diese Funktionen werden für die aktuelle ActionSelect-Startkette nicht zusätzlich benötigt. |
| `app/src/main/cpp/ports/motion64/motion.cpp`: `MotionHokan`; `animation.cpp`: cMot3-/Waist-Adaptionen | Die vorangehende H1-Übernahme bleibt erhalten. Aktuelle Action-Auswahl nutzt die neue Modell-/Motionverwaltung; kein Cast zwischen den verschiedenen `cModel`-Layouts. |
| `app/src/main/cpp/follow_camera.cpp`: `resolveShoulderEye` | Nutzt originale Profilwerte und SAT-Strahlen, lässt aber ausdrücklich Waffen-Zoom für feste Bildgestaltung weg. Keine vollständige originale Kamerasteuerung; hier nicht übernommen. |
| `app/src/main/cpp/event_playback.cpp`: `OriginalEvent::roomCallback`, `initCarActors` | Eigenständige EVD-Wiedergabe mit Paketnamen-Abzweigungen. Nicht anstelle nativer Original-Callbacks, Module und Scheduler übernommen. |
| `app/src/main/cpp/scenario_collision.cpp`: `ScenarioCollision::geometry`, `enabled` | BE-AEV-Aufbereitung ist technisch nützlich; `enabled` enthält eigene r100-Flagentscheidungen. Die Originalskripte und ein zukünftiger gemeinsamer SceAt-Owner bleiben maßgeblich. |
| `app/src/main/cpp/object_actions.cpp`, `room_jumps.cpp` | Objekt-/Traversalzustände besitzen eigene Besitzer und Übergänge. Nicht in native Player-/ObjMgr-Work-Strukturen hineininterpretiert. |
| `app/src/main/cpp/room_effect_cloth.cpp` | Raum-Effekt-/Cloth-Berechnungen, unter anderem `OriginalClothSin`; keine native originale `PlClothMoveLeon`-/`PenClothMove`-Kette gefunden. |

In diesem Auftrag wurde **kein zusätzlicher Spender-Funktionsbody direkt
kopiert**. Die geeigneten Motion- und Sphere-Berechnungen sind bereits
integriert; die fehlende originale Actionverwaltung ist im Spender nicht
vollständig gelöst. Der konkrete Gewinn des Vergleichs ist die belegte
Abgrenzung dieser vorhandenen Algorithmen von nicht kompatiblen Gameplay-
Besitzern. Es wurde keine LeonPreview-Steuerung übernommen.

## Originalfunktionen und native Bindungen

`port/tools/generate_actions.py` übernimmt 50 vollständige Originalkörper mit
Quellpfad, `#line` und Manifest. Alle Quellen und zugehörigen Header werden
über normalisierte SHA-256-Fingerprints geprüft; Änderungen führen zu einem
Generatorfehler. Die ursprünglichen `src/`- und `include/`-Dateien wurden nicht
verändert.

Neu angebunden sind insbesondere:

- `src/game/act_btn.cpp`: `cActionButton::init`, `pullWork`, `set`, `move`,
  `checkButton`, `checkPLStatus`.
- `src/game/pl_class.cpp`: `cPlayer::actionSelect`, `actCheck`, `keyReload`,
  `actWallCheck`, `fanceCheck`, `fallCheck_80172E38`, `jumpCheck`, `upDownCk`,
  `windowCheck`, `moveEye`, `moveEyeNormal`, `moveEyeMotion`.
- Dortige residenten Callbacks `holdOn`, `fallOn`, `fanceOn`, `jumpFallOn`,
  `levelUpOn`, `levelDownOn`, `level2UpOn`, `level2DownOn`.
- `src/game/pl_sub.cpp`: `PlGetStatus`, `joyKamae`, `joyLKamae`.
- `include/pl_wep.h`: die originale Basisimplementierung `cObjWep::keyKamae`.
- `src/game/pl_push.cpp`: `cPlPush::catchCheck`, mit den bestehenden EmMgr-
  Slots, SAT-Abfragen und unveränderten Rack-/Höhenbedingungen.
- `src/game/at_mod.cpp`: `EmAtCheck`, `priorityCheck`, `__em_at_core`,
  `At_em_rect_rect_ck`, `em_rect2_ck_sub`, `At_em_sphere_rect_ck`,
  `sphereRectCk`, `At_em_sphere_sphere_ck`.
- `src/game/at_sub.cpp`: `At_rect_point_ck`, `At_rect_rect_ck`, `Get_ang_dir`;
  zugehörige Originalmathematik aus `src/game/sub2.cpp`.
- `src/game/emwindow.cpp`: der ganze `ChkWindow`-Body, mit ausdrücklich
  noch fehlenden Live-Fenster-/Ladder-/Rack-Verbrauchern.
- `src/game/sce_at.cpp`: `SceAtCheckFieldInfo`. Im isolierten Probe wurde
  `SceAtInit` nicht ausgeführt: die originale Null-`pS`-Verzweigung wird
  ausgeführt. Dies ist keine Behauptung einer initialisierten leeren Raumwelt.
- Drei zusätzliche Inventarkörper aus `src/game/item.cpp`: beide
  `cItemMgr::reloadable`-Überladungen und `WeaponId2BulletId`, für die
  originale Reload-Abfrage. Die englischen Originaltabellen bleiben maßgeblich.

Das neue `actions.cpp` bildet ausschließlich Plattformgrenzen ab:

- Acht Works und 16 OT-Prioritäten bleiben erhalten. Native Indizes ersetzen
  die 32-Bit-RAM-Tags. Die Traversierung läuft von hoher zu niedriger Priorität,
  innerhalb einer Priorität in originaler LIFO-Reihenfolge. Ein abgelehnter
  hoher Eintrag führt korrekt in die niedrigeren Buckets weiter.
- Funktionsaufrufe haben tatsächliche C++-Signaturen. `arg` bleibt `void*`,
  `d` wird `intptr_t`; kein Funktionszeiger wird als Datenpointer gespeichert.
  Ein Sce-Taskmodell wird getrennt als nativer Pointer angegeben.
- Actor-Callbacks erhalten einen Lifetime-Token. Die Rückabwicklung entfernt
  ihre Frame-Works vor der Freigabe des Actors. Kopierte alte Callbacks werden
  auch bei erneut verwendeten Actor-Slots abgelehnt.
- Szenarioaktionen verwenden den vorhandenen stackerhaltenden Scheduler,
  originale Auto-Slotwahl, Priorität und Kind 1/2 sowie `ModuleToken`.
  Es werden keine Owner-Pins auf abbrechbaren Taskstacks angelegt.
- Nicht residenter direkter Moduldispatch bleibt ausdrücklich ungebunden.
  Ungebundene Promptausgabe und `ACT_FUNC_SCE_AT`-Owner werfen Fehler.
- Die Eye-Statik gehört dem einzelnen Engine-Kontext. Die originalen
  Interpolations- und Blinkberechnungen werden unverändert ausgeführt.
- `PlayerPushCpu` speichert native Root-/Actorreferenzen. Es gibt keinen Cast
  in einen GameCube-`cPlayer`-/`cEmRack`-Speicherblock.

## Tatsächlicher Actor-Fortschritt

```text
EmMgr.create(0, 0) -> cEmMgr::construct
  -> erhaltene cPlayer-/cPlLeon-Konstruktorbodies
  -> Originalmodell: 119 Gelenke, 8 Modellinfos und Weapon-Attachments
  -> ReadWepData / wep01 bzw. wep02 / WeaponInitFunc
  -> init1 / originale Cloth- und Body-Initialisierung
  -> startUp -> virtueller cPlLeon::move -> cPlayer::move
  -> pl_R0_Move -> pl_R1_Footwork
  -> originale primäre Motion und Hals-Motion
  -> actionSelect, joyKamae/joyLKamae, SAT-Kontextprüfungen, Push-Auswahl
  -> originale Augen-, Body-/Waist- und Parts-Matrizen
  -> EmAtCheck -> SAT-Boden-/Wandbehandlung
  -> Weapon-Matrixupdate
  -> Abbruch: PlClothMoveLeon/PenClothMove native simulation owner
```

Die neutralen Versuche dokumentieren jeweils einen begonnenen virtuellen
Move, einen erfolgreichen MotionMove, eine abgeschlossene Action-Auswahl,
Augensteuerung und Actor-Kollisionsverwaltung. Routines bleiben `(0,0,1,1)`,
Idle-Status ist `1`, `STA_SSCRN_ENABLE` ist gesetzt. Es sind keine Action-Works
angeboten. Das ist eine vor der Cloth-Simulation abgebrochene Aktualisierung,
kein erfolgreicher vollständiger Player-Tick.

Zusätzlich wählen definierte Eingaben durch die zentrale `PadRead`-Kette:

| Native Eingabe im automatisierten Test | Originale Routine nach ActionSelect |
|---|---|
| Linker Stick vorwärts | `(0,1,0,0)` |
| Rechter Analogtrigger | `(0,6,0,0)` |
| Linker Analogtrigger | `(0,11,0,0)` |

Diese Prüfungen belegen die **Auswahl**, nicht bereits Walk-/Weapon-/Knife-
Ticks der jeweiligen Folgeroutine. Es wurde keine Eingabebelegung verändert.
Die frühere Annahme im neuen Test, Android-R1 sei der Aim-Trigger, wurde
korrigiert: R1 ist in der bestehenden Anbindung GameCube-Z.

Im neutralen Versuch wird die ursprüngliche globale Startposition zuerst
unverändert übernommen. Anschließend verändert echte SAT-Verarbeitung die
Bodenhöhe. Der Test lädt die originalen SAT-/EAT-Blöcke des geladenen r120-
Archivs mit dessen CNS-Kapazitäten. Er verändert weder die Position, um einen
Boden zu finden, noch Diagnoseflags, um eine fehlende Funktion zu umgehen.
Die Bodenhöhe wird zusätzlich gegen eine unabhängige Triangle-Referenz aus
denselben BE-Daten geprüft; zulässiger numerischer Fehler ist 0,002.

## Ausstehende gemeinsame Verbraucher

1. `PlClothMoveLeon` in `src/game/pl_cloth.cpp:298`: Jacket, Holster und Hair
   werden wirklich simuliert und danach `be_flag & ~0x00E00000` ausgeführt.
   Die vorhandene Initialisierung reicht dafür nicht. Die zugehörigen
   `PenClothMove`-/`PenClothMove3`-Bodies beginnen in
   `src/game/pendulum.cpp:225` bzw. `:823`. `penClothAtMake` verwendet derzeit
   originalen Locked-Cache-Scratchspeicher `0xE0000000` (`:1160`); erforderlich
   sind ein nativer Besitzer und die originalen Volumen-/Link-/Orientierungs-
   berechnungen. Ein Überspringen würde Originalflags und Pose verändern.
2. Danach sind im erhaltenen Move-Body unter anderem die tatsächlichen
   Sound-/Wasser-/Debug-Verbraucher zu prüfen. Die originale Konstruktion
   benötigt außerdem noch die Foot-Shadow-Tabellenbindung. Diese wurden nicht
   mit leeren Erfolgsfunktionen ersetzt.
3. Live-Interaktionen benötigen `SceAtInit`/AEV-/ITA-Works und deren OT,
   Parentreferenzen, Feldgeometrie, Hit-/Exec-/Enable-Flags. `ChkWindow` führt
   bei einem echten Fenster weiter zu Fence-/Break-/Ladderprüfungen und
   `cEmWindow::ExeWindowEvent`. Diese Live-Owner fehlen.
4. `cEmRack`-Konstruktion und Rack-Work-/Range-Lebenszyklen sowie vollständige
   Push-/Fenster-/Tür-Folgeroutinen sind nicht durch die Spender-Interaktionen
   ersetzt worden. Die ersten Routineübergänge sind verfügbar, ihre komplette
   Animation und Objektwirkung ist noch nicht bewiesen.
5. Die originale Promptausgabe `cActionButton::disp` benötigt die konkrete
   `cMes`-/ID-Unit-/Cockpit-Widgetbindung. Die Tests verwenden gekennzeichnete
   Prompt-Sinks; die Spielruntime besitzt keinen Fake-HUD-Erfolg.
6. Die übrigen `Pl_func_move_tbl`-Routinen und die ganze `gameRoomInit`-
   Reihenfolge müssen nach der erfolgreichen Actor-Freigabe gebunden werden.
   Raum-/Event-/SceAt-Initialisierung wird nicht künstlich abgeschlossen.

Die originale Sphere-Sphere-Routine enthält zusätzlich einen Adressvergleich
als Tie-Break bei Priorität 0. Dieser wurde auf `uintptr_t` verbreitert, ohne
Pointer-Trunkierung. Gleichheit dieses Zweigs mit der ursprünglichen
GameCube-Pooladressordnung ist für eine vollständige dynamische Actor-/Objekt-
Welt noch nicht nachgewiesen; hierfür muss die originale logische Poolordnung
untersucht werden. Die aktuellen Startproben sind kein solcher Nachweis.

## Verifikation

| Prüfung | Tatsächlich ausgeführt / Ergebnis |
|---|---|
| Windows MSVC x86 und x64 | Je 27/27 CTest-Gruppen bestanden; Original-Quellgeneratoren einschließlich Action-Fingerprintänderung geprüft. |
| Android NDK ARM32 und ARM64 | Native Tools und Gradle-Debug-/Test-APK gebaut; Offline-Gradle und Lint bestanden. |
| Retroid Pocket Flip2, `dbdcacc`, Android 13 / API 33 | `armeabi-v7a` und `arm64-v8a` als tatsächliche 32-/64-Bit-Prozesse verifiziert. |
| Retroid native Grundsuite, je zwei Durchläufe | ARM32: 13.386, ARM64: 13.387 Prüfungen, jeweils null Fehler. Der Unterschied ist eine ABI-/Typprüfung; Scheduler-/Background-/Yz2-Traces sind gleich. |
| Finale Retroid-APK | Je ABI 20/20 Instrumentierungstests bestanden, einschließlich Controller/JNI, Originalruntime und tatsächlichen Vulkan-Surface-Clears/Präsentationen. |
| Original-Datenruntime | Zwei Wiederholungen × beide Start-Disc-Eingaben × New-game/Load-save-Eingabe: acht Bootprüfungen pro Plattform. Die tatsächliche r120-Raumdisc wird durch den originalen StageSet-Pfad gewählt. |
| Actor-Konstruktion im Datenlauf | Pro Boot zehn neutrale Waffen-/Variantenversuche und drei definierte PadRead-Eingaben, mit vollständiger Rückabwicklung an der dokumentierten Cloth-Grenze. 104 Versuche pro Plattform; keine als erfolgreicher vollständiger Actor gezählt. |
| Action-/Callback-Isolation | Originale Work-Poolgrenze, hohe/niedrige Priorität, gleicher Slot/LIFO, `NO_EXEC`, Exact-Key, Aim-/Dead-Gates, native Argumentbreite, Callback-Verfall, Sce-Sleep/Fortsetzung, Abbruch und Modulinvalidation geprüft. Acht echte residente Callbacks sowie Fence-down-Flags und Damage-Timer ausgeführt. |
| Scenery-Regression | SAT/EAT aus r100, r102, r119, r120 und r300, auf beiden Hosts und Retroid-ABIs, je zwei Wiederholungen. Originalattribute, Indizes und Resultate gegen unabhängige Triangle-Referenz geprüft. |
| GX-Hostregression | Originale Primitive-, OT-, TEV-, Licht-, Kamera- und TPL-Referenzprüfung auf x64 mit zwei Wiederholungen bestanden. |
| Retroid-GPUregression | r120-Diagnose, zwei Durchläufe je ABI: ARM32 29/29, ARM64 14/25 Geometrieframes, jeweils 229.376 Nicht-Clear-Pixel. Alle vier Readbacks sind bytegleich und stimmen mit dem bisherigen Diagnosehash überein. |

Die neutrale r120-Bodenhöhe wurde als `-0.000488281` gemessen; die unabhängige
Triangle-Referenz liefert `0`. Fehler etwa `0.000488281`, innerhalb 0,002.
Dieser numerische Rest wird gesondert geprüft und nicht als Gameplayflag in
den semantischen ABI-Hash aufgenommen. Die verbleibenden Callback-/Routine-
Entscheidungen werden plattformübergreifend verglichen.

Die Scenery-Maximalfehler hängen vom Originalraum ab, zum Beispiel r120 SAT
und EAT `0.00025`, r119 SAT `0.015125` und EAT `0.010168`, r300 SAT
`0.00332307` und EAT `0.00641391`. Die getesteten Fehlergrenzen und getrennten
Prüfungen stammen aus der bestehenden unabhängigen Kollisionsreferenz. Es
wird keine bitgenaue PPC-Floating-Point-Gleichheit behauptet.

Die 32 Boot-Ergebnisberichte (acht je Plattform) besitzen jeweils für
denselben Disc-/New-game-/Wiederholungsfall identische normalisierte
semantische Traces. Zusammen sind 416 rückabgewickelte Originalkonstruktionen
enthalten, davon 96 mit definierten ActionSelect-Eingaben. Physische Eingaben,
vollständige Actor-Ticks oder ein abgeschlossenes `startUp` sind damit nicht
nachgewiesen.

Sämtliche Hardwarefolgen verwenden den vorhandenen
`Global\Retroid_ADB_Exclusive`-Mutex über PowerShell `System.Threading.Mutex`,
60 Sekunden Timeout und `try/finally`. Installation, Start, Auswertung und
Bereinigung liegen innerhalb der jeweiligen Sperre; ADB wird immer mit
`-s dbdcacc` ausgeführt. Es gab keinen Server-Kill, Neustart oder fremden
Paket-/Testdaten-Cleanup. Dies sind automatisierte Eingabetests; keine neuen
physischen Tastendrücke des Nutzers wurden behauptet.

Die frühe Testfassung scheiterte nachvollziehbar an fehlendem geladenen SAT
(Floor-Diagnoseline), einer zu strikten Positionsprüfung vor/nach echter
Kollision, der falschen R1-/Aim-Annahme und dem Referenzlookup von r120 auf
der Start- statt der tatsächlichen Raumdisc. Diese Testfehler wurden an ihren
Ursachen korrigiert. Ein Android-Buildfehler wegen einer nach Anpassung
unbenutzten Originalvariablen wurde durch Weitergabe des originalen `flag`
behoben. Kein fehlender Gameplayverbraucher wurde dafür übersprungen.

Private Ergebnisverzeichnisse unter `E:\Development\Android\RE4-Data`:

- `h1-action-host32`, `h1-action-host64`, `h1-action-retroid`: vollständige
  JSON-Berichte, Quell-/Assethashes, pro Lauf ausführbares Binary-SHA-256,
  Zustände und normalisierte semantische Traces.
- `h1-action-collision32`, `h1-action-collision64`,
  `h1-action-collision-retroid`: BE-SAT-/EAT-Prüfungen und Fehlerwerte.
- `h1-action-gx64`: Original-GX-/Material-/Texturreferenz.
- `h1-action-gpu-retroid`: echte GPU-Readbacks, PNG-Screenshots und Validation-
  JSON-Berichte. Der Hash ist
  `b46d87280a1701af2992f04a55b1aed31a409dbc293313c7cec4eabff02ae3bf`.

Khronos-Validation ist auf dem Retroid aktiv: je GPU-Bericht null Fehler und
eine Performancewarnung. Das bestehende Identity-/ROTATE90-PreTransform-Thema
bleibt offen. Diese Bilder stammen aus dem vorhandenen isolierten Original-
SMD-/Eventpose-Diagnosepfad; sie zeigen keinen laufenden Leon-Actor, keine
vollständige Spielschleife und keinen Nachweis neuer visueller Originaltreue.
Der abschließend unter demselben Mutex gelesene ARM64-Prozesslog enthält
außerdem AdrenoUtils-Fehlermeldungen `Unknown Format 0` und
`Memory Layout input parameter validation failed!`. Ihre Ursache ist nicht
geklärt. Null Khronos-Validationfehler bedeutet hier ausdrücklich keine
vollständig sauberen Vendorlogs. Der prozessgefilterte Log ist lokal unter
`port/build/h1-action-gpu-driver.log` gespeichert.

Ignored Build-/Gerätelogs stehen unter `port/build/h1-action-*.log`; darunter
`build32`, `build64`, `build-android`, `ctest32`, `ctest64`, `data32`, `data64`,
`data-retroid`, `native-retroid` und `app-retroid`.

Wiederholung: `port/scripts/build-android.ps1`, `test-device.ps1` (Repeat 2),
`test-app-device.ps1`; `port/tools/test_game_runtime.py` und
`test_collision.py` mit `--data` auf `import-phase3a`, separatem privatem
`--output`, `--repeat 2` sowie `--host-exe` oder `--adb ... --serial dbdcacc`.
Der Python-Hardwarebroker hält denselben Mutex über den kompletten Lauf.

## Reproduzierbarkeit und geänderte Dateien

Alle Implementierungen befinden sich im neuen Projekt. `actions-sources.json`
und `generate_actions.py` erzeugen die vollständigen überprüften Quellkörper;
Build-Ausgaben und Originalressourcen gehören nicht in Git.

Neu: `port/src/action_original.h`, `port/src/actions.cpp`,
`port/config/actions-sources.json`, `port/tools/generate_actions.py` und dieser
Bericht.

Geändert: `port/CMakeLists.txt`; Header `game_runtime.h`, `player.h`,
`subscreen.h`; `game_runtime.cpp`, `object_original.h`, `player.cpp`,
`player_original.h`, `player_live.cpp`, `player_live_original.h`,
`subscreen.cpp`, `subscreen_original.h`; Generatoren `generate_player.py`,
`generate_subscreen.py`; Tests `game_runtime_tool.cpp`,
`game_runtime_verification.cpp`, `test_runtime_sources.py`,
`test_consumer_sources.py`, `test_game_runtime.py`.

`master` bleibt auf `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` auf `15cf64f80e08bbc64712c39be330b75d1512a0d0`. Die aktualisierte
README wurde gelesen. Es gab keinen Merge, Rebase oder Push. Original-Assets,
Images und abgeleitete Bilder bleiben ausschließlich im privaten Datenbaum.
Vulkan und die vorgesehenen Grafikoptionen MSAA Aus/2×/4×, AF 2×/4×/8× sowie
persistentes DoF mit Nah-/Zielfokus bleiben unverändert. Keine weitere
Hybridphase wurde begonnen.
