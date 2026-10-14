# M37: erster Raumausgang, Felsenakteure und Original-Fokusfilter

## r100 bis zum ursprünglichen Raumausgang

`r100` ist der Waldweg mit dem ersten Haus. Die nächste Karte ist `r101`, das Dorf; der komplette Dorfkampf gehört zu dessen eigenem Raumskript. In der tatsächlichen Disc1-AEV von r100 existiert genau **ein** Raumübergang: Bereich0 nach r101. Es gibt keinen direkten r100→r102-Übergang.

M36 erkannte diesen Bereich, ließ aber nur den früheren r300↔r301-Testübergang zu. M37 gibt den einzeln geprüften, unveränderten r100→r101-Ausgang frei. Die ursprünglichen Daten bleiben erhalten:

| Eigenschaft | Originalwert |
| --- | --- |
| Bereich / Ziel / Part | 0 / r101 / 0 |
| Aktivierung | Trigger2: A gedrückt **oder gehalten** |
| Zielposition | −52144.40234375, 90.57080078125, 22524.2734375 |
| Zielrichtung | 2.1220216751098633 Radiant |
| Schlüssel / Callback / Modellparent | jeweils0 |
| Fade | 0 |

`r100.cpp` deaktiviert oder ersetzt diesen AEV-Ausgang nicht und verlangt dort keine Zahl getöteter Gegner. Die RSF-Abfragen von `r100_DoorCk` betreffen die Tür des ersten Hauses im ETS, nicht den Raumwechsel am Ende des Waldwegs. M37 erfindet deshalb keine zusätzliche Freigabe über RSF10/SCF17 oder das Töten aller sieben Gegner.

Der gegenteilige r101→r100-Übergang kann im ursprünglichen Dorfkampf durch `r101_DoorDontOpen100` ersetzt werden. M37 gibt ihn und die übrigen r101-Ausgänge nicht pauschal frei. Das vollständige Dorfskript mit seinen Wellen, Kettensägengegnern und Glockenereignissen wird durch diese Korrektur nicht implementiert.

`tools/test_m37_campaign_path.cpp` prüft die echten r100/r101-Archive mit **23 Prüfungen**. Die Mitte des AEV-Ausgangspolygons liegt jenseits der geschlossenen Schranke und besitzt selbst keinen SAT-Boden. Der Test findet deshalb eine echte freie SAT-Standposition, deren ursprünglicher, an SAT verkürzter 550-Einheiten-Frontpunkt den Bereich trifft. Er prüft außerdem die genaue Zielposition, gedrücktes/gehaltenes/losgelassenes A, das Nichtauslösen vom Hauskampf aus, die normale Körperplatzierung und die ursprüngliche 400-Einheiten-Szenenkugel am Ziel. Die vorhandenen `room_flow_tests.cpp` sichern ab, dass andere Zielräume, die falsche Triggerart und der Rückweg gesperrt bleiben. Der unabhängige Datenprüfer zählt nun drei ausdrücklich geprüfte direkte Übergänge.

## Die drei ursprünglichen Felsenakteure

`r100_trap_set` legt beim ersten Durchgang (`game_cnt==0`) die ESL-Indizes6/7/8 an und bindet deren Raumbewegungen über `setEvtMotion(...,0,0,0)` ein. Diese Figuren sind keine zusätzlichen kämpfenden Gegner: `em10_R1_R100Cliff` setzt bereits bei der Initialisierung HP0 und deaktiviert die Boden-/Szenenkollision. M37 übernimmt diesen eigenen Ablauf statt drei zusätzliche gewöhnliche Ganados zu erzeugen.

| ESL-Index | EM12-Typ / Set | Flags | Raum-ARC / Archiveintrag | FCV-Maximum |
| --- | --- | --- | --- | --- |
| 6 | 1 / 2 | 0x00400030 | 0x2e / 42 | 190 |
| 7 | 3 / 3 | 0x00000030 | 0x2f / 43 | 255 |
| 8 | 0 / 4 | 0x00800030 | 0x30 / 44 | 180 |

Bis zur Freigabe des ursprünglichen Raumskripts bleiben die vorbereiteten Figuren unsichtbar und pausiert. Danach setzt die Wartephase die jeweilige Raum-FCV in jedem Tick mit Flags5, Frame0 und ohne Blend/SEQ neu auf. Erst ein ursprünglicher horizontaler Abstand **kleiner als28000** beginnt den Fall. Nur Set2 spielt dabei den Raum-Soundblock6, Cue4. Die Originalabfrage zum Mitnehmen weiterer Figuren berücksichtigt HP>0; die bereits mit HP0 initialisierten Felsenakteure werden deshalb nicht künstlich alle gleichzeitig aktiviert.

Die Fallphase folgt der wirklichen 3D-Root-Bewegung ohne SAT-Bodenstopp. Ihr Timer beträgt FCV-Maximum−10; anschließend sinkt der originale Float-Sichtbarkeitsfaktor in jedem30-Hz-Tick um0,1. Bei0 wird die Figur inaktiv. Das Anfangs-HP0 erzeugt weder Kampfbeute noch einen vorzeitigen gespeicherten Tod: `cliffFalling()` unterscheidet die anfängliche Wartephase vom ursprünglichen `EmSetDie` der Fallphase.

`EnemyPreview::installRoomEventMotion` ergänzt eine separate Aktion hinter den bestehenden30 EM12-Aktionen und verändert keine gewöhnlichen Kampfbewegungen. Der GLES-Aktor verwendet den gleichen Sichtbarkeitsfaktor in seiner bestehenden Coverage-Ausblendung. Diese Darstellung ist keine bitgenaue Emulation der ursprünglichen GX-Alpha-Rasterung.

Nachweise mit den echten lokalen Disc1-Daten:

- `tools/test_m37_r100_cliff.cpp`: **708 Prüfungen**, einschließlich aller drei Profile, ursprünglicher Startpositionen, HP0-Wartephase, strenger Abstandsschwelle, Timer/Fade, Soundzuordnung sowie Schussfehler und Konkurrenz externer Ziele.
- `tools/test_m37_r100_cliff_source.py`: die getrennt kompilierten ursprünglichen `MotionSetCore`/`MotionMove` prüfen **673 Ticks und277.276 Pose-/Root-Floats** für die drei Raum-FCVs; maximale Root-Abweichung0.
- `tools/test_m37_r100_cliff_gl.cpp`: **50 echte GLES3-Prüfungen** für alle drei Modelle, originale Raumbewegungen, Sichtbarkeitsfaktoren1/0,5/0, pausierte Posen, unabhängige GPU-Besitzer und EGL-Neuanlage.

## Schüsse ohne Treffer und gemeinsame Zielauswahl

Die Gruppen-Schussabfrage gab bei einem Fehlschuss `float::max()` als Begrenzung an den Objektlaufzeitcode weiter. Dessen legitime Bereichsprüfung wies diesen Wert zurück. M37 verwendet hier die endliche quadrierte Länge des tatsächlichen Schussstrahls. Die Regression führt den Schuss durch die wirkliche Objektlaufzeit von r100 aus.

Die gemeinsame Auswahl berücksichtigt außerdem die nächsten externen Krähen-/Hund-/Fallenziele über `EnemyGroup::setExternalTarget`. Gewöhnliche Handgun- und einzelne Shotgun-Strahlen reservieren genau einen ursprünglichen Zielplatz; die unterstützte Punisher reserviert zwei. Ein näherer externer Treffer verbraucht denselben Zielplatz wie ein Ganado oder eine Kiste. Weitere Shotgun-Pellets werden weiterhin einzeln abgefragt und überspringen entsprechend dem ursprünglichen Flag4 die Kisten/Fässer. Durchlässiges Fensterglas behält seine separate ursprüngliche Ausnahme, geschlossene Türblätter verkürzen den Strahl.

```sh
m37_r100_cliff_check r100.dat emleon00.esl em12.drs pl00.drs wep01.drs
python3 tools/test_m37_r100_cliff_source.py m37_r100_cliff_tool em12.drs emleon00.esl r100.dat
m37_r100_cliff_gl_check em12.drs emleon00.esl r100.dat
```

## Fokus aus dem ursprünglichen Ereigniscode

Quelle: `upstream/re4/src/game/event.cpp::FocusMove`, `filter01.cpp`, `trans_ot.cpp::AddOtDirect`/`ExecOt`, `main_sub.cpp::Render_init` und `lib/tealeaf.c::__cvt_fp2unsigned`.

Die Ereignisse fordern `Filter01` **Typ1** an. `event_focus_gl.cpp` setzt dessen tatsächlichen Ablauf um:

1. Kameratiefe mit ursprünglichem Near100/Far1.000.000 in den ganzzahligen Bildschirm-Z umrechnen. Bei Kameratiefe0 verwendet das Original0,01. Die negative Ausgabe wird über die ursprünglichen vorzeichenbehafteten unteren32 Bits übernommen; dafür wird keine undefinierte C++-Float→Unsigned-Konvertierung benutzt.
2. Das Bild halbieren, mit dem ursprünglichen Copy-Filter `{32,0,0,0,0,0,32}`. Diese sieben Gewichte beschreiben drei EFB-Zeilen, hier32/0/32. Das GLES-Ziel verwendet die halbe aktuelle Ausgabegröße.
3. Vier diagonal versetzte Kopien mit `level × 0,33` und Alpha128 zeichnen. Vor jedem Durchlauf wird Z um100 erhöht und auf65536 begrenzt. Near verwendet `GEQUAL`, Far `LEQUAL`; die Szene schreibt dabei keinen neuen Tiefenwert.
4. Bei Level>1 das bereits bearbeitete Bild erneut kopieren und mit Alpha `min(255, (level−1)×25)` an der letzten Verschiebung plus0,25 zeichnen.
5. Die ursprüngliche OT-Liste fügt neue Einträge vorne ein: Far läuft deshalb **vor** Near, obwohl `Filter01Trans` Near zuerst anmeldet.

Die Verschiebungseinheiten stammen aus dem eigentlichen Spiel-EFB **512×448**, nicht aus der Auflösung der Titelgrafiken. Sie werden auf die aktuelle Vollbildausgabe skaliert. Der Filter wird vor Ereignisuntertiteln, Ereignisblende und globaler Helligkeit ausgeführt. EFB-Alpha und vorhandene Szenentiefe bleiben unverändert; veränderte GLES-Zustände werden zurückgesetzt. Nach EGL-Verlust werden die Objekte neu erzeugt.

Das Original berechnet seinen Fokus-Z für einen Far-Wert von1.000.000, während der bisherige GLES-Szenenadapter gewöhnlich200.000 verwendet. M37 transformiert die wirkliche ortho-Tiefenschwelle (`Z/65536`) über die Kameradistanz in die aktuell verwendete Szenenprojektion. Es setzt keinen falschen Original-Z-Wert ungeprüft in den abweichenden GLES-Tiefenpuffer ein.

## Nachweise und Grenzen

- `tools/test_m37_event_focus_source.py` kompiliert den eingefrorenen **ursprünglichen** Filtercode in einen instrumentierten GX-Adapter. Nur die Plattformaufrufe werden aufgezeichnet; die Originalausdrücke für die Filterdurchläufe bleiben unverändert. Für den negativen Float→Unsigned-Fall ersetzt der Adapter den Host-Cast durch das geprüfte ursprüngliche PPC-Laufzeitverhalten. **5.256 bitgenaue Koordinaten-, Tiefen-, Alpha- und Kopierprüfungen** über216 Near/Far-Anforderungen bestehen.
- `tools/test_m37_event_focus_gl.cpp` besteht **7.193 Prüfungen** mit echtem GLES3: Near/Far-Tiefenauswahl, unveränderte Fokusmitte und Alpha, Far-vor-Near einschließlich überlappender Bereiche, Clip-Verhalten bei Kameratiefe0, beliebige vorherige GL-Zustände, verschiedene Viewports und EGL-Neuanlage.
- Der Quellenvergleich beweist die genannten Durchlaufparameter; er ist kein vollständiger GX-Bildvergleich. GLES verwendet die aktuelle Ausgabeauflösung, seine lineare Textureinheit und seine RGBA8-Blende. GX-EFB-Samplemuster, Subpixel-Rasterung, VI-Feldjitter und die konkrete GameCube-Farb-/Blendquantisierung werden damit nicht bitgenau emuliert. Der Filter ersetzt außerdem keine bisher fehlenden Schatten oder sonstigen GX/TEV-Funktionen. Die bestehenden Szenenprojektionsgrenzen bleiben bestehen.

Beispielprüfungen ohne im Projekt enthaltene Spieldaten:

```sh
m37_campaign_path_check r100.dat r101.dat
m37_event_focus_gl_check
python3 tools/test_m37_event_focus_source.py g++ app/src/main/cpp upstream/re4 GLES_INCLUDE GLES_LIBRARY
```

## r120: der ursprüngliche Projektionsspiegel

`EvtTexRenderCamTrans` gilt in r120s00/Cut0 und r120s01/Cut2/6. Es zeigt den unsichtbar verwalteten Leon-Klon `pl0010` ausschließlich in TexRender0; `obm3010f` verwendet dieses Bild in der Hauptszene. Die Kamera bleibt die aktuelle Ereigniskamera mit ursprünglichem4:3-Verhältnis. Es gibt keine erfundene zweite Spiegelkamera oder geometrische Reflexion.

`event_mirror_gl.cpp` übernimmt das Ziel592×512, den mittigen512×512-Ausschnitt abX40, dessen2×Auflösung auf256×256, ausgeschalteten GX-Copy-VFilter und Repeat-Wrapping. Das ursprüngliche Bild beginnt mit schwarzer transparenter Farbe; normale Raumgeometrie gehört nicht in diesen RTT0-Durchlauf. Der Projektionsspiegel verwendet Vorderseiten-Culling, ALWAYS-Z mit Tiefenschreiben, RGB×2 und den ursprünglichen Sichtbarkeitsfaktor0,4. Die8-Bit-Alphaumwandlung ergibt102/255 bei Modellalpha255 und76/255 beim originalen Cut6-Alpha192.

Die unabhängige Originalprüfung `test_m37_event_mirror_source.py` kompiliert die eingefrorenen RenderTex-/CopyTex-Funktionen und das SDK-Projektionsmatrixverfahren: **342 Prüfungen** bestehen. `test_m37_event_mirror_gl.cpp` prüft mit den tatsächlichen r120s00/s01-Daten **68 GLES-Fälle**, darunter alle aktiven Cuts, pausierte Pose/Zeit, Original-LightMask0x20 des Klons, deterministische Bildkopie, umfassende GL-Zustandswiederherstellung und EGL-Neuanlage.

Dies beweist Ablauf, Koordinaten und Parameter; die GLES-RGBA8-Texturfilterung und Rasterung emulieren nicht alle GameCube-EFB-Samples und GX-Farbquantisierungen bitgenau. Der Speicherbesitzer löscht beziehungsweise verwirft seine GPU-Objekte beim normalen Wechsel beziehungsweise EGL-Verlust.

## r100: ursprünglicher Schreck am Haus

Der zusätzliche ESL-Eintrag0x25 hat EM12-Typ0, Set0x1b, Flags0x24 und HP1000. Das ursprüngliche `em10InitRtnSet` wählt hierfür `R100TurnWalk` statt einer gewöhnlichen Wait-Routine. `beginR100HouseScare` führt dessen ursprüngliche Initialisierung aus: zufällig gesetzte Wait-Motion, Timer90 einschließlich des bereits im Konstruktionstick ausgeführten ersten Schritts, dann Raum-ARC18/19 mit Blend10 und Flags1, danach die ursprüngliche Walk-Motion mit Blend7. Der Walk-Typ entscheidet zwischen ARC8 und ARCe. Nur dieser gesonderte, vom Ereignis verwaltete Akteur folgt dem Ablauf; er wird nach dem Kameracut zerstört und erzeugt weder normalen Kampf noch gespeicherten Tod oder Beute.

`test_m37_r100_house_scare.cpp` besteht **104 Prüfungen** mit originaler ESL und EM12, einschließlich Profil, Timer, Turn-Übergang, beiden Walk-Familien und Suspension. Die getrennt kompilierten ursprünglichen MotionSetCore/MotionMove prüfen **310 Ticks und127.720 Pose-/Root-Floats** mit maximaler Root-Abweichung0. Der Port verwendet seinen bestehenden Akteur-Zufallsbesitzer; die komplette ursprüngliche globale EmMgr-Zufallsfolge, Hals-/Bodenneigung und alle ursprünglichen Atarimanager-Interaktionen werden dadurch nicht vollständig rekonstruiert. Die Kamerafolge, Leon-Bewegung und Ereignisfreigabe haben einen getrennten Ereignisbesitzer.

## Ursprüngliche Speicherdatei und begrenzte native Zustandsabbildung

`save_game.cpp` codiert den wirklichen `bh4_data00`..`bh4_data19`-Inhalt von `cCard::makeSaveData`. Die sinnvollen0xeaFC Bytes liegen in acht0x2000-Byte-Kartenblöcken; die nativen Dateien enthalten deshalb0x10000 Bytes. Der Header benutzt Version0x116, die ursprünglichen zehn32-Bit-Kalenderwerte, Modus1/2/3 und die tatsächlichen Spielmetadaten. Beide ursprünglichen MSB-CRC32 verwenden Polynom0x04c11db7, Start0 und keinen abschließenden XOR. Die ursprüngliche16-Bit-Speicherzahl wird beim Erstellen einer privaten Dateikopie erhöht; fehlerhafte Daten verändern den laufenden Zustand nicht. Die20 Android-Dateien ersetzen die CARD-SDK-Geräteverwaltung, nicht das Originaldateiformat.

| Originalabschnitt | Dateioffset | Länge |
| --- | --- | --- |
| GLOBAL_WK0x4f80..0x8678 | 0x2034 | 0x36f8 |
| ItemMgr | 0x572c | 0x1204 |
| RoomData | 0x6930 | 0x6628 im aktuellen Originalbuild |
| Subscreen-Speicherwort | 0xe7d0 | 4 |
| Merchant | 0xe7d4 | 0x304 |
| Vollständiger Daten-CRC | 0xeaf8 | 4 |

RoomData besitzt121 Records in der ursprünglichen Stage-/Raumreihenfolge, mit jeweils0xd8 Bytes. r120 hat im Quellcode keinen solchen Record. `save_native.cpp` schreibt Position/Richtung, Leben, Metadaten, Szenario-/Schlüssel-/globale Fundwörter, die vorhandenen Raum-/Objektwörter, sämtliche zwölf Gegner-Todestabellen und die aktuelle unveränderte ESL explizit in Big Endian. Kofferslots behalten Lücken, Ausrüstung, Munition, Rotation und ursprüngliche Halbzellen-Mittelpunkte. Keine GC-Zeiger oder Host-Struct-Padding werden kopiert. `InventoryCase::restoreSnapshot` validiert die Rückgabe vollständig und übernimmt sie erst danach.

Ein geladener Rohdatenbesitzer bleibt als Basis erhalten: unbekannte Global-Felder, andere Inventarsätze, Waffentunings/Anbauteile, Raum-BGM, Subscreen und Händlerdaten werden bei weiteren Speicherungen nicht pauschal gelöscht. Frische Daten erhalten die tatsächlich bekannten Originalvorgaben für RoomData, leere ItemMgr-Slots, MerchantGameInit und SubScreenGameInit. Die übrigen noch nicht durch den nativen ersten-Raum-Lauf verwalteten Felder bleiben im neuen Speicherbild0; dies ist keine vollständige Rekonstruktion jedes Originalmanagers oder Unterstützung beliebiger späterer GameCube-Spielstände. Die native Ladeintegration muss ihren spielbaren Raum-/Waffenbereich ausdrücklich prüfen. Modus3 ist im Dateicodec darstellbar; der gesamte Original-New-Round-Reset samt Merchant-Boni wird dadurch allein nicht spielbar.

Persistente freie Fundsätze folgen `sceAtCheckSaveItem`: ausschließlich Typ5 oder7. Die irreführende Kommentarbenennung wird nicht als Logik übernommen: Pesetas78/79/71 sind im tatsächlichen `itemInfo` Typ8, Schatz57 ist Typ12, also erzeugen diese keinen ursprünglichen dauerhaften ITEM_SAVE_WORK. Gespeicherte Positionen werden nach dem Original als signed16(position/10) quantisiert; nur die passende Wiederherstellung addiert den ursprünglichen sichtbaren Pickup-Höhenversatz.

`LoadOriginalSaveCardArt` liest den wirklichen MRAM-Teil von `etc/memcard.das`, den ersten TPL und exakt den regionalen96×32-RGB5A3-Banner, den32×32-CI8-Icon und dessen256-Einträge-RGB5A3-TLUT. Es schreibt keine synthetische Grafik. Die separate Android-Auswahlliste verwendet die ursprünglichen save_e.dat-Elemente.

Nachweise: **65 native Prüfungen** mit realer ESL/DAS sichern Metadaten, Raw-Basis-Erhalt, Flag-Bitreihenfolge, Kofferslots, Korruptions-/Bereichsprüfungen, alle20 Dateinamen, alle drei SaveKind-Werte, Zählerüberlauf und originale Fundquantisierung. `test_m37_save_game_source.py` kompiliert die eingefrorenen makeSaveData/CRC-Funktionen; nur hostseitige Scalar-Stores werden vor dem ursprünglichen CRC in die GameCube-Bytefolge umgesetzt. Die unabhängige Originaldatei stimmt über **65.659 Byte-/Quellenprüfungen** mit der erzeugten Fixture überein, einschließlich der originalen121 Raum-IDs.

```sh
m37_save_game_check DISC_FILES /tmp/m37-original-save-fixture.dat
python3 tools/test_m37_save_game_source.py g++ upstream/re4 /tmp/m37-original-save-fixture.dat
```
# Original r100 cloth sheets (Esp4e)

All three immutable SST1 records now own the original 9×10 spring grids. The
backend executes the source120-step settling, four pinned rows, neighbour
force order, gravity/damping, inverse-space velocity and shared byte-RNG sine
wind. `SINF` preserves the original paired-single polynomial and fused lane
operations. Texture animation never advances for these sheets. World vertices
use the original placement×60.857143 scale; normals use its inverse transpose.
The world-angle conversion uses the original DEG2RAD .017453292 constant.

The isolated GLES pass loads original TPL0, clamp/no-mip linear sampling,
scenery-light mask0x10 and eight-light limit, two-sided row geometry, source
TEV color/alpha scale4, GEQUAL4 alpha and LEQUAL depth. Standard blending writes
depth; additive blending does not. Creation, draw, deletion and context-loss
recovery are wired into the room owner; rendering never advances CPU state.

Validation:184 actual-asset CPU checks;526,972 frozen `Cloth`/`cEsp4e`/`Rnd`
state, placement, inverse-normal and expanded-strip checks across all three
actual records and40 frames;34 actual-asset EGL rendering/state/recreation
checks. The source oracle translates the frozen SINF inline assembly lane by
lane, with SDK vector/matrix primitives supplied by the existing host adapter.
It verifies source arithmetic and draw contracts; it does not assert identical
PPC reciprocal-estimate bits or GX raster/interpolation quantization.

## Original save time and results counters

`main_sub.cpp` measures play time from absolute `OSTicksToSeconds(OSGetTime())`
segments, including EVD, case/item messages and room loading. `gameStageInit`
folds the segment before `StageSet` but does not reset it after loading. Only
the explicit option screen folds/stops and initializes a new baseline at its
exit. The Android lifecycle also excludes application background time.
`save_clock.h` preserves the original unsigned-second subtraction/wrap;
simulation frame count and rendering refresh never determine saved time.

Accuracy follows the returned candidate count of `GetWepTargetList2`, including
its source enemy-manager targets, rather than later HP reduction. The native
group compares Ganado/object/external/interior hits with original EAT clipping.
Only the primary shotgun ray contributes one accuracy/shot result; flag4
secondary rays and the original non-result weapon modes are excluded.
`save_stats.h` retains the complete original counter-tail exclusion switch.
Kill totals consume `EnemyEncounter::sourceKillCount()` once after completed
normal Die_Cramp, not at initial HP0 or Timer49. Scripted cliff and transient
house actors do not award kills. The header and BE global payload share the
same folded source time and current chapter/game result counters.

Validation:13,840 frozen `InitGameTime`/`GetGameTime`/`SetGameTime` and
`PlWepHitCheck2` counter-tail checks;8 clock pause/wrap boundary checks;713
actual-asset cliff/occlusion/primary/secondary accuracy checks. The isolated
`m37_save_stats_runtime_check` covers the production JNI/card path and
foreground/background clock gate with the existing original assets; its
integrated execution is handled by the shared-build owner.

## Permanent room effects during original event/script frames

`SstSet` (`est.cpp`) starts each SST generator with flags0x4001. The permanent
core flag makes `ESP_IsActive` retain these sprites under `STA_SUSPEND`; the
original `game.cpp` continues `EffAreaUpdate` and `EspMove` during EVD and
ordinary scripted room procedures. The native bridge now runs their CPU tick
exactly once in each executed30Hz EVD or room-script frame, including the radio
introduction, message terminal and final close frame. Ordinary player frames
retain their existing single tick. Android pause/background guards remain in
the outer simulation caller, so suspended rendering alone cannot mutate the
source effects.

EVD entry sets `STA_EFFAREA_USE_CAM`: the active original camera eye is therefore
the effect-area point without a player-height offset. Its original FOV/roll
camera basis is supplied to camera-dependent source effects. Ordinary SceCam
and radio scripts retain the original player-feet+100 area point while using
their displayed camera basis. The SceneGL pass now draws these permanent room
quads during events, alongside the already visible original cloth sheets.

The production runtime regression covers one-tick ownership, active-camera area
masks, paused EVD/effect state and deterministic rendering, plus all three
radio phases. It does not add the permanent exception to unrelated non-SST
particles whose source suspension policy differs.
