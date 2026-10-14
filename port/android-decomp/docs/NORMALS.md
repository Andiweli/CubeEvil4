# M11: Originalnormalen und Welttransformation

## Belegter Datenweg

`upstream/re4/src/game/trans.cpp`, commonModelTrans, konfiguriert Normalen
als INDEX16 mit getrenntem Attributindex. BIN-Flag 0x20000000 wählt GX_S8
mit vier Byte Arraystride; ohne dieses Flag GX_S16 mit acht Byte Stride.
Drei Komponenten belegen die ersten drei bzw. sechs Bytes. Restbytes sind
Polster, kein vierter Normalenwert. Der BIN-Header verweist bei Offset 52
auf das Originalarray; BE-u16 bei Offset 58 ist die Normalenanzahl.
Der Normalindex steht bei Streamoffset 2 hinter dem Positionsindex.

GX verwendet für S8-Normalen sechs und für S16-Normalen vierzehn implizite
Nachkommabits. Das shift-Argument von GXSetVtxAttrFmt wird für Normalen
nicht im VAT gespeichert (src/lib/GXAttr.c, SETVAT). Deshalb teilt M11
signed S8 durch 64 und signed BE-S16 durch 16384. Das ist insbesondere
kein S8/127 oder S8/128. Werte außerhalb [-1,1] werden nicht abgeschnitten
oder bei der Datenaufnahme voreilig normalisiert.
Primärreferenz für diese Hardware-Formatkonvention, separat überprüft:
https://github.com/dolphin-emu/dolphin/blob/master/Source/Core/VideoCommon/VertexLoader_Normal.cpp
Es wurde kein Dolphin-Code übernommen; die unabhängige Implementierung
besteht aus signed Integer-Umwandlung und dem belegten Zweierpotenzfaktor.

commonModelTrans lädt für Normalen die inverse transponierte Modell-/Viewmatrix.
M11 berechnet daraus zunächst den Weltanteil: inverse Transposition der
3x3-Rotations-/Skalierungsmatrix der starren Basisplatzierung. Translation
und Gelenkzentrum werden auf Normalen nicht angewandt. Der Kamerateil wird
für die spätere Beleuchtungsportierung noch nicht als Normalenmatrix benötigt;
die Prüfansicht zeigt ausdrücklich Weltnormalen.

## Umsetzung und Grenzen

SmdModel speichert die nicht normalisierten Originalwerte. Arraygrenzen,
Originalanzahl und bestehende GX-Normalindizes bleiben geprüft; ein zusätzliches
Budget begrenzt Normalen auf zwei Millionen Einträge pro SMD. Die Scene löst
Normalen pro Dreiecksecke über ihren eigenen Index auf. Keine Ableitung
von Normalen aus Positionen, UVs oder Dreieckswinding.

Die Normalenmatrix wird einmal pro Platzierung mit Double-Zwischenwerten
berechnet, danach als Float übertragen. Negative Skalierung wird berücksichtigt.
Bei einer exakt singulären Matrix gibt es keine inverse Matrix: M11 meldet
die Platzierung und setzt ihre Prüf-Normalen auf null. Geometrie wird deswegen
nicht versteckt oder durch eine erfundene Richtung ersetzt. Nicht endliche
oder übergelaufene Werte werden abgewiesen. Nullnormalen werden ausdrücklich
gezählt. Sie können auch bereits als gültige Originaldaten vorhanden sein.

Der bestehende Vertexbuffer besitzt nun zwölf Floatwerte pro Ecke:
Position 3, UV 2, RGBA 4, Normalen 3. GLES-Attribut 3 ist die Weltnormale.
Die normale Texturansicht verwendet weiterhin den M10-Materialzweig; neue
Normalen werden dort noch nicht für eine erfundene Lichtquelle benutzt.

## SELECT-Prüfansicht

Nur in der statischen 3D-Ansicht schaltet SELECT den normalen Texturzweig
und eine Normalenansicht um. Es ist derselbe Controller-Map-Action-Eingang;
das zukünftige Gameplay-Mapping wird nicht geändert. Es werden keine Texturen
neu dekodiert oder hochgeladen und Orbit/Abstand bleiben erhalten. Modellwechsel
und Start-Gesamtansicht behalten den gewählten Prüfmodus bei. Nach Neuerstellung
des GLES-Kontexts beginnt die Vorschau wieder mit Texturen.

Die Ansicht zeigt interpolierte Weltnormalen als RGB = (normalize(n)+1)/2.
Eine Nullnormale wird magenta markiert. Das ist ausschließlich eine Diagnosefarbe,
keine defekte Spieltextur. Dieselben Alphamasken, Grenzwerte und Blendzustände
bleiben aktiv, sodass ausgesparte Flächen ausgespart bleiben.
RE4Scene meldet Normalen-Ecken, Nullnormalen, singuläre Platzierungen sowie
Normalenansicht EIN/AUS beim Wechsel. Die Statuszeile kennzeichnet den Prüfmodus.

Dies ist ein belegter Attribut- und Transformationsschritt vor echter Beleuchtung.
Weiterhin fehlen dynamische Lichtlisten, GX-Lichtparameter, Material-/Ambient-
Register aus der Runtime, Nebel, Schatten, Spezial-TEV, Animation und Gameplay.
GLES-Interpolation und Normalisierung sind keine Zusage eines pixelidentischen
GX-Endbilds. Vier ältere SMD-Blöcke bleiben wie zuvor vom Rendering ausgeschlossen.

## Prüfstand

Alle 142 Raumarchive: 139 unterstützte SMD-Blöcke, 7.190.666 S8-Normalen,
34.961.214 Normalen an Welt-Dreiecksecken, 3.961 Nullnormalen und null
singuläre Platzierungen. Die Originalraumdaten benutzen hier ausschließlich S8;
S16 ist durch feste synthetische Erwartungswerte geprüft. Der unabhängige
NumPy-Vergleich liest Originalbytes und getrennte Normalindizes und berechnet
inverse Matrizen unabhängig vom C++-Cofaktorweg. Normalentoleranz: relativ 1e-4,
absolut 1e-5; bestehende Weltpositionsprüfung weiterhin absolut 0,125.

r300-Modell 29: 19.197 Normalen-Ecken, null Nullnormalen, null singuläre
Platzierungen. Die Prüfansicht verändert im Mesa-Test 16.024 Bildpunkte;
beim Zurückschalten wird das Textur-Framebuffer exakt wiederhergestellt.
Bestehende 6.399 Basis-Dreiecke, 304 Alpha-Dreiecke und 20 Mipmap-Zusatzstufen
bleiben unverändert. Geräteprüfung von M11 steht nach der Installation aus.
