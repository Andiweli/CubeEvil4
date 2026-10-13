# Phase 4c – Originale Modellinitialisierung und TexRender, Ergänzung

Stand: 10. Oktober 2026. Grundlage: `50f665ec` auf `android-port`.
Dieser Bericht ersetzt die damaligen offenen Modell-/TexRender-Punkte des
[vorherigen Teilberichts](phase4c-status.md); dessen übrige Grenzen gelten weiter.

## Nachgewiesenes Ergebnis

Die originale CPU-Kette `cModInfoMgr::create` → `cModel::modelInit` →
`initJoint`/`setPartsOffset` läuft mit echten BIN/TPL-Ressourcen auf Windows
x86/x64 und dem Retroid unter ARM32/ARM64. Die Initialisierung erstellt native
Modellinformationen, originale Teilehierarchien, Rest-/Welt-/inverse Bindematrizen,
Boundingboxen, Motion-Anfangszustand und Texturzuordnungen. Sie erzeugt keine
Ersatzgeometrie und keine neu erfundene Raum- oder Actorlogik.

Die vollständige CPU-Initialisierung von `TexRenderInit` funktioniert.
Die separat aufgerufene originale `R120Init` kehrt jetzt zurück. Ihr Openingtask
setzt nach dem originalen ersten `SceSleep` fort und erreicht die konkrete
VI-Präsentationsabhängigkeit. `GameTask` erreicht nach originalem `ClothInit`
den noch fehlenden `GXSetCopyClear`-Zustand. Ein vollständiger Spielstart,
Eventablauf oder Renderer ist damit weiterhin **nicht** nachgewiesen.

## Git und unveränderte Grundlagen

- Ausgangsarbeitsbaum war sauber; alle Änderungen liegen unter `port/`.
- `master`: `9b76ff003081d6ece8b838453fb8310743a972c7`, unverändert.
- Lokal vorliegendes `origin/master`: `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
  Die README-Abweichung ist genau eine Datei mit 25 Ergänzungen. Ihre Angaben
  zur Debugversion G4BE08, zwei Discs und 114 RELs wurden berücksichtigt.
- Kein Merge, Rebase, Push oder Wechsel auf master; keine alten Commits entfernt.
- Originale Dateien unter `src/`, `include/`, `tools/`, `docs/`, `config/`
  bleiben unverändert. Private Originaldaten/Images werden nur gelesen.
- Keine Assets in Git oder APK. Testdaten im Repository sind ausdrücklich
  synthetische Format-/Fehlerfixtures, keine nachgebauten Spielräume.

## Originalfunktionen und native Anpassungen

Die Buildgeneratoren prüfen normalisierte SHA-256-Fingerprints, extrahieren
vollständige Originalfunktionskörper und schreiben pro Funktion Herkunft,
Quellzeile und explizite Ersetzungen in die Buildmanifeste. Die ursprünglichen
Quelldateien werden dabei nicht geändert. Der Consumer enthält jetzt 49,
TexRender 7 und der Runtimegenerator 40 extrahierte Funktionen.

| Neu ausführbare Originalkette | Tatsächliche Quelle |
| --- | --- |
| `cCoord`, `cModel`, `cModelInfo` Konstruktoren | `include/model.h:34`, `src/game/model.cpp:47`, `:732` |
| `cModInfoMgr::create`, `cModel::modelInit` | `src/game/model.cpp:1343`, `:99` |
| `initJoint`, `releaseJoint`, `setPartsOffset` | `src/game/model.cpp:148`, `:167`, `:177` |
| `partsMatCalc`, `partsWorldCalc`, `getBoundingBox` | `src/game/model.cpp:237`, `:403`, `:1197` |
| `addModel`, `releaseModelInfo`, `makePartsList`, `setJointInfo`, `releasePartsList` | `src/game/model.cpp:666`, `:1070`, `:1084`, `:1114`, `:1136` |
| `cPartsMgr::createSequential`, `PartsMgrWork` | `src/game/model.cpp:1279`, `:1269` |
| Originale Pool-/Listen-/Freigabekörper von `cManager` | `include/cManager.h` |
| `RotMatrix`, `TransMatrix`, `ScaleMatrix` | `src/game/math_sub.cpp` |
| `C_MTXIdentity/Copy/Concat/Trans/Scale/RotRad/RotTrig`, `C_MTXMultVec` | `src/lib/mtx.c`, `src/lib/mtxvec.c` |
| `TexRenderMgrInit/RoomInit`, `GetTexRenderMgr`, `TexRenderMng::Init/AllocBuf/ReAllocBuf`, `TexRenderInit` | `src/game/TexRender.cpp:33`, `:45`, `:59`, `:193`, `:206`, `:218`, `:228` |
| `ClothInit`, kompletter Kontrollflusskörper von `gameInit` | `src/game/cloth.cpp:32`, `src/game/game.cpp:239` |

Die portable C-Matriximplementierung stammt aus dem vorhandenen SDK-Quellcode.
Sie ersetzt die PowerPC-Paired-Single-Instruktionen. Strikte FP-Optionen sind
aktiv (`/fp:strict` beziehungsweise `-fno-fast-math -ffp-contract=off`). Die
getesteten Zustände sind über vier ABIs identisch. Eine bitgenaue Gleichheit
sämtlicher späterer rotierter/animierter Matrizen mit der PPC-GPU-Pipeline ist
damit noch nicht bewiesen.

### Native Speicher- und Layoutadapter

`model_types.h` enthält native Ansichten der tatsächlich benötigten Originalfelder
von Unit, Coord, Parts, Modellinformationen und allen benannten `MOTION_INFO`-
Feldern. Es gibt keine Kopie eines GC-Objekts mittels festem `sizeof` oder
32-Bit-Vptr-Layout. Die C++-Objektgrößen dürfen zwischen den ABIs variieren.
Es handelt sich um CPU-Modellobjekte, noch nicht um vollständige `cEm`/`cObj18`-
Actorinstanzen mit allen virtuellen Methoden und Gameplay-Subsystemen.

`model_runtime.cpp` liest belegte BIN-/TPL-Felder über begrenzte BE-Zugriffe.
Die Originaldaten bleiben unverändert und über `ResourceLease` im Besitz der
Modelllebenszeit. Jointfloats, signierte Fixed-Point-Vertices und Blend-/Flip-
Tabellen werden in ausgerichteten nativen Speicher umgesetzt. Blend/Flip-
Payloads bestehen aus echten `u16`-Objekten; der 32-Bit-Count wird per `memcpy`
gespeichert/gelesen. Die spätere Motion-Portierung muss diesen Count ebenfalls
ohne typfremden Pointerzugriff lesen.

Nachweis für Blend: `tools/motion/modelbin.py` und `MotionMove` in
`src/game/motion.cpp:533`: s32 Count und vier u16 je Datensatz. Nachweis für
Flip: `setJointInfo` überspringt vier Headerbytes; `MotionMoveCore` indexiert
`flipTbl[pno]`. Die geprüften neuen BIN-Dateien enthalten davor ihren Jointcount.
Der Adapter validiert Count, Jointindizes und vollständige Tabellenbereiche.
Er unterstützt belegte BIN-Versionen `0x20010801` und `0x20030818`.

`calcModelAddr` und `calcTplAddr` sind **native Fixup-Adapter**, keine als
ausgeführt behaupteten Original-Relokationskörper. Die ursprünglichen
32-Bit-Inplace-Pointerfixups wären unter ARM64 ungültig. Der Adapter löst
Wire-Offsets tatsächlich auf, validiert Grenzen und bindet native Ansichten;
erst danach laufen die Originalfunktionen. `modelInit` liefert einen typisierten
`ModelInfoView*` statt des originalen pointerhaltigen `int`-Rückgabewerts.

Der Manager verwendet native Elementstrides und definierte C++-Objektlebenszeiten.
Die Original-Poolvergabe und Listenreihenfolge bleiben erhalten. GC-Adressbereich-
Checks werden durch die tatsächliche Poolzugehörigkeit ersetzt. Beim Freigeben
bleibt ein lebender nativer Slot mit originalem Flag-Clear erhalten. Eine
unterlaufende unsigned Schleifengrenze bei ungültiger `createSequential`-Größe
wird vor der Originalschleife begrenzt.

Produktive Poolgrößen müssen weiterhin aus den originalen
`ConsGetRoomValue(CONS_R_NMODELINFO/CONS_R_NPARTS)` und SMD-Anzahlen kommen.
Die Testanwendung gibt ausdrücklich eigene begrenzte Harness-Kapazitäten vor;
dies ist keine behauptete vollständige `gameRoomInit`-Integration.

TPL-Version, Deskriptortabelle, tatsächliche Headerfelder und Basispixelbereiche
werden gebunden. Unterstützt sind die durch `tools/motion/gxtex.py` belegten
Formate I4 (0), IA8 (3), CMPR (14). CLUT und andere Formate bleiben benannte
fehlende Consumer. Min/MaxLOD und alle Originalbytes bleiben erhalten;
Mipmapketten, GX-Texturobjekte, Skinning, Displaylist-/Primitiveausführung und
GPU-Uploads sind noch nicht umgesetzt. Ein Null-Joint-BIN wird ausdrücklich
abgelehnt, bis sein originaler Ein-Teil-Fallback-Layoutpfad belegt ist.

## Tatsächliche Originaldaten und Modellabdeckung

Die Auswahl der Openingpakete folgt den Phase-3a-Quellreferenzen. Die weiteren
Pakete werden universell aus dem FST-Inventar ausgewählt: kleinstes zusätzliches
EVD mit BIN-Einträgen je Disc. Es gibt keine raumspezifischen Modellregeln.

| Disc / echtes EVD | Geprüfte BIN-Hierarchien | Erfolgreiche vorhandene BIN/TPL-Paarungen |
| --- | ---: | ---: |
| 0 / `Evd/r120s00.evd` | 18 | 17 |
| 0 / `Evd/r120s01.evd` | 19 | 18 |
| 0 / `Evd/r100s00.evd` | 5 | 5 |
| 1 / `Evd/r317s15.evd` | 1 | 1 |

Die Zahlen zählen Vorkommen pro Paket, nicht einzigartige Assetinhalte.
Alle 37 bereits geprüften r120-Hierarchien bleiben geprüft. Von ihnen werden
35 tatsächlich durch die neue vollständige CPU-Initialisierung ausgeführt.
Pro Durchlauf werden alle 41 vorhandenen Paarungen zweimal initialisiert und
wieder freigegeben; zwei Durchläufe ergeben **164 reale `modelInit`-Aufrufe je ABI**.
Zusätzliche synthetische Pool-/Fehlertests sind hierin nicht mitgezählt.

Unveränderte Paket-SHA-256:

- r120s00: `ecf46d255e69415449d8f5245f14ec1815635fd13207fba34df2259d27838513`
- r120s01: `5e05fa3e07beff13970373ec19c8e3577f05f27ae2a99a56b5212fbe2c39928d`
- r100s00: `d4f33336fef74473038c56b1ed66ded95b8dcc4fe16497f2cde71fc66324f47a`
- r317s15: `599bd0facea0751dfcbea0d7054a86eba805e88bffaa2dbd360625be4adc90f8`

Die Paarungen stammen aus den tatsächlichen `SetOm`-/`SetParts`-Packetfeldern
in `include/event.h:160` und den Zugriffsstellen in
`Event::ExePacket_SetOm/SetParts`. Gleiche Dateistämme werden nicht als
Zuordnungsregel verwendet. Damit bleibt beispielsweise die gemeinsame TPL
von `obm3000a` bis `obm3000f` und die besondere s01-Textur von `ev0002` erhalten.

**Offen:** In beiden r120-Paketen verweist `ev000e.bin` auf
`em/pl00/pl000a.tpl`, die dort nicht enthalten ist. Weitere Packetreferenzen
betreffen externe `pl000a/pl000d/pl000l`-Modelle. Insgesamt bleiben vier
BIN/TPL-Paare pro Openingpaket als `external-resource-not-bound` dokumentiert.
Die exakt benannten Dateien sind auch keine direkten FST-Einträge im Inventar.
Das beweist nicht, dass keine andere gepackte Ressource den Inhalt enthält.

Die reale Fallbackabhängigkeit ist `EventMgr::GetBin` in
`src/game/event.cpp:2655`: bei nicht registriertem Namen und `flagGet==0`
folgen `x:/soft/room/`, `HDReadDebugAlloc` und `SetBin`. Dieser PC-Debug-Dateipfad
ist nicht implementiert und darf nicht durch zufällig passende Texturen aus
anderen Paketen ersetzt werden. Weitere Archive/Registrierungen müssen dafür
erst nachgewiesen werden.

Die tatsächlichen Motionpacket-Referenzen werden ebenfalls geprüft:
33 in s00 und 45 in s01. Das sind Referenz-/Integritätsprüfungen, keine
Ausführung von `MotionSetCore`, IK, Hermite-Interpolation oder Animation.
`Event::SetMod/GetMod`, Obj18-Erstellung, Parts-Anhängung und vollständiges
`Event::Init/ExePacket` bleiben eigene noch fehlende Originalconsumer. Der
diagnostische Modellpfad setzt dafür keine erfolgreichen Ersatz-Actorinstanzen.

## TexRender und erreichte Initialisierungsreihenfolge

Die sieben Original-TexRender-Körper verwalten acht Slots, Standardgröße
128×128, Textur-ID `0xF8+Slot`, Maskenbit `8<<Slot`, Alive- und Repeat-Zustand.
Der originale Default-Alloc→Resize→ReAlloc-Pfad bleibt erhalten. Für r120
entsteht der reale 256×256×4-Byte-Puffer im bestehenden Gameplayheap mit
32-Byte-Ausrichtung. Ein Ownerledger hält seinen Speicher außerhalb
abbrechbarer Taskstacks. Freigabe erfolgt nach Task-Quieszenz am Raum-/Frame-Rand.
Bei ReAlloc-Fehlern wird der freigegebene Pointer vor Exception-Bereinigung
genullt; Count und belegte Speicherblöcke werden zurückgerollt.

In `TexRenderInit` selbst gibt es keine GX-Aufrufe. Deren konkrete spätere
Abhängigkeit ist `CopyTexRenderMgr` (`TexRender.cpp:105`) mit EFB-Kopie,
`GXInitTexObj`, Wrap-/LOD- und Drawzustand sowie `RenderTexRenderMgr` und
`TransTexRenderMgr`. Der native GPU-Einstieg wirft eine benannte fehlende
Abhängigkeit. Kein Texturobjekt oder Renderabschluss wird vorgetäuscht.

Nachgewiesene Abläufe:

1. `GameTask` → `gameDebug` → originaler `gameInit` → originaler `ClothInit`
   → **Abbruch bei `GXSetCopyClear/EFB clear state (Phase 5)`**.
   Rno0 bleibt 0. Der komplette gameInit-Körper ist kompiliert; noch ungebundene
   spätere Imports brechen ausdrücklich ab, statt erfolgreich zurückzukehren.
   Die Clothansicht enthält nur das von ClothInit verwendete Flag; keine
   Cloth-Simulation wird behauptet.
2. Separater Diagnoseaufruf: originaler `R120Init` → native Modul-Workallokation
   → originale Eventcallbackregistrierung → `SceExec(0x12,R120Event,...)`
   → originaler `TexRenderInit(...,0x100,1)` → **normaler Return**.
3. Expliziter Szenario-OT-Besuch im Test: `R120Event` → `SceSleep(1)` →
   Fortsetzung → originales Cancelbit-Clear → **Abbruch bei
   `systemVISetBlack/presentation adapter`**, vor `Sofdec.Initialize`.
   Der Harness ruft diese Schedulerbesuche ausdrücklich auf; im produktiven
   Ablauf gehören sie zu `ScenarioMove/gameMainLoop`. Sie werden nicht pauschal
   zusätzlich in `GameRuntime::tick` eingefügt.

Fehlende Präsentation oder Video darf das Original nicht einfach überspringen.
Die Fehlerbereinigung überlebt den bereits invalidierten Taskhandle, merkt sich
die konkrete Rauminstanz und prüft den Modulepoch. Ein neuer Link wird nicht
von einer alten Failure-Retirement-Anforderung beendet. Die Original-Gameplay-
und Controller-Reihenfolge sowie der bestehende 30-Hz-Takt bleiben unverändert.

## Tatsächlich ausgeführte Tests

| Ausführung | Ergebnis |
| --- | --- |
| Windows x64 / MSVC 19.51 | finaler Build; **25/25 CTests**; **609 Runtimechecks** |
| Windows x86 / MSVC 19.51 | finaler Build; **25/25 CTests**; **609 Runtimechecks** |
| Android `armeabi-v7a` / NDK 28.2, Retroid `dbdcacc` | **609 Runtimechecks**; zwei reale Datenrunden; **164 modelInit-Aufrufe** |
| Android `arm64-v8a` / dasselbe Gerät | **609 Runtimechecks**; zwei reale Datenrunden; **164 modelInit-Aufrufe** |
| Basis/Scheduler/Background/Yz2 auf Retroid, je ABI zweimal | ARM32 **13.386**, ARM64 **13.387**, je **0 Fehler** |
| Bestehende Module/RELs, vier ABIs, je zweimal | **216 native Checks**; **110+4 echte REL-Inventare** pro Runde |
| Aktuelle Debug-App + Test-APK, jeweils tatsächlich installiert mit `--abi` | **19/19 Instrumentation-Tests je ABI**, `primaryCpuAbi` und Pointerbreite geprüft |
| Gradle offline | `assembleDebug`, `assembleDebugAndroidTest`, `lintDebug` erfolgreich |

Gerät: Retroid Pocket Flip 2, Android **13 / API 33**; ADB meldet
`arm64-v8a,armeabi-v7a,armeabi`. Dies sind echte ARM32-/ARM64-Prozesse auf dem
physischen Gerät, keine crosskompilierten Hosttests oder Emulatorbehauptungen.
Die eine zusätzliche Basisprüfung unter ARM64 ist die bestehende High-Address-
Callbackargumentprüfung in `src/scheduler_verification.cpp:96`. Die neue
Modell-/TexRender-/Runtime-Suite hat auf allen ABIs genau 609 Checks.

Die Tests beweisen originale Hierarchieverknüpfung, Boundingboxen unabhängig
gegen BE-Vertices, Translation und inverse Bindematrizen, Texturdeskriptoren,
Blend-/Flip-Endianness, Motiondefaults, unsichtbare Modelle, Originalpool-
Erschöpfung, Fehlerrückrollung, zwanzig Wiederverwendungen, Handleinvalidierung,
Packetgrenzen, unveränderte Assetbytes, acht TexRender-Slots, Heapgrößen,
Überlaufablehnung, Repeatoptionen und vollständige Pufferfreigabe. Zwölf
Original-r120-Initialisierungen prüfen Sleep→VI-Fehler→Framebereinigung.

Die bestehende Scheduler-, Datei-/Raum-/Block-/ESL-, Heap-, Modul-, Eventcallback-
und Controllerregression bleibt erhalten. Pro ABI werden zwanzig reale
Raumvorbereitungen mit Phase-3a-Hashes geprüft. Physische Controllerbetätigung
wurde in dieser Fortsetzung nicht neu vom Benutzer angefordert; die früheren
ARM32/ARM64-Captures bleiben erhalten, aktuelle JNI-Adaptertests bestehen.

Entwicklungsläufe scheiterten zunächst an Extraktionssyntax, fehlenden nativen
Typbindungen und überholten TexRender-Fehlererwartungen. Sie wurden korrigiert
und erneut ausgeführt. Ein Test erwartete irrtümlich Szenario-Ausführung allein
durch den Framepump; er verwendet jetzt explizite originale OT-Besuche. Diese
Fehlläufe sind keine erfolgreichen Nachweise. Der finale Teststand hat keine
Fehler. ASan/UBSan/TSan sowie PPC-Golden-Animationstests wurden nicht ausgeführt;
die Speicherbelege gelten für die geprüften Grenzen/Lebenszeiten und Fälle.

Privater automatischer Messbericht:
`E:\Development\Android\RE4-Data\phase4c-model\consumer-runtime-summary.json`.
Er enthält Executablehashes und gleiche semantische Traces über alle vier ABIs.
Opening-Trace: `f59bc8cf18712682e55d274a373ff8a1c54bfc14cdcf94ea442e08e6c3de68fd`.
Einzelberichte stehen in `phase4c-model-host`, `phase4c-model-device` und
`phase4c-model-modules-host/device`; Build-/CTest-/APK-Logs unter `port/build/model-*`
und `port/build/instrumentation-{armeabi-v7a,arm64-v8a}.txt` sind Git-ignoriert.

## Reproduktion

In einer Visual-Studio-Developer-Shell mit passender x86-/x64-Umgebung:

```powershell
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-x86 --output-on-failure
```

Vorhandene gepinnte Androidwerkzeuge verwenden:

```powershell
.\port\scripts\build-android.ps1 -SdkRoot "$env:LOCALAPPDATA\Android\Sdk" -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -Offline
.\port\scripts\test-device.ps1 -Adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" -Serial dbdcacc
```

Mit dem genannten Pythoninterpreter (oder einem eingerichteten Python ≥3.9):

```powershell
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-model-host --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-model-host --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data E:/Development/Android/RE4-Data/import-phase3a --output E:/Development/Android/RE4-Data/phase4c-model-device --adb "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" --serial dbdcacc --repeat 2
python port/tools/summarize_consumers.py --host E:/Development/Android/RE4-Data/phase4c-model-host --device E:/Development/Android/RE4-Data/phase4c-model-device --output E:/Development/Android/RE4-Data/phase4c-model
```

`test_modules.py` lässt sich mit denselben `--data`, `--output`, `--repeat` und
Host-/ADB-Optionen ausführen; Hostprogramm ist `re4_module_tool.exe`.
Neue Datenoutputs müssen getrennt vom Import und außerhalb des Repositories
liegen. Der CLI-Modus `models` führt keine Original-Roomcallbacks anderer Räume
aus; `consumers` prüft zusätzlich den belegten Opening-Init-/Cancelpfad.

## Vollständig gespeicherte geänderte Projektdateien

Alle Pfade relativ zum Repository; keine Original-Assets oder bloßen Patchdateien:

```text
port/CMakeLists.txt
port/README.md
port/android/app/src/androidTest/java/org/re4/port/GameRuntimeTest.java
port/config/consumer-sources.json
port/config/runtime-sources.json
port/config/tex-render-sources.json                         (neu)
port/docs/phase4c-model-status.md                           (neu)
port/include/re4/port/consumers.h
port/include/re4/port/game_runtime.h
port/include/re4/port/model_types.h                         (neu)
port/include/re4/port/tex_render.h                          (neu)
port/src/consumers.cpp
port/src/game_runtime.cpp
port/src/model_original.h                                  (neu)
port/src/model_runtime.cpp                                 (neu)
port/src/runtime_original.h
port/src/tex_render.cpp                                    (neu)
port/tests/consumer_verification.h
port/tests/game_runtime_tool.cpp
port/tests/game_runtime_verification.cpp
port/tests/model_verification.h                            (neu)
port/tests/test_consumer_sources.py
port/tests/test_tex_render_sources.py                      (neu)
port/tools/generate_consumers.py
port/tools/generate_runtime.py
port/tools/generate_tex_render.py                          (neu)
port/tools/summarize_consumers.py
port/tools/test_game_runtime.py
```

## Verbleibende Blocker und nächste begrenzte Arbeit

**Nachgewiesen offen:** GX-Clear-/EFB-Zustand im ursprünglichen GameTask;
VI-Präsentation und danach Sofdec im Opening; externe Event-Binregistrierung;
Obj18-/Event-ModTbl-Einbindung; MotionSetCore/IK/Hermite; originale produktive
Poolkonfiguration; vollständige Camera-/Player-/Light-/Message-/Saveconsumer.
Die 35 unvollständigen englischen Raum-MDT-Blöcke werden unverändert abgelehnt,
nicht erfunden oder durch andere Sprachen ersetzt.

**Technische Annahme:** Die bestehenden Lease-/Poolansichten eignen sich für
die nächsten CPU-Consumer. Das muss für deren tatsächliche Objektfelder,
Destruktoren und alle neuen Callstellen erneut geprüft werden.

**Konkreter nächster Schritt innerhalb von Phase 4c:** den originalen
`EventMgr::SetBin/GetBin/SetEvd`- und `Event::SetMod/GetMod`-Registrierungsweg
samt Obj18-Modell-Erstellung untersuchen und die fehlenden pl00-Abhängigkeiten
in Originalarchiven/Registrierungen nachweisen. Danach MotionSetCore mit echten
FCV-/SEQ-Daten verbinden; keine manuelle Eventinterpretation als Ersatz.
Für einen Start über GameTask sind zusätzlich die benannten Grafik-/Präsentations-
Abhängigkeiten erforderlich und müssen mit dem folgenden Renderingauftrag
abgestimmt werden. Diese Fortsetzung beginnt **keine Phase 5**.

Die native Controlleranbindung und die verbindlichen Anforderungen aus
[graphics-settings.md](graphics-settings.md) bleiben erhalten: backendneutrales
Ingame-Menü, MSAA Aus/2×/4×, AF 2×/4×/8×, DoF Ein/Aus mit Nah-/Zielfokus,
persistente Speicherung und wahrheitsgemäße Hardwarevalidierung.
