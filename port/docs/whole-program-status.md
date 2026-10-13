# Ganzprogramm-Port: Stand

Diese Datei ist das Gedächtnis zwischen den Sitzungen. Jede Sitzung liest sie
zuerst und aktualisiert sie am Ende (siehe `CLAUDE.md`).

## Kurzstand

| Meilenstein | Stand |
|---|---|
| M1 Probelauf | ✅ 13.10.2026 (`whole-program-probe.md`) |
| M2 Alle Spieldateien kompilieren (ARM32 + 32-bit-Linux) | ✅ 13.10.2026 |
| M3 Linken, lauffähiges 32-bit-Linux-Programm, Module als eigene Bibliotheken | ✅ 13.10.2026 |
| M4 Booten mit echten Disc-Daten bis `gameRoomInit` | offen, nächster Schritt |

## Bauen und prüfen

```sh
# Werkzeuge (lib32-Bibliotheken, ARM32-Cross-Compiler, qemu-user) installiert
# in der Cloud das Startskript .claude/hooks/session-start.sh automatisch.
# g++-multilib geht nicht: Ubuntu verweigert es neben dem ARM-Cross-Compiler.
port/tools/whole_build.sh          # baut und prüft i686 und armhf, ~1,5 Minuten
```

Je Ziel: Build, dann `re4_host --self-test` (Weiterleitungen zwischen Modulen
geben Argumente korrekt weiter) und `re4_host --check-modules` (alle Module
laden, alle Querverweise lösen sich auf). armhf läuft dabei unter `qemu-arm`.
Rückgabewert 0 heißt: alles bestanden.

**M3-Nachweis (13.10.2026), frischer Build:**

```
i686: 0 failed build steps, 0 distinct errors
  link plan: 363 function stubs, 9 data stubs, 893 cross-module calls, 1 cross-module data imports (private copies)
  re4_host --self-test: exit 0 | self-test: trampolines ok (code 0)
  re4_host --check-modules: exit 0 | check-modules: 114 modules, 0 failed to load, 0 unresolved imports, 363 stubs
armhf: 0 failed build steps, 0 distinct errors
  (gleiche Zeilen, ausgeführt unter qemu-arm)
```

Alle Bibliotheken linken mit `-Wl,--no-undefined`: Kein Symbol bleibt offen.

**Spielstart (`re4_host` ohne Option), i686 und armhf gleich:** Der
Originalcode läuft in `re4_game_main` → `systemStartInit` und ruft 28
SDK-Funktionen auf (OSInit, VIInit, DVD-, CARD-, Arena- und Heap-Funktionen),
die alle als Platzhalter antworten. Dann stürzt er in `SystemMemInit` →
`mem_alloc` ab, weil der Platzhalter `OSAllocFromHeap` 0 liefert und die
Fehlermeldung über das noch fehlende `pLog` läuft. Das ist der erwartete erste
Absturz und der Einstieg in M4.

**Nicht getestet:**
- Der GameCube-Matching-Build (`ninja` im Hauptverzeichnis). Disc-Images und
  Original-Compiler fehlen hier. `src/` und `include/` sind unverändert.
- Der Android-NDK-Build. `dl.google.com` ist von der Netzwerkregel der Cloud
  gesperrt. Geprüft wurde mit clang für `arm-linux-gnueabihf` (glibc statt
  Bionic). Der NDK-Build kommt mit der Einbindung in das Android-Studio-Projekt.

## Aufbau

| Ergebnis | Inhalt |
|---|---|
| `libre4_main.so` | Spielcode von `main.dol` (295 Dateien), Plattform-Laufzeit, erzeugte Platzhalter |
| `modules/libre4_<modul>.so` | eine Bibliothek je Original-REL-Modul (114) |
| `re4_host` | Linux-Startprogramm (`--self-test`, `--check-modules`, sonst Spielstart) |

| Pfad | Inhalt |
|---|---|
| `port/whole/CMakeLists.txt` | Build: Objektbibliothek je Modul, Link-Plan, Bibliotheken, Startprogramm |
| `port/whole/cmake/linux-*.cmake` | Toolchains i686 (Cloud-Test) und armhf (ARM32, Test unter qemu-arm) |
| `port/whole/include/` | Ersatz-Header: `types.h`, `dolphin/types.h`, Prelude, `re4_task_func.h` |
| `port/whole/patches/` | Geprüfte Einzelkorrekturen, angewendet auf die Kopie |
| `port/whole/runtime/` | Plattform-Laufzeit: Log, Modul-Lader, SN-Toolchain-Ersatz, Modul-CRT |
| `port/whole/host/main.cpp` | Linux-Startprogramm mit Absturz-Backtrace |
| `port/whole/tests/` | Selbsttest der Modul-Weiterleitungen |
| `port/tools/whole_prepare.py` | Kopie von `src/` + `include/`, Umschreibungen, Patches, Modulplan `modules.json` |
| `port/tools/whole_link_plan.py` | Probe-Link, erzeugt Platzhalter, Modultabelle und Weiterleitungen |
| `port/tools/whole_make_patch.py` | Erzeugt einen Patch gegen die umgeschriebene Fassung |
| `port/tools/whole_build.sh` | Ein Befehl für Bauen und alle Prüfungen |

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

Neuen Patch erzeugen: Datei aus `whole_prepare.rewrite()` kopieren, ändern,
`port/tools/whole_make_patch.py NAME pfad/im/repo kopie` ausführen.

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

| Platzhalter | Anzahl |
|---|---|
| Funktionen: GX 97, OS 54, AX 28, AXFX 21, DVD 18, CARD 17, PSMTX 16, mwPly 12, MIX 11, PSVEC 10, VI 10, PPC 9, SYN 8, PAD 7, PC-Host-I/O 7, C_MTX 5, DC 5, ADX 4, Rest ≤ 3 | 363 |
| Daten: `Em32_fs_tbl`, `GXWGFifo`, `SceAtSys`, `ScrIdRefTbl`, `ScrObjIdNum`, `__OSCurrHeap`, `_ctype_`, `fontTMtx`, `g_EspToolSeqHedAddr` | 9 |

**Echter Ersatz statt Platzhalter** (`runtime/sn_runtime.cpp`): `_ctors`/`_dtors`
(leer, weil der ELF-Lader Konstruktoren selbst ausführt), `__builtin_new`/
`__builtin_delete` (GCC-2.95-Namen von `operator new/delete`),
`memset_asm`/`memclr_asm`.

## Offene Punkte für M4 und später

- **Speicher:** `OSInitAlloc`, `OSCreateHeap`, `OSAllocFromHeap`,
  `OSGetArenaLo/Hi` brauchen eine echte Umsetzung. Möglichkeit: der portable
  C-Code des SDK aus `src/lib` (OSAlloc) plus eine Arena aus `malloc`.
- **Modul-Laden:** `OSLink`/`OSUnlink` sind noch Platzhalter. Umsetzung: ID aus
  dem REL-Kopf lesen (big-endian), `re4_module_load(re4_module_name(id))`,
  `_prolog`/`_epilog` per `dlsym` in den Kopf eintragen.
- **Spielfunktionen ohne Definition** (Platzhalter, die 0 liefern):
  `EmCatchSubSet`, `setRoomEtcBreakDisp`, `QuakeKill`, `cDbgWindow::Init`,
  `cModel::isTrans`, `cPlayer::subCharLiveCheck`, dazu die Daten `Em32_fs_tbl`,
  `SceAtSys`, `ScrIdRefTbl`, `ScrObjIdNum`, `fontTMtx`, `g_EspToolSeqHedAddr`.
  Jeweils klären, wo das Original sie definiert (Inline-Funktion, `static` in
  einer anderen Datei, `src/lib`).
- **`_ctype_`:** newlib-Zeichentabelle, die `include/newlib_local.h` direkt
  liest. Als genullter Platzhalter liefert z. B. `isupper` immer „nein“. Echte
  Tabelle nachrüsten, falls Spielcode `newlib_local.h` benutzt.
- **`yz2Decode_Decode`:** Platzhalter. Die vorhandene Übersetzung in
  `port/src/yz2_kernel.cpp` anbinden.

## Bekannte Risiken für die Laufzeit (M4/M5)

- Mit `-w` sind Warnungen ausgeblendet. Bei Abstürzen lohnt ein Lauf ohne `-w`
  für die betroffene Datei.
- Funktionsaliase mit abweichender Signatur (z. B. `EvtFlgOnStatus` →
  `Event::FlgOnStatus`) funktionieren auf ARM32 und i686, weil `this` dort wie
  ein erstes Argument übergeben wird.
- Disc-Daten sind big-endian und werden noch nirgends umgewandelt.
