# M36: Gemeinsamer Originalrahmen auf allen Menüseiten

Die echten Android-Bilder `ingamemenu1..4.png` zeigen zwei Fehler im M35-Zeichenpfad. Der gemeinsame Rahmen wurde ausschließlich auf der Kofferseite gezeichnet. Auf Schlüssel-, Karten- und Dateiseiten endete deshalb die 511 Pixel breite Original-Hintergrundplatte vor dem rechten Fensterrand. Außerdem wurde der 640×360-Rahmen aus `ss_cmmn.dat`, Eintrag `0xC`, zusammen mit den unbenutzten 60-Pixel-Rändern einer 640×480-Zwischenfläche vergrößert. Damit blieben oben und unten große schwarze Flächen erhalten.

`GameMenuArt` verwendet jetzt auf jeder Seite dieselbe originale Umrandung, einschließlich des rechten Metallbereichs, Kopfbereichs und unteren Randes. Die neun Dekorbereiche lesen nur den tatsächlichen Quellbereich `y60..420`; ihre Zielrechtecke decken den gesamten Viewport ab. Die originale Hintergrundtextur wird unter die erweiterte Umrandung gelegt. Es wurden weder Ersatzgrafiken noch künstliche schwarze Balken ergänzt.

Der Koffer liegt hinter der gemeinsamen Umrandung. Der obere Kofferdeckel kann die Kopfzeile dadurch nicht mehr überzeichnen. Leon wird anschließend im rechten Bereich gezeichnet; Namen, Raster, Gegenstandsbilder, Auswahl und Zahlen liegen in ihren bisherigen proportionalen Ebenen darüber. Die ursprüngliche Kameraprojektion und ihre Touch-Rückrechnung bleiben gleich. Die zusätzliche Fläche vergrößert die Dekoration und Zwischenräume; Leon und Gegenstände behalten eine gemeinsame Skalierung für beide Achsen.

Schlüssel-/Schatz-, Karten- und Dateiseiten beginnen mit ihren eigenen Titeln unterhalb der gemeinsamen Reiter. Die Zeilen der Schlüssel- und Dateiliste benutzen die additive Original-Auswahlgrafik aus `ss_file.dat`, Eintrag7/Marke1. Die fünf Reiter benutzen weiterhin ihre vollständigen Original-ID-Gruppen einschließlich der jeweils aktiven Markierung. Im Koffer ersetzen die Original-Ziffern und Schatten aus `ss_cmmn.dat`, Eintrag8, die bisherigen Android-Zahlen für geladene Munition und Gegenstandsmengen.

Quellbelege: `game/sscrn.cpp` richtet die gemeinsamen Reiter, Pesetas und Hintergrundeinheiten ein; `ss_item.cpp`, `ss_map.cpp`, `ss_file.cpp` und `ss_pzzl.cpp` installieren die gemeinsame Umrandung aus Eintrag `0xC`. `ss_main.cpp::numDisp` wählt die drei Ziffern und ihre Schatten und blendet führende Nullen aus. `ss_file.cpp` bewegt die originale Listen-Auswahl unter den ausgewählten Eintrag.

Prüfungen:

- **7.432** Layoutprüfungen bestanden: sämtliche Originalkofferzellen, proportionale Projektion und Touch-Rückrechnung, Reiter, Modellfenster und vollständige Abdeckung durch die tatsächlichen `y60..420`-Rahmenteile.
- **80** bestehende Menü-Navigationsprüfungen bestanden.
- Die betroffenen Java-Klassen kompilieren gegen Android35; nur die bereits vorhandenen Deprecation-Warnungen der Android-Vollbild-API bleiben.
- `tools/export_menu_art.cpp` dekodiert die tatsächlich vorhandenen Original-ID-Layouts und Texturen in ein temporäres Verzeichnis. `tests/GameMenuArtRenderPlan.java` exportiert die Produktionsgeometrie. `tools/check_menu_art.py` zeichnet daraus alle fünf Reiter bei 640×480, 1920×1080, 2400×1080 und 1080×1920 und prüft **1.070** Ressourcen-, Auswahl-, Ziffern- und Pixelbedingungen. Insbesondere werden Kopf-, Fuß- und rechte Randpunkte jeder Seite geprüft. Die Kofferseite enthält das Originalkoffer-/Leonmodell, den projizierten Originalwaffen-Thumbnail, Raster, E-Markierung und Original-Munitionsziffer.

Die optionalen PNGs dieses Prüflaufs sind Host-Kompositionen der tatsächlichen dekodierten Grafiken mit der Java-Produktionsgeometrie. Der Prüflauf ersetzt keinen ausgeführten Android-Canvas-/Gerätetest und emuliert keine vollständigen Menüanimationen. Anders als die M35-Sichtprüfung umfasst er alle Reiter und prüft die fehlenden Randbereiche ausdrücklich. Originaldaten werden nur aus den vom Benutzer bereitgestellten Disc-Dateien gelesen; sie sind nicht Bestandteil des Quellpakets.

Die Grenzen aus `MENU-M34.md` bleiben bestehen: eingefrorene Leon-Pose, teilweise angepasste Canvas-Untermenüs und keine vollständige ID-/Kofferanimation. M36 behebt die nachgewiesenen fehlenden Rahmen und Überlagerungen; es behauptet keine vollständige pixelgenaue Originalemulation.
