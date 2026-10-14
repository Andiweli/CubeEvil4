# M34: Koffermenü mit Originaldaten

Das Koffermenü verwendet jetzt die originalen Disc-Texturen und ID-Layouts für die Reiter, Peseta-Ziffern, Metallumrandung, Namensleiste und Hinweisleiste. Die bisherige gezeichnete Titelzeile und Reiter-Schaltflächen entfallen. Es werden keine Spielgrafiken mit der APK oder dem Quellcode ausgeliefert; die vorhandenen Disc-Dateien bleiben die Datenquelle.

## Quellen und Darstellung

| Bestandteil | Originalquelle | Umsetzung |
| --- | --- | --- |
| Reiter und Auswahl | `ss_cmmn.dat` 0x0b, `ss_main.cpp::idMainMenu` | Originalpositionen, Elternmatrizen, Farben und additive Auswahlstufen |
| Ziffern und Grundgrafiken | `CORE_COCKPIT_TEXTURE` 0x1d, `ss_cmmn.dat` 0x07 | Originalbilder und acht Stellen mit unterdrückten führenden Nullen |
| Umrandung und Hinweise | `ss_cmmn.dat` 0x09/0x0c/0x10 | Originalbilder, ursprüngliche Zeichenreihenfolge |
| Koffer | `ss_pzzl.dat` 0x1a5..0x1a9, `ss_pzzl.cpp::caseModelMove` | Alle vier Originalgrößen; projiziertes Originalmodell, 100 Einheiten je Zelle, Neigung −0,57 rad, Position −95/98/−300 |
| Leon rechts | `pl00.drs`, `ss_wep00/01/02/07.dat`, `ss_model.cpp` | Originalkörper, Anbauteile, Hände, Waffe, FCV-Pose bei Frame 0; Originalposition 850/−1370/0 und Drehung −0,3 rad |
| Licht | `CORE_LIGHT` Cut 9, `ss_cmmn.dat` 0x12 | Originalambiente plus die drei von `sscrnLightCreate` einzeln aktivierten Lampen; keine frei gewählte Aufhellung |
| Gegenstände | `ss_pzzl.dat` Originalmodelle und -texturen | Originalmodell-Vorschaubilder auf die projizierten Zellen; Drehung und Auswahl bleiben bedienbar |
| Namen, Dokumente und Schlüsselbilder | Original MesData/Font-, Datei- und Item-Daten | Bestehende Originaltitel, 156 Dokumentseiten und originale Schlüsselbilder bleiben verwendbar |

Ein Fehler im bisherigen ID-Dekoder wurde behoben: ein numerischer `maskId` allein schaltet keine Maskentextur ein. Wie `IDSystem` aktiviert erst `tex_flag & 1` die zweite Stufe. Ohne diese Prüfung wurde die unbenutzte Null als CORE-Rauschtextur auf fast alle Menügrafiken multipliziert. Außerdem werden die ID-Quads in der ursprünglichen LIFO-Reihenfolge des OT-Buckets gezeichnet, damit Auswahlplatten die Beschriftung nicht überdecken.

Die einzelne originale Hinweisgrafik mit eingebautem GameCube-`X` und „Move selection“ wird ausgeblendet, weil sie zur universellen Controllerbelegung nicht passt. Namensleiste und Rahmen bleiben original. Der deutsche Footer zeigt die tatsächliche Belegung: `Y` nimmt einen Gegenstand zum Verschieben auf und dreht ihn anschließend; `X` beginnt das Kombinieren; `LB`/`RB` wechseln die Reiter. Die Tastenbelegung selbst wurde dabei nicht geändert.

Die Kofferdeckel-Animation endet laut ursprünglichem FD-ID-Kurvenwert bei 0°. Das Modell bleibt in der Originalkameraprojektion teilweise oberhalb des Bildrandes. Die Reiter zeichnen darüber. Koffermodelle verwenden absichtlich beide Seiten (`CullMode=2`), während Leons Standbild die ursprüngliche einseitige Darstellung verwendet.

## Threading und Prüfung

Canvas lädt und dekodiert keine Disc-Dateien synchron. Ein einzelner Hintergrundworker veröffentlicht Layouts und Bitmaps auf dem UI-Thread; der Java-Bildcache ist auf acht Millionen Pixel begrenzt. Die originalen Motion-Routinen für die Leon-Pose laufen ausschließlich beim Öffnen beziehungsweise Waffenwechsel auf dem GL-/Simulationsthread. JNI liefert der Menüansicht danach nur das gecachte Portrait.

Hostmessung mit der vorhandenen Disc: Koffermodelle ungefähr 0,03–0,05 s je Größe, Portraits ungefähr 0,11–0,14 s je Waffenpose. Das sind Hostwerte, keine Android-Gerätemessung. Die PNG-Komposition zur Sichtprüfung ist eine Hostdarstellung derselben Assets, Modellbilder, Projektion und Ebenen; sie ersetzt keinen Ingame-Test auf dem Gerät.

Die automatischen Prüfungen decken die Aktivierung von Masken, den Endpunkt des originalen ID-Pfades, alle vier Koffergrößen und drei bewaffnete Portraits ab. Ein Portrait darf nicht durch fehlerhafte Bindeposen in den linken Kofferbereich explodieren. Die unabhängige Java-Geometrieprüfung kontrolliert auch angrenzende Zellkanten und die Perspektive.

## Noch keine vollständige 1:1-Menüemulation

Leons Menüpose ist ein Original-Frame, kein fortlaufend animiertes Idle. Öffnen/Schließen des Koffers, dynamische ID-Kurven, pulsierende Auswahl sowie alle GX-Specular-/TEV-Details sind nicht vollständig emuliert. Die Gegenstände verwenden projizierte Modellvorschaubilder statt eigener beweglicher 3D-Modelle pro Kofferstück. Schlüssel-/Wertgegenstandslisten, Karte, Dateiliste, Aktions-/Bestätigungsfenster und Android-Optionen behalten teilweise die bisherigen Canvas-Anordnung und Textdarstellung. Ihre Grundfunktionen und die Originaldokumentseiten bleiben vorhanden. Das Ergebnis ist deshalb eine deutlich originalnähere Kofferansicht, aber kein Nachweis pixelgenauer Übereinstimmung des gesamten Menüs.
