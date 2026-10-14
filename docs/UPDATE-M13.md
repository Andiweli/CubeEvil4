# M13 — Raumwahl und SMX-Materialfarben

Voraussetzung: vollständiges Projekt einschließlich M12. Alle vollständigen geänderten/neuen Dateien mit ihrer Struktur über **RE4-Android-Decomp** kopieren. VersionCode 13 / VersionName 0.13.0-dev; weiterhin com.ast.re4decomp, ARM64/API21/GLES3. Mit demselben Signaturschlüssel über die vorhandene App installieren; keine Deinstallation oder erneuter Disc-Import nötig.

Falls Gradle beim Aktualisieren doppelte generierte R-Klassen im Dex meldet, einmal `clean assembleDebug` ausführen. Das verändert keine installierten App-Daten.

## Gerätetest

1. „Raumarchiv und Yz2 prüfen“ öffnen. Die bekannte Prüfung mit r100/r300 bleibt verfügbar.
2. Im neuen Abschnitt „Raum wählen“ zunächst r100 auswählen, laden und danach in 3D öffnen. Dazu genügt Disc 1.
3. Die Brücke startet weiterhin als Basis-Modell 6. Sie hat 2.108 Basisdreiecke und 1.104 Maskendreiecke, beide vollständig aufgelöst. Ihr SMX-Materialregister ist weiß; hier ist keine zusätzliche Abdunklung vorgesehen.
4. LB/RB bis Basis-Modell 0 schalten: 94 Dreiecke verwenden nun das originale Material-RGB 42/42/42. Die dunklere Darstellung ist beabsichtigt. SELECT ein/aus muss zur gleichen Material-/Texturansicht zurückführen.
5. Weitere Räume aus beiden Discs wählen. Die geöffnete Ansicht muss den geladenen Raumnamen tragen. Nicht unterstützte Daten oder fehlende Disc-Imports sollen im Auswahlmenü gemeldet werden, ohne eine 3D-Ansicht zu starten.
6. Nach Laden eines Raums einen anderen Eintrag wählen: „Gewählten Raum in 3D öffnen“ bleibt bis zum erfolgreichen Laden dieses Eintrags deaktiviert.
7. Während des Ladens Android-Zurück testen; anschließend neu laden. In der Ansicht App-Pause/Wiederaufnahme und Controller-Trennung testen.
8. r300 erneut öffnen: Texturen, Transparenzen, Mipmaps, Vertexfarben und Normalenansicht müssen erhalten bleiben. Materialfarben können einzelne Objekte nun zusätzlich modulieren.

Logs: `RE4Room` für die Auswahl-/Prüfschritte, `RE4Scene` für Geometrie und SMX-Draw-/Materialfarbzähler, `RE4Archive` für die bekannte Prüfung. Bitte jeweils den Raumnamen und sichtbare Auffälligkeiten nennen.

Die Auswahl erschließt die unterstützten **statischen Teilansichten**, nicht spielbare Räume. SMX-Lichtmasken, UV-Animation, Culling, Blend-/TEV-Sonderzustände, Gameplay und Gruppeninstanzen sind noch keine aktive Original-Runtime. Details: SMX.md.
