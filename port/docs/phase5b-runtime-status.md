# Phase 5b: Originale Merchant-, Motion-/IK- und SAT-/EAT-Laufzeit

Fortsetzung von `8e5591c690b2ced36c3a2f0746d9f8c496db62fb` auf `android-port`,
11. Oktober 2026. Verbindliche Datenquelle bleibt der lokale Import beider
G4BE08-Debug-Discs vom 25.11.2004 unter
`E:\Development\Android\RE4-Data\import-phase3a`. Englisch bleibt Referenzsprache.

## Ergebnis und Abnahmegrenze

**Dies ist ein geprüfter Phase-5b-Teilstand, keine abgeschlossene Spielsimulation.**
Der originale Standardzweig von `GameTask -> gameInit` beendet jetzt auch
`MerchantGameInit`, Zeit-/Speed-Initialisierung und `systemVISetBlack(0)` und
setzt selbst `Rno0 = 1`. Der anschließende Dispatch meldet die weiterhin
unportierte gemeinsame Funktion `gameStageInit/cMes/GameSave`. Im CLI-Nachweis
beobachtet ein ausdrücklich isolierter CPU-VI-Adapter die Präsentationsaufrufe;
dieser Test erzeugt keinen GPU-Spielframe.

Persistente originale Leon-Modellobjekte führen `MotionMove`, SEQ-Fortschritt
und die originale Bein-/Fersen-/Zehen-IK über mehrere Ticks aus. Die sieben
originalen FCV-/SEQ-Paare aus `pl00.drs` werden auf Windows x86/x64 und Android
ARM32/ARM64 geprüft. Der benötigte Actorstatus wird dabei ausdrücklich durch
einen isolierten, typisierten Kontext mit leerem originalem SAT-Manager
bereitgestellt. Das ist noch kein konstruierter `cPlLeon`, Bodenkontakt oder
von der Spielerlogik gewählter Animationszustand.

Originale SAT-/EAT-Segment- und Bodenabfragen arbeiten auf gehaltenen,
validierten nativen Kopien realer Big-Endian-Ressourcen. Die vollständige
Actor-Kollision mit Kugelverdrängung, Weapon-Laufzeit, Live-Schulterkamera,
`Event::Run` und das ereignisgesteuerte Auto bleiben offen. Der neue Motion-
Tickpfad ist noch nicht die Quelle der über Vulkan gezeichneten Diagnosepose.
Es gibt deshalb **keinen neuen Nachweis einer zusammenhängenden originalen
r100-Startszene oder einer GPU-Animation über mehrere Simulationsframes**.

## Neu kompilierte und ausgeführte Originalkörper

Die Generatoren übernehmen vollständige Funktionskörper mit Originalsignatur,
`#line`-Zuordnung und Manifest. Normalisierte Quellhashes verhindern, dass
geänderte Originalquellen unbemerkt mit den geprüften Anpassungen kompiliert
werden. `src/` und `include/` wurden nicht geändert.

| Quelle | Ausgeführte Originalfunktionen / Bedeutung |
|---|---|
| `src/game/merchant.cpp:696/722` | `MerchantGameInit`, `Merchant2ndRoundInit`: persistenter Waren-/Tuningzustand, Freundschaft und originale Debug-/Second-Round-Zweige |
| `src/game/merchant.cpp` | `chkFlag6C`, `merchant_stage1_full`, `merchant_stage2_full`, `merchant_stage3_full`, `stockDataInit`, `add_stock`, `stockDataAdd`, `levelDataInit`, `levelDataAdd`, `MerchantCharacter::setChar`; unveränderte Originaltabellen |
| `src/game/motion.cpp:414/1088/1196` | Der bereits vorhandene vollständige `MotionMove`-Körper ruft nun echte IK auf; persistentes `MotionSequenceCtrl` und neu `MotionCheckCrossFrame` |
| `src/game/ik.cpp:22/162/214/300` | `IKInit`, `ikCalc`, `heel2toe`, `InverseKinematics` einschließlich originaler Matrix-, Twist- und Zehenoperationen |
| `src/game/math_sub.cpp`, `src/lib/vec.c`, `src/lib/mtx.c`, `src/game/sub2.cpp` | `SetOrientationZX`, `VecAngle`, `C_VECDotProduct`, `C_MTXRotAxisRad`, `GetDistance3` als benötigte IK-Mathematik |
| `src/game/atari.cpp:399/826/841` | `cSatMgr::getFloor`, `hitCheck`, `hitCheck2`: originale Trefferreihenfolge, Filter, Normalen, Koordinatentransformation und Kein-Boden-Sentinel |
| `src/game/atari.cpp` | `cSatBlock::lineOverlap`, `hitCheckSphere`, `lineCross`, `blkPolyLineCk/Core`, `cSat::setCoord/setMatrix`; `hitCheckSphere` dient hier der Block-Vorauswahl, nicht der vollständigen Actor-Kugelverdrängung |
| `src/game/at_sub.cpp`, `src/game/sub2.cpp`, `src/lib/vec.c` | `Get_poly_attr`, `InterVectorXYZ`, `At_poly_line_ck`, `GetDistance`, `C_VECDotProduct`: originale Polygonabfragen und SAT-/EAT-Attributfilter |

Die Consumer-Generierung umfasst jetzt 111 Körper; Merchant- und Collision-
Manifeste enthalten zusätzlich 12 beziehungsweise 15 Körper. Das sind
Generierungszahlen, keine Aussage über den Anteil der fertigen Gesamtengine.

## Native Grenzen, ABI und Speicher

`MerchantRuntime` besitzt seinen Originalzustand und bindet ihn an den
wirklichen `NativeGameState` und die vorhandene Sprachvariable. Der
pointerfreie `MERCHANT_DATA`-Block bleibt auf allen ABIs `0x304` Bytes groß.
Native Character-Pointer werden nicht als GameCube-Wörter gespeichert.
`stage_no` wird aus dem kanonischen `room_id >> 8` abgeleitet, statt das
originale Big-Endian-Unionlayout auf Little Endian zu überlagern.
`memclr_asm` wird für diesen Pfad durch typisierte Nullinitialisierung ersetzt.
Shop-UI, Merchant-Actor und aggregierter Save/Load sind nicht Teil dieses Boots.

`EngineConsumers::advanceMotion` verwendet das bereits gehaltene Modell und
die geleasten FCV-/SEQ-Ressourcen. Stale Handles, fremde Besitzerthreads,
fehlende Motiondaten und ungültige Folgeindizes werden vor Ausführung erkannt.
SEQ-Keys werden gegen die originale FCV-Ausdehnung geprüft; die originale
Behandlung übergroßer angeforderter Startframes bleibt erhalten. Hokan,
Sekundärmotion und angehängte Motionkamera sind weiter explizite Fehler vor
Mutation. Originale Loop-, Rückwärts-, Pause- und Speedregeln werden nicht
durch normalisierte neue Regeln ersetzt. Der Tick-Speed bleibt unabhängig
vom Android-Displaytakt.

`MotionActorContext` transportiert genau die derzeit benötigten Statuswerte
und den SAT-Besitzer. Der originale `cEm*`-Cast auf ein Modell wird damit nicht
als unzulässige native Speicherüberlagerung ausgeführt. Das ist ein notwendiger
Plattformadapter, kein Actor-Ersatz. Ohne gültigen Kontext wird aktive IK
abgelehnt. Der Kontext wird nach Erfolg und Ausnahme wiederhergestellt.
Originale Ketten mit Flag `0x80` werden weiterhin vom Original übersprungen.

`CollisionRuntime` hält das Originallease, dekodiert BE-Skalare und baut
native Vertex-/Normal-/Edge-, Polygon- und Blockreferenzen auf. Die originale
pointerfreie Polygonstruktur bleibt 20 Bytes. Offsets, Indizes, Blocklinks,
Dateigrenzen, Zyklen, Erreichbarkeit und endliche Werte werden geprüft.
GameCube-Offsets werden nicht zu truncierten Hostpointern.
Poolslots bleiben stabil: freigegebene Plätze werden nicht durch Umordnung
anderer Collider ersetzt. Kapazitätsüberschreitung und Poolresize bei lebenden
Einträgen werden abgelehnt; Owner-/Epoch-Handles erkennen alte Referenzen.
Aktivierung, Deaktivierung, Transformation, Freigabe und Reset sind überprüft.
Der vollständige `GameRuntime::initialize` räumt jetzt auch Merchant und
SAT/EAT auf; der Raum-Retirementpfad invalidiert seine SAT/EAT-Slots nach
dem Stoppen der besitzenden Tasks. Ein zusätzlicher Real-Daten-Lebenszyklustest
legt SAT und EAT aus dem tatsächlich geladenen ersten Archiv jeder Disc nach
dem Erfassen der Lesemessungen im vorhandenen Runtimebesitzer an, setzt diesen
in einem ausdrücklich isolierten Lebenszyklustest zurück,
verwirft beide alten Handles und erstellt beide Ressourcen erneut. Die
vorhandenen Raum-/ESL-Nachweise werden dadurch nicht durch Actor-Stubs ersetzt.
Der zukünftige `gameRoomInit`-Aufrufer muss die originalen
`CONS_R_NSAT/CONS_R_NEAT` liefern; dieser Raumregistrierungspfad wird jetzt
noch nicht als ausgeführt behauptet.

`SEck`, Polygonbits und Bypassreferenz sind an den kontrollierten Ownerthread
gebunden. Die originale Polygonreihenfolge bleibt erhalten; die Formierung
eines Pointers vor dem Anfang des Indexarrays wurde durch gleichwertiges
Lesen/Inkrementieren ersetzt. `Get_poly_attr` liest den dekodierten Status
anstatt eines Big-Endian-Halfword-Overlays. Keine Threads werden pro Actor
oder Motion angelegt; der vorhandene Scheduler bleibt der zentrale Taktgeber.

`GetDistance3` verwendet den originalen Differenzvektor mit nativer skalarer
Quadratwurzel; Paired-Single-/SDK-Operationen werden über die bestehenden
skalaren Matrixadapter und vollständigen SDK-C-Körper ausgeführt. Weder NEON-
noch GPU-Skinning wurde als ungeprüfter Ersatz eingeführt. Bytegleichheit
zwischen den nativen ABIs beweist keine Bitgleichheit zur PPC-Hardware.

## Tatsächlicher GameTask-Fortschritt und Actor-Grenze

Die vier Real-Daten-Boots pro ABI (beide Discs, zwei Wiederholungen) führen
die bisher integrierten Message-, SubScreen-, Checkpoint-, ID-, Licht-,
Scenario- und `PlayerInit`-Körper sowie jetzt Merchant aus. Gemessen werden
Freundschaft 50, ungebundener Merchant-Character und `Rno0 = 1`.
`PlayerInit` bleibt die originale globale Spielerinitialisierung;
`player_actor_constructed` bleibt **false**.

Dieser Nachweis verwendet den unveränderten Standardzustand von
`systemWorkInit`. Sein `SYS_NEW_GAME`-Inventarzweig ist nicht aktiv. Die dort
noch fehlenden `ItemMgr.gameInit`, `SceAtInitSaveItem` und `GamePointInit`
werden nicht durch Löschen von Flags umgangen und sind weiterhin offene
Abhängigkeiten eines vollständigen neuen Spielstarts.

Der nächste tatsächlich erreichte Fehler lautet:

```text
Original consumer unavailable: gameStageInit/cMes/GameSave
```

Das ist die noch unportierte Funktion als Ganzes, kein behaupteter Fehler
nach ausgeführtem `GameSave.save`. `src/game/game.cpp:292` enthält
Continue-/Costume-Zustände, Messageauswahl, Discwechsel und anschließend
`SetGameTime`, `GameSave.save(pSaveData, -1)`, `StageSet` und `Rno0 = 2`.
Die bisherigen Save-Körper bearbeiten Raumdaten und native Save-Sektionen;
der vollständige aggregierte Save aller Manager ist noch nicht integriert.
`StageSet` ist bereits vorhanden, darf aber nicht durch künstliches Setzen
von `STA_SAVEDATA_NO_UPDATE` an dieser Reihenfolge vorbeigeführt werden.

Die anschließende Actor-Abhängigkeit ist ein zusammenhängender Originalpfad:

1. `gameRoomInit` in `src/game/game.cpp:354` richtet CNS-große Manager ein,
   registriert SAT/EAT samt Effektinformationen und ruft `EmMgr.create(0, 0)`
   **vor** SMD-Setup, SAR-/ETM-/ETS-Einbindung und Kamerainitialisierung auf.
2. `cEmMgr::construct` in `src/game/em.cpp:76` konstruiert bei Player-ID 0
   wirklich `cPlLeon` im Workpool. Das feste originale GC-Worklayout darf
   nicht als native ARM64-Actorallokation wiederverwendet werden.
3. `cPlayer` in `src/game/player.cpp:202` benötigt echte Actor-/Work- und
   Motiontabellen, Körper-/Waist-/Neckzustände, Kollision und Managerbindung.
4. `cPlLeon::cPlLeon` in `src/game/pl_leon.cpp:26` führt `init0`, `setModel`,
   `weaponRelease/weaponLoad/weaponInit`, `init1`, `setMotion`, `EspDataLoad`
   und `startUp` aus. Modell-/Motionressourcen allein erfüllen diese Kette nicht.
5. `cPlayer::startUp` in `src/game/player.cpp:277` verwendet originale globale
   Position/Ausrichtung, Updates und Motion sowie `SatMgr.check` und
   Matrixaktualisierung. `cSatMgr::check/scrAtCheckSphere/wallAdjust/polySphereCk`
   fehlen noch. Neue Strahlabfragen sind dafür eine Abhängigkeit, kein Ersatz.
6. Weapon-Actor-/Ausrüstungszustände, Model-/Damage-/Effectregistrierung und
   das echte Playerupdate müssen gemeinsam an diese Besitzer gebunden werden.

Der aktuelle Patch konstruiert keine verkürzte `cPlayer`-Klasse, ersetzt keine
Weaponfunktionen und meldet keinen künstlich erfolgreichen Actorstart.
Die zentrale bestehende `PadRead`-/Key-Anbindung bleibt erhalten. Ihre
automatisierten Analog-/Trigger-/Mappingregressionen bestehen; physische
Tastendrücke wurden in dieser Fortsetzung nicht erneut angefordert.

## Motion, IK und numerischer Nachweis

Je Real-Daten-Boot werden beide originalen Leon-Modellinitialisierungen mit
119 Gelenken, acht Modellinfos und 14 Motionbindings geprüft. Sieben FCV-/SEQ-
Paare werden jeweils 16 Ticks persistent ausgeführt. Pro ABI ergeben sich
896 dieser protokollierten Motion-Ticks aus zwei Discs, zwei Wiederholungen
und zwei Modelllebenszyklen, zusätzlich vollständige Loopdurchläufe,
Rückwärtslauf, Pause, halbe Tickgeschwindigkeit und Startframe-Prüfungen.
SEQ-Frame/Framekey/SE/Free/State werden unabhängig aus Originalbytes geprüft.
Alle geprüften Gelenk- und IK-Matrizen bleiben endlich. Motionkreuzungen
werden mit dem originalen `MotionCheckCrossFrame` geprüft.

Diese Tests benutzen `ikOff = false`, `atariFlags = 0x100` und einen leeren
originalen SAT-Manager. Eine Bodenabfrage liefert damit den originalen
Kein-Treffer-Wert; es wird keine künstliche Bodenhöhe eingespeist. Die
IK-Algorithmen laufen, eine richtige geerdete Leon-Startpose ist damit nicht
bewiesen. Die ausgegebenen Motionzustände sind auf allen vier ABIs und
Wiederholungen gleich. Vollständige tickweise Posematrizen werden in diesem
Bericht nicht als PPC-Referenzvergleich ausgegeben.

Ein als synthetischer numerischer Test gekennzeichneter Zwei-Knochen-Aufbau
prüft zusätzlich echte `IKInit/ikCalc` bei Zielabständen 80, 120, 150, 200 und
250 gegen unabhängige analytische Kreisgeometrie; Toleranz 0,002 Original-
einheiten. Das ist kein erfundenes Spielmodell und wird nie ins Spiel geladen.

Die bestehende Original-Skinning-/FCV-Suite wird ebenfalls erneut ausgeführt:
18 Ausführungen und 1.739.820 skalare Referenzprüfungen je ABI plus 617
Adapterprüfungen. Maximale Abweichungen gegenüber dem unabhängigen Decoder:

| Messgröße | Maximum |
|---|---:|
| Quantisierte PSQ-Vertex-/Normalwerte | 0 |
| Weight-Matrix, linearer Anteil | 1,19e-7 |
| Weight-Matrix, Translation | 0,011290427 Originaleinheiten |
| FCV-Position | 0,004801790 Originaleinheiten |
| FCV-Winkel | 1,578619e-6 |
| FCV-Skalierung | 2,4e-8 |
| Verglichene ARM32-/ARM64-Matrix-/Posewerte | 0 |

Diese ältere Suite prüft auch reale Eventmodelle, das Auto und erweiterte
Weightressourcen aus anderen Räumen. Sie ist ein isolierter Skinningnachweis.
Hokan, Sekundärmotion, angehängte Kamera, Morph-/ShapeMove, Cloth sowie das
Dispatch von SEQ-Sound-/Gameplayereignissen bleiben offen. Originale
Root-Motion-Körper bleiben erhalten; ein durch Originalplayerentscheidungen
erzeugter Root-Motion-/Kollisionsverlauf wird noch nicht behauptet.

## SAT-/EAT-Originaldaten und Referenzprüfungen

Je Durchlauf werden zehn echte Kollisionsblöcke geprüft:

| Disc (Originalnummer) | Raum | SAT-Proben | EAT-Proben |
|---|---|---:|---:|
| 0 | r100 | 833 | 831 |
| 0 | r102 | 883 | 836 |
| 0 | r119 | 954 | 816 |
| 0 | r120 | 98 | 98 |
| 1 | r300 | 814 | 802 |

Das sind 6.965 Segmentabfragen pro Durchlauf, **13.930 je ABI** bei zwei
Wiederholungen. Die Testproben werden aus echten Dreiecken abgeleitet, nicht
als neue Kollisions- oder Raumdaten ins Spiel eingeführt. Ein unabhängiger
Double-Precision-Ebenen-/Baryzentriktest prüft Treffer, Punkt, Normale und
Attributfilter. Maximale Punktabweichung: **0,01512500001** Originaleinheiten;
Testgrenzen: Punkt 0,2 und Normalen 0,0001. Trefferattribute stimmen exakt.
Der separate Metadatenvergleich aller vier ABI-Berichte ergibt maximale
Punkt-/Normalabweichung 0 und identische Quellenhashes, Zähler und Attribute.

Zusätzliche native Prüfungen vergleichen `getFloor` mit dem gleichen
originalen Segment, Koordinatentransformationen, Ein-/Ausschalten, volle
Pools, Resize bei lebenden Slots, Freigabe, erneute Erstellung, alte Handles,
trunkierte Dateien und unveränderte Quelldaten. EAT-Effektcallbacks,
Wasser-/Raumeffekte und Actor-Kugelverdrängung werden damit nicht als fertig
ausgegeben.

## Kamera, Introauto und Helligkeitsgrenze

`CameraQuasiFPS::init/move/hitCheck` in `src/game/cam_qfps.cpp:1152/1189/705`
benötigen einen wirklichen Spieler, dessen Zustände und SAT-Hindernisbehandlung.
Diese Laufzeit ist noch nicht integriert. Die GPU-Diagnose liest originale
EVD-Kameradaten; sie trifft keine originale Live-Kameraentscheidung.

`r100_StartEvent` in `src/st1/r100.cpp:550` wartet auf das wirkliche Event 9,
bevor es Leon auf `(-99685, -484, -1343)` mit Yaw `2.246` setzt. Diese Zahlen
sind ein Quellnachweis, keine in den Renderer kopierten Startparameter.
Das Ereignis wird nicht künstlich abgeschlossen. Der vorhandene Dolphin-
Screenshot zeigt einen anderen nachfolgenden Spielzustand; eine identische
Pose, Kamera und Eventphase zu den neuen GPU-Aufnahmen ist nicht bewiesen.

`obm1a00` bleibt ein reales CPU-Modell mit 24 Gelenken und originaler Motion
`event/r120/s00/obm1a00/obm1a00_s00_001.fcv`. Seine echte Sichtbarkeit,
Partskalierung, OT-Einordnung und Callbacks kommen aus `EventCarInit` in
`src/st1/r120.cpp:462` und den Live-Eventpaketen. `Event::init/Run` in
`src/game/event.cpp:152/205` benötigt Actor-/Objektregistrierung, Parent-
Transformationen, Position-/Motionpakete, Scheduler, Fade, Audio und Cutwechsel.
Diese Integration fehlt. **Das Auto wurde nicht statisch in den Diagnoseframe
eingefügt; seine Original-Eventregistrierung ist nicht als abgeschlossen zu
werten.** `R120Event` bleibt bei `Sofdec.Initialize/opening.sfd` begrenzt.

Der normale r100-Lichtpfad meldet weiterhin fehlenden LIT-Parenttyp 1, ID 0
(lebender Player in `EmMgr`). Modellgebundene Lichtmathematik und Original-
LIT-Daten sind vorhanden, ihre Ownerreferenz ist im Diagnosezustand nicht
erfüllt. SAR-Flächen und vollständiger Live-Licht-/Umweltupdate bleiben offen.
Es wurde weder eine neue Lichtquelle noch ein Aufhellungsfaktor eingeführt.

## Echte GPU-Aufnahmen und Vulkan-Validation

Vier frische Retroid-Captures verwenden den gemeinsamen bestehenden Renderer:
`r100`, `Evd/r100s40.evd`, Cut 0, Frame 30, `pl0000`, **TextureDebug 1**.
Die Kamera stammt aus `event/r100/s40/cam/cam_s40_000.fcv`;
FOV 28,6077805 Grad. Die Aufnahmen zeigen die Brücke und Leon am linken Rand.
Sie sind ausdrücklich unbeleuchtete Originaltextur-Diagnosen, keine r100-
Start-Schulterkamera, keine ausgeführte Eventanimation und kein neues Gameplay.

Private GPU-Bilder (Originalpixel, außerhalb von Git):

- [ARM32 GPU-Capture](<E:/Development/Android/RE4-Data/phase5b-gpu-r100/gpu-armeabi-v7a-1.png>)
- [ARM64 GPU-Capture](<E:/Development/Android/RE4-Data/phase5b-gpu-r100/gpu-arm64-v8a-1.png>)
- [ARM32 tatsächliches Display](<E:/Development/Android/RE4-Data/phase5b-gpu-r100/display-armeabi-v7a-1.png>)
- [ARM64 tatsächliches Display](<E:/Development/Android/RE4-Data/phase5b-gpu-r100/display-arm64-v8a-1.png>)

Je ABI liegen zusätzlich zweite Capture-, Display-, RGBA- und JSON-Belege in
demselben privaten Verzeichnis. Alle vier RGBA-Readbacks sind bytegleich,
SHA-256 `7c2676d8361694ceb5f9db106b8c3d8bec3126caa032b96be519b4ceb1cf5cd6`.
Je Capture: 512 x 448 Pixel, 226.966 Nicht-Clear-Pixel, 317 Modelle,
372.702 Vertices, 347.330 Dreiecke, 504 Draws, 510 Texturen und sechs
gewichtete Modelleinträge. Framezähler: ARM32 60/14, ARM64 27/21.
Die Zähler hängen von der asynchronen Observerabfrage ab und bedeuten keine
vergleichbaren Gameplayticks. Activity-Recreation im selben Prozess prüft
zweimal pro ABI die bisherigen nativen/GPU-Lebenszyklen.

Khronos Validation 1.3.290 war auf beiden ABIs aktiv, mit **0 Fehlern**.
Pro Surface wird eine bekannte Performancewarnung ausgegeben:
`WARNING-Swapchain-PreTransform`, Identity statt `ROTATE_90` als aktueller
Surfacetransform; die Präsentationsengine führt die Rotation aus.
Diese Warnung bleibt offen und wird nicht als warnungsfreier Lauf ausgegeben.
Log: `port/build/phase5b-vulkan-logcat.txt`. Kein Descriptor-, Lebenszeit- oder
Synchronisationsfehler wurde in diesen Läufen gemeldet.

Für diese ausdrücklich unbeleuchtete Texturdiagnose sind die Helligkeitswerte
auf beiden ABIs gleich (gewichtetes **kodiertes RGB8**, keine photometrische
Luminanz): Mittel 49,616669, Median 48,1352, P05 9,0722, P95 92,2848;
8,8109 % der Pixel liegen unter 16, 0,65395 % über 128.
Das beweist weder richtige originale Beleuchtung noch die Behebung der
historisch dunklen normalen Darstellung. EFB/Texturen/Swapchain bleiben
UNORM, Format 37, Colorspace 0; keine Gamma- oder sRGB-Korrektur wurde geändert.

Gemessene letzte CPU-Framezeiten sind ARM32 47,205/47,259 ms und ARM64
40,124/36,150 ms. Laden bis mindestens zwölf Diagnoseframes dauerte
23,953/11,484 beziehungsweise 11,157/9,469 s. Dies sind einzelne Debug-
Messungen einschließlich Diagnose-/Readbackkosten, kein GPU-Timestamp-
Benchmark und keine nachgewiesene stabile Gameplay-Framerate.

## Ausgeführte Builds und Tests

Gerät: **Retroid Pocket Flip2**, Serial `dbdcacc`, Android 13/API 33,
ABIs `arm64-v8a,armeabi-v7a,armeabi`, Adreno 650, Vulkan 1.1.128.
Beide getesteten Prozesse bestätigen ihre tatsächlichen 32-/64-Bit-Pointer.
Windows verwendet MSVC für x86/x64; Android verwendet das vorhandene NDK,
CMake/Gradle, APK und AndroidTest-APK. Der finale Android-Build einschließlich
Lint meldet `BUILD SUCCESSFUL`.
Der Android-Build meldet außerdem `CXX5304` wegen SDK-XML-Version 4 gegenüber
Version 3 in den vorhandenen Buildtools. Diese Werkzeugwarnung verhindert
den geprüften Build nicht und ist von den Vulkan-Meldungen getrennt.

| Prüfung | Windows x86/x64 | Retroid ARM32/ARM64 | Umfang / Grenze |
|---|---|---|---|
| CTest | 27/27 je ABI | enthaltene native Suiten zusätzlich unten | Originalscalar, Scheduler, Loader, Module, Modell-, Daten-, Quellhash- und Generatorregressionen; isolierte Fixtures entsprechend markiert |
| Game-Runtime-Originaldaten | bestanden, je zweimal | bestanden, je zweimal | zwei Discs, Merchant/GameTask-Standardboot, vorhandene Message/SubScreen/ID/Save/Licht-/Player-Asset-Körper, reale Motion-/SEQ-Ticks |
| SAT/EAT | bestanden, 13.930 Proben je ABI | bestanden, 13.930 Proben je ABI | unabhängiger Originaldaten-Orakeltest, keine Actor-Kugelverdrängung |
| Originales Skinning/FCV | bestanden, 1.739.820 Prüfungen je ABI | bestanden, 1.739.820 Prüfungen je ABI | PSQ-/Weight-/FCV-Referenzen, isolierte Eventposen |
| GX-Szenen-CPU | bestanden, je zweimal | bestanden, je zweimal | r100-Primitive/TPL/Material/OT/Kameradaten, ausdrücklich Debug-Lichtpfad |
| Scheduler/Core | Bestandteil der Hosttests | je zweimal, ARM32 13.386 / ARM64 13.387 Checks pro Lauf, 0 Fehler | identische Scheduler-/Background-/Yz2-Semantiktraces; ein zusätzlicher High-Pointer-Test auf ARM64 |
| APK-Instrumentierung | Android-Build erstellt | 20/20 Tests je ABI | Controlleradapter, zentraler Padpfad, Loader/Frame, Module, Originalcode, Vulkan-Clears/Präsentation |
| GPU-Geometrie | keine Windows-Vulkan-Ausgabe behauptet | vier gültige Captures | bytegleiche Diagnoseframes, Validation aktiv, 0 Fehler/1 Warnung pro Surface |
| Real-Daten-Ressourcen | bestanden, je zweimal | bestanden, je zweimal | 157/151 Ressourcenausführungen und 85/57 Räume pro Disc/Durchlauf; Datenresidenz und Lebenszyklen |
| Original-REL-Module | bestanden, je zweimal | bestanden, je zweimal | 110/4 RELs pro Disc/Durchlauf, partielle native Registrierung; kein PPC-Code ausgeführt |

Der gemeinsame Metadatenbericht `phase5b-summary/consumer-runtime-summary.json`
prüft Wiederholungen und vier ABIs. Je ABI: 20 Raumvorbereitungen, 180
gezählte Original-Modellinitialisierungen der Event-/Zusatzmodellpakete,
acht `SubScreenGameInit`-Aufrufe und vier Original-GameTask-Boots.
Der separate Real-Daten-Boot zählt je Lauf 350.189 Prüfungen;
die isolierte native Runtime-Suite 1.073. Native Layoutunterschiede
(`ID_UNIT`: 312/344 Bytes; `SAVE_DATA_HEAD`: 28/56 Bytes) sind dokumentiert
und aus dem semantischen Gleichheitsvergleich getrennt.
Die CPU-Lichtsuite behält 11.049/8.399 Prüfungen pro Disc bei und benennt
nicht auflösbare Modellbesitzer ausdrücklich.
ASan/UBSan wurden nicht ausgeführt: Die bestehende Buildoption verweigert
Sanitizer ohne geprüfte Fiberhooks. Grenzen-/Lease-/Handle-/Wiederholungs-
tests und Vulkan-Validation sind konkrete Prüfungen, kein vollständiger
Beweis der Abwesenheit aller Speicherfehler.

Entwicklungsfehler wurden vor dem finalen Stand korrigiert: native
Header-/Makrokonflikte und originale Tabellenwarnungen verhinderten zunächst
den Build; Warnungsunterdrückung bleibt auf erzeugte Originalkörper begrenzt.
Eine Bodenvergleichsprobe verwendete zuerst einen anders gerundeten
Endpunkt; der Test rekonstruiert nun genau das Segment des Originals, ohne
Kollisionsalgorithmusänderung. Die letzte Quellprüfung fand zudem fehlende
Merchant-/SAT-/EAT-Freigabe im vollständigen Runtime-Reset; diese ist vor
dem finalen Build ergänzt und durch native sowie Real-Daten-Resettests geprüft.
Der erste zusätzliche Resettest versuchte zwei GameRuntime-Scheduler auf
einem Ownerthread anzulegen und wurde korrekt mit
`Only one original task domain per owner thread` abgewiesen. Der Test verwendet
nun die eine bestehende Runtime nach dem Erfassen der Original-Lesemessungen;
die Scheduler-Schutzregel wurde nicht abgeschwächt.
Der erste Host-CTest-Lauf im Sandboxkontext
hatte Temp-/Dateizugriffsfehler; die verfügbaren Tests wurden mit zulässigem
Zugriff erneut vollständig ausgeführt. Die abschließenden oben genannten
Prüfungen bestehen; die ausdrücklich fehlenden Funktionen bleiben Fehler.

## Reproduktion und Belegdateien

Alle generierten Originaldaten, Testfixtures, Inventare und GPU-Pixel bleiben
außerhalb von Git. `port/build` enthält nur ignorierte Build-/Logbelege.
Die privaten Ausgabeordner heißen:

```text
phase5b-host64, phase5b-host32, phase5b-device, phase5b-summary
phase5b-collision-host64, phase5b-collision-host32, phase5b-collision-device
phase5b-skin-host64, phase5b-skin-host32, phase5b-skin-device
phase5b-gx-host64, phase5b-gx-host32, phase5b-gpu-r100
phase5b-resources-host64, phase5b-resources-host32, phase5b-resources-device
phase5b-modules-host64, phase5b-modules-host32, phase5b-modules-device
```

PowerShell, vom Repositoryroot; dieselben vorhandenen Buildskripte wurden
verwendet. Für jeden Test muss ein getrenntes privates Ausgabeziel angegeben
werden. ARM32 und ARM64 werden von den Device-Skripten beide ausgeführt.

```powershell
$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskSdk = 'C:\Users\andiw\AppData\Local\Android\Sdk'
$taskAdb = "$taskSdk\platform-tools\adb.exe"
$taskData = 'E:\Development\Android\RE4-Data'
# In einer x64-VS-Compilerumgebung:
& "$taskSdk\cmake\4.1.2\bin\cmake.exe" -S port -B port/build/host-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release -DRE4_BUILD_TESTS=ON "-DPython3_EXECUTABLE=$taskPython"
& "$taskSdk\cmake\4.1.2\bin\cmake.exe" --build port/build/host-ninja --parallel 4
# Separat in einer x86-VS-Compilerumgebung:
& "$taskSdk\cmake\4.1.2\bin\cmake.exe" -S port -B port/build/host-x86 -G Ninja -DCMAKE_BUILD_TYPE=Release -DRE4_BUILD_TESTS=ON "-DPython3_EXECUTABLE=$taskPython"
& "$taskSdk\cmake\4.1.2\bin\cmake.exe" --build port/build/host-x86 --parallel 4
& "$taskSdk\cmake\4.1.2\bin\ctest.exe" --test-dir port/build/host-ninja --output-on-failure
& "$taskSdk\cmake\4.1.2\bin\ctest.exe" --test-dir port/build/host-x86 --output-on-failure
.\port\scripts\build-android.ps1 -SdkRoot $taskSdk -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python $taskPython -ValidationLayers "$PWD/port/build/validation/layers/android-binaries-1.3.290.0" -Offline
.\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-host64" --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-host32" --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_game_runtime.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_collision.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-collision-host64" --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_collision.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-collision-host32" --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
& $taskPython port/tools/test_collision.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-collision-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_skinning.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-skin-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_gx_scene.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-gpu-r100" --adb $taskAdb --serial dbdcacc --room 100 --event Evd/r100s40.evd --cut 0 --frame 30 --model pl0000 --debug-view 1 --repeat 2
& $taskPython port/tools/test_resources.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-resources-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/test_modules.py --data "$taskData\import-phase3a" --output "$taskData\phase5b-modules-device" --adb $taskAdb --serial dbdcacc --repeat 2
& $taskPython port/tools/summarize_consumers.py --host "$taskData\phase5b-host64" --host32 "$taskData\phase5b-host32" --device "$taskData\phase5b-device" --output "$taskData\phase5b-summary"
```

Für Skinning-, GX-, Ressourcen- und Modul-Hostregressionen ersetzt man
`--adb ... --serial ...` durch das jeweilige `--host-exe`:
`re4_gx_tool.exe`, `re4_gx_tool.exe`, `re4_resource_tool.exe`,
`re4_module_tool.exe`, jeweils in `port/build/host-ninja` beziehungsweise
`host-x86`, mit getrennten oben genannten Ausgabeordnern.
Die tatsächlich verwendeten, nicht eingecheckten `light-host*.cmd` sind
lokale VS-Umgebungshilfen, die `VsDevCmd.bat -arch=x64/x86 -host_arch=x64`
und die entsprechenden CMake-Buildaufrufe ausführen. Die Hostkonfiguration
benötigt diese Hilfsdateien nicht; `port/scripts/test-host.ps1` bietet den
vorhandenen Configure-/Build-/CTest-Einstieg in einer initialisierten
Compilerumgebung. Ninja und der konfigurierte Pythonpfad müssen verfügbar sein.

## Vollständig gespeicherte Projektdateien

Neue Dateien:

```text
port/config/collision-sources.json
port/config/merchant-sources.json
port/include/re4/port/collision.h
port/include/re4/port/merchant.h
port/src/collision.cpp
port/src/collision_original.h
port/src/merchant.cpp
port/src/merchant_original.h
port/tools/generate_collision.py
port/tools/generate_merchant.py
port/tools/test_collision.py
port/docs/phase5b-runtime-status.md
```

Geänderte vorhandene Dateien:

```text
port/CMakeLists.txt
port/README.md
port/config/consumer-sources.json
port/include/re4/port/consumers.h
port/include/re4/port/game_runtime.h
port/include/re4/port/motion.h
port/src/consumers.cpp
port/src/game_runtime.cpp
port/src/gx_original.h
port/src/gx_render_original.h
port/src/motion.cpp
port/src/motion_original.h
port/src/runtime_original.h
port/tests/game_runtime_tool.cpp
port/tests/game_runtime_verification.cpp
port/tests/test_consumer_sources.py
port/tests/test_runtime_sources.py
port/tools/generate_consumers.py
port/tools/generate_runtime.py
port/tools/summarize_consumers.py
port/tools/test_game_runtime.py
```

## Nächste Arbeiten innerhalb Phase 5b

Zuerst muss `gameStageInit` mit tatsächlichem aggregiertem Save und den
ursprünglichen Message-/Disc-/Statezweigen ausführbar werden. Parallel dazu
gehört die gemeinsame typed Actor-/Workverwaltung einschließlich Weapon-
und `cSatMgr::check`-Kette in eine konsistente native Lebenszeit. Danach
können `gameRoomInit`, echter Leon-Bodenkontakt, Motionentscheidungen und
`CameraQuasiFPS` in Originalreihenfolge laufen. Erst mit diesen Besitzern
können Live-Eventpakete das Auto, Parentlichter, SAR-Bereiche und die zur
Darstellung eingereihten Modelle steuern. Morph/Hokan/Cloth und SEQ-
Ereignisse müssen jeweils vor ihrer tatsächlichen Verwendung integriert
werden; vorhandene explizite Fehler dürfen nicht zu Erfolgs-Stubs werden.

Der Gegenvergleich mit einem identischen G4BE08-Spiel-/Kamera-/Animationsframe
steht weiterhin aus. Die vollständige Actor-/Event-/Kamerasimulation ist
der verbleibende Umfang **dieser Phase 5b**, keine eigenständig begonnene
Folgephase. MSAA Aus/2x/4x, AF 2x/4x/8x, Nah-/Zielfokus-DoF und persistente,
hardwarevalidierte Einstellungen bleiben verbindliche spätere Anforderungen;
dieser Teilstand ändert sie und den bestehenden Vulkan-/Controllerpfad nicht.

Git: `master` bleibt `9b76ff003081d6ece8b838453fb8310743a972c7`,
`origin/master` bleibt `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
Die aktualisierte README wurde berücksichtigt; keine Zusammenführung,
kein Rebase und kein Push. Nur vollständig gespeicherte Portquellen und
Dokumentation gehören zum lokalen geprüften Commit; keine Originalassets,
Buildprodukte, Images oder Screenshots.
