# K9 – Kräuterkanten, Fenster, Intro-Skip und LOAD

Stand: 7. Oktober 2026. Android-Version **0.37.9-dev**, Versionscode **42**.

K9 korrigiert die vier zuletzt gemeldeten Punkte. Verglichen wurden die bereitgestellten Bilder, der vorhandene Originalquellstand und die tatsächlichen Disc-Ressourcen. Das Ersatzdateipaket enthält auch den aktuellen Stand der vorherigen K5–K8-Korrekturen.

## Kräuter mit hellbraunem Rand

`setItemObj` in `upstream/re4/src/game/obj19.cpp` behält den ursprünglichen Modelltyp `ot_type=0`. Damit werden Gegenstände im Modellpass OT `0x0d` nach dem Raumhintergrund gezeichnet. Der Port hatte die Item-Modelle dem früheren Hintergrundpass zugeordnet. Die halbtransparenten Blattkanten wurden deshalb teilweise mit der Hintergrund-Löschfarbe vermischt, bevor die Landschaft an dieser Stelle gezeichnet war.

Die Item-Modelle verwenden jetzt die ursprüngliche Reihenfolge. Die tatsächliche I4-Blattmaske, ihre Zwischenwerte, der Alpha-Vergleich und die Materialbeleuchtung bleiben erhalten. Die Korrektur entfernt keinen Rand durch einen erfundenen Alpha-Schwellwert. Der GLES-Test prüft die Kantenmischung mit der tatsächlichen grünen Kräutermaske gegen eine unabhängige Berechnung.

## Blick durch intakte und zerstörte Fenster

Glasmodelle und gewöhnliche Waldsprites gehören im Original gemeinsam in den nach Tiefe sortierten Weltpass OT `0x11`. Im Port wurden die Waldsprites gesammelt nach den Scheiben gezeichnet. Dadurch konnten Blätter den bereits gemischten Scheibenhintergrund überdecken; das sah wie ein abgeschnittener Teil der Scheibe aus.

K9 sortiert die betroffenen Raumobjekte und Sprites gemeinsam in den ursprünglichen Tiefenabschnitten. Für Sprites bleibt der ursprüngliche Positionspunkt maßgeblich, auch wenn Animationsdaten die sichtbaren Eckpunkte versetzen. Bildschirm- und feste Effekte behalten ihre getrennten Zeichenphasen. Die Scheiben werden außerdem nach den Figuren gezeichnet.

`cEmWindow::init` setzt den Glas-OT-Typ, behält aber den ursprünglichen Modell-Tiefenmodus `z_mode=0`. Der zuvor erzwungene andere Tiefenmodus ist für diese Scheiben und die zugehörige zerstörte ET00-Variante korrigiert. Die originale Scheibenmaske besitzt selbst unterschiedliche Deckkraft im oberen und unteren Bereich; diese Texturdaten werden weiterhin verwendet.

Geprüft wurden die tatsächlichen r100-Fenstermodelle in intakter und zerstörter Form sowie die GLES-Schichtung von Sprites vor und hinter Glas. Maßgebliche Quellen: `game/emwindow.cpp`, `model.cpp`, `trans.cpp`, `trans_ot.cpp` und `esp.cpp`.

## Intro direkt überspringen

Während der anfänglichen Logo- und Bildfolge führen **START, A oder X** unmittelbar ins Hauptmenü. Die ursprüngliche Menüanimation beginnt dabei bei Frame 0 und blendet das Menü einmal ein. Derselbe Tastendruck bestätigt keinen Menüeintrag. Wiederholte Tastendrücke starten die Einblendung nicht erneut.

Der automatische Ablauf ohne Tastendruck bleibt erhalten. Die Prüfungen decken jeden Zeitpunkt der anfänglichen 663 Originaltakte ab. Der direkte Zielzustand entspricht dem Titelmenü-Eintritt aus `titleNintendo`; die jederzeitige Belegung mit START/A/X setzt die gewünschte Bedienung des Ports um.

## LOAD im Titelmenü

Der Bildschirm verwendet weiterhin die tatsächlichen CARD-Grafiken, Schriftzeichen, Platzzeilen und Animationskurven. K9 korrigiert drei konkrete Abweichungen:

- Der Eintritt der nahen Schreibmaschinenfläche läuft über ihre eigenen 30 Takte. Er endet nicht mehr zusammen mit dem 14-Takt-Eintritt der Dateiliste.
- Die beiden ursprünglichen Rückfrage-Wurzeln und ihre untergeordneten Grafiken bleiben ohne aktive Meldung verborgen. Der Dialoghintergrund verdeckt dadurch keine unteren Speicherplätze mehr.
- Der sichtbare Ausschnitt umfasst Y=60 bis Y=420 des 640×480-Arbeitsbildes. Das entspricht der Aufteilung des beigefügten Referenzbildes und füllt den jeweiligen Bildschirm. Anzeige und Touch-Auswahl verwenden dieselbe Transformation; Rückfragen bleiben mittig. Der gemeinsame SAVE-Bildschirm nutzt denselben Ausschnitt.

Die Schreibanimation beginnt nur beim Speichern. LOAD hält den betreffenden Hebel in seiner Ruheposition, abgesehen von der ursprünglichen Scrollbewegung. Die laufende Darstellung und eine begonnene Operation behalten ihre Zeitgeber auch bei einer Wiederherstellung der Titelaktivität.

Die Speicherfunktion bleibt eine Android-Karte mit 20 Originalplätzen. Die vollständige Hardwareverwaltung beider GameCube-Memory-Card-Anschlüsse ist weiterhin nicht emuliert. Quelle für die Bildschirmabläufe: `upstream/re4/src/game/card.cpp`, insbesondere `CardID::init`, `up_down` und die getrennten SAVE-/LOAD-Abläufe.

## Prüfungen

| Prüfung | Ergebnis |
| --- | --- |
| Vollständiger Android-Clean-Build | Erfolgreich, ARM64 mit NDK 28.2.13676358 |
| Abschließender Android-Build mit allen Produktionskorrekturen | Erfolgreich |
| Native CTest-Vertragstests | 43 von 43 bestanden |
| Startablauf einschließlich direktem Skip | 1.343 Prüfungen bestanden |
| Titelansicht, Bildschirmtransformation und Filmlifecycle | 86 Prüfungen bestanden |
| Speicherablauf einschließlich getrennter Animationen und Dialogsichtbarkeit | 40 Prüfungen bestanden |
| Rückfragen mit tatsächlichen Font-/MES-Daten und Touch-Geometrie | 46 Prüfungen bestanden |
| Tatsächliche Titel-/Options-/CARD-/Font-Ressourcen | 691 Prüfungen bestanden |
| Native und Java-Speicherformate | 78 bzw. 56 Prüfungen bestanden |
| GLES-Distanzdarstellung, Kräutermaske und Glas-/Spriteschichtung | 6.585 Prüfungen bestanden |
| Einleitung, Haus, Funkübergang, Außenkampf und Sprengfalle über produktiven JNI-/GLES-Pfad | 4.102 Prüfungen bestanden |
| APK-Signatur und Ausrichtung | Vorgeschriebenes Zertifikat; v1 und v2 sowie 16-KiB-Ausrichtung erfolgreich geprüft |

Die Bild- und Rendererprüfung erfolgte mit den bereitgestellten Spieldaten auf dem Host-GLES-System. Die neue APK wurde hier nicht auf deinem Android-Gerät ausgeführt. Insbesondere die endgültige Darstellung der Kräuterkanten und der Fenster muss dort noch bestätigt werden. Die Prüfungen belegen die beschriebenen Korrekturen und erklären nicht das gesamte Spiel für vollständig 1:1 portiert.

Die Originaldaten für die Java-Rückfragen werden bei `tools/test_host.sh` automatisch als Testfixtures exportiert, wenn `RE4_TEST_DISC_FILES` auf die bereitgestellten Disc-Dateien zeigt und der native Titeltest mitgebaut wurde. Originalspieldaten werden nicht mitgeliefert.

## Paket und Einbau

`RE4-Android-Decomp-R100-Korrekturen9.zip` enthält **72 vollständige Code- und Testdateien**, diesen Bericht und `FILES-K9.sha256`. Es führt die benötigten Ersatzdateien aus K5–K9 im aktuellen Stand zusammen; Patchskripte sind für den Einbau nicht nötig.

1. ZIP entpacken.
2. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden Projektordner zusammenführen und die entsprechenden Dateien ersetzen.
3. Projekt synchronisieren und neu bauen oder die signierte `RE4-M37-K9.apk` installieren.

Der zusätzliche `RE4-Android-Decomp-K9-Quellstand.zip` enthält den vollständigen Projektstand. Die Quellpakete enthalten weder Keystore und Passwörter noch lokale SDK-Pfade, Build-Verzeichnisse oder extrahierte Disc-Dateien. Die Installation mit dem vorhandenen Signaturzertifikat und das Ersetzen der Quelldateien löschen keine Spielstände.
