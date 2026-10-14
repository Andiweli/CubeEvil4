# K17 Fix7 – vollständige Austauschdateien

Den Ordner RE4-Android-Decomp über K17 oder K17 mit Fix1–Fix6 kopieren und Dateien ersetzen. Dieses ZIP ist kumulativ und enthält keine APK. Kein neuer Disc-Import erforderlich. Maßgeblich für diesen Stand ist FILES-K17-FIX7.sha256; ältere Prüfdokumente beschreiben ihre damaligen Stände.

## Änderungen

- SELECT-Popup breiter; Infinite Health und Infinite Ammo nebeneinander, Dropdown mit kleinem ADD-Button und getrennte MSAA-/AF-Radiogruppen.
- MSAA: No MSAA, 2x MSAA, 4x MSAA. Native GLES3-Multisample-Farb-/Tiefenpuffer, echte Samples, Auflösung vor Ausgabe. Kein EGL-Kontextneustart und kein erneutes Laden des Raums nötig.
- AF: No (1x), 2x, 4x, 8x als Override für Raum- und Akteurtexturen. Dieser Schalter steuert anisotrope Texturfilterung, nicht MSAA.
- Grafikänderungen erfolgen auf dem GL-Thread. Nicht verfügbare Stufen werden sichtbar zurückgesetzt; das Popup zeigt die wirksame Stufe. Defaults: kein MSAA, 1x AF. Einstellungen gelten für die Sitzung, einschließlich EGL-Kontextverlust, nicht als Savegame-Einstellung.
- MSAA-kompatible Bildkopien für Wasser, Raumfilter, Ereignis-Fokus und Tiefenabfragen. Vor skalierten Kopien wird in einen normalen Farb-/Tiefenpuffer aufgelöst.
- Letzter Versuch für Leons Inventaranimation: drei Pixel-Transferpuffer mit GPU-Fences, keine Warteoperation für laufende Bilder. Nur abgeschlossene Bilder werden übernommen; beim Öffnen wird einmal ein sofort sichtbares Bild erzeugt. Keine asynchrone Hintergrund-Worker-Warteschlange pro Bitmap mehr. GPU-Arbeit erfolgt außerhalb der für die UI-Bildkopie verwendeten Sperre. Originale Animation und Interpolation bleiben erhalten, die Welt bleibt pausiert.
- Rifle-Steuerung und -Animation werden in diesem Update nicht verändert.

## Prüfungen und Grenzen

- Vollständige Android-Java-Kompilierung: bestanden.
- Android ARM64 NDK-Build: bestanden.
- 43 vorhandene CTest-Gruppen: bestanden (bestehende Testprogramme).
- Neu kompilierter Produktions-JNI/GLES-Laufzeittest: 382 Prüfungen bestanden, mit Originaldaten, sechs Inventar-Waffenprofilen, Zwischenposen, pausierter Welt, Hintergrundpause, Kontextverlust, Dorf/Funk/Blut sowie MSAA-/AF-Prüfungen.
- AF 1/2/4/8 wird am tatsächlichen Texturparameter geprüft.
- Software-GLES bietet in dieser Umgebung 4x, jedoch keine exakte 2x-RGBA8/Depth24-Kombination an: 4x und die sichere Ablehnung von 2x sind geprüft. Die echte 2x-Ausgabe auf Android-Hardware steht aus.
- Die EGL-Prüfumgebung verlangt jetzt RGBA8 wie die Android-Surface, damit der MSAA-Farbformatvergleich korrekt ist.
- Kein physischer Android-Gerätetest, keine Messung der Glätte oder FPS auf Retroid. Android-Popup kompiliert, aber keine visuelle Geräteprüfung.

Die Inventaränderung ist ein weiterer begründeter Versuch, keine Zusage ruckelfreier Ausgabe auf dem Gerät. Falls sie weiterhin ruckelt, ist als nächster Schritt die Rückkehr zum sauberen Standbild vorgesehen.
