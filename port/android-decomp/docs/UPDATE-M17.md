# M17 — Originale Bewegungswege, Raumstarts und erster Türlauf

Version **0.17.0-dev**, versionCode **17**. ARM64, API 21, GLES 3.0; Paket und Projektordner bleiben unverändert.

## Ausgangsbefund

Das Gerätelog `Eingefügter Text(20261003-221022).txt` bestätigt M16 mit 119 Gelenken, sieben Leon-Modellen, elf Texturen und Stand/Gehen/Laufen. Der Nutzer bestätigt die korrekte sichtbare Animation nach `clean assemble`. Der ältere Drahtkapselbericht beruhte nach Nutzerangabe auf dem zuvor nicht sauber neu gebauten Stand. Eine Behebung sämtlicher zuvor gemeldeter Durchsichten wird daraus nicht abgeleitet.

## Was sich ändert

- Der echte FCV-Root-Weg steuert die SAT-Kapsel statt der bisherigen festen 2.600/5.200 Testgeschwindigkeit. Originale Rückwärts-/Dreh-/Quickturn-Clips kommen aus derselben bereits importierten WEP01-Datei. Analoge Eingabe verändert Pose und Weg gemeinsam. Wandkontakt blockiert den Weg, ohne den beabsichtigten Animationszustand fortlaufend neu zu starten.
- Debug-Raumstarts und Ausrichtung stammen aus der originalen `Debug/roominfo.dat`. Eine Startposition wird nur nach begrenzter Bodenkorrektur und freier Kapsel übernommen; sonst ist der SAT-Teststart ausdrücklich bezeichnet.
- Die unveränderten originalen AEV-Türen 0 zwischen r301 und r300 lösen native Raumwechsel mit den im Türrecord gespeicherten Zielpositionen und Ausrichtungen aus. Andere Türen werden gelesen, aber außerhalb dieser belegten Teilruntime als noch nicht portiert angezeigt.
- Raumdateien werden im Hintergrund gegen den vorhandenen Originalkatalog geprüft und entpackt. Das Ladeergebnis ist eine CPU-Nachricht; erst ein Renderframe mit aktuellem GL-Kontext übernimmt es. Fehlende/veränderte Daten, ungültiger Einstieg oder gescheiterter GPU-Upload behalten den bisherigen Raum. Veraltete Antworten einer anderen Activity oder Ladeanfrage werden verworfen.
- Start pausiert die Simulation. Ein neu angelegter GL-Kontext erhält innerhalb derselben laufenden Session den aktuellen Raum, Leons Pose, Kamera und laufende Ladeanfrage. Die Sessionkennung wird bei einer Activity-Neuanlage durch Bildschirmrotation erhalten; ein neuer Aufruf oder Prozessneustart beginnt erneut am gewählten Einstieg. SAT-/Modellwechsel behalten die Kameraparameter und den UV-Zeitstand.

Details und belegte Grenzen stehen in `PLAYER-PREVIEW.md`, `WALK-PROBE.md`, `ROOM-FLOW.md` und `VALIDATION.md`. Originaldaten werden weiterhin ausschließlich aus den eigenen importierten Discs gelesen; im ZIP sind keine Spielassets enthalten.

## Auf dem Gerät prüfen

1. ZIP über den vorhandenen Ordner **RE4-Android-Decomp** entpacken und komplette enthaltene Dateien ersetzen. `bash ./gradlew clean assembleDebug lintDebug` ausführen und installieren. Keine Deinstallation oder erneute Disc-Extraktion ist für einen vollständigen vorhandenen Import erforderlich.
2. Einen geprüften Raum öffnen. Leon startet automatisch. Linker Stick/Steuerkreuz: vor/zurück, links/rechts drehen. B/Kreis halten: laufen. Zurück + B/Kreis neu drücken: schnelle 180°-Drehung. Der rechte Stick bewegt weiterhin die Kamera.
3. „Türtest: originalen r301-Einstieg laden“ wählen. Am Originalstart zur nahen Tür drehen/gehen, bis **A/Kreuz: Tür nach r300** erscheint. A/Kreuz drücken. Der Wechsel muss `RE4Flow: PASS` protokollieren; der Zieleinstieg kommt aus der originalen AEV. Dort erneut zur Tür drehen, den Prompt abwarten und zurück nach r301 wechseln. Gehaltenes A erzeugt keine neue Press-Flanke.
4. Start drücken, kurz pausieren, Start erneut drücken. Zusätzlich App in den Hintergrund legen und zurückkehren. Nach einer GL-Neuanlage innerhalb derselben Activity muss der zuletzt betretene Raum erhalten bleiben.
5. X/Quadrat: Modellprüfung, Y/Dreieck: SAT, Select: Normalen. LT/RT: Kameraabstand. Android-Zurück beendet die Ansicht.

Für den nächsten Gerätetest sind **RE4Flow, RE4Player, RE4Walk, RE4Scene** sowie eventuelle Fatal-/ANR-Zeilen relevant. Der neue Host-/Android-Build wurde geprüft; M17 ist bei Auslieferung noch nicht auf dem Nutzergerät getestet.

## Noch fehlender Spielbetrieb

Die neue Steuerung ist an die belegten Originalclips angelehnt, weiterhin eine begrenzte native Spielerteilruntime. Die SAT-Kapsel ersetzt noch nicht `cPlayer`/`cSatMgr`. Fuß-Terrain-IK, Leiter/Kletteraktionen, originale Playerkamera, vollständige Szenarioflags und Türanimationen fehlen. Insbesondere liegt r300s normaler Debug-Start auf einer anderen Höhenebene als die Prüftür; deshalb führt der ausdrückliche Türtest über den originalen r301-Start.

Waffenmodell/Zielen/Kampf, Gegner, Inventar, Audio, Speichern und vollständige dynamische Raumdarstellung sind weiterhin offen. M17 ist ein begehbarer, animierter Raumwechsel-Prototyp und noch kein spielbares vollständiges RE4.
