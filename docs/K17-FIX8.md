# K17 Fix8 – Inventarkanten und Grafikschalter

Vollständige kumulative Austauschdateien einschließlich Fix1–Fix7, keine APK. Den Ordner RE4-Android-Decomp über K17 bzw. den bisherigen Fix-Stand kopieren und Dateien ersetzen. Kein erneuter Disc-Import. FILES-K17-FIX8.sha256 ist der maßgebliche Dateinachweis; frühere Berichte beschreiben historische Stände.

## Änderungen

- Die in Fix7 verbesserte Inventaranimation bleibt bestehen, einschließlich Interpolation, Pixel-Transferpuffern und GPU-Fences.
- Leons separates 640x480-Inventar-Rendering erhält eigene 4x-MSAA-Farb-/Tiefenpuffer, bei fehlender Unterstützung 2x bzw. unverändert Single-Sample. Die Ausgabe wird vor der bestehenden gepufferten Übertragung aufgelöst. Übertragungsgröße und Animationsuhr ändern sich nicht. Diese Kantenglättung ist unabhängig vom MSAA-Schalter für die Spielszene.
- AF wird jetzt auch auf Texturen ohne ursprüngliche Mipmap-Kette angewendet. Für diese Texturen wird erst bei AF >1 eine GPU-Mipmap-Kette erzeugt und wiederverwendet. No AF (1x) stellt die ursprüngliche Filterung und Level-Grenze wieder her. Original vorhandene Mipmaps werden nicht überschrieben.
- Die Android-Surface verwendet ausdrücklich ein nicht multisampled RGBA8-EGL-Format. Damit bleibt No MSAA eindeutig und kann nicht durch eine implizit multisampled Fensterkonfiguration überlagert werden. Das Log weist die Fenster-Samplezahl aus.
- RE4Graphics protokolliert jede Grafikänderung mit angeforderten und angewendeten Stufen, AF-Limit, Bildgröße und GPU. Das gelieferte Log enthielt noch keine solche Bestätigung; eine Fehlfunktion sämtlicher MSAA-/AF-Stufen auf dem Gerät lässt sich daraus nicht beweisen.
- Alle MSAA- und AF-Radiobuttons stehen in ihren Gruppen untereinander. Bei geringer Bildschirmhöhe bleibt das Popup scrollbar. Cheat-Checkboxen bleiben nebeneinander.

## Prüfungen

- Vollständige Android-Java-Kompilierung und Android-ARM64-Build bestanden.
- 43 vorhandene allgemeine CTest-Gruppen bestanden (bestehende Testprogramme; ihre Quellbereiche wurden nicht verändert).
- Neu kompilierter Produktions-JNI/GLES-Test: 383 Prüfungen bestanden, einschließlich sechs Inventarprofile, Körperpose, Zwischenposen, Pause der Welt, Hintergrundpause, GPU-Kontextverlust, Funk und Blut.
- Beim Handgun-Inventarmodell steigt die Zahl von teilgedeckten äußeren Konturpixeln von 6 auf 744. Die Pose bleibt innerhalb einer Pixelbreite zur unabhängigen Originalreferenz. Erwartete Unterschiede durch Antialiasing-Texturabtastung werden separat von Körperkontur und Animationszuständen geprüft.
- Echter Bildvergleich: 4x MSAA verändert 240 Kantenpixel und erzeugt 240 Zwischenstufen der Flächenabdeckung. AF verändert 16384 Pixel einer schrägen Testtextur. Auch 2x und 8x AF verändern 16384 Pixel bei derselben Mipmap-Kette; der Unterschied stammt somit nicht nur aus dem Erzeugen der Mipmaps. No AF stellt das ursprüngliche Bild bytegleich wieder her.
- Linux/Mesa Software-GLES bietet keine exakte 2x-RGBA8/Depth24-Kombination: 4x und die Ablehnung der nicht verfügbaren 2x-Stufe sind geprüft. Echte 2x-Ausgabe auf Android bleibt ein Gerätetest.

Keine physische Android-Prüfung und keine Retroid-Frametiming-Messung. Die Wirkung von MSAA/AF hängt vom Motiv ab: Geometriekanten bzw. schräg betrachtete Texturen. Nicht jede Fläche muss zwischen jeder Stufe sichtbar anders aussehen. Die Pixelprüfungen ergänzen deshalb die bisherigen reinen Zustandsprüfungen.

Mipmapping On/Off wurde besprochen, aber nicht zusätzlich als Schalter beauftragt. Vorhandene Original-Mipmaps bleiben Standard; generierte Zusatzketten gehören hier zum aktivierten AF-Pfad.
