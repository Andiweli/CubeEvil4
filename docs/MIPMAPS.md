# M09: gespeicherte Mipmap-Stufen und CMPR

## Belegte Daten und Originalquellen

`upstream/re4/include/charPipeline/texPalette.h` beschreibt den 36-Byte-TPL-Bildheader:
Höhe/Breite bei 0/2, Format bei 4, Bildoffset bei 8, minLOD/maxLOD bei 33/34.
`src/lib/texPalette.c` aktiviert Mipmaps, wenn minLOD und maxLOD verschieden sind.
`src/lib/GXTexture.c` belegt formatabhängige Tilegrößen, fortlaufende Bildstufen
und Halbierung jeder Dimension mit Minimum 1. Der max_lod-Parameter der
GXGetTexBufferSize-Schleife ist eine Anzahl, nicht der höchste TPL-Levelindex.
`src/game/trans.cpp`, commonModelTrans, setzt im Mipmap-Zweig Filter 5
(GX_LIN_MIP_LIN), sonst linearen Filter. Magnifikation bleibt linear.

In r300 enthalten fünf lokale TPL-Bilder deklarierte Level 0 bis 5:

| Bild | Format | Basisgröße | Belegte Kettenbytes |
| --- | --- | --- | --- |
| 20 | CMPR | 256 × 128 | 21.856 |
| 21 | I4 | 256 × 128 | 21.856 |
| 25 | CMPR | 512 × 512 | 174.720 |
| 63 | CMPR | 128 × 64 | 5.504 |
| 64 | I4 | 128 × 64 | 5.504 |

Die Ketten enden jeweils am Beginn des nächsten Bildes. Modell 29 verwendet
vier dieser Bilder: 20 zusätzliche Stufen, insgesamt weiterhin 21 Texturen,
6.399 texturierte und 304 Alpha-Dreiecke, jeweils null ungelöst.

## Umsetzung und Grenzen

DecodeTpl bleibt standardmäßig ein Basisbild-Decoder. DecodeTplMipmapped
lädt zusätzliche, vollständig gespeicherte Stufen. Jede Stufe wird mit dem
bestehenden GX-Tile-Decoder gelesen; keine automatisch erzeugten Mipmaps.
Das Szenenbudget zählt sämtliche RGBA-Stufen einschließlich Masken mit.
Die GPU erhält jede Stufe explizit über glTexImage2D. MAX_LEVEL/MAX_LOD werden
auf die geladenen Stufen begrenzt; auch eine kürzere Kette bleibt vollständig
im Sinne von GLES. Ohne Zusatzstufen wird ausschließlich linear gefiltert.

Bei manchen Originalbildern gibt der Header mehr Stufen an, als bis zur
nächsten Bild-/Header-/CLUT-Grenze vollständig vorhanden sind. Beispielsweise
r100 Bild 4: 128 × 32 CMPR, deklarierter Maximallevel 5, aber 2.784 Bytes
bis zum nächsten Bild; diese enthalten nur Level 0 bis 4. M09 liest keine
Bytes des Nachbarbildes. Eine bereits vorhandene Zusatzkette darf an ihrer
Grenze oder vor unzureichendem 32-Byte-ausgerichtetem Rest enden. Sie wird
auf die vollständigen Stufen begrenzt und als begrenzte Kette im RE4Scene-Log
gezählt. Unvollständige Basisbilder, nicht ausgerichtete Abbrüche, ein fehlender
erster Zusatzlevel und nicht erfüllbare minLOD-Grenzen werden abgewiesen.
Damit ist diese Behandlung eine begrenzte Android-Dateninterpretation,
keine Behauptung über das Verhalten von GX bei übergroßen Headergrenzen.

Dimensionen, Zweierpotenzen bei Mipmap-Bildern, natürliche maximale Level,
Headerintervalle und Ressourcenbereiche werden geprüft. Original minLOD
wird verwendet, dynamische Szenen-LOD-Werte, Bias, Anisotropie und GX-edgeLOD
sind noch nicht implementiert. GLES-LOD-Ableitung ist nicht identisch mit GX.
Eine vollständige TEV-/Licht-/Runtime-Portierung bleibt offen.

## CMPR-Korrektur

CMPR-Farbwerte zwischen den RGB565-Endpunkten verwenden bei vier Farben
5/8–3/8 bzw. 3/8–5/8 statt der bisher benutzten PC-DXT1-Drittel.
Im transparenten Zweig besitzt der transparente Texel den RGB-Mittelwert
beider Endpunkte bei Alpha 0; RGB darf für gefilterte Kanten nicht pauschal
schwarz sein. Die Mathematik ist unabhängig implementiert und mit festen
Erwartungswerten geprüft: Rot/Grün ergibt (159,95,0) und (95,159,0);
Schwarz/Weiß transparent ergibt (127,127,127,0).
Primärreferenz für die Formatsemantik:
https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/TextureConversionShader.cpp
Es wurde kein Dolphin-Quellcode übernommen.

Der separate M08-Alphazweig bleibt aktiv; CMPR-Basisalpha ersetzt keine
separat referenzierte Maske. Mipmap-Filterung kann entfernte Details beruhigen,
ist aber keine Diagnose oder Zusage für jede mögliche Ursache von Flackern.
