# K8 – Szenenbeleuchtung, Hundebefreiung und Aufnahmedialog

Stand: 7. Oktober 2026. Android-Version **0.37.8-dev**, Versionscode **41**.

K8 korrigiert die Initialisierung der Ereignisbeleuchtung für die Autofahrt, den Ausstieg und die erste Begegnung im Haus. Der Hund übernimmt die Vorwärtsbewegung seiner originalen Befreiungsanimation. Beim Aufnehmen erscheinen die große gerahmte Gegenstandsfläche und das separate Nachrichtenfenster aus den Originaldaten.

## Beleuchtung im Intro und im Haus

Der Port hatte für jedes Ereignismodell sofort eine inverse Lichtmatrix am Weltursprung aufgebaut. Im Original ist der Ablauf anders:

- `cManager::createBack` und `cObjMgr::memClear` löschen den Objektblock vor der Konstruktion.
- `cLightInfo::init2` setzt Form, Größe und Lichtmasken, initialisiert aber die inverse Matrix nicht.
- `SetObj18` schreibt die anfängliche Position direkt. Erst ein expliziter EVD-Positionsbefehl ruft die Funktionen auf, die die Lichtmatrix aufbauen.
- Zahlreiche Hauptmodelle in `r120s00`, `r120s01` und `r100s03` haben überhaupt keinen solchen Positionsbefehl. Ihre ursprüngliche inverse Lichtmatrix bleibt null. `lightHitCheckBBox` bildet deshalb jede ansonsten zulässige Lampe auf den Nullpunkt ab und lässt sie zu.

Die vorzeitig erzeugte Matrix des Ports schloss unter anderem Punktlichter des geparkten Autos und des ersten Hauses aus. K8 übernimmt nun diesen ursprünglichen Initialzustand. Tatsächliche Positionsbefehle erzeugen weiterhin eine begrenzte Lichtbox; angehängte Modelle behalten ihre vorgegebene bewegte Kugelprüfung. Auch beim Neustart eines Ereignisses wird der passende Initialzustand wiederhergestellt.

Die Originalwerte für Lichtfarben, Umgebungslicht, GI-Texturen, Nebel und Scheibenmaterialien bleiben erhalten. Die Scheiben werden nicht durch einen zusätzlichen Helligkeits- oder Transparenzfaktor verändert. Die Lichtkorrektur ist in Aufnahmen mit den tatsächlichen Ereigniskameras geprüft, einschließlich aller 9 Autofahrt- und 15 Ankunftseinstellungen sowie der Haussequenz. Die Sicht durch die Frontscheibe am Ende der Fahrt wurde ebenfalls geprüft; die größte sichtbare Änderung dieser konkreten Einstellung betrifft Leon im Fond, während die vorgegebenen Scheibenreflexe erhalten bleiben.

Maßgebliche Originalstellen: `upstream/re4/include/cManager.h`, `include/obj.h`, `src/game/obj18.cpp`, `lightInfo.cpp`, `light.cpp`, `src/st1/r120.cpp` und die zugehörigen originalen EVD-/LIT-Daten.

## Hund aus der Falle

Die richtige Befreiungsanimation war bereits vorhanden, ihre Ortsbewegung wurde aber verworfen. K8 übernimmt bei jedem Originaltakt die Translation und Drehung der FCV-Kurve. Damit macht der Hund den vorgesehenen Schritt aus der Falle, bevor er losläuft.

Beim Wechsel in den Fluchtlauf wird diese neue Position auch an die Kollisionssteuerung übergeben. Das verhindert ein Zurückspringen zum ursprünglichen Fallenstandort, sobald die laufende Bewegung wieder die Bodenkollision nutzt.

Der Test wertet die tatsächlichen EM21-Daten unabhängig mit einer zweiten Animationsinstanz aus und vergleicht jeden Befreiungsschritt. Er prüft anschließend Flucht, Zaunsprung, Ausblenden sowie die bestehenden Fallenabläufe. Grundlage: `upstream/re4/src/em21/em21.cpp`, FCV-Eintrag `0x19` und SEQ-Eintrag `0x1c`.

## Gegenstand aufnehmen

Der vollständige Rahmen stammt aus `CORE_EXAMINE_ID` (`0x1e`), seine Texturen aus dem originalen CORE-Texturpaket (`0x1d`). Übernommen werden die Seitenflächen, die oberen/unteren Linien und das abgerundete Nachrichtenfenster mitsamt Positionen, Farben, Gruppenbeziehungen und Mischmodi.

Die Zeichenreihenfolge entspricht `bio4Draw`: eingefrorener Spielhintergrund, Rahmen in OT `0x13`, beleuchtetes Gegenstandsmodell in `0x14`, Originalschrift in `0x15`. Der Rahmen liegt dadurch hinter dem Gegenstand. Wiederholtes Zeichnen beginnt stets mit dem ursprünglichen Hintergrund, sodass die Abdunklung nicht von Bild zu Bild stärker wird.

Die aktiven Rahmenlinien liegen bei den ursprünglichen Y-Koordinaten `+176` und `-176`. Rahmen, Kamera, Originalschrift und Touch-Auswahl nutzen deshalb gemeinsam die 640×352-Ansicht innerhalb des ursprünglichen 640×480-Bildes. Das passt zur Aufteilung des beigefügten 1331×571-Screenshots und skaliert gleichmäßig für andere Bildschirmformate. Die bestehende Originalschrift und die ursprünglichen Fragen, Mengenangaben und Auswahlmöglichkeiten bleiben erhalten.

Die fünf benötigten Rahmentexturen werden einmal geladen und zwischen ihren Einheiten geteilt. Sie werden nicht bei jedem Bild erneut dekodiert oder hochgeladen. Grundlage: `src/game/examine.cpp`, `id_sys.cpp`, `id_tex.cpp`, `trans.cpp` und die originalen CORE-Daten.

## Prüfungen

| Prüfung | Ergebnis |
| --- | --- |
| Android-Neubuild mit `clean :app:assembleDebug` | Erfolgreich; ARM64, NDK 28.2.13676358, 40 Aufgaben ausgeführt |
| Native Vertragstests | 43 von 43 bestanden |
| Java-Menügeometrie einschließlich neuer Aufnahmeansicht | 7.456 Prüfungen bestanden |
| Originale Hund-/Fallen-Daten einschließlich FCV-Schritten | 286.386 Prüfungen bestanden |
| Ereignismodelle, Originalkameras, GPU-Skinning und Kontextwiederherstellung | 1.525.166 Prüfungen bestanden |
| Distanzrendering und ursprüngliche Lichtinitialisierung der drei Ereignisse | 6.538 Prüfungen bestanden |
| Gegenstandsrenderer | 1.167 Prüfungen; alle 58 Koffermodelle; zusätzlich Munition und Geld in 1331×571, 1280×720 und 480×640 |
| Natürlicher Haus-/Polizeiwagen-/Funkübergang und vollständige Todes-Sprachausgabe | 2.137 Prüfungen bestanden |
| Native/Java-Speicherformate und bestehender Speicherablauf | 78 / 56 / 33 Prüfungen bestanden |
| APK-Signatur und Ausrichtung | Vorgegebenes Zertifikat, v1 und v2 geprüft; 16-KiB-Ausrichtung geprüft |

Die Szenenprüfung verwendet die originale Spieldatenextraktion und den Produktionsrenderer mit GLES auf dem Linux-Testsystem. Eine Ausführung der neuen APK auf deinem konkreten Android-Gerät wurde hier nicht durchgeführt. Diese Prüfungen bestätigen die genannten Korrekturen, nicht eine vollständige visuelle Gleichheit des gesamten Spiels.

## Paket und Einbau

`RE4-Android-Decomp-R100-Korrekturen8.zip` enthält **66 vollständige Code- und Testdateien**, diesen Bericht und `FILES-K8.sha256`. Das Paket führt die benötigten Ersatzdateien aus K5, K6, K7 und K8 im aktuellen Stand zusammen. Es enthält vollständige Dateien und benötigt keine Patchskripte.

1. ZIP entpacken.
2. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem vorhandenen Projektordner zusammenführen und die entsprechenden Dateien ersetzen. Kein zusätzlicher gleichnamiger Unterordner ist nötig.
3. Projekt synchronisieren und neu bauen. Alternativ die signierte `RE4-M37-K8.apk` installieren.

Das Paket verändert keine Spielstände. Der zusätzliche vollständige Quellstand dient als nachvollziehbarer Projektstand. Keystore, Signaturpasswörter, lokale SDK-Pfade und extrahierte Disc-Dateien sind in den Quellpaketen nicht enthalten.
