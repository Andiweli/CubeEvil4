# Geprüfter Ausbau: Farmregie und r105-Behälter

Stand: 9. Oktober 2026. Vollständige Ersatzdateien, kumulativ auf dem bisherigen Dorfstand und dem akzeptierten Fix18-Inventar. Keine neue APK; keine Original-Spielarchive enthalten.

## Neu implementiert

- **r103/r113-Brunnen:** originale SMD-Objekte 0x52/0x53, SCR_AT 6, AEV 0x0a/7, ITA 0x8a/0x81 mit Modellspender 0x9e/0x9f. SCF 26/27/37 speichern Abdeckung, verschmutzten Anhänger und geöffneten Deckel. Bewegung, Rückprall und Geräusche folgen r103.cpp. Der Aufnahmezustand wird mit Originalflag 6 geteilt. Saubere und verschmutzte Modelle werden vor dem Gameplay geladen; ein Treffer verursacht keinen Modell-/Textur-Nachladevorgang.
- **Abschießbare Gegenstände:** ursprüngliche emitem-Treffervolumen, Waffenfilter, Fallbeschleunigung und EAT-Bodenabfrage. Hängende Modelle können vor dem Abschießen nicht eingesammelt werden. Die Landung setzt den Found-Zustand; Wiedereintritt rekonstruiert die Position aus den Originaldaten.
- **15 Medaillons:** sieben Farmziele und acht Friedhofsziele; das Farmziel 0x13 folgt seinem originalen SMD-Elternobjekt 8. Native Trefferblöcke, unsichtbares zerstörtes Modell, SE 6/0x2e, originale ET1A-Sprite-/Linieneffekte. Tages-/Nachtpaare r103/r113 und r108/r118 erhalten dieselben ETC-Bits. SCF 13 bei zehn sowie SCF 48 bei fünfzehn Zielen werden gesetzt. **Händlerbestand und vollständige Missionsanzeige sind damit noch nicht implementiert.**
- **Original-Brunneneffekte:** ROOM EST 0x10/0x11 mit ursprünglichen Weltkoordinaten; GPU-Ressourcen werden beim Laden vorbereitet. Eigene Effektbesitzer verhindern das Ersetzen von Ereignis-, Gegner- oder Falleneffekten.
- **r105:** fünf originale SceSetItemEvent-Bindungen (AEV a/b/d/e/f; ITA 92/93/94/88/8b; RSF 6–10; CAM 8/6/9/b/a). Drei Doppeltüren, eine um X klappende Klappe, eine um -500 Welt-X verschobene Schublade mit ihrem Itemmodell. OpenBoxMain-Transformationen und jeweils SE 19/1b/1a/18/1a. Laden rekonstruiert die endgültige Originalpose, ohne bereits eingesammelte Items zurückzubringen.

## Tests und Grenzen

| Prüfung | Ergebnis |
| --- | --- |
| r103/r113, Abdeckung zuerst bzw. Anhänger zuerst | Alle vier Fälle bestanden |
| Brunnen: Original-EAT, Modellvarianten, NativeSave, Rückkehr und Tages-/Nacht-Gegenstück | Bestanden |
| Alle 15 Medaillons, gepaarte ETC-Bits, SCF-Schwellen, NativeSave und Nacht-Wiedereintritt | Bestanden |
| 13 Schrank-/Behälterereignisse in r103/r113/r106/r105: originale AEV/ITA/CAM/SMD, Aufnahmebedingungen und NativeSave | Bestanden |
| Produktions-JNI/GLES: reale Raumvorbereitung, Aktionsinput, Pause, Kamerafreigabe, Waffen-Trefferzuordnung, bewegte Itemmodelle und Original-Sprite-/Linieneffekte | Bestanden |
| ITM-Modellverträge, RSF/ETC-Isolation und Kollisionsverträge | Bestanden |
| Bestehende WorldEffects-Verträge | 42 Prüfungen bestanden |
| Vollständiger zusammenhängender Kampagnenweg bis r200 | **Noch nicht bestanden / noch nicht fertig** |
| Android-APK, physisches Gerät, Bildvergleich mit GameCube | **Nicht geprüft** |

Der JNI/GLES-Lauf verwendet die Produktionsbrücke mit originalen US-GameCube-Raum-/Modell-/Texturdateien und einer hostseitigen EGL-Surface. Er ersetzt weder einen Geräte-Lauf noch den regulären Kampagnendurchlauf. Die Tests starten die geprüften Räume einzeln. Raumübergreifende Brunnen-/Medaillon-Persistenz ist zusätzlich über die echten NativeSave-Container geprüft; sämtliche Storyübergänge sind damit nicht abgenommen.

Noch fehlende Sturm- und Spezialeffektprofile bleiben ausdrücklich in den Logdateien diagnostiziert. Vorhandene gewöhnliche Ganado-/Fallenadapter sind keine vollständige Portierung aller Gegner-, Partner- oder Bossroutinen.

## Offene Regie für den tatsächlichen Pfad

| Abschnitt | Noch erforderlich |
| --- | --- |
| r111 und r113 | Vollständige playergebundene Regen-/Gewittereffekte und dazugehörige Spezialgeneratoren; Quellabläufe r111_ThunderMove/r113_ThunderMove |
| r113 → r11c | Ashley als echter Gameplay-Partner, Schulterhilfe mit Originalbewegungen, Türfreigabe und Folgezuständen |
| r106 → r104 | Felsblock/QTE, AEV8/9-Goto-Bewegungen, Luis-Schrank/EVD r106s00 und originales Kapitelende samt Neueinstieg |
| r104/r107/r105 | Ereignisabhängige Wellen und Schlüssel; r105-Türrätsel, Schlüsselkamera, Mendez-/Ada-Ereignisse und eigener Brunnen |
| Kirche/See/Rückweg | Originalrätsel, Boot/Del Lago, Kapitel-/Nachtwechsel, El Gigante und Ashley-Aufnahme |
| r11c → r11d/r11e → r10f/r11f → r200 | Luis-Hüttenbelagerung, beide tatsächlichen Routen, Spezialgegner, Gondel, Mendez, Auge/Tür und Stage-2-Übergabe |

Keine Fortschrittsflags werden gesetzt, um fehlende Ereignisse oder Bosskämpfe zu überspringen. Der Gesamtweg darf erst nach Umsetzung dieser Besitzer und einem regulären Hin-/Rückweg mit Speichern/Laden vor und nach den Ereignissen als fertig gelten.

Quellreferenzen: upstream/re4/src/st1/r103.cpp, r113.cpp, r105.cpp; src/game/emitem.cpp, EtcModel.cpp, stage.cpp, sce_at.cpp, sce_com.cpp, esp0b.cpp, esp09.cpp; bereitgestellte Original-AEV/ITA/CAM/SMD/ETS/ETM/EFF/SAT/EAT.

Reproduzierbare Ziele: village_props_check <disc-files>, room_item_events_check <disc-files>, village_next_runtime_check <fixture-root>. Für den hostseitigen EGL-Lauf: EGL_PLATFORM=surfaceless. Die Fixture enthält files/ und decoded/; Originaldaten werden nicht mitgeliefert.
