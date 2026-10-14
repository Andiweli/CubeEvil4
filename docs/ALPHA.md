# M08 — separate Alphatexturen und Grenzwertprüfung

M07 ist durch Eingefügter Text(9).txt und Nutzerbestätigung geprüft: r100/r300
PASS, Modell 29 mit 6.399 texturierten Dreiecken, null ungelöst, Modellwechsel.
Kein App-Crash/ANR im Mitschnitt.

## Originalbelege

Die Pfade liegen unter upstream/re4:

- include/model.h::cPolyHeader: Flag an 0x0B, Alphatextur-ID an 0x0E,
  Alpha-Grenzwert an 0x14.
- src/game/trans.cpp::shaderSetup: Alpha-Zweig nur bei Materialflag 4.
- trans.cpp::alphaSetup: separate Textur, Alpha aus TEXA und APREV;
  AlphaCompare GREATER/ref OR GREATER/255.
- src/game/model.cpp::cModel: alpha_omit initial 0xFF, daher im Basiszustand
  Grenzwert des Displaylist-Headers.
- include/dolphin/gx/GXEnum.h: GREATER=4, OR=1, TEXA=4, APREV=0, RASA=5.
- trans.cpp::materialSetup: Basisstufe übernimmt Rasteralpha; die
  Basistexturnummer liefert nicht automatisch den Ausschnitt.
- trans.cpp::commonModelTrans: Blendmodus 0 ist SourceAlpha/InverseSourceAlpha.
  ModelRender benutzt bei z_mode 0 Tiefentest und Tiefenschreiben.

Bei 8-Bit-Alpha reduziert sich der Vergleich auf strikt alpha > reference.
Der zweite OR-Term alpha > 255 ist immer falsch. Gleichheit wird verworfen;
Referenz 255 verwirft auch Alpha 255. Die Prüfung wird nur bei aktivem
Materialflag UND aufgelöster separater Alphatextur angewendet.

Die Maske nutzt in dieser statischen Vorschau dieselbe originale UV wie das
Basisbild. Beide Ressourcen teilen Palettenzuordnung, Wrap-Modi, Repeat-
Override, Decoderbudget und Deduplizierung. Fehlende Common-/Renderziel-
Masken bleiben ausdrücklich ungelöst; keine fremde Maske wird verwendet.

## Decoderkorrektur

I4/I8 replizieren Intensität in alle vier RGBA-Komponenten. Der bisherige
Decoder setzte Alpha irrtümlich auf 255. M08 korrigiert dies; RGB bleibt gleich.
Die fünf separaten Masken von Modell 29 sind I4. Ohne die Korrektur würden
schwarze Maskenflächen nicht ausgeschnitten.

Zusätzliche Primärreferenz, geprüft ohne Codeübernahme:
https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/TextureDecoder_Common.cpp
(TexDecoder_DecodeTexel, I4/I8: Intensität auch in dst[3]).

## GLES und Grenzen

Die Shaderstufe liest Maskenalpha, rundet den linear gefilterten Wert für den
Vergleich auf das nächste 8-Bit-Ergebnis und verwirft Werte <= Header-Referenz.
Verworfene Fragmente schreiben weder Farbe noch Tiefe. Ausgabealpha steuert
SourceAlpha/InverseSourceAlpha-Blending; Tiefentest und Schreiben bleiben an.
Ohne Maskenzweig ist Ausgabealpha 1, unabhängig vom Alpha des Basisbilds.
GPU-Batches werden nur bei gleicher Basistextur, gleicher Maske, Aktivierung
UND Referenz zusammengefasst.

Prüfannahmen: Rasteralpha 255, keine Animation/UV-Scroll, Modell-Override 0xFF,
Blendmodus 0 und z_mode 0. Keine vollständigen Runtime-Zustände, Vertexfarben/-
alpha, Shadow-/Thermo-Stufen, Ereignis-Overrides, anderen Blend-/Tiefenmodi oder
Original-OT-Sortierung. Teiltransparente Überlappungen können noch abweichen.
GLES-Filter-/Rundungspräzision wurde nicht pixelgenau gegen GameCube geprüft.
Kein Gameplay und kein vollständiges TEV-Endbild.

## Validierung

Der unabhängige Materialtest exportiert Alpha-Flag, Masken-ID, Referenz und
Auflösbarkeit. Dimensionen/Format der decodierten Texturen werden verglichen.
I4/I8-Alpha wird zusätzlich direkt aus Original-Tile-Bytes rekonstruiert und
über FNV-1a der gesamten Alphaebene geprüft.

142 Raumarchive: 792.166 Dreiecke mit aufgelöstem Alpha-Zweig, 3.543 ungelöst;
1.458 geprüfte I4/I8-Alpha-Bilder. Unveränderte Basiszuordnung:
11.647.718 lokal texturierte und 6.020 ungelöste Basis-Dreiecke.

Der GLES-Test prüft sourcefreie Flächen: 0/0, 127/127 und 255/255 verworfen;
128/127 sichtbar und geblendet; Basisalpha 0 ohne Maskenzweig bleibt sichtbar.
Benachbarte Dreiecke mit verschiedenen Referenzen bleiben getrennte Batches.
Modellwechsel und Ressourcenabbau werden weiter geprüft.

Modell 29: 16 Basistexturen plus 5 Masken, 6.399 Basis-Dreiecke,
304 Alpha-Dreiecke, null ungelöst. GLES-3.2-Hosttest bestanden und Bild visuell
geprüft. M08-Android-Geräteprüfung steht aus.
