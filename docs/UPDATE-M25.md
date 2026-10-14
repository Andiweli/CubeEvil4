# M25 — Wandkamera und originale Behälter mit Vorräten

M25 ist ein Quellpatch über dem vollständigen M24-Projekt. Paket `com.ast.re4decomp`, Ordner `RE4-Android-Decomp`, ARM64, Android ab API21 und OpenGL ES3 bleiben erhalten. Die App ist weiterhin ein nativer Testlauf; sie startet noch keine vollständige RE4-Kampagne.

## Kamera und Rechtsstick

Der bisherige Renderer zeichnete Leon und die Waffe bei einem Kameraabstand unter 1000 Spieleinheiten überhaupt nicht mehr. Diese Ausblendregel ist entfernt. Die normale Tiefenprüfung bleibt aktiv: Eine tatsächliche Wand verdeckt weiterhin dahinterliegende Körperteile.

Die Schulterkamera prüft jetzt die drei ursprünglichen Linien aus `CameraQuasiFPS::hitCheck`: Mitte sowie die beiden Seiten der Nahfläche. Sie berücksichtigt das SEE-Attribut `0x8000`, die originale Ausschlussmaske `0x1C2810`, den ursprünglichen Nahbereich von 100 Einheiten und das Bildschirmseitenverhältnis. Der bisherige Kugelsweep für diese Ansicht zog die Kamera auch an Flächen heran, die das Original ausschließt. Augen- und Zielpunkt bleiben zusammengehörig; nahe Wände senken den Blick nicht auf Leons Füße ab.

**Physischer rechter Stick rauf schaut und zielt rauf; runter schaut und zielt runter.** Das gilt für Bewegung, Pistole und Messer. Die bereits korrigierte horizontale Steuerung bleibt erhalten. Die Java-Eingabe liefert physisch rauf bereits als positiven Wert; die zusätzliche native Y-Invertierung entfällt.

Die gemeinsame Schulterkomposition mit 50° Blickwinkel bleibt die gewünschte Anpassung aus M24. Das Original besitzt weitere Kameraprofile, Glättungszustände und Aktionsschnitte. Diese vollständige Zustandsmaschine wird mit M25 nicht behauptet. Bei sehr kurzer Distanz ist ein näherer Ausschnitt von Kopf/Schulter/Waffe normal; ein ganzer Körper kann dann nicht vollständig ins Bild passen.

## Originale Kisten/Fässer und Gegenstände

Für r400 werden 13 originale `cEmBox`-Behälter angebunden: drei kleine Kisten, zwei große Kisten und acht Holzfässer. Die ursprünglichen ETS-Platzierungen, YARE-Treffervolumen, Drehungen und ITA-Bindungen werden verwendet. Pistole und Messer können sie zerstören. Der nächste gültige Treffer entscheidet zwischen Behälter, Ganado und EAT-Weltgeometrie; ein verdeckter Ganado erhält keinen zusätzlichen Treffer durch einen Behälter hindurch.

Nach der Zerstörung verschwindet das Behältermodell und der Gegenstand aus seiner ursprünglichen ITA-Zuordnung erscheint. Die 13 Fundstücke verwenden fünf originale ITM-BIN/TPL-Modelle: 2.844 texturierte Dreiecke und sieben Texturen. Sie stehen an der ursprünglichen Behälterposition plus den originalen 50 Y-Einheiten. Modelle werden einmal hochgeladen und anhand ihres Zustands ein-/ausgeblendet; Materialbatches behalten ihre einzelne Objektidentität.

| Inhalt aller 13 Behälter | Menge |
|---|---:|
| Pistolenmunition | 4 Packungen à20, insgesamt80 |
| Gewehrmunition | 3 Packungen à10 und1 à15, insgesamt45 |
| Grünes Kraut | 2 |
| Rotes Kraut | 2 |
| Brandgranate | 1 |

Das sind die aus r400 aufgelösten Originalinhalte für Leon. Es werden keine zusätzlichen zufälligen Munitionstropfen oder Ersatzgegenstände erzeugt.

**A/Kreuz** nimmt einen angebotenen Gegenstand bei abgelegter Waffe auf. Die ursprüngliche ITA-Bereichsprüfung, Priorität und EAT-Sichtprüfung verhindern Aufnahme durch eine Wand. Ein Tastendruck nimmt einen Gegenstand auf; Halten wiederholt die Aktion nicht. Tür-/Trittangebote haben Vorrang vor derselben A-Flanke.

Pistolenmunition ergänzt die vorhandene Reserve. Nur tatsächlich übernommene Patronen werden aus dem Vorrat entfernt; ein Überlauf verliert keine Munition. Die ursprüngliche 16-Bit-Grenze für die Gesamtsumme bleibt erhalten. Gewehrmunition, rote Kräuter und Brandgranaten werden mit ihrer tatsächlichen ID/Menge aufbewahrt. Gewehr, Granatenwurf, Kräuterkombination und der vollständige Inventarkoffer sind noch nicht angebunden.

**Y/Dreieck** öffnet das pausierte Testmenü mit der Vorratsanzeige. „Grünes Kraut verwenden“ heilt bis zu 600 HP. Bei vollständiger Gesundheit oder besiegtem Leon wird kein Kraut verbraucht. Vor der Verwendung die Waffe ablegen; während Griff-/Trittaktionen wird die Verwendung abgewiesen. Das Ergebnis erscheint im HUD und in der Vorratsanzeige. Ein rotes Kraut allein heilt nicht. Die universale Inventartaste respektiert die vorhandene A/B-/X/Y-Layoutoption.

## Raumwechsel, Pause und Lebenszyklus

Zerstörte Behälter und aufgenommene Gegenstände bleiben innerhalb derselben nativen Session erhalten. Der Wechsel nach r301/r300 und zurück zum r400-Kampftest füllt Behälter nicht erneut. Vorräte aus dem inaktiven Raum bleiben sichtbar; grüne Kräuter können auch dort verwendet werden. Schnappschüsse sind an Raum und Originaldatenprofil gebunden und werden vor einer Änderung auf Inhalt und Prüfsumme geprüft.

Ein echter GL-Kontextwechsel lädt die vorhandenen Modelle erneut hoch, ohne HP, Munitionsbestand, Behälter oder Fundstücke zurückzusetzen. CPU-/GPU-Ladefehler nehmen den Raumwechsel zurück und binden den bisherigen Ganado wieder an seine ursprünglichen SAT-/EAT-/Objektdaten. Die UI stellt nur eine Kräuteranforderung; Verbrauch/Heilung passieren auf dem owning GL-Thread. Ein Ladezustand hält diese Anforderung zurück; Start-Pause kann eine ausdrücklich angeforderte Kräuterverwendung ausführen, ohne Gegner/Animationen weiterlaufen zu lassen.

**Prozessneustart oder eine neue native Session setzen diese Testzustände zurück.** M25 implementiert noch kein dauerhaftes Savegame. Der neue Kampftest übernimmt Munition und Angriffsserien des bisherigen Leon, verwendet aber weiterhin seinen ausdrücklich beschriebenen Testneustart mit voller Gesundheit.

Der begrenzte Behälteradapter zeigt Zerstörung unmittelbar nach dem Player-Callback; im Original verarbeitet der nächste `EmMgr`-Durchlauf den vorgemerkten Treffer. Zerstörungspartikel, Splitter, Audio und dynamische Laufkollisionszylinder der Fässer fehlen. Die vorhandene statische SAT-Laufkollision wird durch diese Stufe nicht geändert.

## Holzplanken und Feuer in r400

r400 ist das Mercenaries-Dorf. Sein AEV enthält14 Einträge und keine Raumwechsel-Türen. M24 zeigte zusätzlich33 Originalobjekte; weder neue Sperrmodelle noch neue SAT-Laufkollision wurden angelegt. Sichtbare geschlossene Türen/Planken sind deshalb nicht automatisch ein neu gebautes Levelende. Eine konkrete äußere Planke lässt sich ohne Position/Bild nicht sicher zuordnen; M25 entfernt keine Originalgeometrie auf Verdacht.

Das Kampagnendorf mit brennendem Scheiterhaufen ist **r101**. Dessen `R101Init` aktiviert den Pyre und eine eigene EST-Effektgruppe; r400 besitzt diesen Aufruf und diese EST-Gruppe nicht. Die vorhandene Wasseranimation sowie r400-Glut/Lichtstrahlen bleiben erhalten. M25 ergänzt keine künstlichen Flammen an beliebigen Stellen.

Als nächster größerer Ausbau bietet sich r101 an: seine originale Pyre-Geometrie, Flammen-/Rauchsequenzen und Gebietsfreigaben verbinden, dann Raum-/Szenarioflags und passende Gegneraktivierung schrittweise integrieren. Der Behälter-/Vorratskreislauf aus M25 liefert dafür eine prüfbare Grundlage. Ein vollständiger Kampagnenlevel benötigt zusätzlich die beteiligten Türen, Skripte und Gegnerzustände.

## Einspielen und testen

1. ZIP im Elternordner des vollständigen M24-Projekts entpacken; die vollständigen geänderten Dateien unter `RE4-Android-Decomp/` ersetzen.
2. `RE4-Android-Decomp/app/.cxx` und `RE4-Android-Decomp/app/build` entfernen. Gradle-Sync ausführen und `clean assembleDebug` bauen. Vorhandene vollständige Disc-Imports können weiterverwendet werden.
3. Im Ganado-Nahkampftest r400 laden. An einer Wand rückwärtsgehen/drehen und Pistole sowie Messer anlegen: Leon soll bei engem Abstand weiterhin sichtbar sein.
4. RS rauf/runter in Bewegung, Pistolen- und Messeransicht prüfen; RS rechts/links darf keine erneute Umkehr zeigen. Gegenwehr mit LS links/rechts bleibt verfügbar.
5. Eine Kiste/ein Fass mit Messer oder Pistole treffen. Behälter verschwindet, tatsächlicher Gegenstand erscheint; Waffe ablegen, A/Kreuz aufnehmen. Pistolenmunition erhöht die Reserve nur einmal.
6. Nach Kräuteraufnahme etwas Gesundheit verlieren. Y/Dreieck → „Grünes Kraut verwenden“: bis600HP Heilung, ein Kraut weniger. Bei voller Gesundheit bleibt der Vorrat erhalten.
7. Start pausieren, App in Hintergrund/Vordergrund bringen, Türtest nach r301 und zurück zu r400 starten: Munitions-/Fundzustände dürfen innerhalb der Session nicht dupliziert werden. Ein App-Prozessneustart beginnt ausdrücklich einen neuen Testzustand.

Bei einem Fehler das vollständige Logcat mit `RE4Flow`, `RE4Player`, `RE4Weapon`, `RE4Enemy`, `RE4Items` und `RE4Scene` senden. Die genaue Wand-/Behälterposition oder ein Bild erleichtert die Zuordnung zum Originalobjekt.

Abschlussprüfungen und ihre Grenzen stehen in [VALIDATION.md](VALIDATION.md); konkrete Originalquellen und der Feuerpfad in [SOURCE-NOTES-M25.md](SOURCE-NOTES-M25.md). Der Patch enthält keine Disc-Dateien, Originalspieldaten, APK oder Buildausgaben; `upstream/re4` bleibt unverändert.
