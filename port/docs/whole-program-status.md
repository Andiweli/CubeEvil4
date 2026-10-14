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
| `--runtime-test` | Plattformschicht ohne Disc-Daten (`port/whole/tests/runtime_tests.cpp`): `link` OSLink/OSUnlink mit synthetischem big-endian REL-Kopf und Testmodul; `dvd` DVD-API über einen synthetischen Zwei-Disc-Ordner; `yz2` Yz2 des Spiels + Kern mit den Testdaten `port/tests/yz2_fixtures.h`; `dvdq` die DVD-Warteschlange des Spiels liest eine synthetische mehrteilige Datei in den Spiel-Heap; `math` PS*/C_*-Matrixfunktionen, die C-Fassungen der Gekko-Assembler-Funktionen (Tabelle mit Bitmustern aus einem Python-Modell der Paired-Single-Befehle), Skinning `CalcSk1` mit GQR-Quantisierung, `EmRoutineSetW`, die big-endian Sichten, die Länge von `__FILE__`; `trace` Zustands-Trace, GetDataExt-Abdeckung, Ende des Test-Tastenskripts; `tasks` der Task-Scheduler des Spiels (`TaskExec`, `TaskSleep`, `TaskExit`, `iTaskExec`) auf der Thread-Emulation, dazu VI-Retraces |
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

**Schnittstelle** `port/whole/runtime/re4_core.h` (Version 4, reines C, gleiche
Struktur-Größe auf 32 und 64 bit): `re4_core_reserve`, `re4_core_start`,
`re4_core_state` (idle/running/ended/failed), `re4_core_message`,
`re4_core_pad`, `re4_core_acquire`/`re4_core_release`, `re4_core_log_tail`
(letzte 24 Log-Zeilen für das Overlay), für Tests `re4_core_start_entry`.
Umgesetzt in `runtime/core_api.cpp` (in `libre4_main`).

**Schnappschuss** (`Re4CoreSnapshot`, 466 996 Byte auf 32 und 64 bit): Spielschritt
`Rno0..3`, Raum, Disc, Kamera aus `pG->Camera` (Augpunkt, Ziel, Rollwinkel,
Öffnungswinkel; bei offenem Untermenü die Weltkamera `CamCtrl.camera`, denn
`pG->Camera` ist dann die Kamera des Menüs), `screen` (Untermenü offen,
Ereignis, Debug-Kamera), bis 256 Modelle aus `EmMgr` (Gegner) und `ObjMgr` (Objekte):
lebend (`be_flag & 0x201 == 1`), Zeichen-Bits `2|4`, Weltmatrix, je Teil die
Weltmatrix `cParts::mat` und die Bindung (−`lt_inv_mat`-Verschiebung), bis
4096 Teile. Für jede Modell-Info (`pModelInfo`) die Herkunft der BIN- und
TPL-Daten auf der Disc (Datei, Archiv-Offset, Eintrag) und wie das Spiel
sie zeichnet (`trans.cpp commonModelTrans`): nur mit `be_flag & 8`; ein
starres Modell (ein Teil, keine Gewichte) hängt an `pHead->partsNo` mit
seinen Punkten im Raum dieses Teils (Leons Haare am Kopf); dazu die eigene
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
Herkunft und zeichnet es so auch während solcher Pausen. Kopiert das Spiel
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

### Lokale Prüfungen für K2

1. `port/scripts/test-r100-core.ps1` (wie M5) zeichnet jetzt zusätzlich
   Bilder: Der Kern schreibt mit `RE4_BRIDGE_SNAPSHOT` bis zu 8
   Schnappschüsse (30 Bilder nach Beginn der Hauptschleife, dann alle 2500),
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
   Vorher in der App „RE4 Kern“ beide Discs importieren.

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

Compiler-Schalter: `-std=gnu++11 -Wno-c++11-narrowing -fdelayed-template-parsing
-fno-strict-aliasing -ffp-contract=off -fwrapv -w`. C++11 wird gebraucht,
damit `TaskFunc` trotz Konstruktoren in Unions stehen darf.

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
| 0027 `model.cpp`, `objRocket.cpp` | Mit `-O2` (Android) gibt clang Inline-Funktionen, die nur in einer `.cpp` definiert sind, nicht aus; Module brauchen sie: `cModel::getPartsPtr` ohne `inline`, `cManager<cObj>::destroyNow` ausdrücklich instanziiert. Ohne den Patch zwei Platzhalter mehr als bei `-O0` |

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
| `dolphin_os.cpp` | `OSInit`, `OSReport` (zeilenweise ins Log), `OSPanic`, Konsolentyp/Speichergröße, `OSGetTime`/`OSGetTick` (40,5 MHz, Epoche 2000), Kalenderzeit, Interrupt-Sperren, Caches (no-op) |
| SDK `OSAlloc.c`, `OSArena.c` | unverändert aus `src/lib`: Heap und Arena; `main_mem.cpp` liest die Heap-Deskriptoren direkt |
| `os_thread.cpp` | `OSThread`, `OSSemaphore`: ein Host-Thread, strikte Prioritäten, Wechsel nur in OS-Aufrufen; jeder Thread hat einen Host-Stack (`port/src/stack_context.cpp` aus dem alten Port, neu mit ucontext für Linux x86, Boost.Context-Assembler für Linux ARM wie auf Android); ein Verteiler auf dem Stack des Startprogramms (`re4_os_run`). Spielsichtbar wie im SDK: `state`, `suspend`, Priorität, Warteschlangen, Stack-Magie `0xDEADBABE` |
| `vi.cpp` | VI-Retrace alle 1/59,94 s Echtzeit, zugestellt an Interrupt-Punkten (Verteiler im Leerlauf, `VIWaitForRetrace`, Patch 0012, später lange Hintergrundarbeit); Thread-Wechsel aus dem Rückruf erst am Interrupt-Ende. `RE4_VI_FAST=1`: ohne Warten (Tests) |
| `dvd.cpp` | `DVD*` über einen Ordner mit entpackten Disc-Dateien (siehe unten); Lesen sofort, Rückruf vor der Rückkehr; FST der eingelegten Disc oben in MEM1, Disc-ID bei `0x80000000`; Disc-Wechsel lädt den anderen Ordner |
| `aram.cpp` | ARAM (16 MB) als Host-Puffer, `ARInit`/`ARAlloc` (Stapel ab `0x4000`), ARQ kopiert sofort und ruft den Rückruf |
| `os_link.cpp` | `OSLink`/`OSUnlink` (siehe unten) |
| `yz2_decode.cpp` + `port/src/yz2_kernel.cpp` | `yz2Decode_Decode`: der geprüfte Kern des alten Ports unverändert, ein Adapter verbindet ihn mit `yz2code.cpp` des Spiels |
| `mtx_ps.c` + `sdk_c/` | Matrix-/Vektor-/Quaternion-Mathematik (C-Fassungen des SDK) |
| `byte_order.cpp` | Byte-Tausch-Helfer für Format-Umwandler |
| `newlib_ctype.c` | newlib-Zeichentabelle `_ctype_` (heißt seit K2 `re4_newlib_ctype_`, Prelude-Regel) |
| `card.cpp` (M4 B) | Speicherkarte: beide Schächte leer, jeder `CARD*`-Aufruf liefert `CARD_RESULT_NOCARD` (wie das SDK bei leerem Schacht); das Spiel zeigt „keine Speicherkarte“ und läuft ohne Speichern weiter. Echtes Speichern: M10 |
| `pad.cpp` (M4 B) | Controller: Kanal 0 angeschlossen, Kanäle 1–3 leer (ein zweiter Controller ließe den Titel die Logos überspringen). Tasten aus der Testfolge `RE4_PAD_SCRIPT` (`TASTEN@BILD` drückt 3 Bilder lang, `TASTEN@VON-BIS/ALLE` wiederholt; Tasten `A B X Y Z L R START UP DOWN LEFT RIGHT`, mit `+` verbunden; Bild = Zahl der `PADRead`-Aufrufe). Seit K2 zusätzlich `re4_pad_live` (Controller der App, sobald kein Testskript läuft) und Bildhaken `re4_add_frame_hook` (bis 4, je `PADRead`). Feinschliff der Eingabe: M7 |
| `core_api.cpp` (K2) | Einbettungs-Schnittstelle `re4_core.h` für die App: Kern-Thread, Status, Controller, Dreifachpuffer der Schnappschüsse; Testausgaben `RE4_BRIDGE_DUMP`, `RE4_BRIDGE_SNAPSHOT` |
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
| Teiltyp 1 (Tonbank) | Kopf, ISS-Block, `SND_SIT`, Wavetable (`WTFILEHEADER`, `WTREGION`, `WTART`, `WTSAMPLE`, `WTADPCM`), Sequenz-Tabelle (Sequenzen selbst sind Byteströme) | `formats_snd.cpp` |
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
