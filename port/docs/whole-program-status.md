# Ganzprogramm-Port: Stand

Diese Datei ist das Gedächtnis zwischen den Sitzungen. Jede Sitzung liest sie
zuerst und aktualisiert sie am Ende (siehe `CLAUDE.md`).

## Kurzstand

| Meilenstein | Stand |
|---|---|
| M1 Probelauf | ✅ 13.10.2026 (`whole-program-probe.md`) |
| M2 Alle Spieldateien kompilieren (ARM32 + 32-bit-Linux) | ✅ 13.10.2026 |
| M3 Linken, lauffähiges 32-bit-Linux-Programm | offen, nächster Schritt |

## Bauen und prüfen

```sh
# Compiler für 32-bit-Linux und ARM32 installiert in der Cloud das Startskript
# .claude/hooks/session-start.sh automatisch (g++-multilib geht nicht: Ubuntu
# verweigert es neben dem ARM-Cross-Compiler, deshalb nur die lib32-Pakete)
port/tools/whole_build.sh          # baut i686 und armhf, ~1 Minute
```

Das Skript meldet je Ziel die fehlerhaften Dateien und danach die
Link-Vorschau (`port/tools/whole_symbols.py`). Rückgabewert 0 heißt: alle
Dateien kompiliert.

**M2-Nachweis (13.10.2026):**

```
i686: 0 failed units, 0 distinct errors
armhf: 0 failed units, 0 distinct errors
objects: 600
```

Alle 600 Spiel-Übersetzungseinheiten kompilieren mit clang 18 für
`i686-linux-gnu` und `arm-linux-gnueabihf` (armv7) zu Objektdateien.
Das Android-NDK ist in der Cloud nicht erreichbar, weil `dl.google.com`
von der Netzwerkregel gesperrt ist. Den NDK-Build (`armeabi-v7a`) prüft
deshalb der lokale Windows-PC. Der Compiler ist derselbe (clang), nur die
C-Bibliothek ist eine andere (Bionic statt glibc).

**Nicht getestet:** Der GameCube-Matching-Build (`ninja` im Hauptverzeichnis)
wurde hier nicht ausgeführt, weil Disc-Images und Original-Compiler fehlen.
`src/` und `include/` sind unverändert (`git status` zeigt dort keine
Änderungen), deshalb kann er nicht betroffen sein.

## Aufbau

| Pfad | Inhalt |
|---|---|
| `port/whole/CMakeLists.txt` | Build aller Spieldateien als eine Objektbibliothek `re4_game` |
| `port/whole/cmake/linux-*.cmake` | Toolchains i686 (Cloud-Test) und armhf (ARM32-Kompilierprüfung) |
| `port/whole/include/` | Ersatz-Header: `types.h`, `dolphin/types.h`, Prelude, `re4_task_func.h` |
| `port/whole/patches/` | Geprüfte Einzelkorrekturen, angewendet auf die Kopie |
| `port/tools/whole_prepare.py` | Kopiert `src/` + `include/`, schreibt mechanisch um, wendet Patches an |
| `port/tools/whole_make_patch.py` | Erzeugt einen Patch gegen die umgeschriebene Fassung |
| `port/tools/whole_symbols.py` | Link-Vorschau: fehlende und doppelte Symbole |
| `port/tools/whole_build.sh` | Ein Befehl für Bauen und Bericht |

Die Original-Quellen werden nie verändert. `whole_prepare.py` legt bei jedem
CMake-Lauf eine Kopie unter `port/build/whole-*/prepared/tree` an.

Übersetzt werden alle `.cpp` unter `src/` außer `src/lib/`. Die 58 `.c`-Dateien
in `src/game` (`printf.c`, `strcpy.c`, `sn_malloc.c` …) sind die SN/newlib-
C-Laufzeit. Sie entfallen, weil die Plattform-C-Bibliothek sie ersetzt.

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

Neuen Patch erzeugen: Datei aus `whole_prepare.rewrite()` kopieren, ändern,
`port/tools/whole_make_patch.py NAME pfad/im/repo kopie` ausführen.

## Ausgangslage für M3 (Linken)

Link-Vorschau des i686-Builds (`whole_symbols.py`):

| Fehlende Symbole | Anzahl | Woher sie kommen müssen |
|---|---|---|
| Nintendo-SDK | 335 | Plattformschicht: GX 98, OS 55, AX 28, AXFX 21, DVD 18, CARD 17, PSMTX 16, MIX 11, PSVEC 10, VI 10, PPC 9, SYN 8, PAD 7, Rest ≤ 5 |
| C-Bibliothek | 39 | wird automatisch gelinkt |
| Spielcode | 20 | prüfen (siehe unten) |
| CRI-Middleware | 16 | ADX (Audio) und Sofdec (Video), Code in `src/lib/` |
| SN-Toolchain | 16 | `__builtin_new/delete`, `_ctors/_dtors`, `PC*`-Host-I/O, `memset_asm`, `memclr_asm`, `yz2Decode_Decode` (vorhandene Übersetzung in `port/src/yz2_kernel.cpp`) |
| C++-Laufzeit | 13 | wird automatisch gelinkt |

**Fehlende Spielsymbole (20)**, jeweils einzeln zu klären: `Em32_fs_tbl`,
`EmCatchSubSet`, `QuakeKill`, `SceAtSys`, `ScrIdRefTbl`, `ScrObjIdNum`,
`ToolArrayPush`, `ToolEmArraySet`, `ToolWorkPop`, `_ctype_`,
`cDbgWindow::Init`, `cManager<cObj>::destroyNow`, `cModel::getPartsPtr`,
`cModel::isTrans`, `cModel::move`, `cModel::setNoSuspend`,
`cPlayer::subCharLiveCheck`, `fontTMtx`, `g_EspToolSeqHedAddr`,
`setRoomEtcBreakDisp`. Vermutlich sind es Inline-Funktionen ohne Körper im
Header, `static`-Variablen, die eine andere Datei per Alias anspricht, oder
Symbole aus `src/lib`.

**Doppelt definierte Symbole: 286.** Im Original lagen die 114 REL-Module
getrennt im Speicher, jedes durfte eigene Kopien haben (z. B. `em_wrap.cpp`
in drei Varianten, `db_light*.cpp` viermal, `t_prim.cpp` zweimal). Ein
einziges Programm erlaubt jeden Namen nur einmal. Mögliche Lösungen für M3:
Module als einzelne Shared Libraries laden (kommt dem Original am nächsten)
oder Doppelte pro Modul umbenennen. Diese Entscheidung zu Beginn von M3 treffen.

## Bekannte Risiken für die Laufzeit (M4/M5)

- Mit `-w` sind Warnungen ausgeblendet. Bei Abstürzen lohnt ein Lauf ohne `-w`
  für die betroffene Datei.
- Funktionsaliase mit abweichender Signatur (z. B. `EvtFlgOnStatus` →
  `Event::FlgOnStatus`) funktionieren auf ARM32 und i686, weil `this` dort wie
  ein erstes Argument übergeben wird.
- Disc-Daten sind big-endian und werden noch nirgends umgewandelt.
