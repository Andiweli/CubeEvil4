# Phase 5a: Modelllebenszeiten, Eventlicht und r100-GPU-Diagnose

Fortsetzung von `c5c988d0092a3bb677a18c980504e391ec817a28` auf `android-port`,
11. Oktober 2026. Quelle: ausschließlich der lokale G4BE08-Import beider
Debug-Discs vom 25.11.2004. Private Daten und GPU-Aufnahmen liegen unter
`E:\Development\Android\RE4-Data`; keine davon wird eingecheckt.

## Ergebnis und Abnahmegrenze

Der gemeinsame SMD-/Modellpfad behält seine echten Modellobjekte jetzt bis
zum Abschluss der Licht- und OT-Erfassung. Poolgrößen und Slotreihenfolge
folgen den originalen CNS-Werten. Modellgebundene Lichtpositionen und
Richtungen werden durch die vollständigen Originalkörper berechnet;
Borrow-Tokens verhindern die Freigabe oder Verlagerung referenzierter Modelle.

**Die vollständige r100-Spieler-Startszene ist noch nicht erreicht.**
Der normale r100-Renderpfad scheitert weiterhin am tatsächlich fehlenden
Gameplay-Spieler in `EmMgr`: LIT-Parenttyp 1, Nummer 0. Der native pl00-
Asset-Loader ist kein konstruierter `cPlLeon`-Actor. Originale Startpose,
SAT-Bodenkontakt und `CameraQuasiFPS` werden nicht als ausgeführt behauptet.

r100, r102 und r119 liefern echte Vulkan-Geometrieframes auf beiden Retroid-
ABIs über denselben Renderer. Diese Frames sind ausdrücklich **unbeleuchtete
Debug-Ausgaben mit originalen EVD-Kameratracks**, kein laufender Gameplayframe
und keine Schulterkamera. Alle geladenen Raumblöcke sind gleichzeitig resident;
die spielerabhängige Live-Blockauswahl bleibt offen.

Der normale isolierte r120-Cut 0 verwendet jetzt seine tatsächliche
Event-LIT-Ressource. Getrennte Licht-, Material-, Textur-, Vertexfarben- und
Fog-Aufnahmen dokumentieren die Helligkeitsgrenze. Ein identischer
GameCube-Referenzzustand wurde nicht nachgewiesen; visuelle Originaltreue und
die vollständige Behebung der dunklen Darstellung sind deshalb **offen**.

## Nachgewiesene Originalfunktionen

| Originalquelle | Neue Ausführung / Verbindung |
|---|---|
| `src/game/light.cpp:1265/1290/1338` | `cLight::getCoord`, `calcPos`, `getNormal`: tatsächliche Parent-Parts, Positions- und Richtungstransformationen |
| `src/lib/mtxvec.c`, `C_MTXMultVecSR` | Vollständiger CPU-Körper; Translation wird bei Richtungen nicht angewendet |
| `src/game/scroll.cpp`, `SmdGetGroupObjPtr2` | Vollständiger Originalkörper für gruppierte Scrollreferenzen |
| `src/game/cons.cpp`, `ConsInitRoom/ConsGetRoomValue` | Originale Defaults und CNS-Overrides bestimmen alle drei Modellpools |
| `src/game/light.cpp`, `cLit::versionUp/cLightMgr::roomLitSet` | Eigentümlich gebundene, native Kopie der echten Event-LIT-Datei |
| `src/game/motion.cpp:1239/1501` | `HermiteInterpolation/Fcc_get_data_033`: exakte FCC15-Key-Abfragen, benötigt für die r102-EVD-Kamera |

Bestehende `calcParent/loadLit/create/Light00_Move`, `setModel2/LightSetModel`,
Modellinitialisierung, CPU-Skinning, Motion-FK, Material-/Specular-/Bump-Körper
und Original-OT-Verarbeitung werden wiederverwendet. Die Generierungsmanifeste
verzeichnen Originalsignatur, Quelle, Zeile und native Anpassungen. Quellhashes
werden vor der Generierung geprüft. Kein Originalquelltext wurde verändert.

Der zusätzliche FCC15-Pfad erfindet keine lineare oder gestufte Interpolation:
`Fcc_get_data_033` schreibt ausschließlich Werte, keine Tangenten. Exakte Keys
und das originale Halten des letzten Keys funktionieren. Ein Zwischenwert
wirft vor dem ursprünglichen Zugriff auf undefinierte Tangenten einen expliziten
Fehler. r102 liefert für die geprüfte Kamera ganze Frames 0 bis 34 als echte Keys.

## Modellpools und Parent-Lebenszeiten

`src/game/game.cpp:383/389/401` addiert `SmdGetObjNum()` zu
`CONS_R_NMODELINFO`, `CONS_R_NPARTS` und `CONS_R_NOBJ`. Der native Adapter
verwendet genau diese Werte, begrenzt die Summen und prüft den tatsächlichen
Partsbedarf. Beispielsweise ergeben r120s Defaults und seine 241 SMD-Works
Objekt-/ModelInfo-/Parts-Pools von **441 / 541 / 1541**.

Die originale `createBack`-Reihenfolge bleibt erhalten. Ein LIT-Objektindex
ist deshalb kein Index in eine nachträglich verdichtete Modellliste. Parenttyp
2 wird über die originale Scrolltabelle, Typ 4 über den tatsächlichen lebenden
Objektpoolslot aufgelöst. Typ 1 und Typ 3 benötigen die noch fehlenden Live-
Registrierungen von `EmMgr` und den ETC-Managern; es gibt keine Ersatzzuordnung
zu einem isolierten Eventmodell.

`EngineConsumers::borrowModel/checkModelRelease` verhindert Release und Clear
bei lebenden Lichtreferenzen. `BlockDestroy` prüft vorab alle zugehörigen
Modelle, bevor irgendein Gruppenmitglied freigegeben wird. Fehlgeschlagene
Lichtregistrierungen räumen ihre Borrow-Tokens auf. Lichtfreigabe lässt das
Modell bestehen; Modellfreigabe invalidiert dessen Handle. Poolneuanlage mit
lebenden Modellen bleibt verboten.

Die Scene-Erfassung behält den SMD-Domain über Material-, Licht- und OT-
Erfassung. Anschließend werden die echten Modelle freigegeben. GPU-Buffers
und immutable ResourceLeases besitzen ihre Daten selbst; keine temporären
ModelCpu-Pointer gelangen in den GPU-Frame. Die isolierte EVD-Skinning-
Erfassung hat einen getrennten Consumer-Besitzer, damit sie die SMD-Pools
nicht durch `clear()` zerstört. Diese Trennung ist eine Diagnosegrenze;
die gemeinsame lebende Actor-/EventObject-Registrierung fehlt weiterhin.

## Nachgewiesener Fehler in der Event-Lichtauswahl

`Event::ExePacket_Lit` in `src/game/event.cpp:1285` löst den Paketnamen über
`EvtMgr.GetBin` auf, ruft `LightMgr.roomLitSet(dat)` und danach
`LightMgr.update(0, -1)` auf. Die vorherige Renderdiagnose verwendete stattdessen
den Raum-LIT-Eintrag mit dem Index des Kameracuts. Kameracut und Lichtcut sind
keine austauschbaren IDs.

Der begrenzte Paketadapter sucht nun das letzte zutreffende originale
Lit-Paket desselben Cuts bis zum angeforderten Frame, übernimmt dessen
Ressourcenname und verwendet dessen Cut 0. Namen, Paketgrenzen, Referenzen
und Quellhash werden unabhängig geprüft. Ohne zutreffendes Paket wird
abgebrochen. Vollständiges `Event::Run/ExePacket_Lit`, `LightMgr.update/setEnv`,
Hokan, Wind, Filter und SAR werden dadurch nicht simuliert.

- r120 Cut 0: `event/r120/s00/etc/s00_000.lit`; keine Modellparents erforderlich.
- r120 Cut 1: `event/r120/s00/etc/s00_001.lit`; Parenttyp 4 verweist auf
  Objektpoolslot `0xC3` (195). Der echte Event-Objektbesitzer fehlt. Es wird
  **keine** vermeintliche Zuordnung dieses Slots zu `pl0000` vorgenommen.
- r100s40: `event/r100/s40/etc/s40_000.lit` im geprüften Cut; benötigt
  Parenttyp 1, Nummer 0, also den echten Spieler.
- r102s00: benötigt ETC-Parents und Spieler; r119s10 ebenfalls Spieler/ETC.

Eine anfängliche Arbeitshypothese, fehlende Licht-Farbcallbacks seien die
Ursache der Dunkelheit, hat sich nicht bestätigt: Der Originalkörper von
`cLightMgr::create` ruft bereits `move()` auf. Ein Test prüft ausdrücklich
die resultierenden `DispCol`-Werte. Diese Hypothese ist keine Fehlerdiagnose.

## Helligkeitsdiagnose und Farbraum

Debug-Modi: 1 Basistexturnachschlag, 2 Materialfarbe, 3 Vertexfarbe,
4 Beleuchtung, 5 TEV ohne Fog, 6 Fog-Faktor. Modus 0 erhält den begrenzten
erfassten Material-/Lichtzustand. Geometrie, OT, Tiefenverarbeitung und
originale TEV-Alphaabdeckung bleiben aktiv. Modi 1 bis 3 dürfen eine fehlende
Lichtregistrierung ausschließlich als sichtbar markierte Debug-Ausgabe
übergehen. Modi 0 und 4 bis 6 brechen an fehlenden Live-Parents ab.

Messung aus dem tatsächlichen GPU-Readback, 512 × 448 Pixel: gewichteter
RGB8-Codewert `0.2126 R + 0.7152 G + 0.0722 B`, **keine photometrische
Luminanz**. Alle Werte der Tabelle sind auf ARM32/ARM64 bytegleich.

| Originaler Diagnosezustand | Modus | Mittel | Median | P95 | Anteil <16 |
|---|---:|---:|---:|---:|---:|
| r100s40 Cut 0, Frame 30 | Textur | 49,9687 | 48,4934 | 94,4986 | 9,743 % |
| derselbe r100-Zustand | Material | 248,6592 | 255,0000 | 255,0000 | 0,000 % |
| derselbe r100-Zustand | Vertexfarbe | 41,0980 | 0,0000 | 128,0000 | 66,360 % |
| r102s00 Cut 0, Frame 30 | Textur | 64,8044 | 63,5496 | 117,4734 | 5,684 % |
| r119s10 Cut 0, Frame 30 | Textur | 78,1779 | 77,8596 | 116,4292 | 0,281 % |
| r120s00 Cut 0, Frame 30 | erfasste TEV/LIT/Fog | 133,9844 | 134,1926 | 134,1926 | 0,000 % |
| derselbe r120-Zustand | Beleuchtung | 75,4952 | 87,0000 | 87,0000 | 14,311 % |
| derselbe r120-Zustand | TEV ohne Fog | 12,3484 | 13,8556 | 24,0000 | 70,078 % |
| derselbe r120-Zustand | Fog-Faktor | 253,5877 | 254,0000 | 254,0000 | 0,000 % |

Die r100-Aufnahmen lokalisieren sichtbare Texturdaten und Materialfarben,
beweisen aber keine korrekte Spielerbeleuchtung. Vertexfarben enthalten
tatsächlich große schwarze Bereiche; `LightSetModel` wählt bei BIN-Flag
`0x40000000` genau diese Farben als Ambientquelle. Sie dürfen nicht generell
durch Weiß ersetzt werden.

Beim geprüften r120-Cut erzeugt TEV ohne Fog ein dunkles Bild, während der
Fog-Faktor fast voll wirkt. Der vollständige erfasste Frame ist deshalb
weitgehend die original gelesene Nebelfarbe. Die privaten Zustandsreports
enthalten pro Material Ambientquelle, Rasterfarbe, Lichtmasken, alle GX-
Lichtparameter, TEV-Skalen und Fog-Werte. Beispielsweise verwendet das erste
Material Vertex-Ambient, Raster RGB 245, TEV-Skala 1 und Lichtmaske 3.
Der Fog ist Typ 5, Start 6, Ende 106674 und Farbe RGB 141/135/120.
Diese Werte stammen aus dem ausgewählten Event-LIT; ihre Live-Anwendung
einschließlich Hokan und Filter bleibt offen. Der fast uniforme Frame ist
keine behauptete visuell korrekte Introaufnahme.

Originale LightSetModel/lightSetColor/lightSetLocalAmb und die Typfunktionen
aus `src/game/trans_lit.cpp` bestimmen Ambient, Diffuse und Specular.
`src/game/main_sub.cpp:118` setzt `GXSetDispCopyGamma(0)` (Gamma 1,0).
TEX, EFB und Retroid-Swapchain sind `VK_FORMAT_R8G8B8A8_UNORM` (37),
Swapchain-Colorspace `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` (0). Der Shader
führt keine zusätzliche sRGB-Transferfunktion oder globale Aufhellung aus.
Diese geprüfte Bytekette belegt keine endgültige GameCube-XFB-/VI-
Farbgleichheit. GX-Copyfilter, XFB/YUV-Konvertierung und originale
Postprocessingpässe sind weiterhin getrennte Rendering-Abhängigkeiten.

Der historische dunkle r120-Cut-1-Readback hat Mittel 8,1017, Median 5,1404,
P95 29,7742. Er verwendete die falsche LIT-Zuordnung. Der neue Cut-0-Frame
ist **kein** zulässiger Vorher-/Nachher-Vergleich desselben Zustands.
Die falsche Auswahl ist behoben; eine quantitative visuelle Fehlerkorrektur
für genau denselben vollständig laufenden Originalframe ist noch nicht bewiesen.

## Android-Laufzeit und GPU-Sicherheit

`GraphicsBootstrapView` lädt und rendert jetzt auf einem geordneten
HandlerThread je Surface. JNI/Vulkan und die threadlokalen Originaldomänen
bleiben auf demselben Besitzerthread. Choreographer stellt höchstens einen
Frame gleichzeitig zu; der bestehende zentrale ControllerLoop/PadRead bleibt
erhalten. Surface-Retirement reiht Close hinter Load/Frame ein, ohne erzwungenes
Suspendieren oder UI-Join. Veraltete Observer-Ergebnisse werden verworfen.

Der vorherige UI-Load erzeugte bei r100 einen echten ANR. Mit dem neuen
Besitzerthread blieb die UI frei; der unoptimierte CPU-Aufbau benötigte aber
174,367 s und überschritt fast die GPU-Testfrist. Android-Debug verwendet
jetzt `-O2` mit Debugsymbolen und aktiven Assertions, weiterhin ohne Fast-Math
und mit unveränderter FMA-Policy. r100 benötigt im endgültigen Testlauf
6,250 s CPU / 0,893 s GPU-Setup auf ARM32 und 4,113 s / 0,729 s auf ARM64.
Gemessene synchrone Frame-CPU-Zeiten: 45,762 / 36,037 ms. Das sind
Diagnosezeiten einschließlich Synchronisation, kein Gameplay-FPS-Benchmark.

Vulkan erstellt nur tatsächlich benötigte Blend-/Cull-Kombinationen und
verwendet einen gerätegebundenen Pipelinecache. Debug-Shader werden über eine
Specialization-Konstante gewählt. Es gibt keine raumspezifischen Pipelines.
MSAA 0/2/4, AF 2/4/8 und DoF mit Nah-/Zielfokus und persistenten Einstellungen
bleiben Anforderungen; aktive Werte sind weiterhin ehrlich 1 Sample, AF 1,
DoF aus. Keine dieser Menüoptionen wurde in diesem Schritt fertiggestellt.

## Größere gemeinsame Engine-Grenze

Die Quellen belegen eine zusammenhängende fehlende Live-Room-/Actor-/SAT-
Laufzeit, nicht bloß einen fehlenden Shader oder Lichtmatrixaufruf:

1. `src/game/game.cpp:443` erstellt SAT/EAT und ihre echten Records.
   `:465` erstellt `EmMgr.create(0,0)` **vor** `SmdSetup`.
2. `src/game/em.cpp`, `construct/getEmPtr`, bindet diesen Slot an `cPlLeon`.
   Der native PlayerRuntime führt bisher nur PlayerInit/setModel/setMotion
   und Ressourcenbindung aus; `actor_constructed` bleibt false.
3. `src/game/pl_leon.cpp:26` ruft init0, setModel, weaponRelease/Load/Init,
   init1, setMotion, EspDataLoad und startUp auf. `cPlayer::init0/init1`
   (`src/game/player.cpp:217/230`) benötigt Wep, Body, Push, Waist, MotBase,
   Neck, Hitboxen, Cloth und die angehängte Kamera-Motion-Struktur.
4. `cPlayer::startUp` (`:277`) führt move, zweimal motionMove, SAT.check und
   matUpdate aus. Seine Pose entsteht somit aus Routine-, Weapon-, SEQ-,
   Körper- und Kollisionszuständen. Eine beliebige vorhandene FCV wäre kein
   Nachweis dieser Startpose.
5. `cSatMgr::check/getFloor/hitCheck` (`src/game/atari.cpp`) liefert Boden-
   und Hinderniszustände. `CameraQuasiFPS::init/move/hitCheck`
   (`src/game/cam_qfps.cpp:1152/1189/705`) verwendet Spielerparameter,
   Kamerakollision und Frustumkorrekturen. Diese Bodies laufen noch nicht nativ.
6. `LightAreaDataLoad` für SAR, ETM/ETS und Cloth werden im Original vor
   LightMgr.update/CameraRoomInit registriert (`game.cpp:476` ff.). Die
   entsprechenden live Actor-/ETC-Parents fehlen der Diagnose.
7. `r100_StartEvent` (`src/st1/r100.cpp:550`) führt Event 9 aus und setzt
   **danach** Leon auf (-99685,-484,-1343) mit Yaw 2,246. Diese Konstanten
   wurden nicht vorgezogen und keine Event-Fertigstellung wurde künstlich gesetzt.

Der vorhandene Dolphin-Screenshot `G4BE08_2026-10-10_23-42-29.png` zeigt
Raum 100, Debugposition ungefähr (-99691,-454,-1344), HUD und r100s43-EVD.
Das ist ein passender Raumbeleg, aber weder derselbe EVD-Cut noch dieselbe
Kameramatrix/Pose wie die neue r100s40-Brückendiagnose. Kein direkter
Originaltreuevergleich wird daraus abgeleitet.

Der echte GameTask verbleibt bei `MerchantGameInit()` (`game.cpp:271`),
R120Event bei `Sofdec.Initialize` für opening.sfd. `gameStageInit`,
`gameRoomInit` und `gameMainLoop` sind weiterhin explizite native Fehlergrenzen.
Keine davon wurde überschritten oder als Erfolg ersetzt. Der nächste
zusammenhängende Entwicklungsauftrag muss die oben aufgeführte originale
Actor-/SAT-/Weapon-/SEQ- und Room-Registrierung gemeinsam integrieren,
danach QFPS und SAR an deren lebende Besitzer binden. Phase 5b wurde nicht begonnen.

## Tests, Belege und Wiederholung

Windows x86/x64: jeweils 27/27 CTest-Tests bestanden, einschließlich
Originalbody- und isolierter Parent-/Borrow-/FCC15-Grenztests. Die neuen
Parenttests benutzen synthetische BE-LIT-/Modellrecords und echte Original-
Funktionskörper; sie werden ausdrücklich nicht als Leon-/Raumzustand bezeichnet.
Originaldatentests verwenden getrennt die hashgeprüften echten Disc-Blobs.

| Suite | Windows x86/x64 | Retroid ARM32/ARM64 |
|---|---|---|
| CTest | jeweils 27/27 | Native CLI- und APK-Prüfungen statt Host-CTest |
| Core/Scheduler/Background | Bestandteil CTest | je zwei Läufe; 13.386 / 13.387 Checks, 0 Fehler |
| APK/JNI/Controller/Vulkan-Surface | nicht zutreffend | jeweils 20/20 Instrumentation-Tests |
| Ressourcen | je zwei Läufe beider Discs | je zwei Läufe beider Discs |
| Module | je zwei Läufe, 110 / 4 RELs inventarisiert; partielle native Init-Körper | derselbe Umfang, zwei Läufe je ABI |
| Skinning | je 1.739.820 Checks plus 617 Adapterchecks | gleiche Prüfzahlen, Quantisierungsdifferenz 0; ARM32/64 Pose-/Matrixdifferenz 0 |
| GameRuntime | zwei Läufe je Disc/ABI | zwei Läufe je Disc/ABI; echte Core-/Font-/Message-/SubScreen-/Checkpoint-/ID-/Player- und Lichttests |
| Originallichtdaten | 11.049 / 8.399 Checks für Disc 0 / 1 | dieselben Prüfzahlen für beide ABIs |

Der Core-Unterschied von einem Check ist ABI-spezifisch im Scheduler-
Pointertest, kein unterschiedlicher Gameplay-Zustandsübergang. Die
synthetischen Controllerregressionen bestehen; physische Tastendrücke wurden
in dieser Fortsetzung nicht nochmals aufgezeichnet. Vollständige REL-Gameplay-
Ausführung wird durch die Modul-Inventartests nicht behauptet.

Die unabhängige Skinning-Referenz misst maximal 0,01129043 für Translationen,
1,19e-7 für lineare Weight-Matrixanteile und 0,00480179 für FCV-Positionen
(Rotation 1,58e-6). Die daraus erzeugten quantisierten PSQ-Vertices und
Normalen stimmen exakt überein. Das sind dieselben gemessenen Referenzgrenzen
wie vor dieser Fortsetzung; keine neue numerische Gameplay-Abweichung.

Originale Scroll-Geometrie gegen Windows-x64-Referenz (je zwei ARM32-/ARM64-
Ausführungen, fünf Vergleichssamples): r100 maximale Positionsabweichung
0,015625 und Normalenabweichung 7,15e-7; r102 beide 0; r119 Position
0,00048828125, Normalen 2,38e-7. Indizes, Quellfarben, UVs und rohe Normalen
sind bytegleich. Das sind gemessene native SDK-Float-Unterschiede innerhalb
der geprüften Toleranz, kein PPC-Bitgleichheits- oder visueller Treuenachweis.

Der zusätzliche r120-Cut-1-Debugdraw verwendet pl0000 und dessen originale
`event/r120/s00/pl0000/pl0000_s00_001.fcv` bei Frame 30, einschließlich
der registrierten Anbauten. Gegen dieselbe Kamera ohne das Modell ändern
sich **17.247 GPU-Pixel** auf beiden ABIs, bei bytegleichen Wiederholungen.
Die vorhandenen 70 Leon-Drawgruppen werden weiter ausgegeben. Die fehlende
Objekt-Lichtregistrierung ist in dieser Texturdiagnose weiterhin explizit
ausgewiesen. Sie beweist weder einen konstruierten Gameplay-Leon noch Cloth-
Simulation, originale Beleuchtung oder eine r100-Spielerpose.

Das Auto fehlt in diesem Leon-Diagnosebild. Die originale Fahrzeugreferenz
ist in `EventCarInit` (`src/st1/r120.cpp:462/481`) als `obm1a00` registriert.
Der geprüfte r120-EVD-Snapshot für Cut 1, Frame 30 löst
`obj/objmodel/obm1a00.bin` mit 24 Gelenken und
`event/r120/s00/obm1a00/obm1a00_s00_001.fcv` auf. Das ist eine tatsächlich
CPU-seitig initialisierte Modellpose. `loadDiagnosticScene` wählt bei
`eventModel=pl0000` jedoch nur dieses Modell und seine Anbauten zum Draw aus;
das Fahrzeug und andere Eventmodelle werden nicht mitgezeichnet.
`Evt_R120S00_Func/EventCarInit`, Event-Parent-Transforms und TexRender-
Spiegelpässe werden nicht vollständig ausgeführt. Der Leon-Nachweis ist
deshalb ausdrücklich keine vollständige Autofahrt-/Introaufnahme.

Private GPU-Belege befinden sich in `phase5a-start-r100-{texture,material,vertex}-device`,
`phase5a-start-r102-texture-device`, `phase5a-start-r119-texture-device` und
`phase5a-start-r120-{lit,lighting,no-fog,fog}-device`. Je Zustand gibt es
zwei Captures je ABI, `gpu-<abi>-<n>.{json,rgba,png}` und echte Android-
`display-<abi>-<n>.png`. Alle vier Readbacks eines Zustands sind bytegleich.
UUIDs, tatsächliche Pointerbreite und gleichbleibende Prozess-PID prüfen die
Wiederholung mit Activity-/Surface-Neuerstellung statt Prozessneustart.
Weitere Modellbelege: `phase5a-start-r120-cut1-texture-device` und
`phase5a-start-r120-leon-texture-device`. Insgesamt sind 44 GPU-Captures
für elf Zustände/Ansichten mit zwei Wiederholungen je ABI vorhanden.
Scroll-Vergleiche: `phase5a-start-r{100,102,119}-scroll-device` mit
`geometry-comparison.json`. Der tatsächliche Gerätetyp ist Retroid Pocket
Flip2 (`dbdcacc`), Android 13/API 33, Adreno 650, Vulkan 1.1.128.

Khronos-Validation 1.3.290 ist tatsächlich verfügbar und aktiv. In diesen
Captures: **0 Fehler**, jeweils **1 Performancewarnung** für die absichtlich
nicht vorrotierte Swapchain (Identity gegenüber nativer Panelrotation).
Die Warnung wird nicht verschwiegen. Keine Descriptor-, Speicher- oder
Synchronisationsfehler wurden gemeldet.

Die abschließende native Negativprüfung auf beiden Retroid-ABIs bestätigt
außerdem, dass normale Modus-0-Draws von r100, r102 und r119 an fehlenden
Live-Parents abbrechen: Typ 1 bei r100/r119, Typ 3 bei r102. Exitcode 1 und
die konkrete `original light parent manager type`-Fehlermeldung sind erwartete
Grenznachweise, keine bestandene Gameplay-Ausgabe. Die abschließend erneut
aufgenommenen r100-Textur- und r120-Leon-Debugframes bleiben zwischen beiden
ABIs und den jeweiligen Wiederholungen bytegleich.

Reproduktion aus dem Projektverzeichnis, private Ausgaben außerhalb von Git:

```powershell
$taskPython = 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$taskAdb = 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe'
$taskData = 'E:\Development\Android\RE4-Data'
& $taskPython port/tools/test_gx_scene.py --data "$taskData\import-phase3a" --output "$taskData\phase5a-start-r100-texture-device" --adb $taskAdb --serial dbdcacc --room 100 --event Evd/r100s40.evd --cut 0 --frame 30 --debug-view 1 --repeat 2
& $taskPython port/tools/test_gx_scene.py --data "$taskData\import-phase3a" --output "$taskData\phase5a-start-r120-lit-device" --adb $taskAdb --serial dbdcacc --room 120 --event Evd/r120s00.evd --cut 0 --frame 30 --repeat 2
& .\port\scripts\test-device.ps1 -Adb $taskAdb -Serial dbdcacc -Repeat 2
& .\port\scripts\test-app-device.ps1 -Adb $taskAdb -Serial dbdcacc
```

Die entsprechenden Host-CLI-Tests verwenden `--host-exe
port/build/host-ninja/re4_gx_tool.exe` bzw. `host-x86`; `--smd-only`
prüft tatsächliche BlockCreate/BlockDestroy-Wiederholungen ohne Rendering.
`--geometry-reference` vergleicht native Vertex-/Indexbytes und numerische
Abweichungen gegen eine private andere-ABI-SMD-Ausgabe.

Fehlversuche: frühe Parentfixture mit zweitem Scheduler auf demselben Besitzer
wurde auf den bereits vorhandenen Scheduler korrigiert; UI-ANR und anschließender
-O0-GPU-Timeout lieferten keine gültigen Frames. Ein r102-Versuch wurde vor dem
FCC15-Key-Fix explizit abgewiesen. Ein zwischenzeitlicher USB-Abbruch wurde
nach Wiederverbindung erneut geprüft. Keine dieser Ausführungen zählt als bestanden.

## Vollständige geänderte Projektdateien

- Build: `port/CMakeLists.txt`.
- Android: `GeometryActivity.java`, `GraphicsBootstrapView.java` unter
  `port/android/app/src/main/java/org/re4/port`; `graphics_jni.cpp`,
  `vulkan_bootstrap.cpp` unter `port/platform/android`.
- Public native Schnittstellen: `consumers.h`, `gx_pipeline.h`, `gx_scene.h`,
  `lights.h` unter `port/include/re4/port`.
- Laufzeit: `consumers.cpp`, `gx_pipeline.cpp`, `gx_render_original.h`,
  `gx_scene.cpp`, `light_original.h`, `lights.cpp`, `motion.cpp`,
  `scroll_original.h`, `scroll_scene.cpp` unter `port/src`.
- Shader: `port/shaders/gx_diagnostic.vert`, `gx_diagnostic.frag`.
- Quellbindung: `port/config/light-sources.json`,
  `port/tools/generate_lights.py`, `generate_scroll.py`, `generate_consumers.py`.
- Tests: `port/tests/game_runtime_verification.cpp`, `gx_tool.cpp`,
  `light_parent_verification.h` (neu), `model_verification.h`,
  `port/tools/test_gx_scene.py`.
- Dokumentation: dieser Bericht, `port/README.md`,
  `port/docs/phase5a-r100-status.md` (Verweis auf den Folgestand).

Alle Dateien sind vollständig im Projekt gespeichert. Git-Master blieb auf
`9b76ff003081d6ece8b838453fb8310743a972c7`, origin/master auf
`15cf64f80e08bbc64712c39be330b75d1512a0d0`; dessen README-WIP-Aktualisierung
ist berücksichtigt. Keine Merges, Rebases, Pushes oder Assets in Git.
