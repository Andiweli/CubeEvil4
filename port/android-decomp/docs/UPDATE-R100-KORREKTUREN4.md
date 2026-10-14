# R100 – Korrekturen 4

Dieses Paket ergänzt den vollständigen M37-Stand einschließlich **R100-Korrekturen 3**. Es enthält die vollständigen geänderten und neuen Dateien. Den Ordner `RE4-Android-Decomp` mit dem bestehenden Projekt zusammenführen und die enthaltenen Dateien ersetzen. Die Prüfsummen stehen in `FILES-R100-KORREKTUREN4.sha256` im Projektordner des ZIPs.

Anwendung: `com.ast.re4decomp`, Version `0.37.0-dev`, Versionscode `38`, Android ab API 21. Originale Spieldaten sind weiterhin erforderlich und nicht Bestandteil des Pakets.

## Änderungen zu den gemeldeten Fehlern

| Meldung | Änderung |
| --- | --- |
| LOAD-Texte überlagern sich | Die Originalgruppen werden in ihrer OT-Reihenfolge gezeichnet: Hintergrund, Platzzeilen, vordere LOAD-/SAVE-Platte, Meldungsplatte, Schrift. Die vordere Platte verdeckt die darunterliegenden Zeilen. Beim Scrollen nach oben werden die originalen ausgeblendeten Elemente berücksichtigt. |
| Autofahrt: helle Flächen und falsche Spiegelungsprojektion | Der gemeinsame Raumshader berechnet GX-Nebel aus der rasterisierten Tiefe mit den Originalregistern. Ereigniskameras behalten ihre originale 4:3-Projektion; das fertige Bild wird auf den Bildschirm gestreckt. Dadurch verwenden Hauptansicht und Rückspiegel dieselbe Quellprojektion. Modellnormalen verwenden für Beleuchtung und Spiegelungen die inverse transponierte Platzierungsmatrix. |
| Haus und Wald haben unterschiedliche Distanzhelligkeit | Himmelgeometrie wird nicht mehr künstlich auf die hinterste Tiefenebene verschoben. Die gemeinsame Nebelberechnung gilt auch für transparente Weltgeometrie. Kamerawechsel wählen die im CAM-Datensatz verknüpfte Lichtregion, statt Kameranummer und Lichtregion gleichzusetzen. Die originalen Blur-/Kontrastwerte aus den Lichtdaten werden berücksichtigt. |
| Gold/Münzen fehlen beim Aufnehmen | Die Aufnahme verwendet den Originalpfad `ss/Cmn/itmXX.bin` bzw. die passenden ITM-Daten des Raumes. Untersuchen im Koffer verwendet weiterhin IDM/CAP. Der bisherige Versuch, die Münze 0x78 als nicht vorhandenes `ss/Item/idm078.bin` zu laden, entfällt. |
| Ganado am Fenster: Ton und Schatten fehlen | Die Feedback-Warteschlange des ursprünglichen Hausgegners wird auch während des Hausereignisses abgearbeitet. Das originale Raum-SHD enthält die Schattenempfänger; diese werden mit dem animierten Ganado und den passenden Original-Schattenlichtern gezeichnet. Der vorhandene Originalstream 1/0x22 wurde im Ablauf überprüft. |
| SAVE-Auswahl und Platzierung | Horizontale YES/NO-Auswahl verwendet HAT_X bzw. AXIS_X; die Platzliste verwendet HAT_Y bzw. AXIS_Y. Berührungen prüfen die tatsächlichen Auswahlrechtecke. Meldungen verwenden die Originalpositionen und Glyphenmaße. Die Laufanimation endet nicht mehr vorzeitig mit der Dateiarbeit, sondern läuft in der Abschlussphase weiter. |
| Kartenzoom ruckelt; Rahmen beginnt zu weit rechts | Die originale Kameraanimation wird zwischen den 30-Hz-Schritten kontinuierlich dargestellt. Zoomschritt und Pan-Geschwindigkeit folgen den Originalwerten. Sortierte Geometrie, Texturshader und Arbeitsobjekte werden wiederverwendet. Gedrückte Zoomtasten werden fortlaufend verarbeitet. Der Kartenrahmen bleibt links verankert und wird bis zum Bildschirmrand erweitert. |
| Kleine Statusinfo unten links | FPS und die erste Statuszeile sind im Kampagnenbetrieb wieder sichtbar, mit 10-sp-Schrift und kleinen Abständen unten links. Ladebilder und Filmsequenzen behalten ihre bisherige Darstellung. |

## Rendering-Abgleich

Verglichen wurden die im Projekt vorhandenen Originalquellen, insbesondere `GXPixel.c`, `game/trans.cpp`, `game/filter00.cpp`, `game/card.cpp`, `game/sce_at.cpp`, `st1/r100.cpp`, `st1/r120.cpp` sowie die bereitgestellten Originalressourcen. Das verlinkte YouTube-Video konnte in dieser Umgebung nicht abgerufen werden; ein visueller Vergleich mit diesem Video wird nicht als erledigt ausgewiesen.

### Nebel und Unterschiede zwischen den Räumen

GX berechnet Nebel nach der Projektion aus der Pixeltiefe. Der GLES-Pfad verwendet jetzt dieselben quantisierten A-/B-/C-Register, die 24-Bit-Tiefe, die verschiedenen Original-Nebelfunktionen und die 256-stufige Farbverrechnung. Die Projektion behält die originalen Nah-/Fernwerte 100 und 1.000.000. Die KamerafarPlane des Spiels dient separat der Sichtbarkeitsprüfung; sie wird nicht als Ersatz für die Projektionsferne verwendet.

r100, r101 und r120 besitzen tatsächlich verschiedene Licht- und Filterdaten. Beispielsweise verwenden Abschnitte in r100 exponentiell quadratischen Nebel vom Typ 5, r101 linearen Nebel vom Typ 2 und r120 invers exponentiell quadratischen Nebel vom Typ 7. Dazu kommen unterschiedliche Farben, Ambientwerte und Kontrastparameter; die Ereignisse der Autofahrt liefern eigene CUTLIT-Daten. r101 darf deshalb nicht als pauschales Helligkeitsprofil für r100 übernommen werden.

Die Fehlerbehebung ergänzt die gemeinsame Berechnung und die fehlenden Originalparameter. Sie setzt keinen künstlichen globalen Helligkeits- oder Nebelverstärker ein.

### Filter00 und Wiederherstellung

Der neue Filter00-Pfad setzt die aus den Raum-/Ereignislichtdaten gelesenen Blur- und Kontrastparameter um. Die Originalreihenfolge ist: vorheriges Bild für Blur verwenden, das Ergebnis in die Bildhistorie kopieren, anschließend den additiven Kontrast zeichnen. Der Kontrast subtrahiert seinen Byte-Bias, verwendet die Originalstufe und den Originaldeckwert.

Die Historie wird einmal pro ursprünglichem 30-Hz-Simulationsschritt weitergeschaltet. Zusätzliche Displaybilder und pausierte Bilder verstärken den Effekt nicht wiederholt. Bei einer EGL-Neuanlage wird die richtige Historie zusammen mit der Lichttransition wiederhergestellt. Ein echter Raumwechsel verwirft sie; ein Ereignis mit anderen CUTLIT-Daten innerhalb desselben Raums behält sie.

### Fenster-Schatten

Die neuen SHD-Dateien werden mit dem bestehenden GX-Modellleser als ungeskinnte Schattenempfänger eingelesen. Es wird keine Ersatzfläche am Fenster erzeugt. Der Schattengeber verwendet die aktuelle Originalpose des Hausgegners. Lichtauswahl, FOV, Randbereich, Distanzabnahme und die im Haus verwendeten harten bzw. weichen Schatten stammen aus den Originaldaten. Die GPU-Puffer werden nach Kontextverlust aus den gespeicherten Raumressourcen neu aufgebaut.

Dieser Pfad implementiert die im Haus benötigten Fit-Schattenlichter. Er ist noch kein vollständiger Ersatz aller festen, parallelen und Selbstschatten der Originalengine. Mehrere passende Lichter werden derzeit in einzelnen Empfängerpässen verarbeitet; das vollständige ursprüngliche TEV-Batching mehrerer Schatten ist damit nicht zugesichert.

## Prüfungen dieses Pakets

| Prüfung | Ergebnis |
| --- | --- |
| Android ARM64 `:app:assembleDebug` und `:app:lintDebug` | Erfolgreich mit den endgültigen Quelländerungen |
| Native CTest-Grundsuite | 43 von 43 erfolgreich |
| Raumbeleuchtung auf GLES | 96 Prüfungen erfolgreich, einschließlich GX-Beleuchtung, CORE-Beleuchtung, Spiegelungsnormalen, Filterhistorie, Raumwechsel und GPU-Neuanlage |
| Gegenstandsmodelle auf GLES | 1.100 Prüfungen erfolgreich; 58 tatsächliche IDM-Szenen sowie das tatsächliche Aufnahme-Modell der Münze 0x78 |
| Originaler r120-Rückspiegel auf GLES | 307 Prüfungen erfolgreich; Original-RTT und kontrollierte Materialprüfung |
| Haus-/Nebenereignisse über produktiven JNI-/GLES-Pfad | 213 Prüfungen erfolgreich; Original-Kamera/Lichtregion, Hausstream, SHD-Schatten, Funkgerät, Pause und EGL-Neuanlage |
| Spielablauf mit Originaldaten | 2.672 Prüfungen erfolgreich; Einleitung, Haus, erster Gegner, Übergang zum Funkgerät, drei Außen-Ganados, Falle, Meldungen und Aufnahme-Audio |
| Java-Kartenkamera | 11 Prüfungen erfolgreich, einschließlich Zwischenbildern der Zoomanimation |
| Java-Card-Ablauf | 31 Prüfungen mit Originalformat-Fixture erfolgreich |
| Menüressourcen und Geometrie | 1.070 vorhandene Prüfungen erfolgreich |

Der Schattenvergleich im tatsächlichen Hausereignis verändert 224.972 Pixel. Das nach GPU-Neuanlage wiederhergestellte Bild ist identisch. Der Fallentest setzt die Simulation nach der tatsächlichen Explosion fort; ein erreichbares Menü allein gilt dabei nicht als Nachweis einer laufenden Spielsimulation.

Die Spiegelungstests prüfen sowohl die Originaldaten als auch kontrollierte Texturwerte am tatsächlichen Fenstermodell. Einige originale CORE-Spiegelungstexturen sind bei den geprüften Normalenwinkeln schwarz; daraus wird kein sichtbarer Glanz in jedem Kameraschnitt abgeleitet.

Für weitere lokale Prüfungen stehen `tools/test_host.sh` und die aktualisierten Originaldaten-/GLES-Testprogramme zur Verfügung. `RE4_TEST_DISC_FILES` muss auf die bereitgestellten Disc-Dateien zeigen. Es werden keine Test-Fixtures mit Originalspieldaten mitgeliefert.

## Grenzen der Verifikation

Der Android-Build wurde geprüft; ein Lauf auf dem Android-/Adreno-Gerät und ein visueller Bildvergleich mit dem Originalvideo stehen noch aus. Insbesondere die berichteten Helligkeits- und Reflexionsunterschiede sowie die tatsächliche Scroll-/Zoomflüssigkeit müssen auf dem Gerät kontrolliert werden. Host-GLES-Prüfungen ersetzen diese Kontrolle nicht.

Eine vollständige 1:1-Gleichheit aller Räume, aller Leon-Aktionen und sämtlicher Engineeffekte ist weiterhin nicht nachgewiesen. Der bestehende Spielerfunktions-Audit und die Einschränkungen der Kampagnen-/SAVE-LOAD-Profile aus Korrekturen 3 gelten weiter. Dieses Paket korrigiert die aufgeführten gemeinsamen Render-, Menü-, Aufnahme- und Ereignispfade; es erklärt die übrige Engine nicht für vollständig portiert.
