# H1: Originale Cloth- und Player-Laufzeit – geprüfter Teilstand

Ausgangsstand `4fdd8192bbb57b11c159f095b877d37e7baa476a`, Branch
`android-port`, Prüfung am 12. Oktober 2026. **H1 bleibt offen.**

Die erhaltene Konstruktion `EmMgr.create -> cEmMgr::construct -> cPlLeon ->
init1 -> cPlayer::startUp` kehrt jetzt erfolgreich zurück. Ein nativer Actor
mit 119 Originalgelenken, acht Modellinformationen, originalen Waffenmodulen,
Body-/Neck-/Motion-Works und Cloth-Ketten wird veröffentlicht. Wiederholte
Konstruktion, Updates und Freigabe wurden auf Windows x86/x64 und Retroid
ARM32/ARM64 tatsächlich ausgeführt.

Die vollständige eingabegesteuerte Laufzeit ist damit noch nicht hergestellt:
zwei Walk-Ticks schließen ab, der dritte erreicht ein originales
SEQ-Soundereignis und scheitert am weiterhin ungebundenen `SndCall`. Seine
bereits geänderte Position wird beobachtet, aber dieser Tick wird ausdrücklich
als unvollständig gezählt. Die neutralen Ticks sind keine vollständige
GameTask-Spielszene.

## Tatsächlich ausgeführte Originalfunktionen

Die Generatoren prüfen normalisierte SHA-256-Fingerabdrücke aller verwendeten
Originaldateien. Vollständige übernommene Bodies und Anpassungslisten liegen
reproduzierbar unter `port/build/<build>/original/`; die unveränderten Quellen
bleiben in `src/` und `include/`.

| Funktion | Originalquelle | Ausführungsnachweis |
| --- | --- | --- |
| `PlClothMoveLeon` | `src/game/pl_cloth.cpp:298` | Jacket, Holster, Hair in Originalreihenfolge; anschließend `be_flag &= ~0x00E00000` |
| `testJacketMoveLeon` / `testHairMoveLeon` | `src/game/pl_cloth.cpp:431` / `:392` | Tatsächliche pl00-Ketten in jedem neutralen und versuchten Walk-Tick |
| `testHolsterMoveLeon` | `src/game/pl_cloth.cpp:472` | Originaler Kostüm-0-Zweig; die Kostüm-1-Simulation wird hier nicht als Ressourcennachweis behauptet |
| `PenClothMove` / `PenClothMove3` | `src/game/pendulum.cpp:225` / `:823` | Gravitation, Dämpfung, Nachbar-/Längen-/Winkelbedingungen, Kollisionsprüfung und Gelenkmatrizen |
| `PenClothReset` | `src/game/pendulum.cpp:1580` | Warp-Reset vor erster Simulation; zwei Resets pro Kostüm-0-Actor, danach keine künstlichen Wiederholungen |
| `penClothAtMake` | `src/game/pendulum.cpp:1144` | Sechs Jacket- und vier Hair-Volumen aus den tatsächlichen Modellmatrizen |
| `penClothAtCk`, `penClothAtCkBorder`, `penClothAtCkParallel` | `src/game/pendulum.cpp:1236`, `:1288`, `:1424` | Originale Projektionen/Parallelkorrekturen in den tatsächlichen Chain-Kernels; zusätzliche isolierte Zahlenprüfungen |
| `PosToPos` / `PenWindSet` | `src/game/sub2.cpp:321` / `src/game/pendulum.cpp:1613` | Originale Volumeninterpolation; Originalwindrechnung separat numerisch geprüft |
| `cPlayer::startUp` | `src/game/player.cpp:277` | Ganze Funktion inklusive erstem virtuellen `move`, zwei weiteren `motionMove`, SAT und abschließendem `matUpdate` |
| `cPlayer::move`, `pl_R0_Move`, `pl_R1_Footwork` | `src/game/player.cpp:294`, `:396`, `:406` | Durchgehende originale Updatekette mit Action-, Augen-, Body-, Weapon-, Neck-, Motion-, Kollision- und Cloth-Verbrauchern |
| `pl_R1_Walk` | `src/game/player.cpp:446` | Auswahl über zentrale `PadRead`-/KEY-Verarbeitung; tatsächliche FCV-/SEQ-Ausführung bis zur Soundgrenze |
| `cPlayer::debugMove`, `cPlayer::emSearch` | `src/game/pl_debug.cpp:222`, `:235` | Ganzer Debug-Aufrufkörper und Originalsuche auf dem nativen Actor-Pool; aktivierte ungebundene Debug-Verbraucher werfen explizit |
| `pl_fs_tbl` / `pl_fsd_dat` | `src/game/foot_shadow_tbl.cpp:5`, `:69` | Sechs unveränderte Quelldatensätze mit geprüften Gelenkreferenzen, nativer Tabellenpointer im Modell |

`pl_R1_Back` ist ebenfalls als vollständiger Body gebunden. Der neue
Originaldatenlauf beweist seine aktive Ausführung nicht; er wird deshalb nur
als kompiliert aufgeführt. Die restlichen Bewegungstabellen sind weiterhin
explizite Fehlergrenzen, keine vereinfachten Bewegungsfunktionen.

## Native Anpassungen und Grenzen

Der Locked-Cache-Pointer `0xE0000000` wurde durch pro Laufzeit besessenen,
typisierten Scratchspeicher ersetzt. `ClothAtData` enthält ausschließlich die
originalen Skalare/Matrizen und hat geprüft `0x84` Bytes. `ClothAtWork` hat
native Pointer und ein begrenztes Array für 256 Volumen; keine GameCube-Bytes
werden als ARM64-Pointerstruktur interpretiert. Negative/überlange Anzahlen
und ungültige Gelenkverweise werden vor der Originalschleife abgelehnt.

Die wirkungslosen PPC-Registerpins und leeren Compiler-Anker werden entfernt;
ihre Zuweisungen, alle Rechenschritte, Bedingungen und Iterationsreihenfolgen
bleiben erhalten. Die bisherigen skalaren SDK-Matrix-/Vektorfunktionen werden
verwendet. Es gibt keine neue NEON-, GPU- oder erfundene Cloth-Animation.
Die ursprünglichen `pAt - 1`-Schleifen beginnen nativ am ersten gültigen
Arrayelement und behalten ihre Besuchsreihenfolge.

Externe `pPtbl`-/Fremdmodell-Borger sind noch nicht gebunden und werden
abgelehnt. `PenClothMove2`, weitere Charakter-Cloth-Sätze und der vollständige
Raumwind-Verbraucher `cPenWind::set` / `LightMgr.setEnv` gehören nicht zum
nachgewiesenen Umfang. Die Tests setzen keine Wind-/NoCloth-/Warpflags, um
fehlende Simulation zu umgehen.

`GetWaterHeight` unterstützt derzeit ausschließlich den originalen frühen
Rückkehrzweig bei nicht gesetztem `STA_WATER_ALIVE`
(`src/game/Espgen42.cpp:296`). Bei aktiver Wasserverwaltung wird ausdrücklich
`GetWaterHeight/live original water generator registry` gemeldet. Der restliche
Water-Generator-Body ist nicht ausgeführt. Das ist kein allgemeiner
"kein Wasser"-Erfolgsersatz.

Die deaktivierten Originalzweige von `scrHitCheck`, `satMakeTest`,
`localCoordTest` und `EmYarareDisp` bleiben erhalten. Aktivierte Zweige, OBA-
und Positions-Debugausgabe sowie Cloth-Debuggeometrie melden fehlende
Verbraucher. Sie werden nicht durch leere Zeichnungsfunktionen ersetzt.

## Besitzer und Freigabe

Der erste zusätzliche Freigabe-Vorabcheck behandelte die eigenen
Waffen-Parentreferenzen fälschlich wie fremde Borger. Dadurch wurde die
normale Freigabe verweigert; das anschließende Unwinding mit noch lebendem
Actor führte unter Windows zum Abbruch `0xC0000409`. Dieser Zwischenstand
wurde nicht als bestanden gewertet oder committed.

`EngineConsumers::checkModelRetirement` prüft jetzt explizit die
Kontrollblockidentität der tatsächlich vom Player besessenen Waffenborger,
lehnt fremde/doppelte Borger ab und verlangt weiterhin einen ruhenden
Scheduler. Anschließend werden Waffen zuerst zerstört, dann das Modell mit
dem unveränderten normalen `releaseModel`-Check, dann der Actor und seine
Works/Module. Ein zusätzlicher externer Borger verhindert die Freigabe vor
jeder Veränderung an Waffen, Callback- oder Effectzuständen. Diese negative
Prüfung und die anschließende erfolgreiche Freigabe werden mit jedem
Original-Actorfall wiederholt.

Die Live-Beobachtung erfasst Motion, Position, Kollisions-/Actionzähler und
Cloth-Werte. Immutable Ressourcenhashes werden nur beim Konstruktionsergebnis
erfasst und nicht in jedem Player-Tick neu berechnet.

## Originaldaten und tatsächliche Ticks

Quelle bleibt `E:/Development/Android/RE4-Data/import-phase3a`. Die bestehenden
Inventare und Hashes werden für beide Disc-Eingaben verwendet. Die Playerprobe
nutzt das originale `em/pl00.drs`, native `wep01`-/`wep02`-Bindungen und
r120-SAT-Daten. Ausrüstung ist als isolierter Testeingang gekennzeichnet;
Position und Rotation kommen aus den vorhandenen originalen globalen
Playerzuständen und werden nicht eingesetzt, um einen Boden zu finden.

Jeder tatsächliche Boot-Prüffall enthält:

- Zehn Konstruktion-/Freigabezyklen mit den bestehenden originalen
  Waffenfällen und jeweils 24 vollständig abgeschlossenen neutralen Ticks.
- 119 Gelenke, acht Modellinfos; zwei Cloth-Kernels je Player-Update,
  Jacket 24 Links / sechs Volumen, Hair 21 Links / vier Volumen.
- Whole-`startUp`: ein virtuelles Player-Update, drei Motion-Aufrufe,
  zwei Cloth-Resets und zwei Volumenaufbauten. Danach laufen Action,
  Actor-/SAT-Kollision, Motion und Cloth in jedem neutralen Tick.
- Definierte Forward-/Aim-/Knife-Eingänge über die vorhandene zentrale
  `PadRead`-Anbindung. Diese sind synthetische Eingangstests, kein neuer
  physischer Controllernachweis und keine vollständige Aim-/Knife-Laufzeit.
- Ein eigener neutral gestarteter Actor und danach gehaltenes Forward:
  zwei komplette Ticks, dritter Versuch mit tatsächlicher Root-Motion-
  Positionsänderung und anschließendem Soundabbruch. `complete=false` für
  diesen Versuch; Actorfreigabe danach ist geprüft.

Es wird kein erfolgreicher Tick nach `SndCall` simuliert, kein SEQ-Ereignis
gelöscht und keine erfundene Bewegung eingesetzt. Die alten separat
vorberechneten Modellposen bleiben Regressionen, nicht der neue Actorbeweis.

## Host- und Retroid-Ergebnisse

| Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| Windows x86/x64 Builds | Bestanden, abschließend erneut gebaut |
| Finale CTest-Läufe | Je 27/27 bestanden |
| Originaldaten-/GameRuntime-Suite | Alle vier Plattformen; zuerst `repeat=2`, danach `repeat=1` für den finalen Stand mit getrennter Ressourcenbeobachtung; beide Disc-Eingaben, New-Game-/Save-Zweige |
| Finaler Actorumfang | Vier Bootfälle je Plattform, je 240 komplette neutrale Ticks, zehn wiederholte Lebenszyklen und zwei vollständige Forward-Ticks plus explizit unvollständigem dritten Versuch |
| Retroid-Kernsuite | Zwei Läufe je ABI: ARM32 13.386 / ARM64 13.387 Checks, null Fehler; identische semantische Schedulertraces |
| APK-Instrumentation | Je 20/20 auf beiden ABIs, auch mit finaler APK erneut; Controller/JNI, Scheduler, Module, Vulkan/Surface-Prüfungen |
| GX-Hostorakel | x86/x64, je zwei Läufe; Originalgeometrie, Materialien, Kameras, TPL und native Zustände bestanden |
| Bestehende r120-Vulkan-Diagnose | Zwei GPU-Läufe je ABI, 229.376 Nicht-Clear-Pixel; Readback-SHA-256 unverändert `b46d87280a1701af2992f04a55b1aed31a409dbc293313c7cec4eabff02ae3bf` |
| r103-/r106-Callbackregression | Je zehn Hostfälle pro Architektur; Retroid beide Räume zweimal pro ABI, sichtbare Original-Gelenkänderungen, 31 Schedulerbesuche, sichere Freigabe |

Gerät: Retroid Pocket Flip 2, Serial `dbdcacc`, Android 13/API 33,
`armeabi-v7a` und `arm64-v8a`, Adreno 650. Die vorhandenen Hardwarewrapper
halten `Global\Retroid_ADB_Exclusive` über PowerShell `System.Threading.Mutex`
mit Timeout und `try/finally` während jedes vollständigen Testdurchlaufs.
Alle Geräteoperationen verwenden `adb -s dbdcacc`; kein Serverreset,
Geräteneustart oder Zugriff am Lock vorbei.

Der Vergleich der finalen aufgezeichneten Zustände ergibt gleiche semantische
Hashes auf allen vier Plattformen. Der maximale Unterschied der gespeicherten
Walk-Positionen, Rotationen und Cloth-Positionssummen gegenüber Host x64 beträgt
jeweils 0 bei der JSON-Ausgabepräzision. Geprüfte Vergleichsgrenzen sind
0,002 / 0,0001 / 0,1 Originaleinheiten. Die ursprünglichen unabhängigen SAT-
und Cloth-Setup-Referenzprüfungen bleiben bestehen. **Dies beweist keine
bitgenaue Gleichheit mit PPC Paired Single**; dafür fehlt ein passender
Original-PPC-Zustandsdump.

Khronos-Validation war aktiv: null Fehler, eine bekannte Performancewarnung
zu Identity-`preTransform` gegenüber ROTATE_90. Die Shelf-Logs enthalten
weiterhin Adreno-`Unknown Format 0`-/MemoryLayout-Meldungen. Deren Ursache
ist nicht behoben; der gesamte Treiberlog wird nicht als fehlerfrei bezeichnet.
Die GPU-Aufnahmen sind bestehende Render-/Callbackdiagnosen, keine Frames
des neuen laufenden Leon-Actors. Eine neue zusammenhängende Spielszene oder
ihre visuelle Originaltreue wird hier nicht behauptet.

## Tatsächlicher Stand von gameRoomInit

Der echte Original-GameTask erreicht weiterhin `Rno0=2` nach `gameInit` und
`gameStageInit`. `port/src/runtime_original.h::gameRoomInit` bleibt eine
explizite Fehlergrenze. Sein Originalkörper aus `src/game/game.cpp:354` ist
in diesem Auftrag nicht künstlich als abgeschlossen registriert worden.
Es gibt keinen Übergang zu `Rno0=3` und keinen behaupteten `gameMainLoop`.

Nach dem jetzt erfolgreichen Actorstart fehlen insbesondere gemeinsame
Consumer für:

1. Soundadress-/Bank-/Room-Initialisierung, Original-ISS-Requests und
   Voicelebenszeiten. Der direkte Tickabbruch stammt aus
   `cPlayer::seqSeCtrl` (`src/game/pl_class.cpp:812`) -> `SndCall`
   (`src/game/snd.cpp:717`) -> `Snd_iss_req_para` / `req_iss_main`
   (`src/game/snd_iss0.cpp`). Die weitere Request-/Sequence-/AX-Ausführung
   einschließlich `AXAcquireVoice` in `snd_iss3.cpp` ist nicht gebunden.
   Ein Request-Recorder, Rückgabewert 0 oder `DBG_SE_STOP` wäre kein Ersatz.
2. Den ganzen `gameRoomInit`-Verbund: originale Room-Memory-/Poolreihenfolge,
   Ctrl-/Eventmanager, AEV-/ITA-Works und `SceAtInit`, `EvtMgr.myRoomInit`,
   gemeinsame Raum-Effect-/EAT-Verbraucher und tatsächliche Raumlicht-
   einschließlich SAR-/Wind-Aktualisierung.
3. `CameraRoomInit` / `CameraControl::roomInit`, Room-/Core-CAM-Verbraucher,
   vollständige `Block.check`-Aktivierung, Szenario-/Eventpakete und deren
   Modell-/Actor-/Callbackbesitzer. Die erfolgreiche Shelf-Kette bestätigt
   den vorhandenen originalen Scheduler-/Modell-Präsentationsweg; sie
   ersetzt diese Consumer nicht.
4. Die übrigen Player-Routinen einschließlich Run, Aim, Knife, Damage und
   tatsächlicher Interaktionen; aktive Wasser-/Sand-/Debug-Effectpfade
   bleiben konkrete Fehlergrenzen.

Der nächste begrenzte Schritt innerhalb H1 ist der gemeinsame Original-
Soundrequest-Verbund für echte SEQ-Ereignisse und danach länger laufende
Walk-/Eingabetests. Der alte, ausschließlich gelesene Port besitzt dafür
geeignete technische Bausteine: `sound_bank.cpp::DecodeDspAdpcm` und
`sound_mixer.cpp::SoundMixer::playSpatial`. Sie ersetzen weder die originalen
SIT-/ISS-Entscheidungen noch Sound-IDs, Prioritäten oder Fertigstellungszustände.
Ihre Übernahme wurde in diesem Auftrag nicht vorgetäuscht. Eine zusätzliche
Hybrid- oder Gameplayphase wurde nicht begonnen.

## Nachweise und Reproduktion

Alle Assets und GPU-Nachweise bleiben außerhalb Git. Finale Originaldatenlogs:

- `E:/Development/Android/RE4-Data/hybrid-h1-cloth-final64`
- `E:/Development/Android/RE4-Data/hybrid-h1-cloth-final32`
- `E:/Development/Android/RE4-Data/hybrid-h1-cloth-final-android`
  (`messages-<ABI>.json`, `player-numeric-comparison.json`)
- Vorläufe mit je zwei Wiederholungen unter `hybrid-h1-cloth-host64`,
  `hybrid-h1-cloth-host32`, `hybrid-h1-cloth-android`.
- `hybrid-h1-cloth-vulkan` enthält echte GPU-/Display-PNGs beider ABIs.
  `hybrid-h1-cloth-shelves` enthält die Callback-Readbacks/Displays und Logs.
  Hostorakel liegen in den entsprechenden `*-host64`-/`*-host32`-Ordnern.

Build-/Testlogs liegen ignoriert unter `port/build/h1-cloth-*.log`.
Nach dem vorhandenen Host-/Android-Build können die geprüften Runner erneut
mit jeweils eigenem Outputverzeichnis ausgeführt werden:

```powershell
python -B port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-h1-cloth-final64 --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 1
python -B port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-h1-cloth-final32 --host-exe port/build/host-x86/re4_game_tool.exe --repeat 1
python -B port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/hybrid-h1-cloth-final-android --adb C:/Users/andiw/AppData/Local/Android/Sdk/platform-tools/adb.exe --serial dbdcacc --repeat 1
```

Die unveränderten Runner `test_gx_scene.py`, `test_shelf_callbacks.py`,
`test-device.ps1`, `test-app-device.ps1` liefern die weiteren Regressionen.

## Vollständig gespeicherte Projektdateien

- Build/Provenienz: `port/CMakeLists.txt`, `port/config/cloth-sources.json`,
  `player-sources.json`, `player-live-sources.json` im selben Configordner.
- Native Schnittstellen: `port/include/re4/port/consumers.h`, `model_types.h`,
  `player.h` im selben Headerordner.
- Laufzeit: `port/src/cloth.cpp`, `cloth_original.h`, `consumers.cpp`,
  `player.cpp`, `player_original.h`, `player_live.cpp`,
  `player_live_original.h` im selben Sourceordner.
- Generatoren: `port/tools/generate_cloth.py`, `generate_player.py`,
  `generate_player_live.py`; tatsächlicher Datenrunner `test_game_runtime.py`.
- Prüfungen: `port/tests/game_runtime_tool.cpp`,
  `game_runtime_verification.cpp`, `test_runtime_sources.py`.
- Dieser Bericht und der historische Verweis in
  `port/docs/hybrid-h1-actions-status.md`.

Keine Original-Assets, Änderungen im Referenzport oder Originalquellen.
`master` bleibt auf `9b76ff00`; `origin/master` bleibt auf `15cf64f`
(README-only, 25 zusätzliche Zeilen). Die README-Aktualisierung ist
berücksichtigt, aber nicht automatisch gemergt. Keine Pushes, Merges oder
Rebases. Vulkan und zentrale Controllerverarbeitung bleiben erhalten;
MSAA Aus/2x/4x, AF 2x/4x/8x und persistentes DoF mit Nah-/Zielfokus bleiben
verbindliche spätere Anforderungen.
