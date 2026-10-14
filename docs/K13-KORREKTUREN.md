# K13 – Titelmenü und ursprüngliche Fahrzeugsperren

Android-Version **0.37.13-dev**, Versionscode **46**, auf Grundlage von K12.

Das Titelmenü wählt beim ersten Start START vor. Beim Beenden des LOAD-Bildschirms sowie des SAVE-Bildschirms an einer Schreibmaschine ist YES vorausgewählt. Bestätigungen für Laden, Speichern, Überschreiben und Löschen behalten ihre bisherige Auswahl.

Beim Rückweg aus den beiden Demo-Filmen wurde die Titelanimation bisher auf Frame 615 zurückgesetzt und dort festgehalten. Dieser Frame liegt mitten in den originalen Helligkeits- und Überblendungsanimationen von TITLE6. Die Titelanimation läuft nun über ihre ursprüngliche Übergangsphase hinaus. Der Filmrückweg bewahrt ihre bestehenden Animationsuhren und verwendet eine getrennte schwarze Rückblende über 15 Ticks, entsprechend `title.cpp::titleMain`. Die Android-Zeichenfläche wird beim Entfernen des Films neu erstellt. Damit bleiben keine alten Film-/Menü-Bitmapdaten im folgenden Bild. Die sichtbare Korrektur des gemeldeten hellgrauen Bildes muss noch am Android-Gerät bestätigt werden.

## Ursache der durchgehbaren Fahrzeuge

Die vorhandene Laufkollision las die statischen SAT-Daten eines Raums. Das Original erstellt zusätzlich Kollisionsflächen aus AEV-Datensätzen des Typs SCR_AT (`0x0B`). Genau diese zusätzlichen Flächen fehlten bei den gezeigten Fahrzeugen. Die Korrektur übernimmt die tatsächlichen Originalpolygone, Höhen, SAT-/EAT-Attribute und Dreiecksreihenfolgen aus `sce_at.cpp` und `atari.cpp`.

In r100 werden alle sieben vorhandenen SCR_AT-Bereiche angebunden: `04`, `05`, `1C`, `1E`, `25`, `26` und `27`. Vor dem Lastwagenereignis blockieren die Originalflächen den Lastwagen und das Polizeiauto. Nach dem Ereignis werden die Fahrzeugflächen gemäß `r100_Car_pos_move` deaktiviert; die ursprüngliche Schluchtsperre wechselt gemäß `R100Init` und `r100_Sce_zombi_dead`. Der Zustand stammt aus dem gespeicherten Raumflag RSF10 und wird beim Laden, Raumaufbau und Ereignisabschluss berücksichtigt.

Die zusätzlichen Flächen gehen in die gemeinsamen Lauf-, Boden-, Kamera- und Schussabfragen ein. Die statischen Raumflächen und ihre bestehenden Besitzer bleiben erhalten. Es werden keine aus sichtbaren Fahrzeugmodellen geschätzten Ersatzboxen verwendet.

Der Parser verarbeitet auch ungeparentete SCR_AT-Vierecke anderer Räume mit ihren Original-Startflags. Die vollständige dynamische Original-Engine einschließlich an Modelle gebundener Flächen und sämtlicher raumspezifischer Schalter ist damit noch nicht portiert. Die konkrete Prüfung dieser Korrektur betrifft die r100-Fahrzeuge und ihre ursprünglichen Ereigniszustände.

## Prüfungen

- 1.378 Java-Startablaufprüfungen, 123 Titelpräsentationsprüfungen und 42 CARD-Ablaufprüfungen bestanden. Darunter sind wiederholte Demo-Rückwege und YES bei LOAD-/SAVE-Abbruch.
- 60 native Szenariokollisionsprüfungen bestanden, einschließlich der acht Fahrzeugseiten, schneller Bewegungen, fünf Annäherungen auf dem tatsächlichen SAT-Boden und Freigabe nach dem Ereignis.
- 880 Geometriefelder stimmen exakt als Float32 mit den unveränderten Originaltabellen und den tatsächlichen DVD-Daten überein. Geprüft sind SAT und EAT vor und nach dem Ereignis.
- Alle 43 vorhandenen nativen CTest-Gruppen bestanden. Originale Titel-/CARD-Daten: 937 Prüfungen bestanden.
- Produktive JNI-/GLES-Laufzeit: 337 Lade-/Speicher-/Raumwechselprüfungen und 213 r100-Nebenereignis-/Grafikprüfungen bestanden.
- ARM64-Android-Build, tatsächlicher APK-Inhalt, Signaturen V1/V2 und 16-KiB-Ausrichtung wurden geprüft. Der erste K13-Build erfolgte nach Entfernen der Android-Build-Ausgaben und Projekt-Caches; anschließende Quelländerungen wurden erneut gebaut.

Ein zusätzlicher älterer Einstiegstest (`m37_entry_runtime_check`) scheitert beim Wiederaufbau eines r120-Grafikfilters mit `Filter00: incomplete framebuffer`. Derselbe Test scheitert mit der älteren Vergleichsbibliothek an derselben Stelle. Dieser Befund ist weiterhin offen und wird nicht als bestandener K13-Test geführt.

Es wurde kein Android-Gerätetest ausgeführt. Für die Geräteprüfung: nach dem Start die Auswahl START prüfen; LOAD und SAVE jeweils mit B/Kreis verlassen; beide Demo-Filme mehrfach überspringen; vor dem Lastwagenereignis gegen Polizeiauto und Lastwagen laufen; anschließend den freigegebenen Weg sowie Speichern/Laden vor und nach dem Ereignis prüfen. Der maschinenlesbare Bericht steht in [K13-PRUEFUNGEN.json](K13-PRUEFUNGEN.json).

## Installation und vollständige Dateien

`RE4-M37-K13.apk` verwendet dasselbe Signaturzertifikat wie K12 und kann darüber installiert werden. Vorhandene Spieldaten und Spielstände bleiben verwendbar. Die englische Laufzeit und die K12-Importfilter bleiben aktiv; ein erneuter DVD-Import ist für diese Korrektur nicht erforderlich.

`RE4-Android-Decomp-R100-Korrekturen13.zip` enthält die vollständigen aktuellen Ersatzdateien K5–K13 einschließlich `FILES-K13.sha256`. Den enthaltenen Projektordner mit dem vorhandenen Projekt zusammenführen und die entsprechenden Dateien ersetzen. `RE4-Android-Decomp-K13-Quellstand.zip` enthält alternativ das vollständige Projekt. Beide Archive enthalten ganze Dateien. Signaturgeheimnisse, lokale Build-Ausgaben und extrahierte DVD-Daten sind ausgeschlossen.
