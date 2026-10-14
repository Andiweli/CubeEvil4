# M34: Raumumgebung und Quellcode-Abgleich

Die Beleuchtung ist nicht auf r101 und r400 beschränkt. Alle 127 eindeutigen
Raumarchive aus dem Disc1-/Disc2-Katalog enthalten ein eingelesenes originales
LIT-Cut0. Die 123 mit dem derzeitigen SMD/BIN-Parser darstellbaren Räume verwenden
dieses Cut0 für Raum-, Figuren- und Effekt-Ambientfarbe, Lichtauswahl und Nebel.
80 der 127 Räume haben in Cut0 einen aktivierten Nebelmodus. Die Quellarchive
enthalten insgesamt 1236 Cut-Verzeichniseinträge, davon 1071 mit einem Offset.
Diese Zahlen bedeuten keine vollständige visuelle Originalgleichheit: Die
Kamera-Cut-Auswahl und Überblendung wird noch nicht allgemein nachgebildet.

## Neu nachgebildetes Wasser

Espgen42 und Espgen45 erzeugen ihre Geometrie aus EFF-Daten. Diese Geometrie fehlt
im SMD; ein animierter Materialshader allein konnte den Fluss und den See deshalb
nicht wiederherstellen. M34 liest die originalen Rastergrößen, Positionen,
Wellentypen, Farben, Masken, Glanztexturen und alle 60 gekachelten I8-Rauschframes.
Die Dreiecksstreifen sowie die vier 15-fachen Randflächen unbeschränkter
Espgen45-Flächen folgen dem Original. Es wird keine geschätzte Seeebene angelegt.

| Aktivierung aus dem Original | Räume |
| --- | --- |
| Globale SST, Typ 0xffff, Disp-Flag 0 | r102, r107, r10a, r10c, r10d, r10e, r112, r11a, r11b, r205, r21d, r222, r224, r226, r303, r310, r311, r31a |
| Unbedingtes R10bInit, EST2 und EST5-Steuerung | r10b |
| Unbedingtes R31bInit, EST3 | r31b |

Der Fluss in r10a verwendet das originale 120×120-Raster mit Zellgröße 530.
Der See in r10b verwendet das originale 168×168-Raster. Esp4c setzt dort
Zellgröße 309.636474609375 und feste X/Z-Position; die Wasserfarbe bleibt durch
die originale Farbmultiplikation 125/125/120/255. Die Parametersteuerung mit
Esp4c-Flag 1 in r31a und r31b übernimmt Winkel, Wellentyp, Dämpfung, Maske,
Schimmer und die absolute Farbe nach den Originalfunktionen.

Der 200-fache Vorlauf von EspGenLoopMove verändert im Original Frame_cnt nicht.
M34 verwendet deshalb bei diesen 200 Aufrufen denselben Rauschframe und dieselbe
Pufferparität. Der Frame-Zähler wird erst beim normalen nächsten Spielschritt
weitergeschaltet. Snapshot und Kontextwiederherstellung erhalten beide
Wellenpuffer, Höhen und rohe, bewusst nicht normierte Originalnormalen.

## Verifikation

`tools/check_water_reference.py` übersetzt die unveränderten Originalroutinen
Espgen42_Move00/Espgen45_Move00 mit Host-Stubs für PPC/GX-Register und Cachezugriffe.
Für beide Generatoren, alle drei Wellentypen, ein rechteckiges Raster, 200
Vorlaufaufrufe und 240 anschließende Frames wurden 3.687.300 Float-Werte exakt
verglichen: maximale Abweichung 0. Die bestehenden Umgebungstests prüfen weitere
1701 Verträge. `m34_water_check` liest zusätzlich alle 127 Originalräume, prüft
alle 20 aktiven Raster, 90 Schritte mit exakter Snapshot-Wiederholung, verwirft
beschädigte Phasen/Höhen und prüft die erzeugten Meshkoordinaten auf Endlichkeit.
Der kombinierte Originaldaten-/GLES-Durchlauf besteht 1.216.930 Prüfungen.
Der GLES3-Test mit den originalen Debug-Wasserstartpunkten (Point1) bestätigt
7660 sichtbare Wasserpixel in r10a und 52247 in r10b gegenüber demselben Raum
ohne Wassergeometrie. Animation, pausiertes Framebufferbild und Wiederherstellung
nach einem tatsächlich neu erzeugten EGL-Kontext bestehen für beide Räume.
Die Point0-Einstiege liegen in einer Passage beziehungsweise auf dem Zugangspfad;
von dort verdeckt das Gelände das Wasser. Alle QA-Kameras behalten die originalen
Startkoordinaten und Blickrichtungen; die reine Nahansicht dient zusätzlich dem
Animationstest.

## Weiter offene Originalgleichheit

Die GLES-Wasserkomposition bildet Bildschirmkopie, indirekten Bump, originale
Farbe, Reflektion, Glanztextur, Maskenalpha, fünf Lichtslots und Raumnebel ab.
Die GameCube-GX/TEV-Bildschirmkopie, Rasterisierung und Filterung sind damit noch
nicht bitidentisch. Ein vollständiger pixelweiser Vergleich mit dem Originalspiel
auf jeder Android-GPU liegt nicht vor.

Room-spezifische TexRender-Konfigurationen sind weiterhin auf r100/r300/r301
begrenzt, die verifizierten Himmelprofile auf r100/r101/r300/r301. Allgemeine
Kamera-Cut-Wechsel, dynamische Raumlicht- und Schattenhandler, Farbfilter,
eventabhängige Effektgruppen sowie Wasserinteraktionen/Boots- und Jetskiabläufe
bleiben weitere Portierungsarbeit. Die übrigen vier Wasserrecords im Katalog
werden nicht vorausgreifend aktiviert: r10b EST3 gehört zum späteren Bootsereignis,
r229 SST2 zu einem Bereich, r333 EST16 wird beim Raumstart ausdrücklich durch
SPF_WATER/DPF_WATER angehalten und ausgeblendet, r404 EST0 gehört zum bislang
nicht portierten TexRender-Ziel.

Die bestehenden vier nicht unterstützten SMD/BIN-Varianten sind r313, r314, r322
und r323. Sie sind getrennt von den erfolgreich eingelesenen 127 LIT-Daten und
den 123 tatsächlich renderbaren Räumen zu zählen.
