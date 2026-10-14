# M04 — SMD-Platzierungen und BIN-Geometriegrenzen

## Belegte Formate

Der Leser ist aus dem unveränderten Referenzstand abgeleitet:

- `upstream/re4/include/scroll.h` und `src/game/scroll.cpp`: SMD-Header,
  Gruppentabelle, 0x48-Byte-Platzierungen, relative BIN/TPL/Motion-Tabellen.
- `include/model.h`, `src/game/model.cpp::calcModelAddr` und `getBoundingBox`:
  BIN-Header und originale Vertexpositionen als drei Big-Endian-s16 in 8-Byte-
  Datensätzen, skaliert mit 2^-shift.
- `src/game/dbmodule.cpp::DrawObjWireframe`: 0x20-Byte-Primitive-Header,
  buff_size an +0x18, anschließend der Stream; nächster Header nach 0x20+buff_size.

SMD-Version 0x40 wird unterstützt. Vier SMD-Teil-Dateien der beiden Discs haben
Version 0 oder 0x31; sie werden ausdrücklich als noch nicht unterstützt gemeldet.
Eine erfolgreiche Archivprüfung ist keine Zusage, dass deren Geometrie geladen ist.

## Regeln des Lesers

Es werden **nModel** Basis-Platzierungen gelesen. Bei Flag bit0 beginnt die
Work-Liste hinter nGroup und nGroup Gruppenzählern. getWorkNum ergibt dagegen
nModel plus die Gruppenzähler; diese Summe beschreibt keine zusätzlichen
fortlaufenden 0x48-Byte-Records. r100 SMD[0] hat 13 Basis-Platzierungen, aber
310 logische Works. Das falsche Lesen von 310 Records würde Tabellen als
Transformationen interpretieren.

BIN/TPL/Motion-Tabelleneinträge sind relativ zum jeweiligen Tabellenanfang,
nicht zum SMD-Anfang. Gemeinsame Ressourcen über flags bit4/bit6 werden
als externe Verweise erhalten und noch nicht aufgelöst. Inaktive WorkNo=0xff
bestimmen keine lokalen Tabellenlängen. Bei aktiven BIN/TPL-IDs 0xff wird die
Index-0-Ersatzregel aus SmdSetParam berücksichtigt.

Dateioffsets bleiben 32-Bit-Werte; keine GameCube-Zeiger werden in native
Strukturen gecastet. Transformationswerte müssen endlich sein. Größen,
Gruppensummen, Tabellen, Vertexarrays und Displaylist-Blöcke werden begrenzt.
BIN-Lesebereiche enden spätestens an der nächsten bekannten Resource oder
Tabelle; Polster innerhalb solcher Bereiche sind keine präzisen Dateilängen.

Der Leser berechnet **lokale** Vertex-Minima/Maxima. Gelenktransformationen,
Weltkoordinaten, Skinning und Animation sind nicht enthalten. BIN-Versionen
0x20010801, 0x20030817 und 0x20030818 sind zugelassen. Primitive-Blöcke werden
anhand buff_size begrenzt. Seit M05 werden ihre belegten VAT-0-Kommandos und
Attributindizes zusätzlich dekodiert und geprüft; siehe `GX.md`.

## Nachweis mit Originaldaten

Alle 142 Raumarchive erneut eingelesen; Quell- und Ausgabehashes geprüft:

- 139 unterstützte SMD-Teil-Dateien, 4 ausdrücklich nicht unterstützte.
- 22.922 Basis-Platzierungen und deren neun Transformationswerte geprüft.
- 11.852 lokale BIN-Tabelleneinträge, 4.979.572 Vertices und 29.751 Displaylist-Blöcke.
  Die Zählung folgt Tabellenindizes, nicht einer globalen Deduplizierung.
- 11.991 BIN/TPL/Motion-Abfragen mit den tatsächlich ausgeführten originalen
  PowerPC-getBinPtr/getTplPtr/getMotPtr verglichen: identische Offsets.
- Beginn der Work-Liste und logische Work-Anzahl jedes unterstützten SMD
  mit originalem getWorkPtr(0)/getWorkNum verglichen.
- Alle lokalen Vertexgrenzen mit einer unabhängigen Python-s16-Referenz verglichen.
  Ein versuchter binärer Original-getBoundingBox-Lauf brach im Emulator mit
  UC_ERR_EXCEPTION ab und zählt ausdrücklich nicht als bestandener Nachweis.

Reproduzieren nach Host-CMake-Build mit separat vorliegenden Spieldaten:

```sh
python3 -m pip install unicorn==2.1.4
python3 tools/test_smd_data.py .host-cmake/yz2_tool .host-cmake/smd_tool /pfad/game
```

Die Testwerkzeuge gelangen nicht in die Android-App. Das Paket enthält keine Spieldaten.

## Auf Android

Der bestehende Raumtest ergänzt SMD-Zusammenfassungen in der scrollbaren
Inhaltsliste und Logcat unter RE4Archive. r100 und r300 behalten ihre
Yz2-/SHA-256-Prüfung; die bisherige Raumtexturvorschau bleibt verfügbar.

| Raum / Teil | Basis-Platzierungen | Gruppen | Logische Works | Lokale BIN-Einträge | Vertices | Displaylist-Blöcke |
|---|---:|---:|---:|---:|---:|---:|
| r100 SMD[0] | 13 | 5 | 310 | 13 | 5.290 | 22 |
| r100 SMD[1] | 18 | 0 | 18 | 18 | 13.086 | 24 |
| r300 SMD[0] | 48 | 0 | 48 | 48 | 39.684 | 246 |

## Nächster Schritt

M05 dekodiert die belegten GX-Primitive und Vertexformate; siehe `GX.md`.
M06 verbindet originale Positionen, starre Ruhepose und Welttransformationen
für eine erste untexturierte Prüfansicht; siehe `SCENE.md`. Common-Verweise und
animierte Modelle sind darin ausdrücklich ausgeschlossen. Noch kein Gameplay,
kein vollständig portierter GX-/TEV-Renderer und keine neue Controller-Spielanbindung.

## Erweiterung M07

M07 ergänzt die originale lokale Basistexturzuordnung und getrennt indizierte
UVs für die starre Prüfansicht. Belege, unabhängige Vergleiche und noch fehlende
Materialeffekte stehen in `MATERIALS.md`; Installation in `UPDATE-M07.md`.
