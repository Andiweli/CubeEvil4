# Original-Leon, Root-Motion und Waffenaktionen — M18

## Belegte Datenquellen

Referenz: `upstream/re4` Commit **9b76ff003081d6ece8b838453fb8310743a972c7**. Originalbelege: `pl_leon.cpp::setModel`, `wep01/objFn57.cpp::{init,setMotion,moveFire,moveReload}`, `wep/pl_handgun.cpp`, `player.cpp::{pl_R1_Walk,pl_R1_Back,pl_R1_Run,pl_R1_Turn,pl_R1_Turn180}`, `pl_class.cpp::{cPlayer::motionSet,cMot3,cPlWaist::set}`, `pl_wep.cpp::PlWepLockCtrl`, `motion.cpp`, `ik.cpp`, `model.cpp`, `math_sub.cpp` und die verwendeten SDK-Matrix-/Vektor-/Quaternionfunktionen. Die vollständige Rekonstruktion ist eine GameCube-Debugversion; ihre Existenz ersetzt keine native Android-Anpassung.

| Importdatei | Größe | SHA-256 |
| --- | --- | --- |
| `disc1/files/em/pl00.drs` | 2.215.392 | `5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090` |
| `disc1/files/em/wep01.drs` | 437.536 | `fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689` |

Die App prüft beide auf dem GL-Thread vor dem nativen Laden. Das betrifft zusammen etwa 2,7 MB, keine vollständige Disc. Die native Seite liest den Typ-0-DRS-Datenrecord ab 0x400 mit geprüfter Länge und verwendet den vorhandenen Archivleser für die darin liegenden Teil-Dateien. Assets bleiben im Importverzeichnis; keine Textur oder Motion wird mit diesem Quellcodepaket verteilt.

Archivindizes, nullbasiert, ohne die vier ursprünglichen Headerwörter:

| Bestandteil | BIN-Quelle | TPL-Quelle |
| --- | --- | --- |
| Körper / Kostüm | PL00: 0 / 6 | PL00: 1 |
| Kopf / Haare / Augen | PL00: 4 / 2 / 5 | PL00: 3 |
| Rechte Griffhand / linke Hand | WEP01: 6 / PL00: 20 | PL00: 13 |
| Normale WEP01-Handfeuerwaffe | WEP01: 2 | WEP01: 1 |

PL00-Index 9 ist das normalerweise verborgene Messer, kein Gesichtsteil; das Messer bleibt außerhalb der Vorschau. Die in M17 eingeführten sieben WEP01-FCV/SEQ-Paare bleiben aus derselben bereits verwendeten Datei erhalten:

| Zustand / Clipindex | FCV / SEQ, nullbasierter DRS-Index | Sequenzframes | Originale Root-Bewegung pro Zyklus |
| --- | --- | --- | --- |
| Stand / 0 | 7 / 8 | 90 | keine Translation |
| Gehen / 1 | 13 / 14 | 28 | +1.624,8726 auf Z |
| Laufen / 2 | 17 / 18 | 21 | +2.934,1172 auf Z |
| Rückwärts / 3 | 15 / 16 | 21 | −1.378,3201 auf Z |
| Links drehen / 4 | 11 / 12 | 41 | etwa +π um Y |
| Rechts drehen / 5 | 9 / 10 | 41 | etwa −π um Y, geringe XZ-Translation |
| Schnell wenden / 6 | 19 / 20 | 23 | etwa −π um Y, geringe XZ-Translation |

Die ursprünglichen ARC-Zeiger enthalten vier Headerwörter; die Tabelle zeigt jeweils ARC-Index minus vier. Die ersten drei Clipindizes und die frühere Vorschau-/Sampling-API bleiben für die M16-Referenzprüfungen erhalten.

M18 ergänzt neun Spieler-FCVs zu insgesamt 16 Clips. Die folgenden Archivindizes sind ebenfalls nullbasiert:

| Zustand / Clipindex | FCV / SEQ | Auswertung |
| --- | --- | --- |
| Anlegen / 7 | 30 / 31 | Originale 15-Frame-SEQ und Hokan |
| Ablegen / 8 | 32 / 33 | Originale 15-Frame-SEQ und Hokan |
| Zielen / 9, 10, 11 | 34, 35, 36 / ohne SEQ | Originaler `cMot3`-Blend: Basis, positiver, negativer Zielwinkel |
| Rückstoß / 12, 13, 14 | 37, 38, 39 / ohne SEQ | Drei Zielwinkel mit Originalrückstoß |
| Nachladen / 15 | 41 / ohne SEQ | Originale Basistuning-Kurve bis Frame 51 |

Die Waffe verwendet ihr eigenes Vier-Gelenk-Skelett und sechs FCVs ohne SEQ: geladenes Stand-/Schuss-/Nachlademodell 48/46/50, leere Varianten 53/51/47. Diese Gelenke gehören zur Waffe; sie werden nicht als Leons Gelenke interpretiert. Die Elternmatrix ist Leons rohe posierte rechte Hand **Teil 10**, anschließend Leons Weltplatzierung. Die inverse Bindepose der Waffe bleibt davon getrennt. Bereits vorhandene vollständige PL00-/WEP01-Imports reichen aus. Details zu Ereigniszeiten, Originalbelegen und Grenzen stehen in [WEAPON-ACTIONS.md](WEAPON-ACTIONS.md).

## Datei- und Laufzeitlayouts getrennt

`model_skin.cpp` verwendet den begrenzten BIN/GX-Leser und liest zusätzlich 8-/12-Byte-Gewichtseinträge, bis zu drei Gelenke pro Eintrag, Positions-/Normalenpaletten sowie Mischgelenke. Alle Referenzen werden gegen das Eigentümerskelett geprüft; die kleinen Tabellen der angehängten Kopf-/Handmodelle sind kein vollständiges Ersatzskelett.

`animation.cpp` wandelt FCV/SEQ byteweise von Big-Endian in eigene, geprüfte native Arrays um. Keyoffsets bleiben Dateioffsets bis zur Bereichsprüfung; native Kurvenzeiger liegen in einer getrennten Zeigertabelle. Kurvenachsen können ungerade ausgerichtet sein und werden byteweise gelesen. Framebereiche, Keyreihenfolge, Kanaltyp, Gelenk-/IK-Verweise und Sequenzlänge werden vor der Originalauswertung geprüft. Eine fehlende SEQ ist für die belegten Aktions- und Waffen-FCVs zulässig und verwendet die originale lineare Frameauswertung. Der Adapter begrenzt einen Besitzer auf höchstens 64 Clips und 1.023 Kurvenframes je Clip.

`ports/motion64` enthält tatsächlich gebaute Ableitungen der Originalauswertung. `tools/generate_motion64.py` reproduziert diese Dateien aus dem fixierten Referenzbaum. Es ersetzt Registerbindungen, konsolenspezifische Pointerarithmetik und diskbasierte Relokation. Eine leere Original-Assembleranweisung mit gebundenem Registerausgang wird als Identitätszuweisung erhalten; reine Scheduleranker entfallen. Das ist insbesondere für `MotionHokan` und optimiertes Clang wichtig.

Es werden keine `MAP_32BIT`-Allokationen, PowerPC-Objekte oder GameCube-Festadressen auf Android verwendet. Der unabhängige x86-Referenzhelper des Upstreams verwendet weiterhin seine eigene niedrige Allokation; er ist ausschließlich ein Host-Test und nicht Bestandteil der App. Native Standardtrigonometrie ersetzt Original-libm/Paired-Single-Ausführung, daher wird keine bitgenaue PowerPC-Floatgleichheit zugesagt.

## Bewegung und Zustände

`LeonPreview::step` akkumuliert feste 30-Hz-Schritte und übernimmt höchstens 50 ms Zeit pro Renderframe. Pro Schritt wird das originale `MotionGetSpeed` genau einmal vor Pose und `MotionSequenceCtrl` ausgewertet. Die Root-Kurven liefern XYZ-Translation und Y-Drehung; ihre ursprüngliche Schleifenkorrektur verhindert einen Positionssprung am Clipende. Der Adapter rotiert die Translation mit Leons bisheriger Weltrichtung, übergibt sie einmal an `WalkProbe::moveDelta` und übernimmt anschließend die Root-Drehung. Gelenkmatrizen bleiben im Modellraum; die GPU erhält Leons Weltplatzierung separat. Dadurch wird die Bewegung nicht doppelt angewendet.

Bei diesen sieben Bewegungsclips ist die vertikale Root-Translation null. Die eigene SAT-Kapsel bestimmt Leons Y-Koordinate durch Boden-/Stufensnap und löst Wandkontakt/Gleiten. Die originalen Root-Kurven ersetzen die bisherigen frei gewählten Geh-/Laufgeschwindigkeiten: Bei vollem Ausschlag ergeben sich im Mittel etwa 1.741 Einheiten/s beim Gehen, 4.192 beim Laufen und 1.969 rückwärts. Analoger Vor-/Rückwärtsausschlag skaliert `Seq_speed`; Pose und Bewegungsstrecke laufen mit demselben Takt. Beim Drehen im Stand skaliert der seitliche Ausschlag den originalen Drehclip.

Der linke Stick beziehungsweise das D-Pad steuert vorwärts/rückwärts und die Figurendrehung. Während Gehen/Laufen/Rückwärts wird nach der Root-Bewegung die originale Steuerdrehung von `0.0418879` rad pro Schritt angewandt, proportional zum seitlichen Ausschlag; Laufen übernimmt den Quellenfaktor `1.1`. Physisches Rechts steuert mit negativer Y-Drehung. Das folgt der tatsächlichen Originalübersetzung in `pad.cpp`: positiver Stick-X-Ausschlag wird über `-atan2(x,y)` dem irreführend benannten `JOY_SLEFT` und anschließend Key-Bit 4 zugeordnet; dieses subtrahiert die Richtung. Der native Motion-Kern bleibt unverändert. B/Kreis gehalten wählt vorwärts Laufen. Rückwärts plus eine neue B/Kreis-Tastenflanke startet den Schnellwendeclip einmalig; gehaltenes B startet ihn nach Abschluss nicht erneut. Eine Tastenflanke ohne verstrichenen Simulationsschritt bleibt bis zum nächsten Schritt vorgemerkt.

Die Zustandswahl folgt der beabsichtigten Eingabe. Wandkontakt stoppt die akzeptierte Translation, während der Geh-/Laufclip weiterläuft; so kann die Figur nach Loslassen oder Rückwärtsgehen wieder vom Hindernis wegkommen. Die Hokan-Überblendung verwendet acht Schritte für gewöhnliche Wechsel, sieben für Standdrehung und fünf für Schnellwende oder Gehen/Laufen-Wechsel. Bei Gehen↔Laufen wird die normalisierte Sequenzphase übertragen. Diese direkte Phasenabbildung ist eine begrenzte Adapterentscheidung; sie ersetzt die ursprüngliche ungefähre 8-Bit-Phasenübertragung. Die Schnellwende spielt ihre vollständige 23-Frame-Sequenz; die ursprüngliche optionale vorzeitige Eingabeunterbrechung ist noch nicht übernommen.

Start pausiert Bewegung und Animation. Pause, Controller-/Inputwechsel und laufende Raumtransfers verwerfen verstrichene Zeit und vorgemerkte Tastenflanken über `suspendClock`; Pose, Position und ein bereits laufender Schnellwende-Zustand bleiben erhalten. Waffenaktionen werden erst nach Freigabe beider Trigger erneut angenommen, damit ein gehaltener Trigger beim Fortsetzen keinen unerwarteten Schuss auslöst. Die Raumansicht startet automatisch an einem nutzbaren Original-Raumstart aus `Debug/roominfo.dat`, mit geprüftem SAT-Boden und freier Kapsel. Andernfalls wird ein gekennzeichneter SAT-Teststart verwendet. A/Kreuz aktiviert bei abgelegter Waffe die unterstützten Türübergänge; Y/Dreieck schaltet SAT um. Die Schaltfläche „Modellprüfung / Leon-Steuerung“ öffnet die Prüfansicht. Beim Gehen steuert der rechte Stick den Kameraorbit; Figurendrehungen werden als Richtungsdifferenz zusätzlich auf die Folgekamera übertragen. LT/RT verändern den Kameraabstand ausschließlich in der Modellprüfung. Seit M20 spiegelt `InputMapper` den horizontalen rechten Stick nach der radialen Totzone genau einmal: physisches Rechts (`RawInput.rx > 0`, Android Z beziehungsweise RX) wird zum negativen Kamera-/Zieleingang `InputFrame.rx`. Die JNI-Brücke reicht diesen Wert unverändert an beide Verbraucher weiter. Linker Stick, D-Pad und die nach oben positive vertikale Achse bleiben erhalten; auch die Modellprüfung verwendet dieselbe Korrektur.

Bei gehaltenem LT/L2 bleibt Leon stationär und legt die Waffe an. Der rechte Stick steuert danach die originale Zielkurvenmischung und den begrenzten Hüftwinkel; physisch oben ergibt eine nach oben gerichtete originale Handachse. Der korrigierte horizontale Eingang wird direkt als `PlayerActionInput.yaw` verwendet: physisches Rechts ergibt den negativen Eingang und damit einen positiven Hüft-/Figurenwinkel in der ursprünglichen Formel `-x * acceleration * PI / 40000`. Kameraorbit und Waffensteuerung führen keine weitere Stickspiegelung aus. RT/R2 startet den Rückstoß und verbraucht eine Testpatrone. Gehaltenes RT kann alle 14 Simulationstakte den nächsten Schuss auslösen; Loslassen von LT kürzt die anfängliche Rückstoßphase nicht ab. X/Quadrat startet das originale Nachladen. Bei leerem Magazin und vorhandener Reserve löst eine neue RT-Flanke ebenfalls Nachladen aus. Die sichtbare Waffenpose kreuzt Frame 31; der folgende ursprüngliche Objektprüfschritt füllt das Magazin genau einmal auf. Eine volle Waffe lädt nicht nach. Die Testmunition beginnt mit 10 Patronen im Magazin plus 60 frei gewählten Reservepatronen, bleibt bei Türwechseln erhalten und ist kein portiertes Spielinventar.

Kurven-IK und Mischgelenke sind aktiv. Die ursprüngliche Anpassung der Füße an reale SAT-Bodenhöhen ist weiterhin nicht verbunden; der lokale Animationsadapter besitzt keine Terrain-IK. Originale Attach-Camera, vollständige `cPlayer`-Zustände, Waffentreffer/Schaden, Gegner, Schusseffekte, Audio, Inventar, Leitern, Sprünge und Szenariologik gehören nicht zu dieser begrenzten Runtime. Die neuen Waffenaktionen führen keine Gegner- oder Schadenberechnung aus.

## GPU-Darstellung

Die Gelenkmatrizen werden mit der inversen Bindepose kombiniert und als **128 mat4 / 8.192 Byte** in einem GLES3-Uniformblock übertragen. Leons 119 Gelenke liegen innerhalb dieses Budgets; die Waffe besitzt eine getrennte GPU-Instanz mit eigener Palette und eigener Hand-Anbindungsmatrix. Jede Modellecke behält bis zu drei separate Positions- und Normalengewichte. Benachbarte Primitive mit identischer Textur-/Masken-/Farbauswertung werden unter Erhalt ihrer Reihenfolge zusammengefasst. Mesh-VBO, TPL-Texturen und gespeicherte Mips sind statisch; es erfolgt kein vollständiger Mesh-/Texturupload pro Frame. Placement und Kamera verwenden dieselben Weltkoordinaten wie SAT und Raumgeometrie.

Basistextur-Alpha wird wie im bisherigen belegten Materialzweig ignoriert; separate Masken und Materialalpha behalten ihre eigene Funktion. Leons Vorschau erhält einfache gerichtete Beleuchtung. Das ist noch nicht die originale Raumlicht-/Schatten-/Augen- und Gesichtsruntime. Der Actor wird nach den Raumdraws gezeichnet und über den Tiefentest eingebunden; vollständige gemeinsame OT-Sortierung mit dynamischen Modellen steht noch aus. Bei Kameradistanz unter 1.000 Spieleinheiten werden Leon und das Waffenmodell ausgeblendet; die Simulation bleibt aktiv.

GPU-Namen gehören dem aktuellen GL-Kontext. Context-Loss verwirft Namen ohne Löschaufrufe im neuen Kontext; ein normaler Ansichtswechsel löscht die eigenen Ressourcen. Parser- oder Animationsfehler werden als RE4Player protokolliert und führen zur Kapseldiagnose.

## Reproduzierbare Prüfungen

- `tests/animation_tests.cpp`: synthetische Original-BIN-Gewichte, inverse Bindepose, Gelenkkette, 30/60-Hz-Auswertung, Root-Zyklen und Schleifennähte, analoger Takt, Phasenübertragung, einmalige Sequenz sowie ungültige FCV/SEQ-/Skinningdaten. Kein Spielasset nötig.
- `player_motion_tests`: ohne Assets werden Platzierung, fehlgeschlagene Platzierung mit Positions-Rollback sowie kontinuierlicher Root-Kapselkontakt und Rückzug geprüft. `player_motion_tests PL00.drs WEP01.drs` prüft zusätzlich die belegten Originalstrecken, 20 Geh-/Lauf-/Rückwärtszyklen, identische Position/Pose bei 30/60-Hz-Renderfrequenz, analoge Schrittlänge, Schnellwende-Flanke/Abschluss, Suspend sowie Wandblockierung mit anschließender Bewegung vom Hindernis weg.
- `player_action_tests PL00.drs WEP01.drs`: echte Originalposen mit stationärem Anlegen/Zielen/Ablegen, Handrichtung nach oben/unten, Hüftgrenzen, identische Körper-/Waffenposen und Schusszahlen bei 30/60 Hz, 14-Takt-Feuerintervall, Rückstoß trotz LT-Freigabe, Tastenkanten bei Zeit null und Triggerfreigabe nach Suspend. Das Nachladeereignis wird einmalig beim sichtbaren Übergang Waffenframe 31→32 geprüft; 70 Schüsse mit leerem Magazin, automatischem Nachladen und erschöpfter Reserve erhalten die Munitionsbilanz. Türplatzierung erhält die Testmunition; ungültige Eingaben verändern die Pose nicht.
- `python3 tools/test_actor_data.py actor_tool PL00.drs WEP01.drs`: der unabhängige M16-Upstream-Hostvergleich bleibt für die ersten drei Clips erhalten: 18 Posen / 119 Gelenke einschließlich IK und Mischgelenken, maximale Abweichung im erneuten Lauf 0,0006104 Spieleinheiten. Zusätzlich 300 Schritte echter Stand-/Geh-/Lauf-SEQ-Wiedergabe mit Loop und Übergängen sowie unabhängiger BIN-Vergleich aller 27.288 Modellecken. Diese Prüfung ist kein unabhängiger Goldenvergleich aller sieben M17-Clips oder der vollständigen Spielerzustandsmaschine; die Übergangsspur prüft Endlichkeit und Poseänderung.
- Optionaler CMake-Schalter `RE4_BUILD_GL_CHECK=ON`: `actor_gl_check PL00.drs WEP01.drs r300.dat bild.ppm` prüft den tatsächlichen GLES3-Vertexshader per Transform Feedback gegen eine unabhängige Double-Auswertung der Gewichte, danach Framebuffer, Masken/Texturen, Normalenwechsel, GPU-Neuaufbau und 120 bewegte Raumframes. Capture-Varyings werden nur in dieses Host-Testziel gelinkt.
- ASan/UBSan prüfen die native Auswertung separat. Android-JNI und ARM64-Code werden durch den regulären Android-Build geprüft.

M16 wurde auf dem Gerät nach `clean assemble` vom Nutzer bestätigt: Leon und seine Animationen sehen korrekt aus. Das Log `Eingefügter Text(20261003-221022).txt` enthält den geladenen Spieler mit 119 Gelenken und Originalanimationen. M17 wurde ebenfalls auf dem Gerät bestätigt: `Eingefügter Text(20261003-225513).txt` belegt Root-Motion-Zustände und erfolgreiche Transfers r301→r300 sowie r300→r301. Die neuen M18-Waffenaktionen benötigen den anschließenden Gerätetest; Host-/GPU-Prüfungen sind davon getrennt dokumentiert.

## M19 — Anschluss der Schussereignisse und Gegneranimation

Das folgende Gerätelog `Eingefügter Text(20261003-234535).txt` bestätigt M18-Anlegen, Ziel-/Schuss-/Nachladeaktionen, 22 Schüsse und die erhaltene Munitionsbilanz. Der Nutzer bestätigt Leons Originalanimationen. Diese Rückmeldung belegt noch keinen M19-Gerätetest.

`LeonPreview` veröffentlicht Schussereignisse nur nach gelöstem Rückstoßframe im vorhandenen 30-Hz-Schritt. Der neue optionale Tick-Beobachter synchronisiert einen original animierten Ganado und dessen vorgemerkten Schaden; bestehende Bewegungs-/Waffenaufrufe behalten ihren Takt. `AnimationRig` ergänzt einmalige `MotionGetSpeed`-Auswertung für Root-Aktionen, Original-SEQ-Free-Abfrage und eine besessene validierte Spiegeltabelle. Die bisherigen M16/M18-Samplingregeln bleiben erhalten.

EM12 verwendet eine unabhängige GPU-Instanz mit 34 Gelenken, vier gemeinsam gebundenen Modellen und drei Texturen. Rohmatrizen versorgen Originaltrefferzonen; die Shaderpalette bleibt fürs Skinning zuständig. Der vollständige gemeinsame GPU-Neuaufbau umfasst Leon, Pistole und Gegner. Grenzen, Quellen und Prüfungen: [ENEMY-ENCOUNTER.md](ENEMY-ENCOUNTER.md).
