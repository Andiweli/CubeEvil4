# Phase 4c: originale PlayerInit-Kette und Leon-Ressourcen

Fortsetzung am 10. Oktober 2026 auf `android-port`, Ausgangscommit
`c1e4eecb7e7d1a63aed473c0b0eefb537a79d5ed`.

## Ergebnis und Ausführungsgrenze

**Der originale `PlayerInit()` wird vollständig im originalen GameTask
ausgeführt.** Der gemessene nächste Abbruch lautet
`Original consumer unavailable: MerchantGameInit()` (`src/game/game.cpp:271`).
`Rno0` bleibt 0. Der GameTask wurde nicht künstlich in die Raum- oder
Gameplayphase geschaltet.

`PlayerInit` und die Erstellung eines Spieleractors sind im Original getrennt:

```text
GameTask -> gameInit
  -> bisherige Message/SubScreen/Cockpit/ID/Licht-Initialisierung
  -> ScenarioInit (bestehender nativer Adapter)
  -> PlayerInit (jetzt vollständiger Originalkörper)
       -> PlayerLifeReset -> U16Set2
       -> DbgFlagOff(DBG_PL_LOCK_FOLLOW)
       -> ReleaseWepData -> InitModule, sofern weapon_no_old != 0xFF
       -> pl_flag = 1, PlKaiou = 0
  -> ashley_life = 600
  -> optional ItemMgr.gameInit / SceAtInitSaveItem bei SYS_NEW_GAME
  -> SysFlagOff(SYS_DOOR_AFTER)
  -> MerchantGameInit: tatsächlicher nächster Blocker
```

Der geprüfte vorhandene Bootpfad setzt `SYS_NEW_GAME` nicht. Bei gesetztem
Flag bleibt zusätzlich `ItemMgr.gameInit()` offen (`game.cpp:267`); dessen
Überspringen wird nicht behauptet. `ItemMgr::init/get` aus der bisherigen
SubScreen-Integration sind andere Funktionen als die komplette Startinventar-
Initialisierung.

**Ein vollständiger `cPlayer`-/`cPlLeon`-Actor existiert weiterhin nicht.**
Die neue CPU-Ressourcenanbindung führt die originalen Leon-Modell- und
Motionhelfer getrennt aus, veröffentlicht dabei aber keinen Ersatz-`pPL`.
Sie ist eine gemeinsam verwendbare Grundlage für die spätere tatsächliche
Actor-Konstruktion, kein simulierter spielbarer Leon.

## Integrierte Originalfunktionen

`generate_player.py` erzeugt vollständige Funktionskörper aus den geprüften
Quellen. `player-sources.json` schützt elf Originaldateien mit normalisierten
SHA-256-Werten. Ein abweichender Quelltext lässt die Generierung scheitern.
`player-manifest.json` im jeweiligen Build dokumentiert 17 Funktionskörper
und ihre Anpassungen. Dazu kommt `cModel::deleteModelInfo` in der bestehenden
Consumer-Generierung; deren Umfang steigt von 87 auf 88 Originalfunktionen.

| Originalquelle / Funktion | Tatsächliche Ausführung |
|---|---|
| `src/game/player.cpp:107`, `PlayerInit` | Vollständig im GameTask und in isolierten Zustandsprüfungen |
| `player.cpp:162`, `PlayerLifeReset`, dazu `U16Set2` | Alle sechs Spielerarten, unbekannter Typ und Debug-Overrides geprüft |
| `src/game/read.cpp:718`, `ReleaseWepData` | Originale `0xFF`-Bedingung und Wiederholung geprüft |
| `read.cpp:232`, `InitModule` | Leerer Startslot sowie Freigabe nativer eigener Daten mit beiden malloc-Flagvarianten geprüft |
| `src/game/pl_leon.cpp:71`, `cPlLeon::setModel` | Vollständiger Körper mit echten pl00-Daten, Standardkostüm, außerhalb der noch fehlenden Actor-Konstruktion |
| `pl_leon.cpp:44`, `cPlLeon::setMotion` | Vollständiger Körper; 14 originale Tabelleneinträge aufgelöst |
| `pl_leon.cpp:156/190`, `setRightHand/setLeftHand` | Startauswahl, Wechsel und Rückkehr zur vorherigen linken Hand geprüft |
| `pl_leon.cpp:239`, `setFace` | Originaler Startfall 0, einschließlich `ShapeEnd` |
| `src/game/pl_body.cpp:8`, `cPlBody::cPlBody`; `include/pl_body.h`, `setKnife` | CPU-Body-Helfer und originale unsichtbare Messermatrix geprüft |
| `src/game/shape.cpp:87/97`, `ShapeEnd/SetOriginalShape` | Neutralzustand und alle fünf nativen Shape-Kanäle geprüft |
| `src/game/pl_class.cpp:958/971`, `keyConfig/keyConfigTypeA` | Echte Originalkörper als isolierter Consumer-Test desselben KEY-Vertrags |
| `src/game/model.cpp:859`, `cModel::deleteModelInfo` | Entfernen/Freigeben beim originalen Handwechsel |
| `src/game/motion.cpp:134`, `MotionSetCore` | Vorhandener Originalkörper jetzt zusätzlich mit geprüften nativen SEQ-Referenzen |

`cPlLeon::setWound` ist ebenfalls vollständig generiert. Sein Verletzungszweig
wird beim geprüften Standardstart nicht ausgeführt. Andere Kostümarchive und
der verletzte Leon werden deshalb nicht als neu nachgewiesen aufgeführt.

## Originale Daten und Modellzustände

Die neue Prüfung fordert `em/pl00.drs` über die bestehende Engine-Ladeschnittstelle
mit dem original belegten Modus `0x8100` an. Die vorhandene asynchrone DVD-/Yz2-
Laufzeit liefert den Archivkörper. Die Diagnose verwendet keine direkte
Dateilesefunktion als Ersatz für diesen Ladevorgang. Die vollständige
`ReadPlayerData`-Funktion selbst ist noch nicht portiert.

Beide Disc-Inventare werden separat verwendet. Der gemessene pl00-Körperhash
ist auf beiden Discs
`7171022d741e11d8d06f1cbe522ca33cbe07e897379d336343e1b2420bc3e583`.
Die folgenden Zuordnungen stammen aus `cPlLeon::setModel`, nicht aus einer
nachgebauten Spielerbeschreibung:

| Funktion des Modells | BIN-Slot | TPL-Slot |
|---|---:|---:|
| Hauptmodell | `0x04` | `0x05` |
| Kostümzusatz im Standardstart | `0x0A` | `0x05` |
| Messer | `0x0D` | `0x05` |
| Face-/Kopfmodell | `0x08` | `0x07` |
| Zusatzmodell `Body->m_pHead` | `0x06` | `0x07` |
| Zusatzmodell `Body->m_pHair`, originales Flag `0x40` | `0x09` | `0x07` |
| Rechte Hand, Auswahl 0 | `0x12` | `0x11` |
| Linke Hand, Auswahl 1 | `0x15` | `0x11` |

Das Hauptmodell enthält nach den tatsächlichen BIN-Daten **119 Gelenke**.
Alle Elternreferenzen, acht Modellinfos, BIN-/TPL-Hashes, Texturanzahlen,
die Messermatrix und der neutrale Face-Zustand werden geprüft. Es wurden keine
Gelenke anhand einer angenommenen Leon-Struktur ergänzt oder entfernt.

`setMotion` registriert die sieben FCV-/SEQ-Paare aus `0x32` bis `0x3F` in
`m_MotTbl[0x5F..0x6C]`. Die letzte Vierergruppe wird in der originalen
Reihenfolge `0x6B,0x6C,0x67,0x68,0x69,0x6A` geschrieben. Der Bericht enthält
jeden Originalindex, Slot und Hash. Die separate FCV-/SEQ-Initialisierung
ruft `MotionSetCore` am ersten, mittleren und letzten Sequenzframe auf.
Sequenzanzahl, Flags, sämtliche Schlüssel und initiale Originalzustände
werden mit `tools/motion/fcv.py` verglichen. **Das beweist keine vollständige
per-frame Spieleranimation oder Ausführung seiner Bewegungsroutinen.**

Bestehende raumübergreifende Prüfungen bleiben erhalten: alle **37** BIN-
Vorkommen in `r120s00.evd/r120s01.evd`, zusätzliche Modelle aus
`r100s00.evd` und `r317s15.evd`, originale Eventregistrierung und bisherige
FCV-/Hermite-Verifikation. Die neue Anbindung baut keinen eigenen Raum nach.

## Native Adapter und Speicherlebenszeiten

- `PlayerRuntime` verwendet denselben `NativeGameState`, denselben zentralen
  `KEY` und dieselbe Pad-Konfiguration wie GameTask/PadRead. Referenzen bleiben
  beim typed Reset stabil. Es gibt keine zusätzliche Android-Tastenbelegung
  und keine neue Bewegungslogik.
- Die ergänzten globalen Skalarfelder werden mit ihren Originaltypen aus
  `include/global.h` generiert. Ein `GLOBAL_WK`-Byteoverlay wird nicht eingeführt.
- PL_ARC-Zugriffe behalten den Originalslot bei. Die bestehende Archivprüfung
  löst BE-Dateioffsets in begrenzte `ResourceLease`-Spannen auf. Originalbytes
  werden nicht umgeschrieben. BIN/TPL und FCV/SEQ werden vor Verwendung geprüft.
- Die Motiontabelle umfasst nativ 109 Pointer, abgeleitet aus den originalen
  `0x1B4` Bytes bei vier Byte pro GC-Pointer. Ein ARM64-Pointer wird nicht in
  einem vier Byte großen Slot abgelegt. Die zusätzliche 12-Einträge-Tabelle
  des vollständigen `cPlayer`-Konstruktors gehört weiterhin zum offenen Actorpfad.
- Handauswahlparameter und gespeicherte Auswahlwerte werden im nativen Helfer
  auf `intptr_t/uintptr_t` erweitert: Das Original erlaubt dort neben kleinen
  Selektoren auch Modelladressen. Der Adapter akzeptiert nur auflösbare eigene
  Archiveinträge. Getestet wurden die Startselektoren und Rückkehr `0x63`;
  beliebige externe Pointer sind kein unterstützter Importpfad.
- `NativePlayerModule` kopiert benannte MODULE_DAT-Felder mit nativen Pointern.
  `InitModule` behält seine originale Reihenfolge und Bedingungen. Typed Reset
  ersetzt das Byte-Löschen einer pointerhaltigen GC-Struktur. Eigene Heap-
  Allokationen werden tatsächlich freigegeben; fremde Pointer werden abgewiesen.
  `DLL_Unlink` für einen nichtleeren Waffen-REL-Slot bleibt eine ausdrückliche
  fehlende Abhängigkeit und meldet keinen Erfolg. Reservierte `pBss[32]`-Wörter
  sind keine Implementierung nativer Waffen-BSS oder Waffenprologe.
- Modell-/Motion-/Sequenzbesitz wird vor Veröffentlichung der Referenzen
  gebunden. Handwechsel nutzt originale Listen-/Poolfunktionen. Vollständiges
  Reset und Destruktion quieszen Tasks und lösen Playerressourcen vor den
  gemeinsamen Consumer-Pools. Raumretirement löst Playerassets vor deren
  Consumer-Speicher. Freigabe gebundener Playerassets mit lebenden Taskstacks
  wird abgewiesen. Leere Playerassets verhindern vorhandene Modulwechsel nicht.
- `ConsumerHandle` und Epochen schützen Zugriffe nach Release/Reset. Retired
  ResourceLeases halten Originalbytes weiter am Leben. Aufrufer müssen den
  Besitzvertrag beachten: Player-eigene Modelle werden über `PlayerRuntime`
  freigegeben, nicht unabhängig durch direkte Löschung im Consumer-Pool.

Der neue SEQ-Adapter übernimmt die belegte Struktur aus `MotionSetCore` und
`fcv.parse_seq`: BE `u16 count`, `u8 flags/pad`, vier Byte je Schlüssel und
einheitliches `0x00`-/`0xCD`-Padding unter 32 Byte. Zero-count-SEQ wird vor
Ausführung abgewiesen, da das Original anschließend `count-1` und Schlüssel 0
verwendet. Header-/Bereichsfehler werden nicht in eine leere Animation verwandelt.

## Tatsächlich ausgeführte Verifikation

| Plattform | Host-CTest | Runtime-Selbsttest | Privater GameTask-/Leon-Boot |
|---|---:|---:|---:|
| Windows x86, MSVC | 25/25 | 927 Prüfungen | 4 Läufe, je 2277 Prüfungen |
| Windows x64, MSVC | 25/25 | 927 Prüfungen | 4 Läufe, je 2277 Prüfungen |
| Retroid ARM32, `armeabi-v7a` | native CLI/APK | 927 Prüfungen | 4 Läufe, je 2277 Prüfungen |
| Retroid ARM64, `arm64-v8a` | native CLI/APK | 927 Prüfungen | 4 Läufe, je 2277 Prüfungen |

Private Bootläufe: jeweils zwei Wiederholungen für jede der beiden Discs.
Pro Plattform: vier echte GameTask-PlayerInit-Aufrufe, zwölf isolierte originale
Leon-`setModel`/`setMotion`-Aufrufe inklusive Reset-Prüfung und 168 zusätzliche
FCV-/SEQ-`MotionSetCore`-Aufrufe. Die bisherigen 180 Event-/Raummodell-
Initialisierungen, 48 Inventar-/Map-Modellinitialisierungen und Motionprüfungen
bleiben gesondert erfasst.

Die 927 Runtime-Prüfungen enthalten 56 Kombinationen aus Spielerart und Flags:
originale Lebenswerte, Reihenfolge der ST2-/ST3-Overrides, Ausrüstungsreset,
Erhalt unbekannter Charakterfelder, präzises Debugflag-Löschen, unveränderte
andere Statusflags, echte Speicherfreigabe und die originale `0xFF`-Bedingung.
Die KEY-Prüfungen führen tatsächlichen Originalcode auf isolierten PadRead-
Ausgangszuständen aus. Sie werden nicht als neue physische Tastendrücke gewertet.

Retroid: per ADB als `Retroid Pocket Flip2`, Seriennummer `dbdcacc`,
Android **13** erkannt; unterstützte ABIs `arm64-v8a,armeabi-v7a,armeabi`.
Beide benötigten Ausführungsumgebungen wurden tatsächlich gestartet.

- Offline Android-NDK-/CMake-/Gradle-Build: erfolgreich, beide ABIs,
  Debug-APK und Instrumentierungs-APK.
- Native Kernprüfungen auf dem Gerät: zweimal je ABI, **13386** Prüfungen unter
  ARM32, **13387** unter ARM64, jeweils null Fehler. Die zusätzliche ARM64-
  Prüfung betrifft einen Pointer oberhalb der 32-Bit-Grenze; semantische
  Schedulertraces sind identisch.
- APK: Installation mit erzwungener Prozess-ABI und **20/20** Tests je ABI.
  Vorhandene Controller-, Scheduler-, Modul- und echte Vulkan-Clear-/Surface-
  Präsentationsprüfungen bestanden. Kein neuer manueller physischer
  Controllernachweis; der bisherige Nachweis bleibt erhalten.
  Beide Vulkan-Messberichte nennen Adreno 650, Vulkan 1.1.128, EFB 512�448
  sowie 17 pr�sentierte Frames und 17 nachgepr�fte GPU-Clears. Effektiv aktiv
  sind weiterhin ein Sample, AF 1� und DoF aus; F�higkeiten werden nicht als
  eingeschaltete Grafikoptionen ausgegeben.
- Originales Licht: je Bootpipeline zwei Durchgänge pro Disc, unverändert
  **11049** Prüfungen Disc 1 und **8399** Disc 2. Lichtzustände/Traces identisch.
- Zusätzliche Ressourcenregression auf Host x64 und Android ARM32/ARM64:
  zweimal je Disc, **308 Ressourcen** (157/151) und **142 Raumvorkommen**
  (85/57), mit MRAM-/ARAM-Transfers und Lebenszeiten.
- Zusätzliche Modulregression auf Host x64 und Android ARM32/ARM64: zweimal
  je Disc, **114 Original-RELs** (110/4), einschließlich bestehender nativer
  Teilregistrierung und nachgewiesener originaler Raumeinträge. Dies bedeutet
  keine Ausführung sämtlicher noch nicht portierter REL-Funktionen.
- Vorhandene R120Event-Abbruch-/Sleep-/VI-/Retirement-Regressionen bestehen.
  Headless bleibt VI-Präsentation eine Grenze; die APK prüft die tatsächliche
  Präsentation. Sofdec bleibt anschließend eine fehlende Abhängigkeit.

Alle vier Plattformen liefern gleiche relevante Originalzustände und Hashes.
Der gemeinsame Messbericht liegt ausschließlich privat unter
`E:\Development\Android\RE4-Data\phase4c-player-summary\consumer-runtime-summary.json`.
Boot-/Modelldaten liegen in `phase4c-player-host` und `phase4c-player-device`;
Ressourcen-/Modulberichte in `phase4c-player-resources` und
`phase4c-player-modules`. Build-/Testlogs stehen ignoriert unter `port/build`.

Zwischenstände mit Fehlern sind nicht als erfolgreiche Tests gezählt:
ein temporärer Dateizugriff scheiterte im Sandbox-Hosttest; die erlaubte
Ausführung wurde wiederholt. Ein isolierter Test versuchte zunächst einen
zweiten Scheduler auf demselben Ownerthread; korrigiert auf den vorhandenen
Scheduler. Eine neue Freigabesperre blockierte zunächst leere Playerassets bei
bestehenden Modulwechseln; korrigiert und vollständig nachgeprüft. Der erste
CTest-Lauf scheiterte außerdem an der alten erwarteten Anzahl 87 statt 88
Consumerfunktionen; die Ergänzung ist jetzt explizit in der Quellenprüfung
belegt. Abschließende Tests haben null Fehler.

## Noch offene Originalkette und nächster Schritt

Der weitere reale Actorpfad ist in den Originalquellen belegt:

```text
gameRoomInit (game.cpp:354)
  -> ReadPlayerData(pl_type, pl_costume) (read.cpp:578)
       -> ReleasePlData, FileTbl-Auswahl, DVD/Yz2, optionale Spieler-REL
  -> originale CONS/SMD-Manager- und Ressourceninitialisierung
  -> EmMgr.create(0,0) (game.cpp:465)
       -> cEmMgr::construct (em.cpp:76) -> cPlLeon bei pl_type 0
  -> cPlayer-Konstruktor (player.cpp:202)
       -> pPL, hp, zwei Motiontabellen, debugInit
  -> cPlLeon-Konstruktor (pl_leon.cpp:26)
       -> init0 -> setModel -> weaponRelease/weaponLoad/weaponInit
       -> init1 -> setMotion -> EspDataLoad -> startUp -> Fußschattentabelle
```

Noch fehlen insbesondere vollständige native cEm/cPlayer-Workobjekte und
Managerkonstruktion, originale Waffenmodule und Ausrüstungs-Motiontabellen,
Push/Waist/Neck/MotBase, Atari/Yarare/SatMgr-Kollision, Cloth-/Effekt-/Shape-
Wiedergabe, AttachCamera sowie `startUp` mit den tatsächlichen `move`- und
Motion-/Kollisionsaufrufen. Die GC-Poolgröße `0xDE0` aus `em.h` darf dabei
nicht als `sizeof(cPlayer)` oder ARM64-Speicherlayout übernommen werden.
Die globale Lebens-/Waffenvorgabe ist bewiesen; Actor-HP, Routinezustand,
vollständige Ausrüstung und Bewegungsbereitschaft werden noch nicht behauptet.

`ShapeSet` für aktive Gesichtsmorphs und `DLL_Unlink` für native Waffen-RELs
brechen ausdrücklich ab. Camera/Collision/Actor-Aufrufe erhalten weiterhin
die bestehenden ausdrücklichen MissingConsumer-Grenzen.

**Nächster begrenzter Phase-4c-Auftrag:** originales `MerchantGameInit` und
gegebenenfalls die originale New-Game-Iteminitialisierung analysieren und
integrieren; danach den nächsten GameTask-Abbruch erneut messen. Der spätere
Actorpfad muss `ReadPlayerData` und cEm/cPlayer-Besitz gemeinsam integrieren
und die hier geprüften Modell-/Motionhelfer wiederverwenden. Es wurde weder
Merchantverhalten noch eine vollständige Phase 5 begonnen.

Vulkan-Bootstrap und Controllerarchitektur bleiben erhalten. MSAA Aus/2×/4×,
AF 2×/4×/8× und persistentes DoF mit Nah-/Zielfokus bleiben die verbindlichen
späteren Anforderungen mit hardwaregeprüftem effektivem Zustand. Es wird
weiterhin keine vollständige GX-Geometrie, GPU-Beleuchtung oder Videoausgabe
als implementiert ausgegeben.

## Reproduktion und Projektdateien

Hostbuild über die vorhandene MSVC-/CMake-Konfiguration für `host-ninja` und
`host-x86`, danach `ctest --test-dir ... --output-on-failure`.
Androidbuild mit `port/scripts/build-android.ps1 -Offline` und den vorhandenen
SDK-, JBR- und Pythonpfaden. Gerätetests über `test-device.ps1 -Repeat 2` und
`test-app-device.ps1`, ADB-Seriennummer `dbdcacc`.

Private Datenprüfung: `port/tools/test_game_runtime.py --data
E:\Development\Android\RE4-Data\import-phase3a --output <privates-Verzeichnis>
--host-exe <re4_game_tool.exe> --repeat 2`, separat x86/x64. Android mit
`--adb <adb.exe> --serial dbdcacc --repeat 2`. Entsprechend bestehende
`test_resources.py`-/`test_modules.py`-Aufrufe. Der zusammenfassende Vergleich
verwendet `summarize_consumers.py --host ... --device ... --output ...`.

Neue vollständige Dateien:

- `port/config/player-sources.json`
- `port/include/re4/port/player.h`
- `port/src/player.cpp`
- `port/src/player_original.h`
- `port/tools/generate_player.py`
- dieser Bericht, `port/docs/phase4c-player-status.md`

Geänderte vollständige Dateien:

- `port/CMakeLists.txt`
- `port/include/re4/port/consumers.h`, `game_runtime.h`, `model_types.h`, `motion.h`
- `port/src/consumers.cpp`, `game_runtime.cpp`, `model_runtime.cpp`, `motion.cpp`,
  `runtime_original.h`
- `port/tests/game_runtime_tool.cpp`, `game_runtime_verification.cpp`,
  `test_consumer_sources.py`, `test_runtime_sources.py`
- `port/tools/generate_consumers.py`, `generate_runtime.py`,
  `summarize_consumers.py`, `test_game_runtime.py`

Originaldateien in `src/` und `include/` bleiben unverändert. Keine Images,
extrahierten Assets oder privaten Messdaten werden versioniert. Lokaler
`master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`, `origin/master`
bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`. Die aktualisierte README
wurde berücksichtigt; keine Zusammenführung, kein Rebase, kein Push.
Der geprüfte Stand wird ausschließlich als lokaler Commit auf `android-port`
gesichert.
