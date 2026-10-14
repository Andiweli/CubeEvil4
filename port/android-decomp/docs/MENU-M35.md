# M35: Ingame-Menüs über den ganzen Viewport

Die bisherige feste 640×480-Fläche wurde mit seitlichen beziehungsweise oberen Rändern in das Fenster eingepasst. `GameScreenDialog` nutzt jetzt den gesamten Viewport. Die logische Breite und Höhe wachsen mit dem Bildschirmformat; eine gemeinsame Skalierung für beide Achsen erhält die Proportionen der Schrift, Gegenstände und Leon.

| Ansicht | Anpassung an zusätzliche Fläche |
| --- | --- |
| Reiter | Originalgrafiken bleiben gleich proportioniert; zusätzliche Breite verteilt sich zwischen den fünf Reitern. Die Touchbereiche folgen denselben Positionen. |
| Waffen / Heilung / Koffer | Der Koffer erhält eine gleichmäßige, begrenzte Vergrößerung und wird im verfügbaren linken Bereich platziert. Raster, Gegenstandsbilder, Drehungen, Auswahl und Touchgeometrie verwenden dieselbe Projektion. |
| Leon und Pesetas | Das Originalportrait bleibt unverzerrt und hängt am rechten/unteren Rand; der Geldbetrag bleibt oben rechts. |
| Schlüssel / Wertgegenstände | Beide Spalten wachsen gleichmäßig; mehr verfügbare Höhe zeigt zusätzliche Zeilen. Originalbilder und Namen werden mit gleicher Skalierung auf beiden Achsen eingepasst. |
| Karte | Der Kartenbereich wächst mit dem Viewport. Raumgeometrie und Markierungen verwenden weiterhin einen einheitlichen Maßstab. |
| Dateien | Dateiliste, Leserrahmen und Seitenbereich nutzen die verfügbare Fläche. Originaldokumentseiten behalten ihr Bildformat. |
| Aktionen / Ablegen / Optionen | Die Bedienfelder zentrieren sich im neuen Viewport; Schrift und Buttons bleiben proportional. |
| Aufnahme / Untersuchung | Das native 3D-Modellfenster erhält die tatsächlichen normalisierten UI-Grenzen. GLES-Viewport und Scissor folgen dieser Fläche; ein engeres Format erhöht bei Bedarf den Betrachtungsabstand, damit das rotierende Objekt hineinpasst. |

Der dekorative Metall-/Steinhintergrund wird in neun Bereiche aufgeteilt. Außenkanten, Kopf-/Fußbereiche und rechte Portraitumrandung behalten ihre festen Quellmaße; nur die mittleren Dekorflächen nehmen die zusätzliche Breite/Höhe auf. Die Menüansicht wird nicht als komplettes Bild horizontal gestreckt. Das Originalportrait, Koffermodelle, Gegenstände, Karten und Schrift sind von dieser Dekoranpassung getrennt.

Bei Koffertouches werden die Bildschirmkoordinaten durch die ursprüngliche Kameraprojektion und die neue gleichmäßige Paneltransformation zurückgerechnet. Gegenstands-Touchflächen sind die tatsächlichen projizierten Vierecke. Dadurch wählen die schrägen Kanten des Rasters nicht versehentlich eine benachbarte Bounding-Box.

`GameMenuLayoutTests` prüft 4:3, 16:9, 20:9, 21:9, quadratische und hohe Fenster: volle Viewportabdeckung, Reiter-/Spaltenbegrenzungen, zusätzliche Listenzeilen, native Modellgrenzen, unverzerrte Koffertransformationen und Touch-Rückprojektion für sämtliche Zellen aller vier Originalkoffergrößen. Ergebnis: **6.872 Prüfungen bestanden**. Die bisherigen **80 Menü-Navigationsprüfungen** bleiben erfolgreich. Die Android-Menüklassen kompilieren; `subscreen_item_gl.cpp` besteht die Syntaxprüfung mit `-Wall -Wextra -Werror`.

Die Sichtprüfung mit 1280×720, 2400×1080 und 1080×1920 verwendet aus Java exportierte Layoutkoordinaten sowie die bereits dekodierten Originalbilder und Modellansichten. Diese PNGs sind Host-Kompositionen, keine Screenshots einer ausgeführten Android-App. Der normale Controller-Spielbildschirm bleibt laut Manifest in `sensorLandscape`; hohe Fenster dienen zusätzlich zur Prüfung der Geometrie.

Die in `MENU-M34.md` genannten Grenzen der Menütreue bleiben bestehen: eingefrorene Leon-Pose, keine vollständige ID-/Kofferanimation und teilweise angepasste Canvas-Untermenüs. M35 erweitert die Darstellung an unterschiedliche Viewports; es behauptet keine pixelgenaue Emulation aller Originalmenüs.
