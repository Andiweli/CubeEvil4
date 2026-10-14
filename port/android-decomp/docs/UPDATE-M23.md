# M23 — Gegenwehr, Schulterkamera und Nahkampf auf r400

Stand 04.10.2026. Kombiniertes Reparaturpaket mit vollständigen M22- und M23-Dateien über **M21**, Ordner `RE4-Android-Decomp`, Paket `com.ast.re4decomp`, Version23 / `0.23.0-dev`. ARM64, Android ab API21 und GLES3 bleiben unverändert.

## Die fünf Korrekturen

1. **Abschütteln mit LS:** Widerstand erhält eigene Analog-Flanken, unabhängig vom bisherigen Menü-Schwellwert. Nach der radialen Totzone wird ab0,25 aktiviert und unter0,10 freigegeben. Direkte Links/Rechts-Wechsel zählen erneut; Halten und Schwellwertzittern zählen nicht. Die ursprünglichen PlGachaMove-Gruppen bleiben erhalten: horizontale Flanke zählt zweimal, vertikale einmal, A/Kreuz oder B/Kreis einmal. Mehrere Flanken zwischen Renderbildern werden addiert und einmal an die30-Hz-Kampfsimulation übergeben. Ein gehaltenes L2 blockiert diese Gegenwehr auch nach einer Unterbrechung nicht. Pause/Controllerwechsel/Kontextunterbrechung löschen alte Eingaben wie zuvor. Mehr als30 Original-Gegenwehrpunkte können weiterhin den originalen tödlichen Abschüttelzweig auslösen.
2. **Schulteransicht auch beim Bewegen:** Lauf-, Pistolen- und Messeransicht verwenden dieselbe seitliche Schulterposition statt einer zusätzlichen zentralen Orbitkamera. Ein neuer Bewegungs-/Raumstart blickt horizontal statt mit dem alten0,45-Radian-Abwärtswinkel.
3. **Nahkampftest auf r400:** „Ganado-Nahkampf: r400 / Neustart“ lädt den originalen Disc-1-Raum. Leon startet nahe dem originalen RoomInfo-Punkt0 (MURA) bei(-12400,2576,31080), mit Blick zum Testgegner. Der Gegner startet bei(-12400,2274,28080). Beide Kapseln werden auf Original-SAT aufgesetzt; die tatsächlichen Bodenhöhen sind ungefähr2575,77 bzw.2273,52. Die EAT-Kollision bleibt aktiv. Der einzelne unbewaffnete EM12 bleibt der auditierte **r100-ESL-Datensatz #37** mit seinen1.000 HP, Flags und RNG-Index; lediglich die Testaufstellung wird explizit nach r400 verlegt. Dies ist keine Behauptung, dass die r400-Szenariogegner bereits portiert seien.
4. **Vertikale Blickachse umgekehrt:** Der rechte Y-Eingang wird einmal im nativen InputMapper invertiert und erreicht freie Kamera, Pistolen- und Messerzielen. Der linke Stick und Menüachsen behalten ihre bisherigen Richtungen. Die zuvor korrigierte horizontale Blickachse bleibt erhalten.
5. **Höherer Kamerapunkt an Wänden:** Der Bezugspunkt sitzt bei95% der1600-Einheiten-Figurhöhe (1520 oberhalb der Füße); der seitliche Abstand beträgt900. Die Kamera verkürzt bei Hindernissen den horizontalen Versatz durch denselben SAT-Kugelsweep einschließlich SEE-Filter. Sie sinkt dabei nicht auf Hüfthöhe. Blickneigung verändert die Blickrichtung statt die Augenhöhe. Die vorhandene Ausblendung der Leon-Darstellung unter1000 Kameraabstand schützt weiterhin vor einem Blick in seinen Körper. Transparente Modellgruppen werden jetzt mit der tatsächlichen versetzten Kamera sortiert.

## Installation

Reparatur-ZIP über die bestehende **M21**, **M22** oder unvollständige **M23**-Quelle entpacken; vollständige Dateien unter `RE4-Android-Decomp/` ersetzen. Keine Disc-/Spieldaten sind enthalten. Vorhandene vollständige Disc-Imports können weiterverwendet werden; für r400 muss Disc1 vollständig importiert sein. Der vorhandene Raumkatalog prüft seine gepackte Quelle und das entpackte Archiv. Es sind keine neuen Referenz- oder Katalogdateien notwendig.

Vor dem Build in Android Studio das Projekt schließen bzw. laufende Builds beenden und die generierten Ordner `app/.cxx` und `app/build` löschen. Danach mit JDK17 aus dem Projektordner:

```text
./gradlew clean assembleDebug lintDebug
```

Das Paket hat Versioncode23. Im Menü muss die Nahkampf-Schaltfläche **r400** nennen; wenn weiterhin r100 angezeigt wird, läuft noch der alte Build. Das Update ersetzt keine importierten Disc-Dateien und enthält keine neue APK.

## Gerätetest

- Nahkampftest r400 starten: Gelände, höherer Blick über die rechte Schulter und Leon müssen direkt sichtbar sein. L2 sollte keine zweite Kameraposition mehr freischalten; während Waffenaktionen folgt die Blickrichtung weiterhin der original animierten Zielpose.
- RS hoch/runter ohne Waffe, mit L2 und mit L1 prüfen. LS-Bewegung bleibt unverändert.
- Den Ganado greifen lassen; LS mehrfach ungefähr halb nach links/rechts bewegen. Auch kurze Wechsel zwischen zwei Bildern zählen. L2 darf dabei gehalten bleiben.
- Mit der Kamera dicht an Wände gehen: hoher Blickpunkt, verkürzter Abstand, kein Absenken auf Leons Hüfte. Bei sehr kleinem Abstand kann Leon ausgeblendet werden.
- Pistole/Messer, Pause, Pfeil zurück ins Menü und Fortsetzen prüfen; anschließend den bisherigen r301↔r300-Türtest wiederholen.

## Prüfung und Grenzen

Original-r400-SAT/EAT-Integration prüft Aufstellung auf dem Hang, Annäherung, original gekoppelten Griff, sechs Analogwechsel, erfolgreiches Abschütteln und Rückkehr zur Bewegung. Der Test endet mit Leon990HP und Ganado900HP. Die Produktions-JNI/GLES-Prüfung lädt r400 mit92 SMD-Modellen und prüft sichtbare Originalfiguren, Treffer, Controllerachsen, Pause, Kontextwiederherstellung und CPU/GPU-Laderückfälle. Hosttests prüfen Analog-Hysterese, Mehrfachflanken, gehaltene Tasten, Richtungsalias und höhere Wandkamera. Details: `VALIDATION.md`.

Der Port bleibt ein begrenzter nativer Bewegungs-/Kampftest. r400 erhält die vorhandene unterstützte Geometrie/Textur-/SMX-Ausgabe; die M22-Umweltprofile r100/r300/r301 bleiben erhalten. Neue r400-spezifische Wasser-/Effektprofile, Kampagnen-Szenario, vollständige Gegner-KI, Audio und Speichern werden durch diesen Patch nicht hinzugefügt. Android-Geräteausführung und Treiber-/Leistungsprüfung bleiben beim Nutzer; hier wurden der echte Android-Build und Host-Mesa/GLES3 ausgeführt.
