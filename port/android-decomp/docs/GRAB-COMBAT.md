# Frontgriff des einzelnen EM12-Ganado

Primärquelle: unverändertes `upstream/re4`, Commit`9b76ff003081d6ece8b838453fb8310743a972c7`. Begrenzt auf normalen Leon, villageTyp0, Rang5, ohne Rüstung, Nicht-Hard-Modus und den originalen r100-ESL-Eintrag37 mit Flags0x24. Die vollständigen Originalmanager werden nicht als fertig portiert bezeichnet.

## Daten und gekoppelte Bewegungen

| Funktion | Originalquelle | DRS-FCV / SEQ | Hokan / Flags |
|---|---|---:|---:|
| Frontaler Griffversuch | `em10_R1_Catch` | EM12 130 /131 | 10 /1 |
| Ganado hält Leon | `em10_R1_NeckHang` | EM12 651 /ohne | 5 /1 |
| Leon im Griff | `plem10_NeckHang` | EM12 659 /ohne | 5 /1 |
| Gegenwehr fehlgeschlagen, Ganado | NeckHang Phase2 | EM12 652 /653 | 5 /1 |
| Gegenwehr fehlgeschlagen, Leon | Player Phase2 | EM12 660 /ohne | 5 /1 |
| Ganado abwerfen | Enemy Phase4 | EM12 654 /655 | 5 /1 |
| Leon wehrt ab | Player Phase4 | EM12 661 /ohne | 5 /1 |
| Leon steht nach Fehlversuch auf | `damageNormal` Phase10 | PL00 75 /76 | 3 /5 |
| Ganado steht nach Abwurf auf | `em10_R1_DownWake` | EM12 105 /106 | 15 /1 |

ARC-Indizes enthalten vier Headerwörter; die Tabelle nennt bereits DRS-Einträge. Die Spieler-Griffkurven liegen im Gegnerarchiv. Der Kommentar beiPlayerARC299 im Upstream ist irreführend: ausgeführter Code wählt ihn bei erfolgreicher Gegenwehr; ARC298 ist der Fehlversuch. Toter Leon bleibt am Ende der Fehlversuchsbewegung. Der Ganado hält seine tatsächliche letzte Abwurfpose am Boden, ohne erfundene Down-FCV. Überlebende warten original`Rnd()%60+60` Takte und spielen dann die Aufstehkurve.

Offene Hände sind EM12-BIN445/450, normale Hände444/449; Body440 und Kopf442 bleiben erhalten. Ein zusätzlicher Satz Handvertices wird einmal in denselben Gegner-VBO geladen. Drawgrenzen behalten ihren BIN-Schlüssel, damit nie beide Handvarianten gleichzeitig gezeichnet werden. Keine neuen Texturen und kein zusätzlicher GPU-Akteur.

## Fangprüfung und Platzierung

Der Test aktiviert nur den frontalen Versuch bei Abstand<=1100 und passendem Winkel. Originale Routewahl, schnellere Angriffsdistanz bei laufendem Leon, Seiten-/Rückvarianten und Gruppendruck fehlen. Die Fangprüfung benutzt vor der Bewegung das Original-`Seq_old.Free&2`, Lebens-/Schadenszustand und die inverse Gegnerplatzierung: X strikt(-400,400), Y[-500,500], Z[0,900]. EAT prüft die Verbindung aufY+500 und den originalen+x300-Seitenstrahl zweimal; das gebündelte Original enthält tatsächlich diese doppelte identische Prüfung.

`EmCatchPLSet` richtet beide Figuren gegeneinander aus. Offset(-180,0,470.18) wird mit **Leons endgültiger Zielrichtung** transformiert. Typ2 verteilt die Korrektur zur Hälfte auf beide Akteure. Pro Takt gehen20% der XZ-Positionskorrektur und30% des restlichen Winkels ein. Die Root-Translation verwendet den temporären vollständigen Zielwinkel; gekoppelte Root-Yaw wird danach wie im Original verworfen. Rootbewegung passiert genau einmal.

Während des Griffs ist die Gegnergröße1. Der nachfolgende Abwurf gibt sie nach55Takten wieder zur originalen Größe frei; Rückkehr glättet mit0.9/0.1. Der getrennte aufrechte Akteurblocker wird für das Paar ausgesetzt, während Raum-SAT weiterhin geprüft wird. Das ist kein vollständiger `cAtari`-Port. Originalpartnerdelta überträgt erst nach dem Bewegungsstempel entstandene Fremd-/Kollisionskorrekturen, nicht den Weg des vorherigen Frames. Bei sofortiger SAT-Auflösung dieses Adapters entsteht kein zusätzlicher Partnerweg.

## Schaden, Gegenwehr und Takt

Originales `PlayerLifeReset` setzt Leon auf1200. Spawnflags0x24 enthalten Flag4: der normale Basisschaden10 wird außerhalb Hard zu10/2+1=6 pro Grifftakt. Rang5 skaliert mit1. Nach35Takten entscheidet die Gegenwehr: mehr als9 Punkte bedeutet Abwehr. StartHP<=199 zusammen mitHP<=1 erzwingt den Fehlzweig; dieser setztHP1 auf0. Ohne Abwehr kommt inPlayerFrame37 zusätzlicher Basisschaden180 hinzu. Ein erster kompletter Fehlgriff ergibt1200→990→810; der Originalablauf kann nach weiteren Fehlversuchen tödlich sein.

`LifeDownSet2` verbraucht zwei RNG-Bytes auch beirnd0. Spieler-PreHP>300 zieht zusätzlich die originale1/4-Überlebensregel; keep-one bewahrt gegebenenfalls1HP. Der Abwurf verursacht100 Gegner-HP; Gegenwehr>30 erzeugt9999, >20 zieht zusätzlich seine unabhängige Zufallsregel, auch wenn>30 bereits kritisch ist. Kopfverlust/Parasiten/Effekte dieses kritischen Originalpfads sind noch nicht portiert; die Ganzkörper-Abwurfpose bleibt sichtbar.

`PlGachaMove` zählt drei Triggergruppen: Richtungen, A/B, und horizontale Richtungen erneut. Damit zählen echte LS-/D-Pad-Links-/Rechts-Flanken2, Hoch-/Runter1, A/Kreuz/B/Kreis zusammen1. Der rechte Stick und Waffen-Trigger zählen nicht. Eingabeflanken werden bis zum nächsten Simulationstakt gesammelt und genau einmal verbraucht.

Simulation läuft30Hz, unabhängig vom Renderintervall. Gegner laufen vor Leon; echte Schüsse werden nach der Spielerpose vorgemerkt und im nächsten Gegnertakt verarbeitet. Die Griffentscheidung ändert zunächst nur die Phase; beide Release-Animationen beginnen im folgenden Takt. Pause, Inspector und Hintergrund löschen Restzeit sowie ausstehende Eingabeflanken und behalten HP, Phase, Pose und RNG.

Ein neuer Ganado-Test baut einen neuen Leon mit den zusätzlichen Clips und1200HP vor. Testmunition wird übernommen. Raum-/GPU-Fehler tauschen den vorherigen vollständigen Akteurszustand zurück. Normale Türwechsel bewahren HP und Munition; es gibt noch keine Kampagnen-Spielstände oder Original-Game-over-Routine.
