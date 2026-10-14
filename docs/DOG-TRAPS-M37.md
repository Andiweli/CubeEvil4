# M37 – Originalhund und Fallen in r100

Referenz sind `upstream/re4/src/em21/em21.cpp`, `em2a/em2a.cpp`, `st1/r100.cpp` sowie die tatsächlichen Disc-1-Dateien `etc/emleon00.esl`, `em/em21.drs`, `em/em2a.drs`, `etc/core.das` und `r100.dat`. EM21 ist der Hund; EM2B ist El Gigante. Die DRS-Tabelle beginnt bei ARC4, deshalb gilt für die Quellcode-ARC-Nummern `DRS-Index = ARC - 4`.

`DogTraps` ist der native Eigentümer der acht originalen ESL-Einträge. Jeder Akteur besitzt sein echtes BIN/TPL, seine originalen Körperteile und einen stabilen GPU-Eigentümer. Es gibt keine Ersatzmodelle oder frei gewählten Fallenpositionen.

| ESL-Index | Originalakteur | Aktivierung |
|---|---|---|
| `9` | EM21 type1/set1, Hund in der Falle | `r100_trap_set()` nach dem Lastwagenereignis/RSF10 |
| `0x0A`, `0x0B` | EM2A type2, Sprengdrähte mit hp2160/hp1860 | gleicher Originalaufruf |
| `0x0C` | EM2A type0/set1, Falle am Hinterbein des Hundes | gleicher Originalaufruf |
| `0x0D`, `0x0E`, `0x1D`, `0x7A` | EM2A type0, normale Bärenfallen | gleicher Originalaufruf |

Vor diesem Aufruf sind diese ESL-Einträge ausdrücklich unsichtbar. Ein allgemeines „alive“-Flag erzeugt sie nicht verfrüht.

## Hund und Leon

Der Hund wartet mit FCV ARC18 und dem originalen 75-bis-104-Tick-Timer für Soundblock8/Cue0x0C. Das Rettungsangebot verwendet die originale quadrierte 3D-Entfernung von höchstens9.000.000. Leon wird über die echte Hundematrix nach `(431.86002,0,-989.88)` ausgerichtet und spielt seine tatsächliche 185-Frame-FCV ARC1B. Seine Waffe ist währenddessen verborgen und normaler Lauf-/Schussinput kann diese Aktion nicht unterbrechen. Die Rettung setzt unmittelbar SCF12 und fordert genau einmal den originalen Stream Block1/Cue0x0E an.

Der Hund spielt gleichzeitig ARC19/SEQ1C und wechselt auch bei einem originalen looping-SEQ-Endstatus in den Fluchtzustand. Die normale Flucht verwendet ARC1A/SEQ1D; ein Schuss auf Hund oder haltende Falle verwendet ARC9/SEQ12 und Cue0x0B, ohne SCF12 zu vergeben. Der Zielpunkt bleibt `(-24950,-400,-26740)`. Der Sprung über den Zaun verwendet ARC1E mit Cue7 bei Frame3. Anschließend folgen der tatsächliche Laufclip und das originale Ausblenden mit0.1 je Tick. Die haltende Falle sitzt bei Hundematrix×`(256.48,0,-304.75)` und verwendet ihre originalen Halte-, Öffnungs- und Rückstellclips.

Leons Rettungskamera verwendet die originalen lokalen Augen-/Zielwerte und FOV55. Die Bärenfallenaktion spielt Leons tatsächliche ARC10/SEQ11 mit Hokan5 und300 Schaden unter Erhaltung mindestens eines Lebenspunkts. Während der ersten120 Ticks liefert die Fallen-Kamera den tatsächlichen Fallenursprung und Leons Richtung; `actionCameraBlend()` liefert die originale0.1-Easingsrate für den Kameraeigentümer. Beim Ende der Motion werden Bodenplatzierung und normale Steuerung wieder hergestellt.

## Bärenfallen und Sprengdrähte

Die normale offene Bärenfalle spielt ARC0A, der Biss ARC0B/SEQ0D, Zerstörung ARC0C/SEQ0E beziehungsweise die bereits verbrauchte Endpose SEQ0F. Der Kontakt verwendet unverändert `XZ² <= 90000`, `abs(Y) <= 500`, lebenden und nicht bereits beschädigten Leon. Der Fangversatz ist `(34.69,0,250.42)`, einschließlich der originalen zehn Fang-Ticks und Leons30-Prozent-Drehung des restlichen Fangwinkels. Die originalen ausgesparten Waffenkategorien werden berücksichtigt.

Sprengdrähte haben keine FCV. Die echten nativen Ruhekörperteile1/2 werden über `parts.pos.z *= hp *0.001 *0.5` gestreckt; Originalgeometrie und inverse Bindematrizen bleiben erhalten. `AnimationRig` besitzt dafür einen ausdrücklichen statischen Modellpfad sowie echte Teilepositions- und Teilewinkel-APIs, ohne erfundene Animationskurven. `GenericEventActor` kann vorübergehende Teilewinkel beim Präsentationsende wieder auf ihren Originalwert zurücksetzen.

Die Kontakterkennung berechnet die inverse Fallenmatrix und verwendet die originalen eingeschlossenen Grenzen `X[-400,400]`, `Y[-2000,250]`, `Z[0,Abstand(Parts0,Parts2)+100]`. Lebende Ganados der ursprünglichen IDs10bis20 können den Draht ebenfalls auslösen. Waffen verwenden die originalen sieben Treffervolumen; ein Messer trägt die ursprüngliche Waffenkennung0x10. Die Explosion folgt nach den originalen drei Warte-Ticks, verbraucht den ESL-Eintrag und erzeugt zwei getrennte Schadenssphären mit Radius3000. Ihre Mittelpunkte liegen jeweils2000 Einheiten vom Originalpfosten0 beziehungsweise2 in Richtung Parts1. Schaden an Leon, Gegnern und anderen Objekten wird an die vorhandenen gemeinsamen Eigentümer weitergegeben.

Die Initialisierung fordert die ursprünglichen dauernden EST3/5-Funkeneffekte an. Treffer besitzen ihren tatsächlichen `EmGetDmPos`-Versatz und Ausrichtung; ein Kettenexplosionsschaden ohne registrierte Treffergeometrie erfindet keinen solchen Funken. Die Soundereignisse tragen den wirklichen EM21-/EM2A-Bankeigentümer; Leons Fanglaut verwendet dessen eigene Position und Bankargument0.

`BuildTrapDestructionEffects()` extrahiert alle13 unveränderten FE-Kaboom-Records der Sequenzen0/2/4/6 und die tatsächlichen CORE-Modelle8/14. Die parented Records behalten Parts0/2. `AppendDestructionData()` führt ihre Modelle und Meshindizes vor dem Aufbau des gemeinsamen Scene-/Simulationsbesitzers zusammen.

Für die kombinierte Sprite-/Modell-EST-Auswertung nimmt `triggerTrapRecord()` einen einzelnen Originalrecord, dessen echten Emissionszeitpunkt, Transform und Startseed entgegen. Der gemischte Effektbesitzer steuert den Generator. Der Modellbesitzer wiederholt weder Burstgenerator noch Verzögerung. Die13 Seed-Aufrufe aus `EfmSetObj05` und die getrennten sechs privaten Seed-Aufrufe pro neu zerstreutem BIN-Teil bleiben damit getrennt wie im Original. Emissionen werden nach Raumslot, Ereignis und Emissionsnummer genau einmal angenommen.

SCF12 und verbrauchte ESL-Indizes verhindern ein Wiedererscheinen geretteter/angeschossener Hunde und gezündeter Drähte beim erneuten Raumaufbau. Gesprungene Bärenfallen bleiben in ihrer tatsächlichen geschlossenen Endpose. Ein GPU-Kontextverlust baut ausschließlich GPU-Daten wieder auf.

## Nachweis und Grenzen

`tools/test_m37_dog_traps_gl.cpp` besteht zusätzlich **152.484 GLES-Prüfungen** mit allen acht tatsächlichen Disc-Akteuren. Die Produktion rendert jedes originale texturierte Modell sichtbar. Unabhängig berechnete CPU-Teilepaletten und die GPU-Positions-/Normalenausgabe unterscheiden sich höchstens um0.000138256 Originaleinheiten. Rettungsmotion, getrennte GPU-Eigentümer und deren Freigabe sowie die Wiederherstellung nach EGL-Kontextverlust erhalten den pausierten Originalzustand.

`tools/test_m37_dog_traps.cpp` besteht285.653 Prüfungen mit den tatsächlichen Disc-Dateien. Dazu gehören unabhängige wörtliche Kontaktorakel an den Originalgrenzen, sämtliche acht ESL-/BIN-/TPL-Eigentümer, statische Drahtskelette und inverse Bindematrizen, tatsächliche FCV-/SEQ-Rettung und natürliche Flucht bis zum Verschwinden, Schuss-Deduplizierung, beide Explosionszentren, Bissschaden und Raum-Wiederbetreten, dauernde Draht-EST, einzelne gemischte FE-Emissionen und die Originalkamera. Die echte offene Bärenfallen-FCV ARC0A hat zehn Frames und null Kurvenkanäle; Decoder und nativer Motionadapter halten dabei die unveränderte Modellruhepose, ohne einen erfundenen Kanal oder eine Ersatzmotion.

Die Adapter ersetzen nicht die gesamte ursprüngliche Engine. Der Hund verwendet die wirklichen Rootmotions und die echte Raum-SAT mit einem begrenzten Boden-/Kapseladapter; vollständige dynamische EAT/cAtari- und Routeneigentümer sind weiterhin getrennte Portierungsarbeit. Die globale Original-RNG-Kette über sämtliche Gegner hinweg ist ebenfalls nicht vollständig übertragen. Die r100-Hunderoutinen setzen das allgemeine Kopfverfolgungsflag nicht; die anderen EM21-Verhaltensfamilien aus späteren Räumen sind hier nicht enthalten. Der Sonderfall Wasserexplosion benötigt die originale Wasserhöhenabfrage und Esp40-Auswertung. Der Effektbesitzer dokumentiert separat die Grenze der ursprünglichen Esp07-Blutsimulation. Native und GLES-Prüfungen ersetzen keinen Spieltest auf einem Android-Gerät.

## Zusätzliche ursprüngliche Raummotion

`LeonPreview::installRoomScriptMotion()/beginRoomScriptMotion()/endRoomScriptMotion()` lassen den ursprünglichen r100-Raumeventbesitzer eine tatsächliche FCV auf Leons vorhandenem Skelett ausführen. Rootbewegung, feste30-Hz-Auswertung und die finale Pose bleiben bis zum Ende der Originalkamera erhalten. Dadurch benötigt das Haus-/Polizistenskript keinen zweiten Leon oder Drahtgitter-Ersatzakteur. Der Originaleventbesitzer kontrolliert Kamera, Beginn und Ende dieser Aktion.
