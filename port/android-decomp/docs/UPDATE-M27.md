# M27 — Raumzustand und originale r101-Innenrauminteraktionen

Stand: 2026-10-04. Appversion `0.27.0-dev`, versionCode 28, Paket `com.ast.re4decomp`, Projektordner `RE4-Android-Decomp`. ARM64, minSdk21 und GLES3 bleiben die Anforderungen dieser implementierten Basis.

Dies ist der erste Baustein des vereinbarten zusammenhängenden Dorfabschnitts. Türen und Fenster erhalten eigene Zustände, die Aktion, Darstellung, Waffenstrahlen und Bewegung gemeinsam verwenden. Die Originaldaten und vorhandenen Quellen bestimmen Platzierungen, Bewegungen und Freigaben. Hausereignis, Fenster-/Leiteraktionen, Gegneraktivierung und Glockenablauf werden damit noch nicht ausgeführt.

## Neu ausführbar

| Funktion | Universaler Controller | Verhalten |
| --- | --- | --- |
| Innenraumtür öffnen | A / Kreuz beim Hinweis | Originale Leon-Öffnung, Blattbewegung am Scharnier und freier Durchgang |
| Tür eintreten | A / Kreuz während der ersten Öffnungsphase erneut drücken | Originaler Tritt, Blatt öffnet vom Spieler weg |
| Schloss lösen | LT / L2 halten, Schloss mit RS anvisieren, RT / R2 schießen | Ein Treffer löst das normale Schloss an der Eisentür; das Blatt bleibt zunächst geschlossen |
| Freigegebenes Fenster zerstören | Pistole anlegen und schießen | Originale Fenster-HP und Bruchmodell; ein geschütztes Fenster bleibt geschützt |
| Messer | LB / L1 halten, RT / R2 schneiden | Vorhandene Messerfunktion bleibt aktiv; Fenster verlieren durch das Messer im Original keine HP |

Die drei Holztüren ETS1/2/3 öffnen von beiden Seiten. Ihre eigene PL00-FCV bewegt das Blatt um den originalen Scharnierpunkt; Leons PL00-Aktion liefert Ausrichtung, Annäherung und Bewegung. Ein zweiter A-Tastendruck während der ersten 45 Spieltakte wechselt zum Originaltritt. Der Tritt prüft den Quellmarker 14. Ist Leon weit genug weg, schließt die Tür nach der Original-Näherungsregel. Geschlossene und bewegte Blätter haben eine eigene Wandkollision; während Leons Türaktion wird nur das aktive Blatt wie beim originalen `offOba` aus seiner Körperprüfung genommen. Boden und Stufen bleiben die originale statische SAT-Geometrie.

ETS11 ist die Eisentür mit dem von `R101Init` gesetzten normalen rechten Schloss. Schüsse und Messer prüfen die Originaltrefferbox des Schlosses vor dem Türblatt. Ein passender Treffer setzt beim normalen Schloss die Original-HP auf 0 und die Objektflags; normales Schloss bedeutet hier nicht drei erforderliche Schüsse. Verschlossene Tritte benutzen den gesonderten Originalclip und reduzieren das Schloss nach der geprüften Seitenregel. Das Schlossmodell verschwindet nach dem Lösen. Originaler Schlossfall, Biegeteile und Funken sind noch offen.

Alle fünf Fenster ETS0/8/10/19/34 erhalten ihre ursprünglichen Trefferflächen und Regeln. Freigegebene ET00-Fenster brauchen zwei Pistolenhits; ET25 bricht beim ersten. ET00/ET07 verwenden die echten gebrochenen BIN-Modelle mit ihrer ursprünglichen TPL-Palette. ET25 hat kein eigenes Bruchmodell und wird entfernt. Die im USA-Startprofil geschützten Fenster 0 und 19 werden nicht vor ihrem Ereignis freigegeben. ET07 hat zudem die ursprüngliche widerstandsfähige Schadensklasse. Das Fenster-Messerverhalten folgt `GetWepSizeGroup(0x10) == -1`; ein Messerhieb ersetzt keinen Pistolenhit.

Türblatt, Fenster, Schloss und Behälter werden auf jedem Waffenstrahl nach Entfernung ausgewählt. Ein geschlossenes Blatt oder geschütztes Fenster nimmt den Treffer vor einem dahinterliegenden Behälter auf. Schuss-/Messerserien verhindern mehrfache Schäden durch wiederholte Auswertung desselben Ereignisses. Vorhandene Originalbanken liefern Tür-, Schloss- und Glascues. Die CPU-Dynamikkollision wird vor jedem 30-Hz-Spielertakt neu gebunden; nach einem Fenstertreffer übernimmt der nächste Takt die geänderte Körperkollision. Darstellungs- und Trefferauswahl reagieren bereits auf den neuen Zustand.

## Gemeinsamer Zustand und Quellen

`room_state` ersetzt keine GC-Zeiger durch native Adressen. Es verwendet sichere Tabellen im Besitz der nativen Laufzeit mit der Originalbedeutung:

- `Scenario_flg[8]`, `Key_flg[2]` und `Room_flg[4]` sind 32-Bit-Wörter mit Flagindex 0 im höchsten Bit. Nur die beiden originalen transienten Framewörter werden pro Takt geleert.
- Jeder Raum erhält ein separates MSB-Raumwort und 64 `u16`-Objektmasken. Raumflagindices und Objektbitmasken werden getrennt behandelt.
- Die r101-Flags 6/7/8 bestimmen Kampf-/Glocken-/Hausprofil. M27 setzt diese Ereignisflags noch nicht; insbesondere wird kein Ersatz-Glockentimer gestartet. Die ursprünglich vor dem Hausereignis versteckten Leitern 5/6/7 bleiben versteckt.
- Fensterbruch, gelöstes Schloss und bestehende Behälterbruchbits werden in den Raumzustand übernommen. Aufgenommene Items und Inventar behalten zusätzlich die vorhandenen Objekt-Snapshots. Ein wieder geladener Raum stellt gebrochene Fenster und das gelöste Schloss daraus her; ein offenes Türblatt beginnt wie beim Original-Raumaufbau wieder geschlossen.

Diese Daten gelten **innerhalb derselben laufenden Sitzung**. Pause und eine EGL-Wiederherstellung derselben Sitzung verändern sie nicht. Bei einem fehlgeschlagenen Raum-GPU-Aufbau bleiben der bisherige Raumzustand, seine Kollisionsdaten und Leon erhalten; ein erfolgreicher Wechsel übernimmt erst den geprüften Zielraum. Eine ausdrücklich neue Testsitzung setzt den bisherigen Prototypzustand zurück. Es gibt noch keinen dauerhaften Spielstand und keine kompatible Memory-Card-Datei.

Die maßgeblichen Originalstellen im unveränderten Upstream-Commit `9b76ff003081d6ece8b838453fb8310743a972c7`:

| Quelle | Übernommene Bedeutung |
| --- | --- |
| `include/flag.h`, `global.h`, `flag_rsf.h`; `game/EtcModel.cpp::GetEtcFlgPtr` | MSB-Flagindices, getrennte Tabellen, Raum-/Objektwörter |
| `src/st1/r101.cpp::R101Init` | Normales Schloss an 11, Skriptposition von Regal17, USA-Fensterschutz, Leitern 5/6/7 |
| `src/game/emdoor.cpp::emDoorActEvtCk`, `plemDoorOpen`, `plemDoorKick` | Reichweite, Richtung, Ausrichtung, PL_ARC1E/1C/1D, Trittmarker |
| `emDoor_R1_Open2`, `emDoor_R1_Open`, `emDoor_R1_Close`, `emDoorDoorAutoCloseCk` | Eigene PL_ARC1F-Blattkurve, Spiegelung, Öffnungsrichtung und Schließen |
| `emDoorSetDmgLock_R`, `setLock`, `ckObj`; `emrack.cpp` | Rechte Schlossfläche und Flags, Regalblockierung mit echten Abmessungen |
| `src/game/emwindow.cpp::WindowData`, `DmCk`, `SetBreakModel`; `pl_wep.cpp::GetWepSizeGroup` | Trefferrechtecke, HP-Klassen, Bruchmodelle, Waffenklassen |

Die originalen PL_ARC-Angaben enthalten vier Headerwörter; die nativen DRS-Einträge für Leon sind 26/24/25, für das Blatt 27. Die drei Spielerclips werden nach den bestehenden Bewegungs-, Kampf- und Explosionsclips angefügt. Die Diagnose meldet beim vollständigen EM12-Profil jetzt korrekt **37 originale Spieler-FCV**: 30 bisherige Aktionsclips, 4 Explosionsclips und 3 Türclips. Das Basisprofil ohne EM12 hat 23. Der M26-Fix1-Messerstart bleibt enthalten.

## Installation und ein kombinierter Gerätetest

ZIP über den vollständigen **M26-Fix1-Stand** entpacken und die enthaltenen Dateien ersetzen. Es enthält vollständige geänderte/neue Dateien samt Ordnerstruktur. Ein vollständiger unterstützter Disc-Import enthält die benötigten Dateien bereits; kein Neuimport ist erforderlich.

```sh
bash ./gradlew clean assembleDebug lintDebug
```

Unter Windows: `gradlew.bat clean assembleDebug lintDebug`. Die Startanzeige muss **0.27.0** sein. Das neue `RE4Interaction`-Log beschreibt anfangs vier Türen, fünf Fenster und ein Schloss, mit r101-Raumflags 6/7/8 auf 0.

1. „Kampagnendorf r101 testen“ öffnen. Bei einer erreichbaren Holztür mit abgelegter Waffe den A/Kreuz-Hinweis benutzen, das Blatt beobachten und durchgehen. Von der anderen Seite testen, A beim Öffnen loslassen und nochmals drücken. Weggehen und das automatische Schließen prüfen.
2. Das freigegebene Erdgeschossfenster ETS8 mit der Pistole zweimal treffen: nach dem ersten Treffer bleibt es stehen, nach dem zweiten erscheint das Originalbruchmodell. Falls das Schloss ETS11 erreicht wird, gezielt darauf schießen und anschließend die Tür öffnen. Geschützte Fenster 0/19 dürfen durch Pistole oder Messer nicht freigeschaltet werden.
3. Einmal mit dem Pfeil ins pausierte Menü und zurück. App kurz in den Hintergrund bringen und wieder öffnen. Fenster-/Schlosszustand innerhalb dieser Sitzung prüfen; zusätzlich L1-Messer, Schuss/Nachladen und eine Behälterzerstörung kurz gegenprüfen. Bei Auffälligkeiten das vollständige Logcat mit `RE4Interaction`, `RE4Player`, `RE4Flow`, `RE4Audio` und `RE4WorldEffects` senden.

Ein kompletter manueller Durchlauf aller Räume ist dafür nicht nötig. Die gemeinsamen Systeme und Raum-/Grafikwechsel werden automatisiert geprüft. Ein Prozessneustart ist noch kein Speicher-/Ladetest.

## Prüfungen und nächste Stufe

Neue Originaldatenprüfung:511 Verträge für Türen von beiden Seiten, echte Tür-/Leonkurven, Scharnierpunkt, Durchgang, Auto-Close, Schloss, fünf Fenster, Varianten und Raumflags. Neue Produktions-JNI/GLES-Prüfung: 774 Verträge für echte A/LS-Türbedienung, gespiegelten Tritt, die tatsächlichen Shader-/Variantendraws, Pause, EGL-Verlust, Raumwechsel und GPU-Rückfall. Die Waffenstrahlen in diesem neuen Interaktionstest werden direkt an den Produktionsrouter gegeben; reale LT/RT/X-Waffeneingaben werden von den bestehenden Regressionen geprüft. Die gemeinsame Brückenregression besteht weiterhin mit 2918 Prüfungen. Vollständige Prüfumgebung und Android-Ergebnisse stehen in [VALIDATION.md](VALIDATION.md).

Als nächste Stufe werden Fenstersprünge und Leiteraktionen samt Originalkurven, Positionen und Kollisionswechseln an denselben Zustand angeschlossen. Danach folgen das echte Hausereignis und die originale Aktivierung mehrerer Ganados, anschließend der Dorfablauf bis zur Glocke. Die EVD-/Tasksequenzen und Endbedingungen müssen dabei aus dem Original nachvollzogen werden. Eine frei erfundene Gegnerwelle oder ein pauschaler Zeitablauf ersetzt diese Arbeit nicht.

Weitere Grenzen dieses Patches: Regale werden noch nicht geschoben; Holztürpaneele und deren Zerstörung, der kurze Endanschlag beim eingetretenen Blatt sowie Ashley-/Mehrgegner-Autocloseprüfungen sind nicht vollständig portiert. Die dynamischen Objektwände erweitern den bisherigen Kapseladapter und die Waffenprüfung; die Kamera benutzt weiterhin ihre statischen SEE-Wandabfragen. r400 erhält keine nachträglich freigeschalteten Innenraumtüren. Kampagne, vollständiges Inventar, Musik/Schritte/Stimmen und dauerhafte Saves bleiben eigene gemeinsame Systeme.
