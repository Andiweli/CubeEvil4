# Hybrid H1: ReadWepData, native Waffenmodule und Effect-Ressourcen

Ausgangspunkt: `ea3c145e60049dff88c814a98571a595f20a94cf`, Branch `android-port`.
Tatsächliche Prüfungen am 12. Oktober 2026. Dieser Bericht ergänzt die bisherigen
H1-Berichte; deren frühere Blocker bleiben historische Nachweise.

## Ergebnis und Abnahme

**Geprüfter Teilstand. H1 ist nicht abgeschlossen.** Der originale
`ReadWepData`-Funktionskörper läuft mit echten importierten Waffenarchiven.
Die native Modulverwaltung führt die originalen Prologe von `wep01` und `wep02`
aus. Die registrierten konkreten Waffenobjekte führen über nativen virtuellen
Dispatch ihre originalen `init`- und `setMotion`-Körper aus. Auch die benötigte
CPU-seitige Effect-Datenregistrierung funktioniert.

Die erhaltene Leon-Konstruktionsfolge erreicht zusätzlich den Originalkörper
von `cPlayer::init1`: Neck-Konstruktion, Licht-/Atari-Einrichtung, fünf verkettete
Yarare-Hitboxen, Spiegelindizes und `partsWorldCalc`. Sie stoppt ausdrücklich bei:

```text
Original consumer unavailable: PlClothSetLeon/PenClothSet native cloth owners, joints and collision volumes
```

Es wird weiterhin **kein vollständiges C++-Objekt `cPlLeon` über EmMgr erzeugt
oder veröffentlicht**. Die Konstruktorbodies laufen auf den bestehenden
getrennten nativen Feld-/Modellbesitzern. Das ist ein isolierter Originalcode-
Konstruktorprobe, keine Ausführung innerhalb des vollständigen `gameRoomInit`.
`cPlayer::startUp()` und vollständige Player-Ticks wurden nicht ausgeführt.

| H1-Abnahmekriterium | Tatsächlich nachgewiesen |
| --- | --- |
| Vollständiger originaler cPlLeon-Actor | Nein; EmMgr-Typ-/Virtual-Dispatch noch offen |
| Originales startUp erfolgreich | Nein |
| Mehrere originale Player-/Motion-/Kollisionsticks | Keine Player-Ticks; vorhandene isolierte Motion-/SEQ- und Kollisionsprüfungen bestehen |
| Wiederholte Konstruktion/Freigabe | Ja für den ausgeführten Konstruktorpräfix, Waffenobjekte, Modelle, Module und Effectregistrierungen; nicht für einen vollständigen Actor |
| ARM32 und ARM64 | Dieser Teilstand tatsächlich auf beiden ABIs geprüft |

## Ausgeführte Originalfunktionen

Die Generatoren übernehmen vollständige Funktionskörper mit dokumentierten
Typ-/Besitzanpassungen und geprüften Originalquell-Fingerprints. Generierte
Manifeste unter `port/build/*/original` weisen Quelle, Signatur, Zeile und
Adaptierungen aus. Originaldateien unter `src/` und `include/` wurden nicht geändert.

* `src/game/read.cpp:802`: `ReadWepData`; Charaktertabellen, Varianten,
  Cachevergleich, ursprüngliche Fehlergrenzen, `ReadCheck`-/`TaskSleep`-Schleife,
  eingebettete REL-Auswertung und MODULE_DAT-Zustände. `ReleaseWepData` und
  `InitModule` bleiben die vorhandenen Originalkörper.
* `src/wep01/wep01.cpp`: `_prolog`, `_epilog`, `Wep01_init`.
* `src/wep02/wep02.cpp`: `_prolog`, `_epilog`, `Wep02_init`.
* `src/wep01/objFn57.cpp`, `src/wep/objRuger.cpp`,
  `src/wep02/objMauser.cpp`: originale Placement-Konstruktoren, virtuelle
  `init`-/`setMotion`-Körper und Schussframe-Tabellen. Der gemeinsame Ruger-
  Quellpfad ist die vom Original-Modulbuild verwendete Fassung.
* `src/game/objWep.cpp`: `cObjWep::resetMotion`; tatsächliche Model-/Motion-
  Initialisierung und originale Wahl zwischen normaler und leerer Magazinpose.
* `include/pl_wep.h`: `setAbility`, `setParent`, `setStat`;
  `src/game/pl_wep.cpp`: `cPlayer::weaponInit` zusätzlich zu den vorhandenen
  `weaponRelease`-/`weaponLoad`-Körpern.
* `include/pl_body.h`: `cPlBody::initWepHand` mit nativ breitem Ressourcenargument.
* `src/game/item.cpp`: alle vier `bulletNum`-Überladungen und `num(int)`.
* `src/game/eff_sys.cpp`: `EspDataLoad`, `EspDataRelease`, `EspPullTexObj`,
  `espTexRegist`, `estRegist`, `sstRegist`, `pathRegist`, `efmRegist`, die fünf
  Release-Körper sowie `GetTexObjFlag`/`SetTexObjFlag`.
* `src/game/player.cpp:230`: `init1` bis zum Cloth-Aufruf;
  `src/game/pl_class.cpp:1487`: Neck-Konstruktor;
  `src/game/at_mod.cpp`: `yarareInit0`, `YarareInit`, `YarareAdd`;
  `include/model.h`: `SetLightIgnore`.

`WeaponMoveFunc` wird vom Originalprolog registriert. **`PlHandgunMove` und
die konkreten Waffen-Move-/Fire-/Reload-Pfade sind noch nicht ausgeführt oder
vollständig portiert.** Ihre Grenzen werfen explizite Missing-Consumer-Fehler.
Die Registrierung eines Zeigers wird nicht als erfolgreiche Bewegung ausgegeben.

## Originaldaten und nachgewiesene Waffenbindung

Quelle bleiben ausschließlich die unveränderten G4BE08-Disc-Importe unter
`E:\Development\Android\RE4-Data\import-phase3a`. Es werden weder neue
Waffenmodelle noch Gameplaydaten erzeugt.

| Isolierte Ausrüstungseingabe | Originalarchiv | Natives Originalmodul | Konkretes Objekt | Teile | Motiontabelleneinträge |
| --- | --- | --- | --- | --- | --- |
| weapon_no=1, type=0 | em/wep01.drs | wep01, ID 3 | cObjFn57 / 0x32 | 4 | 26 |
| weapon_no=2, type=0 | em/wep02.drs | wep02, ID 4 | cObjRuger / 0x21 | 4 | 25 |
| weapon_no=3, type=0 | em/wep03.drs | wep02, ID 4 | cObjMauser / 0x27 | 7 | 25 |
| weapon_no=2, type=1 | em/wep02.drs | wep02, ID 4 | cObjRuger / 0x21 | 4 | 25 |

Die Zuordnung von `wep03`-Daten zu `wep02`-Code stammt aus der Originaltabelle
und der eingebetteten REL. Es gibt keine Registrierung eines erfundenen
`wep03`-Codemoduls. Andere Waffenmodule sind weiterhin ausdrücklich ungebunden.

Je CLI-Start werden die fünf Ausrüstungsfälle `2/0 → 1/0 → 3/0 → 2/1 → 2/0`
jeweils zweimal konstruiert und zurückgerollt. Die Eingaben sind ausdrücklich
isolierte Testparameter, keine künstlich erzeugten Start-/Save-Zustände in GameTask.
Vor jeder Konstruktion läuft das originale `PlayerInit`. Die vorherigen
Inventarprüfungen werden durch originale ItemMgr-/SubScreen-Initialisierung
abgeschlossen; die Konstruktorprobe behauptet keine neue Spielstart-Inventarausführung.

Geprüft sind Datei-/Body-/BIN-/TPL-Hashes, Item-IDs, alle Motion-Indizes und
Quellarchiv-Slots, Parent-Gelenk 10, tatsächliche Effect-IDs und Patternzahlen.
Die unabhängigen Python-Orakel lesen Originalquellen und BE-Daten. Leons
Modellwurzel besitzt dabei 119 Gelenke und acht ModelInfo-Anbindungen.

## Native Adapter und Besitzer

`WeaponRuntime` besitzt Ladeanforderungen, MRAM-Puffer, Datei-/Sound-Leases,
geprüfte Archivansichten und den nativen REL-Metadatensatz. GC-Pointer werden
nicht als native Pointer verwendet. BE-Offsets werden explizit aufgelöst.
`NativeGameState::pWep` liegt außerhalb des pointerfreien Save-Blocks.

Die Originalgrenzen `WEP_DATA_MAX=0x70000` und `DLL_BSS_MAX=0x80` bleiben
unverändert. Native Puffer werden erst nach dem Original-Cachevergleich und
der alten Ressourcenfreigabe angelegt. Keine Allocation entsteht im Cache-
Frühreturn. Andere Charaktertabellen werden erhalten; daraus folgt keine
Behauptung, dass deren gesamte Modulabhängigkeiten bereits verfügbar sind.

Die vorhandene Dvd-/Yz2-Laufzeit lädt asynchron. Ihr nativer Adapter lässt die
originale `ReadCheck`-/`TaskSleep`-Schleife warten; er blockiert den Frame-Owner
nicht im alten synchronen DVD-Treiber. `ModuleRuntime` aktiviert den Prolog
einmal an der vorgesehenen Frame-Grenze. Native Initialisierungscallbacks
prüfen Runtime, Aktivierungsepoche und Modulphase. Konkrete Objekt-Vtables
pinnen ihren Modulbesitzer; Stop/Unload mit lebenden Objekten wird verweigert.
Waffenmodelle pinnen zusätzlich den tatsächlichen Player-Modellbesitzer.

Ein zunächst nachgewiesener Adapterfehler wurde behoben: die verschachtelte
Soundbank wurde fälschlich in `read_size[0][1]` und den Waffen-MRAM-Puffer
gerechnet. `src/game/dvd.cpp::readMain` schreibt `sizeTbl` nur für
TRANS_MRAM/TRANS_ARAM; TRANS_NEST verarbeitet die Soundbank auf Tiefe 1.
Die korrekte Root-Größenmeldung zählt nur den Body. Die vollständige Soundbank
bleibt separat unter ihrem geprüften Datei-Lease erhalten. Bodygrößen sind
261344 / 276960 / 296768 Bytes, Soundcontainergrößen 175168 / 164896 / 229312
Bytes für wep01 / wep02 / wep03.

**Die originalen Sound-Transfers, `SndBlkInit`, DSP-Stimmen und Audioausgabe
sind dadurch nicht implementiert.** Dies ist eine dokumentierte Daten-/Modul-
Anbindung, keine vollständige Emulation sämtlicher DVD-Audio-Seiteneffekte.
`SndStop` eines tatsächlich aktiven Waffen-SE-Handles bleibt eine explizite
Grenze; der geprüfte erste Reset besitzt noch keinen solchen Handle.

Effect-Daten erhalten native, kontrollierte Ansichten von EFF-Version 11,
ID-/Offsettabellen, TPL, ANM, EST/SST/Pfaden und EFM-Ressourcen. Null-Offsets
leerer Tabellen stammen aus den Originaldaten. Bestehende TPL-/Palette- und
GX-Textur-CPU-Zustandsadapter werden gemeinsam verwendet. Kein GPU-Effect-
Spawning oder kompletter `EspRoomInit` wird behauptet. Originale Zählung
(einschließlich Duplicate-Fehlerzählung), Freigabe und TexObj-Poolalgorithmen
werden ausgeführt. Bei Konstruktorabbruch werden genau die neu erworbenen
Effect-Referenzen freigegeben, anschließend Modelle, Modul und Puffer.

## Tatsächlich ausgeführte Prüfungen

| Umgebung | Ergebnis |
| --- | --- |
| Windows x64 / x86 | Beide Builds erfolgreich; jeweils 27/27 CTest-Prüfungen |
| Originaldaten x64 / x86 | Beide Discs, normal/new-game-Eingabe, zwei Wiederholungen; acht Start-CLI-Prüfungen pro Architektur |
| Retroid Pocket Flip2, dbdcacc, Android 13 / API 33 | Tatsächliche ARM32- und ARM64-Prozesse; dieselben Originaldatenfälle und Wiederholungen |
| Native Retroid-Basistests | Je ABI zwei Läufe; ARM32 13386, ARM64 13387 Checks, jeweils null Fehler |
| Debug-APK / Instrumentation | APKs gebaut, Lint bestanden; jeweils 20/20 Tests unter ARM32/ARM64 |
| SAT/EAT-Originaldaten | x86/x64 und beide Android-ABIs, zwei Wiederholungen, r100/r102/r119/r120/r300 |
| Vulkan-Geometrieregession | Beide Android-ABIs, je zwei GPU-Läufe; OT-/TEV-/Lichtzustände und Readbacks bytegleich |

Der normale Originaldaten-Starttest umfasst auf allen vier Architekturen
390377 Checks. Diese Zahl umfasst auch frühere Systeme, nicht nur neue Waffenprüfungen.
Die ARM64-Basisprüfung enthält einen zusätzlichen Nachweis von Pointerbits
oberhalb UINT32_MAX (`port/src/scheduler_verification.cpp:96`); die eine
Prüfzahldifferenz ist keine abweichende Gameplaysequenz. Relevante
Originaldaten-Traces stimmen über x86/x64/ARM32/ARM64 überein.

Zusätzlich geprüft: Quellen-Fingerprints, deterministische Generierung,
Ablehnung geänderter Originalquellen, kaputte EFF-Offsets/Dateigrenzen,
Original-Ownerzählung und erneute Freigabe, veraltete NativeCallToken,
Modulstop/-unload mit gepinntem Code, heap-/modellfreie Konstruktorrollbacks.
Vorhandene Controller-/PadRead-, Scheduler-, Ressourcen-, Modell-, Motion-/SEQ-,
Modul- und Event-Regressionen bestehen. Die Controllerprüfung umfasst die
vorhandenen automatisierten Tests; neue physische Tastendrücke wurden nicht angefordert.

Khronos-Validation war auf dem Retroid aktiviert: **null Fehler, eine bekannte
Performance-Warnung pro GPU-Prozess**. `WARNING-Swapchain-PreTransform`
betrifft IDENTITY statt dem gemeldeten ROTATE_90-Surface-Transform; die
Presentation-Engine führt die Rotation aus. Eine Darstellung ohne Warnungen
wird nicht behauptet. Vier Readbacks besitzen SHA256
`b46d87280a1701af2992f04a55b1aed31a409dbc293313c7cec4eabff02ae3bf`.
Die GPU-Aufnahmen prüfen den bestehenden isolierten r120-Renderpfad. Sie sind
**keine Bilder einer neuen laufenden Leon-/GameTask-Spielszene**.

Alle Hardwaresequenzen verwenden den bestehenden PowerShell-Mutex
`Global\Retroid_ADB_Exclusive`, Timeout 60000 ms, explizit `adb -s dbdcacc`
und zuverlässige Freigabe. Brokerjournale unter `port/build/adb-coordination`
belegen Erwerb und Exit-Code 0 der Freigabe. Kein Server-Kill, Neustart oder
globale Bereinigung wurde ausgeführt.

Private Originaldatenberichte liegen unter `RE4-Data\h1-weapons-host64`,
`h1-weapons-host32`, `h1-weapons-retroid`, den drei
`h1-weapons-collision-*`-Verzeichnissen und `h1-weapons-gpu-retroid`.
Build-/CTest-/App-/Validation-Logs liegen unter `port/build/h1-wep-*`.
Keine dieser Daten oder GPU-Aufnahmen wird in Git übernommen.

Die anfänglichen Fehlprüfungen waren real: falsche Test-Cachevoraussetzung,
fehlende ItemMgr-Voraussetzung, Soundcontainer-Größenfehler, Compiler-/Matrix-
Signaturen und die alte erwartete Funktionszahl 40 statt 45. Sie wurden
behoben und die betroffenen vollständigen Prüfungen erneut erfolgreich ausgeführt.

## Verbleibende gemeinsame Verbraucher

1. **Cloth:** `src/game/pl_cloth.cpp::PlClothSetLeon` ruft Jacket-/Holster-/Hair-
   Setup auf. `src/game/pendulum.cpp:128::PenClothSet` benötigt natives
   `ModelPart::Pen`, originale CLOTH_INFO/PEN_INFO-Besitzer und Tabellen,
   Restlängen, Nachbarschaft und Kollisionsvolumen. Spätere Player-Ticks
   benötigen außerdem PenClothMove/PenClothMove3. Dies wird nicht mit einem
   erfolgreichen leeren `initCloth` übersprungen.
2. **Rest von init1:** originale Body-Morph-Einrichtung `makeSpaeData` und
   ATTACH_CAMERA-Allokation/Besitz. Ihre Bodies wurden noch nicht ausgeführt.
3. **Vollständiger Actor:** native gemeinsame Work-/Actor-Pools und die
   Originalkette `EmMgr.create → cEmMgr::construct` (`src/game/em.cpp:76`)
   mit echter cEm/cPlayer/cPlLeon-Klassen-/Virtual-Dispatch-Anbindung.
4. **startUp/Player-Ticks:** `src/game/player.cpp:277/294` benötigt die
   originale virtuelle Move-/Routineausführung, WeaponMove, Damage-/Action-
   Verbraucher, Body/Neck/Cloth-Updates, Actor-SAT-Prüfungen, Kamera und
   nötige Audio-/Effect-Aufrufe. Bereits vorhandene MotionMove-/SEQ- und
   SAT-Berechnungen ersetzen diese Actor-Laufzeit nicht.
5. **gameRoomInit:** tatsächlicher GameTask erreicht weiterhin Rno0=2.
   `port/src/runtime_original.h` besitzt weiterhin den expliziten Fehler
   `gameRoomInit/model/light/player`. Originalquelle `src/game/game.cpp:354`
   verlangt zuerst Sound-/Raumheapinitialisierung, Player-/Area-Laden,
   CNS-/SMD-/Managerpools einschließlich EmMgr, vollständiges EspRoomInit,
   Ctrl-/Shadow-/Damage-/Filterverbraucher, SAT/EAT und Szenario-/Event-
   Registrierung. Erst danach folgt EmMgr.create und die Kamera-/Block-/
   Szenarioanbindung. Teilfunktionen sind bereits vorhanden; deren
   zusammenhängende Ausführung wird hier nicht behauptet.

Der nächste konkrete H1-Schritt ist die gemeinsame Cloth-/Body-/Camera-
Besitzkette zusammen mit dem echten EmMgr-Actor-Dispatch; anschließend
`startUp` und mehrere originale Idle-/PadRead-/Motion-/Kollisionsticks.
Keine nächste Hybridphase wurde begonnen.

## Vollständig gespeicherte Projektdateien

Neu:

* `port/config/weapon-sources.json`
* `port/include/re4/port/weapons.h`
* `port/src/weapon_original.h`, `port/src/weapons.cpp`
* `port/src/effect_data_original.h`, `port/src/effect_data.cpp`
* `port/src/message_texture_original.h`
* `port/tools/generate_weapons.py`
* `port/docs/hybrid-h1-weapons-status.md`

Geändert:

* `port/CMakeLists.txt`, `port/config/player-sources.json`
* `port/include/re4/port/{effects,modules,objects,player,subscreen}.h`
* `port/src/{effect_original,effects,game_runtime,message_original,modules,object_original,objects,player,player_original,subscreen,subscreen_original}` mit ihren vorhandenen `.h`-/`.cpp`-Endungen
* `port/tests/{game_runtime_tool,module_verification}.cpp`
* `port/tests/{test_consumer_sources,test_runtime_sources}.py`
* `port/tools/{generate_objects,generate_player,generate_runtime,generate_subscreen,test_game_runtime}.py`

Das Spenderprojekt `E:\Development\Android\RE4\RE4-Android-Decomp` wurde
ausschließlich gelesen. Es gab dort keine Builds oder Änderungen. Die README-
Aktualisierung auf origin/master (`15cf64f`) bleibt berücksichtigt, ohne Merge.
`master` bleibt auf `9b76ff003081d6ece8b838453fb8310743a972c7`.
MSAA Aus/2x/4x, AF 2x/4x/8x, DoF mit Nah-/Zielfokus und persistente,
hardwarevalidierte Einstellungen bleiben gemäß `graphics-settings.md` verbindlich.
