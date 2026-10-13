# Probelauf: gesamten Spielcode auf einmal kompilieren

Stand: 13.10.2026, Branch `claude/re4-gamecube-android-port-97hbjj`
(enthält `android-port` bis `6f18a34`).

## Frage

Lässt sich der komplette Original-Spielcode, statt Funktion für Funktion,
in einem Rutsch für ARM kompilieren? Und wie groß ist der Unterschied
zwischen ARM32 und ARM64?

## Methode

`port/tools/probe_full_compile.py` kompiliert jede `.c`/`.cpp`-Datei unter
`src/` außer `src/lib/` (Nintendo-SDK und CRI-Middleware, die der Port
ersetzt) mit clang im Modus `-fsyntax-only` für `armv7a` und `aarch64`.
`src/` und `include/` bleiben unverändert. Das Skript arbeitet auf einer
Kopie in `port/build/probe/tree` und wendet nur diese mechanischen
Umschreibungen an:

| Umschreibung | Grund |
|---|---|
| `asm(...)`, `__asm__(...)` entfallen (`port/probe/prelude.h`) | PowerPC-Registerbindung, Abschnittsdirektiven und PPC-Befehle haben auf ARM keine Bedeutung |
| `asm volatile` → `asm` | damit die vorige Regel greift |
| `operator new/delete(unsigned int…)` → `size_t` | C++-ABI auf 64 bit verlangt `size_t` |
| `types.h`, `dolphin/types.h` aus `port/probe/include` | exakte Breiten: `u32` bleibt auf ARM64 32 bit (`unsigned long` wäre dort 64 bit) |
| `strcat`, `strstr` deklariert | fehlen in `include/libc/string.h`; der Port nutzt die echte C-Bibliothek |

Compiler-Schalter: `-std=gnu++98 -fdelayed-template-parsing
-Wno-address-of-temporary` (GCC-2.95-Verhalten bei Templates und
Temporärobjekten).

Ausführen: `python3 port/tools/probe_full_compile.py` (rund 20 Sekunden).
Die vollständige Liste steht danach in `port/build/probe/report.json`.

## Ergebnis

| Ziel | Dateien fehlerfrei | Verschiedene Fehler |
|---|---|---|
| ARM32 (`armeabi-v7a`) | **600 von 658** | **133** |
| ARM64 (`arm64-v8a`) | 387 von 658 | 3068 |

**ARM32**, verbleibende Fehlerarten:

- 111 nicht deklarierte Bezeichner, vor allem Debug-Variablen (`include/dbg_var.h`)
  und Funktionen, die erst später im Quelltext deklariert werden
- 10 GCC-2.95-Builtins für variable Argumente in den newlib-Dateien
  (`printf.c`, `fprintf.c`, `sprintf.c`, `sscanf.c`). Diese Dateien entfallen
  im Port, weil die Plattform-C-Bibliothek sie liefert.
- etwa 10 Einzelfälle

**ARM64**: dieselben Fehler plus **2932 Stellen, an denen ein Zeiger in eine
32-bit-Ganzzahl (`u32`) gesteckt wird**. Sie verteilen sich über das ganze
Spiel: `Sscrn` 516, `game` 406, `Tools` 159 und je rund 100 in jedem Gegner-Modul.

## Bewertung

1. **Der Ganzprogramm-Ansatz ist realistisch.** 91 % der Dateien übersetzen
   für ARM32 schon jetzt ohne Fehler. Die übrigen 133 Fehler sind mechanisch.
2. **ARM32 zuerst ist der richtige Weg.** Auf ARM64 kommen fast 3000
   Zeiger-Kürzungen hinzu. Jede einzelne davon wäre zur Laufzeit ein Absturz.
   ARM32 hat dieselbe Zeigerbreite wie die GameCube und umgeht das komplett.
3. **ARM32 lässt sich in der Cloud testen.** `g++ -m32` erzeugt 32-bit-Linux-
   Programme mit derselben Zeigerbreite. Spiellogik, Laden und Abstürze
   lassen sich so ohne Gerät prüfen.

## Was der Probelauf nicht zeigt

- **Linken:** Welche SDK- und CRI-Funktionen fehlen, zeigt erst der nächste
  Schritt. Ersatz-Stubs werden dann automatisch erzeugt.
- **Laufzeit:** Byte-Reihenfolge der Disc-Daten (big-endian) und Strukturen
  mit Zeigern, die direkt aus Dateien geladen werden. Diese Fehler treten erst
  beim Ausführen auf.
- **Template-Körper:** `-fdelayed-template-parsing` prüft sie erst bei Benutzung.
  Weitere Fehler können beim Linken auftauchen.
