# R100 – Korrekturen 5

Dieses Paket ergänzt den vollständigen M37-Stand einschließlich **R100-Korrekturen 4**. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden Projekt zusammenführen und die Dateien ersetzen. Es enthält vollständige geänderte Dateien; Originalspieldaten und Signaturschlüssel sind nicht enthalten. Die Prüfsummen stehen in `FILES-R100-KORREKTUREN5.sha256`.

Paketname `com.ast.re4decomp`, Version `0.37.0-dev`, Versionscode `38`, ARM64 und Android ab API 21 bleiben erhalten.

## Abgleich der gemeldeten Fehler

| Punkte | Befund und Änderung |
| --- | --- |
| 1, 13: LOAD-/SAVE-Abfragen und Steuerung | Bestätigungsblöcke werden anhand der tatsächlichen Originalglyphen, Breiten, Leerzeilen und Auswahlmarker zentriert. Zeichnen und Berührungsabfrage verwenden dieselbe Geometrie. Die originale Exit-Abfrage hat vertikale YES/NO-Zeilen und verwendet Rauf/Runter; die horizontale Overwrite-Abfrage verwendet Links/Rechts. Dies gilt für DPAD, HAT und Stick im Titelmenü und beim Speichern. Die Zentrierung bleibt nach dem Skalieren auf unterschiedliche Bildschirme erhalten. Informationsmeldungen ohne Auswahl behalten ihre Quellposition. |
| 2: Originaltreue des LOAD-Bildschirms | Gruppen, Glyphen, Platzzeilen, Überdeckung, Auswahlrichtung, Scrollkurven und Übergänge wurden erneut mit `card.cpp` und den tatsächlichen CARD-/MES-Ressourcen verglichen. Das Android-Speichermedium bleibt eine virtuelle Karte mit 20 Plätzen; es emuliert nicht sämtliche Zustände, Fehler und beide Anschlüsse der GameCube-Memory-Card. Der gesamte LOAD-Bildschirm wird deshalb nicht als vollständig zu 100 % identisch ausgewiesen. |
| 3: Seitliches Flackern beim Filmübergang | Beim Wechsel von der Sofdec-Wiedergabe zur Spielaktivität wird die zusätzliche Android-Fensteranimation unterdrückt. Die vorhandenen ursprünglichen Film-/Ereignisblenden bleiben maßgeblich. Die Kontrolle des tatsächlichen Betriebssystemübergangs auf dem Android-Gerät steht aus. |
| 4: Spiegelung im Auto | Car-, Fenster-, Leon- und Spiegelmodelle verwenden ihre originalen OT-Typen und Tiefenreihenfolgen. Der Rückspiegel wird am ursprünglichen Pass 0x10 zwischen den Modell- und Weltgruppen gezeichnet. Der reflektierte Leon bleibt im besonderen RTT-Pfad und wird nicht zusätzlich als normale Weltfigur gezeichnet. Die Spiegeloberfläche `obm3010f` erhält den in beiden r120-Callbacks ausdrücklich gesetzten OT-Typ 1. Die originale CORE-Beleuchtung wird auch an den Spiegelklon gebunden. |
| 5: Tiefenschärfe | Die bislang fehlenden `cLightEnv`-Parameter FocusZ, FocusLevel und FocusMode werden gelesen. Neben expliziten EVD-Fokuskurven wird nun auch der originale Filter01-Typ-0-Pfad aus den ENV-Daten ausgeführt. Die Passparameter wurden durch Ausführen des eingefrorenen Originalcodes gegengeprüft. |
| 6, 8: Schwarzer Leon und wechselnde Figurenhelligkeit | Die ursprüngliche CORE-GI-Stufe war zuvor nur für einen Teil der Ereignisfiguren verfügbar. Sie wird nun gemäß der originalen SetOm-Präfixtabelle an Leon, die übrigen entsprechenden Figuren und ihre Modelle gebunden, einschließlich Spiegelklon. Die Texturkoordinatenskalierung berücksichtigt das ursprüngliche Modellflag. Fremde GLES-Sampler aus vorausgehenden Pässen werden vor den Schauspielertexturen entfernt. Außerdem multipliziert der Lichtkanal die Materialfarbe vor seinem Clamp, wie GX. |
| 7, 9: Nebel, Wald und Horizont | Nebelrechnung und Sichtbarkeitsferne sind verschiedene Originalfunktionen. Die Sichtbarkeitsferne folgt jetzt `Fog.End * (1 - farRatio) + 1`; bei ausgeschaltetem Nebel bleibt sie 1.000.000. Die Modellprüfung verwendet die originalen sechs Sichtflächen, die Modellkugel und den Toleranzwert. Der ursprüngliche OT-6-Sonderpfad bleibt erhalten. Die GX-Projektion behält unabhängig davon Near=100 und Far=1.000.000. Es werden keine erfundenen globalen Nebel- oder Helligkeitsverstärker eingesetzt. |
| 10: Abgeschnittene Todessprache | Die Position des Sprachkanals stammt aus dem tatsächlich animierten Ganado-Part 0. Während der EVD-Kameraschnitte ohne `SPF_SE_CALC` werden die laufenden gegnerischen Stimmen nicht erneut auf die entfernte Ereigniskamera abgeschwächt. Ein Test spielt die tatsächliche lange Todessample-Gruppe über den produktiven JNI-Mixer vollständig ab und vergleicht jeden PCM-Block einschließlich des Endes mit ununterbrochener Wiedergabe. |
| 11: Gebückter Leon nach der Fensterszene | Das natürliche Ereignisende übergibt unmittelbar an den Original-Funkablauf, ohne ein normales Raumbild dazwischen. `PlSetEyeMode(1)` wird vor MotionSetCore und dem verborgenen ersten `SceSleep(1)` gesetzt. Die erste sichtbare Pose entspricht ARC0x79 nach diesem ersten Schritt; das Funkgerät wird nach dem weiteren originalen Warteablauf bei Tick 22 angehängt. Die Cleanup-Reihenfolge stellt Griff und Augenmodus wieder her. |
| 12: Heller Rand an Herbs | Gegenstände verwenden ihre originalen Modell-/Lichtvolumen, Part-Bezüge und Specular-Ressourcen. Die gemeinsamen Material- und Samplerfehler sind korrigiert. Aus dem verfügbaren Quell-/Datentest lässt sich jedoch nicht sicher ableiten, ob der berichtete dünne Rand auf dem Gerät vollständig verschwindet; dies bleibt ein visueller Prüfpunkt. Es wurde kein pauschaler Alpha-Cutoff erfunden. |
| 14: Schreibmaschinenanimation | Textur-, Eintritts-, Scroll- und Schreibkurven verwenden getrennte Zeitgeber. Die Texturanimation läuft weiter, wenn Dateiarbeit beginnt oder endet. Die Original-Schreibkurve läuft auch in der Abschlussphase weiter, anstatt durch eine bereits beendete Dateioperation wieder auf einen anderen Frame zu springen. Der spezielle Pfeil-/Papier-Texturablauf bei Scroll nach oben wird berücksichtigt. Speichern startet den ursprünglichen Schreibton 0x2d und verwendet die Kurvenzeitpunkte für 0x2e; LOAD/DELETE spielen diese Schreibgeräusche nicht. |
| 15: Funkeln | Das Original bestimmt den Effekt durch die ITA-Daten bzw. den erzeugenden Item-Pfad und seine Flags. Es funkeln weder grundsätzlich alle festen Gegenstände noch ausschließlich Wertsachen. In den tatsächlichen 24 festen r100-ITA-Einträgen haben die Krähen-Drops Spinel und Handgranate ausdrücklich einen Effekt; die gewöhnlichen festen Einträge besitzen dort Effekt 0. Der erfundene Ersatzwert 1 für effektlose feste Items wurde entfernt. Modellgebundene Effekte verwenden die tatsächlichen Parts des Item-Modells. |

## Quellen und technische Grenzen

Verglichen wurde der vorhandene eingefrorene Originalquellstand mit den bereitgestellten Disc-1-Ressourcen, insbesondere `game/card.cpp`, `game/sscrn.cpp`, `game/light.cpp`, `game/trans.cpp`, `game/filter01.cpp`, `game/event.cpp`, `game/mirror.cpp`, `st1/r100.cpp` und `st1/r120.cpp`. Für das Funkeln wurden zusätzlich die tatsächlichen ITA-/Item-/CORE-Effektressourcen verwendet.

r100, r101 und r120 enthalten verschiedene Fog-/Ambient-/Filterdaten; ein r101-Lichtprofil wird daher nicht pauschal für andere Räume verwendet. Für r100 ergibt die tatsächliche Fog.End/farRatio-Kombination eine Sichtbarkeitsferne von 42.743,8. Die vorbereiteten Ereignis-Lichtdaten der Autofahrt gelten weiterhin pro Kameraschnitt.

Der Buildfehler durch weit in der Zukunft liegende Quellzeitstempel ist ebenfalls berücksichtigt: Die vorhandene Normalisierung läuft jetzt auch vor `externalNativeBuildClean`. Dadurch kann Ninja beim Clean nicht wiederholt seine Builddateien regenerieren. Ein vollständiger Android-Clean-Build wurde erfolgreich ausgeführt.

Dieses Paket überträgt die oben bezeichneten Originalfunktionen. Die vollständige GX-Passverwaltung einschließlich aller Tiefenlöschungen, alle Schatten-/TEV-Kombinationen und die komplette originale Ereignis-Audioverwaltung sind damit nicht als vollständig portiert erklärt. Der Audiofix übernimmt die relevante Berechnungssperre; er behauptet keine vollständige Umsetzung aller ursprünglichen Event-Audio-Fades. Die Einschränkungen der vorhandenen Spieler-, Kampagnen- und Speicherprofile gelten weiter.

## Prüfungen

| Prüfung | Ergebnis |
| --- | --- |
| ARM64 `assembleDebug`, `assembleDebugUnitTest`, `assembleDebugAndroidTest`, `lintDebug` | Erfolgreich mit den endgültigen Produktionsdateien; die Test-APKs wurden gebaut, nicht auf einem Android-Gerät ausgeführt |
| Vollständiger Android-Clean-Build mit Zeitstempelkorrektur | Erfolgreich; 75 Aufgaben. Die letzten Funk-/Spiegel-Detailkorrekturen wurden anschließend mit allen oben genannten Android-Aufgaben erneut gebaut |
| Native CTest-Grundsuite | 43 von 43 erfolgreich |
| Raumlicht-/Sichtbarkeitsvertrag | 58 Prüfungen erfolgreich |
| Filter01 gegen instrumentierten eingefrorenen Originalcode | 6.464 Prüfungen erfolgreich, einschließlich ENV-Typ 0 und EVD-Typ 1 |
| Filter01 über produktiven GLES-Pfad | 13.347 Prüfungen erfolgreich |
| GX-Lichtkanal, CORE-GI, Filterhistorie und Schatten auf GLES | 100 Prüfungen erfolgreich; halbierte Materialien bei überhellem Licht ergeben den Originalwert statt des früheren zu frühen Clamps |
| Tatsächlicher r120-Rückspiegel auf GLES | 86 Prüfungen erfolgreich; Originalkameraschnitte 0/2/6, OT-/Passgrenzen, Fremdsampler, Pause und EGL-Neuanlage |
| Originale Aufnahme-/IDM-Modelle auf GLES | 1.100 Prüfungen erfolgreich; 58 tatsächliche IDM-Szenen und das tatsächliche Münzmodell |
| Tatsächliche Item-/ITA-/Effektdaten | 4.508 Prüfungen erfolgreich |
| Haus-/Nebenereignisse über produktiven JNI-/GLES-Pfad | 213 Prüfungen erfolgreich; Original-SHD verändert 224.972 Pixel und wird nach Neuanlage identisch wiederhergestellt |
| Tatsächliche Nebenereignisse und vollständige Quellzeitlinien | 14.541.503 Prüfungen erfolgreich |
| Natürliches Truck-/Fensterende bis Funkgerät und vollständige Todessprache | 995 Prüfungen erfolgreich; erste Pose gegen frische Original-FCV und vollständiger PCM-Vergleich |
| Durchgehender produktiver Ablauf mit ersten Außen-Ganados und Falle | 2.960 Prüfungen erfolgreich; Einleitung, Haus, erster Gegner, natürliches Fenster-/Funkende, drei Außen-Ganados, Drop-/GPU-Neuaufbau und 240 laufende Spielschritte nach der Fallenexplosion |
| Java-Speicherablauf | 33 Prüfungen erfolgreich |
| Bestätigungsgeometrie mit tatsächlichem Font/MES5015/MES5017/MES5019 | 28 Prüfungen erfolgreich; einschließlich 1920x1080 und 2400x1080 |

Die erweiterten Testdateien sind enthalten. Tests mit Originaldaten benötigen weiterhin die bereitgestellten Disc-Dateien; diese werden nicht in das Paket kopiert. Der Native-JNI-Ablauftest ist `r100_render_runtime_check`, der Spiegeltest `m37_event_mirror_gl_check`, der Lichttest `m32_lighting_gl_check`. Ihre Programme geben die benötigten Argumente aus. Die Java-Geometrieprüfung erwartet die exportierten Original-Font-/MES-JSON-Daten; der Speicherablauf erwartet einen gültigen Original-Header als Fixture.

Ein visueller Vergleich auf dem Android-/Adreno-Gerät fehlt weiterhin. Insbesondere Autofahrt/Spiegelbild, Nebel am Waldrand, Kräuterrand und Schreibmaschinenbild müssen dort mit dem Original kontrolliert werden. Ein erfolgreicher Build und Host-GLES-Tests belegen keine vollständige visuelle 1:1-Gleichheit des Spiels.
