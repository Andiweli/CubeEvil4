# M24 — Verfolgerkamera, Originaldrehung und belebtes r400

Versioncode **24**, Versionname **0.24.0-dev**, Paket `com.ast.re4decomp`, Projektordner `RE4-Android-Decomp`. Android ab API 21, ARM64 und OpenGL ES 3.0 bleiben unverändert.

## Kamera und Drehmechanik

M23 ließ Blickwinkel und Augenposition beim normalen Bewegen getrennt arbeiten. Dadurch konnte der rechte Stick die Sicht auf den Boden drehen, obwohl der Schulterpunkt hinter Leon blieb. M24 berechnet stattdessen einen zusammenhängenden Augen-/Zielpunkt aus dem originalen `CameraQuasiFPS`-Übergangsprofil (`cam_qfps.cpp`). Beide Punkte liegen in Leons lokalem Koordinatensystem und drehen gemeinsam mit ihm. Die Ansicht bleibt damit an Leon gebunden; sie ist keine frei integrierte Orbitkamera mehr.

RS links/rechts verändert den lokalen Blickversatz innerhalb von ±60°. RS hoch/runter blendet zwischen den originalen oberen, neutralen und unteren Übergangspunkten. Nach dem Loslassen klingt der Blickversatz mit dem originalen Faktor 0,9 je 30-Hz-Tick ab. Die korrigierten horizontalen und vertikalen Eingangsrichtungen bleiben erhalten. Waffenanlegen übernimmt den aktuellen Schulterblick über den ursprünglichen vier Frames langen Drehübergang; die Sicht folgt nicht mehr einer unabhängig animierten Mündungsrichtung.

Kameraeingaben werden vor Leons Aktionstakt ausgewertet. Deshalb übernimmt auch ein gleichzeitig mit RS gedrücktes L2 den aktuellen Blick. Beim frühen Abbruch des Ziehens und nach dem Ablegen wird der verbleibende Kamera-/Taillenwinkel in die Verfolgeransicht übernommen und anschließend zurückgeführt. Der zusätzliche Übergangswinkel ersetzt in diesem begrenzten Adapter die noch fehlende vollständige Kamera-Glättungszustandsmaschine. Die Körperrichtung wird separat vom diagnostischen Blickwinkel gespeichert; auch eine Ansicht ohne sichtbares Leon-Modell driftet bei wiederholtem Zeichnen nicht.

Bewegung, Pistole und Messer benutzen **dasselbe Übergangsprofil mit 50° Bildwinkel**. Das erfüllt die gewünschte einheitliche Bildkomposition. Der Originalcode besitzt zusätzlich eigene Waffenprofile und Zoomzustände; deren Unterschiede werden hier bewusst nicht übernommen. Dies ist eine belegte Teilportierung mit dieser Anpassung, keine Behauptung einer bereits vollständig portierten Kamerazustandsmaschine.

Bei Hindernissen prüft ein SAT-Kugelsweep mit Radius 120 den Weg vom originalen nahen Augenpunkt zum äußeren Augenpunkt. Der SEE-Attributfilter bleibt aktiv. Nach einer Verkürzung entsteht die Blickrichtung erneut aus dem tatsächlichen Augen- und Zielpunkt. Dadurch bleibt kein alter Eulerwinkel stehen, der bei verändertem Abstand auf Leons Hüfte oder den Boden zeigt. Der kontinuierliche Kugelsweep ist die Plattformanpassung an die unterstützte SAT-Abfrage; die vollständigen Original-Frustumtests sind noch nicht nachgebaut.

Die Lauf-/Drehsteuerung verwendet jetzt die originalen PAD-Sektoren aus `pad.cpp` mit Stickschwelle 30 bei axialem Bereich 72. Der moderne Stick wird nach seiner Totzone zurück in diesen Bereich abgebildet. Sobald ein originaler Bewegungssektor aktiv ist, läuft die gewählte FCV/SEQ-Animation im vollen Originaltakt; geringe Auslenkung verlangsamt ihre Kurven nicht mehr. Gehen und Rückwärtsgehen drehen um 2,4°, Rennen um 2,64° je 30-Hz-Tick. Standdrehung und schnelle 180°-Drehung verwenden weiterhin ihre Originalkurven. Rückwärtsrichtung plus neu gedrücktes B/Kreis löst die schnelle Drehung aus.

## Originalobjekte und Tiere in r400

M24 liest die originalen ETS-Platzierungen und ETM-Modelle einschließlich der zugehörigen Texturpaletten. r400 zeigt alle **33 unterstützten Platzierungen** mit 6.234 Dreiecken: sechs Türen, fünf Fenster, vier Leitern und 18 Behälter. Die Koordinaten stammen aus den Raumdaten; sie werden nicht aus SAT-Testflächen oder der Kamera erzeugt.

Diese Objekte zeigen zunächst ihre ursprüngliche Pose. Die sechs sichtbaren r400-Türen sind damit noch keine freigeschalteten Raumwechsel; Fenster, Leitern, Behälter und Gegenstände bekommen dadurch ebenfalls noch keine Aktionen. Der bestehende geprüfte r301↔r300-Türtest bleibt separat verfügbar. Zerstörung, Einsammeln, bewegliche Objektkollision und die r400-Szenarioaktionen brauchen eigene Laufzeitpfade.

Zusätzlich erscheinen die **sechs originalen EM28-Hühner** aus `etc/omake01.esl`: vier normale und zwei Tiere mit der zweiten Modell-/Texturvariante. Das Originalmodell besitzt 19 Gelenke und 1.022 Dreiecke. Fünf belegte Wait-/Walk-Kurven liefern Stand, Picken und Gehen im festen 30-Hz-Takt. Auf passendem SAT-Boden folgen die Tiere der begrenzten Bewegungslogik. Eine erhöhte Originalplatzierung ohne passenden statischen SAT-Boden bleibt animiert an ihrem ursprünglichen Platz, statt auf einen tiefer liegenden Boden zu springen.

Flucht, Flug, Schaden, Eier, Tiergeräusche und Kampagnenaktivierung sind damit noch nicht portiert. Andere ESL-Gegner werden nicht durch Hühner ersetzt. Der bisherige einzelne EM12-Nahkampfgegner und seine Original-/Testprovenienz bleiben erhalten.

## Erste weltbezogene Effekte

Die global aktiven r400-Sequenzen SST 3 und SST 13 erhalten ihre originalen EFF-Records und ANM-Texturen. Der unterstützte Teil umfasst 13 Sprite-Records und vier Animationssätze: horizontale Feuerstellenglut sowie Umgebungslichtstrahlen. Die Effekte werden in Weltkoordinaten mit ihren ursprünglichen Platzierungen, Farb-/Blendverläufen und unterstützten UV-Bewegungen ausgegeben.

Dies ist **noch kein vollständiger Fackel- oder Scheiterhaufeneffekt**. Fackelflammen, Scheiterhaufenfeuer, Rauch sowie die gebietsabhängigen Freigaben über EAR bleiben offen. Solche Sequenzen werden nicht pauschal im ganzen Raum aktiviert. Die bisher unterstützten Himmel-/Wassereffekte aus M22 bleiben erhalten.

## Installation und Daten

Das M24-ZIP enthält nur neue oder geänderte **vollständige Dateien über dem kompletten M23-Stand einschließlich M22**, mit Ordnerstruktur unter `RE4-Android-Decomp/`. Ein unvollständiger M23-Stand benötigt zuerst die M23-Reparatur. Keine GCM-/ISO-Dateien, extrahierten Spieldaten oder APKs sind enthalten.

Bestehende vollständige Disc-Imports enthalten die zusätzlich benötigten Daten bereits. r400, `em/em28.drs` und `etc/omake01.esl` liegen auf Disc 1; die App prüft die benötigten importierten Quellen. Ein erneuter Import ist bei vorhandenem vollständigem Datenbestand nicht nötig. Der Importer extrahiert seit der ersten Basis alle vorgesehenen Nicht-Videodateien. Der Datenpfad bleibt `Android/data/com.ast.re4decomp/files/game/`.

Dateien im bestehenden Projekt ersetzen, laufende Builds beenden und die generierten Verzeichnisse `app/.cxx` und `app/build` löschen. Danach mit JDK 17 aus dem Projektordner:

```text
./gradlew clean assembleDebug lintDebug
```

Unter Windows:

```text
gradlew.bat clean assembleDebug lintDebug
```

Der installierte Build muss Versioncode 24 beziehungsweise `0.24.0-dev` besitzen. Die Nahkampf-Schaltfläche nennt weiterhin r400.

## Gerätetest

1. r400-Nahkampftest starten. Leon muss mit neutralem RS sofort in einer festen Schulterkomposition erscheinen. Geradeaus gehen, im Stand drehen und Kurven laufen; die Kamera muss gemeinsam mit Leon folgen.
2. RS seitlich und vertikal bewegen, loslassen und die Rückkehr zur Neutralstellung prüfen. Die Ansicht darf sich nicht mehr unabhängig um einen entfernten Bodenpunkt drehen. Direkt aus einem seitlichen Schulterblick L2 drücken, loslassen und dasselbe mit L1 prüfen: einheitliches Profil, kein Wechsel zur alten freien Orbitkamera.
3. An Wände herangehen und die Kamera verkürzen lassen. Zielpunkt und Blickrichtung müssen zusammenpassen; bei engem Abstand darf kein Blick in Leons Körper entstehen.
4. Rückwärtsrichtung und neu gedrücktes B/Kreis für die schnelle Drehung prüfen. Im Griff kurze LS-Links/Rechts-Wechsel testen; die M23-Gegenwehr bleibt erhalten.
5. Durch r400 laufen: originale Türen, Fenster, Leitern und Behälter sowie Hühner prüfen. Die erhöhte Tierplatzierung darf nicht durch den Boden fallen. Die Objektaktionen sind in dieser Stufe noch nicht aktiv.
6. Pfeil zum pausierten Menü, Fortsetzen, Controllertrennung und Grafik-Kontextwiederherstellung prüfen. Anschließend den bisherigen r301→r300→r301-Türtest wiederholen; ein fehlgeschlagener Ladevorgang muss die vorhandene Szene erhalten.

## Prüfstand und Grenzen

Die ausgeführten Prüfungen und ihre konkreten Ergebnisse stehen in [VALIDATION.md](VALIDATION.md). Diese Anleitung nennt keine vorab angenommenen Testzahlen. Geräteausführung, Treiberverhalten und Bildkomposition benötigen zusätzlich den obigen Gerätetest.

M24 bleibt ein begrenzter nativer Bewegungs-/Kampftest. Kampagnenszenario, vollständige Gegner- und Tier-KI, Inventar, Audio, Speichern, vollständige Objektaktionen und die gesamte GX-/TEV-/Effektruntime fehlen weiterhin. Die neuen sichtbaren Modelle sind ein überprüfbarer Ausbau der Raumdarstellung, noch kein vollständig spielbares r400-Level.
