# Ganzprogramm-Port: Stand

Diese Datei ist das Gedächtnis zwischen den Sitzungen. Jede Sitzung liest sie
zuerst und aktualisiert sie am Ende (siehe `CLAUDE.md`).

## Kurzstand

| Meilenstein | Stand |
|---|---|
| M1 Probelauf | ✅ 13.10.2026 (`whole-program-probe.md`) |
| M2 Alle Spieldateien kompilieren (ARM32 + 32-bit-Linux) | ✅ 13.10.2026 |
| M3 Linken, lauffähiges 32-bit-Linux-Programm, Module als eigene Bibliotheken | ✅ 13.10.2026 |
| M4 Teil A: Plattformschicht ohne Disc-Daten (Cloud) | ✅ 13.10.2026 |
| M4 Teil B: Booten mit echten Disc-Daten bis `gameRoomInit` (lokal, WSL2) | ✅ 13.10.2026 (i686 und armhf unter qemu) |
| M4.5 Kern korrekt machen: C-Ersatz für Assembler, Byte-Aggregate, `__FILE__`-Länge | ✅ 14.10.2026 (Cloud, i686 und armhf) |
| M5 Erster Raum r100 | ✅ 14.10.2026: lokal mit Disc-Daten (i686 in WSL2) 10 000 Bilder ab der Hauptschleife ohne Absturz, Abdeckung ohne „nein“, Formatvergleich 152 Dateien ohne Fehler. armhf mit Disc-Daten nicht gelaufen (`-Arm`, optional) |
| K2 „Kern in die App“ (M6) | ⏳ 14.10.2026: Cloud-Teil fertig (Einbettungs-Schnittstelle, App-Variante „core“, Zustandsbrücke, Prüfungen ohne Disc-Daten grün); NDK-Build, APK-Build und Retroid-Test stehen aus (lokal, `port/scripts/test-core-app.ps1`) |
| G1–G4 GX nachbilden (Teil von M6, `gx-plan.md`) | ⏳ 15.10.2026 (Cloud): GX-Bibliothek aus `src/lib`, FIFO, PE-Unterbrechungen (G1); Befehlsdecoder mit Bildstücken (G2); GLES3-Renderer in der App (G3); App zeigt standardmäßig das GX-Bild, PC-Prüfung zeichnet GX-Bilder (G4). Alle Cloud-Tests grün; mit Disc-Daten und auf dem Gerät noch nicht getestet. G5 offen |

**Entscheidung des Projektinhabers (14.10.2026):** Endprodukt ist
RE4-Android-Decomp (`port/android-decomp/`, Subtree von `re4-standalone`). Der
Kernmodus läuft vorerst als armeabi-v7a (ARM32); der bestehende ARM64-Modus der
App bleibt parallel. Die App bleibt Hülle (Disc-Import, GLES3-Grafik,
Controller, Tonausgabe, Menü); die Spiellogik kommt im Kernmodus aus dem
Originalcode, die eigenen Spielmodule der App werden dort nicht mehr
erweitert.

## Bauen und prüfen

```sh
# Werkzeuge (clang, lld, cmake, ninja, lib32-Bibliotheken, ARM32-Cross-Compiler,
# qemu-user): sudo port/tools/whole_setup_linux.sh. In der Cloud ruft das
# Startskript .claude/hooks/session-start.sh es automatisch auf; lokal in WSL2
# (Ubuntu 24.04) einmal von Hand. g++-multilib geht nicht: Ubuntu verweigert es
# neben dem ARM-Cross-Compiler.
port/tools/whole_build.sh          # baut und prüft i686 und armhf
RE4_JOBS=4 port/tools/whole_build.sh   # höchstens 4 Compiler gleichzeitig (Speicher)
RE4_OPT=2 port/tools/whole_build.sh    # mit -O2 wie der Android-Build (Ordner whole-<ziel>-O2)
```

Je Ziel: Build, dann drei Prüfungen von `re4_host` und der Formatvergleich
(armhf unter `qemu-arm`):

| Prüfung | Inhalt |
|---|---|
| `--self-test` | Weiterleitungen zwischen Modulen geben Argumente korrekt weiter |
| `--check-modules` | alle 114 Module laden, alle Querverweise lösen sich auf |
| `--core-test` | Einbettungs-Schnittstelle der App ohne Disc-Daten (`port/whole/tests/core_tests.cpp`, `core_probe.cpp`): `bridge` Schnappschuss aus einem nachgebauten Gegner (EmMgr, Teile, Modell-Info, Kamera, Raum), `thread` Kern-Thread mit Testeinstieg, Controller über `re4_core_pad`, Dreifachpuffer |
| `--runtime-test` | Plattformschicht ohne Disc-Daten (`port/whole/tests/runtime_tests.cpp`): `link` OSLink/OSUnlink mit synthetischem big-endian REL-Kopf und Testmodul; `dvd` DVD-API über einen synthetischen Zwei-Disc-Ordner; `yz2` Yz2 des Spiels + Kern mit den Testdaten `port/tests/yz2_fixtures.h`; `dvdq` die DVD-Warteschlange des Spiels liest eine synthetische mehrteilige Datei in den Spiel-Heap; `stream` der Stream-Code des Tontreibers spielt synthetische Stream-Dateien (Nachladen im Audio-Rahmen, Aufträge aus dem Interrupt enden erst danach); `math` PS*/C_*-Matrixfunktionen, die C-Fassungen der Gekko-Assembler-Funktionen (Tabelle mit Bitmustern aus einem Python-Modell der Paired-Single-Befehle), Skinning `CalcSk1` mit GQR-Quantisierung, `EmRoutineSetW`, die big-endian Sichten, die Länge von `__FILE__`; `trace` Zustands-Trace, GetDataExt-Abdeckung, Ende des Test-Tastenskripts; `tasks` der Task-Scheduler des Spiels (`TaskExec`, `TaskSleep`, `TaskExit`, `iTaskExec`) auf der Thread-Emulation, dazu VI-Retraces |
| `re4_format_compare` | Formatumwandler gegen die geprüften big-endian Leser der App, mit synthetischen Beispielen jedes verglichenen Formats; danach `port/tools/test_whole_format_compare.py`: dieselben Beispiele als Disc-Ordner (Raumdatei Yz2-gepackt, `.drs`-Container), Modus `--data` (siehe „Formatvergleich mit der App“) |

Danach einmal `port/tools/test_whole_disc_folder.py` (Disc-Ordner-Werkzeug mit
synthetischen Disc-Abbildern). Rückgabewert 0 heißt: alles bestanden.

`re4_host` ist seit M4 Teil B ein kleiner Vorlader (`host/loader.c`): Er hält
den Adressbereich unter `0x83000000` frei und lädt dann `libre4_host.so`
(Startprogramm und Laufzeittests, `host/main.cpp`) mit den Spielbibliotheken,
die deshalb oberhalb von `0x80000000` liegen wie auf Android (siehe
„Adressen der Spielbibliotheken“).

## M4 Teil B: Booten mit Disc-Daten (lokal, WSL2)

```sh
# einmal: Disc-Ordner außerhalb des Repositorys (Abbilder werden nur gelesen)
port/tools/whole_disc_folder.py --disc ".../RE4_2004-11-25_Disc1.gcm" \
    --disc ".../RE4_2004-11-25_Disc2.gcm" --out ~/re4-data
port/tools/whole_disc_folder.py --check ~/re4-data
# Spielstart mit Test-Tastenfolge (Speicherkarten-Dialog, Titel, Menüs)
RE4_PAD_SCRIPT="UP@100-3000/60 A@115-3000/60 START@130-3000/60" \
    port/build/whole-i686/re4_host --data ~/re4-data
# armhf: QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf qemu-arm port/build/whole-armhf/re4_host ...
```

Disc-Ordner (13.10.2026): Disc 1 1154 Dateien, Disc 2 1090 Dateien, 0 Probleme.

**Nachweis (13.10.2026), i686 und armhf (qemu-arm) gleich:** Der Originalcode
läuft mit echten Disc-Daten durch `systemStartInit` (Ton-Tabellen, Schrift,
Größentabelle), `systemRestartInit` (`debug/roomInfo.dat`), `Title_task`
(Kernarchiv `etc/core.das` mit Tonbank, `SS/eng/option.dat`, Speicherkarten-
Prüfung „keine Karte“ → „ohne Speichern fortfahren“, `SS/eng/title.dat`,
Titel und Titelmenü), das Debug-Startmenü (`titleExit`, Raum r120 = Stufe 1,
Raum 0x20), `GameTask` → `gameInit` → `gameStageInit` (`rel/st1_0.rel` per
OSLink, `etc/emleon00.esl`) bis in **`gameRoomInit`**: `ReadPlayerData` lädt
`em/pl00.drs` mit Tonbank, `ReadAreaData` lädt `st1/r120.das`, der Yz2-Kern
entpackt 6 086 688 Byte, der Formatumwandler wandelt das Raumarchiv um. Danach
stürzt `ModInfoMgr.arrayAlloc` ab, weil die Raum-Zähler (`CNS`) noch
big-endian sind: Das ist der Einstieg in M5. Letzte Logzeilen:

```
[re4] OSReport: DVD: Read File: em/pl00.drs
[re4] formats: em/pl00.drs part 0.0: archive
[re4] OSReport: DVD: Read File: st1/r120.das
[re4] formats: st1/r120.das part 0.0: room archive (Yz2)
[re4] formats: st1/r120.das (unpacked): archive, 6086688 bytes
[re4] fatal signal 11 ... cModInfoMgr::memClear <- cManager<cModelInfo>::arrayAlloc <- gameRoomInit
```

`port/tools/whole_build.sh` am Ende der Sitzung (13.10.2026): i686 und armhf
je 0 fehlgeschlagene Schritte, 0 Fehler; `--self-test`, `--check-modules`
(114 Module, 0 offene Querverweise, 209 Platzhalter, alle Module oberhalb von
`0x80000000`) und `--runtime-test` (alle sechs Teile) bestanden, Disc-Ordner-
Werkzeug ok.

Die Tastenfolge drückt alle 60 Bilder OBEN, 15 Bilder später A, 15 Bilder
später START. OBEN+A wählt im Speicherkarten-Dialog „ohne Speichern
fortfahren“ (A allein wählt „erneut prüfen“). Die Bildnummer, ab der der
Dialog bereit ist, schwankt zwischen Läufen; die Wiederholung fängt das ab.

**M4-Teil-A-Nachweis (13.10.2026), frischer Build, 1:26 min:**

```
i686: 0 failed build steps, 0 distinct errors
  link plan: 234 function stubs, 1 data stubs, 893 cross-module calls, 1 cross-module data imports (private copies)
  re4_host --self-test: exit 0 | self-test: trampolines ok (code 0)
  re4_host --check-modules: exit 0 | check-modules: 114 modules, 0 failed to load, 0 unresolved imports, 234 stubs
  re4_host --runtime-test: exit 0 | runtime-test link: ok dvd: ok yz2: ok dvdq: ok math: ok tasks: ok, 0 failed
armhf: (gleiche Zeilen, ausgeführt unter qemu-arm)
disc folder tool: ok
```

Alle Bibliotheken linken mit `-Wl,--no-undefined`: Kein Symbol bleibt offen.

**Spielstart (`re4_host` ohne Disc-Daten), i686 und armhf gleich:** Der
Originalcode läuft als Boot-Thread der Thread-Emulation durch fast ganz
`systemStartInit`: OSInit (MEM1, Arena), `SystemMemInit` (Heap 0
`0x809740a0–0x817f4000`, `pRK`), DVD-Init, CARD, `TaskSchedulerInit`,
Bildschirm, GX-Init (Platzhalter), Lichter, Sound-Init (ARAM, liest
`bgm/*.hed`, `etc/memcard.das`), Sofdec, Id-System, Log, Debug-Modul, Größentabelle.
Jede Datei meldet „File not found“, weil die Cloud keine Disc-Daten hat. Der
letzte Schritt `cMes.init` → `loadSystemFont` → `setupFont` stürzt ab, weil
`Font/common_p.fnt` fehlt. Hier ist ohne Disc-Daten Schluss.

Mit einem synthetischen Disc-Abbild (Werkzeug-Test) liest die originale
DVD-Warteschlange `bgm/bio4str.hed` an die feste GameCube-Adresse `0x80370000`:
Die Kette Disc-Abbild → Ordner → `DVD*` → Spiel funktioniert.

**Nicht getestet:**
- Der GameCube-Matching-Build (`ninja` im Hauptverzeichnis). Original-Compiler
  fehlen in WSL2. `src/` und `include/` sind unverändert.
- Der Android-NDK-Build und das Reservieren von MEM1 in einer echten
  Port-App. Gemessen ist nur, dass der Bereich in einem 32-bit-App-Prozess frei
  ist (siehe „Adressbereich auf dem Retroid“).
- Spielinhalte jenseits von `gameRoomInit` (M5) und alles, was erst ein Bild
  zeigt (K2): Umgewandelte Texturköpfe, Effekte und Layouts sind nur so weit
  geprüft, wie der Spielcode sie beim Booten liest.
- Stand 14.10.2026 ausdrücklich ungetestet: der Lauf in r100 mit echten
  Disc-Daten (10 000 Bilder), das Abdeckungs-Log mit echten Daten, der
  Formatvergleich mit echten Disc-Dateien, `test-r100-core.ps1` unter echtem
  Windows/WSL (nur PowerShell 7 unter Linux mit nachgebautem `wsl.exe`), die
  ARM32-Ausführung auf echter Hardware (nur qemu-arm).

## M4.5: Kern korrekt machen (14.10.2026, Cloud)

**Assembler mit echten Befehlen** (Inline-`asm` entfällt sonst ersatzlos,
Prelude): jetzt C-Fassungen als Patches, und `whole_prepare.py` bricht ab,
sobald ein `asm` mit echten Befehlen ohne Ersatz entfallen würde
(`port/tools/whole_asm_check.py`, läuft bei jedem `cmake`). Bestandsaufnahme
nach den Patches: 195 Barrieren (`asm("")`, ohne Wirkung), 17 mit Befehlen (alle
ersetzt), 146 Sprungmarken/Kommentare, 155 Abschnitts-/Layout-Direktiven.
Leere `asm`-Kopien mit gebundenen Operanden (`asm("" : "=r"(x) : "0"(y))`)
sind echte Wertzuweisungen und werden zu `x = y` umgeschrieben.

| Funktion | Ersatz | Abweichung von der GameCube |
|---|---|---|
| `fabsf` (math_sub.h) | `re4_fabsf` (re4_ppc.h): Vorzeichenbit löschen | keine |
| `SQRTF` | `sqrtf` | Original: `frsqrte`-Schätzung + ein Newton-Schritt; Ergebnis kann in den letzten Bits abweichen (wie Bewegungs-Hilfswerkzeug und App) |
| `SINF`/`COSF` | Reihe der Paired-Single-Fassung in C (`PsSeries`), `ps_madd` als `fmaf` | bitgleich zum Python-Modell der Gekko-Befehle (`math`-Test, 24 Zeilen) |
| `LIMIT_ANGLE` | Schleife in C, jeder Schritt auf float gerundet | NaN/unendlich: Original hängt, der Port gibt den Wert zurück und loggt einmal |
| `PSQ_L/ST`-Makros (shape, trans, Espgen42, espgen45, dbmodule, pendulum) | `re4_psq_*` (re4_ppc.h): GQR-Skalierung, Sättigung, Abschneiden zur Null | keine (u8/s8/u16/s16, feste GQR2–5) |
| `CalcSk1_x`/`CalcSk1_x2` (trans.cpp, Skinning) | `Sk1Loop` in C mit GQR6 aus `setupGQR6` (Shift je Modell, Normalen 2^-14, s8 2^-6) | Locked Cache `0xE0000000` → Puffer `re4_locked_cache` (16 KB) |
| `dbg_tool.h` `li %0,0` | `z = 0` | keine |

**Byte-Aggregate** (`*(u32*) &bytes`): `port/tools/whole_byte_views.py`
findet mit `clang-query` alle Casts von der Adresse eines Byte-Aggregats auf
einen breiteren Zeiger (Schattenbaum ohne `#line`, sonst stimmen die Orte
nicht). 85 Stellen geprüft, Liste mit Entscheidung je Stelle:
`port/whole/byte_views.json`. 74 werden zu big-endian Sichten (`RE4_BE(T, E,
p)`, Prelude: Lesen/Schreiben/Verbundzuweisung in GameCube-Byte-Reihenfolge),
5 lesen einen Teil eines größeren Feldes (`RE4_BE_AT`), 4 bleiben (gleiche
Bedeutung auf beiden Plattformen), 2 per Patch: `EmRoutineSetW` (em.h, Patch
0023, setzt `r_no_0..3` byteweise) und das Spiegel-Bit in `pl_sub.cpp` (Patch
0024). `whole_prepare.py` setzt die Sichten anhand der Liste ein und bricht ab,
wenn eine Stelle nicht mehr gefunden wird.

**`__FILE__`:** `-fmacro-prefix-map` kürzt Pfade auf `src/game/x.cpp`
(`DvdReadN` kopiert `__FILE__` in 48 Byte). Geprüft mit dem Build in einem
langen Verzeichnispfad (`math`-Test prüft die Länge).

**i686:** `-msse2 -mfpmath=sse`, damit float-Ergebnisse wie auf ARM bitgleich
sind (x87 rechnet mit höherer Genauigkeit). Linux-Testbuilds seit 14.10.2026
mit `_FILE_OFFSET_BITS=64` (readdir/stat scheitern sonst an 64-bit-Inodes, z. B.
Windows-Laufwerke in WSL und qemu-arm).

## M5: Erster Raum r100

### Formatumwandler, Abdeckung, Trace (14.10.2026, Cloud)

Neue Umwandler (Tabelle unter „Formatumwandler“): Raumtabellen, Modelle,
Motionen, Lichter, Kameras, Ereignisse, Kollision, Gegnerlisten. Dazu:

- **Abdeckungs-Log** (Patch 0025): Jeder `GetDataExt`-Zugriff meldet Kürzel,
  Archiv und „umgewandelt ja/nein“ (`nein (kein Umwandler)`, `nein
  (unvollständig)`, `nein (unbekannter Eintrag)`), je Eintrag einmal;
  `RE4_COVERAGE_LOG=DATEI` schreibt zusätzlich tabgetrennt Archiv, Kürzel,
  Nummer, Adresse, Status.
- **Zustands-Trace** (`runtime/state_trace.cpp`, `RE4_TRACE=DATEI`): eine
  JSON-Zeile je Bild: `frame`, `Rno0` (Spielschritt), `room`, `pl`
  (Spielerposition), `rsf` (Raum-Speicherwort), `em` (lebende Gegner: Slot,
  Id, `r_no_0..3`, HP, Position).
- **Debug-Start** (Patch 0026, `RE4_START_ROOM=0x100`): wählt den Raum im
  originalen Debug-Startmenü vor; das Titelmenü steht dann auf „Neues Spiel“,
  so dass ein Test-Tastenskript nur START/A drücken muss.
- **Laufsteuerung:** `RE4_PAD_UNTIL_ROOM=1` beendet das Tastenskript, sobald
  der erste Raum lädt (Spielschritt 2); `RE4_STOP_AFTER=N` beendet den Lauf
  nach N Bildern ab dem ersten Bild der Hauptschleife (Schritt 3) mit
  Rückgabewert 0 und der Logzeile `run: stopped after …`.

### Formatvergleich mit der App (`re4_format_compare`)

Beide Seiten lesen dieselben big-endian Bytes:
- App-Seite (`tests/format_compare/app_side.cpp`, C++17 wie die App): die
  geprüften Leser `archive.cpp`, `smd.cpp` (mit `gx_stream.cpp`), `smx.cpp`,
  `sat.cpp`, `room_flow.cpp` (AEV und Raumsprungtabelle), `enemy_spawn.cpp`,
  `event_data.cpp` (EVD und Kurven), `model_skin.cpp`, `room_lighting.cpp` (LIT
  und Kamerabereiche aus CAM).
- Kern-Seite (`game_side.cpp`, Schalter und Header des Spielcodes): eine Kopie
  wird mit dem Formatumwandler gedreht und über die Originalstrukturen
  gelesen.
Jedes Feld wird als Schlüssel/Wert verglichen (Floats als Bitmuster, lange
Felder als Anzahl + Hash + die ersten 48 Einträge einzeln). Teile, die der
App-Leser ablehnt, erscheinen als „nicht vergleichbar“ mit seiner Meldung.

Synthetische Beispiele (Cloud, i686 und armhf): Raumarchiv mit SMD (mit BIN
und TPL), SMX (Typen 1, 2, 0), SAT, EAT (zwei Dateien), AEV (Tür, Leiter,
Schreibmaschine), LIT (Spot mit Schatten, Typ 4), CAM (B404), zwei BIN
(cWeightExt mit 256 Gewichten / cWeight, s8/s16-Normalen, farbig/nicht),
FCV, ein leerer FCV-Eintrag mit dem Offset des folgenden BIN; Gegnerliste;
EVD (Kamera-, Positions-, Blenden-, Dummykamera-, Effekt- und Endpaket,
Kurven- und LIT-Anhang); Raumsprungtabelle: 5129 Werte gleich.
Gegenprobe: sechs absichtlich eingebaute Umwandlerfehler (SMX `TexV`, SAT
`m_Status`, ESL `rot`, AEV `pExitParam`, CAM `Height`, LIT `Intensity`)
wurden alle als Abweichung gemeldet.

**Gefundener Fehler:** Die Teile-Datensätze der Modelle (`ModelDataHead`)
blieben big-endian; ihr Wort `x0` ist durch Patch 0016 eine Byte-Sicht für
Host-Wörter (`partsNo`, `parentNo`). Jetzt wird der ganze Datensatz
wortweise gedreht (`formats_model.cpp`).

Mit Disc-Daten: `re4_format_compare --data ORDNER [--room r100]` vergleicht
`st1/r100.das` (über den Yz2-Entpacker der App), `etc/*.esl`,
`debug/roominfo.dat`, die Ereignisse `*/r100*.evd`, `etc/core.das` und
`em/*.drs`. Bei Abweichungen nennt es die betroffenen Archiv-Einträge mit
Nummer, Offset, Größe und womit der Umwandler sie behandelt hat
(`Eintrag BIN0. #8 BIN @0xea0, 0xd20 bytes, umgewandelt als BIN`).
Erster Lauf mit echten Daten: siehe „Zweiter lokaler Lauf“.

**Ohne Vergleich mit einem App-Leser** (nur Strukturgrößen per
`static_assert` und die Läufe): CNS, ETS, EMI, SAR, EAR, FSE, ESE, DSE, STB,
BLK, RTP, ITA, SHD, ETM, ITM, EVS, SEQ, TPL, EFF/TEX/VIB, MDT, UWF, Ton.

### Erster lokaler Lauf (14.10.2026, i686 in WSL2)

Der Kern lief vom Titelbild über das Debug-Startmenü bis in **r100**: Raum-
archiv entpackt und umgewandelt, Abdeckung für RTP, MDT, OSD, EMI, CNS, SMX,
LIT, SAT, EAT, AEV, ITA „ja“, `em/wep02.drs` (Pistole) geladen und per OSLink
gebunden („Wep02 HANDGUN prolog Ok“). Danach blieb er in `gameRoomInit`
(Spielschritt 2) stehen; die Bilder liefen weiter (über 2,3 Millionen), es
kamen keine weiteren GetDataExt-Zugriffe (SHD, EFF, CAM … fehlen im Log): Die
Wartestelle liegt zwischen `EmMgr.create(0, 0)` (Spieler) und `ShdInit`.

- **Behoben:** Beide SMD des Raums (`#4`, `#5`) waren nur halb umgewandelt:
  ihr Motion-Tabellen-Offset zeigt genau auf das Blockende (leere Tabelle),
  `tableCount` wertete das als Fehler und brach vor den BIN/TPL-Modellen ab.
  Jetzt gilt ein Offset am Blockende als leere Tabelle; das synthetische SMD
  des Formatvergleichs hat dieselbe Form (Gegenprobe: mit dem alten Fehler
  313 Abweichungen und „Umwandlung unvollständig“).
- **Wahrscheinliche Ursache des Halts:** `SmdSetup` → `SmdSetParam` →
  `cModInfoMgr::create` prüft die BIN-Version; mit den nicht umgewandelten
  Modellen schlägt das fehl, und das Original wartet dann für immer in
  `notBinData()` (`eprintf("not bin data")` + `TaskSleep(1)`, nur auf dem
  Bildschirm, nicht im Log). Das passt zur Wartestelle. Bestätigt erst der
  nächste lokale Lauf.
- **Diagnose für den nächsten Lauf:** `RE4_WAIT_TRACE=1` merkt sich bei jedem
  Threadwechsel die Rückverfolgung (`os_thread.cpp`), `SIGUSR1` gibt danach
  jede Spielaufgabe mit ihrem Warteort aus (`re4_os_dump_threads`); der Wächter
  des Prüfskripts löst das aus, wenn die Hauptschleife nach 10 Minuten nicht
  erreicht ist.
- Archiv-Kürzel `DRA` in r100 hat keinen Umwandler (GetDataExt griff im Lauf
  bis zum Halt nicht darauf zu).

### Zweiter lokaler Lauf (14.10.2026, i686 in WSL2)

Mit der SMD-Korrektur kam der Kern über `gameRoomInit` hinaus bis
`ScenarioRoomInit` → `R100Init` → `EmSetEvent` und stürzte in `Em12Set` ab
(Gegnermodul em12, beim Setzen der ersten Gegner von r100).

- **Ursache des Absturzes:** `em.cpp` trägt `cEm::subArc` (Archiv des
  Gegners) ein und ruft dann die Init-Funktion des Moduls, die das Objekt mit
  `new (em) cEm10()` aufbaut. C++11 füllt bei `T()` eine Klasse ohne eigenen
  Konstruktor erst mit Nullen; der GameCube-Compiler hat bei solchen Klassen
  nur den Konstruktor aufgerufen. Damit war `subArc` wieder 0. **Behoben**
  mit der Umschreibung `PLACEMENT_VALUE_INIT` (Tabelle unten, 142 Stellen:
  Gegner, Objekte, Waffen, Lichter, Modelle). `port/tools/whole_value_init.py`
  fragt clang nach allen Stellen, die danach noch mit Nullen füllen: nur noch
  vier einfache Typen, die auch das Original nullt (`GXColor()` zweimal, die
  Flags `flg()`/`status()` von `cSubChar`).
- **Formatvergleich mit echten Daten:** 152 Dateien, 4 019 092 Werte gleich,
  75 Dateien mit Fehlern. Ursachen:
  - *Leere Archiv-Einträge* (fast alle `em/*.drs`, `pl*.drs`, `wep*.drs`):
    Ein Eintrag ohne eigene Daten hat denselben Offset wie der nächste
    Eintrag. Der Umwandler drehte die Daten mit dem Kürzel des ersten
    (leeren) Eintrags, z. B. ein BIN als FCV; die App-Seite las den leeren
    Eintrag als Daten des nächsten. Jetzt gehören die Daten auf beiden Seiten
    dem letzten Eintrag mit diesem Offset (`formats_arc.cpp`), leere
    Einträge werden übersprungen. Gegenprobe mit der alten Regel: genau das
    Bild aus dem Bericht („BIN0 … Kern (fehlt)“, „umgewandelt als FCV“).
    Die FCV-Meldungen der Waffen (`wep10/21/24/31/32/47`, `pl14`) haben
    vermutlich dieselbe Ursache; **bestätigt erst der nächste Lauf**.
  - *Kollision mit mehr als 512 Blöcken* (SAT/EAT von r100): Grenze nur im
    Vergleichswerkzeug, jetzt dynamisch.
  - *Kernarchiv `etc/core.das`*: CAM ohne Versionskennung (32 Byte) und zwei
    SAT mit unbekannter Version; das Spiel liest sie nicht über die
    Umwandlerpfade. Bleiben jetzt mit Logzeile unverändert statt als
    „unvollständig“ zu gelten.
  - `etc/emleon08/09.esl`: kürzer als 255 Datensätze, der App-Leser lehnt sie
    ab (nicht vergleichbar, kein Fehler).
- Der Zustands-Trace schreibt jetzt nach jedem Bild auf die Platte (vorher
  alle 64 Bilder; im Bericht stand deshalb Bild 575 statt der Absturzstelle).
- Weiterhin ohne Umwandler: Archiv-Kürzel `FNT` (`ss/eng/ss_cmmn.dat`), ein
  leeres Kürzel in `ss/eng/ss_pzzl.dat`, `DRA` in r100 (vielleicht ein leerer
  Eintrag; nächster Bericht zeigt es). Abdeckung im Lauf: 27 Zugriffe, kein
  „nein“.

### Dritter lokaler Lauf: M5 erreicht (14.10.2026, i686 in WSL2)

Stand 1c4f551. Bericht „ALLES OK“:
- **Spiellauf:** Hauptschleife in Bild 621 erreicht (Raum r100), danach
  10 000 Bilder in Spielschritt 3 ohne Absturz (25 s mit `RE4_VI_FAST`).
  Letzte Trace-Zeile: Spieler an (-99682, -484, -1340), 18 lebende Gegner
  (Ids 0x41/0x43/0x46, HP 1000). Tasten gab es nach dem Laden keine, der
  Spieler steht also; ob die Gegner sich wie im Original verhalten, ist nicht
  geprüft (erst mit Bild, M6).
- **Abdeckung:** 31 GetDataExt-Zugriffe, kein „nein“.
- **Formatvergleich:** 152 Dateien, 4 226 888 Werte gleich, 0 Fehler; die
  leeren Einträge waren die Ursache aller Abweichungen des zweiten Laufs.
- **Danach behoben (Cloud):** `op/opNN.das` (Funkgespräche, `ss_term.cpp`
  `opArc`) enthält das Archiv erst 0x400 Byte nach dem Anfang des Teils;
  eigener Umwandler `convertCodecArchive`, im Formatvergleich als
  „Funkarchiv“ (synthetisch mit Gegenprobe; mit echten Daten noch nicht
  gelaufen).
- **Offen, für r100 ohne Folgen:** FCV-Einträge, die auch der App-Leser
  ablehnt und bei denen der Umwandler abbricht (`wep09/10/21/24/31/32/47`
  #16/#28, `pl14` #8; `EVD-FCV: Key budget/Channel offset/Key order`);
  Archiv-Kürzel ohne Umwandler: `FNT` (ss_cmmn.dat), `PTH` (em36), `DMY`
  (em2f), `DRA` (r100). Werden wichtig, sobald diese Waffen, Gegner oder
  Menüs laufen.

### Lokale Prüfung mit Disc-Daten (`port/scripts/test-r100-core.ps1`)

Windows-PowerShell-Skript, startet in WSL2 `port/tools/whole_r100_check.sh`:
Werkzeuge einrichten (`whole_setup_linux.sh`), Quelltext nach `~/re4-kern`
klonen (Stand des Windows-Checkouts), `whole_build.sh i686` (mit `-Arm` auch
armhf), Formatvergleich mit den Disc-Dateien, dann das Spiel vom Titelbild:
`RE4_VI_FAST=1 RE4_START_ROOM=0x100 RE4_PAD_UNTIL_ROOM=1
RE4_PAD_SCRIPT="UP@100-60000/60 A@115-60000/60 START@130-60000/60"
RE4_STOP_AFTER=10000` mit Trace und Abdeckungs-Log. Die Tastenfolge hat das
Timing des Boot-Tests vom 13.10.2026: OBEN+A wählt im Speicherkarten-Dialog
„ohne Speichern fortfahren“, START überspringt Logos, A wählt „Neues Spiel“,
A/START verlässt das Debug-Startmenü. Dort verschiebt OBEN die Zeile (21
Zeilen, Zeile 1 lädt, 2 erzwingt r120, 0x12 schwer + Neustart); vor dem
nächsten A/START kommt höchstens ein OBEN an (Zeile 20 = normaler Start).
Wächter (seit 14.10.2026, nachdem der erste lokale Lauf ohne Anzeige hing):
alle 30 s Fortschritt (Bild, Spielschritt, Raum aus dem Trace); Abbruch mit
Rückverfolgung (`kill -USR1`, Ausgabe von `host/main.cpp`), wenn 180 s kein
neues Bild kommt, die Hauptschleife nach 600 s nicht erreicht ist oder nach
3600 s (Umgebungsvariablen `RE4_CHECK_STALL`, `RE4_CHECK_MAIN_LIMIT`,
`RE4_CHECK_LIMIT`; armhf 600/5400/14400 s). Bericht
`Desktop\RE4-Kern-Pruefung\bericht.txt`: Ergebnis (OK/Absturz/Zeitlimit),
erreichte Hauptschleife, Abdeckung mit allen „nein“-Zeilen, Umwandler-
Meldungen, aufgerufene Platzhalter, letzte Trace-Zeile. Disc-Daten als
Disc-Abbilder (.iso/.gcm, werden einmal nach `~/re4-kern/disc` entpackt),
Dolphin-Extraktion, Port-Ordner oder App-Import-Ordner. In der Cloud nur mit
einem künstlichen Disc-Ordner und einem nachgebauten `wsl.exe` unter
PowerShell 7 durchgespielt.

## K2 „Kern in die App“ (M6)

### Cloud-Teil (14.10.2026)

**Ablauf in der App (Variante „core“):**

1. `JNI_OnLoad` (`app/src/main/cpp/core_mode.cpp`) lädt `libre4_main.so` mit
   `dlopen(RTLD_NOW | RTLD_LOCAL)` (die Bibliotheken liegen unkomprimiert in
   der APK, `libre4_platform.so` und `libc++_shared.so` lädt der Linker mit)
   und ruft `re4_mem1_init`: MEM1 `0x80000000–0x83000000` ist reserviert,
   bevor die App GL oder Java-Threads startet. Fehlt eine Bibliothek, zeigt
   die App „core libraries missing (port/tools/whole_build_android.sh)“.
2. `CoreActivity` ruft `re4_core_reserve(<Datenordner>, "")`: Datenordner
   der App (`…/files/game` mit `disc1/`, `disc2/`), Module per bloßem Namen
   `libre4_<modul>.so` aus der APK; prüft mit `dladdr` die Lage von
   `libre4_main` (wie `re4_check_game_address`, ohne Abbruch, Meldung im
   Overlay). Danach `re4_core_start`: eigener Thread (8 MB Stack), darin
   `re4_os_run(re4_game_main)` als Boot-Thread der Thread-Emulation. Der
   Bildtakt kommt wie unter Linux aus den VI-Retraces der Plattformschicht;
   der GL-Thread wartet nie auf den Kern.
3. Pro Spielbild (in `PADRead`, Bildhaken `re4_add_frame_hook`) schreibt der
   Kern einen Schnappschuss (`re4_bridge_capture`, `runtime/core_bridge.cpp`)
   in einen Dreifachpuffer; der GL-Thread holt den neuesten mit
   `re4_core_acquire`/`re4_core_release`.
4. Der Controller geht über `re4_core_pad` an PAD-Kanal 0
   (`runtime/pad.cpp`, ersetzt das Test-Tastenskript, sobald es endet).
   Belegung (Meilenstein 7 verfeinert sie): A→A, B→B, X→X, Y→Y, L1→L, R1→Z,
   L2→L, R2→R, Start→START, Steuerkreuz/Hat→Steuerkreuz, linker/rechter
   Stick → Haupt-/C-Stick (±100), analoge Schultertasten 0–255 (ab 85 %
   zusätzlich digital L/R). SELECT blendet das Diagnose-Overlay ein/aus,
   Zurück fragt „End the game?“ und beendet den Prozess (der Kern lässt sich
   im selben Prozess nicht neu starten).

**Schnittstelle** `port/whole/runtime/re4_core.h` (Version 8, reines C, gleiche
Struktur-Größe auf 32 und 64 bit): `re4_core_reserve`, `re4_core_start`,
`re4_core_state` (idle/running/ended/failed), `re4_core_message`,
`re4_core_pad`, `re4_core_acquire`/`re4_core_release`, `re4_core_log_tail`
(letzte 24 Log-Zeilen für das Overlay), für Tests `re4_core_start_entry`.
Umgesetzt in `runtime/core_api.cpp` (in `libre4_main`).

**Schnappschuss** (`Re4CoreSnapshot`, 1 278 032 Byte auf 32 und 64 bit): Spielschritt
`Rno0..3`, Raum, Disc, Kamera aus `pG->Camera` (Augpunkt, Ziel, Rollwinkel,
Öffnungswinkel; bei offenem Untermenü die Weltkamera `CamCtrl.camera`, denn
`pG->Camera` ist dann die Kamera des Menüs), `screen` (Untermenü offen,
Ereignis, Debug-Kamera), bis 1024 Modelle aus `EmMgr` (Gegner) und `ObjMgr` (Objekte):
lebend (`be_flag & 0x201 == 1`), Zeichen-Bits `2|4`, `HIDDEN`, wenn das Spiel
das Modell in diesem Bild trotzdem nicht zeichnet (`trans.cpp`: Anzeige-
Schalter `Disp_flg` für Spieler, Partner, Gegner, Objekte, Kulisse; während
eines Ereignisses oder einer Nahaufnahme, `STA_SUSPEND`, nur Modelle mit
`be_flag & 0x800`; ganz ausgeblendete, `invisible_factor` 0), die
Zeichen-Einstellungen (Ausblenden `invisible_factor`, Tiefenmodus `z_mode`,
Flächenseite `CullMode`, Ordnungsklasse `ot_type`; je Info Mischmodus
`blend_mode`, Materialfarbe, eigener Ausblend-Anteil), Weltmatrix, je Teil die
Weltmatrix `cParts::mat` und die Bindung (−`lt_inv_mat`-Verschiebung), bis
8192 Teile. Für jede Modell-Info (`pModelInfo`; alle Infos aller Modelle in
einer gemeinsamen Liste, bis 2048, Leon allein hat neun) die Herkunft der BIN- und
TPL-Daten auf der Disc (Datei, Archiv-Offset, Eintrag) und wie das Spiel
sie zeichnet (`trans.cpp commonModelTrans`): nur mit `be_flag & 8`; ein
starres Modell (ein Teil, keine Gewichte) hängt an `pHead->partsNo` mit
seinen Punkten im Raum dieses Teils (z. B. Waffen in der Hand); dazu die eigene
Matrix der Modell-Info. Die Formatumwandler
merken sich beim Laden jedes Archiv und jeden Eintrag
(`re4_formats_locate`; bis 1024 Archive und 65 536 Einträge, vorher 64
Archive: r100 lädt mehr, ältere Herkunftsangaben gingen verloren). Liegt ein
Modell mitten in einem Eintrag (BIN in einem SMD, Modell in einer
Raumblock-Datei), meldet die Brücke den umgebenden Eintrag und `innerOffset`.
Angaben zu Archiven, über die später anderes geladen wurde, verfallen: lädt
das Spiel ein neues Archiv über einen Teil eines älteren, wird nur dieser
Teil gesperrt (über den Kopf oder denselben Bereich: das ganze ältere);
Daten ohne eigene Notiz erkennt eine Prüfsumme der Eintragsnamen; stimmt
sie nicht, ruhen die Angaben nur, bis die Daten wieder da sind (ein
Untermenü lagert Teile von MEM1 vorübergehend nach ARAM aus,
`SubScreenAramRead`). Die App merkt sich je Modell die letzte bekannte
Herkunft und zeichnet es so auch während solcher Pausen. Tauscht das Spiel
Speicher zwischen MEM1 und ARAM (`MemorySwap`, Patch 0028: Ereignisse
tauschen ihre Datei aus ARAM in den Speicher eines Gegners, Untermenüs Teile
des Heaps), wandern die Herkunftsangaben beider Seiten mit. Kopiert das Spiel
Daten aus ARAM in den Hauptspeicher (Ereignisse laden ihre Dateien nach ARAM
und holen sie beim Abspielen), folgt die Herkunft der Kopie
(`re4_formats_aram_copied`, `aram.cpp`). Ereignis-Dateien
(`Evd/*.evd`) werden als Ganzes gemerkt: Modelle darin haben `entry = -2`,
`innerOffset` = Position in der Datei. Jede Überlappung und jede Pause steht
im Log („formats: archive … overlaps …“, „… suspended“, im Bericht unter den
Umwandler-Meldungen). Yz2-gepackte Archive (Raumdateien) haben keinen
Datei-Offset (`0xFFFFFFFF`); solche Modelle (Gegenstände, Tiere des Raums)
liest die App aus der entpackten Raumdatei, die sie für den Raum ohnehin
vorbereitet. Die Raumkulisse (`cObjScr`, ObjMgr-Kennung 2: die SMD-Modelle
des Raums und seiner Blockdateien) zeichnet die App mit dem Raum; die Brücke
zählt sie nur („scenery“). Bewegt das Spiel ein Kulissenteil, sieht man das
deshalb noch nicht.

**Zeichnen** (`app/src/main/cpp/core_scene.cpp`, `CoreScene`): bis zum ersten
Raum (Schritt < 2) das Titelbild (größtes Bild der Titeldaten der App), danach
der Raum über den Raumlader der App (`RoomRepository` entpackt `r%03x.das` und
die Blockdateien, `core.das` für Himmel/Wasser) und der vorhandene
GLES3-Renderer (`DrawSceneGl`) mit der Kamera des Kerns (4:3 wie das Original,
auf den Bildschirm gestreckt wie bisher). Die Modelle des Kerns zeichnet ein
neuer Haken `SceneProbe::extraActors` mit dem `GenericEventActor` der App: BIN
und TPL liest die App selbst von der Disc (ihre geprüften big-endian Leser),
die Pose kommt aus den Teile-Matrizen des Kerns. Display-Listen und Texel aus
dem Spielspeicher werden dafür nicht gebraucht. Statusanzeige im Overlay:
„frame, step, room, disc, models (drawn, missing), room loaded“ und die
letzten Log-Zeilen.

**App-Variante** (`app/build.gradle`, `flavorDimensions 'engine'`):
`classic` = bisherige App (arm64-v8a, `com.ast.re4decomp`, Standard in
Android Studio), `core` = Kernmodus (armeabi-v7a, `com.ast.re4decomp.core`,
Name „RE4 Kern“, ab Android 6.0, CMake `-DRE4_CORE_MODE=ON`). Im Kernmodus startet „Start
game“ die `CoreActivity` statt `StartupActivity`; Raumbrowser und
Diagnosen (eigene Spielmodule `r100_scenario`, `enemy_encounter`,
`event_playback` …) sind ausgeblendet und werden nie aufgerufen (sie bleiben
im Code für `classic`). Der Disc-Import übernimmt im Kernmodus alle Dateien
(auch andere Sprachen) und schreibt zusätzlich `sys/boot.bin` (0x440 Byte)
und `sys/fst.bin` (Layout wie `runtime/dvd.cpp`); eine Disc gilt im
Kernmodus nur mit `sys/fst.bin` als importiert. `runtime/dvd.cpp` nimmt
`disc1/`, `disc2/` (App) wenn `disc0/` fehlt.

**Android-Build des Kerns** `port/tools/whole_build_android.sh` (WSL/Linux,
NDK r28c = 28.2.13676358 wie die App, `sudo port/tools/whole_setup_linux.sh
--android` lädt es nach `/opt/android-ndk-r28c`): armeabi-v7a, API 21,
`c++_shared`, `-O2`, 16-KB-Seiten. Prüft vor dem Kopieren: alle Bibliotheken
gebaut, 114 Module, dieselben Platzhalter wie der Linux-Build (Patch 0027),
die `re4_core_*`-Exporte. Kopiert nach `app/src/core/jniLibs/armeabi-v7a/`
(nicht im Repository), Debug-Abschnitte entfernt.

**Testschalter für die App:** Das Intent-Extra `coreSettings`
(`"RE4_X=wert;…"`) setzt Umgebungsvariablen des Kerns vor dem Start
(`RE4_START_ROOM`, `RE4_PAD_SCRIPT`, `RE4_PAD_UNTIL_ROOM` …;
`state_trace.cpp` liest seine Schalter deshalb beim Kernstart neu).

**Prüfungen in der Cloud (14.10.2026):**

| Prüfung | Ergebnis |
|---|---|
| `port/tools/whole_build.sh` (i686, armhf unter qemu) inkl. `--core-test` | grün |
| `RE4_OPT=2 port/tools/whole_build.sh` (wie Android) | grün, 209 Platzhalter wie `-O0` |
| `core_scene_gl_check` (App-Renderer, EGL ohne Bildschirm, Mesa llvmpipe; synthetische Disc aus `tests/format_compare/samples.h`) | 31 Prüfungen: Pfade ohne Groß/Klein, Yz2-Quelle abgelehnt, Titel ohne Daten schwarz, Raum gebaut, Modell gezeichnet/versteckt, Teile-Matrizen bewegen das Modell |
| `re4decomp_link_check` (die native App-Bibliothek der Variante core auf dem Host mit `--no-undefined`) | linkt |
| Java der App (alle Klassen) gegen Android-API 34 mit `javac` | übersetzt; JNI-Namen (8) passen zu `core_mode.cpp` |
| App-Hosttests 32 bit (i686, `-msse2 -mfpmath=sse`) | wie 64 bit: nur die 2 schon vorher fehlschlagenden Tests |

**Nicht in der Cloud geprüft** (kein Android-NDK/SDK erreichbar,
`dl.google.com` gesperrt): NDK-Build des Kerns, Gradle-/APK-Build,
Gerätelauf, Bilder mit echten Disc-Daten.

### Erster lokaler Lauf (14.10.2026, WSL2 + Windows)

- `test-r100-core.ps1`: ALLES OK (Formatvergleich 155 Dateien ohne Fehler,
  10 000 Bilder ohne Absturz, Abdeckung ohne „nein“, 5 Bilder gezeichnet).
  Befunde zur Brücke: Kamera die ganze Zeit `(0, 0, 2000) → (0, 0, 0)`,
  Winkel 50 (nicht die Spielkamera); 128 Figuren = Obergrenze erreicht;
  112 von 126 sichtbaren Figuren ohne Disc-Herkunft (die Herkunftsliste hielt
  nur 64 Archive). App-Bilder: „drawn 16, missing 120“.
- `test-core-app.ps1`: NDK r28c geladen, Kern für armeabi-v7a gebaut
  (0 Fehler, 114 Module, 209 Platzhalter wie Linux, 117 Bibliotheken,
  21 MB). APK-Bau scheiterte: `JAVA_HOME` zeigte auf Java 11, Gradle braucht
  17–23.
- Behoben danach (Cloud): Das Skript sucht ein passendes JDK (JAVA_HOME,
  Android Studio `jbr`, JDKs unter `C:\Program Files`, `-JavaHome`) und setzt
  es nur für den Lauf; Ausgabe ohne „RemoteException“-Zeilen und ohne
  Treppeneffekt. Herkunftsliste 1024 Archive (Test `core-test bridge` mit
  101 Archiven), Obergrenze 256 Figuren (API-Version 2). Der Zustands-Trace
  schreibt jetzt pro Bild die Kamera mit ihren Schaltern (`cam`: Auge, Ziel,
  Winkel, `sw` = C STA_CAMERA, D Debug-Kamera, S SPF_CAMERA, E Ereignis,
  U Untermenü, X Sonderkamera, `r0` Zustand von CamCtrl, `ctrl` Auge von
  `CamCtrl.camera`); der Bericht zeigt den Kamera-Verlauf. Unbekannte
  Modell-Herkünfte stehen im Brücken-Protokoll mit ihrem Ort
  (`re4_formats_describe`: in welchem Archiv, oder in keinem).
- Ursache der Kamera noch offen: `CameraMove` übernimmt `CamCtrl.camera` nur
  mit STA_CAMERA und ohne Debug-Kamera (DBG_DBG_CAM, schaltet sich bei
  Tasten an Controller 2 ein); der nächste Lauf zeigt, welcher Fall vorliegt.

### Zweiter lokaler Lauf (14.10.2026, WSL2)

- Kamera-Verlauf: bis Bild 621 Titel/Laden, ab 622 Ereignis mit
  Ereigniskamera (Winkel 29), ab 1800 Kamera hinter Leon (`CamCtrl`, Zustand
  17), ab Bild 1871 ein Untermenü (`U`, vermutlich der Funkspruch, der auf
  eine Taste wartet; ohne Eingabe bleibt er offen): `pG->Camera` ist dann die
  Menükamera `(0, 0, 2000) → (0, 0, 0)`. Behoben: Die Brücke nimmt dann die
  Weltkamera und meldet das Menü (App-Overlay „MENU OPEN“).
- Brücke: 226 Figuren (Obergrenze 256 reicht), 15 mit Herkunft; die übrigen
  lagen mitten in Einträgen (Raumarchiv, Blockdateien) oder in Speicher, über
  den ältere Archive geladen worden waren (title.dat, save_e.dat: veraltete
  Angaben). Behoben wie oben (Version 3).
- Formatvergleich: 8 Dateien mit „FEHLER“ (FCV-Animationen in wep09, 10, 21,
  24, 31, 32, 47 und pl14). Kein neuer Fehler: Die größere Notiztabelle macht
  sichtbar, dass diese Einträge schon immer unvollständig umgewandelt wurden
  (vorher als „nicht vergleichbar“ durchgerutscht; die App-Leser lehnen sie
  auch ab). Für r100 ohne Bedeutung (Waffen, die dort nicht vorkommen). Der
  Umwandler meldet jetzt Gelenk, Kennung, Schlüsselformat und Grenze des
  Eintrags; die Ursache folgt aus dem nächsten Lauf.

### Dritter lokaler Lauf (14.10.2026, WSL2)

- Kamera bei offenem Untermenü jetzt die Weltkamera; 188 Kulissenteile
  erkannt; im ersten Bild zeichnet die App 43 Figuren (vorher 17).
- Nach dem Öffnen des Untermenüs fehlte die Herkunft der Modelle aus dem
  entpackten Raumarchiv (Gegner 0x41/0x43/0x46, Gegenstände 0x19) und von
  em12.drs („replaced archive“): Ein später geladenes Archiv überlappte sie,
  und die Regel verwarf das ganze ältere Archiv. Jetzt wird nur der
  überdeckte Teil gesperrt (Test `core-test bridge`), und das Log nennt jede
  Überlappung. Der Bericht wertet die Brücke jetzt zweimal aus (früh im Raum
  und am Ende).
- FCV: Die Diagnose-Zeile fehlte im Bericht (Filter). Der Umwandler prüft
  jetzt jedes Gelenk vor dem Umwandeln: Gelenke, deren Daten außerhalb des
  Datenbereichs liegen, bleiben unverändert; ein Fehler nur, wenn
  `MotionMoveCore` diese Gelenkart liest (Kamerafahrten lesen nach Nummer,
  daher werden alle passenden Gelenke weiter umgewandelt). Test mit einem
  unbenutzten Gelenk, das in den Kopf zeigt.

### Vierter lokaler Lauf mit Bildern (14.10.2026, WSL2)

- Erste Bilder der Brücke mit Disc-Daten: Raum r100 mit Brücke, Bäumen,
  Himmel und Nebel wie im Spiel; Leon mit Funkgerät am Ohr (Bilder 2500 bis
  10 000: der Funkspruch wartet auf eine Taste, das Funk-Menü selbst ist 2D
  und fehlt); Bild 651: Anfangsszene, zwei Polizisten auf der Brücke von
  hinten. Fehler: Leons Haare/Oberkopf fehlen, den Polizisten fehlen die
  Köpfe (nur die Mützen).
- Ursachen aus dem Bericht: Die Köpfe kommen aus den Ereignis-Dateien
  (`Evd/r100s00.evd`), die der Umwandler nicht als Herkunft merkte (jetzt
  `entry -2`). Nach dem Öffnen des Funk-Menüs meldeten r100.das und em12.drs
  „entry names changed“: Das Spiel lagert diesen Speicher vorübergehend aus;
  die Angaben wurden verworfen statt nur pausiert (jetzt Pause plus
  Gedächtnis der App). Leons Haare: Ursache offen; der Bericht zeigt jetzt im
  Abschnitt 4, welche Modelle die App nicht laden konnte und warum.
- FCV: Die 8 Fehler sind Kamera-Gelenke (Kanal 6) in Waffen-Animationen,
  deren Daten hinter ihrem Eintrag liegen (wep09/10/21/24/31/32/47), und ein
  32-Byte-FCV in pl14.drs, das mitten in seiner einzigen Kurve endet. Beides
  liegt an den Original-Daten, nicht am Umwandler: Kamera-Gelenke liest das
  Spiel nur mit angehängter Kamera, und was in den Eintrag passt, wird
  umgewandelt. Die Meldung nennt jetzt den Grund, gezählt als Fehler wird nur
  ein gelesenes Gelenk mit Daten im Eintrag.

### Fünfter lokaler Lauf (14.10.2026, WSL2)

- Während des Funkspruchs zeichnet die App jetzt 42 Figuren ohne Lücke
  (vorher 16 und 26 fehlende); alle 27 Modelle lassen sich laden. Formatvergleich:
  nur noch pl14 (FCV mit 32 Byte, keine Achse passt) – jetzt als Originaldaten
  eingeordnet.
- Leons Haare: kein Ladefehler, sondern die Regel aus `commonModelTrans`
  fehlte: Die Haare sind ein starres Teil am Kopf-Knochen, die App hängte sie
  an Knochen 0 (Hüfte). Jetzt mit Sichtbarkeit, Knochen und Info-Matrix
  (Version 4; Test in `core_scene_gl_check`, 52 Prüfungen).
- Köpfe der Polizisten: Ereignis-Modelle lagen „in em12.drs“ – die Ereignis-
  Datei kommt aus ARAM und überschreibt dabei vorübergehend em12. Jetzt folgt
  die Herkunft der ARAM-Kopie (Test `core-test bridge`).

### Sechster lokaler Lauf (14.10.2026, WSL2)

- PC-Prüfung **ALLES OK** (Formatvergleich 0 Fehler). Bilder unverändert.
- Leons vier Modellteile sind alle gewichtet (keins starr), sichtbar und
  geladen: Die Haare fehlen aus einem anderen Grund. Die Bild-Auswertung
  schreibt jetzt für jedes gezeichnete Teil die Lage seiner Punkte im Raum
  und die Zahl der Materialien mit Alpha-Test in den Bericht.
- Köpfe der Polizisten: Die Ereignis-Datei kommt nicht per Kopie, sondern
  per `MemorySwap` (Tausch MEM1/ARAM über Zwischenpuffer) in den Speicher von
  em12. Patch 0028 meldet den Tausch, die Herkunftsangaben wandern mit
  (Test `core-test bridge`).

### Siebter lokaler Lauf und erster Gerätelauf (14.10.2026)

- PC-Prüfung **ALLES OK**. Bild 651 (Anfangsszene im Polizeiauto): die
  Polizisten jetzt mit Köpfen (Patch 0028 wirkt).
- Leons Haare: Die Brücke gab je Figur nur die ersten **4** Modell-Infos
  weiter. Leon hat neun (`cPlLeon::setModel`: Körper, Kostüm, Messer,
  Gesicht, Kopf, Haare, dazu Gesicht/Hände aus `setFace`/`setRightHand`/
  `setLeftHand`); Kopf, Haare und Hände fielen weg. Jetzt liegen alle Infos
  aller Modelle in einer gemeinsamen Liste (`Re4CoreSnapshot::infos`, bis
  1024, Zähler `droppedInfos`; Version 5). Tests: `core-test bridge` mit einer
  Figur aus neun Infos, `core_scene_gl_check` (53 Prüfungen: alle Infos von
  zwei Modellen gezeichnet).
- Gerät (Retroid, App „RE4 Kern“): APK gebaut (Java 21 aus Android Studio),
  installiert, Kern geladen, MEM1 reserviert. Das Spiel startete nicht, weil
  Disc 1 in der neuen App noch nicht importiert war; der Import, den der
  Projektinhaber während des Laufs begann, wurde abgebrochen, weil das
  Skript die App nach 120 s beendet. `test-core-app.ps1` wartet jetzt auf
  den fertigen Import (Fortschritt in MB, höchstens `-ImportMinutes`,
  Standard 45) und beendet die App währenddessen nicht.

### Erster Gerätelauf mit Spiel (14.10.2026, Retroid)

- Gerät: Import von Disc 1 mit dem neuen Warten im Skript geklappt. Der Kern
  läuft auf dem Retroid: Titelbild (r120), Debug-Start r100, Anfangsszene im
  Polizeiauto mit Polizisten und Leon (Köpfe, Haare), 66 Figuren gezeichnet.
  PC-Prüfung **ALLES OK**, Leon mit Haaren (alle neun Modell-Infos).
- **Absturz** nach dem Funkspruch (Projektinhaber drückte START):
  `seRandomCheck` ← `emSeCheck` ← `SndCall` ← `emMove` (SIGSEGV bei
  0x10395040). Die Zufalls-Tabelle der Geräusche in jeder Tonbank (Kopfwort 1:
  Versatz der Tabelle, darin Versätze der Listen `{num, last, e[num]}`) blieb
  big-endian; außerdem lief die letzte Wavetable-Tabelle bis zum Blockende und
  hätte die Zufalls-Tabelle als 16-bit-Werte gedreht. Jetzt eigener Umwandler
  (nur die Gruppen, die die SITs des Blocks nennen; das Spiel liest keine
  anderen) und die Wavetable endet vor der Tabelle. Test `core-test sound`
  (schlägt mit dem alten Umwandler fehl). Mit echten Daten noch nicht geprüft.
- Warum die PC-Prüfung das nicht fand: Ihre Tastenfolge endet beim Laden des
  Raums, das Spiel wartet dann bis zum Ende im Funkspruch, Gegner laufen nicht.
  Neu: `RE4_PAD_ROOM_SCRIPT` (Bilder ab dem Raum gezählt) und Steuerknüppel
  in der Tastenfolge (`STICKUP` …); `whole_r100_check.sh` drückt 1500 Bilder
  nach dem Raum START (Funkspruch zu) und lässt Leon 10 s laufen. Test
  `runtime-test trace`.
- Offen: „Das Auto sieht man nur teilweise“ (Bild der Stelle fehlt noch);
  eine Figur ohne Herkunft ist eine Waffen-Figur (`cEmWep`, 0x42), nicht das
  Auto. Kein Ton: Meilenstein 8. `test-core-app.ps1` meldet jetzt einen
  Neustart der App (anderer Prozess) als „NEU GESTARTET … abgestürzt?“.

### Zweiter Gerätelauf (14.10.2026, Retroid) und Analyse

Gemeldet: Auto fehlt um Polizisten und Leon, Münder bewegen sich nicht, kein
Ton, der erste Funkspruch endet erst mit START, danach Kamera am Boden mit
Blick nach oben, Absturz beim Erreichen des Hauses. Ursachen und Korrekturen:

- **Kamera am Boden:** Ein Schulterkamera-Schnitt (CAM, `Id 8`,
  `CameraQuasiFPS::setAreaData`) liest immer 24 Kamerapositionen (nahe und
  ferne Punkte) und je 12 Ziele, Rollwinkel und Öffnungswinkel, unabhängig von
  `nPoint`. Der Umwandler drehte nur `nPoint` Werte: Die fernen Punkte blieben
  big-endian, fast 0, also die Kamera an Leons Füßen. Jetzt dreht er, was das
  Spiel liest, jedes Wort einmal (Test `core-test camera`, schlägt mit dem
  alten Umwandler genau bei den 12 fernen Punkten fehl).
- **Auto:** Das Polizeiauto ist Kulisse aus dem Raumskript (`SetObjSmd`,
  Einträge 0x1D/0x1E, 0x1F/0x20), nicht aus dem SMD. Die App übersprang alle
  Kulissen-Objekte. Patch 0029 + Brücke Version 6 (SMD-Eintrag je Objekt), die
  App folgt dem Kern: versteckte, nicht geladene und bewegte SMD-Kulisse, alles
  andere wie die Figuren (Tests `core-test bridge`, `core_scene_gl_check`).
  Dabei auch: r100 versteckt die SMD-Objekte 0x37/0x38 beim Start, die App
  zeichnete sie bisher trotzdem.
- **Münder / Blinzeln:** Formanimationen (`shape.cpp`) verformen die
  Gesichtspunkte im Speicher des Kerns; die App zeichnete die unverformten
  Disc-Daten. Brücke Version 7: Für Infos mit laufender Form rechnet die
  Brücke mit dem Originalcode (`ResetShape`, `CalculateShape_new`) und gibt
  die Punkte weiter; die App lädt das Gesicht dann neu hoch (Tests
  `core-test bridge` „shape“, `core_scene_gl_check`).
- **Funkspruch / Stimmen:** Der Tontreiber läuft im AX-Audiorahmen-Rückruf
  (5 ms); mit AX als Platzhalter lief er nie (Stimmen blieben belegt: „STR
  SND_VOICE L is not available“, Sprach-Stream nie bereit, `ss_term.cpp
  OpeMesMove` wartet). Jetzt `runtime/ax.cpp` ohne Tonausgabe (Plattformtabelle;
  Test `core-test ax`). Hörbarer Ton bleibt M8.
- **Absturz am Haus:** Kein Protokoll (nach Ende des 120-s-Laufs gespielt).
  Die PC-Prüfung stellt Leon jetzt nach dem Funkspruch vor das erste Haus
  (Testhilfe `RE4_TEST_PLACE`, die Stelle, an die `r100_Sce_look` Leon nach
  s03 setzt) und lässt ihn hineinlaufen; sie listet außerdem alle
  Auslöse-Bereiche des Raums (AEV). Ob s03 damit ausgelöst wird, zeigt der
  nächste Lauf; sonst Gerätelauf mit `-Seconds 600` bis zum Haus.
- App-Bildtest: Die Raum-Animationen (Himmel, Wasser, 30 Hz) liefen zwischen
  den Testbildern weiter; frühere Pixelvergleiche passten nur zufällig. Der
  Test zeichnet jetzt ohne Zeitfortschritt; die Prüfungen für starre Teile
  bewegen sichtbar (nach oben).

### Dritter PC-Lauf (14.10.2026, nach der Kamera-Korrektur)

- **Android-Kern baute nicht:** `formats_event.cpp` nutzte `std::vector`; die
  Datei wird mit den Spiel-Schaltern und der Prelude übersetzt, und die
  libc++ des NDK verträgt sie nicht (`__libcpp_allocate` …), libstdc++ der
  Linux-Builds schon. Jetzt einfacher C-Speicher. Neu im Cloud-Build:
  `whole_libcxx_check.py` übersetzt alle Laufzeitdateien zusätzlich gegen
  libc++ (fand den Fehler nachträglich, 150 Dateien ok); `whole_setup_linux.sh`
  installiert `libc++-dev`.
- **PC-Prüfung: Absturz in `CameraQuasiFPS::setAreaData` beim Raumaufbau:**
  Der Kamera-Umwandler brach bei r100 ab („camera keys at 0x818c3400 …
  conversion stopped“): ein Schnitt hat in `pFrame` einen veralteten Zeiger
  des Kamera-Werkzeugs (derselbe Wert steht in einem unbenutzten `pVer` mit
  0 Punkten; der alte Umwandler übersprang ihn darum zufällig über die
  gemeinsame „schon gesehen“-Liste). Bildzeiten liest das Spiel nur bei
  Kamerafahrten (Id 6/7, `HermiteExport`); jetzt nur dort umgewandelt, und ein
  Feld außerhalb des Blocks wird gemeldet und übersprungen statt die ganze
  Umwandlung zu stoppen. Test `core-test camera` nachgebaut wie r100 (fällt
  mit dem vorigen Umwandler mit genau der Meldung aus dem Lauf durch).

### Dritter Gerätelauf (14.10.2026, Retroid) und Analyse

Gemeldet: Nach dem Intro steht Leon mit richtiger Kamera da, aber keine Taste
wirkt, die Vögel stehen still; im Intro steht Leon kurz mit abgespreizten
Armen außerhalb des Autos; Dach, Seitenwände und Boden des Autos fehlen; ein
gelber Fleck im Intro. Lauf 600 s ohne Absturz, die Tasten kamen laut Log im
Kern an (`PADRead`), das Overlay zeigte durchgehend „event“.

- **Keine Steuerung, Vögel still:** Das Spiel hing im Funkspruch nach dem
  Intro (`sscrn.cpp OpeSetOpenTerm` → `event.cpp SndStrPlayBlock`: wartet,
  bis der Sprach-Stream bereit ist), also innerhalb von `SceEventStart`: Tasten
  gesperrt, alle Gegner und Tiere im Ereignis-Modus. Seit `ax.cpp` bekommt der
  Stream eine Stimme (vorher keine: der Funkspruch lief ohne Stream weiter,
  START schloss ihn). Der Stream-Code des Tontreibers (`snd_str2.cpp`) startet
  im Audio-Rahmen (einem Interrupt) einen DVD-Lese- oder ARAM-Kopierauftrag
  und setzt erst **danach** sein Kennzeichen `dvd_busy`/`dma_busy`; der
  Rückruf löscht es. Die Plattformschicht rief den Rückruf schon während des
  Auftrags, das Kennzeichen blieb für immer gesetzt, der Stream wurde nie
  bereit. Jetzt bleibt ein Auftrag aus einem Interrupt heraus „beschäftigt“,
  bis der Interrupt-Rückruf zurückgekehrt ist (wie auf der GameCube, deren
  Laufwerk Millisekunden braucht): `os_thread.cpp re4_os_defer` /
  `re4_os_run_deferred` (nach jedem Audio-Rahmen und am Ende des Interrupts),
  `dvd.cpp`, `aram.cpp`. Außerhalb von Interrupts bleibt alles sofort (die
  Lade-Warteschlangen des Spiels warten in Schleifen ohne Unterbrechung
  darauf). Tests: `runtime-test dvd` (Lesen im Interrupt, Abbruch → −3) und
  neu `runtime-test stream`: der Stream-Code des Spiels spielt zwei Dateien
  einer synthetischen Disc (eine passt in den ARAM-Ring, eine läuft
  11 Blöcke durch den Ring) bis zum Ende, dazu Stopp beim Spielen und beim
  Puffern (`tests/stream_probe.cpp`); mit der alten Plattformschicht schlägt
  er fehl. Der Funkspruch endet damit von selbst (er läuft nach Bildern, nicht
  nach Tonende, `ss_term.cpp OpeMesMove`).
- **Leon außerhalb des Autos (Arme abgespreizt):** Das war die Spielfigur
  Leon, die das Spiel während des Intros nicht zeichnet; das Intro zeigt
  seine eigene Leon-Figur im Auto. Die Brücke kannte die Sichtbarkeitsregeln
  von `trans.cpp` nicht: Anzeige-Schalter (`DPF_PL` …) und vor allem
  „während eines Ereignisses nur Modelle mit `be_flag & 0x800`“. Dadurch
  zeichnete die App im Intro zusätzlich Leon, das geparkte Polizeiauto des
  Raums (`R100Init`: `setNoSuspend(0)`, darin die Teile 11/13/15 auf 0
  skaliert) und die beiden Polizisten des Raums. Jetzt Kennzeichen `HIDDEN`
  (Version 8), die App hält sich daran. Tests: `core-test bridge` (Ereignis
  hält die Welt an, `setNoSuspend`, `DPF_PL`, `DPF_EM`, ganz ausgeblendet;
  schlägt ohne die Regel fehl), `core_scene_gl_check` (verstecktes Modell
  fehlt im Bild).
- **Zeichen-Einstellungen des Spiels:** Glas, Lichtschein und Strahlen
  zeichnet das Spiel mit eigenem Mischmodus (`blend_mode`: addieren …), ohne
  Tiefenschreiben (`z_mode`) oder ausgeblendet (`invisible_factor`); die App
  zeichnete alles undurchsichtig. Jetzt übernimmt sie Mischmodus, Tiefenmodus,
  Flächenseite (`CullMode`) und Ausblenden und zeichnet Durchscheinendes nach
  dem Undurchsichtigen (Test `core_scene_gl_check`, 83 Prüfungen).
- **Auto (Dach, Seiten, Boden):** Im Intro ist das Auto ein eigenes
  Ereignis-Modell `obm1a00` (24 Teile) plus sechs Scheiben-Modelle
  `obm3000a–f` (`r120.cpp EventCarInit`: Scheiben mit `z_mode` 1; im Auto
  versteckt das Spiel selbst die Teile 0xF und 0x12–0x17). Ursache noch nicht
  belegt; mit den beiden Korrekturen oben liegt das geparkte Auto nicht mehr
  im Intro, und die Scheiben werden durchscheinend gezeichnet. Diagnose für
  den nächsten PC-Lauf: Schnappschüsse während der ganzen Autofahrt (30 …
  750 Bilder nach dem Raumstart) und je Modell-Info im Bericht, wie viele
  Dreiecke auf vom Spiel auf 0 skalierten Teilen liegen, wie viele der
  Alpha-Test entfernt (Punkt-Alpha, fast durchsichtiges Alphabild), dazu
  Mischmodus, Tiefe, Seite, Ausblenden.
- **Gelber Fleck:** noch nicht belegt. Vermutung: ein Lichtschein- oder
  Glas-Modell, das die App bisher undurchsichtig zeichnete (jetzt gemischt
  wie im Spiel), oder ein Material mit Spiegel-/Wasser-Textur des Spiels
  (`setTexRender`, Rückspiegel `EvtTexRenderCamTrans`), das die App mit
  seiner Ersatz-Textur zeichnet. Der nächste Lauf zeigt, ob er bleibt.
- **PC-Prüfung:** drückt im Raum kein START mehr. Sie wartet, bis der Spieler
  frei ist (kein Ereignis, kein Untermenü seit 300 Bildern: „run: player
  free“), stellt Leon dann vor das erste Haus (`RE4_TEST_PLACE` mit
  `free+`) und lässt ihn 5 s laufen (`RE4_PAD_FREE_SCRIPT`). Der Bericht nennt
  „Spieler frei“ (sonst FEHLER: Funkspruch endet nicht) und „Steuerung“ (wie
  weit Leon lief; unter 500 Einheiten FEHLER). Das Tastenprotokoll nennt
  jetzt jede Änderung, auch vom Gerät, einmal.

### Rückfragen nach dem dritten Gerätelauf (14.10.2026)

- **Gelber Fleck = ovales Drahtgitter:** das Kollisions-Hilfsbild (Kapsel und
  Bodenfleck) der Bewegungsprüfung der früheren App (`scene_gl.cpp`,
  `SceneProbe::showBody`), im Kernmodus nie abgeschaltet und am Weltursprung
  gezeichnet. Jetzt aus (`core_scene.cpp`). Test `core_scene_gl_check`: Bild
  auf den Ursprung, vorher 163 Punkte in der Farbe der Kapsel, jetzt 0. Der
  Kulissen-Test sah den Boden bisher nur über diese Kapsel; er blickt jetzt
  von der Seite auf den Boden (verstecken: 32 229 Punkte ändern sich).
- **Vollständiges Intro:** Der Gerätetest startete im Debug-Menü direkt r100
  (nur die kurze Ankunft s40). Ein neues Spiel beginnt in r120
  (`r120.cpp R120Event`): Vorfilm `movie/opening.sfd`, dann die Autofahrt
  (Ereignisse r120s00/s01), dann Sprung nach r100. `test-core-app.ps1` startet
  jetzt standardmäßig r120 (`-StartRoom`); `whole_r100_check.sh --room 120`
  ebenso (mit `--frames 30000`).
- **Filme bis M9:** `runtime/movie.cpp` statt der erzeugten Platzhalter. Diese
  hätten r120 und die Demo-Filme des Titels abstürzen lassen: ohne Handle
  ruft `cSofdec::ThreadMove` trotzdem `mwPlyGetStat` über dessen
  Funktionstabelle, und `mwPlyGetCurFrm` ließ den Bildsatz ungeschrieben.
  Jetzt endet jeder Film sofort (Status PLAYEND, „movie: … skipped“ im Log),
  das Spiel geht danach weiter. Test `core-test movie`. Platzhalter: 173.
- **Ton:** `ax.cpp` lässt den Tontreiber laufen (Stimmen werden frei, Streams
  bereit, Funkspruch läuft), erzeugt aber noch keine Töne. Hörbarer Ton ist
  Meilenstein 8: Stimmen dekodieren und mischen (DSP-ADPCM, Abtastrate,
  Lautstärke), MIX/SYN/SEQ/AXART/AXFX aus `src/lib` mitkompilieren (Quellen
  liegen vor), Ausgabe über `GameAudio.java` (AudioTrack) der App.
- **Logos, Titelmenü, Untermenüs, Anzeigen:** Das Spiel zeichnet sie mit GX
  (2D); GX ist noch Platzhalter, die App zeigt bis zum ersten Raum nur das
  Titelbild. Entscheidung (15.10.2026): GX nachbilden (`gx-plan.md`, G1
  unten). Ton bleibt in der geplanten Reihenfolge (M7 Steuerung, M8 Ton, M9
  Filme).

### GX nachbilden, Schritt G1: FIFO einsammeln (15.10.2026, Cloud)

Entscheidung des Projektinhabers (15.10.2026): GX nachbilden statt die
Bildschirme in der App nachzubauen (Bauplan `port/docs/gx-plan.md`, Regel in
`CLAUDE.md`). G1 ist der erste Schritt: Alles, was das Spiel an den
Grafikprozessor schickt, kommt jetzt im Port an.

- **GX-Bibliothek des SDK im Build** (`RE4_SDK_UNITS` in
  `port/whole/CMakeLists.txt`: `GXAttr GXBump GXDisplayList GXDraw GXFifo
  GXFrameBuf GXGeometry GXInit GXLight GXMisc GXPerf GXPixel GXTev GXTexture
  GXTransform`), unverändert aus `src/lib` bis auf Patch 0030
  (Assembler-Stellen in C). Die rund 97 GX-Platzhalter entfallen; die
  Registerwerte stammen damit aus dem Original-SDK.
- **FIFO:** Jeder Schreibzugriff auf `GXWGFifo` (SDK `GXVert.h`, `__gx.h`,
  das Spiel in `gx.h`) wird beim Bauen zu `re4_gx_put_*` (neue Regel in
  `whole_prepare.py`). `runtime/gx_fifo.cpp` sammelt die Bytes big-endian wie
  die Hardware, zerlegt den Strom in Befehle (CP, XF, indiziertes XF, BP,
  Display-Listen-Aufruf, Zeichenbefehle mit der Punktgröße aus
  Punktbeschreibung und -format) und zählt sie; Unbekanntes wird gezählt
  (Zeichen für einen Strom, der seine Rahmung verloren hat). Display-Listen:
  Während `GXBeginDisplayList` schreibt der FIFO in den Speicher der Liste
  (PI-Register wie auf der Hardware, Überlauf-Bit), `GXCallDisplayList` liest
  sie über die physische Adresse (`0x80000000 + physisch`).
- **Register-Blöcke** CP, PE, PI, MEM sind Speicher des Ports
  (`re4_gx_register_block`). BP-Schreiben mit Maske (`0xFE`) werden
  gespeichert; `PE_DONE` (`0x45`) und Token (`0x47/0x48`) lösen die
  PE-Unterbrechungen 0x12 (Token) und 0x13 (fertig) aus, zugestellt beim
  nächsten `OSSleepThread` und bei jedem VI-Retrace über die
  Unterbrechungs-Rückrufe des SDK (`__OSSetInterruptHandler`, neu in
  `dolphin_os.cpp`). Damit kehrt `GXDrawDone` zurück und der
  Zeichen-Rückruf des Spiels bekommt sein Token.
- **EFB lesen** (`GXPeekARGB/Z`): antwortet „nichts gezeichnet“ (Farbe 0,
  Tiefe ganz hinten); echtes Lesen ist G5.
- Noch wird nichts dekodiert oder gezeichnet: Die App zeigt weiter die
  Zustandsbrücke. Nächster Schritt G2: Befehlsdecoder (Zustand aus
  CP/XF/BP, Punkte in Zeichenaufträge).

Test `re4_host --runtime-test gx` (`tests/gx_probe.cpp`, mit den Schaltern
und Headern des Spiels gebaut): `GXInit` schreibt alle Register (mehr als 30
BP, XF, nichts Unbekanntes); `GXSetBlendMode` ergibt das erwartete BP-Wort
`0x41`; ein Dreieck aus Float-Punkten und ein Viereck aus s16-Punkten mit
RGBA8-Farbe und Float-Texturkoordinaten ergeben 2 Zeichenbefehle mit 7
Punkten; eine Display-Liste wird in den Speicher geschrieben (Größe Vielfaches
von 32, enthält den Zeichenbefehl) und beim Aufruf genau einmal gezeichnet;
das Token `0xADEB` erreicht den Rückruf, `GXDrawDone` kehrt zurück.
Ergebnis: alle Laufzeittests grün in `whole_build.sh` (i686, armhf unter qemu,
beide auch mit `-O2`), libc++-Prüfung 152/152. Platzhalter: 76 Funktionen
(vorher 173), 0 Daten (vorher `GXWGFifo`). Nicht getestet: mit Disc-Daten
(lokal) und auf dem Gerät – das Spiel nutzt jetzt die echte GX-Bibliothek,
`test-r100-core.ps1` und `test-core-app.ps1` sollten zeigen, dass es so weit
läuft wie vorher.

### GX nachbilden, Schritt G2: Befehlsdecoder (15.10.2026, Cloud)

`runtime/gx_decode.cpp` arbeitet den Befehlsstrom sofort ab, sobald ein
Befehl vollständig ist (der Grafikprozessor liest die Daten also, solange das
Spiel sie noch so hat wie beim Senden). Er führt den Zustand von CP, XF
(Matrizen, Licht, Register), BP und dem Paletten-Teil des TMEM und schreibt,
solange ein Renderer Bildstücke abholt, die entschlüsselte Arbeit in
**Bildstücke** (`runtime/re4_gx_frame.h`, eigenes Format für die App):

- ein Stück endet mit jeder Bildkopie in den Bildspeicher (`GXCopyDisp`), also
  einmal je Spielbild; es enthält die geänderten BP-/XF-Register,
  Paletten-Ladungen, Zeichenaufträge (Punkte als Float/RGBA8 mit
  Matrix-Indizes, Dreieckslisten statt Vierecken/Streifen/Fächern) und
  Textur-Bindungen;
- Texturen werden über einen Inhalts-Schlüssel (Prüfsumme der Texel, bei
  Paletten-Texturen mit der Palette) nur einmal übertragen; was lange nicht
  gebraucht wurde, wird vergessen (`FORGET`);
- Kopien des Bildpuffers in eine Textur (`GXCopyTex`) hinterlassen eine Marke
  am Ziel; eine Textur dort ist dann diese Kopie (der Renderer hat die Pixel);
- Warteschlange mit 4 Plätzen: ist sie voll, wartet der Kern auf den Renderer
  (ein langsamer Renderer bremst das Spiel wie ein langsamer Grafikchip); holt
  er eine Sekunde lang nichts ab, wird verworfen und mit vollem Zustand neu
  begonnen. Ohne Renderer (Linux-Prüfungen, bisherige Brücke) wird nichts
  aufgezeichnet, nur der Zustand geführt.

**Byte-Reihenfolge** (Bestandsaufnahme aller Stellen, an denen das Spiel dem
Grafikprozessor Speicher übergibt): Befehlsströme (FIFO, aufgenommene und
Disc-Display-Listen) sind big-endian; Punktfelder und per Index geladene
Matrizen sind in Host-Reihenfolge (Formatumwandler, CPU-Puffer wie die
Skinning-Ausgabe); Texel und Paletten bleiben big-endian. Vier Display-Listen
baut das Spiel selbst mit CPU-Schreibzugriffen in Host-Reihenfolge (Sprite-Viereck
`eff_sys.cpp`, Wasser-/Sandgitter `Espgen42/43/45.cpp`): Patch 0032 meldet sie
an, der Decoder liest sie dann in Host-Reihenfolge. Zwei Felder liegen außerhalb
von MEM1 (`g_EspCommonDisplayList`, Palette `CLUTdata` in `filter02.cpp`):
Display-Listen-Aufrufe tragen den Zeiger selbst, für Felder, Texturen und
Paletten gibt Patch 0031 dem Grafikprozessor über `re4_gx_physical` ein
Adressfenster oberhalb von MEM1 (`0x03000000`).

Test `runtime-test gx` (zusätzlich zu G1, `tests/gx_probe.cpp
re4probe_gx_frame_test`): ein künstliches Bild über die SDK-Bibliothek –
Matrix direkt und per Index, texturiertes Viereck mit s16-Positionen (Index,
Host-Reihenfolge, Nachkommabits), RGBA8-Farben (16-bit-Index) und
Float-Texturkoordinaten, Dreiecksstreifen, CPU-gebaute Display-Liste in
Host-Reihenfolge, Palette und C4-Textur, Palette außerhalb von MEM1,
Bildpuffer-Kopie als Textur, Bildkopie; erwartet werden genau diese Punkte,
Indizes, Texel, Paletten und Zustände, beim zweiten Bild dieselbe Textur ohne
erneute Daten, ohne Renderer kein Stück. Ergebnis: grün in `whole_build.sh`
(i686, armhf, beide auch `-O2`), libc++ 153/153. Nicht getestet: mit
Disc-Daten (lokal) – das kommt mit G4.

### GX nachbilden, Schritte G3 und G4: Renderer in der App (15.10.2026, Cloud)

**G3 – GLES3-Renderer** (`port/android-decomp/app/src/main/cpp/gx_gl.cpp`,
`gx_gl_shader.cpp`, `gx_texture.cpp`): führt die Bildstücke des Kerns aus.

- eigener Bildpuffer (EFB) 640×528 mal Faktor (Farbe und Tiefe), GameCube-Zeile
  0 = GL-Zeile 0, dadurch ohne Spiegelungen bei Kopien, Schere und Bildausschnitt;
- Shader werden aus dem Zustand erzeugt und zwischengespeichert: Vertex-Teil mit
  XF-Transformation (Matrix-Indizes je Punkt oder Standard), Licht (8 Lichter,
  Richtung/Spot/Glanz, Diffus-Funktionen, Material/Umgebung aus Register oder
  Punktfarbe), Texturkoordinaten-Erzeugung (Matrizen 2×4/3×4, Quellen Position/
  Normale/Farbe/Texturkoordinaten, Prägung, zweite Transformation mit
  Normalisieren); Fragment-Teil mit bis zu 16 TEV-Stufen in 8-Bit-Ganzzahl-
  Rechnung wie die Hardware (Rundung, Vorzeichen-11-Bit-Register, Vergleichsmodi,
  Konstanten, Tauschtabellen), indirekte Texturen (Matrizen, dynamisch S/T,
  Umbruch, Bump-Alpha), Alpha-Test, Nebel (4 Formeln, perspektivisch und
  orthografisch);
- Mischen, Tiefe, Farb-/Alpha-Schreibmaske, Seiten-Kappen, Schere, Linien-
  und Punktgröße aus den BP-Registern; alle GX-Texturformate (I4 … CMPR,
  Paletten IA8/RGB565/RGB5A3), Mipmaps, Sampler aus den Texturmodi;
- Bildpuffer-Kopien: in Texturen (Farb-, Intensitäts-, Kanal- und
  Tiefenformate, 2×2-Filter, Löschen nach dem Kopieren) und in das Anzeigebild
  (XFB); Kopien, die als Paletten-Textur gelesen werden (Wärmesicht,
  `filter07`/`filter02`), laufen durch die Palette.
- Noch nicht nachgebildet (G5): Ziel-Alpha (`GXSetDstAlpha`), Z-Texturen,
  die meisten Logik-Operationen, `GXPeekZ`, Linienbreite über 1 Pixel hängt am
  Grafiktreiber.

**G4 – in der App und in der PC-Prüfung:**

- Kernmodus (`core_mode.cpp`): Die App holt jedes Bild alle fertigen
  Bildstücke, führt sie aus und zeigt die letzte Bildkopie im 4:3-Rahmen
  (640 breit, Zeilen aus 480). Bis zur ersten Bildkopie (und mit der
  Einstellung `RE4_VIEW=bridge`) zeichnet weiter die Zustandsbrücke.
  EFB-Faktor `RE4_GX_SCALE` (Standard 2). Neuer GL-Kontext: der Kern beginnt
  von vorn (voller Zustand, alle Texturen neu). `test-core-app.ps1 -Bridge`
  zeigt zum Vergleich die frühere Ansicht; der Bericht nennt Ansicht,
  GX-Zähler und Shader-Fehler.
- PC-Prüfung (`whole_r100_check.sh`, `test-r100-core.ps1`): Der Kern schreibt
  mit `RE4_GX_DUMP` bis zu 24 Bilder als Bildstück-Dateien
  (`re4_gx_frame_io.h`; früh: Bild 60 … 2400 für Logos/Titel/Menü, dann der
  Raum-Zeitplan der Schnappschüsse und alle 2500); das Werkzeug
  `gx_chunk_render` (App-Renderer auf Mesa) zeichnet sie, Bericht Abschnitt 5,
  Ordner `bilder-gx`.

Tests (Cloud, ohne Disc-Daten):
- `gx_gl_check` (`tools/test_gx_gl.cpp`): Texturdecoder gegen den geprüften
  TPL-Leser der App für alle 11 Formate (13×9 Texel, keine ganzen Kacheln) –
  gleich; gezeichnete Bildstücke: Löschen per Bildkopie, Punktfarben,
  Ausrichtung, Seiten-Kappen (im Uhrzeigersinn = Vorderseite), Tiefentest,
  Mischen, Alpha-Test, Textur über die TEV (Texel oben links, nächster
  Nachbar), Textur erneut nur per Schlüssel, Konstantfarbe, Lerp mit
  Hardware-Rundung, Vergleichsmodus, Subtraktion mit Faktor 2, Richtungslicht
  mit Umgebungslicht, Bildpuffer-Kopie als Textur, Paletten-Textur, linearer
  Nebel; 60 zufällige TEV-Aufbauten (Eingänge, Vergleiche, Faktoren,
  Tauschtabellen, Konstanten, indirekte Stufen): alle 70 Shader übersetzen.
  Ergebnis: 0 Fehler.
- Durchgehend: Das künstliche Bild von `runtime-test gx` (echter Decoder des
  Kerns) als Datei (`RE4_GX_TEST_DUMP`) mit `gx_chunk_render` gezeichnet:
  5 Zeichenaufträge, 2 Texturen, 1 Kopie, Shader fehlerfrei.
- `runtime-test gx` zusätzlich: Warteschlange zwischen Spiel- und
  Renderer-Thread (60 Bilder in Reihenfolge, keins verworfen; ein Renderer,
  der nichts mehr abholt, hält das Spiel höchstens etwa eine Sekunde auf;
  danach beginnt das nächste Stück von vorn).
- Bisherige App-Prüfungen unverändert grün (`core_scene_gl_check` 85,
  Bindungs-Prüfung `re4decomp_link_check` mit `core_mode.cpp`).

Nicht getestet: mit Disc-Daten (PC-Prüfung, Abschnitt 5) und auf dem Gerät
(`test-core-app.ps1`). Das sind die nächsten Schritte beim Projektinhaber.

### Vierter PC-Lauf mit GX-Bildern (15.10.2026, WSL2) und Analyse

Ergebnis des Laufs (`e4fcc0e2`): Formatvergleich 155 Dateien ohne Fehler;
Hauptschleife bei Bild 621 (r100); die Bilder der Brücke zeigen das Intro
jetzt richtig (Auto mit Dach, Polizisten, Leon). Zwei Fehler:

- **Stillstand bei Bild 1913, Funkspruch:** Der Funk-Bildschirm
  (`ss_term.cpp OpeMesMove`) beendet die Funk-Tonspur mit `SndStrStopBlock`:
  Stopp anfordern, dann `do SceSleep(1); while (spielt)`. `SceSleep` schläft
  nur in Szenario-Aufgaben; im Funk-Bildschirm kehrt es sofort zurück, die
  Schleife dreht. Auf der GameCube blendet derweil der Ton-Interrupt die
  Spur aus; im Port kommen Interrupts nur an Unterbrechungspunkten an, die
  Schleife hat keinen. **Patch 0035:** `SceSleep` außerhalb einer
  Szenario-Aufgabe wartet auf das nächste Halbbild (`re4_wait_interrupt`),
  dabei laufen die Audio-Rahmen. Test `runtime-test stream`, neuer Fall
  „stopped and waited for like the radio screen“: hängt ohne den Patch
  (2000 Durchläufe ohne Ende), endet mit ihm nach 26 Halbbildern. Der
  Zustands-Trace schreibt jetzt die Tonspuren mit (`"str"`, Bericht:
  „Tonspuren im letzten Bild“).
- **GX-Bilder fast ganz weiß:** Die GX-Bibliothek liest Farben (`GXColor`,
  vier Bytes R G B A) an mehreren Stellen als ein 32-Bit-Wort
  (`*(u32*)&color`, `GXCOLOR_AS_U32`); auf little-endian liegen die Bytes
  dann verkehrt herum (Schwarz deckend wird Rot durchsichtig).
  **Patch 0033** setzt das Wort aus den Bytes zusammen: TEV-Farben,
  Konstantfarben, S10-Farben, Material-/Umgebungsfarbe, Nebelfarbe, und die
  Lichtfarbe beim Laden des Lichtobjekts (Patch 0030). **Patch 0034:** sechs
  Stellen im Spiel geben Ein-/Ausblendfarben als `u32` an `FadeSet`
  (`card.cpp`, `tv_mode.cpp`, `event.cpp`); sie werden jetzt in
  GameCube-Byte-Reihenfolge geschrieben (`RE4_BE`). Test `runtime-test gx`
  prüft TEV-, Konstant-, Material-, Licht- und Nebelfarbe in den Registern:
  schlägt ohne Patch 0033 fehl, mit ihm grün.
- Damit die GX-Ausgabe künftig auch in der Cloud an echten Bildern geprüft
  werden kann, legt die PC-Prüfung einige Bildstücke als `gx-daten.zip` in
  den Bericht (nur zum Hochladen in die Sitzung, nie ins Repository).

Gerät: Die App stürzte sofort ab; Protokoll (`logcat.txt`) steht noch aus.

### GX-Daten in der Cloud: weiße Bilder und Zeigerprüfungen (15.10.2026)

Mit `gx-daten.zip` aus dem fünften PC-Lauf (Stand e035f750, mit `-Arm`;
i686 und armhf/qemu je 10.000 Bilder ohne Absturz) zeichnet
`gx_chunk_render` die Bilder 61, 301, 601 und 652 hier nach (die Dateien
bleiben im Sitzungsordner, nie im Repository). Ein Schrittwerkzeug (EFB
nach jedem Zeichenauftrag) zeigte: Schon der zweite Auftrag von Bild 301
(Warnbildschirm) färbt das ganze EFB weiß. Seine TEV-Stufe hatte Wörter wie
`0x8F10C0` (Register-ID im untersten Byte, Bias 3 = Vergleichsmodus): die
Tabellen von `GXSetTevOp` sind Bitfeld-Strukturen, auf Little-Endian falsch
gepackt (Patch 0036). Mit nachträglich korrigierten TEV-Wörtern zeigen die
aufgezeichneten Daten den Warnbildschirm und die Autofahrt des Intros
(einschließlich der Debug-Anzeigen der Entwicklerfassung); der Renderer
selbst war also richtig. Test: runtime-test gx prüft `GXSetTevOp`
(Modulate auf Stufe 0, Replace auf Stufe 1); ohne Patch 0036 schlägt er fehl.

Dabei fiel die Bildschirmmeldung `cLightInfo::init2() PTR ERROR` auf (24-mal
im PC-Lauf): `VALID_PTR` (127 Stellen in 31 Dateien) prüft gegen MEM1, die
Aufrufer übergeben aber Stapelvariablen, die im Port auf Host-Stapeln
liegen. Patch 0037 nimmt Host-Stapel und statische Daten der
Spielbibliotheken hinzu. Test: runtime-test tasks (Stapelvariable eines
OS-Threads, `Global`, MEM1 gültig; NULL, 0x1000, Host-Heap ungültig); ohne
Patch 0037 schlägt er fehl. Offen: 17 weitere Stellen prüfen den
MEM1-Bereich ohne `VALID_PTR` (eigene Ausdrücke); noch nicht durchgesehen.

Noch unklar: der Absturz auf dem Retroid in den ersten 15 s (auch mit
`-Bridge`, also im Kern), während armhf unter qemu durchläuft. Das
Geräteskript sichert jetzt den Absturzbericht (siehe unten).

### Erster Gerätelauf mit Absturzbericht (15.10.2026, Retroid) und Analyse

Stand `6e2288f1`. Das neue Geräteskript erkannte den Absturz nach 20 s
(die App startete einmal neu und stürzte wieder ab) und übersetzte die
Stapelzeilen: SIGBUS `BUS_ADRALN` (unausgerichteter Zugriff) in
`GXInitFifoBase` (`src/lib/GXFifo.c:111`) aus `GXInit` ← `Render_init` ←
`systemStartInit`, Adresse `…515`. `GXFifoObj` ist im SDK ein Byte-Feld
(`u8 pad[128]`, Ausrichtung 1); das statische `FifoObj` lag im
Android-Build an einer ungeraden Adresse, und clang fasst die vier
Wort-Speicherungen in `GXInitFifoBase` zu `strd` zusammen, die auf ARM eine
Wortausrichtung verlangen (der Kernel des Retroid korrigiert das für
32-bit-Prozesse nicht). Im armhf-Linux-Build lag das Objekt zufällig
ausgerichtet; qemu-arm meldet solche Zugriffe sonst ebenfalls (geprüft mit
einem kleinen Programm: SIGBUS). **Patch 0038** richtet `GXFifoObj` auf
32 Byte aus. Tests: `static_assert` in `tests/game_probe.cpp` (der Build
scheitert ohne den Patch) und `runtime-test gx` prüft die Adresse, die
`GXInit` zurückgibt. Durchsicht der übrigen SDK-Typen, die als Struktur
gelesen werden: `GXTexObj`, `GXTlutObj`, `GXLightObj`, `GXTexRegion` sind
`u32`-Felder (Ausrichtung 4), nur `GXFifoObj` war ein Byte-Feld.

Fünfter PC-Lauf (Stand `6e2288f1`, mit `-Arm`): i686 und armhf je 10.000
Bilder ohne Absturz; „Spieler frei: NEIN“: das Untermenü des Funkspruchs
öffnet bei Bild 1876 und bleibt bis zum Ende offen. Das Spiel-Log zeigt den
Ablauf: Funkarchiv `op/op01.das` geladen, „SND: STREAM 1 STOP“ (Klingelton
beendet, Patch 0035 wirkt), etwa 9 s später „SND: STREAM 0 STOP“ (die
Sprachaufnahme lief durch). Der Funk-Bildschirm (`ss_term.cpp OpeMesMove`)
schließt aber nur, wenn seine Ablauf-Tabelle (`OpeMesSeq`: Bild, Meldung,
Anzeigedauer; Ende bei Dauer −1) den letzten Eintrag erreicht. Diese Tabellen
stehen im Funkarchiv: `SS_ARC_PTR(arc, n)` liest das n-te Wort des
Archivkopfs, `OP_ARC_PTR(wk, 4 + i)` ist also Eintrag i; `OpeMesTblInit`
nimmt die erste Hälfte der Einträge als Meldungen, die zweite als
Ablauf-Tabellen (op01 13 + 13, op02 6 + 6, op03 5 + 5). Der Umwandler
wandelte die Einträge nur nach ihrem Kennzeichen; für die Ablauf-Tabellen
meldete er keinen fehlenden Umwandler, sie liefen also durch einen
vorhandenen (vermutlich `SEQ`, die Bewegungs-Ereignisliste: nur das erste
16-Bit-Wort und die Schlüssel gedreht), die Bildnummern blieben
big-endian und wurden riesig. **Jetzt:** `convertCodecArchive` wandelt die
zweite Hälfte als `OpeMesSeq` (`convertOpeMesSeq`, unabhängig vom
Kennzeichen) und schreibt die Kennzeichen beider Hälften ins Log (Bericht:
„Funkarchive“). Test `core-test codec`: ein Funkarchiv mit einer
Ablauf-Tabelle unter dem Kennzeichen `SEQ`; schlägt mit dem alten Umwandler
fehl. Noch nicht mit Disc-Daten geprüft. Der Bericht zeigt dazu den
Tonspuren-Verlauf (jedes Bild, in dem sich Tonspuren ändern; die Zeile
„Tonspuren im letzten Bild“ wird jetzt als JSON gelesen, das alte Suchmuster
schnitt sie an der ersten `]` ab).

### Sechster PC-Lauf (15.10.2026, WSL2): erstmals „ALLES OK“

Stand `38b0c6b9`. Formatvergleich 155 Dateien ohne Fehler, 10.000 Bilder
ohne Absturz. Der Funkspruch endet von selbst: Spieler frei bei Bild 3043
(2428 nach dem Raumstart), danach lief Leon mit der Test-Tastenfolge 10409
Einheiten (Steuerung OK). „Funkarchive“ bestätigt die Ursache: in
`op/op01.das` tragen die 13 Ablauf-Tabellen das Kennzeichen `SEQ` und
liefen vorher durch den Umwandler der Bewegungs-Ereignislisten.

### Zweiter Gerätelauf mit GX (15.10.2026, Retroid) und Analyse

Stand `38b0c6b`: Die App startet und zeigt das Intro über die
GX-Nachbildung (Capcom-Logo, Autofahrt r120s00 mit Leon). Der Lauf endete
nach den 120 s des Skripts (App lief bis zuletzt, Ereigniszähler bei
1856/1970) – kein Absturz. Gemeldet: Debug-Anzeigen, kein Vollbild, sehr
langsam (1–2 Bilder/s sichtbar), Renderfehler (Helligkeit, Leons Haare).

- **Geschwindigkeit:** Das Protokoll zeigt etwa 20 Bildstücke/s mit je
  rund 9000 Zeichenaufträgen; der Ereigniszähler lief mit etwa 20 statt 30
  Bildern/s. Sichtbar waren weniger, weil die App je Anzeige bis zu 8
  Bildstücke abarbeitete und nur das letzte zeigte. Jetzt: ein Bildstück je
  Anzeige (jedes Bild des Spiels wird gezeigt; ist der Renderer langsamer,
  wartet der Kern). Der Renderer überspringt, was GL schon hat:
  Zustand (Scissor, Tiefe, Mischung, Seite, Linienbreite) nur bei Änderung
  der Register, Shader-Schlüssel nur bei Änderung der formgebenden
  BP/XF-Register, PX-Block nur bei Änderung seiner Register oder
  Texturgrößen, Texturen und Sampler je Einheit, `uMtx` je Programm,
  XF-Block wortweise statt ganz neu umgerechnet, Vertex-Attribute einmal je
  Layout und Stride mit `glDrawElementsBaseVertex` (GLES 3.2/OES/EXT;
  `RE4_GX_BASEVERTEX=0` schaltet es ab). Uploads, Kopien, Paletten und die
  Anzeige setzen die Merker zurück (`Touched`). Prüfung: `gx_gl_check`
  grün mit und ohne Base-Vertex; vier aufgezeichnete Bilder (61, 301, 601,
  652) pixelgleich zum alten Renderer, beide Wege. Auf llvmpipe (rechnet in
  Software) nur 10–15 % schneller, dort dominiert das Rastern; auf dem
  Gerät zählt der Aufwand je GL-Aufruf (vorher etwa 50 je Zeichenauftrag,
  jetzt meist unter 10). Messwerte im Protokoll: App „GX: … run x ms
  (max y), shown z/s, base vertex yes/no“, Kern „GX core: … ms each, of it
  … ms waiting for the renderer“ (Bericht: „GX zuletzt“, „GX im Kern“).
- **Debug-Anzeigen:** `RE4_DEBUG_HUD=0` setzt `pG->debug_mode` auf 0, wie
  die Vorführfassung (`main.cpp`, `SYS_PUBLICITY_VER`): keine
  Ereignis-/Punktezeilen, Leistungsbalken, Heap-Zahlen, Debug-Texte
  (`EprintfDrawing`), kein grüner Fernnebel (`DBG_FOG_FAR_GREEN`). Die App
  setzt es als Standard (`-DebugHud` im Skript zeigt sie wieder), die
  PC-Prüfung behält sie (die Zähler helfen bei der Analyse). Das
  Diagnosefenster der App ist beim Start ausgeblendet (SELECT zeigt es).
- **Vollbild:** Das Spiel zeichnet überall mit Balken; die Zeilen 56–391
  der 448 ergeben auf einem 4:3-Fernseher genau 16:9. Die App zeigt nur
  diese Zeilen in einem 16:9-Feld (`Present(…, top, rows)`);
  `RE4_GX_ASPECT=4:3` zeigt das ganze Bild wie bisher.
- **Haare/Helligkeit:** Ein Teil der dunklen Flächen auf den Bildern war
  der halbdurchsichtige Hintergrund des Diagnosefensters (endet genau dort,
  wo die Haare wieder hell werden). Andere Stellen (Bild bei 95 s) bleiben
  offen; mit doppelter EFB-Auflösung zeichnet llvmpipe dieselben
  aufgezeichneten Bilder wie mit einfacher (unter 2 % Abweichung, nur
  Kanten), der Unterschied entsteht also auf dem Gerät. Zur Analyse
  speichert die App mit `RE4_GX_DUMP_DIR` alle `RE4_GX_DUMP_EVERY`
  Sekunden ein vollständiges Bildstück (sie meldet sich ab, das nächste
  Stück beginnt mit vollem Zustand), höchstens 6; das Skript holt sie mit
  `-GxDump` als `gx-geraet.zip` (nur hochladen, nie ins Repository).
- Skript: Beobachtung standardmäßig 240 s.

Nicht getestet: alles davon auf dem Gerät; `RE4_DEBUG_HUD` ohne eigenen
Test (Wirkung nur im laufenden Spiel sichtbar).

### Dritter Gerätelauf mit GX (15.10.2026, Retroid) und Analyse

Gemeldet (Skriptlauf mit `-GxDump` und ein Spiel von Hand, Intro mit
START übersprungen): leichter Grünton vor dem Menü, Filmintro fehlt,
Laderuckler, gröbere/flackernde Pixel in Leons Haaren, Autoscheiben von
außen zu dunkel, Absturz nach 153 s mitten im Intro (r120s01), kein Ton;
im Spiel ein kräftiger Orangeschleier über allem, flackernde Flächen,
Leon verschwindet kurz beim Wechsel Spiel → Filmszene, Steuerung
anpassen, Speicherkarten-Abfrage beim Start überflüssig.

- **Absturz 153 s (Patch 0040):** Stapel `re4gx::command` ←
  `callDisplayList` ← `drain` ← `re4_gx_put_f32` ← `Esp1b_SpTrans`, davor
  „GX: unknown command 0x3f …“ (Float-Bytes als Befehle): Der Befehlsstrom
  war aus dem Tritt, ein Zufallswert als `CALL_DL`-Adresse führte zum
  Zugriff ins Leere. Ursache: `Esp1b_SpTrans` findet die Daten des
  Unterobjekts über `&esp->pad_EC[0xC]`, also das Ende von `cEsp` unter
  MWCC, das den vptr ans Ende legt (0xF4). Die Compiler des Ports legen
  ihn an den Anfang; die Adresse lag 4 Bytes in den Daten, `div` war eine
  Gleitkommazahl als Ganzzahl, `div * 4 * div` passte nicht mehr in die
  16-Bit-Punktzahl von `GXBegin`. Jetzt `(u8*) esp + sizeof(cEsp)` (in
  beiden Layouts dieselbe Stelle); `game_probe.cpp` prüft
  `sizeof(cEsp) == 0xF8`. Andere Zugriffe dieser Art (`pad_…[]`,
  `(u8*) this + 0x…`) gibt es im Spielcode nicht.
- **Decoder abgesichert:** `listMemory` nimmt Zeiger außerhalb von MEM1
  nur noch an, wenn Anfang und Ende in Spielspeicher liegen
  (`re4_valid_port_ptr`: Bibliotheken, Thread-Stapel); sonst meldet der
  Decoder die Liste als „not in memory“ statt abzustürzen. Beim ersten
  unbekannten Befehl schreibt er die 16 letzten Befehle (Draws mit Anzahl,
  Punktgröße, VCD, VAT) und die Bytes danach ins Protokoll („GX: the
  command stream lost its place …“).
- **Orangeschleier:** Am Bildende legt das Spiel ein Leuchten über das
  Bild: Kopie des EFB-Alphakanals (A8, halbe Größe, dreimal verkleinert,
  verwischt) und additiv mit Eckpunktfarbe (0x28, 0x19, 0x0C) mal 2
  zurück. Das Leuchten soll nur dort entstehen, wo Lichtquellen Alpha
  schreiben; fast alle anderen Zeichenaufträge setzen mit `GXSetDstAlpha`
  (BP 0x42) Alpha 0. Das kannte der Renderer nicht, Alpha blieb 1 und das
  ganze Bild bekam +(80, 50, 24). Jetzt: Mit `GXSetDstAlpha` schreibt der
  Shader die Konstante (PX `alphaRef.z`), wenn die Mischung die TEV-Alpha
  nicht braucht; braucht sie sie (SRC_ALPHA-Faktoren), setzen die
  Alpha-Mischfaktoren den Wert 0 (der Fall des Spiels). Andere Werte dann
  zählt `Stats().dstAlphaApprox` (Protokoll „inexact dst alpha“). Geprüft
  mit den PC-Daten (Bild 652): Bildpunkt (320,100) bleibt nach dem Filter
  `1a1a16` statt `6a4c2e`. Der leichte Grünton von Wolken und Logo vor dem
  Menü gehört zum Original (Titelbild), die Geräteaufnahmen 00–05 haben
  keinen Schleier mehr.
- **Leons Haare:** Die Geräteaufnahmen zeigen die Fehler auch auf
  llvmpipe (also Renderer-Logik, nicht Adreno), aber nur klein: einzelne
  dunkle Strähnen der Unterschicht (`00823100`) liegen über hellen.
  Geprüft und richtig: Texturen (CMPR, I4-Maske), Texturkoordinaten,
  Sampler, Koordinatenskalen, Tiefenformat, Projektion der Baumschatten.
  Die großen schwarzen Rechtecke vom Gerätefoto (70 s) sind in keiner
  Aufnahme; offen. Dafür speichert die App jetzt auf **R3** (rechter Stick
  gedrückt) jederzeit das nächste Bildstück (`CoreBridge.gxDump`, bis zu
  40, Ordner `files/gx-dumps`); `test-core-app.ps1` holt sie immer mit,
  nach einem Spiel ohne Skript `port/scripts/pull-gx-dumps.ps1`.
- **Ruckler:** Neue Shader kosten auf dem Gerät bis zu einige hundert ms.
  Der Renderer legt gelinkte Programme jetzt als Binärdatei ab
  (`SetProgramCache`, Ordner `cache/gx-programs`, Name = Hash beider
  Quelltexte und des Treibers, `RE4_GX_PROGRAM_CACHE`) und lädt sie beim
  nächsten Mal mit `glProgramBinary` statt zu übersetzen; ein abgelehntes
  Binary wird neu gelinkt. Geprüft mit llvmpipe: zweiter Lauf 30 von 30
  Programmen aus dem Speicher, Bild bytegleich. Protokoll: „programs from
  the cache“. Der erste Lauf ruckelt weiterhin.
- **Speicherkarte (Patch 0039):** `CardFirstCheck` überspringt die
  Startprüfung wie nach einem Reset (`re4_skip_card_check`, runtime/
  card.cpp) – derselbe Ausgang wie „ohne Speichern fortfahren“ auf dem
  Bildschirm „keine Speicherkarte“. `RE4_CARD_CHECK=1` behält sie; die
  Test-Tastenfolgen (`whole_r100_check.sh`, `test-core-app.ps1` ohne
  `-Manual`) setzen es, weil ihr Zeitplan darauf abgestimmt ist.
- Filmintro (Meilenstein 9) und Ton (Meilenstein 8) fehlen erwartungsgemäß.
  Steuerung: Meilenstein 7. Offen ohne Daten: Leon verschwindet kurz beim
  Szenenwechsel, flackernde Flächen, dunkle Autoscheiben (die Aufnahmen mit
  R3 an genau diesen Stellen helfen).

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2` mit allen
Prüfungen (siehe unten), `gx_gl_check`, Renderer mit den Geräte- und
PC-Aufnahmen, App-Bibliothek gegen JNI gelinkt (`re4decomp_link_check`),
Java der App übersetzt, beide PowerShell-Skripte mit einem nachgebildeten
ADB. Nicht getestet: alles auf dem Gerät, die PC-Prüfung mit Disc-Daten.

### Vierter Gerätelauf mit GX (15.10.2026, Retroid) und Analyse

Stand `45205aa`: Der Skriptlauf lief die vollen 240 s ohne Absturz durch
das Intro bis r100 (der Absturz nach 153 s ist weg). Gemeldet aus einem
Spiel von Hand: Pixelflecken in Leons Haar (heller Hinterkopf), zu dunkle
Glasscheiben, „CLIMB DOWN“ an Geländern und Fenstern (danach kurz keine
Steuerung), kein Sprung durch Fenster, durchlaufbare dünne Bäume, ein
Ganado springt durch die Hüttenwand, Ganados sterben „wie ins Wasser“,
falsche Ganado-Reaktionen, Logo im Hauptmenü verblasst, START im Menü
zweimal nötig, Schatten am Fenster in einer Zwischenszene fehlt, Leon
verschwindet kurz vor Filmszenen.

- **Kollision (Patch 0041):** `Get_poly_attr` (at_sub.cpp) liest das
  Attributwort einer Kollisionsfläche als zwei Halbwörter, das obere zuerst.
  Im Port liegt `SAT_POLY::m_Status` als Host-Wort vor (umgewandelte SAT/EAT,
  `createSat`), die Hälften kamen vertauscht an: aus Zaun (0x20) wurde „hoch“
  (0x200000), aus „Spieler kollidiert nicht“ (0x400000) „Route“ (0x40),
  „Fallkante“ (0x100000) und „runter“ (0x10) tauschten, bei EAT die
  Effektbits (Wasser-Spritzer beim Tod). `Get_poly_attr` liest jetzt das Wort
  (auf dem GameCube derselbe Wert), ebenso die Debug-Anzeige in atari.cpp.
  Das erklärt „CLIMB DOWN“ statt „JUMP OVER“, durchlaufbare Bäume, Ganados
  durch Wände und den Wasser-Tod; nicht auf dem Gerät geprüft.
- **Weitere schmale Sichten** über Zeiger statt `&x` (das Suchskript von
  M4.5 fand nur `&x`): alle 21 Stellen durchgesehen. Richtig waren
  `motion.cpp` (Byte 2 ist das u8-Flag der Sequenzdatei), die EMI-Bytes
  (`EmiEntry::type`), `id_sys.cpp` (`FUNC_PATH_PTR::nPoint` ist ein Byte),
  die Debug-Werkzeuge `t_flag`/`t_emlist`. Korrigiert: `card.cpp`
  `CardID::save` (Byte 3 von `HERMITE_1_PTR::nPoint`, Patch 0041). Offen für
  später: `r117.cpp` liest Byte 0xF9 eines Effekts (Raum r117).
- **Alphakanal ohne Farbe:** Rund 1100–1900 Zeichenaufträge je Bild
  schreiben mit `GXSetDstAlpha` nur Alpha (Farbe aus, Werte 0x33–0x96, eine
  Maske für spätere Filter). Bisher galten sie als „ungenau“ und schrieben 0
  (Protokoll: 4709 „inexact dst alpha“); ohne Farbschreiben schreibt jetzt
  der Shader den Wert genau.
- **Leons Haare im Spiel (Patch 0042):** Leons Haar besteht aus
  Doppelschichten: jede Strähne zweimal mit denselben Punkten, die Innenseite
  mit nach innen zeigenden Normalen und umgekehrtem Umlauf (Bild 4744: Haar
  16451–16602, z. B. 16517 innen / 16536 außen). Auf dem GameCube lässt
  `GX_CULL_FRONT` jeweils die abgewandte Schicht weg. Im Port hatte **kein**
  Zeichenauftrag Rückseiten-Weglassen (BP 0x00 Bits 14–15 überall 0):
  `GXSetCullMode` verwarf das Ergebnis von `__rlwimi`. Beide Schichten wurden
  gezeichnet; wo die Strähnen nicht ganz eben liegen, gewann die dunkle
  Innenseite die Tiefenprüfung (dunkle Rechtecke, flackernd bei Bewegung).
  Geprüft mit der Geräteaufnahme: mit Rückseiten-Weglassen (Hardwarewert 2,
  wie `GX_CULL_FRONT`) ist das Haar glatt und hell wie im Original, die
  Szene sonst unverändert; mit Wert 1 stünde alles „von innen“ da (die
  Richtung im Renderer stimmt also). Ebenso verschwinden die Blöcke in den
  Stirnsträhnen der Autoszene (Bild 1160). Der Test `gx` prüft jetzt alle
  vier Werte. Betrifft vermutlich auch zu dunkle Scheiben und flackernde
  Flächen (doppelt gezeichnete Rückseiten), Schatten auf Modellen
  (`shadow.cpp` nutzt ebenfalls `GX_CULL_FRONT`) und Spiegel; auf dem Gerät
  zu prüfen. Die Stoff-Simulation (`pendulum.cpp`) ist nicht die Ursache
  (Mathe-Ersatz, Strukturlage `cParts`/`PEN_INFO` geprüft).
- **Titel:** „START zweimal“ und das verblassende Logo sind ohne Aufnahme
  des Titelbilds offen.

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2`, alle Prüfungen
grün (Laufzeittest `gx` mit der neuen Prüfung der Rückseiten-Werte,
Formatvergleich, Modul- und Kerntests); Geräteaufnahmen 4744 und 1160 mit
erzwungenem Rückseiten-Weglassen im Renderer nachgezeichnet. Nicht
getestet: Gerät und PC-Prüfung mit Disc-Daten.

### Fünfter Gerätelauf mit GX (15.10.2026, Retroid) und Analyse

Stand `880ef9a` (Patch 0042). Bestätigt vom Projektinhaber: Haare und
Glasscheiben jetzt richtig, Gegner verhalten sich wie im Original. Der Lauf
reichte von Hand bis zum Übergang nach r101 (Dorf) und stürzte dort nach
579 s ab. Gemeldet: kurzes Flackern zwischen Szenen der Autofahrt,
„Spiegelungen“ rechts neben den Gesichtern im Funkgerät, Leon in einem
Baumstamm, Leon verschwindet vor einer Zwischensequenz und danach steht kurz
der leere Raum da (Video), Logo im Titel verblasst.

- **Absturz 579 s:** in der Zustandsbrücke (`core_bridge.cpp`
  `captureModel`, Diagnose-Teil des Ports, nicht Spielcode) beim Raumwechsel
  nach r101: `live()` ließ jede Adresse ab 0x80000000 gelten, eine
  Modell-Info zeigte beim Abbau des Raums auf 0xabc28685. Jetzt nur MEM1 oder
  Spielbibliothek (`re4_valid_port_ptr`).
- **Leon verschwindet / leerer Raum (Video, je 4 Bilder):** Beim Start und
  Ende einer Zwischensequenz setzt das Spiel `SYS_SCREEN_STOP`
  (`event.cpp`, Raumskripte). `Render_swap` (`main_sub.cpp`) zeichnet dann
  weiter, ruft aber `VISetNextFrameBuffer` nicht auf: Der Fernseher zeigt das
  letzte Bild weiter, während das Spiel Figuren ausblendet und die Kamera
  umsetzt. Der Port zeigte jede Display-Kopie sofort. Jetzt wartet ein
  Bildstück mit Display-Kopie bis `VIFlush` (bzw. bis zur nächsten Kopie);
  schaltet der VI nicht auf seinen XFB um, trägt es `RE4_GX_CHUNK_HOLD`: Die
  App führt es aus (EFB, Löschen, Texturkopien), behält aber das sichtbare
  Bild (`GxStats::heldCopies`, Protokoll „held pictures“). Prüfungen: Test
  `gx` (gehaltenes und danach gezeigtes Bild), `gx_gl_check`. Erklärt
  vermutlich auch das Flackern zwischen den Szenen der Autofahrt (jede
  Ereignis-Szene beginnt und endet so); nicht auf dem Gerät geprüft.
- **Logo im Titel:** Aufnahmen 787/846 (Titelmenü) gegen 599: Alle Teile
  des Logos tragen dieselbe Alpha-Kurve der Id-Animation; die Werte springen
  (0xC3 → 0x8D → 0x3A, ein Teil 0x9C → 0x71 → 0xFC). Das passt zu
  überlaufenden Umwandlungen `(u8)(f32)` in `idSysMove02`
  (`u->col[3] = (f32)(u8)(...)`) bei Kurvenwerten über 255, die das Original
  bei 255 abschneidet (siehe Compiler-Schalter oben). Behoben durch
  `-fno-strict-float-cast-overflow`; nicht auf dem Gerät geprüft. „START
  zweimal“: Das Menü wählt mit A (`titleMenuSelect`); offen bis zur
  Tastenbelegung (Meilenstein 7).
- **Funkgerät:** Die Aufnahme 7483 zeigt die Gesichter im Renderer wie auf
  dem Gerät, mit waagerechten Schlieren rechts neben den Köpfen
  (Bildstörungs-Effekt der Übertragung). Ob das Original sie so stark zeigt,
  ist offen; nach der Umwandlungs-Korrektur neu ansehen.
- **Baum:** Kollision läuft über die SAT-Polygone (`atari.cpp`
  `polySphereCk`, Blockbaum, `At_poly_sphere_ck`); Umwandler und Strukturen
  geprüft, ohne Befund. Die App schreibt bei **R3** jetzt zusätzlich Raum,
  Spielerposition und Kamera ins Protokoll („R3: room …, player at …“); mit
  der Position lässt sich die Stelle am PC nachstellen (`RE4_TEST_PLACE`).

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2` mit allen Prüfungen
(neu: Begrenzung der Umwandlung im Test `math`, gehaltenes Bild im Test `gx`),
App-Renderer `gx_gl_check`. Nicht getestet: Gerät, PC-Prüfung mit Disc-Daten.

### Sechster Gerätelauf mit GX (15.10.2026, Retroid) und Analyse

Stand `1c16ab6`. Bestätigt vom Projektinhaber: Logo im Titel jetzt richtig,
r101 (Dorf) erreichbar. Gemeldet: im Titel zweimal START nötig, nach dem
zweiten Druck ruckt das Bild kurz nach rechts; manchmal ein leicht
abgedunkelter Balken unten (Intro am Fenster, oft wenn Leon getroffen wird);
Karte (Inventar oder R1) bleibt schwarz; r101 ruckelt stark; nach dem Tod und
„nicht weiterspielen“ bleibt das Bild schwarz; `test-r100-core.ps1` fragte
nach `DataDir`.

- **Zweimal START = Raumauswahl (Patch 0043):** Nach „New Game“ zeigt die
  Debug-Fassung ein Startmenü (`title.cpp` `titleExit`/`titleDebugMenu`:
  Stage, Raum, Sprungpunkt, Gegnerliste …) und wartet auf A oder START.
  Seine Zeilen sind Debug-Text (`eprintf`), den die App mit
  `RE4_DEBUG_HUD=0` ausblendet: Das Menü war unsichtbar, der zweite Druck
  bestätigte es. Auf Wunsch des Projektinhabers bleibt es (zum Testen
  einzelner Räume) und ist jetzt sichtbar: Patch 0043 meldet das offene Menü
  an die Laufzeit (`re4_title_room_menu`, runtime/dolphin_os.cpp), die
  Debug-Anzeigen-Sperre (`state_trace.cpp`) lässt `debug_mode` dann
  mindestens 1. `RE4_ROOM_MENU=0` überspringt das Menü wie ein sofortiges A
  (Zeile 0 „DEBUG GAME“ mit dem voreingestellten Raum). Bedienung:
  Steuerkreuz hoch/runter wählt die Zeile, links/rechts ändert den Wert
  (STAGE, ROOM, JUMP_POINT), A oder START startet; Zeile 1 lädt, Zeile 2
  startet r120, Zeile 18 (Wert 0x12) schaltet „hard mode“ mit Neustart; der
  rechte Stick (C-Stick) verschiebt das Menü (die Kopfzeile mit dem
  Raumnamen liegt bei y=44, über dem 16:9-Ausschnitt der App).
  Das Bild-Rucken nach dem Bestätigen: Dort beginnt im Original das
  Eröffnungsvideo (Meilenstein 9); nicht weiter untersucht.
- **Kurze Tastendrücke:** Die App gab dem Spiel den Tastenstand im Moment von
  `PADRead`; ein Druck kürzer als ein Spielbild (bei Rucklern länger) ging
  verloren. `pad.cpp` merkt sich jetzt Drücke seit dem letzten `PADRead` und
  meldet sie einmal (Test `core-test` Teil `thread`).
- **Tod → schwarz:** „nicht weiterspielen“ setzt `SYS_SOFT_RESET`
  (`game.cpp`), `systemSoftReset` (`main_sub.cpp`) schaltet das Bild schwarz,
  bricht Lade- und ARAM-Aufträge ab, räumt die Heaps und kehrt zum Titel
  zurück (liest `SS/eng/title.dat` neu). Die PC-Prüfung löst diesen Neustart
  jetzt aus (`RE4_TEST_SOFT_RESET="free+5000"`, state_trace.cpp) und meldet,
  ob der Titel neu geladen wird („Neustart nach Spielende“). Ergebnis steht
  aus (PC-Lauf mit Disc-Daten).
- **Karte schwarz:** Die PC-Prüfung öffnet die Karte mit Z (`RE4_PAD_FREE_SCRIPT`
  `Z@30 B@300`) und schreibt die GX-Daten 200 Bilder nach dem Spielstart
  (`RE4_GX_DUMP_FREE`, core_api.cpp); der Bericht zeichnet sie
  („Karte offen“). Ergebnis steht aus.
- **Dunkler Balken unten:** Verdacht auf die Filter 00/01 (Kontrast,
  Tiefenschärfe), die das EFB in halber Größe kopieren
  (`GXSetTexCopyDst(SCR_W/2, SCR_H/2, …)`) und zurückzeichnen. Braucht eine
  R3-Aufnahme in dem Moment.
- **r101 ruckelt / Zeichenaufträge zusammengefasst:** Das Gerätebild 9131
  (r100, Wald, `gx-geraet-03.bin` aus dem fünften Lauf) hat 15 931
  Zeichenaufträge mit im Schnitt 7 Dreiecken: Der Decoder machte aus jedem
  `GXBegin` einen eigenen GL-Aufruf. 15 465 davon folgen direkt auf einen
  anderen mit gleichem Aufbau (keine Zustandsänderung dazwischen).
  `gx_decode.cpp` `draw()` hängt so einen Auftrag jetzt an den vorigen an
  (gleiche Primitivklasse, gleicher Punktaufbau und Standardmatrizen,
  höchstens 65 536 Punkte, Reihenfolge der Dreiecke bleibt): 466 Aufträge
  statt 15 931, im Autofahrt-Bild 7483 140 statt 2 214. Die vier Gerätebilder,
  so zusammengefasst und mit dem App-Renderer gezeichnet, sind pixelgleich
  mit den ursprünglichen. Auf llvmpipe (Software) kaum schneller, dort
  zählt das Rastern; auf dem Gerät zählt der Aufwand je GL-Aufruf. Wirkung
  auf dem Gerät nicht gemessen. Für r101 selbst fehlt noch das Protokoll;
  die App-Prüfung lässt sich jetzt mit Q beenden und sammelt trotzdem alles
  ein.
- **Werkzeugmenü (L + START im Spiel):** Die Debug-Fassung öffnet mit
  gehaltenem L und START das „TOOL MENU“ (`db_menu.cpp`: AREA JUMP, FLAG
  EDIT, DEBUG OPTION → PLAYER mit INF BULLET, NO DEATH, WEAPON, LIFE …) und
  hält das Spiel an. Auch das war Debug-Text und damit unsichtbar (das
  Spiel stand still, ohne dass etwas zu sehen war). Die Debug-Anzeigen-Sperre
  lässt `debug_mode` jetzt in Ruhe, solange `DBG_TEST_MODE` gesetzt ist; das
  Menü setzt beim Schließen die alte Seite (0) selbst zurück. Nicht getestet.
- **Skripte:** `test-r100-core.ps1` nimmt `DataDir` standardmäßig aus dem
  Nachbarordner `RE4-Discdata`; `test-core-app.ps1` beendet die Beobachtung
  mit Q oder Strg+C und sammelt danach Protokolle und Aufnahmen ein, der
  Bericht zeigt die R3-Zeilen.

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2` mit allen Prüfungen
(neu: kurzer Tastendruck im Test `core-test`, Debug-Seite bei offenem
Startmenü im Test `trace`, zusammengefasste Aufträge im Test `gx`),
Bildvergleich der zusammengefassten Gerätebilder. Nicht getestet: Gerät,
PC-Prüfung mit Disc-Daten (Karte, Neustart nach Spielende), das sichtbare
Raumauswahl- und Werkzeugmenü.

### Lokale Prüfungen für K2

1. `port/scripts/test-r100-core.ps1` (wie M5) zeichnet jetzt zusätzlich
   Bilder: Der Kern schreibt mit `RE4_BRIDGE_SNAPSHOT` bis zu 14
   Schnappschüsse (30, 150, 300, 450, 600 und 750 Bilder nach Beginn der
   Hauptschleife: die Autofahrt des Intros; dann alle 2500),
   `core_snapshot_render` (`port/android-decomp/tools/`) zeichnet sie mit dem
   App-Renderer und den Disc-Daten (Bericht Abschnitt 4, Ordner `bilder`).
   Braucht die Mesa-Pakete aus `whole_setup_linux.sh`.
2. `port/scripts/test-core-app.ps1`: (1) WSL: NDK einrichten, Kern bauen
   (`whole_build_android.sh --source <Windows-Checkout> --into <App>`),
   (2) Windows: `gradlew.bat :app:assembleCoreDebug`, (3) Retroid unter der
   Sperre `Global\Retroid_ADB_Exclusive`: installieren (prüft
   `primaryCpuAbi=armeabi-v7a`), mit Debug-Start r100 und der
   Test-Tastenfolge starten (`-Manual`: Titelbild, selbst spielen),
   Bildschirmfotos bei 15, 40, 70, 95 … s, Protokoll (`logcat` der App ab
   Startzeit), App beenden. Bericht `Desktop\RE4-Kern-App\bericht.txt`.
   Fehlt Disc 1 in der App „RE4 Kern“ (eigener Datenordner, ein Import der
   bisherigen App zählt nicht), startet das Skript die App, bittet um den
   Import und wartet darauf, ohne die App zu beenden.
   Seit 15.10.2026: Das Skript fragt jede Sekunde, ob die App noch läuft.
   Stürzt sie ab, endet die Beobachtung sofort (Bild, Protokoll mit den
   Puffern main/system/crash und allen Zeilen der App-Prozesse, der
   Absturzbericht des Systems aus `dumpsys dropbox` als
   `absturz-system.txt`), und `port/tools/android_symbolize.sh` übersetzt
   die Stapelzeilen in WSL mit den Bibliotheken des Kern-Builds (über die
   Build-ID zugeordnet) in Funktion, Datei und Zeile. Ergebnisse früherer
   Läufe (`logcat.txt`, `absturz-*.txt`) löscht es am Anfang. Anlass: Bei den
   ersten Geräteläufen nach G1–G4 stürzte die App vor 15 s ab, das Skript
   wurde vor dem Protokollschritt beendet, und im Ordner lag noch die
   `logcat.txt` vom Vortag. Geprüft mit nachgebildetem `adb`/`wsl.exe` und
   PowerShell 7 (Absturz nach 4 s erkannt, alter Dropbox-Eintrag
   ausgelassen, Stapelzeile übersetzt).

Ziel des Gerätetests: Titelbild sichtbar, r100 mit Originalablauf (Ereignis
s03, Tor, Hinterhalt). Grenzen dieses Stands: Effekte, Partikel, Schrift,
Menüs und 2D-Anzeigen des Spiels zeichnet die Brücke noch nicht (das braucht
den GX-Ersatz); Modelle ohne Disc-Herkunft (aus gepackten Raumdateien)
erscheinen als „missing“. Ton fehlt (M8).

## Aufbau

| Ergebnis | Inhalt |
|---|---|
| `libre4_platform.so` | Plattformschicht: MEM1, Dolphin-OS-Ersatz (Arena, Heap aus dem SDK, Threads, Zeit), DVD über Ordner, ARAM, VI, OSLink/Modul-Lader, Yz2-Kern, SDK-Mathematik |
| `libre4_main.so` | Spielcode von `main.dol` (295 Dateien), SN-Toolchain-Ersatz, erzeugte Platzhalter |
| `modules/libre4_<modul>.so` | eine Bibliothek je Original-REL-Modul (114) |
| `re4_host` + `libre4_host.so` | Linux-Vorlader (`host/loader.c`) und Startprogramm (`host/main.cpp`: `--self-test`, `--check-modules`, `--runtime-test`, sonst Spielstart; `--data DIR`; `kill -USR1 <pid>` gibt einen Backtrace der laufenden Stelle aus) |

Warum zwei Bibliotheken: Der Spielcode ersetzt `operator new/delete` durch
seine Heaps (`main_mem.cpp`). Die Version-Script-Datei
`runtime/libre4_main.ver` hält diese vier Operatoren lokal in `libre4_main`,
die Module erreichen sie über `--wrap=_Znwj …` (`runtime/game_heap.cpp`).
Plattform-Code, C++-Laufzeit und später Renderer/Android-Teil nutzen so
weiter `malloc` und berühren die Spiel-Heaps nie, auch nicht vor
`SystemMemInit`.

| Pfad | Inhalt |
|---|---|
| `port/whole/CMakeLists.txt` | Build: Objektbibliothek je Modul, Link-Plan, Bibliotheken, Startprogramm |
| `port/whole/cmake/linux-*.cmake` | Toolchains i686 (Cloud-Test) und armhf (ARM32, Test unter qemu-arm) |
| `port/whole/include/` | Ersatz-Header: `types.h`, `dolphin/types.h`, Prelude, `re4_task_func.h`, `fake_tgmath.h` |
| `port/whole/patches/` | Geprüfte Einzelkorrekturen, angewendet auf die Kopie |
| `port/whole/runtime/` | Plattformschicht (Tabelle unten), SN-Toolchain-Ersatz, Modul-CRT |
| `port/whole/runtime/formats/` | Formatumwandler: Disc-Daten in Host-Reihenfolge (siehe unten) |
| `port/whole/host/` | Linux-Vorlader `loader.c` und Startprogramm `main.cpp` mit Absturz-Backtrace |
| `port/whole/tests/` | Selbsttest der Modul-Weiterleitungen, Laufzeittests, Test-REL-Modul |
| `port/tools/whole_prepare.py` | Kopie von `src/` + `include/`, Umschreibungen, Patches, SDK-C-Auszug, Modulplan `modules.json` |
| `port/tools/whole_link_plan.py` | Probe-Link, erzeugt Platzhalter, Modultabelle und Weiterleitungen |
| `port/tools/whole_make_patch.py` | Erzeugt einen Patch gegen die umgeschriebene Fassung |
| `port/tools/whole_build.sh` | Ein Befehl für Bauen und alle Prüfungen |
| `port/tools/whole_setup_linux.sh` | Installiert die Werkzeuge (Cloud-Startskript, WSL2) |
| `port/tools/whole_disc_folder.py` | Legt den Disc-Ordner an (aus Disc-Abbildern oder dem Speicher des alten Imports), prüft ihn |

Die Original-Quellen werden nie verändert. `whole_prepare.py` legt bei jedem
CMake-Lauf eine Kopie unter `port/build/whole-*/prepared/tree` an. Geänderte
Dateien werden nur bei echten Änderungen neu geschrieben, damit nicht alles neu
kompiliert wird.

Die Zuordnung der Dateien folgt dem Original-Link (`configure.py`):
`config/G4BE08/objects.py` liefert die 295 Spieldateien von `main.dol`,
`config/G4BE08/modules.py` und `config.yml` die 114 Module mit ihren Dateien
in Link-Reihenfolge. Insgesamt werden 703 Übersetzungseinheiten gebaut, weil
gemeinsame Dateien (z. B. `em_wrap.cpp`, `st2.cpp`) wie im Original in jedes
Modul einzeln kompiliert werden. Jedes Modul bekommt `-DREL_MODULE=<modul>`
und seine `-D`-Schalter aus `modules.py` (`TOOLS_ARRAY` …). Der GCC-Schalter
`-fno-implement-inlines` hat bei clang keine Entsprechung und entfällt.

Die 58 `.c`-Dateien in `src/game` (`printf.c`, `strcpy.c`, `sn_malloc.c` …)
sind die SN/newlib-C-Laufzeit. Sie entfallen, weil die Plattform-C-Bibliothek
sie ersetzt.

## Mechanische Umschreibungen (`whole_prepare.py`, Prelude)

| Regel | Grund |
|---|---|
| Inline-`asm(...)` und `__asm__(...)` entfallen (Prelude-Makro) | PowerPC-Registerbindung, PPC-Befehle, Abschnittsdirektiven |
| `asm volatile` → `asm` | damit die vorige Regel greift |
| `decl asm("name")` außerhalb von `register` bleibt als `__asm("name")` erhalten | Symbol-Alias: zweiter Name für eine bestehende Funktion/Variable |
| GCC-2.95-Namen in Aliasen → Itanium-Namen (Tabelle `GCC295_SYMBOLS`) | z. B. `FlgOnStatus__5EventUl` → `_ZN5Event11FlgOnStatusEm`; unbekannte brechen den Build ab |
| `extern T x asm("y")` mit `y` in derselben Datei → `static T& x = *(T*)&y` | GCC 2.95 löste den Alias auf die eigene (statische) Variable auf; modernes C++ benennt sie intern um |
| `operator new/delete(unsigned int …)` → `size_t` | C++-ABI |
| `strstr/strchr/strrchr/strpbrk/memchr` → Wrapper mit C-Signatur | C++ deklariert `const`-Überladungen; das Spiel weist an `char*` zu |
| System-C-Header werden vor allen Spielheadern eingebunden | ältere Eigen-Deklarationen (`memcpy` in `joy.h`) sind danach verträglich |
| `types.h`: auf ILP32 bleiben `s32/u32` = `long` wie im Original | gleiche C++-Namen für `u32` und `unsigned long` (sonst 90 Scheinfehler beim Linken) |
| `TaskFunc`: Klasse statt `void*` (`re4_task_func.h`, gleiche Größe) | GCC 2.95 wandelte Funktionszeiger still in `void*` um, modernes C++ nicht |
| `(u16*) &pG->stage_no` → `(u16*) &pG->room_id`, `stage_prev` → `room_id_prev` (`ROOM_ID_ADDR`) | `G_ROOM_ID` und `RsfCheck(*(u16*) &pG->stage_no, …)` lesen die Raumnummer über die Adresse ihres ersten Bytes; auf little-endian liegt `stage_no` nach Patch 0016 am anderen Ende (auf der GameCube dieselbe Adresse) |
| `new (p) T()` → `new (p) T` (`PLACEMENT_VALUE_INIT`) | C++11 füllt eine Klasse ohne eigenen Konstruktor bei `T()` erst mit Nullen; der GameCube-Compiler nicht. `em.cpp` setzt `subArc` vor `new (em) cEm10()` (Absturz in `Em12Set`, 14.10.2026). Prüfung: `port/tools/whole_value_init.py` |
| Prelude: `#define _ctype_ re4_newlib_ctype_` (nach den System-Headern) | Bionic exportiert `_ctype_` als Zeiger, newlib (das Spiel, `t_movie`) als Feld; die Tabelle heißt jetzt `re4_newlib_ctype_` (`runtime/newlib_ctype.c`), Bionics Zeiger bleibt unberührt (K2) |
| Prelude: `Re4BeBytes2` | Byte-Array-Sicht `b[0]` = hohes, `b[1]` = niedriges Byte eines Halbworts, unabhängig von der Host-Reihenfolge (Patch 0016: `cItem::lv8`, `SND_RND::b`) |
| `GXWGFifo.T = x` / `GXWGFifo->T = x` → `re4_gx_put_T(x)` (`GX_FIFO_STORE`, in `GXVert.h` als `re4_gx_put_##T`), Datei bekommt `#include "re4_gx_fifo.h"` | Die Schreibadresse `0xCC008000` des Grafikprozessors gibt es im Port nicht; der FIFO des Ports (`runtime/gx_fifo.cpp`) bekommt jeden Wert (G1) |

Compiler-Schalter: `-std=gnu++11 -Wno-c++11-narrowing -fdelayed-template-parsing
-fno-strict-aliasing -ffp-contract=off -fwrapv -fno-strict-float-cast-overflow -w`.
C++11 wird gebraucht, damit `TaskFunc` trotz Konstruktoren in Unions stehen darf.
`-fno-strict-float-cast-overflow` (seit 15.10.2026): ProDG wandelt mit
`-mfast-cast` Kommazahlen in u8/s8/u16/s16 über `psq_st` mit GQR2–5 um, und
diese Umwandlung **begrenzt** auf den Wertebereich (300 → 255, −4 → 0). clang
ließ überlaufen (300 → 44) bzw. reichte auf ARM den unbegrenzten Wert im
Register weiter; mit dem Schalter begrenzt clang ebenso (Laufzeittest `math`).
Bei u32/s32 bleibt ein Unterschied: ARM begrenzt negative Werte für u32 auf 0,
die GameCube ließ sie überlaufen (schon vorher so, selten).

## Patches (`port/whole/patches/`)

| Patch | Grund |
|---|---|
| 0001 `scheduler.h` | `TaskFunc` aus `re4_task_func.h` |
| 0002 `dbg_var.h` | `this->` für Member der abhängigen Basisklasse (GCC 2.95 suchte dort ohne) |
| 0003 `mes.h` | `lineSpace` öffentlich; GCC 2.95 prüfte den Zugriff nicht |
| 0004 `esp0a.cpp` | Rückruf-Cast für `AddOtWorldPos` |
| 0005 `db_light.cpp` | `DrawTileV`: Alias setzte PPC-Strukturübergabe per Referenz voraus, jetzt Wrapper |
| 0006 `r222.cpp`, 0007 `r224.cpp` | Konstante aus Top-Level-asm (`.rodata`-Wort) als `static const f32` |
| 0008 `main.cpp` | `main()` heißt `extern "C" re4_game_main()`; das Startprogramm (Linux, Android) ruft es auf |
| 0009 `r406.cpp`, 0010 `t_block.cpp` | Hilfsfunktion `static`: im selben Modul hat eine zweite Datei eine gleichnamige (laut Original-Symboltabelle zwei Kopien) |
| 0011 `dvd.cpp` | `cDvd::ReadCheck` (4 Argumente) hat kein `return`; GCC 2.95 ließ das Ergebnis von `readCheckMain` in r3, die Aufrufer prüfen es (`< 0` = Fehler) |
| 0012 `main.cpp` | Die zwei Leerlauf-Schleifen auf `vsync_cnt` rufen `re4_wait_interrupt()`: dort wird der VI-Interrupt zugestellt (sonst Endlosschleife) |
| 0013 `dvd.cpp` | Die 1-KB-Teiletabelle (`DvdHeader`, nur 32-bit-Felder) wird direkt nach dem Lesen in Host-Byte-Reihenfolge gedreht |
| 0014 elf Dateien | Module rufen/lesen `static`-Symbole von `main.dol` beim Namen (der SN-REL-Linker löste sie auf): `EmCatchSubSet`, `QuakeKill`, `setRoomEtcBreakDisp`, `Em32_fs_tbl`, `SceAtSys`, `ScrIdRefTbl`, `ScrObjIdNum`, `fontTMtx`, `g_EspToolSeqHedAddr` werden nicht-statisch; `cModel::isTrans`, `cPlayer::subCharLiveCheck` ohne `inline`; `cDbgWindow::Init` wird in `db_toolbase.cpp` erzwungen ausgegeben |
| 0015 `dvd.cpp` | Ruft `re4_formats_part_loaded` (Formatumwandler) am Ende jedes fertig gelesenen Dateiteils der Lade-Warteschlange (`cDvdQueue::readMain`) |
| 0016 elf Header | Byte-Sichten in Unions für little-endian umgedreht (`#if __BYTE_ORDER__`): `EspGenPrm` (h/b), `cAnm::Frames`, `cEffectCore::Call_no` (esp.h), `SstList::no` (espgen.h), `CRoomInfo::roomNo` (room_jmp.h), `RoomNo_next`/`room_id`/`room_id_prev` (global.h), Teile-Wort (model.h), `ParentNo` (light.h), `no`/`type` (etc_model.h), `save` (sscrn.h), `m_line_gap` (mes.h), `ROOM_SAVE_DATA::RoomNo` (room_data.h); Array-Sichten über `Re4BeBytes2`: `cItem::lv8` (item.h), `SND_RND::b` (snd_drv.h) |
| 0017 `room_jmp.cpp` | `getIndexNum` liest das niedrige Byte des u32-Raumzählers über seinen Wert statt über Offset 3 |
| 0018 `motion.cpp`, `r117.cpp` | Lokale Unions: `FccF32`/`FccS16` setzen Floats aus big-endian Byteströmen zusammen (Bytereihenfolge für little-endian umgedreht), `R117Work::stepMode` |
| 0019 `dvd.cpp` | `cDvd::Init` kopiert alle `FileTbl`-Namen in beschreibbaren Speicher (`strdup`): `read.cpp` benennt Einträge an Ort und Stelle um (`SET_DRS_NAME`: `.das` → `.drs`), auf der GameCube lagen Stringliterale in beschreibbarem RAM |
| 0020 `math_sub.h`, `math_sub.cpp` | C-Fassungen von `fabsf`, `SQRTF`, `SINF`, `COSF`, `LIMIT_ANGLE` (siehe M4.5) |
| 0021 `trans.cpp`, `shape.cpp`, `dbmodule.cpp`, `Espgen42.cpp`, `espgen45.cpp`, `pendulum.cpp` | Paired-Single-Laden/Speichern mit GQR-Quantisierung und Skinning `CalcSk1_x/x2` in C, Locked Cache als Host-Puffer |
| 0022 `dbg_tool.h` | `asm("li %0,0")` → `z = 0` |
| 0023 `em.h` | `EmRoutineSetW` setzt die vier Zustandsbytes einzeln statt über ein u32 |
| 0024 `pl_sub.cpp` | Bit 0x40 im ersten Byte von `cMotBase::pMod` (ungecachter Spiegel der GameCube) entfällt; auf little-endian träfe es ein anderes Objekt |
| 0025 `read.cpp` | `GetDataExt` meldet jeden Fund an `re4_formats_get_data` (Abdeckungs-Log) |
| 0026 `title.cpp` | Debug-Start: `RE4_START_ROOM` wählt den Raum im Debug-Startmenü vor, Titelmenü auf „Neues Spiel“ |
| 0028 `dvd.cpp` | `MemorySwap` meldet am Ende den getauschten Bereich (`re4_formats_memory_swapped`): Ereignisse leihen sich so den Archivspeicher eines Gegners, Untermenüs den Heap; die Herkunftsangaben der Brücke wandern mit |
| 0029 `scroll.cpp` | `setObj` meldet für jedes Kulissen-Objekt, welchen SMD-Eintrag (Raum oder Block, Nummer) es zeigt (`re4_scroll_placed`): Die Brücke gibt das weiter, die App lässt versteckte und nicht geladene Kulisse weg, verschiebt bewegte und zeichnet Kulisse aus Raumskripten (r100: das Polizeiauto) wie die übrigen Modelle |
| 0027 `model.cpp`, `objRocket.cpp` | Mit `-O2` (Android) gibt clang Inline-Funktionen, die nur in einer `.cpp` definiert sind, nicht aus; Module brauchen sie: `cModel::getPartsPtr` ohne `inline`, `cManager<cObj>::destroyNow` ausdrücklich instanziiert. Ohne den Patch zwei Platzhalter mehr als bei `-O0` |
| 0030 `GXInit.c`, `GXTransform.c`, `GXLight.c`, `GXMisc.c` (SDK) | GX-Bibliothek im Port (G1): Register-Blöcke über `re4_gx_register_block` statt fester Hardware-Adressen, `IsWriteGatherBufferEmpty` ohne Assembler (immer leer), Matrix-/Projektions-/Licht-Ladefunktionen in C statt Paired-Single-Assembler (schreiben über `re4_gx_put_f32` in den FIFO), `GXPeek*`/`GXPoke*` über `re4_gx_efb_peek` statt EFB-Adresse. Dazu `-include re4_sdk_intrinsics.h` (`__rlwimi`, `__cntlzw`) für die SDK-Dateien und `-std=c99` für `GXDraw.c` |
| 0031 `GXAttr.c`, `GXTexture.c` (SDK) | Zeiger an den Grafikprozessor über `re4_gx_physical` statt Maskieren (`GXSetArray`, `GXInitTexObj`, `GXInitTexObjData`, `GXInitTlutObj`): MEM1 wie auf der GameCube, Speicher des Ports außerhalb von MEM1 (statische Felder, `filter02.cpp CLUTdata`) über ein Fenster ab `0x03000000` (G2) |
| 0032 `eff_sys.cpp`, `Espgen42.cpp`, `Espgen43.cpp`, `espgen45.cpp` | Display-Listen, die das Spiel mit CPU-Schreibzugriffen baut (Anzahl, Indizes, Floats in Host-Reihenfolge), werden vor `DCStoreRange` mit `re4_gx_host_order_list` angemeldet; der Decoder liest sie in Host-Reihenfolge, solange ihr Inhalt gleich bleibt (G2) |
| 0033 `GXTev.c`, `GXPixel.c`, `GXLight.c` (SDK) | Farben (`GXColor`) als Wort mit R im höchsten Byte zusammensetzen statt `*(u32*)&color` (TEV-, Konstant-, S10-, Material-/Umgebungs-, Nebel- und Lichtfarbe); little-endian vertauschte sonst die Kanäle |
| 0034 `card.cpp`, `tv_mode.cpp`, `event.cpp` | Ein-/Ausblendfarben, die als `u32` an `FadeSet` gehen, in GameCube-Byte-Reihenfolge schreiben (`RE4_BE`) |
| 0035 `sce_sys.cpp` | `SceSleep` außerhalb einer Szenario-Aufgabe wartet auf das nächste Halbbild (`re4_wait_interrupt`): Warteschleifen wie `SndStrStopBlock` (Ende des Funkspruchs) kommen sonst nie voran, weil der Ton-Interrupt nur an Unterbrechungspunkten läuft |
| 0036 `GXTev.c` | Die Tabellen von `GXSetTevOp` (`TEVCOpTableST0/1`, `TEVAOpTableST0/1`) waren Bitfeld-Strukturen, die als ganze Wörter gelesen werden; MWCC (Big-Endian) belegt Bitfelder vom höchsten Bit an, Compiler für Little-Endian vom niedrigsten. Die Wörter werden jetzt Feld für Feld gebaut (`RE4_TEVC_WORD`, `RE4_TEVA_WORD`), Felder und Werte wie im Original |
| 0038 `GXFifo.h` | `GXFifoObj` (ein Byte-Feld, das das SDK als `__GXFifoObj` mit Zeigern und Zählern beschreibt) ist 32-Byte-ausgerichtet; vorher lag das statische `FifoObj` von `GXInit` im Android-Build an einer ungeraden Adresse, und die zusammengefassten Wort-Speicherbefehle (`strd`) brachen auf dem Retroid mit SIGBUS ab |
| 0037 `main_mem.h`, `pl_class.cpp` | `VALID_PTR`/`VALID_PTR2` (Zeigerprüfung gegen MEM1) nehmen zusätzlich `re4_valid_port_ptr` (runtime/os_thread.cpp): Host-Stapel der OS-Threads und die statischen Daten der Spielbibliotheken (`libre4_*`), die auf dem GameCube ebenfalls in MEM1 lägen |
| 0039 `card.cpp` | `CardFirstCheck` überspringt die Speicherkarten-Prüfung beim Start wie nach einem Reset, solange der Port keine Speicherkarte hat (`re4_skip_card_check`, runtime/card.cpp; `RE4_CARD_CHECK=1` behält sie für die Test-Tastenfolgen) |
| 0040 `esp_sub.cpp` | `Esp1b_SpTrans` findet die Daten von `cEsp1b` über `sizeof(cEsp)` statt über `&pad_EC[0xC]`: MWCC legt den vptr ans Ende von `cEsp`, die Compiler des Ports an den Anfang; vorher las es die Punktzahl 4 Bytes daneben (Absturz im GX-Decoder auf dem Retroid nach 153 s) |
| 0041 `at_sub.cpp`, `atari.cpp`, `card.cpp` | Das Attributwort einer Kollisionsfläche (`SAT_POLY::m_Status`) wird als Wort gelesen statt als zwei Halbwörter in GameCube-Reihenfolge (vorher vertauschte Hälften: Zäune als Fallkanten, Wände ohne Kollision, Boden als Wasser); `CardID::save` liest `nPoint` als Wert statt Byte 3 |
| 0042 `src/lib/GXGeometry.c` | `GXSetCullMode` übernimmt das Ergebnis von `__rlwimi` (`hwMode = __rlwimi(...)`): MWCC fügt bei diesem Befehl in den ersten Operanden ein, die C-Fassung des Ports gibt das Ergebnis zurück und die Zeile verwarf es. Aus `GX_CULL_FRONT` (Standard aller Modelle) wurde „keine Rückseiten weglassen“: alles wurde beidseitig gezeichnet, die Doppelschichten von Leons Haar verdeckten sich gegenseitig (dunkle Flecken) |
| 0043 `title.cpp` | Das Debug-Startmenü nach „New Game“ (`titleExit`, Raumauswahl) meldet sich bei der Laufzeit an (`re4_title_room_menu`, runtime/dolphin_os.cpp): solange es offen ist, lässt die Debug-Anzeigen-Sperre der App (`RE4_DEBUG_HUD=0`) `debug_mode` mindestens 1, damit seine Zeilen (Debug-Text) zu sehen sind; `RE4_ROOM_MENU=0` überspringt es wie ein sofortiges A |

Neuen Patch erzeugen: Datei aus `whole_prepare.rewrite()` kopieren, ändern,
`port/tools/whole_make_patch.py NAME pfad/im/repo kopie` ausführen. Mehrere
Patches dürfen dieselbe Datei ändern, solange sich ihre Stellen nicht
überschneiden (`patch` verschiebt die Zeilennummern).

Compiler-Schalter zusätzlich seit M4: `-fno-strict-return`. Fünf
Originalfunktionen fallen ohne `return` aus einer Nicht-void-Funktion
(`cDvd::ReadCheck` → Patch 0011, `cItemMgr::available`, `MercSysSetAddTime`,
eine em30- und eine emwindow-Funktion, deren Rückgabewert niemand nutzt). Ohne
den Schalter setzt clang dort einen Absturzbefehl.

**SDK-Auszug (`whole_prepare.py`):** Aus den Mathe-Dateien `mtx`, `mtxvec`,
`mtx44`, `mtx44vec`, `vec`, `quat` von `src/lib` werden nur die portablen
C-Funktionen (`C_*`) nach `prepared/sdk_c/` übernommen; die
Paired-Single-Assembler-Fassungen (`PS*`) fallen weg und `runtime/mtx_ps.c`
leitet die `PS*`-Namen auf die C-Fassungen um.

## Linken und Module (M3)

**Module als eigene Bibliotheken** (Entscheidung des Projektinhabers,
13.10.2026). Jedes Modul wird mit `RTLD_LOCAL` geladen und mit `-Bsymbolic`
gelinkt. So behält es wie auf der GameCube seine eigenen Kopien doppelter
Dateien (286 Symbole sind in mehreren Modulen definiert). Modul-IDs aus
`rel.json` stehen in der erzeugten Tabelle `kRe4Modules`.

**Aufrufe zwischen Modulen:** 24 Module rufen zusammen 893 Funktionen in
Nachbarmodulen auf. Das betrifft vor allem die Stufenmodule `st1_*`, `st2_*`
und `st3_*`, die gegenseitig ihre Räume registrieren, die Debug-Werkzeuge
(`Tools`, `t_*`) und einzelne Gegner/Waffen (`em27`, `em3e`, `wep17`). Diese
Abhängigkeiten sind zyklisch. Die GameCube löste sie beim Linken jedes Moduls
nach. ELF-Bibliotheken können Zyklen nicht vorab auflösen (Android kennt kein
„lazy binding“). Deshalb bekommt jedes Modul erzeugte Weiterleitungen
(`generated/imports/<modul>.S`, Assembler für i686 und armv7). Sie suchen das
Ziel beim ersten Aufruf in den geladenen Modulen und springen mit unveränderten
Argumenten weiter. Für ARM64 (M12) braucht es eine dritte Assembler-Vorlage.

**Einzige Datenreferenz zwischen Modulen:** `Tools` liest `DbgFileList` aus
`t_event`. Das Debug-Werkzeug bekommt eine eigene, leere Kopie. Das ist nur
für das Debug-Menü relevant.

**Platzhalter (`generated/stubs.cpp`):** Ein Probe-Link aller Objekte gegen
die C/C++-Laufzeit des Zielsystems ermittelt, was niemand liefert. Funktionen
schreiben beim ersten Aufruf `stub called: <Name>` ins Log und geben 0 zurück.
Datenobjekte sind genullt; Größe aus `config/G4BE08/symbols.txt`, sonst 4 KB.

| Platzhalter | M3 | M4 Teil A | M4 Teil B |
|---|---|---|---|
| Funktionen | 363 | 234: GX 97 (M6), AX/AXFX/AXART/MIX/SYN/SEQ/AI/ADX 77 (M8), CARD 17 (M10), mwPly 12 (M9), PAD 7 (M7), PPC-Leistungszähler 8, SN-PC-Host-I/O 7 (nur Entwicklerkonsole), OS 8 (IPL-Schrift `OSInitFont`/`OSGetFontEncode`/`OSGetFontTexture`, Stoppuhren, `OSResetSystem`), `TEXGet` | 209: wie Teil A ohne CARD, PAD und `TEXGet` (ersetzt durch `card.cpp`, `pad.cpp`, `tex_palette.cpp`) |
| Daten | 9 | 1: `GXWGFifo` (GX-Schreib-FIFO, `mes.cpp` schreibt Eckpunkte direkt hinein; M6) | 1: unverändert |

Seit G1 (15.10.2026): 76 Funktionen (Ton 59 für M8: ADX, AI, AXART, AXFX, MIX, SEQ, SYN, `AXSetCompressor`; OS 7: IPL-Schrift, Stoppuhren, `OSResetSystem`; SN-PC-Host-I/O 7; PPC-Leistungszähler 7), 0 Daten. GX, `mwPly` (`movie.cpp`) und die übrigen OS-Funktionen sind ersetzt.

**Echter Ersatz statt Platzhalter** (`runtime/sn_runtime.cpp`): `_ctors`/`_dtors`
(leer, weil der ELF-Lader Konstruktoren selbst ausführt), `__builtin_new`/
`__builtin_delete` (GCC-2.95-Namen von `operator new/delete`),
`memset_asm`/`memclr_asm`.

## Plattformschicht (M4 Teil A)

**Hauptspeicher an der Originaladresse** (`runtime/mem1.cpp`). Das Spiel
rechnet mit der GameCube-Speicherkarte: `SystemMemInit` legt die Heaps über die
festen Adressen `0x80974000–0x817F4000`, `read.cpp` lädt Kern-, Spieler- und
Waffenarchive an feste Adressen, `OS_BUS_CLOCK` liest `0x800000F8`,
`OSPhysicalToCached` addiert `0x80000000`, und rund 60 Stellen prüfen Zeiger
auf `0x80000000–0x82FFFFFF` (teils, um Offsets von Zeigern zu unterscheiden).
Darum wird `0x80000000–0x83000000` (48 MB wie eine Entwicklerkonsole) per
`mmap` genau dort reserviert, und die Startwerte des IPL stehen in den
OS-Globals (Disc-ID, Speichergröße 24 MB, Konsolentyp Retail, Arena, FST,
Takte). Ohne diese Adresse läuft der Port nicht (Abbruch mit Logmeldung).
Statische Objekte in den Bibliotheken liegen außerhalb dieses Bereichs; Prüfungen,
die sie treffen, müssen einzeln behandelt werden (bisher keine aufgetreten).

| Datei | Ersetzt |
|---|---|
| `dolphin_os.cpp` | `OSInit`, `OSReport` (zeilenweise ins Log), `OSPanic`, Konsolentyp/Speichergröße, `OSGetTime`/`OSGetTick` (40,5 MHz, Epoche 2000), Kalenderzeit, Interrupt-Sperren, Caches (no-op); seit G1 Unterbrechungs-Rückrufe des SDK (`__OSSetInterruptHandler`, `re4_os_hw_interrupt`), Kontexte, Reset-Funktionen, Versionsnamen und die Gekko-Register `HID2`/`WPAR` als No-op |
| SDK `OSAlloc.c`, `OSArena.c` | unverändert aus `src/lib`: Heap und Arena; `main_mem.cpp` liest die Heap-Deskriptoren direkt |
| `os_thread.cpp` | `OSThread`, `OSSemaphore`: ein Host-Thread, strikte Prioritäten, Wechsel nur in OS-Aufrufen; jeder Thread hat einen Host-Stack (`port/src/stack_context.cpp` aus dem alten Port, neu mit ucontext für Linux x86, Boost.Context-Assembler für Linux ARM wie auf Android); ein Verteiler auf dem Stack des Startprogramms (`re4_os_run`). Spielsichtbar wie im SDK: `state`, `suspend`, Priorität, Warteschlangen, Stack-Magie `0xDEADBABE` |
| `vi.cpp` | VI-Retrace alle 1/59,94 s Echtzeit, zugestellt an Interrupt-Punkten (Verteiler im Leerlauf, `VIWaitForRetrace`, Patch 0012, später lange Hintergrundarbeit); Thread-Wechsel aus dem Rückruf erst am Interrupt-Ende. `RE4_VI_FAST=1`: ohne Warten (Tests) |
| `ax.cpp` (K2) | AX ohne Tonausgabe: 64 Stimmen mit den Vergaberegeln des SDK (`AXAlloc.c`), die Setter (`AXVPB.c`), Abspielposition je Audio-Rahmen nach Abtastrate (ADPCM mit Rahmenköpfen, PCM16, PCM8; Schleife oder Stopp am Ende), der Audio-Rahmen-Rückruf des Spiels 200-mal je Sekunde Spielzeit, zugestellt mit den VI-Retraces. Damit läuft der Tontreiber (`snd_main.cpp cb_audio_frame`): Stimmen werden frei, Sprach-Streams werden bereit. MIX, SYN, SEQ, AXFX, AI bleiben Platzhalter. Hörbarer Ton: M8 |
| `movie.cpp` (K2) | Sofdec-Filme (`mwPly*`) bis M9: jeder Film endet sofort (PLAYEND, kein Bild), das Spiel macht danach weiter (r120: Autofahrt nach dem Vorfilm) |
| `gx_fifo.cpp` (G1) | GX-FIFO und Register-Blöcke CP/PE/PI/MEM: Display-Listen aufnehmen, PE-Token/fertig als Unterbrechung (`re4_gx_deliver_interrupts`), Speicher wie ihn der Grafikprozessor sieht (MEM1 und Fenster für Port-Speicher, `re4_gx_physical`), EFB-Lesen antwortet „leer“ |
| `gx_decode.cpp` (G2) | Befehlsdecoder: Zustand CP/XF/BP/TLUT, Punkte, Indizes, Texturen mit Inhalts-Schlüssel, Bildpuffer-Kopien, Bildstücke mit Warteschlange für den Renderer (`re4_gx_frame.h`, `re4_core_gx_acquire/release/detach`) |
| SDK `GX*.c` (G1) | unverändert aus `src/lib` bis auf Patch 0030: die gesamte GX-Bibliothek |
| `dvd.cpp` | `DVD*` über einen Ordner mit entpackten Disc-Dateien (siehe unten); Lesen sofort, Rückruf vor der Rückkehr; aus einem Interrupt heraus (Audio-Rahmen: Nachladen der Sprach-/Musik-Streams) bleibt der Auftrag „beschäftigt“, bis der Interrupt-Rückruf zurückgekehrt ist (`os_thread.cpp re4_os_defer`), Abbruch liefert dann −3; FST der eingelegten Disc oben in MEM1, Disc-ID bei `0x80000000`; Disc-Wechsel lädt den anderen Ordner |
| `aram.cpp` | ARAM (16 MB) als Host-Puffer, `ARInit`/`ARAlloc` (Stapel ab `0x4000`), ARQ kopiert sofort und ruft den Rückruf (aus einem Interrupt heraus erst nach dessen Ende, wie `dvd.cpp`) |
| `os_link.cpp` | `OSLink`/`OSUnlink` (siehe unten) |
| `yz2_decode.cpp` + `port/src/yz2_kernel.cpp` | `yz2Decode_Decode`: der geprüfte Kern des alten Ports unverändert, ein Adapter verbindet ihn mit `yz2code.cpp` des Spiels |
| `mtx_ps.c` + `sdk_c/` | Matrix-/Vektor-/Quaternion-Mathematik (C-Fassungen des SDK) |
| `byte_order.cpp` | Byte-Tausch-Helfer für Format-Umwandler |
| `newlib_ctype.c` | newlib-Zeichentabelle `_ctype_` (heißt seit K2 `re4_newlib_ctype_`, Prelude-Regel) |
| `card.cpp` (M4 B) | Speicherkarte: beide Schächte leer, jeder `CARD*`-Aufruf liefert `CARD_RESULT_NOCARD` (wie das SDK bei leerem Schacht); das Spiel zeigt „keine Speicherkarte“ und läuft ohne Speichern weiter. Echtes Speichern: M10 |
| `pad.cpp` (M4 B) | Controller: Kanal 0 angeschlossen, Kanäle 1–3 leer (ein zweiter Controller ließe den Titel die Logos überspringen). Tasten aus der Testfolge `RE4_PAD_SCRIPT` (`TASTEN@BILD` drückt 3 Bilder lang, `TASTEN@VON-BIS/ALLE` wiederholt; Tasten `A B X Y Z L R START UP DOWN LEFT RIGHT`, mit `+` verbunden; Bild = Zahl der `PADRead`-Aufrufe). Seit K2 zusätzlich `re4_pad_live` (Controller der App, sobald kein Testskript läuft) und Bildhaken `re4_add_frame_hook` (bis 4, je `PADRead`). Feinschliff der Eingabe: M7 |
| `core_api.cpp` (K2) | Einbettungs-Schnittstelle `re4_core.h` für die App: Kern-Thread, Status, Controller, Dreifachpuffer der Schnappschüsse; Testausgaben `RE4_BRIDGE_DUMP`, `RE4_BRIDGE_SNAPSHOT`, `RE4_GX_DUMP` (GX-Bildstücke als Dateien, `re4_gx_frame_io.h`) |
| `core_bridge.cpp` (K2) | Schnappschuss des Spielzustands (Kamera, Raum, Modelle aus EmMgr/ObjMgr mit Teile-Matrizen und Disc-Herkunft); mit den Schaltern des Spielcodes gebaut |
| `log.cpp` | Log (Linux: stderr, Android: logcat-Tag `re4`), seit K2 mit den letzten 24 Zeilen für das Overlay der App (`re4_log_tail`) |
| `tex_palette.cpp` (M4 B) | `TEXGet` des SDK (`src/lib/texPalette.c` braucht sonst den SDK-Dateicache) |
| `mem1.cpp` (M4 B) | zusätzlich `re4_check_game_address`: bricht mit Logmeldung ab, wenn `libre4_main` oder ein Modul unter `0x80000000` geladen wurde |

**Disc-Ordner** (`re4_host --data DIR`, sonst `$RE4_DATA_DIR`, sonst
`<re4_host>/data`): `DIR/disc0` und `DIR/disc1` mit `sys/boot.bin`,
`sys/fst.bin` und `files/…`, genau wie Dolphins „Gesamte Disc extrahieren“.
`port/tools/whole_disc_folder.py` legt ihn an: aus den Disc-Abbildern
(`--disc d1.iso --disc d2.iso --out DIR`, mit dem geprüften Disc-Leser
`original_import/gcm.py` des alten Imports) oder aus dem privaten Speicher von
`import_original.py` (`--store STORE --out DIR`, jede Datei gegen ihre SHA-256
geprüft); `--check DIR` prüft einen vorhandenen Ordner. Ziel immer außerhalb
des Repositorys.

**OSLink/OSUnlink:** Der REL-Kopf kommt big-endian von der Disc;
`re4_rel_header_to_host` dreht ihn an Ort und Stelle (erkennbar an der
Versionsnummer, wiederholbar). Dann Modul-ID → `re4_module_name` →
`re4_module_load`, und `_prolog`/`_epilog`/`_unresolved` der Bibliothek werden
in den Kopf geschrieben (`DLL_PROLOG` ruft sie). Zustand wie auf der GameCube:
Wird derselbe Kopf nach `OSUnlink` wieder gelinkt (Raum pausiert/fortgesetzt,
`cRoomData::stopRelData`/`restartRelData`), bleibt der Modulzustand erhalten; ein
neu von Disc gelesenes REL startet mit dem Anfangszustand der Bibliothek (sie
wird neu geladen). `OSUnlink` führt die statischen Destruktoren des Moduls aus
(das Original-Epilog tat das über `_dtors`; betrifft `Sscrn` und Debug-Werkzeuge).
Beim Entladen eines Moduls lösen die Weiterleitungen der anderen neu auf.
Bekannte Abweichung: Lädt das Spiel dasselbe Modul zweimal gleichzeitig, teilen
sich beide eine Bibliothek (Logmeldung).

## Formatumwandler (M4 Teil B)

**Entscheidung des Projektinhabers (13.10.2026): Umwandlung einmal beim
Laden, an einer zentralen Stelle.** Disc-Daten sind big-endian, der
Originalcode liest sie als Strukturen. Jede Datei wird deshalb unmittelbar
nach dem Laden Feld für Feld in Host-Reihenfolge gedreht:

- `re4_formats_part_loaded` (Patch 0015): am Ende jedes fertig gelesenen
  Dateiteils der Lade-Warteschlange (`cDvdQueue::readMain`), mit Dateiname,
  Verschachtelungstiefe, Teilnummer, `DvdHeader` (Typ, Größe) und Adresse
  (MRAM oder ARAM-Puffer). Nur die Tondaten-Streams (`snd_str*.cpp`) lesen
  an der Warteschlange vorbei (Audio, M8).
- `re4_formats_unpacked` (`yz2_decode.cpp`): nach dem Yz2-Entpacken. Einziger
  Nutzer ist `ReadAreaData`, das Ergebnis ist ein Raumarchiv.
- Auswahl: zuerst nach `DvdHeader`-Typ (Typ 1 Tonbank, Typ 2 Tonproben), dann
  nach Dateiname (`fnmatch`), in Archiven nach dem 3-Zeichen-Kürzel jedes
  Eintrags. Unbekanntes wird einmal geloggt
  (`formats: … no converter, left big-endian`) und bleibt unverändert.
- Die Umwandler werden mit den Schaltern und Headern des Spielcodes
  übersetzt (`re4_formats_obj`, Teil von `libre4_platform`): Feldlisten und
  Größen kommen aus den Originalstrukturen (`RE4_SWAP(blob, base, T, feld)`,
  `static_assert` auf die Strukturgrößen). Jeder Zugriff wird gegen die
  Blockgröße geprüft; passt eine Struktur nicht, bricht der Umwandler mit
  Logmeldung ab, statt fremden Speicher zu verändern. Gemeinsam genutzte
  Unterstrukturen werden nur einmal gedreht (`Seen`).
- Big-endian bleibt, was nur Grafik- oder Tonhardware liest: Texel und
  CLUT-Einträge von TPLs, Display-Listen, Tonproben. Darum kümmern sich
  Renderer (M6) und Audio (M8).

| Datei / Kürzel | Format (Quelle im Originalcode) | Umwandler |
|---|---|---|
| `bgm/bio4str.hed` | Stream-Blöcke: Offsets, `SND_RIT`, `SND_SHD` (snd_sub3.cpp) | `formats_snd.cpp` |
| `bgm/bio4midi.hed` | u32-Offsets der BGM-Dateien | `convertWords` |
| `bgm/doorse.hed` | `SndDoorTbl`, u32-Tabelle, u16-Zähler (snd.cpp) | `formats_snd.cpp` |
| `bgm/bgmtbl.dat` | `SndBgmTbl`, `SndBgmRoom`, u16-Raumliste | `formats_snd.cpp` |
| Teiltyp 1 (Tonbank) | Kopf, ISS-Block, `SND_SIT`, Wavetable (`WTFILEHEADER`, `WTREGION`, `WTART`, `WTSAMPLE`, `WTADPCM`), Sequenz-Tabelle (Sequenzen selbst sind Byteströme), Zufalls-Tabelle der Geräusche (Kopfwort 1, `seRandomCheck`: nur die Gruppen, die die SITs nennen; die Wavetable endet vor ihr) | `formats_snd.cpp` |
| `etc/sizetbl.dat` | Zähler + Name/Größe (das Spiel liest es nicht) | `formats.cpp` |
| `*.tpl`, `TPL` | `TEXPalette`, `TEXDescriptor`, `TEXHeader`, `CLUTHeader` (tpl.h) | `formats_tex.cpp` |
| `font/*.fnt` | Kopf (TPL-Offset, Breitentabelle in Bytes) + TPL (mes.cpp) | `formats_tex.cpp` |
| `MDT` | Sprachblöcke `MesTblBlock` + u16-Nachrichtencodes (mes.cpp) | `formats_mes.cpp` |
| `op/op*.das` | Funkgespräche: Archiv bei +0x400 im Teil (ss_term.cpp `opArc`) | `formats_arc.cpp` `convertCodecArchive` |
| `*.das`, `*.drs`, `ss/*.dat`, entpackte Räume | Archiv: Zähler, REL-Offset, Offsets, Kürzel; eingebettetes REL bekommt `re4_rel_header_to_host` (read.cpp `GetDataExt`). Ein Eintrag mit demselben Offset wie ein späterer ist leer, die Daten gehören dem letzten | `formats_arc.cpp` |
| `UWF` | `ID_FILE_HEADER`, `ID_DATA_V1/V2`, B-Spline `FUNC_PATH_PTR`, `HERMITE_1_PTR` (id_sys.cpp) | `formats_id.cpp` |
| `EFF` | `ESP_EFF_HEADER` (Version 0xB): Id-Listen, TPL- und `cAnm`-Tabellen, `cEspSeqHead` + `cEspSeqTbl`, Pfade (`Path`), `EffEfmEnt` (eff_sys.cpp) | `formats_eff.cpp` (Effektmodelle, -motionen, Shape-Köpfe noch nicht) |
| `TEX` | `TexData` (Version 3), Id-Liste, TPLs, `cAnm` (texture.cpp) | `formats_eff.cpp` |
| `VIB` | `VibDataTbl`, `VIB_INFO`, `VIB_DATA` (pad.cpp) | `formats_eff.cpp` |
| `debug/roominfo.dat` | Stufen-Offsets, `CRoomInfo` (room_jmp.h) | `formats_dbg.cpp` |
| `BIN` | Modell `cModelData` (Versionen 0x20010801/0x20030817/0x20030818): Kopf, Teile-Datensätze, Positionen/Normalen/Texturkoordinaten (Host), Gewichte `cWeightExt`, Primitiv-Köpfe, Shape-, Blend-, Flip-Tabellen; Display-Listen und Farben bleiben big-endian. Im Kernarchiv auch der Lichtpfad (lightPath.cpp) | `formats_model.cpp` |
| `FCV`, Kurven in EVD/ETM | `MotionData`: Zähler und Bildnummern Host, Schlüsselwerte bleiben big-endian (`FccF32`/`FccS16`, Patch 0018) | `formats_model.cpp` |
| `SEQ` | Sequenztabelle | `formats_model.cpp` |
| `LIT` | `cLit`, `cLightEnv`, `cLightWork` mit Spot-Block und Bewegungsdaten je Id (light01..10) | `formats_lit.cpp` |
| `CAM` | `CAM_FILE_HEADER` B402–B404: `CUT_INFO`, `AREA_DATA` + Eckpunkte, `CAMERA_DATA` + Schlüssel, `LERP_DATA` (B400/B401 und Daten ohne Versionskennung bleiben unverändert, Log) | `formats_event.cpp` |
| `EVD`, `event/evd/*.evd`, `EVS` | `EvtHeader`, Pakete mit Zahlenparametern je Typ, Anhänge nach Verwendung im Paket (Kamera/Motion/Shape-Kurven, LIT, Nebel, Fokus, EFF, MDT, Modelle, Texturen) oder Endung | `formats_event.cpp` |
| `SMD`, `SMX`, `SHD`, `ETM`, `ITM` | Streumodelle mit BIN/TPL/Motion-Tabellen, Objektparameter (Typen 1/2 mit Feldliste), Schatten, Etc-Archiv (benannte Dateien), Item-Modelle | `formats_scene.cpp` |
| `SAT`, `EAT` | `cSatFile`/`cSatHeader`, Vektoren, `SAT_POLY`, Blockkette (andere Versionen als 0x20/0xFF/Blockliste bleiben unverändert, Log) | `formats_scene.cpp` |
| `CNS`, `ETS`, `EMI`, `SAR`, `EAR`, `FSE`, `ESE`, `DSE`, `STB`, `BLK`, `RTP`, `OSD`, `AEV`, `ITA`, `etc/*.esl` | Raumtabellen aus gameRoomInit/ReadAreaData (Quelle je Format im Kopf von `formats_room.cpp`) | `formats_room.cpp` |
| Effektmodelle in `EFF` | BIN/TPL/Motion der Effekte | `formats_eff.cpp` |
| `rel/*.rel` | REL-Kopf | `OSLink` (`re4_rel_header_to_host`) |
| `debug/*.txt` | Text | nichts zu tun |

Bekannte Lücken (werden beim ersten Auftreten geloggt): SMX-Objekttypen
außer 1 und 2 mit nicht leerem Arbeitsbereich, Bereichstypen ohne
Nutzlast-Feldliste (`area type N … payload has no field list`), Kameradateien
B400/B401. Ob „Error: Can't calc Inverse Matrix !“ im Titel verschwunden ist,
zeigt erst der nächste Lauf mit Disc-Daten.

## Byte-Sichten (Endian-abhängiger Spielcode)

Unabhängig von den Disc-Daten liest der Spielcode an manchen Stellen einzelne
Bytes eines größeren Feldes. Auf der GameCube liegt das höchstwertige Byte
vorn, auf ARM/x86 hinten.

- **Unions** mit Byte-Struktur über einem u16/u32 (z. B. `room_id` =
  `stage_no << 8 | room_no`): Patch 0016/0018 dreht die Byte-Reihenfolge der
  Struktur auf little-endian um. Beide Sichten stimmen dann beim Lesen und
  Schreiben, für umgewandelte Disc-Daten wie für Spielvariablen. Gefunden mit
  einem Suchskript über alle Unions in `include/` und `src/` (20 Treffer,
  `gx.h` ist der GX-FIFO und bleibt, `cModelInfo::colorWord` siehe offene
  Punkte).
- **Lesen über Byte-Offsets** (`*((s8*) p + ofs + 3)`): einzeln per Patch
  (0017).
- **Wort-Zugriffe auf Byte-Strukturen** (`*(u32*) &farbe = 0xFF`): seit M4.5
  über `byte_views.json` (85 Stellen, siehe „M4.5“).
- **Schmale Sichten über Zeiger** (`((u16*) poly)[8]`): das Suchskript
  findet nur `&x`; die 21 Stellen mit Zeigervariablen sind von Hand
  durchgesehen (Patch 0041, siehe „Vierter Gerätelauf mit GX“).
- **Byte-Sichten in Disc-Strukturen** muss der Umwandler als Wort drehen
  (Patch 0016 legt sie für Host-Wörter aus): `ModelDataHead::x0` (gefunden
  vom Formatvergleich), `CRoomInfo::roomNo`, `ETS_DATA::no`, `SstList::no`,
  `EspGenPrm`, `cAnm::Frames` sind so umgesetzt.

## Adressen der Spielbibliotheken

**Entscheidung des Projektinhabers (13.10.2026): natürliche Lage + Prüfung.**
Das Spiel unterscheidet an manchen Stellen über Bit 31 einer Adresse eigene
Daten von Offsets und Listenverweisen; so speichern die Zeichenlisten
(`libgpu.cpp` `ClearOTagR`/`DrawOTag`) Verweise mit gelöschtem Bit 31. Die
Listen selbst sind feste Variablen (`MainOt`, `ActBtn`, `SceSys` …) und
liegen in den Bibliotheken. Auf Android liegen die Bibliotheken einer 32-bit-App
bei `0xC0000000` und höher (gemessen, siehe unten), unter i686-Linux bei
`0xF7…`, unter qemu-arm bei `0x40…` (dort stürzte `DrawOTag` ab).

- Der Linux-Vorlader `re4_host` (`host/loader.c`) belegt vor dem Laden jede
  freie Lücke unter `0x83000000` mit einer unzugänglichen Reservierung
  (`PROT_NONE`, `MAP_NORESERVE`); die Bibliotheken landen darüber, auch unter
  qemu-arm (`libre4_host.so` bei `0x83018000`). Den MEM1-Teil übernimmt
  `re4_mem1_init` (Umgebungsvariable `RE4_MEM1_PLACEHOLDER`).
- `re4_check_game_address` prüft `libre4_main` (vor dem Spielstart) und jedes
  Modul (nach `dlopen`) und bricht unterhalb von `0x80000000` mit Logmeldung
  ab. Auf Android muss der JNI-Start dieselbe Prüfung für `libre4_main`
  aufrufen.
- Prüfungen auf den MEM1-Bereich `0x80000000–0x82FFFFFF`, die feste
  Variablen treffen, bleiben Einzelfälle (bisher keiner).

## Adressbereich auf dem Retroid (gemessen 13.10.2026)

Retroid Pocket Flip 2, Android 13, Kernel 4.19 aarch64, `ro.zygote=zygote64_32`,
ABIs `arm64-v8a,armeabi-v7a,armeabi`. Die alte Port-App `org.re4.port` ist als
arm64 installiert. Gemessen wurde deshalb ein echter 32-bit-App-Prozess aus
dem 32-bit-Zygote: `am instrument --abi armeabi-v7a -e debug true
org.re4.port.test/androidx.test.runner.AndroidJUnitRunner` startet
`org.re4.port` als `app_process32` und lässt ihn auf den Debugger warten;
`run-as org.re4.port` liest `/proc/<pid>/maps`; danach `am force-stop
org.re4.port`. Alles unter der ADB-Sperre (`Retroid-Adb-Exclusive.ps1`), nur
eigene Pakete, Seriennummer `dbdcacc`.

| Probe | Abbildungen | Überlappung mit `0x80000000–0x83000000` | Freie Lücke um `0x80000000` | Bibliotheken |
|---|---|---|---|---|
| 1 (PID 18115) | 2150 | keine | `0x7532A000–0xC3B25000` (1255 MB) | `0xD1D25000–0xF07E2000` |
| 2 (PID 18233) | 2151 | keine | gleich | gleich |
| 3 (PID 18289) | 2150 | keine | gleich | gleich |

Weitere Belegung: ART-Hauptbereich `0x12C00000–0x32C00000`, Boot-Image um
`0x70D06000–0x7532A000`, Stack `0xFF3F4000`. Ergebnis: **MEM1 an der
Originaladresse ist in einer 32-bit-App frei**, und die Bibliotheken liegen
oberhalb von `0x80000000`. Die drei Proben stammen aus demselben Zygote-Start
(gleiche Zufallsverteilung); nach einem Geräteneustart verschiebt sich die
Lage, die Lücke von 1,2 GB lässt aber viel Spielraum. Ungeprüft: das
tatsächliche `mmap` in einer Port-App (M6/M11, sobald der NDK-Build läuft).
Der Platz sollte früh reserviert werden (beim Laden von `libre4_platform`).

## Offene Punkte für M5 und später

- **M5 abschließen (lokal):** `port/scripts/test-r100-core.ps1` mit den
  Disc-Daten; Ziel: Formatvergleich ohne Abweichung, 10 000 Bilder ohne
  Absturz, Abdeckung für r100 ohne „nein“. Abweichungen und Abstürze aus dem
  Bericht beheben. **Erreicht im dritten Lauf** (siehe oben); armhf mit
  Disc-Daten (`-Arm`, qemu) steht noch aus.
- **Null-Füllung durch C++11:** `whole_value_init.py` findet jede
  Konstruktion, die noch mit Nullen füllt (mit und ohne Platzierung, auch
  Member-Initialisierer `x()`). Nach Änderungen an `whole_prepare.py` oder
  den Ersatz-Headern erneut laufen lassen; erwartet sind die vier Stellen aus
  dem Kopf des Skripts.
- **Formate ohne App-Leser im Vergleich** (Liste unter „Formatvergleich mit
  der App“): bei Auffälligkeiten im Lauf weitere Leser der App heranziehen
  (`room_blocks.cpp` BLK, `room_shadows.cpp` SHD, `sound_footsteps.cpp` FSE,
  `sound_ambient.cpp` ESE, `room_area_effects.cpp` EAR, `room_items.cpp` ITA/ITM).
- **SQRTF** weicht in den letzten Bits ab (siehe M4.5); falls Kollision oder
  Kamera sichtbar anders reagieren, die `frsqrte`-Tabelle nachbilden.
- **Von der CPU gebaute Display-Listen** (`EspInit`: `*(u16*) d = 4`) sind
  Host-Reihenfolge, Display-Listen von der Disc big-endian: der GX-Ersatz (M6)
  muss beide kennen.
- **Feste Variablen als DVD-Ziel:** `etc/emleon00.esl` wird in einen Puffer in
  `libre4_main` gelesen (`0xF7…`); DVD und Umwandler kommen damit zurecht,
  MEM1-Bereichsprüfungen darauf wären ein Einzelfall.
- **IPL-Schrift:** `OSInitFont` braucht die Schrift aus dem IPL-ROM (nicht auf
  der Disc), nur für die Fehlerbildschirme. Platzhalter liefert 0.
- **Platzhalter (209 Funktionen):** GX 97 (M6), AX/AXFX/AXART/MIX/SYN/SEQ/AI/ADX
  77 (M8), mwPly 12 (M9), PPC-Leistungszähler 8, SN-PC-Host-I/O 7,
  OS 8 (IPL-Schrift, Stoppuhren, `OSResetSystem`). CARD und PAD sind ersetzt.
- **Weitere Leerlauf-Schleifen** auf Interrupt-Zähler wie in `main.cpp` können
  auftauchen; dort ebenfalls `re4_wait_interrupt()` einsetzen.
- **Hintergrund-Task (iTask):** läuft ohne Unterbrechung bis zum Ende. Für
  echtes Zeitscheiben-Verhalten `re4_interrupt_point()` in lange Schleifen
  (z. B. den Yz2-Kern über seinen Checkpoint) einbauen.
- **Android-Einbindung:** erledigt im Cloud-Teil von K2 (siehe dort);
  `_ctype_` über die Prelude-Regel umbenannt. Gerätetest steht aus.

## Bekannte Risiken für die Laufzeit (M4/M5)

- Mit `-w` sind Warnungen ausgeblendet. Bei Abstürzen lohnt ein Lauf ohne `-w`
  für die betroffene Datei.
- Funktionsaliase mit abweichender Signatur (z. B. `EvtFlgOnStatus` →
  `Event::FlgOnStatus`) funktionieren auf ARM32 und i686, weil `this` dort wie
  ein erstes Argument übergeben wird.
- Die Task-Threads bekommen je 1 MB Host-Stack (das Original: 6–12 KB im
  Spiel-Heap), der Boot-Thread 8 MB.
- Lesen von Disc ist sofort fertig: Code, der auf einen späteren Abschluss
  wartet oder den Fortschritt misst, verhält sich anders als mit Laufwerk.
- Der Formatumwandler kennt Formate nur so weit, wie der Spielcode sie beim
  Booten las. Ein falsch gedrehtes Feld fällt erst auf, wenn es gelesen wird;
  bei Abstürzen zuerst die `formats:`-Logzeilen der betroffenen Datei prüfen.
- Unions mit Byte-Sichten in Dateien, die das Suchskript nicht erfasst
  (benannte Strukturtypen als Union-Mitglieder), können fehlen; `EspGenPrm`
  wurde von Hand gefunden.
- Klassen mit virtuellen Funktionen: MWCC legt den vptr hinter die Felder
  der Klasse, in der er entsteht (`cEsp`: 0xF4), die Compiler des Ports an
  den Anfang. Direkte Feldzugriffe stimmen trotzdem; Code, der über feste
  Abstände oder Füllfelder zu anderen Feldern rechnet, nicht (Patch 0040).
  `sizeof` bleibt gleich, wenn der vptr in die Klasse fällt.
- PowerPC-Befehle als Funktionen (`re4_sdk_intrinsics.h`): MWCC schreibt bei
  `__rlwimi` in den ersten Operanden, die C-Fassung gibt das Ergebnis nur
  zurück. Ein Aufruf als eigene Anweisung (Ergebnis nicht zugewiesen) wirkt im
  Port nicht. In `src/` gab es genau eine solche Stelle (Patch 0042).
