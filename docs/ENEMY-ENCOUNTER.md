# Ganado-Treffertest: Originalteile und Adapter

Diese Datei beschreibt die M19-Trefferbasis. M20 ergänzt den frontalen Griff, Leons HP, Gegenwehr und die ausgewählte Rückkehrkette; Details in [GRAB-COMBAT.md](GRAB-COMBAT.md). Die unten genannten alten Stopp-/Angriffsgrenzen gelten für den M19-Treffertest ohne geladene Spieler-Griffkurven.

Quellbezug: unverändertes `upstream/re4`, Commit `9b76ff003081d6ece8b838453fb8310743a972c7`; bereitgestellte GC-Debug-Discs G4BE08, Revision 0. Die Referenzdateien bleiben unverändert und werden nicht als komplette Engine mitkompiliert.

## Daten und Auswahl

`Em12Set` bindet beim normalen Typ 0 Body ARC1bc, Kopf ARC1be, rechte Hand ARC1c0 und linke Hand ARC1c5 mit Palette ARC1bd. Das entspricht den DRS-Einträgen 440/442/444/449 und 441, weil der ARC-Zeiger vier Headerwörter enthält. Alle vier Modelle teilen die Originalhierarchie/Bindepositionen; es wird keine erfundene Kopftransformation ergänzt. Es gibt 34 Gelenke, 3.013 Dreiecke, 9.039 GPU-Ecken und drei Texturen. Diese Materialien aktivieren keinen Alpha-Test.

| Datei, Disc 1 | Größe | SHA-256 |
|---|---:|---|
| `em/em12.drs` | 5.664.736 | `6468f2d05d496b27d193ad79b3afeb20f40edcd0f54d3c2d16da7a3c03c16e7b` |
| `etc/emleon00.esl` | 8.160 | `65cd90d8979caab88476677aeec3bc33138c8542f106cbdc09290dc71f045126` |

ESL enthält exakt 255 Big-Endian-Datensätze zu 32 Byte. Eintrag FF ist der Ereignis-Sentinel und besitzt keinen Datensatz. EMI im Raumarchiv enthält andere Metadaten und ist keine Ersatz-Spawnliste. ESL-Rohdaten bleiben erhalten. Positionen sind vorzeichenbehaftete 16-Bit-Werte ×10, Winkel vorzeichenbehaftete 16-Bit-Werte ×π/16384.

Der Test aktiviert bewusst den sonst szenariogesteuerten Eintrag **0x25 / dezimal 37**, r100, ID12, Typ0, setNo1b, Flags24, Charakter1, HP1000, Position `(-83950,830,-32960)`. Originalscenarioaktivierung/Deathflags werden nicht vorgetäuscht. `EmSetFromList2` setzt `emset_no` auf den **Listenindex 37**, nicht auf `setNo1b`; deshalb nutzt der gewählte normale Gehzweig FCV-Eintrag4 und SEQ-Eintrag7. Größe für diesen Index: `1.03 + fRand1_1()*0.01`.

Leons Startposition `(-82910,860,-38480)` stammt aus `src/st1/r100.cpp::r100_Sce_look`. Das Original dreht dort auf 1.75 zum anderen geskripteten Ganado; dieser Test richtet Leon stattdessen auf den ausgewählten unbewaffneten Gegner. Beide Positionen bestehen die SAT-Kapselprüfung. Der direkte Laufweg und die EAT-Sichtlinie sind frei. SAT bestimmt Bodenkontakt; eine kleine Original-Y-Abweichung wird auf den geprüften Boden gesetzt.

## Animation

`enemy_preview` enthält die normale Körper-/Kopf-/Hand-Anbindung und fünf ausgewählte Originalclips: Stand ARC5, Gehen ARC8 mit SEQ-B-Variante, Kopfreaktion ARC2c/2d, Körperreaktion ARC26/27, stehender Tod ARC67/68. Der native Motion-/IK-Kern liefert Posen, Hokan, SEQ und Root-Delta. Platzierung übernimmt das Delta genau einmal; native Bewegungsflag1 bleibt dafür entfernt. Die 34 Einträge der Original-Spiegeltabelle werden im Besitzer gehalten. Die Körperfrontreaktion toggelt die bisherige Mirror-Eigenschaft, die ausgewählte Kopfreaktion verwendet die normale Seite.

Körperreaktion kehrt beim Original-SEQ-Free4 oder Ende zurück; Kopfreaktion wartet bis zum Clipende. Tod wird bis zum echten nicht schleifenden Ende ausgewertet und dann gehalten. Explizites `sample(maxFrame)` kann durch die historische Schleifensamplingregel den Endpunkt umschlagen; Laufzeittod benutzt deshalb die tatsächlichen nicht schleifenden Takte.

Die originale KI besitzt weitere Zufallsvarianten, Rückreaktionen, Waffen, Route/Griff/Leiter/Parasiten und gekoppelte Szenarien. Diese sind nicht portiert. Der Test setzt Leon direkt als Annäherungsziel und stoppt bei 1.800 Einheiten. Die Drehsmoothing-Konstanten 0.3 und 0.15707964 stammen aus `em10_R1_Walk`; Zielwahl und Stoppdistanz sind eigene Testregeln. Die SAT-Kapsel und ein zusätzlicher kontinuierlicher Akteurkontakt ersetzen nicht den vollständigen `cAtari`-Manager.

## Treffer und Schaden

`em10_R0_Init` liefert zehn aktive Kapseln und eine deaktivierte Zusatzkopfzone. Gelenknummern sind **einsbasiert**: Kopf partsNo5 benutzt Rohmatrix4; null würde die Modellmatrix benutzen. Treffermatrizen verwenden Rohpose × Modellplatzierung einschließlich Größe. Die inverse Bindepalette des Shaders ist dafür ungeeignet.

`hit_geometry.cpp` überträgt `emLineAtCk`, `emLineCapsuleCrossCk`, `emLineCubeCrossCk`/Flächen und `LineSphereCrossCk` mit float, ursprünglichen strikten Grenzen und der späteren Zone bei gleichem Abstand. Verwendet werden Originaloffsets, -radien und Achsenflags. Nicht endliche Originaldegenerationen werden verworfen. Normaler Typ0 ist unterstützt; andere Größen-/Rüstungszweige werden nicht durch den einzigen Test freigeschaltet.

Der Handgun-Strahl stammt nach dem Start der Rückstoßpose aus dem rechten Handgelenk: Mündung `(234.5,-24,38.33)`, Richtungsvektor `(-50000, fRand1_1()*200, fRand1_1()*200)` in diesem Gelenk, anschließend in die Welt transformiert. Ein Schussereignis wird nur beim tatsächlichen Munitionsverbrauch erzeugt. Die Ziellinie verwendet denselben Strahl ohne Streuung und verursacht keine Treffer.

Die Ziellinie erscheint nur im Aim-Zustand: `wep02_r2_set` erneuert die originale Laseranforderung, `wep02_r2_fire` tut das nicht. Deshalb wird der stark nach oben schwingende Roh-Handstrahl einer späteren Rückstoßpose nicht als weiterer Zielstrahl gezeichnet. Die echte Schussberechnung bleibt auf dem ersten gelösten Fire-Frame.

Die eigene Zielkamera behält die M18-Ausrichtung nach der tatsächlich gelösten Handrichtung. Ihr Schulteroffset beträgt jetzt900 Einheiten statt450: Bei SAT-verkürztem Blick verdeckte Leon sonst fast den ganzen Gegner. Das volle neue Offset wird gegen SAT gesweept. Die Bildprüfung misst sichtbare Originalgegnergeometrie ohne die rote Ziellinie; keine Sichtbarkeit durch Wände wird erzwungen.

`GetWepTargetList2` begrenzt den Trefferbereich mit **EatMgr**, Flag0, Maske400000. EAT wird separat als `SatQueryMode::Effect` geladen. Die bisherigen SAT-SEck0-Regeln schließen bestimmte Attribute zusätzlich aus und sind dafür falsch; EAT-SEck1/Flag0 verwirft nur Maskenattribute. Eine Wand muss vor der getroffenen Kapsel liegen, um den Schuss zu blockieren. Originalpriorität2 für die FN57 kann mehrere Gegner treffen; im Ein-Gegner-Test existiert nur ein Kandidat.

Die Dreieckabfrage/BVH verwendet weiterhin den eigenen nativen `SatCollision`-Adapter mit double und getesteten Grenzen; sie ist kein bitgenauer Port von `At_poly_line_ck`. Portiert sind die Originaltrefferprimitiven und EAT-Ausschlussregeln, geladen die Original-EAT-Daten. Dynamische Szenario-Kollisionsobjekte fehlen.

`Em10DmSetWep02`/`em10SetDmVal`: Klasse0/Typ0, Waffenindex1, Leistungsstufe0 → `int(150*0.9)=135`; alle normalen Körper-/Arm-/Beinzonen behalten diesen Wert. Kopf multipliziert mit1.2 →162; `Rnd()%12==6` multipliziert stattdessen mit10 →1350. `LifeDownSet2` erhält neutralen Game_level5, verbraucht auch bei rnd0 zwei RNG-Bytes und begrenzt Schaden auf verbleibende HP. HP wird aus ESL übernommen, nicht erfunden.

Taktreihenfolge aus `game.cpp`: Gegner vor Spieler. Der Spielerschuss merkt einen Treffer vor; nächster Gegnertakt verarbeitet ihn und setzt den normalen Schadens-Timer auf1; folgender Gegnertakt macht den Akteur wieder treffbar. Es gibt weder zehn Takte pauschale Unverwundbarkeit noch eine Sperre für die ganze Trefferanimation. Fortlaufende Schussnummern verhindern doppelte Verarbeitung derselben Eingabeflanke.

Der Test besitzt einen eigenen stabilen Seed4e19 und verwendet die originale Byte-RNG-Rekurrenz, einschließlich Streuung/Kopfkritik/Schadenshelper. Das ist **nicht die Zufallsfolge der vollständigen Kampagne**: weitere Originalsysteme und zufällige Reaktions-/Standvarianten fehlen. Vorbereitung/Fehlerrollback verändern den bestehenden Gegner-RNG nicht. Pause, Inspektor und Kontextwiederherstellung erzeugen keine Simulationstakte.

## Noch offen

Der frontale Originalgriff mit Spielerzustand, Schaden und Befreiung ist seit M20 eingebunden. Offen bleiben weitere Gegnerangriffe, Gegenstände/Beute, Parasiten/Kopfverlust, vollständige Schadens-/Rückreaktionen, Szenario-KI, dynamische Raumobjekte, Effekte, Audio und Spielstände. Der kritische Kopftod verwendet weiterhin eine stehende Ganzkörper-Todeskurve; die originale Kopfverlustkette ist nicht behauptet.
