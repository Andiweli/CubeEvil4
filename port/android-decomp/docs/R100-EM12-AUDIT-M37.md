# M37 – EM12-Audit für das ursprüngliche erste Gebiet

Referenz ist der eingefrorene GameCube-Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7`. Geprüft wurden die tatsächlichen Disc-1-ESL-Daten und `r100.dat`, nicht bloß synthetische Testspawns. Dieses Dokument beschreibt den tatsächlich übertragenen Umfang und die verbleibenden Abhängigkeiten; es erklärt die vollständige globale EM10-Engine nicht für fertig.

## Tatsächlich verwendete Akteure

| ESL-Index | Typ / Waffe | HP / set | Auslösung im Original |
|---|---|---|---|
| `0xFF` | Typ0 / Axt2 | 500 / `0x13` | Erster Hausgegner über `EmSetEvent`; kein ESL-Record |
| `3` | Typ4 / Axt2 | 500 / 0 | `r100_Sce_zombi_dead` ruft `r100_em_set` nach dem ersten Hauskampf und vor dem Lastwagenereignis s20 auf |
| `4` | Typ3 / Hacke1 | 500 / `0x1F` | Wie oben; anfänglich `R100WalkStay` |
| `5` | Typ1 / Sichel3 | 500 / `0x1F` | Wie oben; anfänglich `R100WalkStay` |
| `0x12` | Typ3 / Hacke1 | 500 / 0 | `r100_em_set` |
| `0x13` | Typ4 / Axt2, gespiegelt | 500 / 0 | `r100_em_set` |
| `0x26` | Typ1 / Sichel3 | 500 / 0 | `r100_em_set` |
| `0x27` | Typ0 / Axt2 | 500 / 0 | `r100_em_set` |
| `6 / 7 / 8` | Typ1 / 3 / 0 | 1000 / 2 / 3 / 4 | `r100TrapSet`; die ursprüngliche Cliff-Routine setzt HP0 vor dem Sturz |
| `0x25` | Typ0, ohne Werkzeug | 1000 / `0x1B` | Vorübergehender Akteur der Haus-Schreckszene; kein normaler Kampfspawn |

ESL0 ist zwar als inaktiver 500-HP-Record vorhanden, wird jedoch nicht zusätzlich zu dem `0xFF`-Ereignisgegner erzeugt. Die Indizes9 bis0E,1D und7A in `r100TrapSet` gehören zu EM2A-Fallen und sind keine zusätzlich fehlenden Ganados. Die fünf anfänglich aktiven Krähen sind EM23, nicht EM29; ihr eigener Audit liegt in `FOREST-CROWS-M37.md`.

## Ergänzungen dieses Patches

`em10FindCk` wurde als vollständiger unveränderter Funktionskörper in `enemy_awareness.cpp` übertragen. Sicht benutzt die strikten Originalgrenzen15000 bei Winkel unter60Grad, Gehör für tatsächliche Leon-SEQ-Schritte unter5000 und unmittelbare Nähe unter3500. Das originale Deaf-Flag10, die Routendistanz25000 für Waffen-/Burstgeräusche, Damage- und Force-Flags sowie der unterschiedliche Höhencheck der Modi bleiben erhalten. Die Bridge reicht die tatsächlich emittierten Schritt- und Schussticks des vorherigen Spielerticks weiter, entsprechend EmMgr vor PlMove. Die Schrittkennungen0/1/2/3/D/E/14 wurden direkt gegen `cPlayer::seqSeCtrl` geprüft;4/5 setzen das Original-Schrittstatusflag nicht.

Die beiden vollständigen Originalfunktionen `em10SomebodyDamageNowCk` und `em10SomebodyFindNowCk` sind ebenfalls unverändert übertragen. Die lebende Gruppe liefert ihre tatsächlich aktuellen Routinezustände in ursprünglicher sequenzieller Update-Reihenfolge. Schäden anderer aktiver Ganados werden im originalen3D-Abstand unter3000 oder vor dem Beobachter unter60Grad und10000 erkannt. Ein anderer laufender Find-Ruf unterdrückt einen zweiten Find-Ruf. Script-Cliff-Akteure mit HP0 werden nicht zu beschädigten Gegnern umgedeutet.

`EnemyRoutes` liest das vollständige tatsächliche r100-RTP mit66 Punkten,160 Links und4356 Next-Hop-Einträgen. Es übernimmt die ursprüngliche Sortierung der zehn nächsten Punkte einschließlich Gleichstandsreihenfolge, die Original-SAT-Masken, direkte Erreichbarkeit mit Bodenprüfung, nächste Wegpunkte,3D-Routenlänge und maximalen Höhenunterschied. Es ersetzt den bisherigen geraden Lauf gegen eine Wand durch die im Disc-Record vorgesehene Route. Ungültige Tabellen und Zyklen werden vor dem Livebetrieb zurückgewiesen. Die originalen Satelliten-/SAT-Abfragen werden über den bestehenden nativen Kollisionsbesitzer bereitgestellt; der Test behauptet keine zweite unabhängige Portierung der Kollisionsprimitive.

Unentdeckte Keeper verwenden die ursprüngliche50.000-Einheiten-Ausblendgrenze. Vollständig ausgeblendete Akteure erhalten keine Ziel-/Kollisionskontakte; beim Näherkommen werden Sichtbarkeit, Opazität und Kontakte wiederhergestellt. Die direkte Schulter-Sichtprobe alterniert ±200 lokalesX bei +1500Y. Find und hintere Entdeckung verwenden echte ARC73/74 beziehungsweise ARC16/17 und die Original-Winkellimits; der Find-Ruf kommt aus dem ursprünglichen SEQ-Free20-Marker. Die vollständigen globalen Ctrl11-Timingregeln bleiben separat.

Die gewöhnlichen Initial-RNG-Aufrufe für Walk_type, DownCnt, Dash, Breath, Jcatch, Csaw und Claw werden in Originalreihenfolge ausgeführt. Die ursprüngliche Grundgangart verwendet je nach Walk_type ARC8 oder ARCE. Nach Entdeckung halten bewaffnete Axt-/Sichel-Ganados ihre Waffe mit ARC D8 und den fünf tatsächlichen SEQ-Varianten D9 bis DD; Hacken-Ganados verwenden ARC14B und14C bis150. Die Original-ESL-Indexvariante und die gespiegelte Waffenseite bleiben erhalten. Häufige Front-/Rückentreffer bei gehaltener Hacke benutzen jetzt ARC159/15A und15B/15C einschließlich des Original-Spiegelflags.

Trockene Ganado-Fußschritte folgen exakt den tatsächlichen Motion-SEQ-Ereignissen, den Originalparts15/19 und Materialblock5/FSE. Ihre Position stammt vom gelösten Fuß, nicht pauschal von der Actor-Wurzel. Die gemeinsame EM12-Soundbank hat Argument10; die Tierbanken21/23/2A überschreiben sie nicht.

Die normale stehende Todesmotion gibt ACTIVE/ITEMSET und damit Beute nach Timer49hex frei, während der ursprüngliche Zusammenbruch noch läuft. Erst die folgende Die_Cramp-Phase nach abgeschlossener Fallmotion führt `EmSetDieCnt` aus. `sourceKillCount()` zählt diese Phase genau einmal; Cliff und der temporäre Hausakteur zählen keinen normalen Kampfkill. Das permanente Liegenbleiben der Leichen in r100 entspricht der expliziten Raumregel in `Die_Cramp`. Garantierte ITA-Beute, Original-Zufallsbeute, Aufnahme und Itemeffekte haben separate geprüfte Besitzer. Der EreignisindexFF erhält reguläre Zufallsbeute und fordert keinen nicht vorhandenen ESL-Record an.

Die Schussabfrage speichert den ursprünglichen EAT-Treffer pro Strahl vor der Damage-/Objektmutation. `GetWepTargetList2` löscht die Scenery-Normale bei jedem gefundenen Akteur oder Objekt, einschließlich durchschießbarem Glas. Ein späterer Render-/Effektcallback fragt diese bereits veränderten Zielzustände daher nicht erneut für den Bodeneffekt ab. Die Standalone- und Gruppenbesitzer geben je einen gespeicherten Wert pro tatsächlichem Strahl zurück; die Schrotflinte verwendet13 Strahlen. Bereits vorgemerkter Schaden sperrt erst folgende Strahlen desselben Schusses, entsprechend `em->dmg.isDamage()` im Original. Ein tatsächlicher Fehlschuss behält den originalen EAT-Punkt, die Normale und Attribute.

Die Gruppe speichert zusätzlich die tatsächlich gewählte fremde Tier-/Fallen-Zielentscheidung pro Schuss- und Messerstrahl. Die sortierte Original-Grenze beträgt zwei gewöhnliche Ziele für den Punisher und ein Ziel für die anderen hier verwendeten Feuerwaffen beziehungsweise das Messer. Ein später vorgemerkter Ganado-Treffer darf dadurch keine dahinterliegende Krähe oder Falle für denselben ursprünglichen Strahl freigeben. Der Messerpfad prüft die tatsächliche EAT-Maske0, davorliegende Boxen und die geschlossene Tür; gewinnt der fremde Besitzer, wird kein zusätzlicher Messertreffer in den weiter entfernten Ganado geschrieben. Die beiden Ergebnislisten werden getrennt unter ihrer tatsächlichen Angriffskennung entnommen. Die Live-Bridge übernimmt diese gespeicherte Auswahl, während die fremden Besitzer ihre eigenen geprüften Reaktionen ausführen.

## Quellenabhängigkeiten, die keine zusätzliche r100-Funktion sind

`em10ThrowAxeCk` verweigert geworfene Äxte ausdrücklich in Stage1/Room0. `cEm10::setWeaponFall` verweigert Waffenabwurf in diesem Raum solange HP positiv ist. Sämtliche sieben normalen Spawns und der erste Hausgegner sind bewaffnet. Die normale Sichel3 wählt dieselbe ursprüngliche AxeAtk-Routine wie Axt2; eine separate ScytheAtk wäre für diese Profile falsch. Vollständige unbewaffnete Seiten-/Rückengriffe sind daher keine normale erreichbare Kampfroutine dieser acht bewaffneten Spawns.

## Verbleibender Umfang

Die getesteten Routinen reichen zum tatsächlichen Erkennen, Anlaufen, ursprünglichen Werkzeugangriff, ausgewählten Treffer-/Nahkampfreagieren, Sterben und Droppen der vorhandenen r100-Kampfprofile. Folgende globale beziehungsweise zusätzliche Zweige sind weiterhin begrenzte Adapter:

- Die vollständige dynamische Walk-Verzweigung mit gegnerischem Tür-/Fensteröffnen, Leitern, Sprüngen, ClimbOver, Lost/Return sowie alle Route_type-Flanken-, Back_wait-, Dash-/Stay-/Threat-Entscheidungen. Das übertragene RTP ist die originale Wegpunktentscheidung, nicht diese gesamte zusätzliche Zustandsmaschine.
- Arm-/Bein-spezifisches Dm_Small, Kniezustände und deren Kick-/Suplex-Kontexte. Die häufige Hacken-Torso-Sondermotion ist übertragen; übrige Limb-Sonderzustände werden noch nicht als identisch ausgegeben.
- Originale Waffenteil-Fallphysik, zusätzliche Todesstaub-/Boden-EST, vollständige Kopfverlust-/Parasitenmodelle, dynamische Kollision/IK und einige Shake-/Vibrations-/Rank-Point-Effekte.
- Geteilte globale EmMgr-RNG-Reihenfolge und Ctrl11-Sprach-/Atem-Cooldowns. Die nativen Akteure besitzen getrennte reproduzierbare RNG-Zustände und bewahren den RNG des Aufrufers; das ist keine Behauptung über eine identische gesamte Original-RNG-Reihenfolge.
- Die vollständigen unbewaffneten Griff-, DashCatch- und Mehrgegner-Cut-in-Kameras anderer Profile. Der vorhandene diagnostische Frontgriff ist separat begrenzt.

## Nachweise

`python3 tools/test_enemy_ai_source.py` prüft Bytegleichheit der drei vollständigen Awareness-/Peer-Funktionskörper gegen die eingefrorene Quelle. Die sechs vollständigen Route-Oracle-Funktionen bleiben ebenfalls gleich; die einzige erlaubte Änderung ersetzt ursprüngliche32-Bit-Pointercasts durch `std::uintptr_t` für den64-Bit-Testhost.

`m37_enemy_ai_check R100.DAT ESL EM12.DRS PL00.DRS WEP01.DRS` bestanden:18.343 Prüfungen. Der Test vergleicht jede der66×66 tatsächlichen Punktpaar-Distanzen und Route-Schritte gegen diese Originalkörper. Weitere Prüfungen betreffen die nächsten zehn Punkte, Sicht-/Schritt-/Schussgrenzen, Peer-Gates, die sieben tatsächlichen Live-Spawns, echte bewaffnete Gangarten und Keeper-Ausblendung/Wiederherstellung.

`m37_enemy_sequence_audio_check R100.DAT ESL EM12.DRS R100.DAS` bestanden:8.527 Prüfungen,111 tatsächliche SEQ-Schritte über die acht Kampfprofile, geposte Fußpositionen, echte FSE-/Materialbank, Timer49-Drop, einmalige Die_Cramp-Killzählung und gespeicherte Standalone-Schussabfragen einschließlich13 Schrotstrahlen. Der Original-Encounter-Gesamttest bestand mit3.386 Prüfungen; die Gruppenprüfung bestand mit5.928 Prüfungen. Die Schussprüfungen umfassen den vorgemerkten Akteurstreffer, durchschießbares Glas, die geschlossene Tür mit den tatsächlichen späteren RSF3/10-Flags, unveränderte EAT-Fehlschusstreffer und einmaliges Entnehmen unter der richtigen Schusskennung. Gemischte fremde Ziele werden vor/hinter zwei tatsächlich geposten Ganados verglichen, einschließlich Punisher-Zweitplatz, sekundärem Schrotstrahl und Fehlschuss außerhalb der echten Segmentlänge. Messerprüfungen verifizieren näheren fremden Besitzer, Abschirmung durch den Ganado, geschlossene Tür, durchschießbares Glas und den unterbliebenen zusätzlichen Ganado-Schaden. Die hier fremde reine Query liefert einen kontrollierten Kandidaten; der Krähen-/Fallenbesitzer hat eigene tatsächliche Disc-Datenprüfungen.

Host-/Quell-/GLES-Prüfungen ersetzen keinen tatsächlichen Android-Gerätelauf.
