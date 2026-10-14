# M18 — SAT-Bewegung, Root-Motion, Waffenaktionen und Folgekamera

Die native SAT-Kapsel aus M15 löst seit M17 die originalen Root-Bewegungsdeltas des animierten Leon-Modells gegen die Raumgeometrie auf. Sie bleibt als eigene Kollisionsantwort erhalten; die sieben Originalclips liefern Gehstrecke und Figurendrehung. M18 ergänzt stationäre Waffenaktionen mit originalem WEP01-Modell und getrenntem Vier-Gelenk-Skelett. Details und Datenzuordnung stehen in [PLAYER-PREVIEW.md](PLAYER-PREVIEW.md) und [WEAPON-ACTIONS.md](WEAPON-ACTIONS.md). Bei fehlenden Disc-1-Spielerassets bleibt die frühere Kapseldiagnose als Rückfall verfügbar. In der SAT-Ansicht bleibt die Kapsel zur Kollisionsprüfung sichtbar. Die unterstützte starre Gesamtgeometrie wird mit den bisher belegten Originaltexturen, Masken, Mipmaps und SMX-Materialfarben dargestellt. Eine gelbe Fußmarke folgt der Ebene des getroffenen Bodenpolygons. Wand-/Randkontakt färbt die Kapsel orange.

## Originalgrundlage und Abgrenzung

Quellstand: 9b76ff003081d6ece8b838453fb8310743a972c7. `src/game/atari.cpp`, `cSatMgr::getFloor` (Zeilen um 399), castet von pos.y + above_limit zu pos.y - below_limit; `blkPolyLineCkCore` wählt Boden/Neigung oder Wand und behält den nächsten Treffer. Damit ist positive Y die Höhenrichtung. `src/game/at_sub.cpp`, `At_poly_line_ck`, `At_poly_sphere_ck2` und `Get_poly_attr`, belegen die SAT_TYPE-/SAT_ATTR-Filter und das Hit-Bit 0x01000000. Definitionen: `include/at_sub.h`. `src/game/player.cpp` initialisiert Radius/halbe Höhe der originalen Spieler-Kollisionsinfo; M15 übernimmt diese Runtime nicht.

Die nativen Abfragen verwenden erhaltene Originalpolygone, Attribute und Normalen. Die Filter für PL, EM, ROUTE, SMALL, SEE und ONLY_SEE_HIT sowie eine Ausschlussmaske folgen dem belegten SEck=0-Zweig. Für die Bewegungskapsel gilt PL (0x1000); für die Kamerakugel SEE (0x8000). Material-/Effektattribute werden nicht als Gameplay ausgeführt.

Die geometrische Linienprüfung ist eine robuste, beidseitige Segment-/Dreieckprüfung mit baryzentrischen Koordinaten und Double-Zwischenwerten, keine bitidentische Portierung der originalen F32-Kantenroutine. Degenerierte und parallele Fälle liefern keinen Treffer, das Segmentende t=1 wird wie im Original nicht als Treffer angenommen. Treffer enthalten Originalpolygon, Attributwort einschließlich Hit-Bit, normalisierte Originalnormale, Weltpunkt und Segmentanteil.

Die Kapselantwort, Wandgleitregel, Grenzwerte und Kamera sind eigene Funktionen. Sie sind mit Originalanimationen, begrenzten Waffenaktionen und einer originaldatenbasierten Raum-/Türnavigation verbunden. Die vollständige originale `cPlayer`-/`cSatMgr`-Runtime ist damit nicht portiert: Einseitige Kugelantwort, Kanten-/Vertex-Cancel-Bits, dynamische SAT/EAT-Objekte, weitere SAT-Varianten, Schwerkraft, Sprünge, Leitern, Szenariologik und komplette Raum-/TEV-Sichtbarkeit fehlen weiterhin. Die Animation verwendet ihre ursprüngliche Kurven-IK; eine Fußanpassung an den realen SAT-Boden ist noch nicht verbunden. Die Waffenaktionen verursachen noch keinen Schaden und ersetzen keine Gegner- oder Inventarruntime.

## Abfragen und Suchstruktur

`SatCollision` hält einen unveränderlichen Verweis auf SAT[0]/Variante 0 und baut eine eigene AABB-BVH über die Dreiecke. Median-Aufteilung und Blattbudget 8 halten die Rekursion logarithmisch. Die Original-SAT-Blockverweise bleiben durch den SAT-Parser geprüft, werden aber nicht als diese neue Suchstruktur ausgegeben. BVH-Abfragen wählen Kandidaten; diese werden für gleiche Distanzen nach Originalpolygonindex sortiert. Der erste nächste Treffer ist dadurch deterministisch.

- `line`: nächster geometrischer Treffer, optional Boden-/Wand-Untermenge, Originalfilter und Ausschlussmaske.
- `ground`: senkrechte Linie mit begrenztem Bereich; Boden- und Neigungsgruppen werden gemeinsam geprüft.
- `clearCapsule`: Abstand der aufrechten Kapselachse zu Wanddreiecken, einschließlich Flächen, Kanten und Ecken.
- `sweepCapsule`: kontinuierliche Translation über denselben Abstand. Konservative Annäherung nutzt die Weglänge als obere Geschwindigkeitsgrenze. Ein Treffer stoppt mit 0,1 Einheiten Toleranz; nach höchstens 48 Schritten wird konservativ geblockt. Das verhindert einen unbeschränkt iterierenden Solver und kann in Grenzfällen früher stoppen. Wände werden beidseitig behandelt.

Die Abstandsprüfung berücksichtigt den Schnitt zwischen Achssegment und Dreiecksfläche, beide Achsenden gegen die Fläche sowie Achse gegen alle drei Kanten. Kapseln mit Höhe gleich Durchmesser sind ebenfalls zulässig. Ungültige oder nicht endliche Eingaben werden begrenzt/abgewiesen; Nullbewegung erzeugt keinen Sweep.

## Bewegung

Die eigene Kollisionskapsel behält Radius 300, Höhe 1600, Schrittaufstieg höchstens 250, Bodensnap nach unten höchstens 600 und begehbare Normale Y mindestens 0,5, jeweils in Original-Welteinheiten. Das sind begrenzte Adapterwerte; sie bilden nicht die komplette originale Spieler-Kollisionsinfo nach.

Mit Leon steuert der linke Stick beziehungsweise das D-Pad vorwärts/rückwärts und dreht die Figur. Beim Gehen verändert der rechte Stick ihre Bewegungsrichtung nicht direkt. Pro festem 30-Hz-Schritt liefert das originale `MotionGetSpeed` eine Root-Translation und Y-Drehung aus FCV/SEQ. Die Translation wird mit der bisherigen Figurendrehung in Weltkoordinaten übertragen und genau einmal durch `WalkProbe::moveDelta` gelöst; anschließend übernimmt Leon die Root-Drehung und beim Gehen die originale Steuerdrehung. Analoger Ausschlag skaliert den Sequenz-/Bewegungstakt. Die beabsichtigte Eingabe wählt den Clip auch bei Wandkontakt; eine abgewiesene Bewegung startet die Animation nicht fortlaufend neu. Standdrehung, Rückwärtsgehen und Schnellwende haben jeweils eigene Originalclips. Gehen↔Laufen erhält die Sequenzphase; die direkte Phasenabbildung und die vollständig ausgespielte 23-Frame-Schnellwende sind begrenzte Abweichungen vom kompletten originalen Zustandsablauf.

Anlegen, Zielen, Schussanimation, Nachladen und Ablegen halten die Kapselposition fest. Bewegungsinput setzt während dieser Aktionen keine Root-Translation um. Der rechte Stick steuert dabei die originale Drei-Kurven-Zielpose und die begrenzte Hüft-/Figurendrehung; anschließend übernimmt die Followkamera wieder die Richtungsdifferenz. LT/L2 loslassen führt nach der nötigen Aktionsphase zurück zur Bewegung. Die zusätzliche Munition von 10+60 ist ein Diagnosezustand mit belegten Nachladezeiten, kein portiertes Spielinventar.

Physisch rechts bedeutet negative Y-Drehung und verwendet das WEP01-Paar 9/10; physisch links bedeutet positive Y-Drehung mit 11/12. Die Quelle `pad.cpp` verwendet `-atan2(x,y)` und ordnet positives Stick-X dem irreführend benannten `JOY_SLEFT` zu, das über Key-Bit 4 eine negative Drehung auslöst. Die Android-Eingabe bildet diese tatsächliche Übersetzung nach; der originale native Motion-Kern wird nicht verändert. Eine Kamera hinter der +Z-Figur sieht −X als rechts, passend zu dieser Drehrichtung. Die vollständige Tabelle der sieben Clips steht in PLAYER-PREVIEW.md.

Eine Bewegung wird in Teilstücke von höchstens einem Viertelradius zerlegt, gegen Wände gesweept und bis zu viermal auf deren XZ-Tangente projiziert. Jeder akzeptierte Teilweg benötigt einen erreichbaren, hinreichend flachen Boden. Auch der anschließende vertikale Bodensnap wird gegen Wände gesweept. Die vorhandenen Bewegungsclips haben keine vertikale Root-Strecke; ausschließlich die SAT-Auflösung liefert Y. Fehlender Boden, große Stufe oder tiefer Abgrund blockieren die Figur; es gibt keine Fallsimulation.

Bei fehlendem Leon bleibt `WalkProbe::step` als frühere kameraorientierte Kapseldiagnose verfügbar: 2.600 Einheiten/s beziehungsweise 5.200 mit gehaltenem B/Kreis, mit auf Länge 1 begrenzter Diagonalbewegung. Diese Geschwindigkeiten gelten nicht für den animierten Leon; dessen Originalkurven ergeben bei vollem Ausschlag ungefähr 1.741 beim Gehen, 4.192 beim Laufen und 1.969 Einheiten/s rückwärts.

Pro Renderframe werden höchstens 50 ms Zeit übernommen. Unter sehr niedriger Bildrate läuft der Prototyp deshalb langsamer. Start pausiert Bewegung und Animation. Input-Reset und Controllerwechsel setzen eine Generation; der erste folgende Bewegungsschritt hat Zeit null. Pause, Inputwechsel und ein laufender Raumtransfer löschen zudem den Animationszeitrest und vorgemerkte Schnellwende-/Schuss-/Nachlade-Eingaben. Nach Suspend müssen LT/L2 und RT/R2 beide neutral werden, bevor neue Waffenaktionen angenommen werden. Eine neue GL-Kontextinstanz stellt GPU-Ressourcen wieder her und erhält den aktuellen Raum, die Figur, die Kamera und einen vorgemerkten Raumtransfer. Bildschirmrotation übernimmt die gespeicherte Sitzungskennung innerhalb desselben Prozesses. Ein neuer Aufruf oder Prozessneustart beginnt erneut am gewählten Raum; Positionen und Testmunition werden nicht dauerhaft gespeichert.

Die Bewegungsansicht startet automatisch. Vorrang hat die Originalposition mit Richtung aus `Debug/roominfo.dat`. `WalkProbe::place` akzeptiert sie nur bei einem passenden Boden höchstens 250 Einheiten über/unter der gelieferten Position, ausreichender Bodennormale und freier Kapsel. Eine abgewiesene Platzierung verändert die bisherige Position nicht. Auch Türziele verwenden diese Prüfung.

Ist der Originalstart nicht nutzbar, wird ein gekennzeichneter SAT-Teststart aus nach Fläche sortierten Boden-Dreiecksschwerpunkten gewählt: gültiger Boden, freie Kapsel, bevorzugt Platz für den anfänglichen Kamerablick. Falls kein solcher Blick frei ist, wird der erste freie Kapselpunkt verwendet. Fehlt auch dieser, bleibt die Modellansicht mit Statusmeldung. Die frühere vollständige SAT-Diagnose fand für 141 von 142 SAT-Raumarchiven solche Testpunkte. Das belegt keine 141 spielbaren Räume und keinen durchgehenden Weg durch Türen, Leitern oder Szenariohindernisse.

## Darstellung und Kamera

Bei aktiver Bewegung wird die gesamte unterstützte starre SMD-Geometrie des Raums verwendet. Die vorherige Basis-Modellwahl bleibt für die Prüfansicht erhalten.

| Eingabe | M18-Raumansicht |
| --- | --- |
| Linker Stick / D-Pad | Vorwärts/rückwärts und Figur drehen |
| B/Kreis gehalten | Vorwärts laufen |
| Rückwärts + neue B/Kreis-Flanke | Einmalige Schnellwende |
| A/Kreuz | Unterstützte Türaktion bei abgelegter Waffe |
| Start | Bewegung und Animation pausieren/fortsetzen |
| LT/L2 halten | Stationär Waffe anlegen und zielen |
| LT/L2 loslassen | Waffe ablegen und zur Bewegung zurückkehren |
| RT/R2 mit gehaltenem LT/L2 | Schussanimation mit Testmunition |
| X/Quadrat mit gehaltenem LT/L2 | Nachladen |
| Schaltfläche „Modellprüfung / Leon-Steuerung“ | Zwischen Raumsteuerung und Prüfansicht wechseln |
| Y/Dreieck | Textur-/SAT-Ansicht umschalten |
| Rechter Stick | Beim Gehen Kameraorbit; mit angelegter Waffe zielen |
| LT/RT ausschließlich in der Modellprüfung | Kameraabstand verändern |

Die Türnavigation prüft Original-AEV-Bereiche und Ziele; sie ersetzt noch nicht die vollständigen Türcallbacks, Storybedingungen und Szenarioabläufe. Fehlende oder abgewiesene Ziele lassen den bisherigen Raum erhalten.

Die Kamera richtet sich auf die halbe Kapselhöhe und folgt der Weltposition. Ihr Startorbit wird an Leons Originalrichtung ausgerichtet. Beim Gehen dreht/kippt der rechte Stick den Orbit unabhängig; bei angelegter Waffe steuert er stattdessen die Zielpose. Leons Richtungsänderung wird zusätzlich als Differenz auf den Orbit übertragen. Die Trigger ändern den Kameraabstand ausschließlich in der Modellprüfung. Die seit M16 verwendete Kugel mit Radius 120 wird kontinuierlich gegen Wände, Schrägen und Böden geprüft. Der SEE-Attributfilter bleibt original belegt; eine kleine Reserve vor dem Treffer schützt den Nahbereich. Es wird kein Mindestabstand durch Hindernisse erzwungen. Bei Abstand unter 1.000 werden Leon und sein Waffenmodell ausgeblendet. Dies bleibt eine eigene Folgekamera, keine vollständige originale `cCamera`-Runtime. Die Drahtkapsel wird als Debugmarker auch durch Geometrie sichtbar gezeichnet. Die Fußmarke wird mit Tiefentest leicht über der Bodenebene gerendert. Normalenmodus verändert die Raumdarstellung, nicht die Markerfarbe.

Marker/Camera verwenden dieselbe Welt-Normalisierung wie die jeweils geladene Szene. Ein eigener dynamischer VBO aktualisiert nur die kleine Kapsel/Fußmarke; der Raum und seine Texturen werden nicht pro Bewegung neu hochgeladen. Context-Loss verwirft auch diese GL-Namen, ohne fremde Kontextobjekte zu löschen.

## Prüfbarkeit

`tests/collision_tests.cpp`: analytische Boden-/Wandtreffer, Null/parallel, kontinuierlicher Kapseltreffer, dünne Wand bei großer Bewegung, Berührung/Retreat, Wandgleiten, Randbegrenzung, gleiche Wege bei 30/60 Hz, Originalausschlüsse, NaN, degenerierte Geometrie und fehlender Spawn.

`tests/player_motion_tests.cpp`: assetfreie Prüfung von Originalplatzierung mit erlaubtem Bodensnap, abgewiesener Platzierung ohne Positionsänderung sowie Root-Kapselblockierung und Rückzug. Mit `PL00.drs WEP01.drs` werden Originalstrecken und Schleifen, 30/60-Hz-Gleichheit von Position/Pose, analoger Takt, Schnellwende-Flanken, Pause/Suspend und Bewegung gegen beziehungsweise weg von einer Wand geprüft. `tests/animation_tests.cpp` ergänzt synthetische Root-/Sequenzverträge; die unabhängige M16-Poseprüfung bleibt auf ihre drei ursprünglichen Clips begrenzt.

`tests/player_action_tests.cpp PL00.drs WEP01.drs`: stationäre Kapsel trotz Bewegungsinput beim Zielen/Nachladen, originale Richtung der Zielhand, endliche Posen und Hüftgrenzen, 30/60-Hz-Gleichheit, 14-Takt-Folgeschüsse, nicht vorzeitig abgebrochener Rückstoß, Zeit-null-Tastenkanten, Suspend-Freigabe, einmaliges Nachladen bei Waffenframe 31→32 und Munitionsbilanz bis 70 verbrauchten Testpatronen. Ein Raumwechsel setzt die Bewegungspose neu, erhält aber die Testmunition.

`tools/test_collision_data.py yz2_tool collision_tool GAME_ROOT`: SHA-256-belegte gepackte/entpackte Archive, unabhängige Python-Referenz für nächste Linienhits und vier Sekunden Testbewegung je freiem Spawn. Ergebnis: 142 Räume, 4.750 Linienabfragen stimmen überein; 141 Startpunkte und 141 am Ende gültig geerdete Bewegungsspuren. Dies ersetzt keinen Gerätetest oder vollständigen Nachweis originaler Spielphysik.

`tools/test_scene_gl.cpp`: r300/r100 mit SAT sowie kompletter unterstützter starrer Texturgeometrie und Followkamera. Marker sichtbar, kein GL-Fehler, exakte Rückkehr nach dem Normalenmodus. Alle früheren Textur-/Masken-/Mip-/Common-/SMX-Regressionen bleiben enthalten.

M16-Leon und seine Animationen wurden nach einem sauberen Neubau auf dem Gerät vom Nutzer bestätigt; `Eingefügter Text(20261003-221022).txt` belegt den geladenen Spieler mit 119 Gelenken und Originalanimationen. Das anschließende M17-Log `Eingefügter Text(20261003-225513).txt` enthält Root-Motion-Zustände und erfolgreiche Transfers r301→r300→r301; der Nutzer bestätigt diesen Türlauf. Diese Rückmeldung beweist keine vollständige Spielphysik oder fehlerfreie Sichtbarkeit aller Materialien. Die neuen M18-Waffenaktionen benötigen ihren nächsten Gerätetest.
