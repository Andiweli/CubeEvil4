# K17 Fix1 – Links/Rechts und Funkkontakt in r101

Gezielter Nachtrag zum bereitgestellten vollständigen **K17** und dem bytegleichen letzten K17-Austauschpaket. Android bleibt **0.37.17-dev / VersionCode 50**. Der mitgelieferte Original-Quellbaum unter `upstream/re4` wird nicht verändert.

## Einspielen

- `RE4-K17-Fix1-Austauschdateien.zip`: den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden vollständigen K17-Projekt zusammenführen und die Dateien ersetzen. Jede enthaltene Datei ist vollständig. Das alte K17-Austauschpaket anschließend nicht erneut darüberlegen.
- `RE4-Android-Decomp-K17-Fix1.zip`: alternativ das vollständige korrigierte Android-Studio-Projekt verwenden. Lokale SDK-/Signierkonfiguration bleibt Bestandteil der eigenen Buildumgebung.
- `RE4-M37-K17-Fix1.apk`: optional die fertige ARM64-APK über die vorhandene K17-App installieren. Ihre Signatur stimmt mit der bisherigen AST-signierten K17-APK überein. App-ID und Versionsnummer sind unverändert.

Ein erneuter Disc-Import ist nicht erforderlich.

## 1. Vertauschtes Drehen

Die physischen Stickwerte, die ursprünglichen digitalen Bewegungssektoren und die feste Lenkung beim Gehen/Laufen waren bereits konsistent. Der Fehler lag in der Auswahl der Drehbewegungen:

- Das Standard-Pistolenprofil ordnet seine zwei Drehclips korrekt zu.
- Der K17-Generator ordnete die Slots `0x0D/0x0F` zusätzlicher Waffen in der entgegengesetzten Reihenfolge zu. Damit wählte eine richtige Rechts-Eingabe den falschen Original-FCV-/SEQ-Drehablauf.
- Die beiden Drehclips der verletzten Figur waren ebenfalls gegensinnig zu den physischen Locomotion-Namen gebunden.

`generate_weapon_carry.py` ordnet die originalen Motion- und Sequence-Paare jetzt in derselben physischen Links-/Rechtsreihenfolge wie das Standardprofil an. Die 38 generierten Einträge wurden neu erstellt. In `leon_preview.cpp` sind die zwei verletzten Drehclips entsprechend korrigiert. Eingabesignale, Menübedienung, Fernglas, Gegenwehr sowie bestehende Lauf-/Zielachsen sind unverändert.

Die neue Prüfung projiziert die tatsächliche Änderung von Leons Blickrichtung auf die rechte Achse der produktiven Schulterkamera. Sie prüft beide Stickrichtungen, Stillstand/Vorwärts/Rückwärts, Gehen/Laufen, drei Startwinkel sowie 1200/700 HP für den Standardaufbau und alle 38 Original-Trageprofile. Dadurch kann ein passend benannter, tatsächlich falsch drehender Clip den Test nicht bestehen.

Originalbezug: `src/game/pad.cpp`, `src/game/player.cpp::pl_R1_Turn`, die `WEP_MOT`-Tabellen der Waffenmodule und die verletzten PL00-Bewegungen.

## 2. Freeze nach dem Funkkontakt

Der beigefügte Log zeigt erstmals um **14:55:20.562**:

`RE4Scene: Rendering paused: Leon: original grab is not ported`

Die Meldung stammt aus `LeonPreview::setRoomScriptLeftHand`, nicht aus einem gegnerischen Haltegriff. Die zusätzliche Waffe hat ihren bereits geladenen linken Griff im Profilmodell; K17 kennzeichnet diesen intern mit Handtyp `0`. Die Funkanimation wählt bei Frame 22 Handtyp `2`. Beim Beenden stellt das Originalkommando `0x63` den vorherigen Handtyp wieder her.

Der K17-Setter erlaubte nur `2/4` und warf beim Zurückstellen auf `0` eine Ausnahme. Der äußere Render-Handler setzte anschließend die Pause, während der Funkablauf nicht abgeschlossen war. Das erklärt das stehende Bild und die weiteren laufenden Meldungen.

Der Setter akzeptiert jetzt auch den vorhandenen waffeneigenen Griff eines geladenen Zusatzprofils und stellt seine Sichtbarkeit wieder her. Er lädt keine Ersatzmodelle und baut keine Gegner-/Raumpuffer neu auf. Ungültige Handtypen bleiben Fehler. Die Fehlermeldung benennt jetzt eindeutig die fehlende Skript-Hand.

## Aktuelle Prüfungen

Die endgültigen Ergebnisse und Testausgaben stehen in `K17-FIX1-PRUEFUNGEN.json` und `K17-FIX1-TESTAUSGABEN.txt`.

- Originaldatenprüfung `k17_fix1_check`: 7060 Kontrollen mit dem Standardprofil und 38 Trageprofilen. Funkanimation, sichtbare Handrückgabe, erneuter Funkkontakt, früher Abbruch und Bewegung nach der Rückgabe sind enthalten.
- Produktions-JNI/GLES `k17_fix1_runtime_check`: 7143 Kontrollen beim tatsächlichen r100→r101-Übergang mit vorher ausgerüstetem Gewehr, ursprünglichem Eintrittsfunk und Kontrollpunkt, anschließend Funkkontakt und Links-/Rechtssteuerung mit allen 25 ausrüstbaren Cheat-Gegenständen. Die Welt muss nach jedem Dialog wieder weiterlaufen.
- Bestehende K17-Produktionsprüfung: 6807 Kontrollen, einschließlich Dorfereignissen, Raumwechseln, erstem Zieldruck, Waffenangriffen und tatsächlich sichtbaren Gegnerpixeln nach Waffenwechseln.
- Bestehende Original-Waffenprüfung: 3436 Kontrollen.
- Registrierte CTest-Gruppen: 43/43 bestanden. Assetabhängige optionale Gruppen werden dabei gemäß ihrer bisherigen Standards ohne zusätzliche Disc-Argumente gestartet; die oben genannten Prüfungen verwenden ausdrücklich die Originaldaten.
- Original-Tragetabelle wird mit dem aktualisierten Generator bytegleich reproduziert.
- Nativer Android-ARM64-Build, v1/v2/v3-Signaturen, bisherige K17-Signatur, APK-/ELF-16-KiB-Ausrichtung und enthaltene endgültige native Bibliothek sind geprüft. Java, DEX, Ressourcen und übrige APK-Nutzdaten stimmen bytegleich mit K17 überein.

Ein zusätzlicher alter Assetaufruf `player_motion_tests PL00.drs WEP01.drs` scheitert in **unverändertem K17 und Fix1 identisch** an seiner Erwartung `actor.aimRate() == 0.f`. Dieser bereits vorhandene Befund wurde nicht durch eine Änderung an der Zielsteuerung oder am Alt-Test verdeckt. Die gemeldete Drehrichtung wird vollständig durch den neuen Originaldaten-/JNI-Kameratest abgedeckt. Dieser Nachtrag untersucht die separate Aimrate-Erwartung nicht weiter.

Die APK verwendet den frisch gebauten nativen Code und die nachweislich unveränderten K17-Java-/Ressourcendaten. Ein neuer vollständiger Gradle-`assembleDebug`-/Lint-Lauf ist damit nicht behauptet.

## Gerätetest

1. Mit dem linken Stick im Stand nach links/rechts drehen; ebenso beim Gehen und Laufen. Standardpistole, eine Cheatwaffe und die verletzte Figur kurz prüfen.
2. In r100 eine andere Waffe ausrüsten, regulär nach r101 gehen und den Funkdialog beenden. Anschließend müssen Bewegung und ein frischer L2/LT-Zieldruck funktionieren.
3. Den Funkdialog zusätzlich mit START schließen und zwischen mehreren Waffen wechseln. Die Handmodelle und die bereits sichtbaren Gegner müssen erhalten bleiben.

Ein Test auf dem konkreten Android-Gerät steht aus. Der Umfang dieses Updates sind die zwei gemeldeten Fehler; die in den K17-/K15-Dokumenten beschriebenen übrigen Portierungsgrenzen gelten weiterhin.
