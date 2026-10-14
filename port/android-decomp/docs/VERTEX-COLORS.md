# M10: getrennte Vertexfarben und Ambientvorschau

## Originalquellen und zwei unterschiedliche Flags

`upstream/re4/src/game/trans.cpp`, commonModelTrans:
BIN-Flag 0x80000000 aktiviert GX_COLOR0 als INDEX16, GX_RGBA8,
Arraystride vier Bytes. Farbarray ist cModelData.pClr, BIN-Headeroffset 12.
Der Farbindex steht im Acht-Byte-Vertexstream zwischen Normal- und UV-Index.
Die UV-/Positionsindizes sind unabhängig und dürfen nicht als Farbindex dienen.
RGBA wird als vier Bytes gelesen, nicht als host-endian u32 umgedeutet.

`src/game/trans_lit.cpp`, LightSetModel:
Erst BIN-Flag 0x40000000 setzt RGB-Ambientquelle auf GX_SRC_VTX.
RGB-Material bleibt GX_SRC_REG; Alpha-Materialquelle ist GX_SRC_VTX,
mit deaktivierter Alpha-Beleuchtung. Ohne dieses Flag bleiben beide
Farbkanäle im Registerzweig. GXEnum.h benennt GX_SRC_VTX, GX_RGBA8,
GX_CC_RASC (10) und GX_CA_RASA (5).

`src/game/trans.cpp`, ShaderSetup/materialSetup:
Im einfachen Materialzweig multipliziert die TEV-Stufe Basistextur-RGB
mit Raster-RGB. Materialalpha stammt aus Rasteralpha. alphaSetup
multipliziert eine getrennte Maskenalpha mit der zuvor erzeugten Alpha;
der strikte Grenzwert wird auf das kombinierte Ergebnis angewandt.

## Bewusst begrenzter Renderzweig

M10 liest die vollständigen, begrenzten RGBA8-Arrays einschließlich möglicher
Ausrichtungspolster. Anzahl ist eine geprüfte Obergrenze, keine erfundene
exakte Originalanzahl. Ein gemeinsames Budget begrenzt Farbarrays auf
zwei Millionen Einträge pro SMD. GX-Farbindizes bleiben streng geprüft.

Die starre Ambientvorschau verwendet bei beiden aktiven Flags die gespeicherte
Ambientfarbe, neutrales weißes Registermaterial und keine dynamischen Lichter.
Ohne beide Flags werden weiße RGBA-Werte verwendet; vorhandene, aber im
Registerzweig unbenutzte Farben verändern das Bild nicht. Der Shader wählt
den Zweig zusätzlich ausdrücklich pro Draw. Farben werden pro Dreiecksecke
über den originalen Farbindex in den gemeinsamen Vertexbuffer übernommen.
Keine Ableitung von Farbe aus Position oder Normalen, keine neue Modelltextur.

Im aktiven Zweig ist die Basisfarbe Textur-RGB mal Vertex-RGB, Alpha stammt
von Vertexalpha. Eine getrennte Maske multipliziert diese Alpha vor der
Grenzwertprüfung und SourceAlpha/OneMinusSourceAlpha-Blendung. Ohne Maskenflag
wird kein zusätzlicher Grenzwert erfunden. Das Basisbild liefert weiterhin
keine separate Maskenalpha. Wenn ein Basisbild fehlt, zeigt der aktive
Farbzweig die Vertexfarbe als Diagnose; Maskenauflösung bleibt gesondert gezählt.
Draws mit unterschiedlichen Farbzweigen werden nicht zusammengelegt.

Diese Vorschau ist nicht die vollständige LightSetModel-Routine. Es fehlen
LightInfo-Zustände, LightDisable-/Modellsonderfälle, Lichtlisten, diffuse
Beleuchtung, Registermaterialfarben aus der Runtime, TevScaleGroup,
Schatten, Nebel, Spezialmaterialien und die vollständige GX-TEV-Quantisierung.
Daher keine Behauptung über die finale Originalhelligkeit oder pixelidentische
GX-Interpolation. Das Ziel ist der belegte Attribut-/Materialweg unter
klar angegebenen neutralen Vorschauzuständen.

## Belegte r300-Zuordnung

Die Modellnummer der App bezeichnet die Basisplatzierung; die BIN-Nummer
bezeichnet das lokale Modellarray. Sie sind nicht identisch:

| App-Platzierung | Lokales BIN | RGBA des verwendeten ersten Farbeintrags |
| --- | --- | --- |
| 4 | 5 | 128, 128, 128, 255 |
| 18 | 12 | 128, 128, 128, 255 |
| 27 | 21 | 55, 55, 55, 255 |
| 28 | 22 | 55, 55, 55, 255 |
| 45 | 38 | 109, 109, 109, 255 |
| 46 | 39 | 31, 31, 31, 255 |

Diese sechs BIN-Modelle besitzen Flag 0xe0000000. Modell 29 in der App
bleibt im neutralen Registerzweig: null Vertexfarb-Dreiecke, 6.399
Basis-Dreiecke, 304 Alpha-Dreiecke, vier Mipmap-Texturen/20 Zusatzstufen.
App-Platzierung 18 besitzt 180 Vertexfarb-Dreiecke. Die Ambientvorschau
ist bei den oben genannten Modellen erwartbar dunkler als die bisherige
unmodulierte Basistextur. Das ist kein Gamma-Regler und kein neuer Lichteffekt.
