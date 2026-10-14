# M35 – Fensterzugriff, Wassergeräusche, Zielwechsel, Grubensprünge und Bildschirmmenüs

**0.35.0-dev / VersionCode 36**, Paket `com.ast.re4decomp`, ARM64, API 21+, GLES 3. Das Änderungs-ZIP über den vollständigen **M34-Stand** entpacken; der Ordner bleibt `RE4-Android-Decomp/`. Es enthält vollständige geänderte/neue Dateien, keine Diffs. Vorhandene Quellen werden ersetzt, neue ergänzt; keine bestehenden Projektdateien müssen gelöscht werden. Anschließend mit Android Studio oder `./gradlew assembleDebug` bauen. SDK-/NDK-Pfade und Signiereinstellungen des eigenen Projekts bleiben lokal.

M35 enthält außerdem drei aus den bereitgestellten Original-Sofdec-Filmen erzeugte MP4-Dateien: opening, demo0 und demo1. Sie ersetzen keine Logos oder Menübilder durch Platzhalter. Das Konvertierungsskript und `assets/movies/manifest.json` dokumentieren Quelle, Hashes, Streams und Encoding. Stereo ist die erste Standardspur, Mono die zweite; H264/AAC ist eine Android-Ausgabe des ursprünglichen MPEG1/ADX-Materials und nicht bitidentisch. Es wird weder eine Vollkopie der Discs noch ein Signierschlüssel mitgeliefert.

## Die fünf gemeldeten Fehler

| M34-Befund | M35-Verhalten / Originalbezug |
| --- | --- |
| Leiter durch geschlossenes Fenster umwerfen | Leiter-Standposition, Weg und obere Reichweite prüfen die gemeinsame aktuelle Fensterkollision. Intaktes oder nur angeschlagenes Glas blockiert den Zugriff; zerbrochenes Glas und Zugang von derselben Seite bleiben möglich. Die ursprüngliche ActBtn-Leiterpriorität bleibt bestehen. Der zusätzliche physische Reichweitenschutz ist im Audit ausdrücklich als Android-Anpassung beschrieben. |
| Keine Geräusche beim Gehen oder Hineinspringen ins Wasser | Leons echte SEQ-Ereignisse werden genau einmal pro Originaltakt verbraucht; originale FSE-Flächen und Wasserhöhe bestimmen Schritt-/Landecues 30–35 statt trockener 0–5. r10a wurde an 21 tatsächlich begehbaren Wasserstellen mit echten Original-Soundbanks geprüft. Es gibt keinen erfundenen Dauersplash beim Überschreiten einer Grenze. Wellenstörungen und vollständige Spritzgrafiken fehlen weiterhin. |
| Blick zum Boden wird beim Waffenanlegen übernommen | Der neue Waffen-/Messer-Zielzustand beginnt mit Pitch 0 wie `CameraControl::getCameraPitch()` im Original. Horizontaler Kamerabezug und anschließendes rechtes Stick-Zielen bleiben erhalten; invertiertes Zielen wirkt nur im Waffenpfad. |
| Größere Gruben in r10c nicht überspringbar | `JumpFall` verwendet ursprüngliche SAT-Kanten, Schulterabfragen, Höhen- und Landebedingungen sowie PL90/92 und PL91/93. A/Kreuz startet die angebotene Aktion „Über die Grube“. Acht tatsächliche r10c-Durchgänge einschließlich Gegenrichtungen bestehen. Nicht als Sprung markierte oder bodenlose Gruben werden nicht pauschal freigegeben. |
| Koffer, Schlüssel/Wertgegenstände und andere Ingame-Seiten nur 4:3 | Alle Ingame-Seiten erhalten den echten Viewport. Rahmen und Listen nutzen zusätzlichen Platz; Gegenstände und Leon behalten einheitliche Skalierung. Auswahl, Touch-Inverse, Dialoge und native3D-Modellfenster benutzen dieselbe Geometrie, auch bei 20:9, 21:9, quadratischen und Hochformat-Viewports. Vollständige Original-ID-/GX-Menüanimationen fehlen weiter. |

## Neuer Spielstart und erster Kampf

Nach vorhandenem Discimport startet der originale Nintendo-/Warnungs-/CAPCOM-/Sofdec-/Dolby-Ablauf und das frische Titelmenü mit START, LOAD und OPTIONS. Authored Grafiken, Hierarchie, Kurven, negative Startzeiten, Texturframes, Originalschrift und Hilfetexte werden aus title.dat, option.dat, save_e.dat, CORE und Memcard gelesen. Titelmusik und Originalcues laufen über einen eigenen Mixer. Die ursprünglichen Zeit-/Abbruchgrenzen, alternierenden Demo-Filme, Optionen und Öffnungsfilm sind angebunden. Der Raumkatalog bleibt über die Entwicklungsansicht erreichbar.

START führt nach dem Öffnungsfilm in den begrenzten r100-Kampagneneinstieg. Hier hat Leon reguläre1200 HP, ursprüngliche Startpistole und10+20 Patronen; der Erkundungsmodus behält HP∞. Lebenspunkte, Munition und gewählter Schadensrang bleiben bei Raumrekonstruktion erhalten. Der originale AEV-Bereich 0A aktiviert den ersten manuellen EM12-Eventgegner mit 500 HP und den ursprünglichen Nach-s03-Positionen. Vor Freigabe wird kein sichtbarer/angreifender Testgegner eingesetzt. Der linke Originalgriff, die echte Axt, gespiegelte FCV/SEQ-Kurven, Waffenpunkte, EAT-Sichtprüfung, Originalschaden und Leons Treffer-/Todesreaktionen sind verbunden. Der Tod setzt den ursprünglichen Status nach 73 Takten, während die Collapse-Animation noch weiterläuft. Interior-ETC-Synchronisierung überschreibt dabei keine Szenarioflags mehr.

**Dies ist noch kein vollständiger Originalstart oder vollständig spielbarer Originallevel.** Der r120-Polizeiwagen-EVD-Abschnitt, s03/s20, Wagen-/Fallen-/Ereignissteuerung, die sieben folgenden Gegner und vollständige Gruppen-/Routen-KI fehlen. LOAD zeigt die Original-Leerliste mit 20 Plätzen und Exit-Abfrage; es besitzt noch keinen CardSave/CardLoad-Backend. DPLII-Encoding sowie vollständige Rumble-/Quick-Knife-TypeII-Spielsteuerung sind nicht umgesetzt. Diese Optionen dürfen nicht als Nachweis vollständiger Hardware-/Spielgleichheit verstanden werden.

## Alle Leon-Aktionen und alle Räume

Die vollständige Quelleninventur erfasst91 Dateien,1820 Definitionen und54 Dispatchtabellen. Aktuell sind 128 ausdrücklich begrenzte Adapter dokumentiert; 1692 Definitionen bleiben unübertragen oder unbestätigt. Das ist kein 100%-Nachweis und kein sinnvoller Prozentsatz für Spielbarkeit. Der Audit benennt alle Dispatchslots und fehlenden Systeme. Die neu nachgebildeten Einzelabläufe sind gegen Quellcode und Originaldaten geprüft, nicht automatisch sämtliche Leon-Zustände, Funde, Gegner oder Raumereignisse.

Grundlegende LIT-Cut0-Beleuchtung und Nebel sind weiterhin raumübergreifend angebunden, nicht nur r101/r400:127 Archive wurden ausgelesen,123 sind durch den vorhandenen Parser darstellbar. Allgemeine Licht-/Kamera-Cut-Wechsel, vollständiges GX/TEV, Spezialshader, Filter, Schatten und ereignisabhängige Effekte bleiben offen. Die M34-Wasser-/See-Korrekturen werden beibehalten.

## Prüfungen und Log

Der bereitgestellte Logcat `Eingefügter Text(20261005-204422).txt` enthält keinen beobachteten RE4-FATAL-/nativen Crash-/ANR-Eintrag. Der Launcher beendet den Prozess per Force-stop. Das beweist weder ein fehlerfreies Gerät noch beseitigt es allein die zuvor gemeldeten Ladeprobleme. Der M34-Fokus-/UI-Ladepfad bleibt erhalten.

- Sauberer ARM64-Android-Build `clean assembleDebug lintDebug` erfolgreich; finaler Build nach Asset-/Quellenabschluss ebenfalls erfolgreich. Lint:0 Fehler/0 Fatal, 25 Warnungen.49 Java-JNI-Methoden stimmen mit den APK-Exports überein. ELF-LOAD und APK-Zipausrichtung bestehen 16 KiB-Prüfungen. Paketversion 36/0.35.0-dev bestätigt; Filmassets entsprechen ihren aktuellen Hashes und enthalten keine temporären Dateien.
- Alle 36 nativen CTest-Gruppen bestanden; aktualisiertes `tools/test_host.sh` mit kanonischem CMake/CTest statt veralteter Einzellinklisten vollständig bestanden.
- Tatsächliche Produktions-JNI-/GLES-Prüfung:100 Checks für Originaltitel/-audio, Kampagnenstartausrüstung, AEV-Aktivierung, Axtdarstellung, Szenarioflag-Erhalt, Pause, exakte EGL-Wiederherstellung und Trennung vom Erkundungsmodus.
- Leon:4727 Checks, acht echte r10c-Sprünge; Fenster/Leitern:9093 Checks; FSE/Wasser: 194 Checks über 127 Räume/1927 Flächen; unverändertes SEQ/FSE/footSeCheck-Quellenorakel:1.286.419 Vergleiche.
- Erstkampf:413 Checks; Original-MotionMove/IK/SEQ:134 vollständige Axtposen, maximale Pose-/Rootabweichung 0; Waffenmatrixabweichung0,00024414 Originaleinheiten. GLES:14 Checks,973 sichtbare Axt-/Griffpixel sowie exakte Pause-/Kontextbilder.
- Titel-/Options-/Load-/Font-Assets:446 Checks, darunter415 gültige Texturen. Java:18 Startfluss-/17 Optionsverträge; Menülayout 6872 Checks und80 Navigationstests. Original-Asset-Menü-/Titelbilder und Axtdarstellung wurden auf dem Host visuell geprüft.
- Helligkeit 34–114:567 Original-Filterkoeffizienten exakt und69.382 GLES-Checks. Die sieben Tap-GLES-Ausgabe approximiert den ursprünglichen VI-Filter; Standard 64 bewahrt das vorhandene Bild. Die Canvas-Titelausgabe nutzt derzeit Helligkeitsgain.

**Kein neuer Test auf deinem Android-/OUYA-Gerät wurde durchgeführt.** Host-GLES, Source-Orakel und Android-Build belegen ihren jeweiligen begrenzten Umfang. Android-MediaPlayer, Controller, Lifecycle, Bildschirmaufteilung und die gemeldeten Spielsituationen müssen zusätzlich auf dem Zielgerät geprüft werden.

Einzelheiten: [Leon](M35-PLAYER-ACTIONS.md), [Quelleninventur](PLAYER-ACTION-COVERAGE-M35.md), [Leiterzugriff](M35-LADDER-PANE-REACH.md), [Wasser/Display](M35_ENVIRONMENT_AUDIO_DISPLAY.md), [Menüs](MENU-M35.md), [Startablauf](STARTUP-M35.md), [r100](R100-M35.md).
