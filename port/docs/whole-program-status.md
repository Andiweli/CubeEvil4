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
| M5 Erster Raum: Spielschleife ohne Absturz | offen, nächster Schritt |

## Bauen und prüfen

```sh
# Werkzeuge (clang, lld, cmake, ninja, lib32-Bibliotheken, ARM32-Cross-Compiler,
# qemu-user): sudo port/tools/whole_setup_linux.sh. In der Cloud ruft das
# Startskript .claude/hooks/session-start.sh es automatisch auf; lokal in WSL2
# (Ubuntu 24.04) einmal von Hand. g++-multilib geht nicht: Ubuntu verweigert es
# neben dem ARM-Cross-Compiler.
port/tools/whole_build.sh          # baut und prüft i686 und armhf, ~1,5 Minuten
```

Je Ziel: Build, dann drei Prüfungen von `re4_host` (armhf unter `qemu-arm`):

| Prüfung | Inhalt |
|---|---|
| `--self-test` | Weiterleitungen zwischen Modulen geben Argumente korrekt weiter |
| `--check-modules` | alle 114 Module laden, alle Querverweise lösen sich auf |
| `--runtime-test` | Plattformschicht ohne Disc-Daten (`port/whole/tests/runtime_tests.cpp`): `link` OSLink/OSUnlink mit synthetischem big-endian REL-Kopf und Testmodul; `dvd` DVD-API über einen synthetischen Zwei-Disc-Ordner; `yz2` Yz2 des Spiels + Kern mit den Testdaten `port/tests/yz2_fixtures.h`; `dvdq` die DVD-Warteschlange des Spiels liest eine synthetische mehrteilige Datei in den Spiel-Heap; `math` PS*/C_*-Matrixfunktionen; `tasks` der Task-Scheduler des Spiels (`TaskExec`, `TaskSleep`, `TaskExit`, `iTaskExec`) auf der Thread-Emulation, dazu VI-Retraces |

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
  zeigt (M6): Umgewandelte Texturköpfe, Effekte und Layouts sind nur so weit
  geprüft, wie der Spielcode sie beim Booten liest.

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
| `newlib_ctype.c` | newlib-Zeichentabelle `_ctype_` |
| `card.cpp` (M4 B) | Speicherkarte: beide Schächte leer, jeder `CARD*`-Aufruf liefert `CARD_RESULT_NOCARD` (wie das SDK bei leerem Schacht); das Spiel zeigt „keine Speicherkarte“ und läuft ohne Speichern weiter. Echtes Speichern: M10 |
| `pad.cpp` (M4 B) | Controller: Kanal 0 angeschlossen, Kanäle 1–3 leer (ein zweiter Controller ließe den Titel die Logos überspringen). Tasten aus der Testfolge `RE4_PAD_SCRIPT` (`TASTEN@BILD` drückt 3 Bilder lang, `TASTEN@VON-BIS/ALLE` wiederholt; Tasten `A B X Y Z L R START UP DOWN LEFT RIGHT`, mit `+` verbunden; Bild = Zahl der `PADRead`-Aufrufe). Echte Eingabe: M7 |
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
| `*.das`, `*.drs`, `ss/*.dat`, entpackte Räume | Archiv: Zähler, REL-Offset, Offsets, Kürzel; eingebettetes REL bekommt `re4_rel_header_to_host` (read.cpp `GetDataExt`) | `formats_arc.cpp` |
| `UWF` | `ID_FILE_HEADER`, `ID_DATA_V1/V2`, B-Spline `FUNC_PATH_PTR`, `HERMITE_1_PTR` (id_sys.cpp) | `formats_id.cpp` |
| `EFF` | `ESP_EFF_HEADER` (Version 0xB): Id-Listen, TPL- und `cAnm`-Tabellen, `cEspSeqHead` + `cEspSeqTbl`, Pfade (`Path`), `EffEfmEnt` (eff_sys.cpp) | `formats_eff.cpp` (Effektmodelle, -motionen, Shape-Köpfe noch nicht) |
| `TEX` | `TexData` (Version 3), Id-Liste, TPLs, `cAnm` (texture.cpp) | `formats_eff.cpp` |
| `VIB` | `VibDataTbl`, `VIB_INFO`, `VIB_DATA` (pad.cpp) | `formats_eff.cpp` |
| `debug/roominfo.dat` | Stufen-Offsets, `CRoomInfo` (room_jmp.h) | `formats_dbg.cpp` |
| `rel/*.rel` | REL-Kopf | `OSLink` (`re4_rel_header_to_host`) |
| `debug/*.txt` | Text | nichts zu tun |

**Noch ohne Umwandler** (beim Booten bis `gameRoomInit` gesehen): Archiv-
Kürzel `BIN` (Modelle, aber auch andere Formate unter derselben Endung, z. B.
der Lichtpfad `CORE_LIGHT_PATH`), `LIT`, `CAM`, `SAT`, `SEQ`, `FCV`, `FNT`,
Raumdaten `SMD`, `SHD`, `CNS`, `EAT`, `SMX`, `FSE` und weitere, `etc/*.esl`,
Effektmodelle in `EFF`. „Error: Can't calc Inverse Matrix !“ im Titel kommt
vermutlich aus einem dieser noch big-endian Blöcke.

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
- **Wort-Zugriffe auf Byte-Strukturen** (`*(u32*) &farbe = 0xFF`): noch offen,
  siehe offene Punkte.

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

- **Raumdaten und weitere Formate (Kern von M5):** `CNS`, `SMD`, `SMX`,
  `SHD`, `EAT`, `FSE`, `LIT`, `SAT`, `CAM`, `BIN` (Modelle; Kürzel ist die
  Dateiendung und deckt mehrere Formate ab, Erkennung am Kopf), `SEQ`/`FCV`
  (Motionen: `motion.cpp` liest die Schlüsselströme der Layouts 0–2 byteweise
  big-endian über `FccF32`/`FccS16`, Layout 3 direkt als `s16`; diese Ströme
  dürfen nur dort gedreht werden, wo direkt gelesen wird), `FNT`, `etc/*.esl`,
  Effektmodelle in `EFF`. Wissen: Originalstrukturen, `tools/motion/*.py`,
  `port/src/*_original.h`, `original_import/formats.py`.
- **Wort-Zugriffe auf Byte-Strukturen** (rund 45 Stellen): Farben werden als
  u32 geschrieben (`*(u32*) &col.start = 0xFF` = schwarz, deckend; `fade.h`,
  `game.cpp`, `event.cpp`, `sscrn.cpp`, Raum-Module `r21d`, `r300`, `r311`,
  `r31c`, `r117` …) oder geprüft (`light.cpp`: `*(u32*) &l->DispCol &
  0xFFFFFF00`). Auf little-endian landen die Bytes vertauscht (falsche Farben).
  Kritisch für M5: `include/em.h:168` setzt die vier Zustandsbytes `r_no_0..3`
  eines Gegners über ein u32. Ebenfalls: `snd_iss2.cpp`/`snd_str2.cpp` lesen
  die AX-Adresspaare Hi/Lo als ein u32 (M8). Vorschlag: mit dem Compiler
  (clang-AST) alle Casts von `&`Byte-Aggregat nach `u32*`/`u16*` finden und an
  genau diesen Stellen einen big-endian Zugriffstyp einsetzen (mechanisch,
  Liste prüfbar). Typ-Wortspiele zwischen gleich großen Skalaren (`rnd.cpp`
  Float-Bits) bleiben unverändert. `cModelInfo::colorWord` schreibt nur
  `0xFFFFFFFF`; der Maskenvergleich in `t_scroll.cpp` (Debug-Werkzeug) bleibt
  falsch, bis diese Regel greift.
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
- **Android-Einbindung:** `libre4_platform`, `libre4_main` und die Module in
  das Android-Studio-Projekt; `_ctype_` kollidiert dort mit Bionics
  gleichnamigem Zeiger (prüfen, nur Debug-Werkzeug `t_movie` liest ihn). Der
  JNI-Start ruft `re4_check_game_address` für `libre4_main` und reserviert
  MEM1 früh.

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
