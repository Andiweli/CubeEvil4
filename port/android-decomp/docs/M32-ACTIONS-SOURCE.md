# M32 – Fenster, Objektleitern und Gegenstände: Originalabgleich

Maßgeblich ist der eingefrorene Upstream `9b76ff003081d6ece8b838453fb8310743a972c7`.

## Fensterereignis

`src/game/emwindow.cpp::ExeWindowEvent` verwendet in den drei Richtungen die Originaldateien `pl00537.fcv`, `pl00538.fcv`, `pl00536.fcv`, die Startpositionen und `MotionSetCore`-Flags `0x201`. Der Ereigniszähler bricht die Scheibe bei 15/16/13 **vor dem nächsten MotionMove**. Die sechs Sprach-/Landungs-/Schrittgeräusche bleiben an den dort angegebenen Zählern. Es gibt keinen künstlich vorgezogenen Bruch und keinen linear interpolierten Ersatzsprung.

Diese FCVs enthalten außerdem die originale Ereigniskamera. Sie wurde zuvor verworfen, während die Verfolgerkamera der springenden Figur folgte. `AnimationRig` liest jetzt auf ausdrückliche Anforderung die Attach-Camera-Kanäle mit dem Original-MotionMove. `CameraAttachedToMotion::move` transformiert Typ1 mit der aktuellen Spielermatrix und Typ2 mit der beim Start gespeicherten Matrix. Die Flagge `0x200` unterdrückt die normale Kamerawandkorrektur; sie unterdrückt **nicht** die Transformation durch `ac->p_mat`. Leon stellt die so berechneten Augen-/Zielpositionen, Rollwinkel und FOV dem Renderer bereit. Nach der Aktion wird die Kamera freigegeben. Das Appending anderer Ereignisclips darf ihre aktive Historie nicht zurücksetzen.

`SetBreakEsp` liefert die EST-Auswahl für den tatsächlichen Bruch: Ereignisgröße1 mit Richtung0/1/2 startet EST0/1/3; ein kleiner Waffentreffer EST6/7; ein Schrotflintentreffer EST0/1. Der Modellwechsel, ETC-Bit0, Ton und die EFM-Fragmentanforderung gehören zur selben Aktion. `et00.eff`, `et07.eff` und `et25.eff` liefern die ursprünglichen Efm05-Modelleinträge; ihre Glas-/Holzgeometrie und TPLs kommen aus dem Original-CORE. Es werden keine Glasscherben aus neuen Dreiecken erzeugt. Das späte EFM-Generatorrecord der Fallrichtung hat `Set_time=24` und bleibt bis dahin ausstehend. Generatorintervall und dessen einmaliger/fortlaufender `R_inter` werden aus `espgen00.cpp` übernommen. Die Gradumrechnung in `esp_efm.cpp` verwendet ausdrücklich `3.14/180`.

Nicht Teil dieses begrenzten Fragmentbackends sind die an Leon gebundenen Staub-/Sprite-EST2/4/8, sämtliche Spezialfenster der übrigen Räume und vollständige Original-Hausereignisse. Die bestehenden r100/r101-Profile bleiben die belegte Interaktionsabdeckung.

## Objektleitern

Die beweglichen ETS-Leitern ET06/ET08 verwenden `obj13.cpp::plobjLadderClimb`, nicht die feste Turmleiter `player.cpp::pl_R1_Ladder`. Leon bindet die vier Original-ETM-Clips separat. Die sechs-/acht-Sprossenprofile, Clipwechsel, Originaltöne, Hokan5 und Flaggen1/5 werden von `ladder_motion` geliefert. Beim Typ0-Ausstieg wird nach Zähler23 vor der Rootbewegung der echte SAT-Boden geprüft und ein darunter liegender Spieler mit 0.9/0.1 angeglichen; beim Ende erfolgt die Originalkorrektur nach oben. Normale Kollisionen werden nach erfolgreicher Landungsprüfung wieder freigegeben. Ein inzwischen blockierter Zielboden führt zum geprüften Ausgangspunkt zurück.

Die feste Neun-Stufen-Turmleiter bleibt ein eigener Ablauf. Leiterscenery-Körper werden mit den Türen/Regalen zusammengeführt, damit deren Kollisionsrebuilds sie nicht wieder entfernen.

## Gegenstände

`src/game/sce_at.cpp::sceAtGetItem` spielt bei der normalen Gegenstandsaufnahme keine Leon-Aufhebe-FCV. Der Originalablauf ist `SceUpCutStart`, Item-Exam-Ansicht und Meldung/Ja-Nein-Auswahl, danach `ItemMgr.get` beziehungsweise `PutInCase` oder Abbruch, anschließend Flag-/Kamera-/Kontrollfreigabe. Eine frei erfundene Bückanimation wäre deshalb keine originalgetreue Korrektur.

`beginItemExam`/`endItemExam` bilden den Spieleranteil dieses Ablaufs ab. Während der Bestätigung/Koffertransaktion bleiben Pose, Root, Eingaben und Angriffsuhr eingefroren. Der Inventareigentümer entscheidet, ob der Gegenstand übernommen wurde. Zustimmung und Abbruch geben dieselbe Spielersteuerung wieder frei; eine gehaltene Waffentaste kann keine unbeabsichtigte Aktion auslösen. Die Originaldarstellung des rotierenden Gegenstands und der vollständige Original-Subscreen sind davon getrennte Render-/UI-Systeme.

## Unabhängige Prüfung

`tools/test_m32_actions.cpp` vergleicht sämtliche Kamera-Eye/Target-Samples der drei tatsächlichen Fenster-FCVs gegen einen unabhängigen Big-Endian-Keydecoder und Hermite-Sampler. Zusätzlich prüft es eingefrorene Gegenstandsaktionen, alle dreizehn auf Disc vorhandenen Fenster-Bruchsequenzen, verzögertes `Set_time24` und deterministische Fragmentwiederherstellung. Die Testdaten müssen lokal vom Benutzer bereitgestellt werden; sie liegen nicht im Patch.
