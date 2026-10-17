# M12 ARM64: Bestandsaufnahme (17.10.2026)

Ziel von M12: Der Kernmodus läuft auch als `arm64-v8a`. Eine APK enthält
`armeabi-v7a` und `arm64-v8a`, weil ein Tester ein Gerät hat, das nur
64-bit-Apps ausführt. Stand: Branch `claude/re4-m12-arm64` (von
`claude/nifty-feynman-jv729h`, Stand `dd762f3`/`f9a0af9`), gemessen in der
Cloud an den echten Compiler-Aufrufen des heutigen ARM32-Builds
(`port/build/whole-armhf/compile_commands.json`: der vorbereitete Baum mit
allen Umschreibungen und Patches, die Ersatz-Header, die Plattformschicht).

Werkzeuge (alle in `port/tools/m12/`, wiederholbar):

| Werkzeug | Was es misst |
|---|---|
| `lp64_probe.py [build] [--target T] [--extra S]` | übersetzt jede Datei des Builds für ein anderes Ziel (`-fsyntax-only`), sortiert die Meldungen nach Art und Bereich; Bericht als JSON |
| `layout_compare.py [build] [--targets A,B]` | vergleicht die Größe jeder Spielstruktur zwischen zwei Zielen (`-fdump-record-layouts-simple`) |
| `wasm_bench.sh` | Geschwindigkeit von Spiel-artigem Code nativ und in der wasm32-Sandbox (wasm2c) |

Für wasm32 braucht die Cloud `sudo apt-get install wasi-libc
libc++-18-dev-wasm32 libc++abi-18-dev-wasm32 libclang-rt-18-dev-wasm32 wabt`
(noch nicht im Startskript `.claude/hooks/session-start.sh`).

x86_64 steht für arm64-v8a: beide sind LP64 (Zeiger und `long` 8 Bytes),
die Zeigerfragen sind dieselben. In der Cloud gibt es keine arm64-Header der
C-Bibliothek.

## 1. Was ein normaler 64-bit-Build trifft (LP64)

### Übersetzen

`lp64_probe.py` über 1025 Dateien (Spiel, Module, SDK, CRI, Plattformschicht):

| Art | Stellen | davon Fehler |
|---|---:|---:|
| Zeiger in eine 32-bit-Zahl (`(u32) p`) | 3060 | 2948 |
| 32-bit-Zahl in einen Zeiger | 238 | 0 (Warnungen) |
| Größenprüfungen (`static_assert`, Feld mit negativer Größe) | 22 | 22 |
| Überladungen mit `long`/`size_t` | 13 | 13 |
| `operator new`/`delete` | 1 | 1 |
| Sonstiges | 4 | 3 |

Nach Bereichen (Zeiger→Zahl): Gegner-Module 1616, Sscrn 513, Spielkern 342,
übrige Module 271, Tools 158, SDK/CRI 112, Raum-Module 27, Plattformschicht 5.
Rund ein Drittel kommt aus wenigen Makros (`DbgFlagOn/Off`, `SpfFlagOn/Off`,
`DpfFlagOn/Off`, `SS_ARC_PTR`, `OP_ARC_PTR`, `Sys/StaFlag…`), 271 sind direkte
Umwandlungen in der Zeile.

**Mit `-fms-extensions`** werden die 2948 Zeiger-Kürzungen zu Warnungen; dann
bleiben **rund 40 Übersetzungsfehler** (die 22 Größenprüfungen sind die der
Plattformschicht für die 32-bit-Anordnung von `OSThread`, `DVDFileInfo`,
`DVDCommandBlock`, `OSModuleHeader`, `PADStatus`, die Spielstand-Konstanten
in `gci.cpp`), und 3141 + 3036 Stellen kürzen bzw. erweitern Zeiger still.
Das ist zur Laufzeit nur richtig, wenn **jede** Adresse, die der Spielcode
sieht, unter 4 GB liegt.

### Anordnung der Daten

`layout_compare.py`: von **1293 Strukturen werden 835 größer** (65 %; 298
um mehr als die Hälfte). Beispiele (Bytes, ARM32 → 64 bit):

| Struktur | ARM32 | 64 bit | Problem |
|---|---:|---:|---|
| `cEm` (Grundklasse aller Figuren) | 992 | 1232 | Figuren liegen in festen Plätzen von `EM_WORK_SIZE` = 0xDE0; **alle 29 Gegnerklassen** wären größer (bis 0xED0) |
| `cObj` / größte Objektklasse | 808 / 0x3D8 | 960 / 0x4B8 | feste Plätze `OBJ_WORK_SIZE` = 0x3D8 |
| `cDmg` | 24 | 40 | feste Plätze 0x118 |
| `cPlayer` | 2204 | 2648 | |
| `SAVE_DATA_HEAD` | 28 | 56 | Kopf des Spielstands mit Zeigern, die vor dem Schreiben auf die Karte zu Abständen werden: die Kartendatei (und `.gci`) hätte eine andere Form |
| `TEXPalette` (TPL, jede Textur) | 12 | 16 | liegt direkt auf den Daten von der Disc; die 4-Byte-Felder, die das Spiel in Zeiger umwandelt, fassen keinen 64-bit-Zeiger |
| `cSmd` (Raumszene) | 88 | 120 | dto. (Raumdatei) |
| `VibDataTbl` (Vibration) | 8 | 16 | dto. (Kernarchiv) |
| `FLR_AT_HEADER` (Kollision) | 16 | 24 | dto. |
| `cModelInfo` | 292 | 408 | |
| `OSThread` / `DVDFileInfo` | 792 / 60 | 856 / 88 | die Plattformschicht prüft die 32-bit-Form |
| `GLOBAL_WK` | 34432 | 34488 | der Spielstand-Block darin (0x36F8 Bytes, 70 Felder) bleibt gleich, nur seine Lage verschiebt sich |

Stichprobe Dateistrukturen: von 14 Strukturen, die der Code direkt über
Archivdaten legt, ändern 5 ihre Größe. Der Kern wandelt rund 80
Dateiformate um (`runtime/formats`); welche davon Zeigerfelder haben, müsste
Format für Format geklärt werden. Jede übersehene Stelle ist zur Laufzeit
ein stiller Speicherfehler, kein Übersetzungsfehler.

### Speicher unter 4 GB

Möglich auf Android arm64: MEM1 liegt schon fest bei 0x80000000; die
Spiel-Bibliotheken (117 Dateien, 22 MB) lassen sich mit `android_dlopen_ext`
(`ANDROID_DLEXT_RESERVED_ADDRESS`) in einen selbst reservierten Bereich
unter 4 GB laden; die Stacks der Spiel-Threads und alle Speicherblöcke der
Plattformschicht, die das Spiel sieht, müssten ebenfalls dort liegen
(eigener Speicher statt `malloc`, Spiel-Thread mit eigenem Stack).

## 2. Die 32-bit-Sandbox (wasm32)

WebAssembly mit 32-bit-Adressen (`wasm32`) hat dieselbe Datenanordnung wie
ARM32 (Zeiger und `long` 4 Bytes, `double` und `long long` auf 8
ausgerichtet). clang 18 übersetzt dafür; `wasm2c` (wabt) macht daraus
gewöhnlichen C-Code, den das NDK für arm64 übersetzt. Der Speicher der
Sandbox ist ein Block von bis zu 4 GB; MEM1 läge darin wieder bei 0x80000000.

| Messung | Ergebnis |
|---|---|
| Strukturgrößen wasm32 gegen ARM32 (`layout_compare.py --targets arm-linux-gnueabihf,wasm32-wasi`) | **0 von 1285 verschieden** |
| Übersetzen von Spiel, Modulen, SDK, CRI für wasm32 (`lp64_probe.py --target wasm32-wasi`) | **keine Zeigerfehler**; SDK und CRI fehlerfrei. Im Spiel eine einzige Ursache: `size_t` heißt dort `unsigned long` statt `unsigned int` – zwei Header-Zeilen (`joy.h` `memcpy`, `db_toolbase.h` `strlen`, 116 Folgefehler), 9 `memcpy`-Aufrufe und ein `operator new` in drei Debug-Werkzeugen; mechanisch (eine Umschreibung) |
| Übersetzen der Plattformschicht | rund 320 Fehler in etwa 12 Dateien, die Threads, Dateien, `dlopen`, `mmap` und den Stackwechsel des Betriebssystems benutzen (`core_api`, `gx_decode`, `stack_context`, `module_loader`, `mem1`, `os_thread`, `ai`, `pad`, `log`, `os_link`, Host-Programm und Tests): genau der Teil, der außerhalb der Sandbox bleiben würde |
| Geschwindigkeit (`wasm_bench.sh`: Objekte mit virtuellen Funktionen in Listen, Matrizen wie `C_MTX`, Entpacken, Speicherkopien) | gleiches Ergebnis; **1,65-fache Zeit** gegenüber nativ `-O2` (0,168 s gegen 0,102 s, bestes von 7, x86_64) |

Was dieser Weg braucht:

1. Die Plattformschicht teilen: alles, was Spielspeicher und Spielstrukturen
   anfasst (Formatumwandler, OS-Nachbau, DSP/AI, Karte, GX-FIFO-Schreibseite,
   Zustandsbrücke), kommt mit in die Sandbox; Threads, Dateizugriff, Zeit,
   Protokoll, Tonausgabe, der GX-Decoder (liest den Sandbox-Speicher über
   seine Basisadresse) und die JNI-Schnittstelle bleiben nativ.
2. Threads des Spiels: jeder emulierte Thread bekommt wie heute einen
   eigenen nativen Stack (Stackwechsel außerhalb der Sandbox) und dazu einen
   eigenen Schattenstack im Sandbox-Speicher (wasm legt adressierte lokale
   Variablen dort ab); beim Wechsel wird der Zeiger darauf mitgetauscht.
3. Die 114 REL-Module: heute je eine `.so`; in der Sandbox ein gemeinsames
   Abbild, dessen Moduldaten beim Laden eines Moduls frisch gesetzt werden
   (wie heute durch `dlopen`).
4. Die Rückrufe des Spiels (Alarm, DVD, AX, …) ruft die native Seite über
   die Funktionstabelle der Sandbox auf.

Geschwindigkeit auf dem Gerät (Schätzung, nicht gemessen): der Spielthread
braucht auf dem Retroid (Snapdragon 865, ARM32 nativ) rund 19 ms von 33,3 ms
je Spielbild. Mit Faktor 1,5–1,8 wären es 29–34 ms: auf einem Gerät dieser
Klasse knapp an der Grenze, auf neueren Geräten (die reinen 64-bit-Geräte
sind neuere) im Rahmen. Der erste Schritt dieses Wegs müsste das auf dem
Retroid messen.

Wichtig für „eine APK“: Enthält eine APK beide ABIs, nimmt Android auf
Geräten, die beide können (Retroid), normalerweise **64 bit**. Die
Manifest-Angaben `android:multiArch="true"` und `android:use32bitAbi="true"`
lassen solche Geräte bei 32 bit (dann der schnelle native ARM32-Kern); reine
64-bit-Geräte nehmen die Sandbox. Ob Android das bei einer normalen
(nicht System-)App beachtet, zeigt ein kurzer Gerätetest.

## 3. Weitere Wege

- **ARM32-Code in einer 64-bit-App emulieren:** gemessen mit qemu
  (ARM32-Kern unter `qemu-arm` gegen nativ, Filmtest): **6-fache Zeit**
  (1,8 s gegen 0,3 s). Der Kern braucht nativ schon 58 % eines Kerns:
  ausgeschlossen.
- **ARM64 mit 32-bit-Zeigern (ILP32):** clang 18 kann es für ELF nicht
  (`aarch64-linux-gnu_ilp32` bricht ab), `aarch64_32` gibt es nur für Apple
  (Mach-O), Android lädt beides nicht: ausgeschlossen.
- **Zeigerfelder einzeln auf 32 bit festlegen (`__ptr32`):** nur
  Microsoft-Erweiterung für x86; die Zeiger auf die Tabelle virtueller
  Funktionen in jedem Objekt blieben 8 Bytes: ausgeschlossen.

## 4. Der bisherige ARM64-Modus der App

Er ist eine eigene App (`com.ast.re4decomp`, Variante `classic`, nur
`arm64-v8a`); der Kernmodus ist `com.ast.re4decomp.core`. M12 ändert nur die
Kern-App; die klassische bleibt baubar wie bisher, bis der Projektinhaber
sie ablöst.

## 5. Vergleich

| | A: nativ 64 bit, Adressen unter 4 GB | W: 32-bit-Sandbox (wasm32 → wasm2c → arm64) |
|---|---|---|
| Übersetzen | ~40 Fehler (mit `-fms-extensions`), 6000+ stille Kürzungen | Spiel/SDK/CRI ohne Zeigerfehler, eine mechanische `size_t`-Umschreibung |
| Datenanordnung | 835 von 1293 Strukturen anders: feste Plätze (Figuren, Objekte, Treffer), Spielstand-Kopf, Dateistrukturen mit Zeigerfeldern (Stichprobe 5 von 14) müssen auf 32-bit-Form gebracht werden | 0 von 1285 anders, wie ARM32 |
| Risiko | hoch: jede übersehene Struktur ist ein stiller Speicherfehler, erst im Lauf (alle 126 Räume neu prüfen) | mittel: die Geschwindigkeit; die Spiellogik ist dieselbe wie auf ARM32 |
| Arbeit | groß, schwer abschätzbar (Format für Format, Raum für Raum) | groß, aber planbar: Plattformschicht teilen (~12 Dateien), Threads/Schattenstacks, Module, Rückrufe |
| Geschwindigkeit arm64 | nativ | geschätzt 1,5–1,8-fach (Retroid-Klasse knapp, neuere Geräte gut) |
| ARM32 | bleibt nativ | bleibt nativ; mit `use32bitAbi` laufen 32-bit-fähige Geräte weiter nativ |
| Cloud-Prüfung | 64-bit-Linux-Build | dieselbe Sandbox läuft als 64-bit-Linux-Programm: PC-Prüfung mit Disc-Daten und Rundgang prüfen genau den Code des Geräts |
| Regeln (src/ unverändert) | viele Ersatz-Header/Patches für Strukturen | keine Änderung an Spielstrukturen nötig |

Mischform C: Weg W, danach heiße Stellen (Matrizen, Skinning, Speicherkopien,
GX-Schreiben) als native Funktionen außerhalb der Sandbox, falls die
Geschwindigkeit auf dem Gerät nicht reicht.

**Empfehlung:** Weg W (Mischform C, falls nötig). Er erhält die Spiellogik
genau so, wie sie auf ARM32 schon in allen 126 Räumen läuft, und lässt sich
in der Cloud vollständig prüfen. Das Risiko ist die Geschwindigkeit; sie wird
im ersten Schritt auf dem Retroid gemessen. **Entscheidung des Projektinhabers (17.10.2026): Weg A, nativ 64 bit.**
