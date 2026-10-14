# Raumarchiv — Milestone 03

Der Geräte-Screenshot von M02 bestätigt beide Originalausgaben: r100 (4.669.568 Byte)
und r300 (5.427.520 Byte), jeweils PASS mit den erwarteten SHA-256-Fingerprints.
M03 verarbeitet jetzt die Dateitabelle INNERHALB dieser entpackten Archive.

## Tatsächliches Dateiformat

Beleg: `upstream/re4/src/game/read.cpp`, `DataExtHeader` und `GetDataExt`,
Originalfunktion im geprüften `main.dol` bei **0x8017FD18**.

| Position | Inhalt |
|---|---|
| 0x00 | Dateianzahl, Big-Endian u32 |
| 0x04–0x0f | Drei weitere Headerwörter, keine neue Bedeutung unterstellt |
| 0x10 | Anzahl × 4 Byte Dateioffsets, relativ zum Archivanfang |
| 0x10 + Anzahl × 4 | Anzahl × 4 Byte Tags |

Das Original vergleicht nur die ersten drei Tagbytes und zählt Mehrfachvorkommen.
So sind `LIT[0]` und `LIT[1]` unterschiedliche Einträge; das vierte Byte wird
aufbewahrt und beim Suchen ignoriert. Die neue API erwartet exakt drei Tagbytes.
Fehlende Einträge und negative Vorkommensnummern ergeben keinen Treffer.

**Die Tabelle enthält keine Nutzdatenlängen.** Der Leser begrenzt jeden Bereich
bis zum nächsten größeren Dateioffset oder Archivende. Dieser Bereich kann
Ausrichtungspolster enthalten und wird ausdrücklich nicht als exakte Dateigröße
bezeichnet. Eigene Formatdecoder müssen die tatsächliche Nutzdatenlänge bestimmen.

`RoomArchive` besitzt die unveränderten Dateibytes. Keine Binärstruktur wird auf
native ARM64-Pointer gecastet; keine Originaladresse wird dereferenziert.
Grenzen: 64 MiB Archiv, maximal 4.096 Einträge; vollständige Tabellen und alle
Dateioffsets müssen innerhalb des Archivs liegen. Auch unsortierte Offsets und
Aliases erhalten begrenzte Bereiche. Die geprüften Discs enthalten streng
aufsteigende Offsets ohne Aliases; synthetische Tests decken die Alternativen ab.

## Unabhängige Validierung

`tools/test_archive_data.py` führt die unveränderten PowerPC-Instruktionen von
`GetDataExt` aus. Die Eingabedateien wurden zuvor mit den M02-Originalfingerprints
abgeglichen. Jede native Suchfunktion wird auf dem Host ebenfalls aufgerufen.
Zusätzlich prüft ein separater BE-Tabellenleser Tags, Offsets und Bereichsgrenzen.

- **142 Raumarchive** beider Discs.
- **5.005 Archiv-Einträge**.
- **13.075 Original-PowerPC-Suchabfragen**: vorhandene Vorkommen, fehlende weitere
  Vorkommen, negative Indizes und fehlender Tag.
- **335 eingebettete TPL-Bilder**, sämtliche Bildindizes der vorhandenen TPL-Teile
  mit dem begrenzten Decoder verarbeitet. Das beweist keine fertige GX-/TEV-Darstellung.

Nach Host-CMake-Build mit separaten Spieldaten reproduzieren:

```sh
python3 -m pip install unicorn==2.1.4
python3 tools/test_archive_data.py .host-cmake/yz2_tool .host-cmake/archive_tool /pfad/game
```

Unicorn bleibt ein optionales Host-Testwerkzeug und gelangt nicht in die Android-App.
Die Tests enthalten keine Original-Spieldaten.

## Android-Verhalten

„Raumarchiv und Yz2 prüfen“ verifiziert weiterhin die beiden vollständigen
entpackten Originalausgaben und liest anschließend deren Teil-Dateien.
Die Teil-Dateien sind raumabhängig: r300 enthält beispielsweise keinen EMI-Teil.
Ein fehlender Tag ist bei der Inhaltsprüfung kein Fehler; Original-GetDataExt
liefert dafür null. Archivgrenzen, Ausgabegröße und SHA-256 bleiben geprüft.
Die Inhaltsliste zeigt Tag, Vorkommen, Offset und begrenzten Bereich.
Die beiden PASS-Ergebnisse stehen vor den ausführlichen, scrollbar angezeigten Listen.

„Raumtextur aus r100 anzeigen“ öffnet die bestehende GLES-/Controller-Testansicht
mit **TPL[0], Bild 6: 256×256, CMPR/GX-Format 14**. Dieser Bildindex gehört zum
fest geprüften r100-Originalbestand. Der vollständige zugehörige TPL-Teil wird
als Vorschau im App-Cache gehalten; die installierten Disc-Dateien bleiben unverändert.
Der Cache kann vom System entfernt werden; dann die Raumprüfung erneut ausführen.

Prüfergebnisse und Archivzeilen erscheinen zusätzlich in Logcat unter **RE4Archive**.
Es gibt kein Logging pro Renderframe und keinen neuen externen Logordner.

## Noch offen

SMD/SAT/RTP/etc. werden gefunden, ihre inneren Strukturen werden aber noch nicht
als native Spielobjekte geladen. Keine 3D-Raumdarstellung, kein GX-/TEV-Renderer,
keine Spielsimulation. M04 liest jetzt SMD-Platzierungen, lokale BIN-Vertices und die Grenzen der
Displaylist-Blöcke; siehe `SMD.md`. M05 dekodiert jetzt die belegten VAT-0-Primitive; siehe `GX.md`.
Welttransformationen und Renderer-Anbindung folgen.

M06 ergänzt nach erfolgreicher r300-Prüfung eine untexturierte starre
3D-Prüfansicht. Das decodierte Archiv wird dafür atomar im App-Cache abgelegt;
die importierten Originaldateien bleiben unverändert. Details: `SCENE.md`.

## Erweiterung M07

M07 ergänzt die originale lokale Basistexturzuordnung und getrennt indizierte
UVs für die starre Prüfansicht. Belege, unabhängige Vergleiche und noch fehlende
Materialeffekte stehen in `MATERIALS.md`; Installation in `UPDATE-M07.md`.
