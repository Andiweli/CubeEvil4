# Zweite r100-Korrektur: Ergebnis und Prüfgrenzen

Stand: 6. Oktober 2026. Grundlage ist das vorhandene M37-Projekt mit dem ersten r100-Korrekturpaket, den bereitgestellten Bildern und dem Logcat sowie dem eingefrorenen GameCube-Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7`. Paketkennung `com.ast.re4decomp`, Version `0.37.0-dev`/38 und das Illuminados-Icon bleiben erhalten.

Dieses Paket enthält weitere Korrekturen für die zwölf zuletzt gemeldeten Punkte. Native Laufzeittests benutzen die tatsächlichen Originalressourcen und JNI/GLES. Sie ersetzen keinen Bild- und Tonvergleich auf dem Android-Zielgerät. Der frühere Gesamtauftrag, alle Räume und alle Leon-Aktionen vollständig originalgetreu zu portieren, ist damit weiterhin nicht als abgeschlossen belegt.

## Die zwölf gemeldeten Punkte

| Nr. | Korrektur im aktuellen Stand | Nachweis und verbleibende Abnahme |
|---|---|---|
| 1 | Der Intro-Compositor erhält den tatsächlichen sichtbaren Bildbereich. Beim Verformen und Hochskalieren des Zwischenbildes werden Randpixel innerhalb dieses Bereichs begrenzt; schwarze Ränder der Quelle werden nicht mehr in das Bild hineingezogen. Der vorhandene Originaleffekt bleibt erhalten. | 30 Compositor-Prüfungen einschließlich einer Bildquelle mit schwarzen Rändern. Der zuvor schwarze untere Bereich enthält nun die richtigen Bildpixel. Die einzelnen Intro-Stills sind noch auf dem Gerät zu prüfen. |
| 2 | ENV-Daten, TEV-Skalierung, die Auswahl der Licht-Cuts aus den originalen CAM-Bereichen und deren zeitlicher Übergang werden ausgewertet. Akteure verwenden die passenden Lichtgruppen und originalen Lichtvolumen. Während eines Ereignisses tragen unterdrückte Gameplay-Akteure keine zusätzlichen Lichter bei. Die projizierte Fensterspiegelung schreibt keine Tiefe und erhält den Raumnebel. | 41 Lichtverträge; echte r120-Spiegelungs-Cuts 0/2/6 mit 101 GLES-Prüfungen und Vergleich gegen die eingefrorenen Original-Spiegelungsroutinen. Helligkeit, zeitliche Lichtwechsel und Fenster müssen zusätzlich im Gerätefilm verglichen werden. |
| 3 | Das Funkgerät übernimmt den vollständigen ursprünglichen CORE-ENV-Cut 9 und die drei gemeinsamen Terminallichter. Die vorherige unvollständige Übernahme von Umgebung/Fog wurde ersetzt. | Beide tatsächlichen Funkgespräche mit elf Originalmodellen, 810 bzw. 430 Source-Frames und insgesamt 1.370 Prüfungen bestehen. Portraithelligkeit auf dem Zielgerät bleibt ein visueller Abnahmepunkt. |
| 4 | Diffuse Beleuchtung wird vor dem Nebel gemäß der TEV-Skalierung begrenzt. Endliche Lichtquellen prüfen die originalen Kugel-, Zylinder- und Boxvolumen statt einer zu großen umschließenden Kugel. Die Raumauswahl berücksichtigt die originalen CAM-Flags und Bereiche. | Lichtverträge und tatsächlicher r100/r120-Renderpfad. Diese Änderungen adressieren nachgewiesene Abweichungen der Beleuchtung. Eine allgemeine Aussage über pixelidentische Ferntexturen oder sämtliche Shader ist noch nicht möglich. |
| 5 | Beim Übergang zu einer Raumscript-Bewegung ohne Überblendung wird die erste Originalpose sofort ausgewertet. Die erste Zeichnung kann dadurch nicht mehr die alte Fenster-/Idle-Pose verwenden. | Der vollständige JNI-Test prüft die direkte Übergabe des Lastwagen-/Brückenereignisses an Funkphase 12 und die bereits ausgewertete Pose vor dem nächsten zeitlichen Bewegungsschritt. |
| 6 | Die Karte zeichnet je nach Kartenmodus nur die passende originale Fußleistenbeschriftung. Die drei Textgrafiken besitzen im Original dieselbe Position und dürfen deshalb nicht gleichzeitig erscheinen. Positionen reagieren auf die Bildschirmgröße. | Java-Verträge für Kartenmodus, Fußleiste und Pan/Zoom sowie Original-Map-Gruppen. Die tatsächlichen Android-Schriftgrafiken und Touchflächen müssen auf dem Gerät betrachtet werden. |
| 7 | Schwarze Map-Elemente mit dem originalen multiplikativen Blendmodus werden nicht mehr als deckende schwarze Flächen gezeichnet. Nicht aktive Source-Gruppen und Marker bleiben ausgeblendet. | Original-Layout-/Assetprüfungen und Java-Layoutprüfung. Andere nicht schwarze GX-Blendvarianten sind im Canvas weiterhin teilweise angenähert. |
| 8 | A/B-Bestätigungen bleiben bis zum nächsten 30-Hz-Welttick erhalten, auch wenn vorher ein Renderframe ohne Zeitfortschritt läuft. Android-Zurück reicht bei aktiver Nachricht B an den nativen Nachrichtenbesitzer weiter. | Tatsächliche JNI-Eingabe mit der originalen zweiseitigen Türnachricht und ihrem Untersuchungsbesitzer: beschleunigtes Anzeigen, Seitenwechsel und Schließen, ohne gleichzeitig einen Gegenstand aufzunehmen. Fest vorgegebene Wartecodes bleiben gemäß `mes.cpp::code09` aktiv; sie sind nicht pauschal überspringbar. |
| 9 | Der Aufhebedialog verwendet die originale CORE-Meldung, Gegenstandsnamen, Schriftglyphen, Zeilenabstände, Auswahlzeichen und das Originalmodell. Der Hintergrund ist ein eingefrorenes Weltbild; die zusätzliche Android-Überschrift, Metallfläche und sichtbaren Ersatzbuttons entfallen. Kamera und Mark-F2/F3-Zuordnung folgen der Quelle. | Alle 58 IDM-Gegenstandsmodelle bestehen 1.077 GLES-Prüfungen, einschließlich Vollbilddialog, eingefrorenem Hintergrund und Viewportgrenzen. Fünf Original-Aufhebenachrichten werden aus den tatsächlichen Assets geprüft. Seltene Sonderfälle sind kein Beleg für vollständige Inventarparität. |
| 10 | Nebel-/Licht-Cuts verwenden die originale Kamerabereichsauswahl. Deaktivierte Editorbereiche und unbenutzte ENV-Felder können keine ungültigen Werte mehr in den Renderer bringen. Die Korrektur des Tiefenschreibens der Spiegelung verhindert zusätzliche Verdeckung nachfolgender Geometrie. | Kamerabereichs-, ENV- und Spiegelungstests bestehen. Das gemeldete Aufpoppen von Himmel/Fensterteilen ist erst nach einem erneuten Gerätevergleich abschließend bewertbar; das vollständige originale VIEW-Fernkugel-Culling ist noch nicht übertragen. |
| 11 | Der native Audiozustand besitzt den Aufhebedialog über beide Übergaben hinweg. Musik und normale Welt-Soundeffekte laufen während des Dialogs weiter, auch wenn die Java-Subscreen- und Spielzustände kurz überlappen oder noch nicht umgestellt sind. | Der fokussierte JNI-Test vergleicht echte Musik-PCM-Blöcke bei Öffnen und Schließen, lässt einen normalen CORE-Effekt während des Dialogs auslaufen und prüft die Nein-/Ja-Transaktion samt Munition und Funkeln. |
| 12 | Der Gegner-Schadensadapter akzeptiert auch die originale Sprengfallenexplosion mit Radius 3.000. Zuvor erkannte er für denselben Blast-Typ nur den Fassradius 6.000 und hielt den Weltpfad wegen eines nicht unterstützten Treffers an. | Die tatsächliche Falle wird beschossen und anschließend 240 Weltticks weitergeführt: Falle verbraucht, Welt nicht angehalten, Leon-/Gegner-GPU-Ressourcen weiterhin gültig und keine GLES-Fehler. |

## Originalquellen und Umfang

Der Abgleich umfasst insbesondere `light.cpp`, `trans.cpp`, `scroll.cpp`, `cam_ctrl.cpp`, `mirror.cpp`, `mes.cpp` sowie die r100-Ereignisse und die ursprünglichen Subscreen-/Terminaldaten. Die Lichtdaten berücksichtigen auch die ursprüngliche Versionsanpassung: TEV-Skalierung wird begrenzt, und ältere ENV-Versionen besitzen keine Hokan-Dauer. Inaktive Lichtdaten werden für ihre spätere Verwendung im Subscreen erhalten. Die Bounding-Box eines Ereignismodells verwendet Modellwinkel/-skalierung und die animierte Root-Translation entsprechend `cLightInfo::updateMatrix`.

Die gemeldete Fallenblockade wurde im wirklichen JNI-Weltpfad reproduziert. Das bereitgestellte Logcat enthält keinen nachgewiesenen nativen oder Java-Fatal-Absturz; daraus folgt keine Entwarnung für unabhängige Geräteabstürze.

## Abschließende Prüfungen

Die Anzahl der Prüfungen beschreibt Assertions und keine Fertigstellungsquote des Spiels.

| Prüfung auf diesem Quellstand | Ergebnis |
|---|---|
| Android-arm64 `assembleDebug` und `lintDebug` | Erfolgreich; 0 Lint-Fehler, 34 Warnungen. Die gebauten C++-/Java-Quelldateien stimmen byteweise mit den ausgelieferten Produktionsdateien überein. |
| Finale native CTest-Suite | 43 von 43 erfolgreich. |
| Java-Verträge über `tools/test_host.sh` | Erfolgreich; unter anderem 7.434 Menügeometrie-, 10 Kartenkamera-/Fußleisten-, 86 Startup-Präsentations- und 76 Raumrepository-Prüfungen. |
| Startup-Compositor | 30 Prüfungen einschließlich des aktiven Bildbereichs. |
| Raumlichtverträge | 41 Prüfungen einschließlich ENV, Versionsanpassung, Lichtvolumen und CAM-Auswahl. |
| Spiegelung gegen eingefrorene Originalquelle | 342 Prüfungen für RTT-/Projektionsparameter. |
| Tatsächliche r120-Spiegelung unter GLES | 101 Prüfungen; Cuts 0/2/6, Tiefenpuffer, GL-Zustand, Pause und Kontext-Neuaufbau. |
| Tatsächliche Gegenstandsmodelle unter GLES | 1.077 Prüfungen für alle 58 IDM-Modelle; Reflexion bei 32 Modellen sichtbar. |
| Tatsächliche Funkgespräche unter GLES/Audio | 1.370 Prüfungen; beide Gespräche, Source-Modelle/-Nachrichten/-Morphs, DSP, Skip, Pause und Kontext-Neuaufbau. |
| Original-Subscreen-/Map-/Aufhebenachrichten | 16.728 Prüfungen erfolgreich; 17 Gebietslisten, 113 Raumzeilen, 256 Namensgrafiken, 156 Dokumentseiten und fünf Aufhebenachrichten. |
| Fokussierte Nachrichten-/Aufhebeprüfung über JNI | 158 Prüfungen erfolgreich; A/B über einen Renderframe ohne Zeitfortschritt, beide Seiten der Original-Türnachricht, Musik und CORE-Soundeffekt sowie Nein-/Ja-Transaktion. |
| Abschließender r100-Regressionstest über JNI/GLES | 2.672 Prüfungen erfolgreich; r120-Intro, Hausgegner, unmittelbare Brücken-/Funkübergabe und erste Pose, drei Außengegner samt Beute, vier GPU-Neuaufbauten, 240 Weltticks nach der Sprengfalle sowie Original-Nachricht und beide Aufhebeantworten mit fortlaufendem Audio. |

## Noch offene Abnahme

Die Tests laufen nativ unter Mesa mit den bereitgestellten Assets. Ein physisches Android-Gerät oder Android-Emulator stand für diesen Durchlauf nicht zur Verfügung. Deshalb sind Bildvergleich, MediaCodec-/Canvas-Darstellung, Controllerfolge, tatsächliche Audiolatenz und Ladezeiten auf dem Zielgerät noch offen. Die Testlaufzeit auf dem Host ist keine Android-Leistungsmessung.

Weiterhin fehlen Teile der allgemeinen GX-Shader-/VIEW-Culling-Parität sowie der Leon-, Gegner-, Kollisions- und gemeinsamen Enginezustände. Der ältere Bericht `R100-KORREKTUREN-PRUEFBERICHT.md` beschreibt den Stand des ersten Pakets; dieser Bericht aktualisiert ausschließlich die hier beschriebenen Korrekturen und Prüfungen. Weder die Beleuchtung aller Räume noch sämtliche optionalen Varianten des ersten Levels sind vollständig nachgewiesen.

## Einspielen und Reproduzieren

Das ZIP enthält vollständige geänderte/neue Dateien unter `RE4-Android-Decomp/`. Die Dateien über das vorhandene Projekt mit dem ersten r100-Korrekturpaket übernehmen. Es gibt keine Dateilöschungen. `FILES-R100-KORREKTUREN2.sha256` listet die enthaltenen Dateien außer sich selbst. Disc-, Video- und Bildressourcen, APK, private Toolchain und Signaturdateien sind nicht enthalten.

Android wie im bestehenden Projekt mit `./gradlew assembleDebug lintDebug` bauen. Die allgemeinen Hostverträge laufen mit `bash tools/test_host.sh`. Die assetabhängigen GLES-/JNI-Programme erhalten die vorhandenen Originaldaten als Argumente; ihre Usage-Zeilen nennen die Reihenfolge. `r100_render_runtime_check` verwendet R120, R100, R101, PL00, WEP01, WEP02, WEP07, ROOMINFO, CORE, EM12 und ESL. Für dessen Audioprüfung müssen die Modellpfade im üblichen importierten Verzeichnis `disc1/files/em/` liegen, damit derselbe Ressourcenpfad wie in der App verwendet wird. `RE4_CHECK_PICKUP_MESSAGE_ONLY=1` beschränkt ihn auf die Nachrichten-/Aufhebetransaktion.
