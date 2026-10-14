# R100 – Korrekturen 3

Dieses Paket wird über den vollständigen M37-Stand einschließlich **R100-Korrekturen 2** gelegt. Es enthält vollständige geänderte und neue Dateien. Ordnerstruktur beibehalten und die vorhandenen Dateien ersetzen. Anwendung: `com.ast.re4decomp`, Version `0.37.0-dev`, Versionscode `38`, Android ab API 21. Das Illuminados-Icon bleibt erhalten.

## Beleuchtung und Spiegelungen

Der Abgleich verwendet die im Projekt vorhandenen Originalquellen und die tatsächlich bereitgestellten Spieldaten.

- `Event::ExePacket_SetOm` und die SetObj18-Präfixe bestimmen die Lichtklasse. Die beiden pl07-Polizisten verwenden nun die originale NPC-Maske `0x40`. Die Lichtposition folgt der Modellwurzel; der Begrenzungskasten dient weiterhin der Auswahl passender Lichtquellen.
- `trans.cpp::specularSetup` wird für die Materialtypen 0 und 1 umgesetzt: originale CORE-Palette, lokale Paletten, Materialfarbe, Normalenprojektion, Skalierung und anschließende TEV-/Nebelverarbeitung. Materialgruppen mit unterschiedlichen Spiegelungsparametern werden getrennt gezeichnet. SMX-SpecularColor überschreibt die BIN-Farbe nur nach der Originalregel.
- `r120.cpp::EventCarInit` aktiviert bei beiden Polizisten den zusätzlichen CORE-Beleuchtungspass. `GlobalIlluminationSetup` multipliziert die vorherige Materialfarbe mit der Originaltextur, einschließlich Originalskalierung und Begrenzung.
- Der originale Rückspiegel behält seine Quellgröße 592 × 512, den 512 × 512-Ausschnitt ab x=40 und die 256 × 256-Kopie. Diese Werte und die Projektion wurden gegen die tatsächlichen Originalfunktionen geprüft. Der Pass stellt den vorherigen Grafikzustand wieder her.
- `scroll.cpp::SmxSetFlag` setzt für Flag `0x10` keine diffuse Beleuchtung, erhält aber Raum- beziehungsweise Vertex-Ambient. Der vorherige weiße Materialpfad wurde entfernt. Das Scroll-Attribut filtert Lichtquellen mit Attribut 4 wie `cLightMgr::setModel2`.
- An Fensterteile angehängte Ereignislichter übernehmen die originale Transformation auch für ihre Richtungen. Es gibt keine pauschale Erhöhung oder Absenkung der Bildhelligkeit.

Die CORE-Spiegelungstexturen sind überwiegend schwarz. Ein sichtbarer Glanz hängt vom Normalenwinkel und der Kamera ab. Der GPU-Test prüft deshalb sowohl die Originaldaten als auch einen kontrollierten Texturwert am tatsächlichen Fenstermodell; er behauptet keinen sichtbaren Glanz in jedem Kameraschnitt.

## Aufnehmen und Koffer-Leon

Die Aufnahmeschrift nutzt die Farben und Deckwerte der Originalschrift, einschließlich ihrer vorhandenen dunklen Konturen. Die zuvor zusätzlich erzeugte Kontur entfällt. Glyphen werden mit bilinearer Abtastung in der originalen Anordnung dargestellt.

`ItemExamine::getEFB` verkleinert im Original die eingefrorene Hintergrundkopie absichtlich. Diese Kopie verwendet nun die halbe aktuelle Bildschirmgröße, statt auf 320 × 224 beschränkt zu bleiben. Der Originalablauf mit Gegenstandsmodell und anschließendem Take/YES/NO-Dialog bleibt erhalten.

Koffer-Leon wird mit dem vollständigen Originalmodell, Kopf-, Hand- und Waffenmodellen, der Originalpose und den Subscreen-Lampen in einem eigenen GPU-Puffer in Bildschirmauflösung gezeichnet. Nicht verwendete Alpha-Werte undurchsichtiger Texturen verursachen keine Löcher im Gesicht oder Körper mehr. Menübeleuchtung und Menü-Grafikzustand gelangen nicht in die Spielansicht.

## Sprengfallen

Der beigefügte Logcat meldet den Abbruch bei `Esp0e: Original-Z-Sichtbarkeitspass`. Die Tiefenkopie verwendet nun ein zum Quellpuffer passendes Format, einschließlich kombinierter Tiefen-/Stencil-Puffer und Fließkomma-Tiefe. Der originale Sichtbarkeitstest mit zwölf Abtastpunkten bleibt aktiv. Fehler enthalten den konkreten fehlerhaften Arbeitsschritt.

Der GPU-Test prüft D16, D24, D32F, D24/S8 und D32F/S8. Der native Spielablauf prüft zusätzlich die tatsächliche Fallenexplosion und 240 anschließende Simulationsschritte ohne angehaltene Spielsimulation.

## SAVE und LOAD

Schreibmaschine und Hauptmenü-LOAD verwenden den gemeinsamen Card-Ablauf mit zwanzig Originalplätzen, Originalgrafiken, Originalschrift, Meldungen, Auswahlmarkierungen und Animationen. Die Oberfläche füllt den aktuellen Bildschirm.

- Leere und beschädigte Plätze bleiben unterscheidbar. Beschädigte Dateien können nach ausdrücklicher YES-Bestätigung gelöscht werden.
- Neues Speichern, Überschreiben, Laden und Abbrechen haben die jeweiligen Originalmeldungen; Bestätigungen beginnen bei NO.
- Dateiarbeit und das Einlesen der zwanzig Kopfdatensätze laufen außerhalb des UI-Threads. Während einer laufenden Operation ist erneute Bestätigung gesperrt.
- Speichern validiert die Originalprüfsummen, schreibt eine temporäre Datei, synchronisiert sie und ersetzt erst dann den vorhandenen Spielstand. Ein Schreibfehler erhält den bisherigen Spielstand.
- Der native Speicherzähler und Continue-Ausgangspunkt werden erst nach bestätigtem erfolgreichem Dateischreiben aktualisiert. Veraltete oder doppelte Abschlussmeldungen werden abgewiesen.
- Erfolgreiches LOAD führt über die originale Ausblendung unmittelbar ins Spiel. Erfolgreiches SAVE zeigt seine Originalabschlussmeldung und kehrt nach Bestätigung zur Spielansicht zurück.
- Der Originalkalender bestimmt die anfängliche Auswahl; innerhalb der Sitzung wird die letzte Auswahl desselben Speichers beibehalten. Prüfsummenfehler beim Laden erhalten die originalen Wiederholungsversuche.
- Das native Originalformat und die bereits vorhandenen Zustandsteile bleiben erhalten. Wiederherstellung umfasst den implementierten Spieler-, Koffer-, Raum-, Gegner-, Fallen- und Szenariozustand.

**Spielbar geladen werden derzeit die implementierten Kampagnenprofile r100/r101.** Andere Räume oder Sonderprofile werden vor dem Start abgewiesen. Vollständige Kompatibilität beliebiger Originalspielstände und aller späteren Kampagnenräume wird nicht zugesichert.

## Prüfungen

- Android ARM64: `assembleDebug` und `lintDebug` erfolgreich; Lint ohne Fehler.
- Allgemeine Host-Suite: 43 native Verträge sowie die vorhandenen Java-Verträge und Quellinventare.
- Originales Card-Format: 56 Java-Speicherprüfungen, 31 Card-Ablaufprüfungen, 257 native SAVE/LOAD-/Continue-Prüfungen und 49 Statistik-/Uhrprüfungen.
- Original-Intro: alle 9/15/8 Kameraschnitte der Ereignisse r120s00/r120s01/r100s40; 71.670.176 Prüfungen.
- Ereignis-GPU einschließlich fünf Tiefenformaten: 1.525.166 Prüfungen.
- Rückspiegel: 307 GPU-Prüfungen und 342 Vergleiche mit Originalfunktionen.
- Beleuchtung: 71 GPU-Prüfungen, einschließlich Ambient, Lichtmasken, beiden Spiegelungstypen und CORE-Beleuchtung.
- Koffer-/Gegenstands-GPU: 1.107 Prüfungen, 58 Originalgegenstände und drei Leon-Waffenvarianten in zwei Bildschirmgrößen.
- Funkgerät: 1.370 Prüfungen für beide Gespräche, Animation, Sprache, Audio, Überspringen und Grafik-Neuanlage.
- Tatsächlicher JNI-/GPU-Spielablauf: 2.672 Prüfungen für Landschaft, Hausereignis, drei Gegner, Übergänge, Fallenexplosion, Meldungen und Aufnahme-Audio.

Für Card-Assetprüfungen mit `tools/test_host.sh` die vorhandenen Originaldateien über `RE4_TEST_DISC_FILES` angeben. Originalressourcen und erzeugte Bildschirmbilder werden nicht mitgeliefert.

## Noch ausstehend

Die Prüfungen erfolgen mit den tatsächlichen Spieldaten auf dem Host und über einen Android-Build. Ein neuer Test auf dem Android-/Adreno-Gerät wurde nicht durchgeführt. Insbesondere die berichteten Helligkeitsunterschiede, Fensterreflexionen und dunklen Figuren müssen dort mit diesem Stand kontrolliert werden.

Eine vollständige 1:1-Gleichheit der gesamten Engine oder aller Leon-Aktionen ist weiterhin nicht nachgewiesen. Bump-/indirekte Materialvarianten, sämtliche Schatten- und Nachbearbeitungspässe sowie alle späteren Kampagnenskripte sind durch diesen Patch nicht vollständig umgesetzt. Der vorhandene Quellinventar-Audit weist weiterhin nicht übertragene und teilweise übertragene Spielerfunktionen aus.
