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
| M4 Teil B: Booten mit echten Disc-Daten bis `gameRoomInit` (lokal, WSL2) | offen, nächster Schritt |

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
- Alles mit echten Disc-Daten (Teil B).
- Der GameCube-Matching-Build (`ninja` im Hauptverzeichnis). Disc-Images und
  Original-Compiler fehlen hier. `src/` und `include/` sind unverändert.
- Der Android-NDK-Build. `dl.google.com` ist von der Netzwerkregel der Cloud
  gesperrt. Geprüft wurde mit clang für `arm-linux-gnueabihf` (glibc statt
  Bionic). Insbesondere ungeprüft: ob eine Android-App den Adressbereich
  `0x80000000–0x83000000` reservieren kann (siehe Risiken).

## Aufbau

| Ergebnis | Inhalt |
|---|---|
| `libre4_platform.so` | Plattformschicht: MEM1, Dolphin-OS-Ersatz (Arena, Heap aus dem SDK, Threads, Zeit), DVD über Ordner, ARAM, VI, OSLink/Modul-Lader, Yz2-Kern, SDK-Mathematik |
| `libre4_main.so` | Spielcode von `main.dol` (295 Dateien), SN-Toolchain-Ersatz, erzeugte Platzhalter |
| `modules/libre4_<modul>.so` | eine Bibliothek je Original-REL-Modul (114) |
| `re4_host` | Linux-Startprogramm (`--self-test`, `--check-modules`, `--runtime-test`, sonst Spielstart; `--data DIR`) |

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
| `port/whole/host/main.cpp` | Linux-Startprogramm mit Absturz-Backtrace |
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

| Platzhalter | M3 | M4 Teil A |
|---|---|---|
| Funktionen | 363 | 234: GX 97 (M6), AX/AXFX/AXART/MIX/SYN/SEQ/AI/ADX 77 (M8), CARD 17 (M10), mwPly 12 (M9), PAD 7 (M7), PPC-Leistungszähler 8, SN-PC-Host-I/O 7 (nur Entwicklerkonsole), OS 8 (IPL-Schrift `OSInitFont`/`OSGetFontEncode`/`OSGetFontTexture`, Stoppuhren, `OSResetSystem`), `TEXGet` |
| Daten | 9 | 1: `GXWGFifo` (GX-Schreib-FIFO, `mes.cpp` schreibt Eckpunkte direkt hinein; M6) |

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

## Offene Punkte für Teil B und später

- **Big-endian-Daten (Kern von Teil B).** Der Originalcode liest alle
  Disc-Daten direkt als Strukturen (`*(u32*)(data + 4)` für den REL-Versatz in
  Archiven, `pDll->bssSize` vor `OSLink`, Schriften, Nachrichten, Tabellen,
  Modelle …). Clang kennt kein `scalar_storage_order`, und Android ist
  little-endian; jedes Format muss daher beim Laden in Host-Reihenfolge
  gedreht werden, Feld für Feld nach seiner Struktur. Erledigt: DVD-Teiletabelle
  (Patch 0013), REL-Kopf (in `OSLink`, für eingebettete RELs muss der Kopf vor
  dem Lesen von `bssSize` gedreht werden, z. B. mit `re4_rel_header_to_host`).
  Erste Kandidaten auf dem Bootpfad: `Font/common_p.fnt` (`setupFont`),
  `etc/sizetbl.dat`, `bgm/*.hed`, `bgm/bgmtbl.dat`, `etc/memcard.das`,
  `etc/moji8.tpl`, `debug/roomInfo.dat`, Nachrichtendateien. Wissen über die
  Formate: die Strukturen im Originalcode, `tools/motion/*.py`, die Leser des
  alten Ports (`port/src/*_original.h`, `asset_index.cpp`) und dessen Import
  (`original_import/formats.py`).
- **Adressbereich auf Android:** prüfen, ob eine 32-bit-App
  `0x80000000–0x83000000` reservieren kann (ART legt seinen Heap bei
  `0x70000000+` an). Schnelltest ohne Port: `/proc/<pid>/maps` einer laufenden
  armeabi-v7a-App (z. B. der alten Port-App) ansehen. Falls belegt: Alternative
  entscheiden (Projektinhaber), z. B. MEM1 an anderer Adresse plus Patches für
  die festen Adressen und Zeigerprüfungen.
- **IPL-Schrift:** `OSInitFont` braucht die Schrift aus dem IPL-ROM (nicht auf
  der Disc), nur für die Fehlerbildschirme. Platzhalter liefert 0.
- **Weitere Leerlauf-Schleifen** auf Interrupt-Zähler wie in `main.cpp` können
  auftauchen; dort ebenfalls `re4_wait_interrupt()` einsetzen.
- **Hintergrund-Task (iTask):** läuft ohne Unterbrechung bis zum Ende. Für
  echtes Zeitscheiben-Verhalten `re4_interrupt_point()` in lange Schleifen
  (z. B. den Yz2-Kern über seinen Checkpoint) einbauen.
- **Android-Einbindung:** `libre4_platform`, `libre4_main` und die Module in
  das Android-Studio-Projekt; `_ctype_` kollidiert dort mit Bionics
  gleichnamigem Zeiger (prüfen, nur Debug-Werkzeug `t_movie` liest ihn).

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
