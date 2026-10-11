# Hybrid H1: MotionHokan, cMot3 und Actor-Modellbesitz

Ausgangspunkt: `4ee682f237ceeff9963b6953b38b5e0b451bc90b`, Branch
`android-port`, 11. Oktober 2026. Implementiert wurde ausschließlich in
RE4-Fixed. Der technische Spender RE4-Android-Decomp wurde ausschließlich gelesen.

## Abnahmegrenze

**Geprüfter H1-Teilstand; kein vollständiger Leon-Actor und keine neue
GameTask-Spielszene.** MotionHokan, der originale Dreiweg-Motionblend und die
originale Sphere-Verdrängung sind nativ ausführbar. Der originale
`cEm`/`cDmgInfo`-Konstruktor-Unterbau wird auf derselben Modellinstanz wie
Leons originaler BIN/TPL-Aufbau ausgeführt. `cPlayer`/`cPlLeon` als vollständige
Gameplayobjekte, `EmMgr.create/construct`, Waffenobjekte und `startUp()` sind
weiterhin offen. Das ist ausdrücklich keine abgeschlossene H1-Abnahme.

Der echte GameTask erreicht wie zuvor `Rno0=2` nach `gameInit` und
`gameStageInit`. Der Originalkörper von `gameRoomInit()` ist weiterhin
ungebunden: `port/src/runtime_original.h` meldet
`Original consumer unavailable: gameRoomInit/model/light/player`.
Dieser Auftrag hat den GameTask-Ausführungspunkt nicht vorgeschoben.
Die vorhandenen Isolationsprüfungen mit absichtlich fehlenden Boot-Consumern
sind davon zu unterscheiden; ihre älteren Abbruchpunkte sind kein Rückschritt
des Originaldaten-Bootpfads.

## Tatsächlich übernommene Originalfunktionen

Die vollständigen Funktionskörper werden bei CMake-Konfiguration aus den
unveränderten Originalquellen erzeugt. Fingerprint-Manifeste und generierte
Funktionsmanifeste erfassen Herkunft und Anpassungen. Es wurden keine
Originalquellen unter `src` oder `include` geändert.

| Originalquelle | Ausführbarer Umfang |
|---|---|
| `src/game/motion.cpp`, `MotionHokan`, `nearZero` | Translation, Quaternion-Slerp, Scale-Blending, Parent-Scale-Cancellation, Flags und `prevMat` |
| `src/game/pl_class.cpp`, `cMot3::cMot3/set/set0/move` | Primärmotion, stabile sekundäre Work, Vorzeichenwechsel, Rate-Clamping, normale und additive Kombination |
| `src/game/math_sub.cpp`, `VecLinearCombination` | Originale Root-Speed-/Rotationskombination der sekundären Motion |
| `src/game/at_sub.cpp`, `At_poly_sphere_ck/ck2`, `At_surface_point_rel/line_ck`, `At_poly_point_rel` | Face-/Edge-Verdrängung, Frontseitenprüfung, SAT-/EAT-Attribute, Masken und Edge-Cancel |
| `src/game/atari.cpp`, `polyBitSet/Ck`, `blkPolySphereCk/Core`, `cSatMgr::polySphereCk` | Originaler Blockdurchlauf, Polygon-Deduplizierung, native Stückmatrizen und Ergebnisnormalen |
| `src/game/em.cpp`, `cEm::cEm/initWork`, `cDmgInfo::cDmgInfo/clear` | Originaler Modell-/Damage-Konstruktor-Unterbau; `be_flag=0x21`, `kindid=0`, gelöschte Damage-Flag/Timer |

Die bereits vorhandenen originalen `MotionMove`, `MotionMoveCore`,
`MotionSequenceCtrl`, SDK-Quaternion-/Matrixfunktionen und `cModel::matBlend`
führen die neuen Motionpfade aus. Die originalen Leon-Modell-, Hände-,
Gesichts- und Motionregistrierungsfunktionen bleiben erhalten.

### Technischer Gewinn aus dem Spender

`RE4-Android-Decomp/tools/generate_motion64.py` zeigt eine wesentliche
Compileranpassung: `asm("" : "=r"(pm) : "0"(p->prevMat))` ist eine echte
Identitätszuweisung, keine entbehrliche Compilerbarriere. Die neue Erzeugung
bewahrt `pm = p->prevMat`; sonst wäre das Ziel des folgenden Matrix-Copy
uninitialisiert. Ebenso ersetzt bei `cMot3::set` der tatsächliche Parameter
`b` das nur auf PPC gültige gepinnte Argumentregister `r9`.

Die alten `((cEm*)model)`-Casts werden nicht übernommen. Die sekundäre Motion
ist bereits ein benanntes natives `ModelCpu`-Feld. Die bestehenden neuen
Model-/Motion-/Ressourcenstrukturen bleiben maßgeblich. Die Source-Kommentare
zur positiven/negativen `mot1`/`mot2`-Auswahl sind teilweise widersprüchlich;
maßgeblich bleiben die tatsächlichen originalen Bedingungen:
negativer Wechsel wählt `mot2`, positiver Wechsel `mot1`.

`app/src/main/cpp/collision_query.cpp::sourceSphere` des Spenders ist ein
geometrischer Auszug. Hier wurden stattdessen die vollständigen Originalkörper
mit ihren Attribut-/Maskenfiltern und Manager-/Blockpfaden integriert.
`LeonPreview`, dessen Bewegung, Kamerasteuerung und Raumabläufe wurden nicht
übernommen. Der Gewinn ist eine wiederverwendbare Motion-/Kollisionsgrundlage;
die fehlende Gameplaykette verschwindet dadurch nicht.

## Native Plattformadapter und Lebenszeiten

* `EngineConsumers::initializeMotionBlend` hält drei geprüfte `MotionAsset`s,
  die primäre SEQ und die nicht kopierbare `MotionBlend3`-Work. Die Work bleibt
  adressstabil. Bei normalem Motionwechsel/Freigabe wird `pMotionB` abgelöst,
  bevor Work oder Datenleases zerstört werden.
  Die neue Work wird vor dem Ablösen bisheriger Daten allokiert; eine fehlende
  Speicherallokation hinterlässt damit den bisherigen Motionbesitz intakt.
* `cMot3::set` behält die vorherige Pose. Die Adapteranbindung führt hier
  keinen zusätzlichen `MotionClear` aus, damit die originale
  Hokan-Snapshot-Semantik erhalten bleibt.
* `ModelRuntime::Object` hält eine native `shared_ptr<ModelCpu>`-Instanz und
  eine Referenz auf genau diese Instanz. Ein ausdrücklich typisierter
  `ActorModelCpu` kann damit dieselbe Rootinstanz wie der Modellpool besitzen.
  Doppelregistrierung wird vor der Poolmutation zurückgewiesen.
* Poolfreigabe entfernt Parts/Infos, Manageradressen und Motion-/Floor-/Flip-
  Referenzen. Ein weiterlebender nativer Besitzer behält keine Pointer auf
  bereits freigegebene Motionleases. Generationhandles und bisherige Borrow-
  und Event-/Lichtregistrierungsgrenzen bleiben wirksam.
* `ActorModelCpu` und `ActorDamageCpu` sind ausdrücklich nur der ausführbare
  Konstruktor-Unterbau mit den dafür benötigten benannten Feldern. Sie sind
  kein vollständiges `cEm`-/`cPlayer`-/Damage-Speicherlayout und werden nicht
  als `pPL` oder als vollständiger Manager-Actor veröffentlicht.
* `CollisionRuntime::pushSphere` bindet den zuständigen originalen
  SAT-/EAT-Querytyp und GameState auf dem Frame-Thread. Dateidaten werden vor
  dem Originalalgorithmus als Big-Endian-Werte in geprüfte native Views
  aufgelöst. Der Wrapper ersetzt nicht `cSatMgr::check` und dessen komplette
  Actor-/Floor-/Link-Behandlung.

Kein Pointer wird auf `u32` verengt. Dateioffsets bleiben 32-Bit-Werte;
native Adressen, Owner und Matrixpointer besitzen die tatsächliche ABI-Breite.
Nicht erfüllte Attach-Camera-/IK-Abhängigkeiten bleiben explizite Fehler.
SAT-Debugpolygon-Ausgabe und PPC-Performancecounter (`debug_mode=0x14`) werden
ausdrücklich als fehlende Consumer gemeldet, nicht erfolgreich simuliert.

## Verifikation

Die Tests wurden mit den unten beschriebenen Abnahmegrenzen ausgeführt.
Sandbox-Dateizugriffsfehler der Baseline und der abschließenden Wiederholung
betrafen Testdateien und `Path.resolve` des gebündelten Python-Interpreters
(`WinError 5`). Die betroffenen Läufe wurden außerhalb der Sandbox wiederholt;
fehlgeschlagene eingeschränkte Läufe werden nicht als bestanden gezählt.

* Baseline: Windows x64 und x86 jeweils 27/27 CTest-Gruppen bestanden.
* Finale Host-/Android-Ergebnisse: siehe abschließender Ergebnisabschnitt.
* `port/tests/test_retroid_mutex.py`: unabhängige PowerShell-Broker schließen
  sich über `Global\Retroid_ADB_Exclusive` aus. Timeout und Freigabe nach einer
  injizierten Exception sind geprüft; dieser Test führt selbst kein ADB aus.
* Analytische Adapterfixtures führen echte Originalfunktionen aus:
  Translation/90-Grad-Rotationsblend, Scale-Cancel, Verbrauch der Flags,
  `prevMat`-Copy, positive/negative cMot3-Wechsel, additive Motion, fehlerhafte
  Eingaben, Work-Detachment sowie externe Actor-Modellfreigabe mit Motionlease.
* Sphere-Adapterfixtures prüfen Face-/Rim-Kontakt, originale `0.99*r`-Antwort,
  Backface, Edge-Cancel, SAT-/EAT-Filter, Masken, deaktivierte Stücke und
  ungültig gewordene Handles. Es sind numerische Testgeometrien, keine
  rekonstruierten Raumdaten.
* Originaldatenprüfungen führen die vorhandenen sieben Leon-FCV/SEQ-Paare
  über Gameplayticks aus und führen jetzt zusätzlich Hokan für jeweils zwei
  Ticks aus. Matrizen müssen endlich bleiben und Hokan bis null ablaufen.
  Diese Eingabemodi sind isolierte Prüfungen auf Disc-Daten, keine von
  `cPlayer::move` gewählten Gameplayanimationen.

Die analytischen Hokan-Schranken betragen absolut `1e-6`, der Blend-/Edge-Test
`1e-5`. Die vorhandene unabhängige FCV-Kurvenprüfung verwendet
`max(2e-5, abs(reference)*2e-5)`. Ein PPC-Hardwaretrace derselben Hokan-Pose
liegt nicht vor; diese Schranken werden nicht als gemessene PPC-Abweichung
ausgegeben. Fused-FP/fast-math bleiben ausgeschaltet.

ADB-Prüfungen verwenden ausschließlich `-s dbdcacc`. Alle Transfers,
Installationen, Starts, Instrumentierungen, Surface-Tests, Logauswertungen
und jeweilige vorhandene Bereinigung sind pro Hardwarefolge exklusiv
gesperrt. Einzelheiten: `port/docs/adb-coordination.md`.

## Warum der vollständige Leon weiterhin offen ist

Die tatsächliche Originalkette ist keine reine Modellladefunktion:

1. `src/game/game.cpp::gameRoomInit` initialisiert Room-Memory, Player-/Area-
   Daten, CNS/SMD und sämtliche gemeinsamen Managerpools. Im aktuellen
   OriginalContext ist dieser ganze Körper noch ungebunden.
2. `src/game/em.cpp::cEmMgr::construct` erzeugt für `id=0, pl_type=0` per
   Placement-New einen `cPlLeon`; Poolwork und vollständiger Klassentyp müssen
   zusammenpassen. `EM_WORK_SIZE` darf unter ARM64 keine Byte-Overlaybasis sein.
3. `src/game/player.cpp::cPlayer::cPlayer/init0` braucht originale Playerflags,
   native Motiontabellen (0x6D und 12 Pointer, keine GC-Bytegrößen), Debug-Work,
   `cPlWep`, `cPlBody`, `cPlPush`, `cPlWaist` und `cMotBase`.
4. `src/game/pl_leon.cpp::cPlLeon::cPlLeon` ruft vor `init1/startUp` die echte
   Weapon-/Objekt-/Effectkette auf. `src/game/pl_wep.cpp::weaponRelease` braucht
   den aktiven ObjMgr, Kamera- und Effect-Consumers; `weaponLoad` benutzt
   `src/game/read.cpp::ReadWepData` und anschließend `WeaponInitFunc`.
   Welche Waffentabelle ausgewählt wird, bestimmen die originalen
   `pl_type/weapon_no/weapon_type`, keine fest angenommene H1-Waffe.
5. Beispiel eines tatsächlichen Modulkonsumenten:
   `src/wep01/wep01.cpp::_prolog/Wep01_init` registriert Waffenroutinen und
   `ObjInitFunc[0x32]`, erzeugt ein echtes `cObjFn57`, installiert Motions und
   lädt Effects. Sein Typ benötigt den vollständigen Player-/Objektvertrag.
6. `src/game/player.cpp::startUp` führt nach Positionierung `move()`, zweimal
   `motionMove()` und `SatMgr.check(this, m_pSatMask)` aus. Nur Flags und eine
   vorberechnete Pose zu setzen würde diese Originalkette überspringen.

Diese noch fehlende gemeinsame Actor-/Objekt-/Weapon-/Effect-Verbindung ist
ein eigenständiger Engineblock. Es wurde kein teilweise konstruierter Leon
als vollständiger Actor publiziert und `startUp` nicht erfolgreich gestubbt.
Der nächste Implementierungsschritt bleibt innerhalb H1: vollständiger
nativer Player-/Objekttyp und die passenden Manager-/Waffenmodulverträge,
dann der Originalkonstruktor bis `startUp`; anschließend der Originalkörper
von `gameRoomInit` mit seiner tatsächlichen Reihenfolge. MotionHokan und
Sphere können dafür unmittelbar verwendet werden.

## Schnittstellen für spätere Spenderübernahmen

Hier ausschließlich Architekturverträge, keine implementierten Ersatzsysteme:

| Spender | Vertrag an die neue Engine |
|---|---|
| `sound_bank.cpp::DecodeDspAdpcm`, `sound_mixer.*` | Geprüfte immutable DSP-Blöcke mit Originalkoeffizienten, Loop-/Predictorzustand und IDs in PCM-Samples; Voicecommands aus originalem Snd-System, keine alternative Eventlogik. Mixer hält Sampleleases bis zur letzten Voice und trennt Audioclock von Gameplayticks. |
| `GameAudio.java` | Ausgabeinterface für PCM16-Stereo, Frames/Samplerate und Start/Stop/Drain. Ein dedizierter Audiothread darf keine Gameplaytasks ausführen; JNI-Pufferbesitz und Pause/Resume müssen explizit sein. AudioTrack ist technischer Spender, Vulkan/Scheduler bleiben bestehen. |
| `GameCubeDisc.java` | Android-SAF-Auswahl von zwei lesbaren Disc-URIs; unveränderliche Quellen, vorhandene FST-/Hash-/Importpipeline als Autorität, transaktionale Installation in privates Datenverzeichnis. Inventory/IDs und Sprachabhängigkeiten bleiben unverändert; keine pauschale Entfernung unbekannter Sprachressourcen. |
| `IngameSettings.java` / Grafikmenü | Backendneutraler gewünschter und effektiv validierter Zustand plus persistente Versionierung. MSAA Aus/2x/4x, AF 2x/4x/8x, DoF Nah-/Zielfokus; Vulkan-Caps bestimmen die zulässigen Renderziele/Sampler. Nicht unterstützte Optionen dürfen nicht als aktiv erscheinen. Menüs nutzen zentrale `PadRead`-Eingaben; kein zweites Gameplay-Keymapping. |

## Dateien und Git-Regeln

Vollständige geänderte Dateien liegen im Projekt; erzeugte C++-Kontrollkörper
und Manifeste sind aus den Quellen reproduzierbar im ignorierten Buildbaum.

* Fingerprints: `port/config/consumer-sources.json`, `player-sources.json`.
* Öffentliche Verträge: `port/include/re4/port/collision.h`, `consumers.h`,
  `motion.h`, `player.h`.
* Runtime: `port/src/collision.cpp`, `collision_original.h`, `consumers.cpp`,
  `model_original.h`, `model_runtime.cpp`, `motion.cpp`, `motion_original.h`,
  `player.cpp`, `player_original.h`.
* Erzeugung: `port/tools/generate_collision.py`, `generate_consumers.py`,
  `generate_player.py`.
* Prüfungen: `port/tests/game_runtime_tool.cpp`, `game_runtime_verification.cpp`,
  `model_verification.h`, `test_consumer_sources.py`, `test_runtime_sources.py`,
  `test_retroid_mutex.py`.
* Gemeinsamer Gerätevertrag: `port/scripts/Retroid-Adb-Exclusive.ps1`,
  `test-device.ps1`, `test-app-device.ps1`, `port/tools/retroid_adb.py`,
  `port/docs/adb-coordination.md` und die ADB-Anbindung in
  `test_collision.py`, `test_engine_loading.py`, `test_game_runtime.py`,
  `test_gx_scene.py`, `test_import_device.py`, `test_modules.py`,
  `test_resources.py`, `test_skinning.py`.
* Dieser Bericht: `port/docs/hybrid-h1-status.md`.

Die README von `origin/master` (`15cf64f80e08bbc64712c39be330b75d1512a0d0`)
wurde berücksichtigt. `master` bleibt
`9b76ff003081d6ece8b838453fb8310743a972c7`. Kein Merge, Rebase oder Push.
Originalimages und extrahierte Daten bleiben außerhalb von Git. Keine weitere
Hauptphase wurde begonnen. ARM32/ARM64, zentrale Controlleranbindung,
Vulkanpipeline und die verbindlichen späteren Grafikoptionen bleiben erhalten.

## Abschließende Ergebnisse

Testgerät: Retroid Pocket Flip2, Seriennummer `dbdcacc`, Android 13,
Adreno 650. Das abgefragte ABI-Angebot lautet
`arm64-v8a,armeabi-v7a,armeabi`. Die tatsächlich getesteten Prozesse waren
`armeabi-v7a` mit 32-Bit-Pointern und `arm64-v8a` mit 64-Bit-Pointern.

| Prüfung | Tatsächliches Ergebnis und Grenze |
|---|---|
| Windows x64 / x86 | Je 27/27 CTest-Gruppen bestanden, final 9,57 / 9,67 Sekunden. Native Laufzeit, Scheduler, Module, Loader, Ressourcen, Collision, Motion/IK/Skinning, GX/TEV sowie Source- und Importprüfungen. |
| Android-Build | NDK 28.2.13676358, CMake 4.1.2; beide ABIs, Debug-APK, Test-APK und Gradle-Lint offline erfolgreich. |
| Retroid native Tests | Je zweimal: ARM32 13.386 Prüfungen, ARM64 13.387 Prüfungen, jeweils null Fehler. Relevante Scheduler-/Background-/Yz2-Traces identisch. |
| Retroid APK | 20/20 Instrumentierungstests pro ABI; darin Original-Runtime 1.100 Prüfungen, null Fehler. Controller-/Lifecycle-/Surface-/Modulregressionen bestanden. Keine erneute manuelle Tastendrückprüfung in dieser Sitzung. |
| Originaldaten auf vier Architekturen | Je zweimal über beide Discs: sieben Leon-FCV/SEQ-Paare mit Hokan, Modelle aus weiteren EVDs, Message-/Font-/Inventar-/Map-/Checkpoint-/ID-/Lichtsystem, Archiv-/ESL-Vorbereitung und echte GameTask-Stageinitialisierung. Alle erwarteten Prüfungen bestanden, explizite Gameplayblocker bleiben Fehlergrenzen. |
| Identität der Datenläufe | Ausführbare SHA-256-Werte der Berichte entsprechen den finalen Binärdateien. Message-, Modell-, Licht-, Consumer- und GameTask-Semantikhashes sind zwischen x86/x64/ARM32/ARM64 und beiden Wiederholungen identisch. |
| Reale SAT-/EAT-Regression | r100/r102/r119/r120 auf Disc 1 und r300 auf Disc 2, je 6.965 Rayproben pro Architektur und Durchlauf, zweimal. Trefferattribute, Positionen und Normalen sind auf allen vier Architekturen identisch. Größter Abstand zum unabhängigen Python-Orakel: 0,015125 originale Koordinateneinheiten, unter der bestehenden 0,2-Grenze. Dies sind gerichtete Abfrageproben, keine vollständige Actorbewegung. |
| Shared Mutex | Zwei unabhängige PowerShell-Prozesse, Ausschluss bei Timeout und Freigabe nach Exception bestanden. Hardwarefolgen halten `Global\Retroid_ADB_Exclusive`; Journale erfassen Gerät, Besitz und ADB-Kommandos. |

Die zusätzliche ARM64-Prüfung ist der High-Address-Nachweis im originalen
Scheduler-Stacktest (`port/src/scheduler_verification.cpp`): Nur bei
`sizeof(void*)==8` wird ein Callbackpointer oberhalb von `UINT32_MAX` geprüft.
Die unterschiedliche Anzahl bezeichnet keine abweichende Spielsemantik.
Die Messageprüfungen zählen 390.194 im normalen und 390.203 im New-Game-Modus;
die realen Lichtprüfungen zählen 11.049 für Disc 1 und 8.399 für Disc 2.

### GPU-Regressionsnachweis, keine neue Spielszene

Der bestehende allgemeine r100-Diagnosepfad wurde auf dem Retroid unter beiden
ABIs zweimal ausgeführt: Raum 100, Original-EVD `Evd/r100s40.evd`, Cut 0,
Frame 30, pl0000 und `debug-view=1` (Texturansicht). Das ist ausdrücklich
keine von `cPlayer::move`, `startUp` und `gameMainLoop` erzeugte Spielerpose
oder Schulterkamera. Es ist kein Nachweis für die korrekte r100-Startszene.

Die vier GPU-Readbacks sind byteidentisch, mit 226.966 vom Clear verschiedenen
Pixeln und final 31/20 Geometrieframes unter ARM32 sowie 27/23 unter ARM64.
Diese durch die Laufdauer verschiedenen Zählwerte sind keine FPS-Messung.
OT-, TEV- und Lichtzustände
sind ebenfalls identisch. RGBA-SHA-256:
`7c2676d8361694ceb5f9db106b8c3d8bec3126caa032b96be519b4ceb1cf5cd6`.
Die mittlere Helligkeit beträgt 49,616669 bei codiertem RGB8; Median 48,1352,
5./95. Perzentil 9,0722/92,2848. Das sind keine photometrischen Messwerte und
keine Lösung der noch offenen Originalbeleuchtung.

Die echten GPU-Aufnahmen wurden visuell gelesen: Leon links im Vordergrund,
Brücke/Tor und Raumtexturen sind sichtbar. Die Texturdiagnose weist harte
Detail-/Alphamuster auf. Ein identischer Originalspielzustand als Referenz
wurde hier nicht nachgewiesen; Material-, Kamera- und Helligkeitsoriginaltreue
werden deshalb nicht behauptet. Der Renderer wurde in H1 nicht verändert.

Khronos-Validation 1.3.290 war aktiv: **null Fehler, eine Performancewarnung
pro Grafikinstanz**. `WARNING-Swapchain-PreTransform` betrifft
`IDENTITY` als Swapchain-preTransform gegenüber `ROTATE_90` als
Surface-currentTransform; der Präsentationsdienst übernimmt die Rotation.
Das ist dokumentiert und nicht als warnungsfreier Lauf ausgegeben.
Die vorhandenen APK-Clear-/Präsentationstests sind von den Geometrieaufnahmen
zu unterscheiden.

GPU-Bilder und JSON/RGBA-Nachweise liegen ausschließlich außerhalb von Git:

* [ARM32](E:/Development/Android/RE4-Data/hybrid-h1-final-gpu-r100/gpu-armeabi-v7a-1.png)
* [ARM64](E:/Development/Android/RE4-Data/hybrid-h1-final-gpu-r100/gpu-arm64-v8a-1.png)
* Verzeichnis: `E:\Development\Android\RE4-Data\hybrid-h1-final-gpu-r100`.

### Reproduktion und Nachweise

Hostbuilds wurden in den vorhandenen MSVC-x64- bzw. x86-Umgebungen mit
`cmake --build port/build/host-ninja --parallel 4` bzw.
`cmake --build port/build/host-x86 --parallel 4` erstellt. Danach:

```powershell
ctest --test-dir port/build/host-ninja --output-on-failure
ctest --test-dir port/build/host-x86 --output-on-failure
python port/tests/test_retroid_mutex.py
```

`python` bezeichnet hier den vorhandenen gebündelten Python-3.12.14-Interpreter,
`cmake/ctest` die SDK-CMake-4.1.2-Werkzeuge; MSVC 18 Community wird je ABI
über `VsDevCmd.bat` initialisiert. Android:

```powershell
.\port\scripts\build-android.ps1 `
  -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' `
  -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' `
  -Python 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' `
  -ValidationLayers 'E:\Development\Android\RE4-Fixed\port\build\validation\layers\android-binaries-1.3.290.0' `
  -Offline
```

Die PowerShell-Testskripte halten jeweils selbst den gemeinsamen Mutex.
Für eine kombinierte Native-/APK-Testfolge können sie auf demselben
PowerShell-Thread innerhalb `Invoke-RetroidAdbExclusive` aufgerufen werden:

```powershell
.\port\scripts\test-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
```

Die vorhandenen universellen Originaldatenrunners werden mit
`--data E:\Development\Android\RE4-Data\import-phase3a`, `--repeat 2`, einem
separaten `--output` und wahlweise `--host-exe` oder `--adb/--serial dbdcacc`
ausgeführt: `test_game_runtime.py`, `test_collision.py`, `test_gx_scene.py`.
Python-Hardwareläufe erwerben automatisch ihren Brokerlock; nicht durch
einen separat sperrenden Elternprozess umschließen. GPU-Parameter wie oben.

Reproduzierbare private JSON-Berichte: `hybrid-h1-host64`, `hybrid-h1-host32`,
`hybrid-h1-device`, `hybrid-h1-collision-host64`, `hybrid-h1-collision-host32`,
`hybrid-h1-collision-device` und das GPU-Verzeichnis, jeweils unter RE4-Data.
Build-/Testlogs im ignorierten `port/build`:
`h1-build-x64.log`, `h1-build-x86.log`, `h1-tests-x64.log`, `h1-tests-x86.log`,
`h1-android-build.log`, `h1-core-device.log`, `h1-app-device.log`,
`h1-original-x64.log`, `h1-original-x86.log`, `h1-original-device.log`,
`h1-collision-x64.log`, `h1-collision-x86.log`, `h1-collision-device.log`,
`h1-gpu-device.log`, `h1-mutex-test.log`, `h1-validation-logcat.txt`.

Zwischenstände scheiterten außerdem an einem falsch normierten
Source-Fingerprint, einem fehlenden Testheader, einem unvollständigen
`Asset`-Typ beim Inlinekonstruktor und zwei veralteten Manifestanzahlprüfungen.
Diese wurden korrigiert. Die anfänglich fehlerhafte analytische
Rotationsfixture wurde auf die tatsächliche 90-Grad-Matrix korrigiert;
der Originalalgorithmus wurde dafür nicht verändert. Die finalen Testläufe
sind erfolgreich. Es gibt weiterhin fachlich offene Actor-/Gameplayconsumer,
die kein bestandener Test verschweigt.
