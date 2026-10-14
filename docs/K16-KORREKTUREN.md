# K16 – Waffen ausrüsten, Fernglas, Verletzung, CONTINUE und Ganado-Stimme

Android **0.37.16-dev**, VersionCode **49**, auf dem vollständigen K15-Quellstand. Grundlage sind die bereitgestellten Originaldaten und der unveränderte Upstream-Commit `9b76ff003081d6ece8b838453fb8310743a972c7`.

## Einspielen

`RE4-K16-Austauschdateien.zip` enthält vollständige neue oder geänderte Dateien gegenüber K15. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem vorhandenen Projekt zusammenführen und Dateien ersetzen, Gradle synchronisieren und `app` kompilieren. Alternativ `RE4-Android-Decomp-K16-Quellstand.zip` als vollständiges Projekt öffnen.

Ein erneuter DVD-Import ist nicht erforderlich. Das Update nutzt die vorhandenen englischen Disc-1-Daten. Eigene `local.properties`, Signierdateien und Spielstände bleiben lokal. Die bestätigten Titel-, LOAD-/SAVE-, Sprach- und Kollisionskorrekturen sind weiterhin enthalten.

## Die vier gemeldeten Fehler

| Meldung | Ursache und Korrektur |
| --- | --- |
| Cheatwaffen lassen sich im Koffer nicht ausrüsten | Die Java-Auswahl und Leons nativer Waffenwechsel unterstützten bisher nur drei Feuerwaffen. Der Koffer erhält seine Ausrüstbarkeit jetzt aus der Original-Waffentabelle. Weitere gelistete Waffen und Granaten besitzen echte Modelle, Paletten, Griffe, Tragebewegungen und eine Darstellung in Leons Menüpose. Der Waffenwechsel erneuert die GPU-Daten der Griffe und Waffen. |
| Fernglassequenz fehlt in r101 | Das Original bietet im AEV7-Bereich **LOOK** an. Ein neuer A-Tastendruck startet beim ersten Mal `r101s00`; danach folgt das steuerbare Fernglas. Die englische Originalkamera, Winkelgrenzen und der Zoom von 45° bis 3° sind angebunden. Die Maske, Anzeigen und Animationen stammen aus ROOM_ARC27/28. B beendet den Blick; eine Gegnerentdeckung beendet ihn ebenfalls. Wiederholtes LOOK überspringt die bereits gesehene Einstiegssequenz. |
| Scheinbar 0 HP ohne Humpeln, danach CONTINUE zurück ins Android-Menü | Der Logcat zeigt nach der ersten Explosion **1/1200 HP**. `LifeDownSet2` kann bei einem starken Treffer aus mindestens 501 HP einen Lebenspunkt erhalten. Diese Originalregel bleibt bestehen. Die bisher fehlenden beschädigten PL00-Bewegungskurven werden jetzt unter 800 HP verwendet, einschließlich der originalen Ausnahme für den Infinite Launcher. Ein tödlicher Treffer mit tatsächlichen 0 HP beginnt die Todesbewegung sofort. Der CONTINUE-Absturz entstand durch `setPixels()` auf einem unveränderbaren Bitmap; das Hintergrundbild ist jetzt beschreibbar. YES führt zum vorhandenen nativen Kontrollpunkt-Neustart. |
| Einfrieren bei Ganado-Entdeckung | Der Logcat nennt `Ganado: invalid original voice slot`. EM15-Modelltyp 11 verwendet gemäß `Em15Set` die Stimmenbank 1, wie EM12-Typ 1. Diese Zuordnung fehlte. Die originale Bank wird jetzt auch für Typ 11 ausgewählt; ungültige Typen und Slots werden weiterhin geprüft. |

**Das Fernglas startet im Original nicht automatisch nach dem Funkkontakt.** Nach dem Gespräch zum Aussichtspunkt vor dem Dorf gehen, zum Dorf ausrichten und bei **LOOK** A drücken. Linker Stick oder D-pad verändert den Blick, rechter Stick auf/ab den Zoom. B oder die Android-Zurück-Taste beendet die Ansicht. Währenddessen bleiben die Ganados aktiv.

**CONTINUE YES öffnet keine LOAD-Platzauswahl.** `game.cpp::gameDiedemo` ruft `GameContinue(0)` auf. Wiederhergestellt wird der gesicherte Kontrollpunktzustand mit Raum, Leon, Koffer und Szenarioflags. Die laufenden Test-Cheats bleiben erhalten.

## Waffenumfang

Die 33 vorhandenen Cheat-Einträge werden weiterhin einzeln angeboten. Messer und sieben Zubehörteile sind keine ausrüstbaren Feuerwaffen: Das Messer wird über L1 benutzt; Zubehör bleibt ein Koffergegenstand. Die übrigen 25 Waffen und Granaten sind ausrüstbar. Zwei davon teilen ihre Basisprofile mit anderen Originalvarianten; der Laufzeittest prüft jedes wählbare Gegenstands-ID separat.

Die generierte Tabelle enthält 38 originale Trageprofile, einschließlich kombinierter Varianten für die spätere Zubehöranbindung. Sie wird mit `python3 tools/generate_weapon_carry.py` aus `item.cpp`, den Waffenmodulen und ihren Bewegungszuordnungen reproduziert. Beispielsweise wird der Infinite Launcher aus dem tatsächlich verwendeten Archiv `wep46.drs` geladen, nicht aus dem gleichnamigen logischen Datenplatz 30.

**Der vollständige Kampf mit zusätzlichen Waffen ist weiterhin offen.** Handgun, Punisher, Shotgun und Messer besitzen die vorhandene Kampfsteuerung. Andere Waffen können ausgerüstet und im Spiel sowie im Koffer betrachtet werden; ihre Schuss-, Ziel-, Nachlade-, Wurf- und Spezialabläufe werden durch dieses Update nicht vollständig portiert. Das englische Cheatmenü nennt diesen Umfang. Krausers Bogen auf Leon ist ein Testprofil mit Original-Bogendaten; das Original besitzt dafür keine reguläre Leon-Menüpose.

## Geräteprüfung

1. SELECT öffnen, eine zusätzliche Waffe hinzufügen, im Ingame-Koffer eine andere Waffe ausrüsten und wieder zur Cheatwaffe wechseln. Modell, Hände und Menü-Leon prüfen. Für den Kampf Handgun, Punisher oder Shotgun verwenden. Bei vollem Koffer muss die Platzmeldung erscheinen, ohne Gegenstandsverlust.
2. Nach dem ersten Funkkontakt in r101 den Aussichtspunkt mit **LOOK** benutzen. Einstiegssequenz ansehen, Blick und Zoom bewegen, mit B beenden und LOOK nochmals benutzen. Die zweite Benutzung öffnet direkt die Fernglasansicht. Nach dem Dorfalarm darf LOOK nicht mehr angeboten werden.
3. **Infinite health ausschalten.** Eine schwere Explosion kann noch 1 HP übrig lassen. Nach Ende der Trefferanimation muss Leon verletzt laufen. Ein weiterer tödlicher Treffer muss zur Todesanimation und zum Original-Continue-Dialog führen.
4. Im Todesdialog YES bestätigen. Der Kontrollpunkt muss direkt ins Spiel zurückführen, ohne Android-Startmenü oder LOAD-Auswahl. Zusätzlich NO einmal prüfen; dieser Zweig führt regulär zum Titel zurück.
5. Mehrere Ganados Leon entdecken lassen, insbesondere die weibliche EM15-Variante. Bewegung, Entdeckungsruf und anschließender Dorfalarm müssen weiterlaufen. Anschließend Hausereignis und Glockenabschluss wie bei K15 prüfen.

Ein Android-Gerätetest mit deinem Controller steht hier aus. Die automatischen Prüfungen verwenden die Produktions-JNI, echte Originalarchive und eine GLES-Hostoberfläche; sie ersetzen kein vollständiges Durchspielen auf dem Gerät.

## Prüfung und Originalbezug

Die endgültigen Ergebnisse stehen in `K16-PRUEFUNGEN.json`; `FILES-K16.sha256` listet die neuen und geänderten Dateien mit Prüfsummen.

- `tools/test_k16_runtime.cpp`: jede wählbare Cheat-ID, Waffenwechsel durch die Produktions-JNI, Original-Menüporträts, LOOK-Bedingungen, Fernglas-EVD und Kamera, Wiederholung und Abbruch, Haus/Wellen/Glocke, Explosion/Verletzung/Tod, Kontrollpunkt-Neustart und EM15-Typ-11-Entdeckung.
- Die verletzte Gehpose wird für sämtliche 119 Gelenkmatrizen mit einer unabhängig aus den originalen PL00-FCV-/SEQ-Einträgen aufgebauten Referenz verglichen.
- `tests/enemy_feedback_tests.cpp`: alle 19 Stimmenslots von Typ 11 gegen Typ 1 sowie die weiterhin ungültigen Eingaben.
- `tools/test_blast_combat.cpp`: bestehende Original-Explosions- und Trefferregeln. Der FCV-Posevergleich verwendet den originalen Modus für vorgegebene Augenbewegungen; normales Blinzeln und Blickwandern überschreiben sonst die Gesichtsbewegung nach `MotionMove`. Der r101-Laufzeittest behält Leons normale Augensteuerung bei.
- `tools/test_k15_village.cpp`: originale Raumverbindungen, Schauspieler, Nachschub und vollständige r101-Ereignisse.
- Java-Prüfungen: HUD-/Continue-Zeitablauf, Titel-/Demo-Verhalten, Optionen, Stimmenende, Koffer-Navigation, Ladezustand und Controller-Routing.

Der native Android-ARM64-Build verwendet NDK 28.2.13676358 und API 21. Alle Android-Java-Dateien und die Ressourcen wurden gegen SDK 35 kompiliert. Ein vollständiger Gradle-`assembleDebug`-Lauf konnte hier wegen nicht auflösbarer Maven-Abhängigkeiten nicht abgeschlossen werden; ein APK- oder Lint-Erfolg wird für K16 nicht behauptet.

Maßgebliche Quellen: `upstream/re4/src/st1/r101.cpp` (`r101_Event00`), `src/game/cam_extra.cpp` (`CameraBinocular`, `IdBinocular`), `src/game/cam_ctrl.cpp` (`HoldBinocular`), `src/game/item.cpp`, `src/game/read.cpp`, `src/Sscrn/ss_model.cpp`, `src/game/pl_dmg.cpp`, `src/game/pl_class.cpp` (`dmMotCk`), `src/game/game.cpp` und `src/em15/em15_set.cpp`.

Der übrige Dorf-Ausbau behält die dokumentierten K15-Grenzen, insbesondere die Boots-/Ferry-Abläufe und weitere Kampagnenskripte. Dieses Update behauptet keine vollständige Originalgleichheit aller Räume oder der gesamten Kampfengine. Die Quellpakete enthalten keine vollständigen DVD-Abbilder, privaten Schlüssel, SDKs, Build-Caches oder APKs.
