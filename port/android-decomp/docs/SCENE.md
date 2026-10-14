# M06 — starre Weltgeometrie und GLES-Prüfansicht

Die Vorschau ist ein Daten- und Grafiktest, kein Spielstart. Sie zeigt originale
r300-Positionen und Dreieckstopologie mit belegten Platzierungstransformationen,
aber ohne Texturen, Animation, originale Beleuchtung oder Raumereignisse.

## Originalbelege und Berechnung

Die Referenz liegt unverändert unter `upstream/re4`:

- `src/game/math_sub.cpp`: `RotMatrix`, `TransMatrix`, `ScaleMatrix`.
- `include/model.h`: `cCoord::matCalc`.
- `src/game/model.cpp`: `setPartsParent`, `setPartsOffset`, `partsWorldCalc`.
- `src/game/trans.cpp`: der Original-Array-Zweig von `commonModelTrans`.

Eulerwinkel sind Radiant. Die Platzierungsmatrix ist `T * Rz * Ry * Rx * S`:
Rotation um X, dann Y, dann Z; Skalierung der drei Matrixspalten und anschließend
Translation. Es wird keine GameCube-Zeigerstruktur nach ARM64 kopiert.
BIN-Positionen bleiben gemäß dem belegten Shift-Feld dekodierte signed-BE16-Werte.
Die Gelenkzentren werden aus dem begrenzten Originalheader gelesen. Die Ruhepose
addiert lokale Zentren entlang der Elternkette. Vorwärtsreferenzen, Zyklen und
nicht endliche Werte werden abgewiesen. Diese Addition ist keine Implementierung
animierter Gelenkmatrizen oder gewichteten Skinnings.

Der Renderer akzeptiert nur den belegten starren lokalen Zweig: ein Teil, ein
Wurzelgelenk ohne Attachment, passende Gewichtsfelder, kein Motion-Verweis und
kein Common-Verweis. Alle ausgewerteten lokalen BIN-Modelle des geprüften
Korpus besitzen ein einzelnes Wurzelgelenk. Mehrgliedrige Ruheposeketten sind
zusätzlich synthetisch getestet, werden aber noch nicht gerendert.

`BuildStaticScene` verarbeitet Basis-Platzierungen (`nModel`), nicht zusätzliche
zur Laufzeit aus Gruppen erzeugte Works. SMX-Sichtbarkeit, Eventzustände und
Common-Modelle werden nicht simuliert. Vier ältere SMD-Blöcke mit anderen
Versionen bleiben wie zuvor ausdrücklich außerhalb der unterstützten Geometrie.

## Prüfansicht und Bedienung

Nach erfolgreichem Raumtest ist „r300: starre 3D-Geometrie anzeigen“ verfügbar.
Das decodierte, hashgeprüfte Archiv wird durch eine temporäre Datei und atomare
Umbenennung im App-Cache aktualisiert. Bei einem Fehler wird kein neuer Cache
aktiviert. Originalimporte unter Android/data/com.ast.re4decomp bleiben erhalten.

Die erste Ansicht zeigt Basis-Modell 29 mit 6.399 Dreiecken. Dieses Modell hat
die größte Dreieckszahl unter den 48 unterstützten r300-Basis-Platzierungen.
Eine Gesamtansicht enthält sehr große Hintergrundgeometrie; dadurch erscheinen
kleine Architekturteile beim automatischen Einpassen entsprechend klein.

| Taste / Achse | Prüfansicht |
|---|---|
| Rechter Stick | Orbit um die eingeblendete Geometrie |
| LT / RT | Abstand vergrößern / verkleinern |
| LB / RB | Vorherige / nächste unterstützte Basis-Platzierung |
| Start | Alle unterstützten Basis-Platzierungen zusammen |
| Android-Zurück | Prüfansicht verlassen |

LB/RB wechseln aus der Gesamtansicht wieder in eine Einzelansicht. Jede neue
Auswahl setzt die Kamera zurück. Die Orbitgeschwindigkeit verwendet vergangene
Zeit mit begrenztem Delta. Die bestehenden semantischen Gameplay-Aktionen werden
nur für diesen Diagnosemodus ausgewertet; Spielersteuerung wird nicht behauptet.

GLES 3 verwendet VBO/VAO, Tiefentest mit 24-Bit-Tiefenpuffer und einen einfachen
untexturierten Debugshader. Beide Dreiecksseiten sind sichtbar. Die Beleuchtung
wird aus Bildschirmableitungen berechnet und ersetzt keine Originalnormalen,
Materialien oder TEV-Stufen. Für die Orbitkamera wird der GPU-Upload um das
Bounding-Box-Zentrum normalisiert; die geprüften Weltkoordinaten bleiben erhalten.
Beim Modellwechsel werden alte GPU-Objekte im besitzenden GL-Kontext gelöscht.
Nach Kontextverlust werden alte Objektnamen verworfen und Ressourcen neu erzeugt.

## Nachweis und Grenzen

Alle 142 Raumarchive wurden ausgewertet: 139 unterstützte SMD-Blöcke,
11.852 Gelenkzentren, 22.922 Platzierungsmatrizen und starre Platzierungen,
11.653.738 Welt-Dreiecke einschließlich Degeneraten. Unter den starren
Basis-Platzierungen des Korpus wurden keine durch den neuen Filter ausgeschlossen.
Eine unabhängige NumPy-Referenz rekonstruiert Matrizen aus separaten X/Y/Z-
Rotationen und die Weltgeometrie direkt aus Originalbytes und GX-Kommandos.
Alle exportierten Floatwerte wurden mit absoluter Toleranz 0,125 in originalen
Welteinheiten verglichen. Das ist kein bytegleicher Floatvergleich mit PowerPC
und kein Vergleich mit Original-GameCube-Renderbildern.

| Raum / Teil | Starre Basis-Platzierungen | Welt-Dreiecke |
|---|---:|---:|
| r100 SMD[0] | 13 | 6.020 |
| r100 SMD[1] | 18 | 24.351 |
| r300 SMD[0] | 48 | 59.559 |

Reproduzierbarer Datentest, mit separat bereitgestellten Spieldaten und NumPy:

```sh
python3 tools/test_scene_data.py .host-cmake/yz2_tool .host-cmake/scene_tool /pfad/game
```

Der optionale Host-Grafiktest wird mit CMake `RE4_BUILD_GL_CHECK=ON` gebaut.
Er benötigt EGL, GLES-3-Header und passende Grafikbibliotheken. Falls nötig den
Headerpfad über `RE4_GLES_INCLUDE_DIR` setzen. Unter Linux verwendet der Test
surfaceless EGL. Aufruf: `scene_gl_check /pfad/decodiertes-r300.dat /pfad/ausgabe.ppm`.
Er rendert das tatsächliche Mesh in einen GLES-3-Pbuffer, prüft GL-Fehler und
sichtbare Pixel und prüft danach GPU-Aufräumen und einen Modellwechsel.
Der Test ist standardmäßig aus und wird nicht in die Android-App eingebaut.
Testdaten und exportierte Geometrie/Bilder sind nicht im Quellpaket enthalten.

Der Host-Test mit Mesa GLES 3.2 zeigte 16.107 Geometriepixel für Basis-Modell 29.
Das bestätigt den Grafikpfad auf diesem Host. M06 muss anschließend auf dem
Android-Gerät geprüft werden; weder Geräteperformance noch ein spielbares RE4
sind durch diesen Nachweis gegeben.

## Erweiterung M07

M07 ergänzt die originale lokale Basistexturzuordnung und getrennt indizierte
UVs für die starre Prüfansicht. Belege, unabhängige Vergleiche und noch fehlende
Materialeffekte stehen in `MATERIALS.md`; Installation in `UPDATE-M07.md`.
