# K17 – Waffen, erster Zieldruck, Messerpose und sichtbare Gegner

Android **0.37.17-dev**, VersionCode **50**, auf dem vollständigen K16-Quellstand. Grundlage sind die bereitgestellten USA-Disc-1-Daten und der unveränderte Upstream-Commit `9b76ff003081d6ece8b838453fb8310743a972c7`.

## Einspielen

- **RE4-M37-K17.apk:** fertige ARM64-APK mit der bisherigen AST-Signatur. Signatur, Version, ABI und 16-KiB-Ausrichtung sind geprüft. Die Signatur stimmt mit der bereitgestellten K14-APK überein; Installationen mit dieser Signatur können per Update aktualisiert werden und behalten dabei ihre App-Daten.
- **RE4-K17-Austauschdateien.zip:** vollständige neue oder geänderte Dateien gegenüber dem vollständigen K16-Projekt. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden Projekt zusammenführen und Dateien ersetzen. Es werden keine Diff-Patches geliefert.
- **RE4-Android-Decomp-K17-Quellstand.zip:** vollständiges Android-Studio-Projekt mit Gradle-Wrapper, allen K17-Dateien und dem unveränderten Upstream.

**Kein erneuter DVD-Import ist erforderlich.** Das Update verwendet den vorhandenen vollständigen englischen Disc-1-Import. Eigene `local.properties` und Signierdateien gehören weiterhin zur lokalen Buildumgebung. Originalspielstände und die vorherigen Titel-/LOAD-/SAVE-/Sprach-/CONTINUE-/Fernglaskorrekturen bleiben Bestandteil des Projekts.

## Die vier gemeldeten Fehler

| Meldung | K17-Verhalten und Ursache |
| --- | --- |
| L2/LT muss zweimal gedrückt werden | Der erste neue Tastendruck löst das Anlegen sofort aus, auch nach Koffer, Pause und Ausrüsten. Die bisherige Neutral-Sperre ignorierte diesen ersten Druck. Ein durch das Menü hindurch gehaltenes Signal bleibt bis zum Loslassen gesperrt. |
| Weitere Waffen lassen sich ausrüsten, schießen aber nicht | Schuss-, Ziel-, Rückstoß-, Nachlade- beziehungsweise Wurfsteuerung wird aus den zugehörigen Original-Waffencontrollern gebunden. Magazin, Reserve, Animation, Schaden und Geschosse verwenden die tatsächliche Waffenfamilie. |
| Gesundes Messerwegstecken benutzt die Verletztenpose | Der gesunde Original-Waffenclip und die verletzte PL87-Bewegung werden nach der tatsächlichen `dmMotCk`-Regel gewählt. Die gesunde Grenze liegt bei mindestens 800 HP; Originalausnahmen bleiben erhalten. Beide Wegsteckposen sind gegen separat geladene Originalbewegungen über alle 119 Gelenke verglichen. |
| Gegner verschwinden nach einem Waffenwechsel | Der bisherige Neuaufbau von Leons Grafik zerstörte zugleich die globalen Gegner-/Tierpuffer. Der Körper- und Griffneuaufbau ersetzt jetzt nur seine eigenen Daten. Regulärer Szenenabbau räumt weiterhin sämtliche Besitzer auf. Die Grafikprüfung vergleicht tatsächlich gezeichnete Gegnerpixel vor und nach jedem der 25 ausrüstbaren Cheat-Gegenstände. |

## Waffen und Originaldaten

Die 38 vorhandenen Trageprofile umfassen Grundwaffen, Varianten und Waffen-/Zubehörkombinationen; sie sind keine 38 zusätzlichen Grundwaffen. Alle Profile laden und führen ihren Angriff im Originaldatentest aus. Im englischen Cheatmenü sind 25 Gegenstands-IDs ausrüstbar und durch die echte JNI-Steuerung geprüft. Messer und reine Zubehörteile bleiben bei ihrer vorhandenen Bedienung beziehungsweise Kofferrolle.

- Handfeuerwaffen und Magnumvarianten verwenden ihre jeweiligen Ziel-/Schuss-/Nachladebewegungen und Original-Schadenswerte.
- Schrotflinten erzeugen die 13 ursprünglichen Schrotstrahlen. Maschinengewehre feuern bei gehaltenem RT/R2; Matilda verwendet ihren Salvenablauf.
- Gewehre besitzen Zielkamera, Zoom, Schuss und Nachladen. Alle sechs vorhandenen Zielfernrohr-Layouts laden ihre Originalmasken und Retikel. Die gemeinsame CORE-HUD-Registrierung ergänzt die im Waffenarchiv fehlenden, tatsächlich referenzierten Texturen.
- Raketen, Minen, Granaten und Pfeile besitzen endliche Flug-/Wurfabläufe. Erst Flugkontakt oder eine fällige Explosion verursacht einen Treffer. Minen haften vor ihrer Explosion; die Blitzgranate löst die vorhandene Originalreaktion ohne direkten HP-Schaden aus.
- Munition wird aus dem passenden Magazin oder Vorrat abgezogen. Nachladen greift am Original-Commitframe. Infinite-ammo-Varianten erhalten ihre Quell-Ausnahmen.

`tools/generate_weapon_carry.py` reproduziert die 38 Trageprofile und `tools/generate_weapon_actions.py` die 21 Basiscontroller samt Originaltabellen. Beide Ergebnisse wurden bytegenau neu erzeugt. Große Original-Bewegungsarchive sind bis 1024 Aktionen zulässig; ungenutzte Kamera-FCV-Slots werden wie im Waffenpfad übersprungen, aktive Bindungen und Dateigrenzen bleiben geprüft.

Die abschließende Tonprüfung hat zwei zusätzliche Parserfehler geschlossen: `wep19.drs` besitzt eine gültige leere lokale Soundbank, die die vorherige Waffenbank ersetzt. `wep28.drs` teilt seinen letzten melodischen Schlüssel mit dem ersten Regionwort. Dieser Zugriff bleibt innerhalb der vollständigen DLS-Grenze; Region-, Sample- und PCM-Indizes werden weiter einzeln geprüft. Alle 27 verwendeten Original-Waffenarchive bestehen DSP-/Mischerprüfungen. Im endgültigen JNI-Lauf erscheinen keine Waffen-Soundbank-Parserwarnungen mehr.

## Geräteprüfung

1. Regulär bis zu sichtbaren Gegnern spielen. Eine zusätzliche Waffe über SELECT geben, im Koffer ausrüsten und ins Spiel zurückkehren. **Einmal** frisch L2/LT drücken: Leon muss sofort anlegen. Ein während des Menüs gehaltenes L2/LT zuerst loslassen; der nächste Druck muss anlegen.
2. Pistole, Magnum, Schrotflinte, TMP, Chicago Typewriter und Matilda prüfen: zielen, feuern, loslassen und mit X/Quadrat nachladen. Mit ausgeschaltetem Infinite ammo Magazin und Reserve beobachten. TMP/Typewriter halten, Matilda-Salve prüfen.
3. Beide Gewehre prüfen: Original-Zielfernrohr, RS-Zielbewegung, D-pad auf/ab für Zoom, Schuss, Repetieren und Nachladen. Waffenwechsel und Koffer müssen die Ansicht sauber verlassen.
4. Raketenwerfer, Minen und die drei Granaten benutzen: sichtbarer Flug, Kontakt beziehungsweise Zündzeit, Treffer und Munition. Beim Bogen einen tatsächlichen Pfeilflug prüfen.
5. Bei mindestens 800 HP L1/LB halten und wieder loslassen: gesunde Messerbewegung. Infinite health ausschalten und unter 800 HP denselben Ablauf prüfen: verletzte Bewegung. RT/R2 führt bei gehaltenem Messer den Schnitt aus.
6. Zwischen mehreren Waffen wiederholt wechseln, insbesondere Handgun → Shotgun → Rifle → Rocket Launcher → Handgun. Vorhandene Ganados und Tiere müssen durchgehend sichtbar bleiben. Danach LOOK/Fernglas, Haus-/Glockenereignis, Tod/CONTINUE und SAVE/LOAD kurz gegenprüfen.

**Dieser Gerätetest steht aus.** Automatische Originaldaten-, JNI-, GLES- und PCM-Prüfungen sichern die geprüften Abläufe ab; Darstellung, Ton und Controllerverhalten auf dem konkreten Android-Gerät sind damit noch nicht bestätigt.

## Prüfungen und Build

Die Ergebnisse stehen in `K17-PRUEFUNGEN.json`, die vollständigen Testausgaben in `K17-TESTAUSGABEN.txt` und die Prüfsummen neuer/geänderter Dateien in `FILES-K17.sha256`.

| Prüfung | Ergebnis |
| --- | --- |
| Registrierte native CTest-Gruppen | 43/43 PASS |
| Original-Waffenprofile, Munition, Nachladen, Geschosse und Messerreferenzen | 3436 Prüfungen PASS |
| Produktions-JNI/GLES: Dorfregression plus 25 Waffen und sichtbare Gegner | 6807 Prüfungen PASS; davon 2735 bisherige Dorf-/K16-Prüfungen und 4072 zusätzliche K17-Prüfungen |
| Waffen-Soundarchive | 27/27 PASS |
| Dorf-Inventur und Originalereignisse | 29 Räume, 66 Verbindungen, 43 Gegnerbesitzer PASS |
| Originale Explosions-/Trefferregeln | 269320 Prüfungen PASS |
| Native Originalspielstände | 79 Prüfungen PASS |
| Java-Regressionen einschließlich Original-Save/Card-Flow | 15 Testgruppen PASS |
| Android ARM64, Java, Ressourcen, DEX und signierte APK | PASS |

Nativer Build: NDK **28.2.13676358**, CMake **3.22.1**, ARM64/API **21**. Java/Ressourcen: JDK **17**, SDK **35**, Build Tools **35.0.0**. Die APK wurde aus den endgültigen nativen Dateien, Java-Klassen, AAPT2-Ressourcen und D8-DEX mit den Android-SDK-Werkzeugen zusammengestellt, 16-KiB-ausgerichtet und mit dem vorhandenen Schlüssel signiert. Signaturprüfung für v1/v2/v3, Abgleich mit der bisherigen Signatur sowie Byteabgleich der enthaltenen nativen Bibliothek und DEX bestehen.

Ein vollständiger Gradle-`assembleDebug`-Lauf war wegen der Maven-Auflösung dieser Umgebung nicht verfügbar; Android Lint wurde nicht ausgeführt. Der erfolgreiche SDK-/APK-Build wird ausdrücklich davon getrennt. Auf einem normalen Android-Studio-System gelten weiterhin die Build-Anforderungen aus der README.

Zur Wiederholung dienen die enthaltenen Werkzeuge `k17_weapons_check DISC1`, `k17_runtime_check DISC1` oder `k17_runtime_check DISC1 --weapons-only`, `audio_check ORIGINAL_WEAPON_DRS...`, die vorhandenen Dorf-/Explosions-/Speicherprüfungen und die registrierten CTest-Gruppen. `DISC1` enthält `files/` sowie die vom Import erzeugten `decoded/`-Räume.

## Weiterer Projektumfang

K17 schließt die vier gemeldeten Fehler und den hierfür beschriebenen Waffenausbau. Der vollständige Originalport des gesamten Spiels ist weiterhin offen. Boots-/Ferry-/Boss-/Begleiter- und weitere Storyabläufe sowie vollständige EM15-Spezial-KI, Gore und sämtliche GX-/Licht-/Filterdetails behalten die dokumentierten K15-Grenzen. Die Wärmesicht benötigt noch ihre vollständige ursprüngliche Bildverarbeitung; ihr Zielfernrohr-Layout und Retikel laden bereits. Krausers Bogen auf Leon ist ein Testprofil mit Original-Bogendaten, kein regulärer Original-Leon-Aufbau. r101 meldet weiterhin ausstehende Esp46/Filter03-Glüh- und zusätzliche EAR/SST-Präsentationsdetails, während die Originalflammen und der Rauch laufen.

Maßgebliche Quellen sind die Waffen- und Spieler-Waffenmodule unter `upstream/re4/src/`, `src/game/pl_class.cpp`, `objWep`, `src/game/em_dm_val.cpp`, `src/game/cam_extra.cpp`, `src/game/snd.cpp`, `src/game/snd_sub3.cpp` und `src/st1/r101.cpp`. Der gesamte Upstream bleibt mit 2132 Dateien unverändert gegenüber K16.

Quellpakete und APK enthalten keine privaten Signierschlüssel, vollständigen DVD-Abbilder, SDKs oder Build-Caches. Die Quellpakete liefern alle Dateien vollständig.
