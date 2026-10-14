# K17 Fix5 – vollständige Austauschdateien

Version **0.37.17-fix5**, VersionCode **54**. Dieses Quellcodepaket enthält die Änderungen aus **Fix1 bis Fix4** und die fünf aktuellen Korrekturen. Keine APK und keine Spieldaten.

## Einspielen

Das ZIP über den K17-Projektordner entpacken und die gleichnamigen Dateien vollständig ersetzen. Es enthält ganze Dateien, keine Ausschnitte oder Diff-Fragmente. Danach das Projekt wie bisher bauen. Die Originaldateien unter `upstream/re4/src` bleiben bytegleich.

## Korrekturen und Originalbelege

| Punkt | Ursache und Änderung | Vergleich im Original |
|---|---|---|
| Rifle-Nachladen | Die allgemeine Nachladebehandlung konnte bei geneigtem Ziel zu früh ins Zielen zurückkehren. Rifle und Semi-Auto verwenden jetzt ihren eigenen Ablauf: Reload-Motion mit Flags5, scopefreies Nachladen und Scope-Rückkehr fünf Sequenzframes vor dem Ende. Beim Loslassen nach dem Nachladen verwendet die Bolt-Rifle auch die besondere Spieler-Motion ARC1C und Waffen-Motion ARC24. | `wep/pl_rifle.cpp`, `wep09/objSniper.cpp`, `wep10/objHkSniper.cpp` |
| Ruhiges Zielfernrohr | Fix4 hielt die Richtung beim Schuss fest, enthielt aber noch nicht die eigene Atembewegung der Scope-Kamera. Ergänzt sind die zwei Cosinusphasen, die geglättete Originalamplitude und die Zufallsaktualisierung alle 151 Scope-Ticks. Schuss/Repetieren erhalten weiterhin den gewählten Zielwinkel und Zoom; die Atembewegung läuft mit 30 Hz. | `game/cam_extra.cpp::CameraScope::move` |
| Inventarmodell | Leons Menümodell war ein einmal gerendertes Standbild. Jetzt läuft die originale SS-Waffenpose auf einem unabhängigen Skelett mit wiederholter Originalanimation. Kleidung und Hände folgen diesem Skelett, die Waffe der Original-Handanbindung. Native Bildausgabe und Java-Anzeige aktualisieren sich während der Menüpause. Meshes, Texturen und Renderziele werden wiederverwendet. Der Spielraum bleibt pausiert; im App-Hintergrund pausiert auch das Menümodell. | `Sscrn/ss_main.cpp`, `Sscrn/ss_model.cpp` |
| Blut | Die native Weiterleitung kombinierte Gegnerkennung und Ereignisnummer zu einem Wert über 32 Bit. Das Effekt-System wies ihn ab, bevor ein Sprite entstand. Jetzt werden eindeutige 32-Bit-Kennungen verwendet. Zusätzlich sind die fehlenden Originalfolgen EST 0C für Maschinenwaffen und EST 87 für Rifles einschließlich Bodenblut geladen. Der originale Release-Wert FF bleibt erhalten. | `em10/em10.cpp::em10BloodSet`, `game/em_sub.cpp::EmGetDmPos`, originale EM15/CORE-EFF/EST/ANM |
| Passive Ganados in r101 | Der Adapter für den Original-Schadensalarm arbeitete immer im Raumkontext r100. Im Original gilt für r101 ausdrücklich eine andere Regel: Schaden an einem aktiven Ganado alarmiert andere auch ohne die sonstigen Entfernungs-/Blickwinkelbedingungen. Jetzt wird der tatsächliche Raumkontext weitergegeben; noch nicht aktive, geskriptete Wellen werden aus dem Alarm ausgeschlossen. Der normale Ganado-Routenaufruf erhält außerdem die ursprünglichen Atari-Bits 2301 einschließlich der durch `init` gesetzten Bits 300. | `em10/em10.cpp::em10SomebodyDamageNowCk`, `game/atariInfo.cpp::cAtariInfo::init`, `game/route_ck.cpp::RouteCkToPos` |

Blutmenge und Bodenblut hängen weiter von Waffenart, Trefferdistanz, Kameraabstand und den Original-Effekten ab. Eine Blutlache wird nicht pauschal bei jedem Treffer erzeugt. Vor dem Alarm bleiben die originalen Sicht-, Hör-, Wachposten- und Entfernungsregeln erhalten.

## Prüfungen

Die endgültigen Ergebnisse stehen in `K17-FIX5-PRUEFUNGEN.json` und `K17-FIX5-TESTAUSGABEN.txt`. Verwendet wurden die bereitgestellten Originaldaten, einschließlich r101-SAT/EAT/RTP, EM15, CORE und SS-Waffenarchiven, sowie das neue Logcat `Eingefügter Text(3).txt`.

43 allgemeine Tests und die Laufzeitregressionen für Fix1–Fix4 sind bestanden. Die gezielte Fix5-Prüfung enthält 22.072 Vergleiche mit Originaldaten und 343 Prüfungen über Produktions-JNI/GLES.

Die Prüfungen umfassen beide Rifles mit allen sechs vorhandenen Grund-/Aufsatzvarianten, geneigtes Zielen und gehaltenen/losgelassenen Zielknopf beim Nachladen; den Original-Routenvergleich für r101; konkrete Alarm- und Laufversuche an acht Spawnpunkten an Häusern und am Zugang; Menüanimationen für 38 Waffen; echte JNI-Menüpause und Treffer-zu-Blut-Weiterleitung sowie GLES-Ausgabe und Wiederherstellung nach Kontextverlust.

Die Laufzeitprüfung erfolgt auf Linux mit EGL/GLES3-Softwarerendering. Der Android-ARM64-Code und die Java-Quellen werden zusätzlich kompiliert. Ein Test auf deinem physischen Androidgerät ist damit nicht ersetzt; insbesondere Darstellung und Laufwege an der von dir beobachteten Ecke sollten dort gegengeprüft werden. Besondere Gegner-Hindernisaktionen und spätere Raum-/Storyabläufe sind weiterhin nur im bisherigen Portierungsumfang enthalten.
