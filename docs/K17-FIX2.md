# K17 Fix2 – Waffen, Darstellung und Dorfabläufe

K17 Fix2 behebt die beim vollständigen K17 gemeldeten Fehler und enthält die vorherigen Fix1-Korrekturen für Links/Rechts und den Funkkontakt. Version **0.37.17-fix2 / VersionCode 51**, App-ID `com.ast.re4decomp`. Grundlage sind die bereitgestellten K17-Quellen, das letzte K17-Austauschpaket, der neue Logcat sowie die originalen Quellen und Disc-Archive. Der Originalbaum unter `upstream/re4` bleibt bytegleich.

## Installation

- **RE4-M37-K17-Fix2.apk:** über die vorhandene AST-signierte K17-App installieren. Dieselbe Signatur und App-ID; kein erneuter Disc-Import erforderlich. Die Versionsnummer ist erhöht.
- **RE4-Android-Decomp-K17-Fix2.zip:** vollständiges Android-Studio-Projekt einschließlich unverändertem Upstream und sämtlichen vollständigen Portdateien.
- **RE4-K17-Fix2-Austauschdateien.zip:** den enthaltenen Ordner `RE4-Android-Decomp` über das vollständige K17-Projekt kopieren und ersetzen. Enthält alle Änderungen seit dem hochgeladenen K17, einschließlich Fix1. Keine manuell einzufügenden Ausschnitte. Kann auch über Fix1 eingespielt werden. Danach kein älteres Update darüberkopieren.

`FILES-K17-FIX2.sha256` enthält die Prüfsummen aller neuen/geänderten Dateien gegenüber K17, außer seiner eigenen Prüfsumme. Die Archive enthalten keine neuen Disc-Archive, lokalen SDK-Pfade oder Signierschlüssel. Die bereits mit K17 gelieferten App-Assets bleiben enthalten.

## Ursachen und Korrekturen

| Gemeldeter Punkt | Befund und Änderung | Originalbezug unter `upstream/re4/src` |
|---|---|---|
| Minenwerfer: Animation und Geschoss | Der Werfer und die separat rechts gehaltene Mine benötigen getrennte Teilhierarchien. Ready-/Draw-/Down-Objektbewegungen, rechter Griff, linke Handwechsel und Schussabschluss sind angeglichen. Das Flugmodell wird mit seiner tatsächlichen FCV-Pose gezeichnet. Die Variante mit Zielfernrohr erhält ihren Kamera-/Mündungsversatz. Die unterstützten ursprünglichen Mündungsfeuer-/Rauchplatten sind aktiviert. | `wep14/wep14.cpp`, `wep14/objMine.cpp` |
| Granaten im Handgelenk | Handgelenkmatrix allein ist nicht die ursprüngliche Modellplatzierung. Der originale Griffversatz `(-95,-30,5)` und die X-Drehung um 90° sind jetzt wirksam; die zweite Granate erhält ihre separate Gürtelplatzierung. Nach dem Loslassen bleibt keine zusätzliche Granate in der Hand. Brand-/Blendgranate benutzen ihre richtigen getrennten Explosionseffekte. | `wep19/wep19.cpp`, `wep/pl_grenade.cpp`, `game/objSubWep.cpp` |
| Rocket/Special/Infinite Launcher: Laser und Visier | Alle drei benutzen die originale Scope-Art; ihr Laser ist abgeschaltet. Ready erreicht Frame 32, der Schuss speichert die Scope-Trajektorie vor dem Rückstoß und verlässt das Visier. Originaler Rücken-/Handgriff, geladenes Raketenmodell, Abschuss, Wegwerfen des leeren Einwegrohres und Rückkehr des Infinite Launchers sind berücksichtigt. | `wep/pl_rocket.cpp`, `game/objRocket.cpp` |
| Rifle/Semi-auto: nur ein Bildviertel | Die vier Originalmasken benutzen X-/Y-Drehungen in der ID-Elternhierarchie. K17 exportierte/komponierte nur Z-Drehung; die gespiegelten Platten landeten übereinander. XYZ werden jetzt vollständig verkettet und erst danach auf Canvas projiziert. Negative Plattengrößen spiegeln korrekt. Fernglas-Sonderbehandlung greift nur beim Fernglas. Sechs Gewehr-/Visierprofile, drei Launcher und der Scoped Mine Thrower sind geprüft. | `game/id_sys.cpp::idSysMove03`, `wep/pl_rifle.cpp`, originale WEP-UWF/TPL |
| Alle anderen Waffen | Alle 38 Original-Waffen-/Zubehörprofile wurden mit erster Zielkante, Ready, Scope/Laser-Auswahl, tatsächlichem Angriff, Munition und den unterstützten Nachlade-/Abschlusswegen geprüft. 25 ausrüstbare Testgegenstände laufen zusätzlich durch die Produktions-JNI/GLES. Beim Bogen sind Objektbewegung und Sichtfenster des Pfeils angeglichen. | `wep/pl_handgun.cpp`, `pl_shotgun.cpp`, `pl_machine.cpp`, `pl_bow.cpp`, WEP-Module |
| LOAD/SAVE ruckelig oder träge | Der SAVE-Dialog lief einen Frame pro Handler-Aufruf und wartete danach erneut 33 ms. Zeichen-/Handlerzeit verlangsamte damit die Originalanimation. Eine monotone 30-Hz-Uhr berücksichtigt diese Zeit und holt verspätete Frames nach. LOAD und SAVE zeichnen nur nach einer Änderung des Originalframes; ID-Kipp-/Spiegelbewegungen verwenden dieselbe reparierte XYZ-Hierarchie. Einfache EFB-Platten kopieren nur ihren betroffenen Bereich; Verzerrungspässe behalten die benötigte Quelle. | `game/sscrn.cpp`, `game/id_sys.cpp`, originale CARD-UWF |
| Träge Reaktion nach Schuss | K17 wartete zusätzlich auf das vollständige Wegstecken, bevor Bewegung möglich war. Der originale Footwork-Dispatcher prüft während dieses Absenkens bereits neue Aktionen. Bewegung, neues Anlegen, Messer und Interaktion dürfen jetzt auf dem nächsten Originaltick übernehmen. Waffenspezifischer Rückstoß, Repetieren und Raketenabschluss bleiben in ihren Originalfenstern. | `game/player.cpp::pl_R1_Footwork`, jeweilige `wep/pl_*.cpp` |
| Verzögerung beim Ziehen | Alle 38 Profile starten Draw auf dem ersten angenommenen Ziel-Tick. Zusätzliche falsche Ready-Abschlüsse beim Minenwerfer/Bogen/Launcher sind korrigiert. Die eigentliche Original-Ziehbewegung bleibt erhalten; sie ist je nach Waffe unterschiedlich lang. | jeweilige Original-Ready-Routinen und FCV/SEQ |
| r101: Ganados stehen herum | Eine pauschale Wachradiusprüfung blockierte auch normale alarmierte Ganados. Wachverhalten wird jetzt nur auf die ursprünglichen Keeper-Datensätze angewendet; ihr Standardradius beträgt 20000. Ein in der Raumwand gestarteter Gegner löst die anfängliche Überschneidung seiner animierten Körperkugel, statt jeden Gehschritt mit Weganteil null zu verlieren. Anfangsgruppe und beide Nachschubgruppen bewegen sich in der Original-Raumgeometrie. | `em10/em10.cpp` Keeper-/Suchabläufe, `em15/em15_set.cpp`, originale ESL/SAT/RTP |
| Glocke: freistehende Tür | Die externe Event-Tür ist ein ursprüngliches SetMod-Objekt mit Weltmatrizen und Platzierung null. Beim Kamera-/Motionwechsel baute der Port diese Weltpose aus lokalen Daten neu auf und setzte die Tür an den Weltursprung. Das Löschen eines Bewegungsbesitzers erhält bei diesem Objekttyp jetzt die zuletzt gelöste Weltpose, bis eine neue Motion sie ersetzt. | `game/event.cpp` externe SetMod-Typ-5-Akteure, `r101s30.evd` |
| Hausleiter verschwindet beim Klettern | Die Geometrie wurde aufgerichtet, ihre Sichtbarkeits-/Lichtgrenzen blieben jedoch an der liegenden Leiter. Grenzen und Achsen werden jetzt mittransformiert; der Renderer prüft die tatsächliche Mitte des beweglichen Modells. Alle drei Original-Hausleitern bleiben während mehrerer Kletterhöhen sichtbar und enden auf dem Dach. Die gewollte Leiterentfernung beim Glockenabschluss bleibt erhalten. | `game/EtcModel.cpp`, `st1/r101.cpp`, ETS/SMD und Original-Leiterkurven |
| LS Links/Rechts; Freeze nach Funk | Sämtliche Fix1-Korrekturen sind enthalten. Falsche Trage-Drehclips sind richtig zugeordnet; die Funkroutine stellt den waffeneigenen Griff wieder her, ohne eine Render-Pause auszulösen. Nach dem Eingangsfunk und nach dem Glockenfunk laufen Welt und Steuerung weiter. | Details in `K17-FIX1.md` |

Ein zusätzlicher Parserbefund betraf die originale Mine-Down-FCV: Laufdauer 15 mit Hermite-Stützstellen bis Frame 20. Stützstellen dürfen hinter dem letzten abgespielten Frame liegen und dessen Kurve beeinflussen. Der Parser akzeptiert das jetzt, prüft aber weiterhin Reihenfolge, Strukturgrenzen, Kanal- und Teilnummern; Sampling außerhalb der Laufdauer bleibt verboten.

## Logcat und Pausen

Im neuen Log treten bei Schüssen/Todesabschlüssen wiederholt vollständige `Geometry and GPU`-Aufbauten mit etwa **277–336 ms** auf. Gewöhnliche Ganado-Beute besteht aus einem lebenden Item-Effekt und Aufnahmezustand, nicht aus einem neuen sichtbaren ITM-Modell. Trotzdem setzte der Port bei jeder solchen Beute das Flag für einen kompletten Raumneuaufbau. Das Flag wird jetzt nur bei tatsächlich neuer sichtbarer Modellgeometrie gesetzt. Der Item-Effekt und die Aufnahme bleiben aktiv. Die Zuordnung authored Drops benutzt außerdem den wirklichen aktuellen Raum statt des r100-Standardwerts.

Der Regressionstest tötet einen tatsächlichen Dorfgegner, wartet auf dessen ursprünglichen Todes-/Dropabschluss und prüft vorhandene Beute ohne Neuaufbau der Szene. Seltene Drops mit einem echten sichtbaren Modell behalten den notwendigen Upload. Der Test wählt einen deterministischen gültigen Original-Zufallszustand; die Zufallsauswahl im Spiel wird nicht verändert.

Dies behebt konkrete Zeitverluste; eine garantierte Bildrate auf dem konkreten Android-Gerät wird daraus nicht abgeleitet.

## Prüfungen

Die endgültigen Anzahlen, Build-Metadaten und Testausgaben stehen in `K17-FIX2-PRUEFUNGEN.json` und `K17-FIX2-TESTAUSGABEN.txt`.

- Originaldaten: sämtliche 38 Profile, originaler Ziehbeginn, Griffmatrix, Scope/Laser, gespeicherte Schussrichtung und unterbrechbares Absenken; Original-Raum-SAT/RTP mit Anfangsgruppe und beiden Nachschubgruppen; sämtliche 1350 Glockenframes und alle drei Leitergrenzen.
- Java: Originale UWF-Geometrie aller zehn Visierprofile; die vier Rifle-Masken müssen alle vier Quadranten abdecken. Die 30-Hz-Uhr bleibt bei 0/8/25 ms zusätzlicher Zeichenzeit im Soll. Bestehende Menü-/Koffer-/LOAD-/SAVE-/HUD-/Raumtests sind erneut ausgeführt.
- Produktions-JNI/GLES: Flugpixel von Mine und Scoped Mine, drei Launchern, Bogen und allen drei Granaten; zehn native Scope-Layouts inklusive Originaltexturen; vollständige Kletterabläufe aller drei Hausleitern; echter Tod/Drop ohne Raumneuaufbau; Glockentür nach jedem Kameraschnitt und folgender Funk mit physischem START-Abschluss.
- Regression: r100→r101 mit ausgerüsteter Rifle, Funk-/Griff-/Stickrichtung mit allen 25 Testwaffen und weiterlaufender Welt; bestehender K17-Gesamtlauf einschließlich Raum-/Dorf-/Waffenprüfungen; 43 registrierte CTest-Gruppen. Optionale CTest-Assetgruppen behalten ihre bisherigen Standardaufrufe; Originaldaten werden ausdrücklich in den separaten Läufen geprüft.
- Android: frischer ARM64-NDK-Build; sämtliche 46 Java-Quellen und generierte Ressourcen gegen API 35 neu kompiliert; frischer DEX; Ressourcen-/Asset-Verknüpfung; gleiche AST-Signatur wie K17, v1/v2/v3, APK-/ELF-16-KiB-Ausrichtung und bytegleiche endgültige native Bibliothek im APK geprüft.

Die Grafikprüfungen laufen auf Linux mit GLES 3 über EGL und Software-Renderer. Eine physische Android-/Adreno-/Controllerprüfung und ein neuer Gradle-`assembleDebug`-/Lint-Lauf sind nicht erfolgt. Der tatsächliche SDK/NDK-, Java-, Ressourcen-, DEX- und Signaturbuild ist separat geprüft. Die ältere optionale `player_motion_tests`-Aimrate-Erwartung aus Fix1 wurde nicht zu einem neuen Gerätebeleg umgedeutet.

Die neuen Prüfziele sind im Quellpaket enthalten. Den Host-CMake-Build mit `app/src/main/cpp` konfigurieren; für JNI/GLES zusätzlich `RE4_BUILD_GL_CHECK=ON`, EGL/GLES3 und `RE4_JNI_INCLUDE_DIR` auf den JDK-Includeordner setzen. `FIXTURE_ROOT` enthält die eigenen originalen `files/` und dekodierte `decoded/rXXX.dat`; r100 benötigt seine originalen Raumblöcke. Keine Test-Discdaten werden im Paket geliefert.

```text
k17_fix2_check FIXTURE_ROOT scope.csv
k17_fix2_runtime_check FIXTURE_ROOT
k17_fix1_check FIXTURE_ROOT
k17_fix1_runtime_check FIXTURE_ROOT
k17_runtime_check FIXTURE_ROOT
java -cp JAVA_TEST_CLASSES com.ast.re4decomp.OriginalPresentationFix2Tests scope.csv
```

Der Java-Aufruf benötigt die kompilierten `StartupMath`, `OriginalFrameClock` und `tests/OriginalPresentationFix2Tests.java`. `tools/test_host.sh` führt die reine Uhr-/Transformationsprüfung bereits ohne Scope-CSV aus; der CSV-Aufruf ergänzt die tatsächliche Original-Visiergeometrie.

## Räume hinter r101 und verbleibender Umfang

Die **29 Stage-1-Räume mit 66 Originalverbindungen** sind registriert. Gemeinsame Raumdarstellung, SAT/EAT, unterstützte Türen, Fenster, Behälter und freie Funde sowie vorhandene Szenariosperren sind angebunden. Der reguläre Übergang hinter r101 ist daher nicht nur eine leere Verbindungsmarkierung.

Die individuellen Kampagnenabläufe der folgenden Räume sind dennoch nur teilweise oder noch gar nicht implementiert. Gegnerpopulationen, weitere Storyskripte, Händler, Begleiter, Boss-, Boots- und Ferryaktionen brauchen eigene native Laufzeiten. Es gibt keinen einzelnen deaktivierten Schalter, der sie vollständig aktiviert. Der umfassender angebundene Anfangsabschnitt bleibt r100/r101.

Auch dieses Update ist kein Nachweis sämtlicher Originaldetails: spezielle Gegnernavigation/-aktionen, alle GX-Indirekt-/Strip-/Filterpässe, Mine-Homing, sämtliche Hülsen-/Abgas-/Partikeleffekte und besondere Tönungen der Special-Rakete sind noch nicht vollständig portiert. Die gemeldeten Ursachen und die aufgeführten Waffen-/Renderwege wurden konkret repariert und geprüft.

## Kurzer Gerätetest

1. Fix2 über K17 installieren. Im Stand, beim Gehen und verletzt LS links/rechts prüfen; in r100 eine andere Waffe ausrüsten, regulär nach r101 gehen und Funk mit START beenden. Leon muss anschließend laufen, drehen und erneut anlegen können.
2. Mine Thrower und Scoped Mine, drei Launcher, beide Rifles sowie alle Granaten prüfen: Griff, fliegendes Geschoss, Schussabschluss, ganzes Visier und Laserfreigabe. Während des Absenkens LS bewegen bzw. erneut LT drücken.
3. LOAD/SAVE ein- und ausblenden. Auf dem Gerät auf gleichmäßiges Tempo und volle Kippdarstellung achten.
4. Dorf alarmieren und beide Nachschübe beobachten. Eine Hausleiter vollständig hochklettern; Leiter und Dachlandung müssen erhalten bleiben.
5. Glockenfilm bis zum Ende ansehen, insbesondere die Tür bei den Kameraschnitten. Den anschließenden Funk schließen und danach bewegen/schießen sowie durch den verfügbaren Ausgang weitergehen.
