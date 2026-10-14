# Native Raumstarts und begrenzter Türtest — M17

M17 liest originale Raumstart- und Türdaten mit nativen, begrenzten Parsern. In der Raumansicht wird zuerst der originale Startpunkt 0 des gewählten Raums verwendet. Die Figur erhält dessen Blickrichtung. Ein Start wird nur übernommen, wenn SAT einen Boden innerhalb von 250 Originaleinheiten, eine ausreichend aufwärts gerichtete Bodennormale und eine freie Kapsel bestätigt. Andernfalls bleibt der ausdrücklich angezeigte SAT-Teststart verfügbar.

Der ausführbare Türumfang besteht ausschließlich aus **Bereich 0 von r300 nach r301 und Bereich 0 von r301 nach r300**. Die Zielpositionen stammen aus den jeweiligen AEV-Datensätzen. Alle anderen gelesenen Türen sind Diagnosedaten; ihre Szenariobedingungen werden nicht umgangen.

## Datenquelle und Originalcode

Die vollständigen Disc-Imports enthalten `disc1/files/Debug/roominfo.dat` beziehungsweise `disc2/files/Debug/roominfo.dat`. Der Laufzeitcheck bevorzugt die gültige Disc-2-Datei und kann auf die identische Disc-1-Datei zurückfallen. Die Datei hat auf beiden geprüften Originaldiscs **10.790 Byte** und SHA-256:

```text
ee61ecc29ab8c21e1089774f65cc69db074d1cb02bd7937d5adf1f92b555f0b2
```

Bei vorhandenem vollständigem Import ist keine zusätzliche Spieldatei nötig. Fehlende oder veränderte Raumstartdaten werden nicht durch erfundene Startkoordinaten ersetzt.

Die Layouts und die verwendeten Regeln stammen aus:

- `upstream/re4/include/room_jmp.h` und `src/game/room_jmp.cpp`: `CRoomInfo`, `cRoomJmp::getRoomInfo`, `CRoomInfo::setNextPos`.
- `upstream/re4/include/sce_at.h` und `src/game/sce_at.cpp`: `SceAtInit`, `sceAtCheck_main`, `sceAtHitCheck`, `sceAtFunc_door`, `SceAtRoomSet`.
- `upstream/re4/include/area.h` und `src/game/area.cpp`: `AREA_HIT_DATA`, Quad- und Zylinderprüfung.
- `upstream/re4/src/game/pl_class.cpp::cPlayer::checkCtrl` und `src/game/act_btn.cpp::cActionButton::checkButton`: ursprüngliche Aktionstasten und Statusbits.
- `upstream/re4/src/game/game.cpp::gameDoordemo`: Übernahme von Zielposition, Blickrichtung, Raumnummer und Part im Original.

Der Port führt dabei keine PowerPC-Zeiger oder Szenariocallbacks aus. Diskfelder werden einzeln gelesen; ein 32-Bit-Zeiger im Originaldatensatz bleibt eine Zahl zur Diagnose und zur Ablehnung nicht unterstützter Bedingungen.

## Rauminfo-Layout

Alle Ganzzahlen und IEEE-754-Floats sind Big Endian. Die Datei beginnt mit einer `u32`-Stageanzahl und einem `u32`-Offset pro Stage. Offset 0 bedeutet keine Tabelle. Jede vorhandene Stage beginnt mit einer `u32`-Datensatzanzahl; danach folgen 32-Byte-Datensätze.

| Offset im Datensatz | Feld |
| --- | --- |
| `0x00` | `u16` Flags; Bit 0 bestätigt gültige Position und Blickrichtung |
| `0x02` | `u16` Raum-ID, oberes Byte Stage und unteres Byte Raum |
| `0x04` | Drei `f32`: Weltposition X, Y, Z |
| `0x10` | `f32`: Blickrichtung um Y, in Radiant |
| `0x14` | `u32`: Name als Offset relativ zum Dateianfang |
| `0x18` | `u32`: erster Bearbeitername als Dateioffset |
| `0x1c` | `u32`: zweiter Bearbeitername als Dateioffset |

`RoomInfo::find(room, point)` behält die originale Nummerierung der Punkte desselben Raums bei. Ungültige Datensätze zählen bei der Nummerierung mit, werden aber nicht als Start zurückgegeben. Die unveränderten 32 Diskbytes und ihr Dateioffset sind für unabhängige Vergleiche verfügbar.

Der Parser begrenzt die Datei auf 1 MiB, die Stageanzahl auf 16, eine Stage auf 256 und die gesamte Tabelle auf 4.096 Datensätze. Er prüft Ausrichtung und Grenzen der Stageoffsets, überlappende Tabellen, die Übereinstimmung von Raum-ID und Stage sowie terminierte Texte außerhalb der Tabellen. Zahlen müssen endlich sein und dürfen betragsmäßig 10 Millionen Originaleinheiten nicht überschreiten.

## AEV- und Türlayout

`RoomFlow` liest `AEV[0]` aus dem entpackten Raumarchiv. Der Header enthält `AEV\0` an Offset 0, Version `0x0104` als `u16` an Offset 4 und die `u16`-Bereichsanzahl an Offset 6. Die Datensätze beginnen an Offset `0x10`; die GC-Datensatzgröße ist **`0x9c` / 156 Byte**. Nur Typ 1 ist eine Tür. Die Datei wird auf 1 MiB und 256 Bereiche begrenzt; abgeschnittene Datensätze, doppelte Bereichsnummern und ungültige Türzahlen werden abgelehnt. Archivbereiche dürfen ihre originalen Ausrichtungspolster enthalten.

| Offset im Bereichsdatensatz | Feld |
| --- | --- |
| `0x04` | `AREA_HIT_DATA`: Aktivbyte, Typbyte, zwei Paddingbytes, danach Formdaten |
| `0x34` | Bereichsflags; Bit 0 aktiv, Bit 1 gesetzt |
| `0x35` / `0x36` | Bereichstyp / Bereichsnummer |
| `0x37` | Treffertyp: Bit 0 Frontpunkt, Bit 1 Blickwinkel prüfen |
| `0x38` | Triggermaske |
| `0x39` | Zielmaske; Bit 0 Spieler |
| `0x3a` | Tasklevel |
| `0x3c` / `0x40` | Originaler Parameter- / Funktionszeiger |
| `0x44` / `0x45` | OT-Priorität 0–15 / Taskart |
| `0x46` / `0x47` | Modellabhängigkeit: Art / Nummer |
| `0x48` / `0x49` | Signierte Richtungs- / Öffnungswinkelbytes, jeweils × 2 Grad |
| `0x4c` | Originaler Parent-Modellzeiger |
| `0x52` | Länderbedingungen |
| `0x5c` | Drei `f32`: Zielposition X, Y, Z |
| `0x68` | `f32`: Zielblickrichtung um Y, in Radiant |
| `0x6c` / `0x6d` | Zielstage / Zielraum |
| `0x6e` / `0x6f` | Schlüsselart / Schlüsselflag |
| `0x70` | Tür-Exit-Funktionszeiger |
| `0x74` | Ziel-Part |
| `0x77` | Originaler Fade-Modus |
| `0x78` | Tür-Exit-Parameterzeiger |

Jede gelesene Tür behält ihre unveränderten 156 Diskbytes und ihren ursprünglichen Datensatzindex. Die Auswahl besucht höhere OT-Prioritäten zuerst und behält innerhalb derselben Priorität die Diskreihenfolge bei.

Bei Formtyp 1 ist der Bereich ein senkrechtes Prisma über vier XZ-Punkten. Ab Bereichsoffset `0x08` stehen Bodenhöhe, Höhe und Editor-Markerradius; die acht Koordinatenfloats beginnen an `0x14`. Bei Formtyp 2 stehen dort Bodenhöhe, Höhe und tatsächlicher Radius; X/Z des Zylindermittelpunkts beginnen ebenfalls an `0x14`. Formtyp 3 ist ein Sichtkegel und wird vom begrenzten Türtest nicht ausgeführt.

Die originale Höhenprüfung akzeptiert `floor − 100 ≤ testY < floor + height`. Der Quadtest verwendet die ursprünglichen vier Kantenvergleiche einschließlich der Kanten; degenerierte Quads treffen nicht. Der Zylindertest verlangt einen XZ-Abstand **kleiner** als den Radius.

Der normale Prüfpunkt liegt 250 Einheiten über den Füßen. Bei Frontprüfung liegt er zusätzlich 550 Einheiten vor der Figur. Die Bridge verkürzt diesen Frontpunkt durch die SAT-Linienprüfung, bevor sie ihn an `RoomFlow` übergibt. Blickrichtung 0 zeigt nach +Z. Die Winkeldifferenz wird über ±π normalisiert; das Öffnungswinkelbyte begrenzt beide Seiten der erlaubten Blickrichtung.

## Aktion und verfügbare Übergänge

Die Triggermasken bezeichnen im Original Aktionszustände, nicht automatisch eine neu betretene Fläche:

| Bit / Maske | Ursprüngliche Bedeutung und Parserverhalten |
| --- | --- |
| `1` | Ohne zusätzliche Aktion aktiv; der Diagnose-Auswahler kann dies erkennen |
| `2` | Aktionsdruck beziehungsweise gehaltene Original-Prüftaste; `cPlayer::checkCtrl` setzt `STA_PL_CHECK` / `STA_PL_CHECK2` |
| `4` | Gehaltene Original-Prüftaste (`STA_PL_CHECK2`) |
| `8` | Sichtbare Aktionstaste; Ausführung bei neuem Aktionsdruck |
| `0x80` | Im Original nach Ausführung deaktivieren; außerhalb des ausführbaren M17-Umfangs |

Die Laufzeitfreigabe verlangt ausdrücklich **Maske 8**. Andere Masken, Länderbedingungen, Schlüssel-/Flagbedingungen, Parent- oder wartende Modelle, Funktions-/Parameterzeiger, Tasks, Ziel-Parts und weitere nicht unterstützte Metadaten bleiben gesperrt. Der Test interpretiert solche Bedingungen nicht als bereits erfüllt. Der Diagnose-Auswahler bildet Maskenbit 8 mit einem neuen Druck, Bit 2 mit Druck oder Halten und Bit 1 ohne Aktion ab; Bit 4 allein wird nicht aktiviert. Er implementiert damit keinen allgemeinen Originalzustandsautomaten für sämtliche Triggerkombinationen.

Die zwei freigegebenen Übergänge sind:

| Quelle / Bereich | Ziel | Quelle der Position und Richtung |
| --- | --- | --- |
| r300 / 0 | r301 | Türpayload von r300 `AEV[0]` |
| r301 / 0 | r300 | Türpayload von r301 `AEV[0]` |

In den Original-Raumfunktionen werden diese beiden Türbereiche nicht durch Szenariocallbacks ersetzt. Diese Prüfung begründet die kleine Freigabeliste. Ein nur aus dem Diskdatensatz erkennbarer offener Türbereich reicht für andere Räume nicht aus: `src/st1/r101.cpp` ersetzt beispielsweise Bereich 0 abhängig von Raumflags durch `r101_DoorDontOpen100` und setzt ihn an anderer Stelle zurück. r100/r101 werden daher auch dann nicht freigegeben, wenn ihre rohen Türpayloads eine direkte Verbindung beschreiben.

## Laden, Übernahme und Fehler

Bei passendem Türbereich zeigt die Ansicht **„A/Kreuz: Tür nach …“**. A/Kreuz fordert den Übergang an. Die Java-Arbeitsschlange prüft zunächst einen vorhandenen Cache nach Größe und Hash. Muss sie den Raum neu entpacken, prüft sie die Quelle gegen den SHA-256 im Raumkatalog, entpackt in eine temporäre Cachedatei und prüft Größe sowie SHA-256 der entpackten Daten. Die neue geprüfte Cachedatei wird durch Umbenennen übernommen.

Das Ladeergebnis gelangt mit einem Anfrage-Token in eine ausschließlich CPU-seitige Mailbox. Der Worker lädt keine GLES-Ressourcen und kann auch während einer pausierten Surface fertig werden. Erst `draw()` übernimmt das passende Ergebnis mit aktuellem EGL-Kontext. Vor dem Wechsel werden Raumressourcen und Zielkapsel geprüft; danach folgt der GPU-Upload.

Bei Quellen-, Entpack- oder CPU-Validierungsfehlern bleibt der bisherige Raum erhalten. Scheitert der neue GPU-Upload, werden der alte CPU-Raum, Figurposition und Blickrichtung zurückgetauscht, der alte Raum erneut hochgeladen und seine Kamerasicht wiederhergestellt. Scheitert auch dieser Wiederherstellungsupload, wird die ungültige GL-Szene zerstört; es wird kein erfolgreicher Raumwechsel behauptet. Ein erfolgreicher Wechsel übernimmt die originale Zielrichtung und richtet die Followkamera dazu aus.

## Türtest mit originalem Einstieg

Der Button **„Türtest: originalen r301-Einstieg laden“** lädt den echten Punkt 0 aus `roominfo.dat`, keinen erfundenen Standort und keine Koordinate aus dem UI. Dieser Punkt liegt auf freiem SAT-Boden bei `(-8475, -7000, 8169)` mit Blickrichtung `−1,94`. Zur Tür drehen, kurz annähern und das A/Kreuz-Prompt betätigen. Damit gelangt die Figur über das originale Türpayload zum originalen Ankunftspunkt in r300; anschließend lässt sich der Gegenweg prüfen.

Der ursprüngliche Punkt 0 von r300 liegt dagegen auf einer anderen Bodenebene als der Türbereich. Die Originaldaten enthalten dafür Leiterbereiche; die native Leiterlaufzeit fehlt. Der spezielle r301-Einstieg macht die zwei Türübergänge prüfbar, ohne einen vollständigen Weg vom normalen r300-Start zur Tür zu behaupten. Der Testbutton ist eine Entwicklerfunktion für diesen begrenzten Übergangstest.

## Reproduzierbare Prüfung

Aus dem Projektverzeichnis, mit den in dieser Arbeitsumgebung vorhandenen Originaldaten:

```sh
cmake -S app/src/main/cpp -B ../tooling/m17/room-flow -DCMAKE_BUILD_TYPE=Debug
cmake --build ../tooling/m17/room-flow --target yz2_tool room_flow_tool room_flow_tests --parallel
ctest --test-dir ../tooling/m17/room-flow -R room_flow_contracts --output-on-failure
python3 tools/test_room_flow_data.py \
  ../tooling/m17/room-flow/yz2_tool \
  ../tooling/m17/room-flow/room_flow_tool \
  ../host-disc-install \
  ../game-source/RE4_2004-11-25_Disc1.gcm \
  ../game-source/RE4_2004-11-25_Disc2.gcm
```

Die Argumente des Python-Checks sind tatsächlich: YZ2-Tool, RoomFlow-Tool, Importwurzel, Disc-1-GCM und Disc-2-GCM. Er liest die originale Rauminfo unabhängig aus den FSTs beider Discs, vergleicht alle Startdatensätze und rohe Türrecords, prüft jeden katalogisierten Raum nach Quellen- und Nutzdatenhash und vergleicht die nativen Treffer gegen eine unabhängige Python-Geometrieprüfung.

Ergebnis der ausgeführten Hostprüfung: **142 Räume, 289 originale Türen, 5.202 unabhängige Bereichs-/Blickwinkelprüfungen, 222 exakte Rauminfo-Datensätze und genau zwei freigegebene Übergänge**. Die synthetischen Tests decken abgeschnittene und überlappende Tabellen, ungültige Offsets/Zahlen, Doppelnummern, Quad-/Zylindergrenzen, Frontpunktverkürzung, signierte Winkel, Winkelumbruch, Aktionsflanken und die verweigerte Ausführung fremder Bedingungen ab.

Die Parser- und Auswahler-Tests liefen außerdem unter AddressSanitizer und UndefinedBehaviorSanitizer. LeakSanitizer musste in dieser Containerumgebung mit `ASAN_OPTIONS=detect_leaks=0` deaktiviert werden, weil der `/proc`-Taskscan unter der vorhandenen Prozessüberwachung scheitert. Das belegt keine Prüfung auf Speicherlecks. Die Hostresultate ersetzen keinen M17-Test auf dem Android-Gerät des Nutzers.

## Grenzen

Dieser Stand implementiert keinen vollständigen Szenario-, Schlüssel-, Inventar- oder Speicherzustand. Originale Fadeeffekte, Türanimationen, Part-abhängige Raumdarstellung und Leiterabläufe fehlen ebenfalls. Die bereits gelesene Ziel-Part-Nummer ist kein Nachweis einer portierten Part-Sichtbarkeit. Die Freigabeliste ist kein pauschales Versprechen, dass statische Geometrie, dynamische Objekte oder Beleuchtung sämtlicher Räume vollständig dem Original entsprechen.
