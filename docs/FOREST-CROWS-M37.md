# M37 – r100 Krähen, Gegnerbeute und getrennte Tier-Soundbanken

Referenz: `upstream/re4`, Stand `9b76ff003081d6ece8b838453fb8310743a972c7`; tatsächliche NTSC-USA-Disc-1-Dateien `etc/emleon00.esl`, `em/em23.drs` und `r100.dat`. Modelle, Texturen, FCV/SEQ und Disc-Sounds werden nicht durch Ersatzgrafiken oder erfundene Tierpositionen ersetzt.

## Originale Krähen

EM23 sind die Krähen. EM29 sind Fledermäuse und gehören nicht an diese r100-Positionen. Die fünf ursprünglichen EM23-Einträge sind schon bei Raumstart aktiv; `r100_trap_set()` schaltet sie nicht erst nach dem Lastwagenereignis ein.

| ESL-Index | Ausgangsposition in Originaleinheiten | Verhalten | Beute |
|---|---|---|---|
| `0x0F` | `-89580, 2060, -3790` | set0, hp10 | Originale zufällige Geldmenge |
| `0x10` | `-85630, 0, -4640` | set0, hp10 | Spinell `0x57`, ITA9, Raum-Itemflag11 |
| `0x11` | `-89250, 0, -6400` | set0, hp10 | Originale zufällige Geldmenge |
| `0x20` | `-36110, 1740, -17410` | set0, hp10 | Handgranate `1`, ITA10, Raum-Itemflag12 |
| `0x21` | `-38430, -180, -18950` | set0, hp10 | Originale zufällige Geldmenge |

`ForestAnimals` besitzt fünf unabhängige 24-Teile-Skelette und stabile GPU-Eigentümer. Die ursprünglichen Körper- und austauschbaren Flügelmodelle sind ARC5/6/7, Texturen ARC8. Der DRS-Datenrecord beginnt erst bei ARC4; deshalb entsprechen die tatsächlichen DRS-Einträge 1/2/3/4. Die beiden kombinierten Körper/Flügelvarianten enthalten 1080 und 1000 Dreiecke.

Übertragen sind die von diesen set0-Einträgen erreichbaren Originalroutinen: Wait, Takeoff, Turn, Dm_Air und Die_Normal, einschließlich vier Wartemotions, fünf Takeoff-SEQ-Varianten und sechs gleitenden Flugvarianten. Die Modelle verwenden dieselbe native `MotionMove`/FCV/SEQ-Auswertung wie die übrigen Originalakteure. `partsFixMemory(20)`/`partsFixAdjust()` halten beim Warten den ursprünglichen Fuß in XZ fest. Die originale zufällige Größenvariation beträgt ±10 Prozent.

Die Auslösung verwendet die strikten Originalgrenzen der quadrierten XZ-Entfernung: unter 25.000.000 ohne Schuss, unter 100.000.000 mit Schussgeräusch. Der Flug beschleunigt pro Originaltick mit +10 bis auf 150 vorwärts und +10 bis auf 50 aufwärts. Die Zielhöhe beträgt `(ESL-Index % 5) * 1000 + 8000` über Leon. Das originale Herumfliegen wird fortgesetzt; die Krähe wird nicht willkürlich beim Erreichen einer Entfernung gelöscht.

Die originale Trefferkapsel sitzt bei Offset `(0,-100,0)`, Radius250, Höhe200. Die Handfeuerwaffen-/Messertreffer wählen 900 bis1099 Schaden; Schrotflinten unter der strikten 6000-Einheiten-Grenze und andere tödliche Waffenkategorien 9999. Nach dem Treffer folgen der ursprüngliche Fall, Kontakt zum echten SAT-Boden, Todesmotion und genau ein Beutevorgang. Gespeicherte Todesindizes verhindern eine Wiederbelebung beim erneuten Raumaufbau; ein EGL-Verlust baut nur GPU-Daten auf und verändert die CPU-Motion nicht.

## Originale Beute statt ausschließlich Zufallsgeld

ITA `waiting_type=1` bezeichnet eine Gegnerbindung. `waiting_type=2` gehört zu ETS-Objekten. `FindAuthoredEnemyDrop()` liest unverändert Item-ID, Anzahl, Flag, Effekt und Originalrecord. Die beiden tatsächlichen r100-Krähenbindungen werden an die Tiere übergeben. Ein bereits beanspruchtes gebundenes Item wird nicht durch Zufallsgeld ersetzt.

`ClaimAuthoredEnemyDrop()` setzt entsprechend `EmSetDropItem()` das globale Itemflag beziehungsweise das Raum-Itemflag vor der Drop-Erzeugung. `ResolveAuthoredEnemyDrop()` behält eine Anzahl0 für die anschließende normale Item-Defaultmenge bei. `ForestAnimals.setAuthoredDrops(room, scenario)` muss beim Raumaufbau und nach einem Eigentümerwechsel gebunden bleiben. `takeDrops()` liefert die endgültige Item-ID/Anzahl/Position an die normale Aufnahmeverwaltung.

`OriginalEnemyRandomDrop()` enthält die vier vollständigen ursprünglichen Funktionskörper `GetBulletPoint`, `GetDropBullet`, `RandomHandgunAmmo` und `RandomItemCk`. Inventar, Lebenspunkte, Spielrang und gespeicherte No-Drop-Zähler werden über einen expliziten Kontext bereitgestellt. Getragene Schrotflinten und andere tatsächlich vorhandene Waffenfamilien werden berücksichtigt. Der gleiche Helfer ist für normale Ganado-Beute verfügbar. Die Krähen-Geldmenge wird mit unveränderter Byte-RNG-Reihenfolge gewählt, einschließlich des vorangestellten allgemeinen Tabellenwurfs. Ein optionaler gemeinsamer `EnemyDropState` kann vom Raum-/Kampagneneigentümer über `setDropState()` gebunden werden.

## Soundbanken

Mehrere Tiere und Ganados verwenden alle Soundblock8. `SoundMixer` speichert Banken nun nach `(block, argument)` und hält den tatsächlichen Bankeigentümer einer laufenden Stimme fest. Eine weitere Tierbank überschreibt deshalb keine Ganado-, Hunde- oder Krähenbank mehr. Zufallsvariationshistorien sind ebenfalls pro Bank getrennt.

| Datei | Block | Tatsächliches Bankargument |
|---|---:|---:|
| `em12.drs` | 8 | `0x10` |
| `em21.drs` | 8 | `0x21` |
| `em23.drs` | 8 | `0x23` |
| `em2a.drs` | 8 | `0x2A` |

Das Ganado-Modul12 verwendet die gemeinsame Bank10; der Modulname allein ist kein gültiges Bankargument. `play`, `playSpatial` und `hasCue` nehmen optional am Ende ein explizites Bankargument an. Bestehende Aufrufe ohne Argument folgen weiterhin der zuletzt installierten Bank dieses Blocks, damit Waffenwechsel kompatibel bleiben. `replaceRoom` entfernt alle Raumargumente von Block5/6. `clear` entfernt ebenfalls die Auswahlhistorie und aktuelle Standardbank.

Krähen verwenden die originalen Flügel-/Treffer-/Todes-/Sequenz-Sounds und den gemeinsamen 10-Tick-Cooldown von `Ctrl11SetSe(...,10,10,11)` für das Abheben. Die Sequenz-Sounds werden an der tatsächlich geposten Parts0-Position ausgegeben.

## Validierung

- `forest_animals_check R100.DAT EMLEON00.ESL EM23.DRS`: 75.591 Prüfungen bestanden, einschließlich aller fünf echten ESL-Einträge, originaler Modelle/Texturen, 30-/60-Hz-Gleichheit, Abheben, Flug, SFX, Treffer/Fall, Geld und beider garantierten ITA-Belohnungen, Itemflags und Wiederbetreten.
- `forest_animals_gl_check R100.DAT EMLEON00.ESL EM23.DRS OUTPUT.PPM`: 187.346 Prüfungen bestanden. Beide Flügelvarianten aller fünf Besitzer wurden mit der produktiven GLES-Pipeline gezeichnet. Maximaler Unterschied zwischen erfassten GPU-Skinningdaten und unabhängiger CPU-Auswertung: 0,00010338 Originaleinheiten. Andere Eigentümer und Kontextverlust verändern den pausierten Bildinhalt nicht.
- `original_enemy_drop_tests`: 203.403 Prüfungen bestanden; sämtliche 65.536 Krähen-RNG-Startzustände gegen eine unabhängige Byte-Rekurrenz, unveränderte Drop-Reihenfolge, Itemflag-Vorrang und garantierte Beute sowie normale Ganado-Drops mit getragener Schrotflinte.
- `python3 tools/test_enemy_drop_source.py`: die vier vollständigen übertragenen Entscheidungskörper sind bytegleich zum eingefrorenen Originalquellcode.
- `enemy_audio_banks_check EM12.DRS EM21.DRS EM23.DRS EM2A.DRS`: 34 Prüfungen bestanden, einschließlich koexistierender gleich nummerierter Sounds und exakt gleicher PCM-Ausgabe wie bei isolierter Bankinstallation.
- Bestehende `audio_tests`: 362 Prüfungen bestanden.

Die spezifischen r20a-Routinen mit Leichenbindung werden von r100 nicht verwendet und nicht als funktionsfähig ausgegeben. Der native Adapter benutzt weiterhin eigene kontrollierte RNG-Eigentümer statt der vollständigen globalen Original-Engine-Zufallskette. Die vollständige `SatMgr.checkAir`-/dynamische Tier-Atari-Welt ist noch nicht übertragen; das Fallen nutzt die echte SAT-Abfrage, normale Flugbewegung die Originalparameter. Der Lichtselektor verwendet wie bei anderen Akteuren einen umschließenden Bereich statt der vollständigen Originalzylinderprüfung. Diese Prüfungen ersetzen keinen Test auf dem Android-Gerät.
