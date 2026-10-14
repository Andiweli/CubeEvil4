# M37: originale Nebenereignisse in r100

Die Implementierung verwendet die Disc-1-Ressourcen und die Aufgaben aus `upstream/re4/src/st1/r100.cpp`. Räume und Ereignisse werden auf dem Lade-Worker vorbereitet; Motion64-Laufzeitobjekte und GPU-Objekte werden auf dem Simulationsthread erzeugt. Ein GL-Kontextwechsel erhält die vorhandenen Besitzer und Zeitzähler.

| Originaler Auslöser | Portierter Ablauf |
| --- | --- |
| AEV 19 / `r100_MesCar00` | r100s41, 160 Frames, Stream 1/16, tatsächliche Figuren, Kamerakurve und MDT 55 |
| AEV 1a / `r100_MesCar01` | r100s43, 110 Frames, Stream 1/18, MDT 23 |
| AEV 1b / `r100_BackCheck` | r100s44, 150 Frames, Stream 1/19, MDT 36, originale Endplatzierung mit SAT-Bodenstrahl |
| AEV 1/18 nach dem Hinterhalt | Einmal r100s30 mit RSF 14; spätere Untersuchungen mit ursprünglicher CAM 9/8 und MDT 28/0e; ursprüngliche Spielerplatzierung wird erhalten |
| AEV 15 / Hauserschrecken | RSF 15; CAM 10; Leon ROOM_ARC 34; temporärer ESL 25/set 1b mit originalem `R100TurnWalk`, anschließend Freigabe |
| AEV 0b/0c und weitere Textbereiche | Originale Raum-MDT, Seiten, Zeichenrate, Eingabe- und Zeitwartecodes; Türklopfen 6/29 und Schädel-Stream/Lautstärkensteuerung |
| Dynamische Untersuchung des ersten toten Ganados | Originale lokale 1500×1500-Area, Höhe 1000, Priorität 1 und Ziel 1; Kamera von Leons Teilen 32/33 zum Ganado-Teil 2; MDT 0d und NoSuspend |
| Ende r100-Einstieg | Funkruf 0, soweit Einführungs-Skip/SCF 59 ihn nicht unterdrückt; `OpeSetMdtNo(0)` wird auch beim Skip erhalten |
| Ende r100s20 | Funkruf 1; temporäre Position (-81500,860,-38900), Winkel 1.6; ursprüngliche Position/Winkel nach dem Ruf wiederhergestellt |

`R100Secondary` erstellt die beiden Polizeiautos, Polizisten samt ursprünglichen Gesichtsmodellen sowie die regionalen hängenden Körper aus den richtigen ARC-Ressourcen. Nach r100s20 gelten die ursprünglichen Wrackplatzierungen, Teileskalen und Sichtbarkeiten. Quellcode-ARC-Nummern enthalten vier Headerwörter; die Bindungen prüfen die tatsächlichen BIN/TPL/FCV-Tags. Sichtbarkeit und Fortschritt während Ereignissen folgen den konkreten `setNoSuspend`-Aufrufen. SST 17/18 werden als eigenständige Bereichseffekte behandelt.

Die Sprachunterdrückung von `Event::MesSet` betrifft englische Film-Untertitel. Gewöhnliche Raumtexte bleiben in Englisch aktiv. Die GPU misst die vollständige aktuelle MDT-Seite vor dem schrittweisen Aufdecken, damit zentrierte Texte nicht während der Ausgabe wandern.

Der Funkbeginn nutzt Leons echte PL_ARC 79 mit Flags 0x201 und den Funk-BIN/TPL 77/78, keinen geklonten Spieler. Die Quelle wartet zunächst einen Tick und dann 21 Schleifendurchläufe. Deshalb erscheinen Funkgerät und Griff 2 bei Tick 22. Das Gerät hängt an Teil 16 mit Offset (111,-22,66) und Rotation (-0.48869219,0.31415927,-0.73303829). Nach Abschluss der 71-Tick-Motion öffnet die tatsächliche `ss_term`-Oberfläche mit elf originalen Modellen, Layouts, MDT und DSP 1/8c beziehungsweise 1/8d. B schaltet auf manuelle Dialogfortsetzung, A bestätigt, START schließt. Der Ablauf erhält den letzten `SceSleep(1)`, stoppt die Streams und stellt Griff 0x63 und Augenmodus 0 wieder her.

`OpeSetOpenTermEnd` startet zwei gleichzeitige Schwarz-nach-Transparent-Fades: System-Slot 0 über drei und Szenario-Slot 1 über zehn Quellticks. Er beendet außerdem Raum-Slot 2. Die beiden Übergänge sind keine aufeinanderfolgenden Aus-/Einblendungen. Funknachrichtenbits und aktuelle Nachrichten-ID werden im originalen Spielstandsformat gespeichert.

Die Laser-Erweiterung nutzt die tatsächlich posierten Trefferformen von Krähen, Hund und Fallen. Die originalen Effekt-SAT-Attribute steuern `DontFire` und `NO_MARK`. Polizeifiguren sind in der Quelle Scroll-Objekte und gehören nicht zum EmMgr-Zielpfad; der Port erfindet dafür keine Treffer-Radien.

## Prüfungen

`m37_secondary_event_check` besteht im abschließenden frischen Build 14.541.503 Prüfungen. Es prüft sämtliche 760 Frames der vier Neben-EVDs in Englisch und Japanisch, tatsächliche Kamerakurven, Modelle und Audio, acht Raumkameras, persistente Figuren, Hausablauf, reale Ganado-Leiche, Laserformen und `DontFire`-Grenzen. Die Funkprüfung umfasst die echten PL_ARC-Ressourcen, das 22-/71-Tick-Timing, den Teileskelett-Anhang mit exakter SDK-Matrixkonkatenation und die Griffwiederherstellung.

`m37_secondary_runtime_check` besteht im abschließenden frischen Build 199 Prüfungen mit Produktions-JNI und echtem GLES/EGL. Es prüft Neben-EVD, Hauskamera, realen Leon, temporären Gegner, MDT, Funkbeginn, Funkoberfläche, Stimmen, Eingaben, Rückplatzierung und Wiederherstellung nach Kontextverlust. Persistente Figuren bleiben nach dem Abbau von Ereignisressourcen sichtbar; vollständiger Szenenabbau und Kontextverlust räumen ihre eigenen GPU-Ressourcen auf.

Diese gezielten Nachweise sind kein Beleg für eine vollständige Gleichheit sämtlicher Leon-, Gegner-, Raum- oder Renderer-Systeme. Die vollständige Quellroutine-Inventur und verbleibende Abhängigkeiten stehen in `R100-SOURCE-INVENTORY-M37.md`.
