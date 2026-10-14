# M07 — originale UVs und lokale Basistexturen

Dieser Patch erweitert die starre M06-Prüfansicht. Er stellt originale
Basistexturen dar, aber noch kein vollständiges GameCube-Material oder Gameplay.

## Belegter Datenweg

`upstream/re4/src/game/trans.cpp::commonModelTrans` setzt TEX0 auf ein
4-Byte-Array. Bei BIN-Flag 0x80000000 ist das Format signed 16-Bit, frac 8;
sonst unsigned 16-Bit, frac 15. Entsprechend wird jedes Paar durch 256 bzw.
32768 geteilt. Diese Flag-Auswertung ist getrennt von der GX-Streamlänge:
farbige Streams besitzen zusätzlich den Farbindex, aber immer einen eigenen
UV-Index. Die Parsergrenze folgt wie bisher der nächsten bekannten BIN-Sektion;
Ausrichtungspadding ist keine behauptete Anzahl tatsächlicher UV-Elemente.
Die getrennten Positions-/UV-Indizes bleiben beim Aufbau aller Dreiecksecken
erhalten. Es wird keine UV an den Positionsindex angehängt, die Textur-Nähte verliert.

`include/model.h::cPolyHeader` und `trans.cpp::materialSetup` belegen die
Basistexturnummer an Displaylist-Headeroffset 0x0C. `scroll.cpp::SmdSetParam`
verknüpft die lokale TPL über TplId des Basis-Works; 0xFF wird beim lokalen
aktiven Verweis wie zuvor durch Index 0 ersetzt. Materialnummern beziehen sich
auf Deskriptoren dieser Palette, nicht auf die separate TPL-Archivvorschau.
Originale UVs werden unverändert an GLES weitergegeben, ohne erfundene V-Invertierung.

Die vorhandenen Decoder für alle elf unterstützten GX-TPL-Formate liefern RGBA.
TPL-Wrap S/T (Clamp, Repeat, Mirror) wird aus Headeroffset 12/16 gelesen;
Werte außerhalb 0..2 werden abgewiesen. Wie im Original erzwingt BIN-Flag
0x80000000 Repeat in beiden Richtungen. Gleiche Palette/Bild/Wrap-Kombinationen
werden innerhalb einer Vorschau nur einmal decodiert und hochgeladen.

Der Originalrenderer kann Common-TPLs anhängen und Texturen ab 0xF8 aus
Renderzielen laden. Diese Ressourcen sind hier noch nicht aufgelöst. Ein solcher
Verweis erhält Diagnosefarbe und einen expliziten Zähler; keine Ersatzzuteilung.
Beschädigte lokale Paletten lösen einen Fehler aus. Begrenzungen: insgesamt
maximal 64 MiB kopierte Paletten, zwei Millionen UV-Paare pro SMD und
128 MiB decodierte Basistexturen pro Vorschau.

## GLES und Grenzen

Die Vorschau verwendet fünf Floats pro GPU-Vertex: Weltposition und eigene UV.
Draw-Bereiche behalten die Originalreihenfolge. Benachbarte Bereiche mit gleicher
aufgelöster Basistextur werden für GLES zusammengefasst, um viele kleine
Primitive nicht als einzelne Drawcalls einzureichen. Modellwechsel und
Kontextverlust behandeln Texturobjekte wie die bestehenden VBO-/VAO-Ressourcen.
Controllerbelegung und Kamera bleiben wie in M06.

Aufgelöste Basistexturen werden ohne das künstliche M06-Debuglicht angezeigt;
ungelöste Flächen verwenden weiterhin Diagnosefarbe. Es wird nur der Basismip
mit linearem Filter verwendet. Vollständige TEV-Stufen, Vertexfarben, Original-
Beleuchtung, separate Alphatexturen, Alpha-Test/Blending, Bump, Specular,
UV-Scroll, Texturanimation, Mipketten und tatsächliche Raum-Sichtbarkeit fehlen.
Insbesondere können transparente Flächen noch als undurchsichtige Rechtecke
erscheinen. Dies ist eine texturierte Geometrieprüfung, kein originalgetreues
Endbild und kein spielbarer Raum.

## Unabhängige Prüfung

`tools/material_tool.cpp` exportiert UV-Werte und primitiveweise Basiszuordnung.
`tools/test_material_data.py` liest die Originalbytes separat, rekonstruiert
Sektiongrenzen und GX-Dreiecksecken und vergleicht exakt:

- alle UV-Paare einschließlich begrenztem Padding,
- separate UV-Indizes jeder Dreiecksecke,
- Basis-Palette, Materialbild, Repeat-Override und lokale Auflösbarkeit,
- Bereiche und Zähler der texturierten/ungelösten Dreiecke.

Alle 142 Archive geprüft: 11.852 lokale Modelle, 7.631.616 UV-Paare,
1.156.057 primitiveweise Draw-Bereiche, 11.647.718 Dreiecke mit auflösbarer
Basistextur und 6.020 mit ungelöstem Zusatzverweis. Die Summe ist die unveränderte
M06-Weltgeometrie von 11.653.738 Dreiecken inklusive Degeneraten. Vier ältere
SMD-Blöcke bleiben wie zuvor ausdrücklich außerhalb der Geometrieunterstützung.

```sh
python3 tools/test_material_data.py .host-cmake/yz2_tool .host-cmake/material_tool /pfad/game
```

Sourcefreie Verträge prüfen unsigned/signed UV-Grenzwerte, UV-Nähte,
Textur-Deduplizierung, Repeat-Override, ungelöste Referenzen und defekte Paletten.
Der optionale EGL/GLES-Test aus SCENE.md rendert jetzt zusätzlich eine identische
untexturierte Referenzansicht und prüft sichtbare Pixeländerungen nach dem
Textur-Upload sowie GPU-Aufräumen beim Modellwechsel.

r300-Basis-Modell 29: 16 Basistexturen, 6.399 texturierte Dreiecke, null ungelöst.
Auf dem GLES-3.2-Mesa-Host änderten sich gegenüber der untexturierten Ansicht
16.107 Pixel; 13.087 lagen im sichtbaren Textur-Farbtest über der Prüfschwelle.
Das Bild wurde zusätzlich visuell geprüft. Dieser Nachweis ersetzt weder einen
Original-GPU-Bildvergleich noch den noch ausstehenden M07-Android-Gerätetest.
Testdaten, exportierte Meshes und Bilder werden nicht mitgeliefert.


## Erweiterung M08

M08 implementiert den separaten statischen Alphamaskenzweig einschließlich
Header-Grenzwert und SourceAlpha-Blending. I4/I8-Intensitätsalpha wird korrigiert.
Obige M07-Grenzen beschreiben den damaligen Stand; aktuelle Belege, weitere
Grenzen und Prüfungen stehen in ALPHA.md.
