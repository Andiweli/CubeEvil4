# SAT — originale statische Kollisionsdaten, M14

Quellgrundlage: unveränderter Upstream-Stand 9b76ff003081d6ece8b838453fb8310743a972c7, `include/atari.h`, `include/at_sub.h`, `src/game/atari.cpp`, `src/game/game.cpp`. Der Raumstart liest dort `GetDataExt(pRoom,"SAT",0)` und erstellt Typ/Variante 0 mit Position und Rotation null. Genau diese erste SAT-Ressource und Variante verwendet die Android-Prüfansicht.

## Format und Grenzen

Alle Zahlen sind Big Endian. Ein direkter SAT-Dateikopf umfasst 20 Bytes, Version 0x20 (beide Discs) beziehungsweise den im Original vorgesehenen direkten Laufzeitkopf 0xff. U16-Zähler beschreiben Vertices, Normalen, Kanten, Polygone und räumliche Blöcke; ein weiterer U16-Wert bleibt uninterpretiert. Drei Gruppen zählen Boden-, Neigungs- und Wandpolygone in dieser Reihenfolge. Ihre Summe muss der Polygonzahl entsprechen; das originale Polygonlimit beträgt 0x1fff.

Bei einem äußeren Kopf mit gesetztem Bit 0x80 und Version ungleich 0xff wird der erste U32-Verweis ab Byte 4 gewählt. Aus dessen Tabellenende wird die Anzahl der Offsetplätze begrenzt abgeleitet; die Kopf-Paddingbytes werden nicht als dokumentierter Zähler ausgegeben. Jeder Verweis wird geprüft. Die nächste höhere Teil-Dateiadresse begrenzt Variante 0. Weitere Varianten werden nicht dekodiert.

An den Dateikopf schließen sich drei Arrays aus F32-Vektoren (je 12 Bytes) an: Positionen, Normalen, Kanten. Ein Polygon umfasst 20 Bytes: drei U16-Vertexindizes, einen Normalenindex, drei Kantenwerte, ein erhaltenes U16-Feld und ein originales U32-Attributwort. Die unteren 13 Bits der Kantenwerte sind Indizes; obere Bits bleiben erhalten. Alle drei Vectorarrays müssen endliche Werte enthalten. Attribute und Kanten werden gelesen, aber noch nicht für Kollisionsantworten ausgewertet.

Räumliche Blöcke besitzen 36 Bytes Kopf. X/Z-Grenzen und Ausdehnungen werden gelesen; die im Original ungenutzten Y-Felder nicht als Koordinaten interpretiert. Bit 0 des U16-Flags kennzeichnet Kinder unmittelbar hinter dem Kopf. Blätter enthalten Boden-/Neigungs-/Wand-Anzahlen und entsprechend viele U16-Polygonindizes. Der U32-Nächste-Verweis ist relativ zum jeweiligen Block. Der Parser prüft Teil-Dateigrenzen, positive Vorwärtsverweise, wiederholte Knoten, überlappende Blockbereiche, Polygonindizes und die deklarierte Gesamtzahl der Knoten. Die Traversierung erfolgt iterativ mit einem durch den U16-Zähler begrenzten Budget.

## Darstellung und Umfang

SAT-Dreiecke werden mit ihren originalen Koordinaten und referenzierten Normalen an den vorhandenen GLES3-Renderer übergeben. Grün (60/210/90) zeigt Boden, Gelb (240/190/50) Neigungen und Blau (70/130/240) Wände. Das sind ausdrücklich Diagnosefarben, keine Originalmaterialien. Die Kamera normalisiert die Ansicht wie bei der Modellvorschau; beim Umschalten beginnt Orbit/Abstand neu. Kein Overlay und keine Texturen in dieser separaten Ansicht.

Die obere Controller-Taste schaltet Modell/SAT um. SELECT behält die Normalen-Prüfansicht bei; rechter Stick und LT/RT steuern die Kamera. LB/RB/Start wirken ausschließlich in der Modellansicht. Das gewählte Basis-Modell bleibt beim Zurückschalten erhalten. Bei neu erstelltem GL-Kontext startet die Modellansicht. Fehlende oder leere SAT-Geometrie wird unter `RE4Collision` protokolliert und löst keine Umschaltung aus. SAT-Metadaten erscheinen auch im Raum-Ladebericht.

Nicht enthalten: Bodenhöhe-/Sweep-Abfragen, Spielerkapsel, Wandgleiten, Kollisionsantwort, dynamische Objekte/EAT, weitere SAT-Varianten, Gameplay oder originale Kameraführung. Ein sichtbares Kollisionsmesh ist noch keine funktionierende Spielerphysik.

## Reproduzierbare Prüfung

`tools/test_sat_data.py yz2_tool sat_tool GAME_ROOT` prüft die SHA-256-Referenzen aller 142 Raumarchive, liest die SAT-Tabellen unabhängig in Python und vergleicht sämtliche Polygonindizes, erhaltene Felder/Attribute sowie Float-Bitmuster der referenzierten Positionen und Normalen über einen deterministischen FNV-1a-Prüfwert. Blockzahl und sämtliche Blattverweise werden unabhängig gezählt/geprüft. Der C++-Parser prüft zusätzlich die Bereichs-/Überlappungsregeln. Erfasst sind alle 152 SAT-Vorkommen mit jeweils Variante 0, insgesamt 95.310 Polygone, 53.268 Blöcke und 417.445 Blattverweise; diese Zahlen sind keine Aussage über alle weiteren Varianten.

`tests/sat_tests.cpp` prüft synthetische Geometrie, Gruppenfarben, Attribute, Kantenflags, alle Kürzungen, Indexgrenzen, NaN, Blockbudget, Kinder-/Nächste-Verweise und Teil-Dateigrenzen. `tools/test_scene_gl.cpp` ergänzt echte SAT-Framebufferprüfungen für r300/r100, Farbdeckung, GL-Fehler und exakte Rückkehr nach der Normalenansicht. Originaldaten werden mit dem Patch nicht verteilt.
