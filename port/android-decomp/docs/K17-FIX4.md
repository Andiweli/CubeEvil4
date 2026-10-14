# K17 Fix4 – Rifle-Zielen, Dorf, Sichtbarkeit und Tiere

Version **0.37.17-fix4 / VersionCode 53**, App-ID `com.ast.re4decomp`. Das Paket enthält sämtliche bisherigen Fix1–Fix3-Korrekturen. Vergleichsquelle ist der mitgelieferte Originalcode unter `upstream/re4/src`; Modelle, Animationen, ESL, SAT, Audio und Aufsätze kommen aus den vorhandenen Originalarchiven. Der ursprüngliche Quellbaum wird nicht bearbeitet. Das beigefügte Geräte-Logcat wurde berücksichtigt.

## Austauschdateien installieren

`RE4-K17-Fix4-Austauschdateien.zip` enthält **vollständige neue bzw. geänderte Dateien** mit dem Projektpräfix `RE4-Android-Decomp/`. Den enthaltenen Ordner über das vollständige K17-Projekt oder einen vorhandenen Fix1–Fix3-Stand kopieren und Dateien ersetzen. Anschließend das Projekt wie bisher selbst bauen. Kein älteres Update darüberkopieren.

Es wird keine APK geliefert. Keine Disc-Archive, SDK-/Build-Caches, Signierschlüssel oder lokalen SDK-Pfade werden hinzugefügt. `FILES-K17-FIX4.sha256` enthält die Prüfsummen aller Austauschdateien außer sich selbst; die früheren Dokumente und Manifeste beschreiben ihre historischen Versionen.

## Befunde zu den sieben Meldungen

| Meldung | Originalabgleich und Änderung | Originalquelle |
|---|---|---|
| Rifle / Semi-Auto besitzen schon einen Scope | **Das ist im gelieferten Originalcode vorgesehen.** Die Basisprofile `0x2e` und `0x2f` aktivieren `CamCtrl.startScope(0,0)` und deaktivieren den Laser. Gekaufte Scope-Aufsätze und Infrared Scope wählen zusätzliche Varianten mit anderen Zoom-/Darstellungsdaten. Die Grundansicht wird deshalb beibehalten; Combine/Remove aus Fix3 berücksichtigt die optionalen Aufsätze. | `wep/pl_rifle.cpp::wep09_r3_set00`, `wep09_r2_set`; `game/item.cpp` |
| Blick schnellt nach dem Schuss hoch | Der Port überschreibt gespeicherten Rifle-Pitch durch eine allgemeine Waffen-Schussbewegung und setzt eine zusätzliche Fire-Pose. Scope-Waffen erhalten diese dritte-Person-Korrektur nicht mehr. Die Rifle behält beim Schuss ihre ursprüngliche Aim-Pose; Repetieren und Rückkehr erhalten Pitch und Zoom. | `wep/pl_rifle.cpp::wep09_r3_fire00`, `wep09_r3_fire20`; `saveScopeParam` |
| Bäume flackern nach der r100-Kurve hinein | GLES hatte gleichartige Modell-Draws trotz unterschiedlicher Sortier- und Lichtgrenzen zusammengefasst. Dadurch konnte das erste unsichtbare Modell weitere sichtbare Modelle ausblenden. Außerdem wurde für Draw-Order 0 der Modellfuß als Culling-Mitte verwendet. Zusammenfassung berücksichtigt jetzt die vollständigen Grenzen; die Sichtbarkeitskugel liegt im Zentrum des zugehörigen Modellvolumens, damit die Baumkrone rechtzeitig sichtbar bleibt. | Original-SMD/RoomModel-Grenzen; Port `scene_gl.cpp` |
| Ganados / Dachgegner | Alle 43 Ganado-Besitzer behalten ihre originalen ESL-Positionen und Nachschubgruppen. Keeper, Alarm und RoofWait bleiben wirksam. Neu sind die ursprünglichen Roof-Damage-, Blow-Air- und Landungsclips. Die normale Bodenbewegung klemmte vorher auch zurückgeschleuderte Körper an der Kante fest. Air-/Roof-Routinen benutzen jetzt ihre ursprüngliche Bewegung und Schwerkraft, ohne normale Kantenstopps oder Körperblocker. **Nicht jeder tote Dachgegner fällt:** `em10RoofDmCk` prüft Kollisionsattribute `0x00182810`, Richtung, Typ, Rang und gegebenenfalls Zufall. Ein Gegner kann daher auch in der GameCube-Logik auf dem Dach liegen bleiben. | `em10/em10.cpp::em10RoofDmCk`, `em10_R1_Dm_Roof`, `em10_R1_Dm_Blow`; r101 ESL/SAT/RTP |
| Angriffssound / Musik bleibt stehen | Kettensägen-Angriffsloops werden beim Verlassen der Aktion beendet, Catch-/Escape-Cues benutzen ihren originalen Soundblock 6. Glockenentfernung stoppt alle Ganado-Audiobesitzer und verwirft deren ausstehende Cues. Die im Original getrennte Musiksteuerung für Kämpfe nach der Glocke startet einmal und blendet nach dem letzten Ganado in drei Sekunden aus. Die Musik vor der ersten Glocke läuft gemäß Original auch über kurze Nachschubpausen hinweg. | `em10/em10.cpp` Kettensägen-Routinen; `st1/r101.cpp::r101_FindPlayer2`, `r101_Event30` |
| Waffe verspätet nach Messer | Der Port wartete bis zum Ende der gesamten Knife-Holster-Animation. Der Originalcode tauscht Messer/Waffe bereits bei `MotionCheckCrossFrame(4)`. Die Waffe erscheint jetzt in diesem Moment, während die Hände noch ihre Rückbewegung ausführen. Die Sonderregel für Launcher-Waffennummer 13 bleibt erhalten. | `wep/pl_knife.cpp::knife_r3_ready10`, `knife_r3_down10` |
| Kuh und Hühner fehlen | Das Original-ESL enthält eine EM26-Kuh auf Slot `0x34` und zwei EM28-Hühner auf `0x4e` / `0x4f`. Diese wurden bisher nicht instanziiert. Neu angebunden sind Originalmodelle, Texturvarianten, FCV-/SEQ-Motion, Idle-/Flucht-/Sprung-/Treffer-/Todesabläufe, zugehörige Soundbanken, Trefferkapseln, Beute und ursprüngliche Speicherbits. Sie zählen unabhängig von den 43 Ganados und verändern keine Kampfwellengröße. | `em26/em26.cpp`, `em28/em28.cpp`; `etc/emleon00.esl`, `em26.drs`, `em28.drs` |

Die originale Kuh steht bei `(-4590, 0, 12190)`. Die Hühner beginnen bei `(-10840, 0, 2660)` und `(-7860, 0, 6300)`. Vor dem Alarm dürfen die Hühner wegen ihres ESL-Warteflags am Platz bleiben; sie fliehen vor Leon bzw. Schüssen. Das ist keine Ganado-Passivität.

## Prüfung und Grenzen

Die endgültigen Ausgaben stehen in `K17-FIX4-TESTAUSGABEN.txt`, maschinenlesbare Ergebnisse in `K17-FIX4-PRUEFUNGEN.json`.

| Prüfung | Ergebnis |
|---|---|
| Neue Originaldaten-/Spielabläufe | 3454 Prüfungen bestanden |
| Neue Produktions-JNI/GLES-/Audioabläufe | 306 Prüfungen bestanden; alle drei Tiermodelle tragen sichtbare Pixel bei |
| r100/r120: Nebel, Vegetation, Transparenz, LOD und Specular | 6585 Prüfungen bestanden |
| Waffen-/Save-Regression | Alle 38 Waffenprofile, 3437 Waffen- und 79 Save-Prüfungen bestanden |
| Allgemeine Regression | 43/43 CTest-Gruppen; 14 Java-Vertragsgruppen und 81 zusätzliche UI-/Scope-Prüfungen bestanden |
| Android-Native-Code | ARM64-NDK-Kompilierung erfolgreich; AArch64 und 16-KiB-ELF-Ausrichtung bestätigt |

- Neue Originaldatenprüfung: beide Rifle-Basisprofile und vier Zubehörprofile, positive/negative Zielneigung, Zoom und je drei Schüsse; Messer-Rückkehr für sämtliche 38 getragenen Waffenprofile; alle 43 Ganado-Spawns mit lokalem erreichbarem Angriffsziel; beide Dachfallwege; drei Tiermodelle, Originalpositionen, Flucht, Treffer, Tod und Wiederherstellung; Nachkampf-Musiksteuerung.
- Produktions-JNI/GLES: tatsächliche Pixel aller drei Tierbesitzer, Messer-Rückkehr, Tier-SAVE, Original-Audiobanken, Kettensägen-Loop-Ende, Nachkampfmusik und zwei gezielte Modell-Sichtbarkeitsfälle.
- Die bisherigen Fix1–Fix3-Tests werden für den neuen Stand ebenfalls wiederholt; maßgeblich sind die tatsächlich dokumentierten Ergebnisse. Das Update erzeugt keine APK und behauptet keinen neuen Gradle-/Lint- oder Gerätelauf.

Grafiktests verwenden Linux/EGL/GLES 3 mit Software-Renderer und eigenen Originaldaten. Ein physischer Android-/Adreno-/Controllerlauf, die subjektive Wirkung der Kurvenfahrt sowie ein vollständiger Durchlauf jeder möglichen Ganado-Hindernissituation stehen aus. Die Einzeltests aller Ganados bestätigen Platzierung und Eintritt in Angriff/Griff bei erreichbarem Nahziel, nicht eine vollständig portierte Spezialnavigation durch jedes Gebäude.

Die vollständige Ganado-Spezial-KI, sämtliche Tier-Partikel-/Gore-/IK-/Atari-Details und alle GX-Sonderdurchgänge bleiben außerhalb dieses Updates. Mine-Homing, vollständige Thermographie und spätere Kampagnenabschnitte behalten die bereits dokumentierten Ausbaugrenzen. Das Paket behauptet keinen vollständigen GameCube-Port.

Neue Hostziele:

```text
k17_fix4_check FIXTURE_ROOT
k17_fix4_runtime_check FIXTURE_ROOT
```

`FIXTURE_ROOT` enthält eigene Originaldaten unter `files/` und dekodierte Räume unter `decoded/`. Die bestehenden JNI/EGL/GLES-CMake-Optionen gelten weiter.
