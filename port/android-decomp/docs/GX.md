# M05 — begrenzte GX-Displaylist-Dekodierung

## Formatbelege

`upstream/re4/src/game/trans.cpp::commonModelTrans` setzt VAT 0 und
GX_INDEX16 für Position, Normale und TEX0. Bei Modellflags bit31 kommt CLR0
mit GX_INDEX16 hinzu. Damit ist eine Referenz **6 Byte ohne Farbe** oder
**8 Byte mit Farbe**, jeweils Big-Endian-u16 in Attributreihenfolge:

| Format | +0 | +2 | +4 | +6 |
|---|---|---|---|---|
| Ohne Farbe | Position | Normale | TEX0 | — |
| Mit Farbe | Position | Normale | CLR0 | TEX0 |

Der Debugpfad `src/game/dbmodule.cpp::DrawObjWireframe` liest dagegen immer
8 Byte pro Referenz. Diese Annahme wurde ausdrücklich nicht übernommen.
Die Primitive-Codes stehen in `include/dolphin/gx/GXEnum.h`.

## Native Ausgabe

`gx_stream.h/.cpp` liest einen begrenzten Stream mit dem belegten VAT-0-Format.
Jedes Primitive enthält seine originalen, getrennten Attributindizes sowie
Dreiecksecken, die auf diese Referenzen zeigen. Es findet keine Zusammenlegung
allein anhand des Positionsindex statt: Normalen, Farben und UV-Nähte bleiben erhalten.

Quads werden mit Diagonale 0–2 geteilt. Triangle Strips wechseln ihre Orientierung
bei jedem erzeugten Dreieck; degenerierte Dreiecke werden erhalten und zählen
bei der Orientierungsparität mit. Triangle Fans, Linien, Line Strips und Punkte
haben eigene geprüfte Fälle; Linien/Punkte werden nicht als Dreiecke ausgegeben.

`SmdModel` behält die 32-Byte-Materialheader aller ursprünglichen Displaylist-
Blöcke und ordnet jedes Primitive seinem Block zu. Die Header werden noch nicht
als TEV-/Materialzustand umgesetzt. Die originale Block- und Primitive-Reihenfolge bleibt erhalten.

GX_NOP/Nullpolster werden akzeptiert. Andere VATs, CP-/XF-/BP-Kommandos,
unbekannte Codes, unvollständige Streams und ungültige Primitive-Anzahlen
werden ausdrücklich abgewiesen; dies ist noch kein allgemeiner GX-FIFO-Interpreter.

## Grenzen

Position und Normale werden gegen die angegebenen BIN-Anzahlen geprüft.
Vertex- und Normalarrays dürfen keine nachfolgende bekannte BIN-Sektion überlappen.
CLR0 und TEX0 haben hier keine ausdrücklichen Elementanzahlen im BIN-Header;
ihr Indexlimit folgt aus dem Bereich bis zur nächsten bekannten Sektion und
kann Ausrichtungspolster enthalten. Es ist eine sichere Bereichsobergrenze,
keine behauptete exakte Nutzdatenanzahl.

Pro SMD: maximal 2.000.000 Vertexreferenzen, 2.000.000 erzeugte Dreiecke und
131.072 Primitive. Diese Budgets werden über alle Modelle/Blöcke geteilt und
vor der entsprechenden Allokation geprüft. Die bisherigen Datei-/Gruppen-/
Offsetgrenzen bleiben aktiv. Common-Verweise und ältere SMD-Versionen sind
weiterhin ausdrücklich noch nicht umgesetzt.

## Prüfung mit den beiden Originaldiscs

`tools/test_gx_data.py` erzeugt unabhängig aus den originalen Streambytes eine
normalisierte Referenz. Der native Export wird **vollständig byteweise** verglichen:
Modellindex, Materialheader, Blockzuordnung, Attributindizes und Dreiecksecken.
Die Discs und Yz2-Ausgaben werden zuvor gegen ihre bekannten SHA-256 geprüft.

142 Raumarchive, 11.852 lokale BIN-Tabelleneinträge:

- 696.593 Primitive: 24.715 Quads, 18.764 Triangle Lists, 653.114 Triangle Strips.
- 12.340.412 Vertexreferenzen vollständig verglichen.
- 7.497.538 erzeugte Dreiecke einschließlich degenerierter Dreiecke verglichen.
- Keine anderen Primitive/VAT-/Registerkommandos in den unterstützten SMD-Daten.
  Fan-/Linien-/Punktfälle sind durch synthetische Verträge geprüft, nicht durch
  Vorkommen in diesem Disc-Korpus.

Das ist ein Bytevergleich mit einer unabhängigen Host-Referenz anhand belegter
Originalformate, kein Vergleich gerenderter Bilder mit der Original-GPU.

```sh
python3 tools/test_gx_data.py .host-cmake/yz2_tool .host-cmake/gx_tool /pfad/game
```

Testdaten/Exporte sind nicht im Quell-ZIP enthalten. CMake-Test gx_contracts und
`tools/test_host.sh` prüfen zusätzlich Formate, Parität, Degeneraten, Attribute,
Trunkierungen, unbekannte Codes/VATs, Budgets und 5.000 veränderte Streams.

## Android-Prüfung

Der bestehende Raumtest verwendet denselben nativen Decoder. Neue GX-Zeilen
erscheinen unter den SMD-Zusammenfassungen und in Logcat unter RE4Archive:

| Teil | Primitive | Vertexreferenzen | Dreiecke inkl. Degeneraten |
|---|---:|---:|---:|
| r100 SMD[0] | 552 | 10.534 | 6.020 |
| r100 SMD[1] | 3.577 | 36.267 | 24.351 |
| r300 SMD[0] | 6.300 | 98.097 | 59.559 |

M06 ergänzt starre Ruhepose, Welttransformationen und eine untexturierte
3D-Prüfansicht; siehe `SCENE.md`. TEV-Materialumsetzung, gemeinsame Ressourcen,
Animation und die tatsächliche Spielsimulation fehlen weiterhin.

## Erweiterung M07

M07 ergänzt die originale lokale Basistexturzuordnung und getrennt indizierte
UVs für die starre Prüfansicht. Belege, unabhängige Vergleiche und noch fehlende
Materialeffekte stehen in `MATERIALS.md`; Installation in `UPDATE-M07.md`.
