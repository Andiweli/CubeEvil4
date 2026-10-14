# Originalquellen für M25 und den nächsten Kampagnenschritt

Referenz: unverändertes `upstream/re4`, Commit `9b76ff003081d6ece8b838453fb8310743a972c7`, und die bereitgestellten USA-Discs G4BE08 Revision0. Die folgenden Notizen enthalten nur Struktur-/Quellenbelege; keine Originalspieldateien.

## M25-Objekt- und Itempfad

- `game/embox.cpp` / `include/embox.h`: cEmBox-Typen, 1000HP, rotierte YARE-Volumen, damage-Eingang und ursprüngliche etc-Zustandsflags. Implementiert ist der begrenzte Behälterpfad aus `object_actions.*`.
- `game/sce_at.cpp`: ITA-Bindung, `sceAtSetItem`, ursprüngliche +50Y-Platzierung, Standardradius1500, Bereichspriorität, drei EAT-Sichtprüfungen und Aufnahmestatus.
- `game/item_model.cpp`: ITM-ID/BIN-/TPL-Verzeichnisse, relative Tabellenoffsets. `room_items.*` liest die echten Modelle aus dem aktiven Raumarchiv.
- `game/sce_at.cpp`: Leon-Systemitems0x1003/0x1004/0x1005 werden als Gewehr15/Gewehr10/Pistole20 aufgelöst. `game/item.cpp`, `cItemMgr::use`, case0x06, ruft healing600. Vollständige Kofferanordnung und Kombination sind noch nicht umgesetzt.
- `game/cam_qfps.cpp`: `CameraQuasiFPS` und ursprüngliche Schulterpunkte, SEE0x8000, Ausschlussmaske0x1C2810 und drei Nahflächenlinien. Die 50°-Komposition für alle Aktionen bleibt ein dokumentierter Plattformadapter.

## r400-Objekte und Grenzen

`st4/r400.cpp` initialisiert das Mercenaries-Dorf; `st1/r101.cpp` das Kampagnendorf. r400-AEV enthält14 Records und keine kind1-Raumübergänge. r400-ETS enthält33 Platzierungen, darunter sechs Türen: ET03 slots1/2/3, ET0D slot11, ET2A slots37/38. Keine `ETC_BARRED*`-/`ETC_DEKA_ITA`-Platzierung ist vorhanden. Die gemessenen Türmodelle haben normale Haus-Türmaße und überspannen keinen Kartenrand.

M24 ergänzt diese Objekte ausschließlich als originale sichtbare Geometrie; es fügt keine SAT-Laufkollision hinzu. `R400Init` initialisiert Fenster/Leitern und Mercenaries-Schatzereignisse, öffnet aber nicht sämtliche Türen. `SetDoor` beginnt ohne vorhandene Zerstörungsflags mit geschlossenen Türen. Eine einzelne gemeldete äußere Planke ist ohne Standort/Bild nicht eindeutig zugeordnet.

Für einen späteren Türport: `emDoorActEvtCk` prüft Ausrichtung innerhalb45° sowie Abstand zur Türfläche±800, zur Scharnierbreite±650 und Höhe±500. Öffnung verwendet Spieler-ARC0x1e/Tür-ARC0x1f; Tritt Spieler-ARC0x1c mit setOpen bei Frame14. `emDoor_R1_Open`/`Close` regeln den ±90°-Schwenk und ursprünglichen150-Frame-Schließtimer. Dafür sind eigene Animation, dynamische Kollision und persistente Originalflags nötig.

## Feuerprüfung: r400 gegenüber r101

Stand: 2026-10-04. Prüfung am eingefrorenen Originalquellstand
`9b76ff003081d6ece8b838453fb8310743a972c7` und an den bereitgestellten USA-Disc-Daten.
Keine zusätzlichen Produktionsänderungen an Welt-Effekten in M25.

## Ergebnis für die aktuelle Testmap r400

`upstream/re4/src/st4/r400.cpp` bezeichnet und initialisiert r400 als
Mercenaries-Dorf. Diese Variante teilt Dorfgeometrie mit dem Kampagnenraum,
übernimmt aber nicht dessen Brennobjekt-/Feuerinitialisierung.

Die r400-EFF hat **0 EST-Einträge** und **12 SST-Einträge**. Insbesondere enthält
sie die Kampagnen-Feuersequenz EST0 überhaupt nicht. `R400Init()` erzeugt weder
das brennende Obj00 der Dorfmitte noch dessen EST0-Feuer. Auch die 33 originalen
ETS-Platzierungen in r400 enthalten keine Etc-Fackelklasse.

| Quelle | Was sie tatsächlich enthält | M24/M25-Stand |
| --- | --- | --- |
| SST3, global `type=0xffff` | Zwei Boden-/Glutflächen mit jeweils Esp08/0x72, Esp00/0xe3 und Esp00/0xf7 | Sechs originale Ebenen gerendert; kein lodernder Scheiterhaufen |
| SST13, global `type=0xffff` | Sieben Esp00/0xe9-Lichtstrahlen | Gerendert; kein Rauch |
| SST5, Area `type=0` | Drei maskierte Ofen-/Glutebenen, drei Innenraum-Lichtstrahlen, ein Esp0a-Lichtpfad und eine weitere Lichtfläche | Nicht global aktiviert; setzt EAR-Lebenszyklus und Maskenrenderer voraus |
| Brunnen | Vorhandene Wasser-/Materialanimation | Kann sichtbar animiert sein; beweist kein Feuer |

SST3-Positionen in Originaleinheiten:
`(-7972.29, 250, -318.29)` und `(-4672.90, 182.14, 9722.33)`.
SST5-Ofenposition ungefähr `(-6423.61, 532, -8184.97)`; die drei Ebenen verwenden
`Tool_flg 0x4001` und Maske `0xff`. Die originale 0xff-TPL/ANM in Core ist ein
statisches 48×48-Bild. Ein einfacher unmaskierter Plane würde falsch aussehen.

Originalbereichslogik:
`game/esp_app.cpp::EffAreaUpdate` prüft `pPL->pos + (0,100,0)` gegen EAR-Areas.
`game/eff_sys.cpp::EffSetAreaState` startet beim Eintritt passende SSTs und
löscht beim Austritt Sprites/Generatoren/Modelle derselben Core_kind.
`game/est.cpp::SstSet` unterscheidet globale `0xffff` von Area 0..31; diese
Trennung darf nicht durch dauerhaft aktivierte Innenraumebenen ersetzt werden.

Auch ein r400-FXX-Block als alternative Pyre-Quelle existiert in diesen Daten
nicht. Es werden keine künstlichen Feuertexturen oder geratenen Platzierungen
vorgeschlagen.

## Gefundene echte Flammen im Kampagnenraum r101

`upstream/re4/src/st1/r101.cpp::R101Init`, Originalzweig `eff_country != 0`:

1. Brennobjekt über `SetObj00` mit Room-ARC-Indizes 0x20/0x21.
2. Originalposition `(-2880, 50, 2810)`, Y-Rotation `-π/2`.
3. `setNoSuspend(1)`.
4. Expliziter Aufruf von `EstSet` mit **EFF_ROOM, EST0, Core_flg 0x801**.
5. Feuergeräusch `SndCall(6,0x58)` an der Position mit +1500 Y.

Das ist eine skriptgesteuerte EST-Sequenz und kein globales SST. Es ist nicht
korrekt, diese Initialisierung automatisch in r400 zu kopieren.
Im vorliegenden Archiv entspricht Room-ARC 0x20/0x21 wegen der vier Headerwörter
den eigentlichen Archiveinträgen **28 BIN / 29 TPL**. Tür-, Item-, Gegner- und
Kampagnenflags von r101 benötigen eigene Initialisierung, bevor der Raum als
vollständiger erster Kampagnenlevel gelten kann.

r101-EFF EST0 enthält genau **13 Records**, alle `Set_time=0`:

| Records | Klasse | Bedeutung |
| --- | --- | --- |
| 0 | Esp46 | Originaler Vollbild-Farbfilter; ist kein zeichnbarer Sprite |
| 1..8 | Esp0b / Esp00 | Acht permanente originale Flammenschichten |
| 9..11 | Espgen00 → Esp0b / Esp00 | Drei wiederholende Flammenemitter |
| 12 | Espgen00 → Esp0b, Textur 0x52 | Aufsteigender Rauch; `Life_max=160` |

EST0-Flammentexturen und Originalanimationen:

| Textur-ID | Ressource | Größe | Pivot X/Y | Frames | Loop |
| --- | --- | --- | --- | --- | --- |
| 0x29 | r101-EFF | 96×96 | 43 / 95 | 30 | 1 |
| 0x26 | Core-EFF | 64×48 | 28 / 40 | 30 | 1 |
| 0x28 | Core-EFF | 64×96 | 31 / 85 | 30 | 1 |
| 0x2e | Core-EFF | 96×96 | 0 / 0 | 30 | 0 |
| 0x52 | Core-EFF | 128×128 | 0 / 0 | 1 | 1 |

Die acht permanenten Flammenschichten liegen bereits mit absoluten
Originalkoordinaten im Bereich X=-4264..-3066, Y=264..980, Z=2036..3349.
Die Rauchquelle ist `(-3198.2,1800,2680.7)`.

## Präziser Folgeschritt für M26

- r101 über den bereits vorhandenen geprüften Raumkatalog aktivieren; keine
  Spieldaten ins Quellcode-ZIP aufnehmen. Wiederverwendet wird `St1/r101.das`
  aus dem bestehenden Disc-Import.
- Original-Rauminitialisierung und `eff_country`-Zweig explizit wählen und
  dokumentieren. Keine neue Variable aus einer geratenen Disc-Region ableiten.
- Brennobjekt aus BIN28/TPL29 mit Originaltransformation aufbauen.
- Separaten EST0-Start mit Core_flg 0x801 portieren; kein SST-200-Frames-Prerun
  auf einen EstSet-Aufruf übertragen, den das Original nicht vorwärmt.
- Esp00, Esp0b, Espgen00 und Espgen10 zuerst vollständig für diese Sequenz
  übernehmen. AnmMove verwendet 1/32-Frame-Zähler und strikten `>`-Vergleich.
- Esp0b ist kameraabhängig: `game/esp0b.cpp` entfernt das vorige Offset, führt
  CommonMove/AnmMove aus und setzt ein neues Kamera-Plane-Offset aus Vec0.
  Bei EST0 sind Vec0.X/Y jeweils 0; Vec0.Z verschiebt manche Ebenen um
  60/100/220 und Rauch um 800 Einheiten zur Kamera. Dies darf nicht als
  frei erfundene Kamera-Billboard-Schicht implementiert werden.
- `Tool_flg 0x80000` verlangt zusätzliche Original-Kamerapan-Drehung für
  bestimmte Flammenschichten. Flags 0x8000 betreffen den Original-Alpha-Pass.
- Generatorintervalle, Lebensdauer, Zufallsbereiche und Fade exakt aus den
  300-Byte-Records übernehmen; unabhängiger visueller RNG, damit Kampfrandom
  unverändert bleibt. Generator/Sprite/Anm-Zustände müssen Pause und
  GL-Kontextverlust einschließlich Rauchphase identisch überstehen.
- Esp46 separat als Original-Farbfilter einbinden oder dessen fehlende
  Unterstützung ehrlich diagnostizieren; kein Fake-Quad als Ersatz.
- Erst danach EAR-SST5 und weitere Innenraumsequenzen integrieren. Die Maske
  muss dem Original-TEV folgen: RGB bleibt CPREV, Alpha ist TEXA × RASA;
  nicht pauschal die Alpha beider Texturen miteinander multiplizieren.
- Tests mit Originaldaten: alle 12 zeichnbaren EST0-Records dekodieren,
  Originalframes/Pivots/Lifetimes, begrenzter 30-Hz-Pool über lange Laufzeit,
  30/60-Hz-Äquivalenz, Pause/Replay, Context-Loss-Pixelvergleich, korrekte
  Tiefenverdeckung hinter Wänden und Kampf-RNG-Isolation.

Weitergehende Dorfaktionen sind unabhängig vom sichtbaren Feuer:
`r101_checkTowerBesieged` startet Flaschenwürfe erst nach 600 Frames im
Tower-Area und dann alle 150 Frames. Die Flaschen betreffen EST0x10..0x12
und Obj01-Physik. Die Glocke hängt an r101-Kampf-/Szenarioflags und Gegnern,
nicht an der Feuersimulation. Diese Teile bleiben separate Portschritte.

## Datenidentität

| Geprüfte Datei | SHA-256 |
| --- | --- |
| Original `St1/r101.das` | b4138f10acf31cc0bc0f63d33873f6059518cfa160f2907e120e9f75a9b70dda |
| Decodierter `r101.dat`, 5.466.016 Byte | 8d2161f73f2359874c885a755722f06118de05c3423fb86147f54517c5b30d15 |
| r101 EFF[0], 525.792 Byte | ec83b2d178cad1daf347841b37f15273948c9d33a78a320ac4ef66244ae7f89e |
| r400 EFF, 308.032 Byte | 12c10b3365beedb3f74210d21026317e8fc5a02188249a12761ee17ecaa6b13b |
