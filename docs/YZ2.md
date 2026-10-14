# Yz2 — Milestone 02

Dieser Meilenstein portiert einen echten Datenladebaustein der Engine.
Er stellt noch keinen Raum dar und startet kein Gameplay.

## Herkunft und Dekodierung

Basis: fixierter Upstream-Commit aus `PORTING.md`, `src/game/yz2code.cpp` und
`src/game/yz2asm.cpp`, abgeglichen mit dem bereitgestellten `main.dol`.

- Zwei adaptive Frequenzmodelle: 0x500 und 0x100 Symbole.
- Beide teilen EINEN arithmetischen 32-Bit-Bereich und Codewert.
- Literal ist ein Hauptsymbol **größer als 0x3ff**; die unteren acht Bit sind das Byte.
  Der gegenteilige Kommentar im Upstream ist nicht maßgebend. Entscheidend ist der
  originale `cmplwi`-/`ble`-Pfad.
- 256 nach Präfixbyte gewählte Wörterbücher mit jeweils 512 Ring-Einträgen.
- Symbole 0..0x1ff verwenden eine neu dekodierte Länge; 0x200..0x3ff die gespeicherte Länge.
- Referenzen werden vorwärts byteweise kopiert, einschließlich überlappender Runs.
- Frequenzgewichte bei der späten Reskalierung stammen aus den Zählern VOR der Halbierung.
  Die Reihenfolge sowie unsigned 32-Bit-Rechenoperationen bleiben erhalten.
- Originale Ausgabepointer wurden durch begrenzte Offsets ersetzt. Keine GameCube-
  Adresse wird auf Android dereferenziert.

Die DAS-Dateien enthalten vor dem Yz2-Stream einen Capcom-DVD-Header.
`DecodeYz2` liest dessen ersten MRAM-Teil über dessen Big-Endian-Größe und Offset.
Weitere Audio-/ARAM-Teile werden nicht als komprimierte Raumdaten interpretiert.
Der ASCII-Header enthält gepackte und entpackte Hex-Größen; der Stream beginnt
an der folgenden 32-Byte-Grenze. Das gepackte Feld schließt die vier Flush-Bytes
nicht ein. Bei den geprüften Dateien wurden höchstens `packed + 4` Bytes gelesen.

Grenzen: maximal 128 MiB Eingabe und standardmäßig 64 MiB Ausgabe. Verkürzte Streams,
ungültige Header, nicht initialisierte Referenzen und Ausgabeverweise außerhalb des
Budgets werden abgewiesen. Mehrere MRAM-Teile und verschachtelte DVD-Container sind
bewusst noch nicht unterstützt. Korrupte Streams können gültige andere Daten ergeben;
für die Originaldateien ist deshalb der zusätzliche SHA-256-Vergleich verbindlich.

## Unabhängiger Originalvergleich

`tools/yz2_oracle.py` führt die unveränderten PowerPC-Instruktionen aus dem geprüften
`main.dol` aus, Einstieg **0x801D3E04**, inklusive beider originaler Frequenzdecoder.
Nur Kontext, Stack und Workbuffer werden wie im Originalsetup vorbereitet.
Das erwartete Ergebnis stammt somit nicht aus einer zweiten Kopie des neuen C++-Decoders.
Die ursprüngliche C-Setupfunktion selbst wird dabei nicht ausgeführt.

**Alle 142 `r*.das`-Dateien: 85 auf Disc 1, 57 auf Disc 2, bytegenau identisch.**
`assets/rooms.tsv` enthält Dateipfade, Ausgabegrößen und SHA-256 von Quelle und
Originalausgabe. Das sind Prüffingerprints; keine Spieldaten werden ausgeliefert.

Host-Nachprüfung:

```sh
cmake -S app/src/main/cpp -B .host-cmake
cmake --build .host-cmake
ctest --test-dir .host-cmake --output-on-failure
python3 tools/test_yz2_data.py .host-cmake/yz2_tool /pfad/game
```

`game` enthält `disc1/files`, `disc2/files` und `disc1/sys/main.dol`, wie der App-Importer.
Mit `python3 -m pip install unicorn==2.1.4` und zusätzlich `--oracle` wird der tatsächliche
PowerPC-Vergleich erneut ausgeführt. Unicorn wird ausschließlich für diesen Host-Test
verwendet und ist **kein Bestandteil oder Laufzeitpfad der Android-App**.
Das Host-Oracle hat engere Workbuffergrenzen und ist auf die geprüften Discs festgelegt.

## Android-Prüfung

Im Hauptmenü „Yz2-Raumdaten prüfen“ öffnen und „Prüfung starten“ drücken.
`r100` von Disc 1 und `r300` von Disc 2 werden nativ entpackt und anschließend
gegen die Originalfingerprints geprüft. Die Original-Quelldateien werden ebenfalls
gehasht. Ausgaben liegen nur kurz im App-Cache und werden danach gelöscht.
Bestehende Disc-Imports müssen nicht erneut durchgeführt werden.
Die Laufzeit umfasst Dateioperationen; sie ist kein FPS- oder Gameplay-Benchmark.

Ab M03 ist die Raumarchiv-Dateitabelle als begrenzte Big-Endian-Struktur umgesetzt;
siehe `ARCHIVE.md`. Danach Modelle/Displaylists portieren.
Die Ausgabe ist noch keine native Runtime-Struktur; ein Cast auf Originalpointer wäre falsch.
