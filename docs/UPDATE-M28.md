# M28 — Waffenlaser, weicher Kamerablick und feste Turmleitern

Version **0.28.0-dev**, versionCode 29. Vollständige geänderte/neue Dateien über dem vollständigen M27-Stand. Paket `com.ast.re4decomp`, ARM64/API 21/GLES3 und Ordner `RE4-Android-Decomp` bleiben erhalten. Keine Originalspieldaten werden mitgeliefert; ein erneuter Disc-Import ist nicht erforderlich.

## Laser

Die bisherige Diagnose benutzte den Schussstrahl als sichtbaren Laser. Der sichtbare Strahl benutzt nun den originalen WEP01-Marker `{262,-24,32}`, die animierte rechte Hand und zusätzlich das eigene Waffengelenk. Die eigentliche Schussberechnung einschließlich Streuung bleibt erhalten. Der ursprüngliche 500-Einheiten-Abstand am Linienanfang verhindert den rückwärts in Leon reichenden Abschnitt.

Aus dem ersten CORE-EFF werden EST3/Esp19 für die Linie und EST0x50/Esp00, Auswahl 0, für den Zielpunkt gelesen. Originalfarbe, 96×96-Textur, Größe, Entfernungsfaktor, additives Blending und Linienlimit werden verwendet. Ein roter Punkt erscheint nur beim nächsten unterstützten lebenden Gegner, intakten Behälter oder verletzbaren Fenster/Schloss. Gewöhnliche Wandtreffer, bereits zerstörte Objekte und geschützte Fenster liefern keinen Punkt. Welt- und Objekttreffer werden nach Entfernung verglichen; die Abfrage verändert weder HP noch Zufallszustände. Der Punkt wird tiefengeprüft und schreibt keinen Tiefenwert.

## Kamerablick

Rechter Stick setzt jetzt einen Zielversatz; der sichtbare lokale Blick folgt weich mit 0,8 Restanteil pro ursprünglichem 30-Hz-Takt. Teilbilder integrieren denselben Faktor zeitabhängig. Die originale 0,9-Rückzentrierung bleibt im 30-Hz-Takt. Damit springen die Blickwinkel beim Stickbeginn und beim Richtungswechsel nicht mehr sofort auf den Zielwert. Physisch rauf schaut weiter rauf, rechts weiter rechts.

Dies ist eine begrenzte Umsetzung des Glättungsprinzips aus `CameraSmooth`, keine vollständig portierte Kamerazustandsmaschine. Das bisherige gemeinsame Schulterprofil, Waffenübergänge und Wandprüfung bleiben angeschlossen. Glättung betrifft den lokalen Kameraversatz, nicht pauschal die Spielereingaben oder die originalen Waffen-Drehregeln. Die Kamera prüft Wandabstand nach der Glättung. Ziele, sichtbare Werte und Teilzeittakt gehören zum gesicherten Grafikzustand.

## Leitern

Freigeschaltet sind die festen Turmleitern **r101/AEV5 und r400/AEV10**. Mit abgelegter Waffe davor ausrichten und die angezeigte **A/Kreuz**-Aktion drücken. Oben zum Einstieg zurückblicken, bis „Leiter hinunter“ erscheint, und erneut A/Kreuz drücken.

Die sechs originalen PL_ARC-Animationen 0x27–0x2c, Blend-/Wiederholungsregeln, dreidimensionale Root-Bewegung und ihre Soundauslöser stammen aus `player.cpp::pl_R1_Ladder`. Die jeweils neun Stufen laufen bis zum Originalausstieg; Anfang und Ziel werden aus den echten AEV-/FCV-Daten bestimmt. Aufwärts dauert 198, abwärts 204 Originaltakte. Währenddessen übernehmen die Animationen die Bewegung; Waffen-/Interaktionsbefehle werden nicht als spätere Aktionen gespeichert. Die bestehende Pause hält den Ablauf an.

Der Rückweg oben ist ausdrücklich ein **Android-Adapter**, denn diese feste AEV beschreibt nur den Aufstieg. Er verwendet die originalen Abwärtskurven und einen geprüften Montageversatz. Er ersetzt weder die ursprüngliche Sprungaktion noch deren fehlende Ereignissteuerung. Beide Ausstiege müssen mit der echten SAT-Kapsel gültig sein, bevor die Aktion angeboten wird. Ein ungültiger Abschluss fällt auf den letzten geprüften Ausgangsstand zurück; es wird kein Ersatzboden erzeugt.

Bewegliche Hausleitern, Leiterumwerfen, Fenstersprünge, Kameraschnitte und Ashley-Aktionen sind noch nicht freigeschaltet. Die Hausleitern behalten ihre ursprünglichen Raumflag-Voraussetzungen. Es wird nicht jede Leiter in jedem Raum pauschal aktiviert.

## Installation und kurzer Gerätetest

1. ZIP über den vollständigen M27-Projektordner kopieren, enthaltene Dateien ersetzen.
2. `bash ./gradlew clean assembleDebug lintDebug` ausführen; Windows: `gradlew.bat clean assembleDebug lintDebug`. Startanzeige **0.28.0** prüfen.
3. r101 öffnen. Eine Kiste anvisieren, Waffenansatz und Zielpunkt beobachten; daneben auf gewöhnliche Wand/Bodenfläche zielen. Kiste zerstören und erneut anvisieren.
4. RS-Blick aus dem Stand beginnen, loslassen, Richtung wechseln; anschließend LT/L2 und L1/LB kurz gegenprüfen.
5. Die feste Turmleiter in r101 auf- und absteigen. Während des Aufstiegs einmal mit dem Pfeil pausieren und fortsetzen. Optional denselben festen Turm in r400 prüfen.

Bei Auffälligkeiten vollständiges Logcat senden, insbesondere `RE4Ladder`, `RE4Player`, `RE4Flow` und `RE4WorldEffects`. Keine manuelle Prüfung aller Räume notwendig.

## Prüfung und Grenzen

Die 31 CTest-Vertragsprüfungen bestehen bei aktiviertem `assert`, darunter neue Lasergrenzen, Leiter-AEV-Daten und zeitgleiche Kamerabewegung bei 30/60/120 Hz. Mit den Originaldiscs bestehen die Auf-/Abstiege beider Turmleitern einschließlich realer Landungen, Pausenzeit und genau 40 Aufstiegs-Soundaufrufen. Der neue Produktions-GLES-Test prüft die originale Zielpunkttextur, stabile Pausenbilder, Tiefenverdeckung und wiederhergestellten Grafikzustand. Die Produktions-Brückenregression besteht mit 2.924 Prüfungen, die Tür-/Fensterregression mit 774. Die neue Produktions-Leiterprüfung besteht mit 850 Prüfungen für echte A-Eingaben, Auf-/Abstieg, Pause und EGL-Kontextverlust während des Aufstiegs. Die bestehende Wandkamera-GLES-Prüfung besteht mit 1.383 Prüfungen. Java-Disc-, Controller- und Raumrepository-Prüfungen bestehen.

In dieser Arbeitsumgebung waren Google-SDK-/NDK-Downloads nicht erreichbar. **Ein neuer Android-ARM64-Build, Lintlauf und Gerätetest werden für M28 deshalb nicht als bestanden behauptet.** Hostprüfungen kompilieren die tatsächliche Produktions-JNI-/GLES-Implementierung mit GCC und Mesa, ersetzen aber keinen Android-/Adreno-Test. Historische Build-Ergebnisse in VALIDATION.md beziehen sich auf ihre dort genannten früheren Versionen.

Im eingesandten App-Log wurde kein fataler App-Absturz gefunden. r100 meldete zunächst fehlende EM12-Aktionen; r101 lud anschließend die vollständigen Messer-/Kampfclips. Die bekannten noch offenen EAR-/SST-Spezialeffekte bleiben offen.

## Nächster großer Schritt

Ein zusammenhängend spielbarer r101-Dorfablauf: originale Gegneraktivierung und mehrere Ganados, Hausereignis, dynamische Leitern/Fenstersprünge und schließlich die Glocken-Endbedingung. Dafür müssen originale Ereignisse und Tasksequenzen an den vorhandenen Raumzustand angeschlossen werden. Eine erfundene Gegnerwelle oder eine frei gewählte Zeitschaltuhr ist kein Ersatz. Vollständige Kampagne, Inventarkoffer, Musik/Stimmen und dauerhafte Saves bleiben weitere gemeinsame Systeme.
