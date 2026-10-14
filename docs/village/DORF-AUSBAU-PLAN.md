# Dorf-Ausbau: geprüfte Vorbereitung

Stand: 7. Oktober 2026. Ausgangspunkt: K14, `0.37.14-dev`, VersionCode 47.
Dieser Stand bereitet die Portierung vor. Er enthält keine neue Gameplay-Implementierung und keine neue APK.

## Umfang und Originalreferenz

Ziel ist der vollständige englische Leon-Kampagnenbereich **Stage 1 mit 29 Originalräumen**: Geometrie, Raumzustände, Verbindungen, begehbare Flächen, originale Objekte, Gegner, Ereignisse und Fortschritt bis zum Übergang nach r200. Das schließt Nachtvarianten, Ashley, die beiden Routen nach der Hüttenbelagerung, Bosskämpfe sowie die ursprüngliche r120-Eröffnung ein.

Die Quellen sind die bereitgestellten US-GameCube-Daten von Disc 1 und der eingefrorene Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7`. `st1.cpp::setTbl()` und `assets/rooms.tsv` registrieren dieselben 29 Räume. r110, r114, r115 und r116 existieren in diesem Originalbestand nicht. Sie werden nicht als fehlende Räume ergänzt. r200 ist ein Übergabepunkt an Stage 2; dessen Innenbereich gehört nicht zu diesem Ausbau.

Für alle 29 Raumarchive wurden komprimierte Originaldatei, entpackte Größe und SHA-256 gegen den bestehenden DVD-Katalog geprüft. Die fünf Zusatzblöcke von r100 sind ebenfalls geprüft. Die Rohdaten verbleiben außerhalb des Projekts. Englisch bleibt die einzige Laufzeitsprache; es werden keine japanischen oder Platzhalterdateien ergänzt.

Die vier ursprünglichen Stage-1-REL-Module wurden zusätzlich in Größe und SHA-1 gegen `references.tsv` geprüft. Rauminfo und die beiden Kampagnen-ESL-Dateien sind im Audit mit vollständigem SHA-256 festgehalten.

Die Raumbezeichnungen in der Bestandsliste stammen aus `Debug/roominfo.dat`. Einzelne erläuternde Kommentare im rekonstruierten Quellstand passen nicht zu diesen Bezeichnungen, etwa bei r102/r103 oder r105. Deshalb bestimmen **Raumnummer, ausführbarer Ablauf und DVD-Daten** die Zuordnung. Kommentarnamen allein begründen keine Umbenennung oder neue Verbindung.

## Geprüfter Datenbestand

| Bestand | Ergebnis | Bedeutung |
| --- | ---: | --- |
| Stage-1-Räume | 29 | Alle auf Disc 1; Registrierung und Katalog stimmen überein |
| AEV-Türdatensätze | 66 | Einschließlich zwei Selbstübergängen und einem Stage-2-Ausgang |
| Verschiedene gerichtete AEV-Raumpaare | 63 | Mehrere Türen können dasselbe Raumpaar verbinden |
| AEV-Leitern, Typ 0x10 | 21 | Lokale Aktionen; keine zusätzlichen Räume |
| ITA-Datensätze | 635 | Authored Itembereiche; nicht 635 gleichzeitig sichtbare Gegenstände |
| SCR_AT-Bereiche, Typ 0x0b | 30 | Zusätzlich zur normalen SAT/EAT-Kollision |
| Leon-ESL-Zeilen für diese Räume | 428 | Listen 00/01; auch deaktivierte, ereignisgebundene Akteure und Hilfsobjekte |
| Native Türfeldvergleiche | 2.046 | Unabhängiger DVD-Parser gegen den aktuellen produktiven RoomFlow-Parser |
| Native Startpunktvergleiche | 52 | Originale RoomInfo-Records einschließlich r120; unveränderte Nummerierung |
| Native statische Raumprüfung | 29/29 ohne Parserfehler | Basis-SMD/SMX/Texturen; kein Nachweis vollständiger Raumdarstellung |

Die Bestandsdateien stehen in `VILLAGE-ROOMS.tsv`, `VILLAGE-TRANSITIONS.tsv`, `VILLAGE-SOURCE.tsv` und `VILLAGE-AUDIT.json`. Die JSON enthält zusätzlich Startpunkte, AEV-Felder, Leitern, ITA-Felder, ESL-Identitäten, Archivressourcen und Quellreferenzen. `VILLAGE-AUDIT.md` wird aus dem Prüflauf erzeugt.

## Aktueller Port und konkrete Lücken

| Bereich | Vorhanden | Für diesen Ausbau zu erledigen | Beleg im Projekt |
| --- | --- | --- | --- |
| Raumausgang | Originalparser und transaktionales Laden; r100 → r101 freigegeben | Zustandsabhängige Übergänge, Rückwege, Parts, Schlüssel und Callbacks portieren | `app/src/main/cpp/room_flow.cpp::unavailableReason` |
| Raumskripte | r100 mit separaten Szenario-/Nebenereignisadaptern; r120-Eröffnung | Originale Init/Main- und Ereignisabläufe der übrigen Räume an die echte Kampagne anbinden | `android_bridge.cpp::prepareRoom`, `upstream/re4/src/st1/*.cpp` |
| r101 | Raum-/Interaktionsvorschau und allgemeine Systeme | Dorfkampf, Glockenende, Türsperren, Spawnwellen und spätere Wiederbesuche; erzwungenes RSF8 aus dem Vorschauprofil gilt nicht als Kampagnenzustand | `android_bridge.cpp` bei `testProfile == 1`, `r101.cpp` |
| Geometrie/Blöcke | Allgemeine SMD/SMX/SAT/EAT-Parser; fünf originale r100-Blöcke | Originale Objekt-Sichtbarkeit und Transformationen nach RoomInit; Zusatzinstanzen, bewegliche/zerstörbare Teile | `room_blocks.cpp`, `scene.cpp`, `r109.cpp`, `r119.cpp`, `r11e.cpp` |
| Kollision | SAT-Abfragen; ungeparentete SCR_AT-Startflächen; geprüfte r100-Fahrzeugschalter | Allgemeine Live-Schalter, parentierte Flächen, Instanzen und bewegliche Böden; synchron für Gehen, Kamera, Sicht und Treffer | `scenario_collision.cpp`, `upstream/re4/src/game/atari.cpp` |
| Gegnerlisten | GC-ESL-Parser und r100-Encounterpfade | Originales `checkEmListNo`, vollständige Listenwechsel, Todesflags, Ereignisspawns und gemeinsame Gegnerdispatches | `enemy_spawn.cpp`, `enemy_encounter.cpp`, `upstream/re4/src/game/stage.cpp` |
| Speichern/Laden | Originalcontainer und vorhandene Zustandsbesitzer | Raum-/ESL-Guards erweitern, erst wenn die neuen Besitzer vollständig restauriert werden; Part, Vorraum, Ashley, Rätsel, Türen, Drops und zerstörte Objekte erhalten | `android_bridge.cpp` bei `savedGameProblem`/Lader, `save_native.cpp` |
| Spezialmechaniken | Einzelne gemeinsame Vorläufer vorhanden | Boot/See, Del Lago, El Gigante, Mendez, Gondel, Ashley, Hüttenbelagerung, Rätsel, Schlüssel und Händler | Originale Raumroutinen und `src/game`-Abhängigkeiten |
| Bild/Ton/Karte | Allgemeine Renderer, Effekte, HUD, Audio- und Kartenpfade | Raumbezogene Camera-/LIT-/EFF-/SST-/EAR-Zustände, Boss-/Partneranzeige, Originalgeräusche und Kartenfreigaben prüfen | Raumarchive und jeweilige Originalroutinen |

Von den 66 AEV-Türdatensätzen ist im aktuellen RoomFlow-Prüflauf **nur r100 AEV0 → r101 verfügbar**. Die anderen 65 werden noch nicht von der Dorf-Kampagne freigegeben. Der bestehende r300/r301-Testkreis liegt außerhalb dieses Umfangs.

Das Laden einer Raumdatei, eine statische Vorschau oder ein vorhandener generischer Gegneradapter genügt nicht als Fertigstellungsnachweis. Insbesondere werden nicht alle 428 ESL-Zeilen gleichzeitig gespawnt.

## Übergänge, die gesondert behandelt werden müssen

| Fall | Originalbefund | Umsetzung und Abnahme |
| --- | --- | --- |
| r120 → r100 | Kein AEV in r120; `R120Event` ruft `SceAtExecRoomJump` auf | Film/Event-Ende und Überspringen müssen denselben vorgesehenen Einstieg erzeugen |
| r100 ↔ r101 | Rückweg kann r101-Kampfcallbacks erhalten | Beide Richtungen in den original erlaubten Zuständen testen; keine pauschale Freigabe |
| r106 → r104 | AEV enthält diese Richtung, r104 keinen direkten AEV-Rückweg nach r106 | Kapitelende und Neueinstieg mit Originalablauf prüfen; keinen Gegenausgang erfinden |
| r108 → r117 → r118 | Tag-/Nachtpfad, kein symmetrisches r117 → r108 im AEV | Storyflags, Kirchenzugang und Rückkehr über Nachtvarianten gemeinsam prüfen |
| r10b → r11b | See-/Boss- und Kapitelübergang | Boot, Bossende, Checkpoint und Listenwechsel erhalten |
| r10e → r10e | AEV4: Part 1; AEV5: Part 2 | Dieselbe Raum-ID ist ein echter Neueinstieg mit anderem Part und Spawnkontext |
| r118 → r117 | AEV0: Part 1 und Schlüssel-ID 1; AEV4: Part 0 | Beide Authored Zugänge und ihre Aktivierung separat testen |
| r117 → r118 | Zwei Türen, darunter AEV0x0b mit Schlüssel-ID 2 | Nicht auf einen einzigen Raumpaar-Link reduzieren |
| r11c → r11d / r11e | Routenwahl mit eigenem persistenten Zustand | Beide Routen, Sperren, Ashley und Wiederbesuch prüfen |
| r10f → r200 | Einziger AEV-Ausgang aus Stage 1 | Schlüssel-/Storyvoraussetzung und saubere Stage-2-Übergabe testen |
| Kapitelenden | Eigene `SceSetChapterEnd`-Aufrufe in r106, r105, r10b, r117 und r11c | Kapitelbildschirm, Speichern und Folgeeinstieg zusätzlich zum AEV-Graphen portieren |

Die vollständige gerichtete Tabelle mit Zielposition, Blickrichtung, Schlüssel, Part und Startflags steht in `VILLAGE-TRANSITIONS.tsv`. Sie beschreibt die Originaldaten vor deren Skriptänderungen, nicht sämtliche zur Laufzeit möglichen Wege.

## Umsetzungsfolge

### 0. Ausgangsstand sichern und reproduzieren

- K14 als unveränderte Ausgangsbasis behalten. LOAD/SAVE und Lebensanzeige sind vom Benutzer bestätigt; für K14 ist der Gerätetest nach Demoende und erstem DPAD weiterhin offen.
- Den bekannten Hostbefund `Filter00: incomplete framebuffer` beim r120-Kontextwiederaufbau gesondert nachstellen und beheben. `K13-KORREKTUREN.md` dokumentiert ihn auch für die ältere Vergleichsbibliothek. Er ist kein bestandener Einstiegstest und kein Nachweis eines aktuellen Gerätefehlers.
- Den hier beigefügten Datenaudit erneut ausführen und einen Native-/JNI-/GLES-Baseline-Lauf protokollieren. Referenzversion, Hashes, Toolversion und Testzustand festhalten.

### 1. Gemeinsame Kampagnen- und Raumgrundlagen

- Einen Raumkontext für Room-ID, Part, Vorraum, Stage, Story-/Room-/Item-/Key-/Objekt-/Gegnertodesflags, Liste und Partnerzustand festlegen. Originale Listen-/Flagidentitäten erhalten; keine Room-ID-Sonderfälle als Ersatz für Szenariologik ausweiten.
- Originale `Init`, `Main`, wartende Szenariotasks und Ereigniscallbacks mit klaren Besitzern portieren. Aufgaben laufen im bestehenden 30-Hz-Welttakt und werden bei Pause, Raumende oder Wiederherstellung kontrolliert angehalten bzw. beendet.
- AEV-Callbackfelder sind serialisierte GameCube-Adressen. Sie werden semantisch an portierte Routinen gebunden; sie sind keine auf ARM64 aufrufbaren Funktionszeiger. Gleichnamige Helfer verschiedener ursprünglicher REL-Module erhalten eindeutige Raum-/Modulbesitzer.
- Raumwechsel als vollständige Transaktion durchführen: Voraussetzungen, Ausgangscallback, Ressourcen, Original-Zielposition/Rotation/Part, RoomInit, Eintrittscheckpoint, Renderer/Audio und erfolgreiche Veröffentlichung. Fehlgeschlagenes Laden muss den alten Raum samt Besitzern erhalten.
- Dynamische SAT/EAT-Instanzen und SCR_AT-/Objektschalter gemeinsam mit den sichtbaren Modellen aktualisieren. Parent-Transformationen, Drehung/Skalierung und Originalpolygone verwenden; keine ungeprüften Ersatzboxen für versperrende Objekte.
- ESL00/01 gemäß `stage.cpp::checkEmListNo` laden: r120 verwendet 00, r100–r10b normalerweise 00, spätere IDs 01; r10e kann die aktuelle Liste behalten oder bei `System_flg & 0x2000` 01 wählen. Diese Ausnahme ausdrücklich testen. Sondermodi werden nicht in die Leon-Kampagne eingemischt.
- Persistente Besitzer und Wiederherstellungsreihenfolge implementieren, bevor zusätzliche Raum- oder Listenwerte im Save-Lader zugelassen werden. Originales RoomInit darf beim Laden keine bereits abgeschlossenen Ereignisse oder zerstörten Objekte zurücksetzen.

### 2. Erster zusammenhängender Kampagnenabschnitt

**r120 → r100 → r101 → r103 → r106**, einschließlich legaler Rückwege, Dorfkampf und Kapitelende. r102 als nächstes vollständiges Paket vorbereiten, dessen Schlüsselfreigabe aber im Originalfortschritt belassen.

r101 ist die erste große Integrationsprobe: normale Ankunft ohne Vorschauflags, Wahrnehmung/Alarm, Hausereignis, Gegnerwellen, Turmzustand, Glockenende und anschließende Türen. Gemeinsame Ganado-/Leon-Aktionen, Originalkamera, Hindernisaktionen und Wegwahl müssen die hier tatsächlich verwendeten Quellzweige abdecken. Danach r103-Objekte und r106-Ereignis/Kapitelneustart fertigstellen.

### 3. Tagroute bis zum See

**r104, r107, r105**, danach der freigegebene Rückweg über **r101, r102, r108, r109, r10a, r10b**. Dazu gehören Händler-/Schlüsselabläufe, Behälter, Rätsel, Fallen/Boulder/QTE und Del Lago mit Bootsteuerung, Harpune, Kamera, Treffer-/Todeszuständen sowie dem vollständigen Bossende. Rätsel- und Händlerfunktionen erhalten ihre originalen Inventar-/Fortschrittswirkungen; die separat aufgeschobene Inventaroptik wird dadurch nicht zur Voraussetzung einer neuen Layoutarbeit.

### 4. Nacht, Wasserfall und Kirchenrettung

**r11b, r10d, r10c, r10e, r119, r11a, r118, r117**. Bewegliche Kisten/Böden, Wasser-/Bootsrendering, Schleusen, Live-Kollision, Inseln, El-Gigante-Zerstörung und Hundehilfe bilden gemeinsame Abhängigkeiten. Anschließend Kirchenzugang, Lichtmechanismus, Kronleuchter, Ashley-Rettung und der vorgesehene neue Nachtzustand.

### 5. Ashley, Belagerung und beide Routen

**r112, r111, r113, r11c, r11d, r11e**. Ashley muss folgen, warten, sich verstecken, klettern und die Original-Gefahren-/Verlustzustände auslösen. Luis, Hüttenwellen, Leitern, Routenhebel und deren persistente Zustände gehören zum Raumskript. Beide Alternativrouten sind eigenständige Pflichtpakete; ein Durchlauf nur einer Route nimmt diesen Abschnitt nicht ab.

### 6. Gondel, Mendez und Stage-Ende

**r10f, r11f**, dann zurück zur Gondelroute und **r200-Übergabe**. Bewegliche Gondeln tragen Leon, Ashley und Gegner korrekt. Mendez erhält die Originalphasen, QTE-/Eventäste, Schaden/Tod, zerstörte Türen, Beute und den anschließenden Schlüsselfortschritt.

### 7. Vollständiger Original- und Gerätedurchlauf

Den normalen Kampagnenpfad und die zweite Alternativroute mit definierten Referenzspielständen durchlaufen. Bilder, Kamera, Ton, Gegner-/Eventfolge und Zeitverhalten mit derselben US-Originalversion vergleichen. Vor-/Nachzustände zusätzlich aus gespeicherten und geladenen Spielen prüfen. Statische Parserchecks ersetzen diesen Durchlauf nicht.

## Raumweise Arbeitspakete

Alle Pakete benötigen zusätzlich die unten beschriebenen gemeinsamen Abnahmekriterien. Die Quellanker sind Einstiegspunkte, keine Behauptung, dass nur die genannten Funktionen portiert werden müssen. Die vollständige Kandidatenliste steht im Source-Audit.

| Raum / Editorname | Phase | Obligatorische Mechaniken und Zustände | Originalanker |
| --- | --- | --- | --- |
| r120 / OPENING | 0–2 | Film, beide Autoereignisse, Spiegel, Skip, einmalige Übergabe an r100 | `R120Event`, `Evt_R120S00_Func`, `Evt_R120S01_Func` |
| r100 / MORI | 2 | Bestehende Haus-/Truck-/Brückenfolge, Fahrzeuge vor/nach RSF10, Hund/Fallen, Rückkehr, Save/Load | `R100Init/Main`, `r100_Car_pos_move`, `r100_Sce_zombi_dead` |
| r101 / MURA | 2 | Alarm, Haus/Turm, Spawnwellen, Glockenende, Schlüsselzugang, alle vier Ausgänge und spätere Ankunft | `r101_checkEmNum`, `r101_checkEmReset`, `r101_DoorCk`, `r101_Event30` |
| r103 / FARM | 2 | Cesspit-Abdeckungen, Schränke/Items, Dokument, Submission-Ziele, Ausgang r106, Wiederbesuch | `r103_initCesspit`, `r103_openShelf`, `r103_setSubMissionTarget` |
| r106 / HAISON | 2 | Boulder/QTE, Gegnersteuerung, Luis-Schrank, Event und Kapitelübergabe r104 | `r106_setRollingStone`, `r106_setCloset`, `r106_Event` |
| r104 / KANKIN | 3 | Ankunftsereignis, Patrouillen, Resetwellen, Behälter, Doppeltür, Schlüssel und r107-Ausgang | `r104_initEmPatrol`, `r104_checkEmReset`, `r104_checkDoor107` |
| r107 / KANKIN_2 | 3 | Doppeltür, Kiln-Items, Fische/Wasserhöhe, Kampfmusik, Übergänge r104/r105 | `R107Init/Main`, `r107_setFish`, `r104_openKiln_main` in r107.cpp |
| r105 / SONTYOU HOUSE | 3 | Insignien-Drehpuzzle, Schränke/Abdeckung, Schlüssel, beide Eventzustände, Gegnerfreigabe und Kapitelende | `r105_mark*`, `r105_initCesspit`, `r105_Event`, `r105_checkDoor` |
| r102 / TIKADOU | 3 | Abdeckung geschlossen/offen, einmaliges Ereignis/Händler, beide Ausgänge, Rückweg | `r102_openCover`, `r102_execEvent00`, `r102_checkBgm` |
| r108 / TOMB | 3 | Glocke/Gegnerreset, Symbolpuzzle, Schlüsselzugang zur Kirche, Altaransicht, Tagzustand | `r108_initChurchBell`, `r108_initPuzzle`, `r108_checkDoor` |
| r109 / COLOSSEUM | 3 | Drei zusätzliche SAT/EAT-Hütteninstanzen, Intaktmodelle, gelöschte Fackeln, drei Ausgänge | `R109Init`, `koya_init` |
| r10a / NUMA | 3 | Boulder/QTE und normale/Verstärkungswellen A/B, Wasserattribute, Kampfmusik | `EmSetNormal`, `EmSetZouenA/B`, `r10a_zouen_ck` |
| r10b / MIZUUMI | 3 | Boot, Del Lago, Inseln, Fernglas/Eventzweige, Boss-/QTE-Tod und Kapitelende | `R10bInit/Main`, `R10b_chkEmDie`, `Evt_R10BS*`, `r10b_setEm` |
| r11b / MIZUUMI (night) | 4 | Bootsanlegepositionen, Wasser-Renderziel, Nachtflag, Event/Wellen und Rückwege | `R11bInit`, `EmSetChange`, `r11b_bort_pos_chk`, `r11b_evtTexRenderSet` |
| r10d / BUKI-SYOUNIN(night) | 4 | Datengetriebene Objekte/ESL/Händler, zwei Leitern, korrekter Seezugang und Startpunkt | `R10dInit/Main`, Raum-ETS/ITA/AEV/ESL |
| r10c / TAKI(night) | 4 | Seilkisten und Trefferboxen, fallende/begehbare Kisten, Wasserrad, Schalter/Tore, Hinterhalt und Schlüssel | `SetEmHitAtari`, `hako_down`, `SwitchExec`, `eat_swap` |
| r10e / SUIRO(night) | 4 | Storysperre, Entry-/Listenabhängigkeit, Selbstübergänge Part 1/2, verzögerte Türfreigabe | `R10eInit`, `R10e_door_set`, `stage.cpp::checkEmListNo` |
| r119 / COLOSSEUM(night) | 4 | El Gigante, Parasit/Hund, Hütten-/Dachzerstörung mit SAT/EAT, Sturm und drei Ausgänge | `r119_EventGolemAppear`, `r119_EventDogAppear`, `koya*_delete`, `Yane*_delete` |
| r11a / NUMA(night) | 4 | Originale Wasser-/Fußeffekte, ESL-Akteure, beide Übergänge und Wiederbesuch | `R11aInit/Main`, Raumdaten |
| r118 / TOMB(night) | 4 | Kirchenkey, vier Authored Ausgänge/Parts, Symbolpuzzle, Ashley-Ruf, Hund und Gewitter | `r118_checkDoor117*`, `r118_execAshleyVoice`, gemeinsames `r108_initPuzzle` |
| r117 / CHURCH(night) | 4 | Lichtpuzzle samt persistenten Drehstellungen, Kronleuchter, Ashley/Saddler, Ausgänge und Kapitelende | `r117_LightMechanism*`, `r117_EventChandelier`, `r117_EventAshleyFind` |
| r112 / TIKADOU(night) | 5 | Nachtvariante, Gewitter, gemeinsame BGM-Logik, zwei Ausgänge mit Ashley | `R112Init`, `r112_ThunderMove`, gemeinsames `r102_checkBgm` |
| r111 / MURA(night) | 5 | Umgesetzte Racks, vorzerstörte Fenster, Regen/Bodenattribute, Nacht-ESL, Rückwege | `R111Init`, `r111_ThunderMove` |
| r113 / FARM (night) | 5 | Gemeinsame Farmobjekte, Ashley-Versteck/Schulteraktion, Torvoraussetzungen und Gewitter | `R113Init`, `r113_execHide`, `r113_EventRideShoulder`, `r113_DoorCheck` |
| r11c / ROUZYOU(night) | 5 | Luis/Ashley, Hüttenwellen, Kapitelende, persistente Leitern, Händler, Hebel/Zahnräder/Kette, beide Routen | `r11c_EventBesiegedStart`, `r11c_initGate`, `r11c_selectRoute`, `Evt_R11CS*` |
| r11d / SEKISYO(night) | 5 | Bella-Schwestern, Wellen/Patrouillen, Keydrop/Eisentor, drei Ashley-Verstecke | `r11d_execEmAppear`, `r11d_setEmSister`, `r11d_checkIronDoor*`, `r11d_execHide*` |
| r11e / HOKORA(night) | 5 | El Gigante, zerstörbare Hütten/Zäune, Boulder, Ashley-Flucht, Schlüssel und Live-Kollision | `r11e_EmSet`, `funcAshley`, `koya*_delete`, `saku*_delete`, `r11e_checkDoor` |
| r10f / GONDOLA(night) | 6 | Zehn Gondelobjekte, Auf-/Absteigen mit Partner, mitfahrende Gegner, Behälter, False Eye und Stage-Ausgang | `r10f_GondolaGetOn/Off`, `r10f_GondolaEmSet`, `r10f_checkFalseEyeUse` |
| r11f / SONTYOU-BOSS(night) | 6 | Mendez-QTE und beide Kampfphasen, Ersatz-Türen, explosive Objekte, Bossende/Flucht/Ashley | `r11f_DoorReplace`, `r11f_EventS00/S10/S11`, `Evt_R11FS*` |

## Abnahme: Was „100 % korrekt“ pro Raum bedeutet

Ein Raum erhält keinen Fertigstatus nur aufgrund eines erfolgreichen Imports. Folgende Nachweise gehören zu **jedem relevanten Originalzustand**: Erstbesuch, nach dem lokalen Ereignis, legaler Wiederbesuch und Wiederherstellung eines dort erlaubten Spielstands/Checkpoints. Part, Vorraum und Partnerzustand werden zusätzlich variiert, wenn die Quelle danach verzweigt.

| Kriterium | Nachweis |
| --- | --- |
| Ressourcen/Geometrie | Originalmodelle, Materialien, Texturen, Beleuchtung, sichtbare Objektgruppen und alle erforderlichen Instanzen; kein Ersatz für fehlende Originalteile |
| Begehbarkeit | Boden, Wände, Stufen, Gefälle, Brücken, Leitern, bewegliche Böden und versperrende/zerstörte Objekte in beiden Zuständen; Tests von mehreren Annäherungen und bei schneller Bewegung |
| Raumübergänge | Jede Tür/Part-Variante einzeln; Originalvoraussetzungen, Zielposition/Rotation, Vorraum, Callback, Fade, Rückweg und Ladefehler-Rollback |
| Objekte/Items | Original-ETS/ITA-Identität, Aktivierung, Öffnen/Zerstören, Schlüssel-/Itemflags, Dropbesitz und Zustand nach Verlassen/Load |
| Gegner | Originale Liste/Listenindex, Modell/Typ/Waffe, Spawn-/Wellenbedingung, Wahrnehmung, Route, Hindernisaktion, Schaden/Tod, Drops und Todesflagpersistenz |
| Ereignisse/Rätsel | Alle tatsächlich erreichbaren Erfolgs-/Fehlschlag-/Skipäste, Flagfolge, QTE, Kamera, sichtbare/physische Zustandswechsel und anschließende Steuerung |
| Ashley/Luis/Bosse | Originaler Dispatch für die im Raum genutzten Aktionen, Partner-/Bossanzeigen, Verlust-/Todeszustände und Save/Checkpoint-Eigentümer |
| Grafik/Ton/Karte | Vergleichsaufnahme mit identischem US-Spielzustand, Originalkamera, Raum-/Ereigniston, Kartenfreigaben und korrekter Rückkehr ins Gameplay |
| Lebensdauer/Gerät | Pause/Resume, EGL-Verlust, wiederholte Raumwechsel und längerer Spieldurchlauf ohne stale Task-/GPU-/Audiobesitzer |

Der Status wird getrennt geführt als **implementiert**, **native/JNI/GLES geprüft**, **auf Android gespielt** und **mit Original verglichen**. Ein unbekannter oder übersprungener Originalzweig bleibt offen. Prüfzahlen sind keine Prozentquote.

Für Skriptsperren wird jeweils vor und nach der Freigabe getestet. Für zerstörbare Objekte werden sichtbares Modell, Walk-/Sight-/Effect-Kollision und gespeicherter Zustand gemeinsam geprüft. Für Gegnerwellen werden Reihenfolge, Bedingungen und Todesspeicher geprüft, nicht nur die Anzahl gerenderter Gegner.

Die aktuelle statische Startpunktdiagnose findet bei r109, r10d und einem r10c-Startpunkt keinen Boden im engen Suchbereich; bei je einem r105-/r117-Startpunkt ist die Testkapsel nicht frei. Das ist ein **Prüfkandidat**, kein nachgewiesener Spielfehler: RoomInit-Instanzen, bewegliche Geometrie und Eventstarts fehlen dieser Diagnose. Diese Fälle erhalten gezielte Eintrittstests statt künstlich versetzter Startpunkte. r120 wird über seinen Ereigniseinstieg geprüft.

## Wiederholbarer Audit

`tools/audit_village.py` nutzt ausschließlich Python-Standardbibliotheken. Voraussetzung sind die Originalextraktion, bereits entpackte Räume und optional die Hosttools aus dem aktuellen Quellstand. Variablen im folgenden Beispiel bezeichnen lokale Pfade; keine DVD-Dateien gehören in das Vorbereitungspaket.

```bash
python3 tools/audit_village.py \
  --game-files "$village_disc1_files" \
  --decoded-rooms "$village_decoded_rooms" \
  --out docs/village \
  --room-flow-tool "$village_native_build/room_flow_tool" \
  --room-resource-tool "$village_native_build/room_resource_tool"
```

Ohne Hosttooloptionen wird nur die Daten-/Quellinventur erzeugt; sie darf dann nicht als native Prüfung bezeichnet werden. Der aktuelle geprüfte Lauf verwendete beide Tools. Hosttools liefern statische Diagnosen, keine Kampagnenfreigabe.

**Nächster Implementierungsschritt:** nach dem Baseline-Lauf die gemeinsamen Raum-/Save-/Szenariobesitzer definieren und r101 als vollständigen Kampagnenraum portieren. Der erste abnehmbare Meilenstein ist r100 ↔ r101 mit originalem Dorfkampf, Glockenende, korrektem Rückweg und Wiederherstellung des Fortschritts; anschließend folgt der Ausgang nach r103.
