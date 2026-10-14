# K15 – Dorfverbindungen, Dorfkampf und Test-Cheats

Android **0.37.15-dev**, VersionCode **48**, auf dem vollständigen K14-Quellstand mit Dorf-Inventur.

## Einspielen und erwartetes Verhalten

Das Austauscharchiv enthält vollständige neue/geänderte Dateien relativ zum vollständigen K14-Projekt. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem eigenen Projekt zusammenführen, vorhandene Dateien ersetzen, Gradle synchronisieren und `app` kompilieren. Alternativ das vollständige Quellstandarchiv in einem eigenen Ordner öffnen. `local.properties` und private Signierdateien werden nicht mitgeliefert.

Ein erneuter DVD-Import ist für dieses Update nicht erforderlich. Voraussetzung ist der bereits vollständige englische Disc-1-Import. Die englische Installation und die bestätigten LOAD-/SAVE-/Titelkorrekturen bleiben bestehen. Kein japanischer Import, keine Sprachwahl.

Über **START** regulär beginnen und durch r100 ins Dorf r101 spielen. Der Dorfkampf startet aus den tatsächlichen Szenarioflags; es gibt keinen Ganado-/r101-Testknopf mehr. Im Dorf werden Gegner, Alarm, Nachschub und Hausereignis an die Originaldaten angebunden. Nach erfüllter Kampfbedingung folgt das originale Glockenereignis mit Rückkehr ins Spiel und Funkkontakt.

**SELECT** öffnet das englische Menü „Test cheats“. Dort „Infinite health“, „Infinite ammo“ und die einzeln gelisteten Originalwaffen wählen. A bestätigt, B/START/SELECT schließt das Menü. Das Spiel pausiert währenddessen. Die Einstellungen gelten für die laufende Sitzung und überleben CONTINUE sowie Raumwechsel; sie werden nicht als eigene Cheatflags in den Originalspielstand geschrieben.

Das Waffenmenü gibt echte Originalgegenstände in den Koffer und überschreibt keine vorhandenen Gegenstände. Bei vollem Koffer meldet es fehlenden Platz. Bereits vorhandene unterstützte Waffen werden ausgerüstet, statt dupliziert. Enthalten sind die 23 einzeln wählbaren Basiswaffen einschließlich Spezialfiguren-Varianten, drei Granaten und sieben Zubehörteile. Kombinierte Waffen-/Zubehör-IDs sind keine zusätzlichen erfundenen Grundwaffen.

**Wichtig für Waffentests:** Die vorhandene Kampfengine unterstützt derzeit Handgun, Punisher, Shotgun und Messer. Weitere gelistete Waffen/Granaten werden korrekt in den Koffer gegeben; ihre vollständigen Kampfsteuerungen sind noch nicht portiert. Die englische Ergebnismeldung kennzeichnet diese Grenze direkt im Cheatmenü. Zubehör wird als Zubehör gemeldet.

## Sinnvolle Geräteprüfung

1. Regulär bis r101 spielen; SELECT öffnen, beide Cheats an- und ausschalten. Für einen längeren Dorf-Test kann „Infinite ammo“ eingeschaltet werden.
2. Alarm auslösen, Haus betreten, nach der Haussequenz bewegen und Türen, Fenster, Leitern sowie die aufnehmbare Schrotflinte prüfen. Der Schrankwechsel muss sofort mit der richtigen Laufkollision wirken.
3. Gegner bekämpfen und die Nachschubgruppen beobachten. Die Glocke wird gemäß `r101.cpp` bei mehr als 14 gezählten Toten ausgelöst; nach dem Hausereignis bei mehr als 17. Alternativ gelten die Originalgrenzen von mehr als 11.700 bzw. 13.500 aktiven 30-Hz-Takten (ungefähr 6:30 bzw. 7:30 Minuten, zusätzlich Originalverzögerungen). Ereignisse und Pause zählen nicht als Kampfzeit.
4. Turm betreten und den Originalangriff mit Brandflaschen prüfen. Danach die Glockensequenz bis zur Rückkehr und zum Funkkontakt durchlaufen lassen; den normalen Ausgang zur Farm benutzen.
5. „Infinite health“ ausschalten, sterben und CONTINUE YES wählen. Es muss der aktuelle Kontrollpunkt erscheinen, ohne Titelmenü und ohne LOAD-Platzauswahl. Nach Tod im Dorf kehren die Gegner zum Kontrollpunktzustand zurück.
6. Zusätzlich erreichbare Türen und Rückwege prüfen. Eine registrierte Verbindung hebt Originalschlüssel und Szenariosperren nicht auf.

Hier wurde kein Android-Gerät mit Controller durchgespielt. Erfolgreiche Originaldaten-/JNI-/GLES-Prüfungen und ein APK-Build ersetzen diese Geräteprüfung nicht.

## Umgesetzte Laufzeiten und Originalbezug

- `village_flow.cpp`: alle 66 literal geprüften AEV-Türdatensätze für 29 tatsächliche r1xx-Räume, einschließlich Rückwegen, zwei r10e-Partwechseln und der Grenze r10f → r200. r110/r114/r115/r116 existieren in dieser Stage-Tabelle nicht. r120 gehört zum geskripteten Intro und hat keine normale AEV-Tür.
- Originalziele, Richtungen, Parts, Schlüssel und Raum-/Szenarioflags werden beibehalten. `stage.cpp::GetEmListNo` bestimmt die ursprüngliche Tages-/Nacht-ESL-Liste. Fehlgeschlagene Transfers behalten Szene und zuvor vorhandenen Spielzustand.
- `r101_scenario.cpp`: erster Funkkontakt, 11 aktive Ausgangsgegner, 43 echte EM15-Besitzer für Anfangs-/Nachschub-/Haus-/Rückkehrgruppen, Alarm, CAM4/CAM5-Nachschub, Hausflag, Glockenbedingungen und Nachlauf. Inaktive Datensätze werden nicht alle zugleich als Gegner gespawnt.
- EM15 verwendet seine Originalmodelle, Körper-/Kopfvarianten, Waffenbindungen und Bewegungsdaten. Der Kettensägen-Ganado verwendet echte EM15-Angriffe und gekoppelte Leon-/Gegnerbewegungen; fehlgeschlagene Gegenwehr kann den regulären Todesablauf auslösen.
- Die vollständigen vorhandenen EVD-Daten `r101s00`, `r101s21`, `r101s30` laufen mit Originalkameras, Actor-Daten und Ton. Die Haussequenz schaltet Schrank, Fenster und Leitern mit neu gebundener dynamischer Kollision um.
- `village_bottles.cpp`: Original-Turmverzögerung, AEV-Zielbereich, Flaschenmodell/-textur, Flugparabel, SAT-Aufprall, Flammen und Schaden. Keine nachgebaute Flaschengeometrie.
- `game.cpp::gameDiedemo` ruft für YES `GameContinue(0)` auf. Das Original öffnet dabei keine LOAD-Auswahl. Der Android-Dialog hält YES nun während des asynchronen Ladens fest; der native Weg stellt den gesicherten Raum-, Spieler-, Inventar- und Szenariozustand wieder her.

Originalquelle: unveränderter Upstream-Commit `9b76ff003081d6ece8b838453fb8310743a972c7`, insbesondere `upstream/re4/src/st1/r101.cpp`, `src/game/stage.cpp`, `src/game/game.cpp`, `src/em15/`, `src/pl0f/pl0f.cpp`. Die Disc-Inventur steht in `docs/village/`; `K15-TRANSITION-STATUS.tsv` kennzeichnet alle 66 Verbindungen und ihre Sonderfälle.

## Konkrete verbleibende Grenzen

Die Registrierung aller Raumverbindungen ist kein vollständiger Port sämtlicher Dorf-Kampagnenskripte. Andere Dorfkämpfe, Begleiter, Händler, Bosskämpfe, bewegliche Plattformen und die übrigen Story-Ereignisse sind weiterhin gesonderte Laufzeiten.

**Bootsverbindung r11b → r10d:** Das Original fährt mit `pl0f` in r10d ein. Dessen Originalankunft und Editorstart haben keinen tragenden statischen SAT-Boden. Das Update erfindet dort keinen Boden und versetzt Leon nicht an eine andere Küste; ein solcher Transfer wird mit Erhalt des bisherigen Raums abgebrochen. Bootsteuerung und Ferry-Abläufe müssen vor regulärer Begehbarkeit dieses Sonderwegs noch portiert werden. Die beiden r10e-Partdatensätze sind registriert; der vollständige ursprüngliche Ferry-Ablauf ist ebenfalls noch offen.

r101 ist bis zum Glockenabschluss angebunden, aber nicht in jeder Präsentations-/KI-Einzelheit originalgleich: kontinuierlicher Fernglasmodus, Kapitel-Titel-Einblendung, vollständige Spezial-KI/Wege aller EM15-Varianten sowie sämtliche Gore-/GX-/Licht- und Filterdetails bleiben offen. Die vorhandene Inventaroptik wurde hier nicht weiter überarbeitet.

## Beide DVDs

DVD 1 enthält die Dorf-/Schlossabschnitte. DVD 2 enthält hauptsächlich Inselabschnitt/Stage 3, Finale und zugehörige Räume, Ereignisse, Modelle und Ton sowie gemeinsame/zusätzliche Daten (unter anderem r22c). `gameStageInit` wechselt Stage 3 auf Disc 2; Stage 1/2 verwenden Disc 1 mit der ausdrücklichen r22c-Ausnahme. Für das vollständige GameCube-Spiel werden beide DVDs benötigt. Für den aktuellen Dorfkampf genügt Disc 1. Der Importer führt die englischen und gemeinsamen Daten beider Discs zusammen.

Die Quellarchive enthalten keine vollständigen DVD-Abbilder, lokalen SDKs, Build-Caches, APKs oder privaten Signierschlüssel.

## Prüfergebnisse

Die endgültigen Ergebnisse und Dateiprüfsummen stehen in `K15-PRUEFUNGEN.json` und `FILES-K15.sha256`. Die Tests sind in `tools/test_k15_village.cpp` und `tools/test_k15_runtime.cpp` enthalten und erwarten legal bereitgestellte Originaldaten.
