# Dorf-Ausbau: Farm-Schrankereignisse und r106

Dieses Paket setzt den zuvor gelieferten Dorf-Zwischenstand auf der K17/Fix18-Basis fort. Es enthält vollständige Austauschdateien samt bisherigen Änderungen. Den Ordner `RE4-Android-Decomp` über das bestehende Projekt kopieren und neu bauen. Original-Spieldaten sind nicht enthalten.

## Neu umgesetzt

- `r103`: drei originale Schrankereignisse mit AEV-Aktionsflächen 7/8/9, ITA-Gegenständen 0x92/0x81/0x83, CAM-Schnitten 10/11/9 und SMD-Türen 0x57–0x5C.
- `r113`: drei entsprechende Nacht-Ereignisse mit AEV 8/9/10 und ITA 0x8E/0x8F/0x8B; dieselben Originalkameras und Türmodelle.
- `r106`, der Anschluss hinter der Tagesfarm: zwei Schrankereignisse mit AEV 6/7, ITA 0x85/0x86, CAM 6/7 und SMD 0x29–0x2C.
- Originaler Ablauf aus `SceSetItemEvent`/`SceExecItemEvent`: Gegenstände zunächst unsichtbar und nicht aufnehmbar; Öffnen setzt das Raumflag und aktiviert den Gegenstand. Türrotation in 30 Schritten, Originalkamerafahrt, anschließend 15 Ticks Wartezeit. Öffnen verwendet SndCall(6, 0x1A).
- Gespeicherte offene Schränke erhalten beim Wiedereintritt die originalen Endwinkel. Bereits aufgenommene Gegenstände bleiben verschwunden. Öffnen selbst vergibt keinen Gegenstand.
- Schranktüren bewegen sich um ihre originalen SMD-Gelenke im Renderer. Leon und Kampf pausieren während der Kamera; Menüpause hält das Ereignis an.
- `r106`: sieben automatisch gesetzte Ganados aus `emleon00.esl` (Indizes 120, 121, 140–144), mit Originalpositionen, Typen, Lebenspunkten und dem vorhandenen gewöhnlichen Ganado-Laufzeitmodell. Tod/Speicherung nutzen die Tagesliste.
- `r106`: neun originale EM2A-Fallen aus derselben ESL (125–132, 138), mit vorhandenem Fallentreffer-, Spielerreaktions- und Fragmentmodell.
- `r106`: EAR/SST-Umgebungseffekte über den vorhandenen nativen Backend. Die Daten enthalten 16 Bänke; 26 Diagnoseeinträge kennzeichnen weiterhin fehlende spezielle Effektprofile.

## Originalquellen

`upstream/re4/src/game/sce_com.cpp`: SceSetItemEvent / SceExecItemEvent.
`upstream/re4/src/st1/r103.cpp`: r103_openShelf_main und R103Init.
`upstream/re4/src/st1/r113.cpp`: R113Init und Bindung der Nachtgegenstände.
`upstream/re4/src/st1/r106.cpp`: R106Init und r106_openShelf_main.
Die Raumgeometrie, Kamerakurven, Aktionsflächen und Gegenstandsmodelle kommen unmittelbar aus den jeweiligen DAS-Archiven; Gegner/Fallen aus der Original-ESL.

## Prüfung

Host-C++-Build mit `-Wall -Wextra -Werror`; Tests verwenden die Originaldaten der GC-USA-Disc1.
`room_item_events_check`: alle acht Ereignisse, Aktionsflächen, tatsächliche Kamerakurven, Gelenkpunkte, Aufnahmevoraussetzungen, Raum- und Gegenstandsflags einschließlich Original-Save-Roundtrip.
`village_next_check`: Original-Ganadopopulationen, Tiere/Fallen und Raum-Effektbank-Decodierung.
`village_next_runtime_check`: Produktions-JNI und GLES, echte Aktionstaste, Leon-/Kampf-Sperre, Pause/Fortsetzen, Ereignisende und Produktions-Speicherflags in r103/r113/r106; bestehende r111-Laufzeitprüfung.
Die zugehörigen Ausgaben liegen unter `docs/village-shelf-validation/`.
Kein Android-APK oder Gerätetest in dieser Umgebung; sichtbare Kameraführung und Gerätesound noch auf Android prüfen.

## Noch offen im vollständigen Weg-Ausbau

Dies ist ein weiterer geprüfter Ausbauabschnitt. Der gesamte Original-Spielweg ist weiterhin nicht vollständig spielbar.
`r103/r113`: Brunnen-/Anhängerlogik, Medaillons, spezielle Original-Eintritts-/Tierabläufe, weitere Untersuchungsszenen; in r113 Ashley und Schulterhilfe.
`r106`: Felsblock/QTE, skriptgesteuerte Ganado-Wegpunkte (AEV 8/9), EM29/EM2E-Ereignisakteure, Luis-Schrankanimation und r106s00 samt Kapitelwechsel. Die sieben Ganados verwenden bislang die vorhandene gewöhnliche KI; die vollständige Raumregie fehlt.
`r111`: vollständiger Originalregen, Blitz/Donner und zugehörige Spezialeffekte.
Weitere Wegabschnitte hinter r106 bzw. r113 benötigen weiterhin die jeweiligen originalen Szenario-, Rätsel-, Partner- und Bossabläufe.

## Gerätetest

1. In r103 alle drei Schränke mit der Aktionstaste öffnen: vorher kein Gegenstand/Glitzern, danach Kamerafahrt und Türbewegung.
2. Während einer Kamerafahrt pausieren und fortsetzen: Leon bleibt geschützt; nach dem Ereignis sind Bewegung und Waffenbedienung wieder frei.
3. Gegenstand aufnehmen, speichern und App neu starten: offene Türen bleiben offen, Gegenstand erscheint nicht erneut.
4. Dieselben Abläufe in r113 und den beiden Schränken von r106 prüfen.
5. In r106 Ganados und Fallen auf Darstellung, Treffer und Wiedereintritt nach Speichern prüfen.
