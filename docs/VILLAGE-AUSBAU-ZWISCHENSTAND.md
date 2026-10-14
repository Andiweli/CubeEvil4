# Dorf-Ausbau – getesteter Zwischenstand auf Fix18

Dies ist **keine Fertigstellung des gesamten Dorfpfads**. Es ist ein kumulatives Austauschpaket mit den bisherigen Fix18-Dateien und den unten genannten Änderungen. Keine Originalassets liegen bei. Die vorhandenen akzeptierten Inventardateien sind unverändert enthalten.

## Umgesetzt und mit Originalassets geprüft

| Raum | Neue Anbindung | Noch offen |
|---|---|---|
| r103, Farm am Tag | 5 automatisch freigegebene EM12-Ganados aus ESL00; sechs EM26-Rinder und sechs EM28-Hühner; originale Rack-Reichweite; ITA-Beutezuordnung; Schreibmaschinen-AEV; unterstützte SST/EAR-Effekte | Freier EM21-Hund; spezielle Arbeits-/Start-KI; Leichenmodelle; Brunnen-/Schatzlogik, Medaillons und Schrankereignisse; Dateien und Raum-BGM |
| r111, Dorf nachts | 8 automatische EM17-Ganados aus ESL01; Modelltyp12 einschließlich flaggesteuerter Haube, Originalpaletten und breiteren Beinkapseln; 5 EM2A-Fallen samt Spielerreaktion und Fragmenten; original vorzerstörte Fenster0/19; alle drei Rack-Reichweiten und Position17 | Vollständiger Regen-/Blitz-/Donnerablauf; Parasiten und fehlende Waffen-/Zubehörvarianten; weitere Sonder-KI; dedizierte Filter-/Shimmer-/Esp07-Effekte |
| r113, Farm nachts | 8 automatische EM17-Ganados aus ESL01; 8 EM2A-Fallen samt Reaktion und Fragmenten; originaler Rack-Bereich; unterstützte SST/EAR-Effekte | Ashley-Folgen/Verstecken/Schulterklettern und Türöffnung nach r11c; Brunnen, Schränke, Medaillons, Datei; vollständiger Gewitterablauf, Parasiten/Sonder-KI |
| r11a, Sumpf nachts | 11 automatische EM12-Ganados aus ESL01; Originalpositionen, RTP-Routen, Beute/Audio und unterstützte SST/EAR-Effekte | Spezifische Raumskripte und noch nicht unterstützte Wasser-/Filter-/Shimmer-Effekte |

Die Gegner verwenden die bereits vorhandenen gemeinsamen Ganado-Kampf-, Wahrnehmungs-, Bewegungs- und Trefferabläufe. Das beweist **keine vollständige Portierung sämtlicher Em10-Sonderroutinen**. Inaktive und bereits gesetzte ESL-Einträge werden nicht willkürlich freigegeben.

Schreibmaschinen werden aus AEV-Typ8 gelesen statt nur in r100 hartcodiert. Die gespeicherten Gegner-/Tier-/Fallen-Todesflags verwenden die aktuell ausgewählte originale ESL-Liste. Beim Listenwechsel bleiben Tag und Nacht getrennt erhalten; Laden und Continue verwenden ebenfalls die gespeicherte Liste.

## Restlicher Kampagnenumfang

Alle 29 vorhandenen r1xx-IDs und 66 authored AEV-Verbindungen wurden inventarisiert. Die Inventur ist **kein Nachweis eines spielbaren Storydurchlaufs**. Die statischen Räume und unterstützten allgemeinen Interaktionen waren schon vorhanden.

Für die vollständige angefragte Strecke fehlen weiterhin eigene Laufzeiten, insbesondere:

- r106/r104/r107/r105: Felsereignis/QTE, Luis-/Mendez-/Kapitelabläufe und spezielle Kampf-/Spawnscripte.
- r102/r112 sowie r108/r118/r117: raumspezifische Übergangs-, Friedhofs-, Kirchenrätsel- und Ashley-Ereignisse.
- r109/r119, r10a/r10b/r10c/r11b/r10e/r10d: El Gigante, Del Lago, Boot/See/Wasserfall, Schlüsselfortschritt und Händlerabläufe.
- r11c/r11d/r11e/r10f/r11f: Luis-Kabinenschlacht, Ashley- und Routenereignisse, Checkpoints, Gondel und Mendez-Boss bis zum Übergang nach r200.
- Vollständige Originalzuordnung von Schätzen, abschießbaren Funden, Medaillons, Dokumenten und ereignisabhängigen Gegenständen in diesen Räumen.

Keine dieser Lücken wird durch offene Türen, vereinfachte Ersatzgegner oder gesetzte Questflags als erledigt ausgegeben. Besonders r113→r11c ist deshalb weiterhin kein regulär vollständig spielbarer Fortschritt.

## Validierung

- Native Release-Build mit `-Wall -Wextra -Werror` erfolgreich, einschließlich echter Android-JNI-Quelldatei und GLES-Renderer im Hosttest.
- `village_next_check`: alle32 neuen automatischen Ganados,12 Farmtiere,13 Nachtfallen, vorzerstörte r111-Fenster, Rack-Anbindung und SST/EAR-Bänke mit Originalassets geprüft.
- `village_next_runtime_check`: alle vier Räume durch die Produktions-JNI geladen, tatsächliche GPU-Uploads,100 gemeinsame Welt-/Spieler-Ticks, Raum-Effekt-Ticks und Encode/Decode des Originalspeichers samt korrekter Tag-/Nacht-Todesflags geprüft.
- Bestehende Raumfluss-, Raumstatus-, Enemy-Spawn- und Enemy-Encounter-Verträge:4/4 bestanden.
- Bestehender K17-Fix6-Source-Test:116327 Prüfungen bestanden; bestehender originaler Hund-/Fallentest:286386 Prüfungen bestanden.
- Zwei ältere Regressionstests bleiben rot: `inventory_case_tests` erwartet eine inzwischen überholte Kofferregel; `k17_fix6_runtime_check` scheitert an seiner alten Porträt-Pixelreferenz. Die zugehörigen Inventar-/Porträtdateien wurden für diesen Ausbau nicht geändert. Diese Fehler werden nicht als bestanden ausgegeben.
- Kein APK-Build und kein Android-Gerätedurchlauf. Ein vollständiger Kampagnendurchlauf bleibt ausstehend.

## Einbau

Den Ordner `RE4-Android-Decomp` aus dem ZIP über das gleichnamige Projekt kopieren und enthaltene Dateien ersetzen. Es sind vollständige Dateien, keine Diffs. Das Paket enthält kumulativ Fix18 und diesen ausdrücklich unvollständigen Ausbau.

Neue Prüfprogramme: `tools/test_village_next.cpp` und `tools/test_village_next_runtime.cpp`. Der Source-Test erhält den Originaldateiordner `files`; der JNI-/GLES-Test den Fixture-Ordner mit `files` und `decoded`.

Die Dateien unter `docs/village-next-audit/` sind Originaldaten-Inventuren; deren historische Build-Metadaten stammen vom vorhandenen Auditwerkzeug. Der tatsächliche neue Teststand ist oben angegeben.
