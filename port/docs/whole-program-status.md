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
| K2 „Kern in die App“ (M6) | ✅ 16.10.2026 (Retroid): ARM32-App mit den Kern-Bibliotheken, Titel, r100, r101, Untermenüs und Karte spielbar; Abschluss und offene Punkte: „Abschluss M6/M6b“ |
| M6b GX nachbilden (`gx-plan.md`) | ✅ 16.10.2026 (Retroid): das Bild kommt aus der GX-Nachbildung (Logos, Titel, Räume, Figuren, Effekte, Menüs, Anzeigen); r100 und r101 flüssig bei etwa 1080p (`-Scale 3`) |
| M7 Steuerung | ✅ 16.10.2026 (Retroid, Branch `claude/re4-m7-steuerung`): alle Tasten, Sticks und L2/R2 kommen als GameCube-Controller an (Original-`PADClamp`, Belegung nach Beschriftung), Menüs bedienbar, Vibration spürbar; Touch später („M7 Steuerung“) |
| M8 Audio | ✅ 16.10.2026 (Retroid, Branch `claude/re4-m8-audio`): Originalton über die SDK-Bibliotheken auf nachgebautem DSP/AI, Hall/Chorus in C, Tonausgabe der App; klingt laut Projektinhaber sauber. Vereinzelt abgebrochene Schüsse (AX-Budget) behoben, Nachtest im ersten Gerätelauf von M9 („M8 Audio“) |
| M9 Videos | ✅ 16.10.2026 (Retroid, Branch `claude/re4-m9-videos`): die Filme laufen über die CRI-Middleware (ADX, Sofdec, MPEG) aus `src/lib`, Bild über die GX-Nachbildung, Ton synchron, START bricht ab; Vorfilm vor der Autofahrt und Demo-Filme des Titels auf dem Gerät, alle 13 Filme der Discs in der PC-Prüfung ohne Fehler. Dabei behoben: Ruckler (Takt an den 60-Hz-Bildschirm gekoppelt), abgebrochene Töne (SYN, Patch 0056, 0 Abbrüche), Helligkeit. **Nicht auf dem Gerät geprüft:** `VISetBlack` (der Titel blitzte vor dem Demo-Film kurz auf) („M9 Videos“) |
| M10 Speichern/Laden | ✅ 17.10.2026 (Retroid und PC-Prüfung, Branch `claude/re4-m10-speichern`): die CARD-Bibliothek aus `src/lib` läuft auf einer nachgebildeten Speicherkarte (Datei im Speicher der App, nur für den Port); neue Karte vorformatiert, Startprüfung echt, aber ohne Meldungen (Systemdatei still angelegt); Speichern an der Schreibmaschine, Laden im Titel und im Pausemenü, „Retry“, Optionen über einen Neustart erhalten, Spielstände mit Banner und Symbol. Dabei behoben: Hänger beim Neustart eines Raums mit laufender Musik (Patch 0059) („M10 Speichern/Laden“) |
| M11 Android-Feinschliff | ✅ 17.10.2026 (Retroid und PC-Prüfung, Branch `claude/nifty-feynman-jv729h`, Stand `093aa7a`): Menü der App über SELECT (Grafikoptionen, FPS-Anzeige, „Depth of field“ mit dem Autofokus der ersten App, Speicherkarte als `.gci` für Dolphin/GameCube, Fehlerprotokoll für Tester, alles auf Englisch), SELECT halten = Debug-Menü des Spiels, Diagnose-Anzeige ausgebaut, Spiel steht im Menü und im Hintergrund. Rundgang über alle Räume (PC mit Disc-Daten): 126 von 126 laufen (Patches 0062–0064). Super Magnum: kein Port-Fehler (Nachlade-Stufe Lv.5 aus dem Debug-Menü), mit Lv.1 bestätigt. Debug-Menü Waffenseite stürzte am Listenende ab (Patch 0066). Leistung: 60 Bilder/s, Kern 33,3 ms je Spielbild mit 13–15 ms Leerlauf, GPU um 20 ms („M11 Android-Feinschliff“) |
| M12 ARM64 | in Arbeit (Branch `claude/re4-m12-arm64`, seit 18.10.2026 `claude/exciting-bohr-6akwfd`): A1 ✅ 64-bit-Linux (x86_64) baut und linkt; A2 ✅ Adressen unter 4 GB; A3 ✅ Strukturen, die auf 64 bit auseinanderlaufen: alle Prüfwerkzeuge 0 (Patches 0070–0073, Regel `lp64_sizes`); A4 ✅ alle Cloud-Tests grün auf i686, armhf, x86_64 und **aarch64 (neu, ARM64 unter qemu)**, auch Filme und Speicherkarte; A5 Android arm64 eingebaut, aber ungeprüft (kein NDK in der Cloud). 18.10.2026: PC-Prüfung `-Lp64` läuft r100 10.000 Bilder ohne Absturz, Speichern stürzte ab (Budget des Speicherbildschirms, behoben, unbestätigt); Neustart mit Bestandsaufnahme aller Prüfwerkzeuge (libclang statt Wörter, Selbsttest, `run_all.py` + `reviewed.json`), vier neue Werkzeuge, Patch 0082 (fünf echte Fehler), Vergleichslauf 32/64 bit (`-Compare`, in der Cloud mit dem synthetischen Film gleich; mit Disc-Daten offen). Offen: `-Compare` und `-Lp64` mit Disc-Daten, Gerätetest beider ABIs. Dabei gefunden: Puzzle-Untermenü las auch auf 32 bit falsche Felder (Patch 0073). Bestandsaufnahme `arm64-bestandsaufnahme.md`. **Entscheidung des Projektinhabers (17.10.2026): Weg A, nativ 64 bit** |
| Reihenfolge | Entscheidungen des Projektinhabers: erst M9 Videos und M10 Speichern/Laden (16.10.2026), dann **M11 vor dem Raum-Rundgang 10b** (17.10.2026: die APK soll an Tester gehen), dann **M12 ARM64 vor 10b** (17.10.2026: ein Tester hat ein Gerät nur für 64-bit-Apps). Gestrichen: Baum-Kollision in r100 (G17) |

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
| `--core-test` | Einbettungs-Schnittstelle der App ohne Disc-Daten (`port/whole/tests/core_tests.cpp`, `core_probe.cpp`): `bridge` Schnappschuss aus einem nachgebauten Gegner (EmMgr, Teile, Modell-Info, Kamera, Raum), `thread` Kern-Thread mit Testeinstieg, Controller über `re4_core_pad`, Dreifachpuffer; seit M10 `card` die CARD-Bibliothek auf einer Kartendatei (`tests/card_probe.cpp`, siehe „M10 Speichern/Laden“) |
| `memcard_list.py` (M10) | liest die Karte des Kern-Tests zurück (`RE4_CARD_TEST_FILE`): Dateien, Banner/Symbol, die Prüfsummen des Spiels (`--expect-system --expect-save 0`) |
| `--runtime-test` | Plattformschicht ohne Disc-Daten (`port/whole/tests/runtime_tests.cpp`): `link` OSLink/OSUnlink mit synthetischem big-endian REL-Kopf und Testmodul; `dvd` DVD-API über einen synthetischen Zwei-Disc-Ordner; `yz2` Yz2 des Spiels + Kern mit den Testdaten `port/tests/yz2_fixtures.h`; `dvdq` die DVD-Warteschlange des Spiels liest eine synthetische mehrteilige Datei in den Spiel-Heap; `stream` der Stream-Code des Tontreibers spielt synthetische Stream-Dateien (Nachladen im Audio-Rahmen, Aufträge aus dem Interrupt enden erst danach); `math` PS*/C_*-Matrixfunktionen, die C-Fassungen der Gekko-Assembler-Funktionen (Tabelle mit Bitmustern aus einem Python-Modell der Paired-Single-Befehle), Skinning `CalcSk1` mit GQR-Quantisierung, `EmRoutineSetW`, die big-endian Sichten, die Länge von `__FILE__`; `trace` Zustands-Trace, GetDataExt-Abdeckung, Ende des Test-Tastenskripts; `tasks` der Task-Scheduler des Spiels (`TaskExec`, `TaskSleep`, `TaskExit`, `iTaskExec`) auf der Thread-Emulation, dazu VI-Retraces; `vsync` (seit M9) die Retraces folgen dem Vsync eines 60-Hz-Bildschirms |
| `--movie-test` (M9) | `port/tools/test_whole_movie.py`: ein synthetischer Sofdec-Film (ffmpeg: MPEG-1 mit B-Bildern, Stereo-ADX mit zwei Tönen; `port/tools/make_test_sfd.py`) durch die CRI-Middleware wie `cSofdec` (`tests/movie_probe.cpp`): Bilder gegen ffmpeg, Reihenfolge, Takt, Ton je Kanal, Ton/Bild-Abstand, Bild 10 durch den GX-Renderer der App (wenn `port/build/app-host-gl` gebaut ist), derselbe Film im Schnelllauf (`RE4_VI_FAST`) und mit 60,0-Hz-Halbbildern (Bildschirmtakt) |
| `re4_format_compare` | Formatumwandler gegen die geprüften big-endian Leser der App, mit synthetischen Beispielen jedes verglichenen Formats; danach `port/tools/test_whole_format_compare.py`: dieselben Beispiele als Disc-Ordner (Raumdatei Yz2-gepackt, `.drs`-Container), Modus `--data` (siehe „Formatvergleich mit der App“) |

Danach einmal `port/tools/test_whole_disc_folder.py` (Disc-Ordner-Werkzeug mit
synthetischen Disc-Abbildern). Rückgabewert 0 heißt: alles bestanden.

Nach Bedarf (seit M9): `port/tools/whole_ir_signatures.py port/build/whole-i686`
übersetzt jede Datei des Builds nach LLVM-IR und vergleicht jede Deklaration
einer C-Funktion mit ihrer Definition (Zahl der Parameter, Ergebnis ja/nein);
so fallen Aufrufe auf, die auf dem GameCube von Werten lebten, die zufällig in
einem Register standen (etwa 40 s).

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
`classic` = bisherige App (arm64-v8a, `com.ast.re4decomp.classic`, bis
17.10.2026 `com.ast.re4decomp`, Standard in Android Studio), `core` =
Kernmodus (armeabi-v7a und seit M12 arm64-v8a, `com.ast.re4decomp`, bis
17.10.2026 `com.ast.re4decomp.core`, Name „Resident Evil 4 Android“, bis
17.10.2026 „RE4 Kern“, ab Android 6.0, CMake `-DRE4_CORE_MODE=ON`). Im Kernmodus startet „Start
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
  **Seit 17.10.2026 (Wunsch des Projektinhabers) überspringt die App das
  Startmenü** (`core_mode.cpp`: Voreinstellung `RE4_ROOM_MENU=0`): „New
  Game“ startet das Spiel in r100 wie ein sofortiges A; Räume wählt man im
  Debug-Menü im Spiel (SELECT halten). `test-core-app.ps1 -StartRoom` und
  die PC-Prüfung setzen `RE4_ROOM_MENU=1` weiter ausdrücklich.
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
  Bericht zeigt die R3-Zeilen; `-Scale 1..4` wählt die interne Auflösung
  (`RE4_GX_SCALE`; 2 = Standard, sichtbar etwa 720p, 3 = etwa 1080p).

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2` mit allen Prüfungen
(neu: kurzer Tastendruck im Test `core-test`, Debug-Seite bei offenem
Startmenü im Test `trace`, zusammengefasste Aufträge im Test `gx`),
Bildvergleich der zusammengefassten Gerätebilder. Nicht getestet: Gerät,
PC-Prüfung mit Disc-Daten (Karte, Neustart nach Spielende), das sichtbare
Raumauswahl- und Werkzeugmenü.

### Siebter Gerätelauf mit GX (16.10.2026, Retroid) und Analyse

Stand `06dead6` (zusammengefasste Zeichenaufträge, sichtbares
Raumauswahl-Menü), interne Auflösung `-Scale 3` (etwa 1080p). Start über das
Raumauswahl-Menü direkt nach r100. Absturz nach 105 s beim Untersuchen der
Totenköpfe unter der Treppe. Laut Projektinhaber funktioniert dasselbe beim
normalen Start (über r120); die Karte (L1/R1) bleibt weiterhin schwarz und
lässt sich nicht schließen (nicht in diesem Protokoll).

- **Leistung in r100 bei 1080p:** Kern 33,9 ms je Bild (0,7 ms Warten auf
  den Renderer), Renderer 6,1 ms je Bild bei 332 Aufträgen (vorher 10–15 ms
  bei 8 000–15 000 Aufträgen und doppelter statt dreifacher Auflösung);
  einzelne Spitzen bis 216 ms beim ersten Auftreten neuer Shader.
- **Absturz (Patch 0044):** `r100_mes_gaikotu` → `SceUpCut` → `CutCall` →
  `switchCamera` → `delete m_pProc` mit Fehleradresse 0x0909090d: der
  vtable-Zeiger des Kamera-Objekts war 0x09090909, das Füllmuster aus
  `CameraControl::roomInit` (`memset(m_Free, 9, …)`). `roomInit` setzt
  `m_pProc = NULL`, ruft dann `Check()` – das bei `d.y <= 500` (Hüfte nicht
  über den Füßen, beim Direktstart noch ohne berechnete Teile) die
  Fall-Kamera `CameraLookAt` in `m_Free` baut – und füllt erst danach
  `m_Free`. Damit zeigte `m_pProc` auf ein überschriebenes Objekt; der
  nächste Kamera-Schnitt mit Kamerafahrt (Typ 6/7) löschte es. Auf dem
  GameCube wäre das genauso abgestürzt; dort trat es offenbar nicht auf
  (beim normalen Start läuft `roomInit` nicht in diesem Zustand). Patch 0044
  füllt `m_Free` vor `Check()`. Diagnose: „run: camera after the room start
  …“ (state_trace.cpp), im PC-Bericht „Kamera:“, im Gerätebericht „Kamera
  nach dem Raumstart“.
- **Karte schwarz, nicht schließbar:** Die Karte liest beim Öffnen
  `SS/<Sprache>/ss_map.dat` (`SsMapInit::move`, Zustand 2/3) und wartet auf
  das Ende des Lesens; erst danach reagiert sie auf B/Z. Ein Lesen, das nie
  fertig wird, erklärt beides. Neu im Protokoll: „run: sub screen open/closed
  at frame …“; die PC-Prüfung meldet „Untermenüs“, „Kartendatei“ und ein am
  Ende noch offenes Untermenü, der Gerätebericht ebenso. Ursache noch offen.

Getestet (Cloud): Kern-Build i686/armhf normal und `-O2` mit allen
Prüfungen; Berichtszeilen der PC-Prüfung mit einem nachgebauten Protokoll.
Nicht getestet: Gerät, PC-Prüfung mit Disc-Daten.

### Achter Gerätelauf (16.10.2026, Retroid): Karte

Stand `9f41b98` (Patch 0044). Intro übersprungen, Karte sofort geöffnet
(auf dem Gerät kam die Taste als GameCube-Z an). Kurz erscheinen die Anzeigen
und der gelbe Kartenrahmen, dann bleibt das Bild schwarz; B schließt nicht.

- **Protokoll:** Untermenü offen ab Bild 1469 und nie geschlossen. Gelesen
  wurden `SS/eng/ss_map.dat` und `SS/cmn/map_obj1.dat`; dazu „archive entry
  "MAP" … no converter, left big-endian“. Danach zeichnete das Spiel nur noch
  5 Aufträge je Bild (ein schwarzes Vollbild, Aufnahme Bild 1769), der Kern
  lief weiter mit 33,9 ms je Bild. Kamera nach dem Raumstart: „none“ (Patch
  0044 nicht berührt).
- **Ursache:** Die Einträge „MAP“ sind die Räume der Karte (Sscrn
  `ss_map.cpp` `mapRoomNum` / `mapBinAddr` / `mapHitAddr`): u32 Anzahl
  (Modelle + 2), drei Wörter, die die Karte nicht liest, dann die Offsets der
  Modelle (BIN) und der zwei Bodenkollisionen (`cSatHeader`). Unumgewandelt
  liest `mapModelInit` eine Modellanzahl in Millionen und Modelle aus
  big-endian-Daten. Neuer Umwandler `convertMapRoom` (formats_scene.cpp,
  Regel „MAP“ in formats_arc.cpp): Kopf und Offsets tauschen, Modelle mit
  `convertBin`, Böden mit `convertSat`; unplausible Anzahl/Offsets bleiben
  unverändert und werden protokolliert.
- **Prüfung:** Formatvergleich mit einem synthetischen Kartenraum (ein Modell,
  zwei Böden; App-Leser gegen Kern-Umwandler, 154 Werte gleich) und als
  Datei `SS/cmn/map_obj1.dat` im synthetischen Disc-Ordner; mit echten
  Disc-Daten vergleicht `re4_format_compare --data` jetzt auch alle
  `SS/cmn/map_obj*.dat` (lokal, steht aus). Ob die Karte damit erscheint und
  sich schließen lässt, zeigt erst der Gerätelauf oder die PC-Prüfung
  („Untermenüs“, „Karte offen“).

### Neunter PC-Lauf (16.10.2026, WSL2, Stand `c6cd51f`)

- **Karte:** geöffnet mit Z bei Bild 3074, mit B bei 3355 wieder
  geschlossen; das GX-Bild der offenen Karte (Bild 3244) hat 135 Aufträge,
  4 286 Dreiecke und 28 Texturen (vorher auf dem Gerät: 5 Aufträge, schwarz).
  Mit dem Umwandler `convertMapRoom` öffnet und schließt die Karte also am
  PC; auf dem Gerät nicht geprüft.
- **Formatvergleich:** 155 Dateien ohne Fehler; die 18 `SS/cmn/map_obj*.dat`
  fehlten („kein DVD-Container mit Archiv an 0x400“): ihr Archiv ist der
  erste Teil der Kopftabelle, nicht der zweite wie bei `.drs`. Der Vergleich
  nimmt jetzt ersatzweise den ersten Hauptspeicher-Teil (`drsBody`), geprüft
  mit einem synthetischen Container dieses Aufbaus.
- **Soft-Reset (Tod → „nicht weiterspielen“) hängt (Patch 0045):** Stillstand
  nach `--SOFT_RESET START!!`; der laufende Kontext stand in
  `SndSoftReset` → `Snd_iss_control` → `Snd_dev_voice_ck`. `SndSoftReset`
  wartet in einer leeren Schleife, bis die Ton-Abspieler im Audio-Interrupt
  den Reset bestätigen (`Snd_soft_reset_ck`). Der Port liefert Interrupts
  nur an Wartepunkten aus; in dieser Schleife gab es keinen. Patch 0045 ruft
  darin `re4_wait_interrupt()` auf (wie Patch 0012 für die VSync-Schleife).
  Das erklärt den schwarzen Bildschirm auf dem Gerät nach dem Tod
  (`systemSoftReset` schaltet vorher auf Schwarz). Ob danach der Titel
  zurückkommt, zeigt der nächste PC-Lauf („Titel nach dem Soft-Reset“).

### Zehnter PC-Lauf (16.10.2026, WSL2, Stand `a9b1f20`)

- **Soft-Reset:** mit Patch 0045 läuft er durch („--SOFT_RESET END!!“ ein
  Bild später), das Spiel liest `title.dat` neu und steht am Ende im Titel
  (Raum 0x120, Schritt 0). Die Prüfung meldete trotzdem „Ziel nicht
  erreicht“, weil sie das Ende in r100 erwartete; nach dem angeforderten
  Soft-Reset gilt jetzt das Ende im Titel (die Rückkehr prüft der
  Soft-Reset-Abschnitt).
- **Kartendateien im Formatvergleich:** wieder „FEHLT“; die Karte liest
  `SS/cmn/map_objNN.dat` ohne Teile-Tabelle (`DvdReadN` ohne Kopf-Bit), die
  Datei ist selbst das Archiv. Der Vergleich nimmt sie jetzt so (die
  Container-Annahme vom neunten Lauf ist zurückgenommen).
- **Dunkle Fläche am Boden (Projektinhaber, GX-Bilder 901 und 922):** In der
  Draufsicht auf das Auto (Ereignis r100s40, Schnitt 1) liegt eine
  scharfkantige, gleichmäßig dunkle Raute auf dem Boden. Diese Bilder haben
  13 statt 9 EFB-Kopien: der weiche Schatten (`shadow.cpp`
  `make_shadow_texture`: Umriss mit konstantem Ziel-Alpha ins EFB, Kopie als
  A8 → I8, `MakeSoftShadow` mit weiteren Kopien) wird auf den Boden
  projiziert. Eine gleichmäßig dunkle Fläche heißt, die Schattentextur ist
  überall „Schatten“ statt nur im Umriss. Ursache offen; die PC-Prüfung
  legt Bild 901 jetzt als Daten bei (`RE4_GX_KEEP`, Standard 901).

### Elfter PC-Lauf (16.10.2026, WSL2, Stand `352677d`): „ALLES OK“

- Formatvergleich 173 Dateien ohne Fehler, darunter alle 18
  `SS/cmn/map_obj*.dat` (die Räume der Karte, Umwandler `convertMapRoom`);
  Soft-Reset kehrt zum Titel zurück; Karte öffnet und schließt.
- **Dunkle Fläche am Boden (Bild 901/922) geklärt:** Mit den Daten von Bild
  901 schrittweise nachgezeichnet. Der Schattendurchgang (`shadow.cpp`
  `make_shadow_texture`) schreibt nur das Ziel-Alpha der Werfer (hier 0xAD
  unter dem Auto) in einen 256×256-Bereich des EFB und verlässt sich darauf,
  dass dort sonst Alpha 0 steht: Jede Display-Kopie des Spiels löscht das EFB
  mit der Löschfarbe Schwarz, Alpha 0 (`bio4_GXSetCopyClear`), bei
  eingeschaltetem Alpha-Schreiben. Der Renderer legte das EFB beim Start aber
  mit Alpha 255 an; ein einzeln gezeichnetes Bild (PC-Prüfung,
  `gx_chunk_render`) hatte deshalb im ganzen Lichtkegel „Schatten“. Jetzt
  beginnt das EFB mit Alpha 0 (`gx_gl.cpp` `Init`). Bild 901 zeigt den Boden
  danach normal; die übrigen Bilder sind gleich bis auf die Randzeilen des
  Auto-Innenraums (Bild 652, 1 %, ebenfalls Alpha-abhängig). In der App
  laufen die Bilder hintereinander, dort löscht schon die Display-Kopie des
  Vorbilds das Alpha; auf dem Gerät war der Fleck daher vermutlich nicht zu
  sehen (nicht geprüft).

### Neunter Gerätelauf (16.10.2026, Retroid, Stand `490b78d`)

Normaler Start (Raumauswahl-Menü nicht benutzt), `-Scale 3`. Karte (Z) öffnet
und schließt jetzt auch auf dem Gerät. Im Menü „FILES“ flackert es laut
Projektinhaber kurz beim Öffnen und danach im Hintergrund. Absturz nach 139 s
beim Sprung aus dem Fenster.

- **Absturz (Patch 0046):** `CameraMove` → `CameraControl::Check` →
  `checkAttachCamera` → `delete m_pProc`, Fehleradresse 0x0909090d. Kamera nach
  dem Raumstart „none“, also nicht der Fall von Patch 0044. Allgemeine Ursache:
  Die Kamera-Unterklassen füllen ihr Objekt im Destruktor mit
  `memset(this, 9, 0x200)`; `CameraControl` löscht eine beendete Kamera, lässt
  `m_pProc` aber gesetzt und löscht sie beim nächsten Kamerawechsel noch einmal.
  Mit GCC 2.95 war das harmlos: `~cCamera() {}` schreibt den vtable-Zeiger von
  `cCamera` zurück, das zweite `delete` landet im löschenden Destruktor von
  `cCamera` (leerer `operator delete`). Mit clang scheitern beide Schritte:
  Bei leerem Rumpf lässt clang den Schreibbefehl weg (das zweite `delete`
  sprang über 0x09090909), und der löschende Destruktor einer abstrakten Klasse
  ist bei clang ein Haltebefehl (SIGILL). Patch 0046: `~cCamera` übergibt das
  Objekt an `re4_keep_vtable` (runtime/dolphin_os.cpp, tut nichts, für den
  Compiler undurchsichtig), `move()` hat einen leeren Rumpf statt rein virtuell
  zu sein (`cCamera` selbst wird nie angelegt). Prüfung
  `re4probe_camera_double_delete` im Kern-Test (zweimal `delete` wie im Spiel):
  ohne Patch „check failed“, mit Patch bestanden (i686/armhf, normal und
  `-O2`). Das erklärt auch den Absturz beim Untersuchen der Totenköpfe
  (siebter Gerätelauf); Patch 0044 bleibt (schadet nicht). Das
  Debug-Startmenü war nicht die Ursache.
- **Kern-Build auf dem PC abgebrochen (Stand `972a857`):** Die erste Fassung
  von Patch 0046 nutzte eine leere `asm`-Anweisung; `whole_prepare.py` lässt
  Inline-Assembler ohne C-Ersatz nicht zu, CMake brach ab. Das Android-Skript
  meldete „0 failed build steps“ ohne Grund; `whole_build.sh` in der Cloud
  wertete den Abbruch gar nicht aus und prüfte die Programme des vorigen
  Stands. Beide Skripte werten jetzt den Rückgabewert von CMake/ninja aus und
  zeigen in diesem Fall das Ende des Protokolls.
- **FILES-Flackern:** Die R3-Aufnahme (Bild 3423, FILES offen, 161 Aufträge)
  zeichnet einzeln richtig; die Zahl der gehaltenen Bilder blieb während des
  Untermenüs gleich (37). Ein Flackern zeigt sich erst über mehrere Bilder:
  R3 nimmt jetzt eine Folge auf, das Bildstück mit vollem Zustand und die
  11 direkt danach (`core_mode.cpp`, `gx-geraet-NN-KK.bin`; bricht ab, wenn
  ein Bildstück fehlt). `gx_chunk_render` zeichnet ein Bildstück, das direkt
  auf das vorige folgt (nächste Folgenummer und nächstes Bild), mit demselben
  Renderer weiter wie die App (EFB, Kopien und Texturen bleiben, ein
  angehaltenes Bild lässt das vorige stehen); ein Folgebild ohne Vorgänger
  wird übersprungen und gemeldet. Geprüft mit einer nachgebauten Folge aus
  der Aufnahme von Bild 3423 (Folgebilder pixelgleich, Einzelbilder wie
  vorher) und dem Link-Test der App.

### Zehnter Gerätelauf (16.10.2026, Retroid, Stand `9313333`)

Normaler Start, `-Scale 3`, 388 s, am Ende Tod und Soft-Reset zurück zum Titel.
Sprung aus dem Fenster ohne Absturz (Patch 0046 auf dem Gerät bestätigt).

- **FILES-Flackern:** vier R3-Folgen zu je 12 Bildern (Titel, zweimal FILES,
  r101), alle mit `gx_chunk_render` als Folge nachgezeichnet. In den
  FILES-Folgen ändern sich von Bild zu Bild nur Leons Animation und das
  pulsierende „Village Files“; kein gehaltenes Bild, keine Kopien. Das
  Bildschirmfoto des Geräts (Bild 145 s) zeigt dasselbe. Ein Flackern ist in
  den Daten nicht zu sehen; Ursache offen (eine Beschreibung oder ein Video
  vom Projektinhaber hilft).
- **Leistung r101 (Dorf):** Kern 34–38,6 ms je Bild (r100: 33,4, also an
  der 30-Bilder-Grenze), Renderer 21–36 ms je Bild bei 650–920 Aufträgen
  (r100: 7–9 ms bei 400–550), gezeigt 27–41 Bilder/s. Die R3-Folge in r101:
  946 Aufträge, 138 000 Dreiecke, 7,9 MB Punktdaten je Bild und rund 1 200
  Uploads der Uniform-Blöcke (XF/PX) mit `glBufferSubData` (1,7 MB). Mobile
  Treiber kopieren einen Puffer, den die GPU noch liest, bei jedem solchen
  Update.
- **Uniform-Ringpuffer (`gx_gl.cpp`):** Jeder geänderte XF- oder PX-Block
  kommt an eine neue Stelle eines 16-MB-Puffers und wird dort mit
  `glBindBufferRange` gebunden (wie bei Dolphin). Dauerhaft eingeblendet
  (`EXT_buffer_storage`), sonst eine unsynchronisierte Einblendung je Block;
  ein Fence schützt jedes Viertel vor dem Überschreiben.
  `RE4_GX_UNIFORM_STREAM=0` schaltet zurück, `RE4_GX_PERSISTENT=0` nimmt die
  Einblendung je Block. Alle 48 Bilder der Folgen sind in beiden Arten
  pixelgleich zu vorher (llvmpipe).
- **Messungen:** Die App protokolliert je 300 Bildstücke „GX work per chunk“
  (Uniform-Uploads, Programmwechsel, Textur-Bindungen, Zustandswechsel,
  Wartezeiten des Ringpuffers, GPU-Zeit über `EXT_disjoint_timer_query`), der
  Kern in „GX core“ zusätzlich die Leerlaufzeit (Warten auf den nächsten
  Retrace, `re4_idle_ns`), FIFO- und Display-List-Bytes und Punkte je Bild.
  Im Gerätetest misst Taste P 15 s lang mit simpleperf aus dem NDK, wo die
  Rechenzeit hingeht; `port/tools/android_profile.py` ordnet die Proben in
  WSL mit den Bibliotheken des Builds Funktionen zu (`profil-N.txt`).
  `gx_chunk_render` zeigt mit `RE4_GX_WORK=1` die GL-Arbeit je Bild.

### Elfter Gerätelauf (16.10.2026, Retroid, Stand `a30abf2`): Messung in r101

Gerät: Snapdragon mit Adreno-GPU (`libGLESv2_adreno.so`). Normaler Start,
`-Scale 3`, zwei Leistungsmessungen mit Taste P im Dorf (r101).

- **Uniform-Ringpuffer wirkt:** Renderer 4–5,7 ms je Bild bei 500–900
  Aufträgen (vorher 21–36 ms), gezeigt 60 Bilder/s, GPU 8–15,6 ms je Bild
  (Timer-Abfrage), 0 Wartezeiten im Ring, dauerhafte Einblendung aktiv.
- **Der Kern ist die Bremse:** 33,4–37,9 ms je Bild (30 Bilder/s brauchen
  33,3 ms), 4–6 ms Leerlauf. simpleperf: Der Kern-Thread rechnet 80–97 %
  eines Prozessorkerns, davon 68–71 % aller Proben in `libre4_platform.so`,
  nur 4–6 % in der Spiellogik (`libre4_main.so`). Größte Posten: der
  GX-Decoder `draw()` (Punkte umrechnen) 31–35 %, `re4gx::memory` 10–11 %
  (für jedes Attribut jedes Punkts), `bindTextures` 4 %, `describe` 3,5 %
  (Punktformat für jeden Zeichenbefehl neu, für die Größenprüfung sogar
  doppelt), `command`/`drain`/`feed` zusammen gut 7 %, Divisionen 3 %.
  Je Bild: 216–246 KB FIFO, 1 MB Display-Listen, 120 000–148 000 Punkte.
- **Decoder schneller (`gx_decode.cpp`), Ausgabe unverändert:**
  Punktformate je VAT zwischengespeichert (neu nach CP-Schreiben auf VCD,
  VAT, MATINDEX); die Arrays eines Zeichenbefehls einmal in Speicher
  übersetzt (`memorySpan`, gx_fifo.cpp) statt `memory()` je Attribut;
  Komponenten durch je Zeichenbefehl gewählte, spezialisierte Leser
  (Format, Anzahl, Byte-Reihenfolge) direkt in den Punkt geschrieben;
  Indizes und XF-Daten in einem Stück angehängt; `bindTextures` nur nach
  einer Änderung der Textur-Register, einer Palette, einer EFB-Kopie oder
  am Anfang eines Bildstücks. Neuer Test „gx fingerprint“
  (`tests/gx_probe.cpp`): ein zufälliger, fester Befehlsstrom mit allen
  Punktformaten (direkt und indiziert, alle Komponenten- und Farbformate,
  Normalen mit NBT/NBT3, Matrixindizes), allen Primitiven, sofort und in
  Display-Listen, mit Registern, Texturen, Paletten, EFB-Kopien und
  indizierten Matrixladungen; der Hash aller Bildstücke ist der des alten
  Decoders (`02524012c6419af2`, auf i686 und armhf gleich) und wird geprüft.
  Er fand beim Umbau einen Fehler (direkte Normalen mit NBT3).
  `RE4_GX_BENCH=N` misst den Decoder mit einem dorfähnlichen Bild
  (Display-Listen, 120 000 Punkte): i686 `-O2` 2,9 ms statt 8,3–9 ms.
- **Absturzmeldung des Host-Programms:** nennt jetzt Fehleradresse sowie
  `pc` und `lr` mit Bibliothek und Versatz (auf ARM entrollt `backtrace()`
  den Signalrahmen nicht).

### Zwölfter Gerätelauf (16.10.2026, Retroid, Stand `26450c1`)

Laut Projektinhaber läuft r101 jetzt gefühlt so schnell wie r100, für 1080p
in Ordnung. Der dunkle, halbtransparente Balken unten erscheint offenbar nur,
wenn Leon getroffen wird, zu kurz für R3. Das Ergebnis-Archiv war mit den
R3-Folgen zu groß zum Senden (höchstens 5 Dateien).

- **Aufnahme nach einem Treffer:** Der Kern zählt Treffer (`pl_life` sinkt im
  Spiel, state_trace.cpp, Protokoll „run: player hit, life A -> B“,
  `re4_core_player_hits`); die App speichert danach drei einzelne Bilder
  (2, 8 und 16 Bildstücke später, `RE4_GX_DUMP_ON_HIT`, Ordner
  `RE4_GX_DUMP_FOLDER`). Das Testskript schaltet das für einen Treffer ein
  (`-HitDumps`, 0 = aus); der Bericht nennt „Treffer auf Leon“. Nicht auf dem
  Gerät getestet.
- **Kleines Archiv zum Senden:** Das Testskript packt `senden.zip` (Bericht,
  Protokoll, Profile, Absturzberichte, Bilder als JPEG mit 960 Pixel Breite)
  und löscht den entpackten GX-Ordner neben `gx-geraet.zip`; dieses kommt nur
  bis 25 MB dazu.

### Dreizehnter Gerätelauf (16.10.2026, Retroid, Stand `07543f0`): Treffer

Leon wurde gepackt und gewürgt: 38 „Treffer“ (Leben −10 je Bild, ab Bild
1675), danach Tod und „Continue“. Die App speicherte drei Einzelbilder 2, 8
und 16 Bildstücke nach dem ersten Treffer (`gx-geraet-00..02`).

- **Kein Balken in den Aufnahmen:** Nachgezeichnet (auch zweimal
  hintereinander, damit die Kopie des Vorbilds existiert, und mit
  `RE4_GX_SCALE=3` wie auf dem Gerät) zeigen alle drei Bilder keinen dunklen
  Balken. Die Filterkette am Bildende (EFB halb kopiert, auf 128×112 und
  64×56 verkleinert und weichgezeichnet, über das Bild gelegt; das Vorbild
  als Kopie 256×224 zurückgezeichnet) ist auch in Dorf-Bildern ohne Treffer
  da, ist also nicht der Treffer-Effekt. Der Balken blitzt vermutlich im
  Moment des Treffers auf, die Aufnahme kam zu spät.
- **Genauer aufnehmen:** Bei einem neuen Treffer (Leben sinkt nach mehr als
  60 Bildern ohne Treffer) markiert der Kern das gerade entstehende
  Bildstück (`re4_gx_mark`, `RE4_GX_CHUNK_MARK`, gx_decode.cpp) und lässt
  das nächste neu beginnen (RESET). Die App speichert ab dem markierten Bild
  24 Bilder so, wie der Bildschirm sie zeigt (`treffer-NN-KK.bmp`, aus dem
  XFB in Spielgröße zurückgelesen), und ab dem nächsten Bildstück eine Folge
  von 4 Bildstücken als GX-Daten. Einstellungen `RE4_GX_SHOTS_ON_HIT`
  (Kern: wie viele Treffer markieren) und `RE4_GX_DUMP_ON_HIT`; Testskript
  `-HitShots` (Standard 3) und `-HitDumps` (Standard 1). Prüfung im
  Laufzeittest „gx“: markiertes Bildstück, danach eines mit RESET, dann ein
  normales. Auf dem Gerät nicht getestet.
- `gx_chunk_render` zeichnet mit `RE4_GX_SCALE=N` in der Auflösung der App
  und liest das XFB in voller Größe zurück.

### Vierzehnter Gerätelauf (16.10.2026, Retroid, Stand `623afd6`): Balken gefunden

Drei markierte Treffer (zwei Würgegriffe, ein Axthieb), je 24 Bilder vom
Bildschirm (`treffer-NN-KK`), GX-Daten zum ersten Treffer und eine R3-Folge.
Laut Projektinhaber zeigen `treffer-03-00` bis `-03` den Balken und rechts
einen dunkleren Bereich: Nach dem Axthieb liegt ein Nachbild über dem Bild,
ab Zeile 345 (von 448) ist das Bild deutlich dunkler.

- **Ursache:** Mehrere Effekte des Spiels kopieren die Bildzeilen 56–447 halb
  (`GXCopyTex`, 256×196) und lesen davon als Textur 256×168, also genau die
  sichtbaren Zeilen 56–391 (auf dem GameCube liegen die Texel der Kopie
  zeilenweise im Speicher, die kleinere Textur liest die ersten 168 Zeilen):
  das Nachbild bei Treffern, das Hitzeflimmern über den Feuern in r101, die
  Überblendung im Titelbild (Ashleys Gesicht) und bei den Totenköpfen. Der
  Renderer rechnete Texelkoordinaten auf die Texturgröße (168) um, die
  GL-Kopie hat aber 196 Zeilen: Das Bild war um 196/168 gestreckt, und ab
  Zeile 56 + 336·168/196 = 344 kamen die schwarzen Zeilen unter dem Bild
  dazu (gemessen auf dem Gerät: 345).
- **Behoben (`gx_gl.cpp` `BindTexture`):** Hat eine Textur dieselbe Breite
  wie die Kopie an ihrer Adresse, gehen ihre Texelkoordinaten nach der Höhe
  der Kopie. Nachgezeichnet: Titelbild ohne dunklen Streifen, Gesicht an der
  richtigen Stelle; Hitzeflimmern unten heller. Vergleich aller 51
  bisherigen Geräteaufnahmen vorher/nachher: Änderungen nur in Bildern mit
  diesem Kopiermuster. Auf dem Gerät nicht getestet. Vermutlich ist das auch
  das „dunkle Glas“ im Titel (G14).
- Die Treffer-Aufnahme ist im Testskript wieder aus (`-HitShots`,
  `-HitDumps`, Standard 0), weil sie das Spiel kurz ruckeln lässt.

### Abschluss M6/M6b (16.10.2026)

Erreicht und auf dem Retroid geprüft: die App „RE4 Kern“ (armeabi-v7a) startet
den Originalkern, zeigt über die GX-Nachbildung Logos, Titel, das Intro im
Auto, r100 und r101 mit Figuren, Effekten, Anzeigen, Untertiteln,
Untermenüs (Inventar, Dateien) und der Karte; Raumauswahl-Menü, Soft-Reset
nach dem Tod, Leistung bei etwa 1080p (Renderer 4–6 ms, GPU 8–16 ms je Bild,
Kern unter 33 ms nach dem schnelleren Decoder).

Offen, mitgenommen in die nächsten Meilensteine:

- Dunkler halbtransparenter Balken unten bei Treffern: Ursache gefunden und
  behoben (vierzehnter Gerätelauf), Gerätetest steht aus.
- Flackern im Untermenü „FILES“ laut Projektinhaber; in den R3-Folgen und
  Bildschirmfotos nicht zu sehen (Beschreibung oder Video fehlt).
- Titel: START zweimal / Logo verblasst (G14), Leon kurz unsichtbar beim
  Szenenwechsel (G12): beim nächsten Gerätelauf prüfen, ob noch vorhanden.
- Baum-Kollision in r100 (G17, Leon läuft in einen dünnen Baumstamm):
  **gestrichen**, wird nach Entscheidung des Projektinhabers (17.10.2026)
  nicht weiter verfolgt.
- 60 Bilder/s und Grafikoptionen: Meilenstein 11.

### Werkzeuge zur Fehlersuche auf dem Gerät (Stand 16.10.2026)

- **Testlauf:** `port/scripts/test-core-app.ps1 -Manual -Seconds 900 -Scale 3`;
  Ergebnis zum Schicken: `senden.zip` (Bericht, Protokoll, Profile, Bilder als
  JPEG), dazu `gx-geraet.zip` bis 25 MB.
- **Im Spiel:** SELECT = Diagnose-Anzeige (erste Zeile seit M7: zuletzt
  gedrückte Taste mit ihrer GameCube-Taste, gehaltene Tasten, Rohwerte von
  Sticks und Triggern); R3 = 12 aufeinanderfolgende Bilder als GX-Daten; Taste P im
  PowerShell-Fenster = 15 s CPU-Profil (simpleperf, `profil-N.txt`,
  `port/tools/android_profile.py`); `-HitShots N`/`-HitDumps N` = Bilder vom
  Bildschirm und GX-Daten ab den ersten N Treffern auf Leon.
- **In der Cloud:** `gx_chunk_render` (Bau: `port/build/app-host-gl`, siehe
  `whole_r100_check.sh`) zeichnet GX-Daten mit dem App-Renderer, Folgen
  hintereinander, `RE4_GX_SCALE=N` in App-Auflösung, `RE4_GX_WORK=1` mit
  GL-Arbeit und GPU-Zeit; `port/tools/gx_chunk_inspect.py` listet EFB-Kopien
  und Kopie-Texturen (`copies`) oder Zeichenaufträge mit Punkten (`draws`).
- **Kern-Prüfungen:** Laufzeittest „gx“ mit Decoder-Fingerabdruck
  (`02524012c6419af2`) und Markierung; `RE4_GX_BENCH=N` misst den Decoder.

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

## M7 Steuerung

### Bestandsaufnahme (16.10.2026, Cloud)

Weg einer Taste bis ins Spiel, Stand M6b:

1. **App, Java** (`CoreActivity`): Android-Tasten und -Achsen → GameCube-Bits,
   Sticks ×100, Trigger 0..255; belegt waren A/B/X/Y nach Android-Code,
   L1 und L2 = L, R1 = Z, R2 = R, SELECT = Overlay, R3 = GX-Aufnahme. Ein
   Zustand für alle Geräte zusammen (ein Gerät ohne Sticks setzte die Sticks
   des anderen auf 0), Achsen ohne Bereichsnormierung, kein Loslassen beim
   Fokusverlust.
2. **JNI** `CoreBridge.pad` → `core_mode.cpp` → `re4_core_pad` (`Re4CorePad`:
   Tasten, Hauptstick, C-Stick, Trigger) → `pad.cpp re4_pad_live`.
3. **Plattform** `pad.cpp PADRead`: Kanal 0 aus Testskript oder App, Drücke
   kürzer als ein Bild bleiben erhalten, `analogA/B` voll bei A/B,
   Kanäle 1–3 „kein Controller“. `PADClamp` und `PADControlMotor` waren
   leer.
4. **Spiel** `src/game/pad.cpp PadRead` (einmal je Bild): `PADRead`, dann
   `PADClamp`; `Joy[0]` mit `on/trg/rel/rep/rep2` (Wiederholung 24/6 und
   18/3 Bilder); **jeder Triggerwert über 0 gilt als L bzw. R**;
   Stick-Richtungsbits ab 30 Einheiten (Winkelsektoren), C-Stick-Bits ab 10;
   über `Key_type_tbl[pSys->pad_type]` (immer Tabelle 0) in das 64-Bit-Wort
   `Key`; analoge Werte nach `Key.stickX…`, `Key.triggerLeft/Right`.
   Verwendung im Spiel: R = Waffe anlegen (`Key.on & 0x10`), L = Messer
   (`0x800`), A = Aktion/Schießen, B = Rennen/Abbrechen (Unten+B =
   Schnellwende, `keyConfigTypeA`), X = Ashley warten/folgen
   (`pl_leon.cpp`), Y = Koffer, Z = Karte (`sscrn.cpp`), START =
   Optionen/Szene überspringen. Analoge Trigger nur in Entwicklerwerkzeugen;
   der C-Stick steuert den Blick (`cam_qfps.cpp` teilt durch 59, den Bereich
   nach `PADClamp`).
5. **Rüttelmotor:** `VibControl` schaltet den Motor je Bild an/aus (schwache
   Stufen als Pulse), über die Spieloption „Vibration“ abschaltbar.

**Befund:** Ohne `PADClamp` kamen die Werte ungeklemmt an: C-Stick bis 100
statt 59 (Blick schwenkte zu weit), Hauptstick bis 100 statt 72, keine
Totzone, und schon ein Triggerrauschen von 1 galt als gedrücktes L/R.

### Entscheidung des Projektinhabers (16.10.2026)

- Gesichtstasten **nach Beschriftung**: Retroid-A = A, B = B, X = X, Y = Y
  (die Buchstaben der Spielhinweise stimmen; B liegt wie beim GameCube links
  unterhalb von A). Schalter im Startmenü der App „Swap A/B and X/Y“ für
  Controller, die die Codes der anderen Anordnung schicken.
- **R2 = R** (Waffe anlegen, analog), **L2 = L** (Messer, analog),
  **R1 = Z** (Karte), **L1 frei**.
- **R3 allein** bleibt die Grafik-Aufnahme (keine Tastenkombination), Taste P
  im Testskript unverändert; SELECT = Diagnose-Anzeige.
- **Vibration** schon in M7.

### Umsetzung (16.10.2026, Cloud)

- **Kern:** `src/lib/Padclamp.c` wird unverändert mitkompiliert (SDK-Einheit
  in `CMakeLists.txt`), das No-op in `pad.cpp` ist entfernt. Totzonen und
  Bereiche damit wie im Original: Sticks 15 je Achse, Hauptstick höchstens 72
  (Achteck, diagonal 40/40), C-Stick 59, Trigger ab 30 (≈12 % Weg), höchstens
  150. `PADControlMotor` merkt den Motor; `PADRead` schreibt je Bild ein Bit
  (`re4_core_rumble`: Bits 0–3 die letzten vier Bilder, Bits 8–31 Bildzahl),
  Protokoll „pad: rumble from frame N“ (höchstens 16). API-Version 9.
- **App:** `CoreInput.java` (ohne Android, Hosttest
  `tests/CoreInputTests.java`, 69 Prüfungen mit einer Java-Kopie von
  `PADClamp`): Belegung wie oben, Zustand je Eingabegerät (Tasten aller
  Geräte zählen, je Stick und Trigger das Gerät mit dem größten Ausschlag),
  Rohwerte wie ein echter Controller (Hauptstick ±101, C-Stick ±92 am
  Anschlag: Mittel echter GameCube-Controller, 0,794 bzw. 0,722 von 127;
  Trigger 0..255, L2/R2 als Taste = voll; digitales L/R erst ab 85 % wie der
  Klick des GameCube-Triggers). `CoreActivity`: Achsen nach ihrem Bereich
  normiert (rechter Stick Z/RZ, sonst RX/RY; Trigger LTRIGGER/BRAKE,
  RTRIGGER/GAS; Steuerkreuz auch als HAT), alles loslassen bei Fokusverlust,
  Pause und abgezogenem Gerät; Vibration alle 16 ms aus den letzten vier
  Spielbildern (Stärke 1–4 von 4 Bildern, mit Amplitudensteuerung 116–255),
  aus, sobald das Spiel steht; Berechtigung `VIBRATE`. Overlay (SELECT):
  zuletzt gedrückte Taste mit GameCube-Taste, Tasten, Rohwerte der Sticks und
  Trigger. Protokoll `RE4Input`: Belegung, jedes Eingabegerät (Name,
  Hersteller-/Produktnummer, Quellen, Achsen mit Bereich und `flat`), jede
  Taste (Android-Code, Scancode → GameCube-Taste), Vibrator.
- **Testskript:** Abschnitt im Bericht mit Belegung, Eingabegeräten, allen
  gedrückten Tasten (Android → GC) und der Vibration.

### Prüfungen (Cloud, 16.10.2026)

| Prüfung | Ergebnis |
|---|---|
| `port/tools/whole_build.sh` (i686, armhf unter qemu), normal und `RE4_OPT=2` | grün; Kern-Test „pad“ (Totzonen, Achteck, C-Stick, Trigger-Schwelle, Motor je Bild) bestanden; 76 Platzhalter wie vorher |
| `CoreInputTests` + alle Java-Hosttests von `tools/test_host.sh` | bestanden |
| `re4decomp_link_check` (native App-Bibliothek, `--no-undefined`) | linkt; 10 JNI-Namen passen zu `CoreBridge` |
| `CoreActivity` mit `javac` gegen nachgebildete Android-Klassen (kein SDK erreichbar) | übersetzt |

Nicht geprüft: echtes Android-SDK/Gradle (nur auf dem PC), PowerShell-Skript
(kein PowerShell in der Cloud), alles auf dem Gerät.

### Gerätetest (16.10.2026, Retroid, Stand `d1794f1`): M7 erreicht

`test-core-app.ps1 -Manual -Seconds 900 -Scale 3`, 200 s gespielt (Titel,
Intro, r100, r101, Würgegriff, Tod). Laut Projektinhaber alle Tasten erkannt,
Vibration funktioniert.

- **Eingabegerät:** „Retroid Pocket Controller“ (Hersteller 0x2022, Produkt
  0x3001, Quellen Gamepad/Joystick/Tastatur), ein einziges Gerät. Achsen:
  X/Y linker Stick, Z/RZ rechter Stick, BRAKE/GAS die analogen L2/R2
  (0..1), HAT_X/HAT_Y das Steuerkreuz (kommt nur als Achse, nicht als
  Taste); L2/R2 schicken zusätzlich Tastencodes. `flat` überall 0 (Totzone
  macht allein `PADClamp`).
- **Tasten:** Die Codes passen zur Beschriftung (BUTTON_A = A usw.), der
  Tausch-Schalter wird nicht gebraucht. Im Bericht: SELECT → Overlay, A, B,
  X, Y, START, R2 → R, L2 → L, R1 → Z, L1 und L3 ohne Funktion.
- **Im Kern angekommen** (174 Tastenwechsel): alle 13 GameCube-Bits, auch in
  Kombination, z. B. L (Messer) und L+A (Stich), R (Zielen, Laser im Bild)
  und R+A (Schuss, Munition 17 → 9), B, Steuerkreuz in alle vier
  Richtungen, Y (Koffer), Z (Karte), X, START. Untermenüs viermal geöffnet
  und geschlossen; „Shake off“ im Würgegriff.
- **Vibration:** Vibrator mit Amplitudensteuerung; das Spiel rüttelte 5×
  (Treffer, Tod).
- **Leistung unverändert:** Kern 33,4 ms je Bild, Renderer 5,7–7,3 ms,
  gezeigt 58–60 Bilder/s.
- Nicht eigens geprüft: wie weit L2/R2 gedrückt werden müssen (das
  Protokoll zeigt nur die digitalen Bits), Gefühl der Totzone (keine
  Beanstandung), R3 in diesem Lauf (unverändert), der Tausch-Schalter.
- „Räume aufgebaut: keiner“ im Bericht betrifft nur die frühere Brücken-Ansicht
  (`-Bridge`), nicht die GX-Ansicht.

## M8 Audio

### Bestandsaufnahme (16.10.2026, Cloud)

- **Vorher:** `runtime/ax.cpp` (K2) bildete die AX-Bibliothek selbst nach
  (Stimmen-Vergabe, Setter, Abspielposition je Audio-Rahmen) und erzeugte
  keine Töne; MIX, SYN, SEQ, AXART, AXFX, AI und ADX waren Platzhalter
  (Gerätelauf M7: 30 Platzhalter aufgerufen, u. a. `AXARTInit`, `MIXInit`,
  `SYNInit`, `SEQRunAudioFrame`, `AXFXReverbHiInit`, `AIInit`).
- **Tontreiber des Spiels** (`snd_*.cpp`, Capcom): `AIInit`, `AXInitEx(1)`
  (Dreifachpuffer), `MIXInit`, `AXARTInit`, `SYNInit`, `SEQInit`,
  `AXSetCompressor(1)`, `AXSetMode` 0 (Stereo) oder 2 (DPL2) je nach Option;
  Stimmen über AX und MIX (Lautstärke, Pan, Surround-Pan, Aux A/B), Musik über
  SYN/SEQ (MIDI mit Wavetables im ARAM), Streams (Sprache, Funk, Musik) als
  ADPCM-Stimmen über einen ARAM-Ring, den `snd_str*.cpp` per DVD/ARQ nachlädt;
  Effekte auf den Aux-Bussen (`snd_efx.cpp`: ReverbHi, ReverbStd, Chorus,
  Delay, ReverbHiDpl2). Scheitert ein `AXFX…Init`, hängt das Spiel keinen
  Effekt ein (trockener Ton).
- **ADX** (CRI) nutzt das Spiel nur für Filme (`sofdec.cpp`, ADXT mono) und
  die DVD-Hilfe `ADXM_ExecMain`: gehört zu M9.
- **SDK-Quellen** in `src/lib`: AX, MIX, SYN, SEQ, AXART reines C; die
  Effekte haben Gekko-Assembler (`reverb_hi.c` DoCrossTalk/HandleReverb,
  `reverb_std.c` HandleReverb, `reverb_hi_4ch.c` HandleReverbDpl2,
  `chorus.c` do_src1/do_src2). Der DSP-Microcode (`DSPCode.c`) ist
  DSP-Maschinencode.
- **App:** Die bisherige App (classic) spielt eigene Mischungen über
  `GameAudio.java` (AudioTrack 48 kHz); im Kernmodus gab es keine
  Tonausgabe.

### Umsetzung (16.10.2026, Cloud)

- **SDK unverändert mitkompiliert:** AX, AXAlloc, AXAux, AXCL, AXComp,
  AXOut, AXProf, AXSPB, AXVPB, DSPCode, mix, syn*, seq, axart*, axfx, delay,
  chorus, reverb_hi, reverb_hi_4ch, reverb_std (`CMakeLists.txt`,
  `RE4_SDK_UNITS`). `runtime/ax.cpp` entfällt. Platzhalter 76 → 27 (Ton nur
  noch ADX, M9).
- **`runtime/dsp.cpp` – der DSP mit dem AX-Microcode:** `DSPAddTask` bootet
  sofort (init-Rückruf), Post 0xBABE0180 + Adresse der Befehlsliste
  (`AXCL.c __AXNextFrame`) führt die Liste aus, danach der resume-Rückruf wie
  beim DSP-Interrupt. Befehle: 0x00 Studio (Startwerte der neun Mischpuffer
  mit den Entknack-Rampen aus `AXSPB.c`; das SDK schreibt jeden Startwert als
  ein 32-bit-Wort über Hi/Lo, so wird er gelesen), 0x07/0x11 Surround des
  letzten Blocks in links/rechts (gleich-/gegenphasig), 0x02/0x03 alle
  Stimmen der PB-Liste je Millisekunde (PB-Updates, Lesen aus dem ARAM:
  DSP-ADPCM mit Rahmenkopf, PCM16 big-endian, PCM8; Ende: Schleife mit dem
  ADPCM-Schleifenkontext, Streams (`type` 1) behalten ihre Historie, sonst
  Stopp am Endpunkt; Abtastraten-Umsetzung: keine / linear / statt der
  ROM-Polyphasenfilter eine 4-Punkt-Kubik; Hüllkurve `ve`, Tiefpass,
  Mischen in L/R/S, Aux A L/R/S, Aux B L/R/S mit Rampen nach `mixerCtrl`,
  Entknack-Werte `dpop`), 0x04/0x05 Aux-Bus zum CPU-Effekt und dessen
  Ergebnis von vor zwei Blöcken in die Hauptmischung, 0x13/0x10 DPL2 auf
  Stereo heruntergemischt, 0x12 Kompressor (nicht nachgebildet, wie bei
  Dolphin), 0x0E Ausgabe (Surround an die CPU, links/rechts 16 bit,
  rechts zuerst). Nicht nachgebildet: ITD (`AXSetVoiceItdOn`), Kompressor,
  die exakten Polyphasen-Koeffizienten (liegen im DSP-ROM, nicht auf der
  Disc). Eine endende Stimme verliert die letzten 1–2 Abtastwerte, die noch
  in der Umsetzung stecken.
- **`runtime/ai.cpp` – AI und Tonausgabe:** 200 Blöcke je Sekunde Spielzeit
  mit den VI-Halbbildern (vorher `re4_ax_field`, jetzt `re4_ai_field`), je
  Block der gerade gestartete in einen Ring (512 ms), dann der
  DMA-Rückruf (AXOut: der DSP mischt den nächsten, der Audio-Rahmen des
  Spiels). `AIReset` setzt wie im SDK nur das Init-Kennzeichen. Leser für die
  App (`re4_audio_read`): hält etwa 60 ms im Ring, passt die Rate um höchstens
  0,5 % an (Drift zwischen Spiel- und Geräteuhr, Schübe der
  Halbbild-Zustellung), springt nach einer Pause der App vor. Protokoll alle
  10 s „audio: … blocks, … ms waiting, … lost, … ran dry, … skips; DSP …
  lists, … voices“ („ran dry“ zählt Aussetzer, nicht das Warten auf den
  ersten Ton). `RE4_AUDIO_WAV=datei` schreibt den Ton zusätzlich als WAV
  (32 kHz stereo), `RE4_AUDIO_WAV_SECONDS=N` begrenzt auf N s.
- **Patch 0047 `axfx-asm-in-c`:** C-Fassungen der fünf Assembler-Funktionen
  (gleiche Schritte, gleiche Reihenfolge, einfache Genauigkeit; `fctiwz` =
  abschneiden, `fctiw` = runden) und das fehlende `return` in
  `AXFXDelayInit` (der Gekko gab in r3 das Ergebnis von `AXFXDelaySettings`
  zurück, das Spiel prüft es). Gefunden beim Übersetzen: Im Original speichert
  `HandleReverb` des hohen Halls (auch der DPL2-Fassung) den weitergerückten
  Zeiger der Vorverzögerung nicht in `preDelayPtr`, sondern in die
  Verzögerungsleitung selbst (Byte 0x1B8); die Vorverzögerung wirkt deshalb
  nur einen Abtastwert lang. Die C-Fassung tut dasselbe (ohne den
  Streu-Schreibzugriff), damit es klingt wie auf dem GameCube.
- **App:** `CoreAudio.java` (eigener Thread, AudioTrack 32 kHz stereo,
  256 Frames je Schreibvorgang; Lautstärketasten = Spiellautstärke),
  gestartet in `onResume`, gestoppt in `onPause`; `CoreBridge.audio` →
  `re4_core_audio` (API-Version 10); Overlay-Zeile „sound: …“; Protokoll
  `RE4Audio`.
- **Prüfskripte:** `whole_r100_check.sh` (PC mit Disc-Daten) nimmt die ersten
  240 s Spielzeit als `ton-<ziel>.wav` auf und zeigt Pegel je 10 s
  (`port/tools/audio_wav_report.py`); `test-core-app.ps1` meldet die
  Tonzähler und die Tonausgabe, `-AudioWav N` nimmt N s auf dem Gerät auf
  (`ton.wav`, bis 12 MB in `senden.zip`).

### Prüfungen (Cloud, 16.10.2026)

| Prüfung | Ergebnis |
|---|---|
| Kern-Test „ax“: SDK-Stimmen-Vergabe; PCM16 1:1 nach links voll, rechts halb, Stopp am Ende; DSP-ADPCM (drei Rahmen, eigene Köpfe) bitgenau gegen die dokumentierte Formel; Schleife; Rate 0,5 linear = 80 Abtastwerte je Block; Hüllkurve; Aux A über einen CPU-Effekt zurück in die Hauptmischung; Leser der App; AI-Takt 200 Blöcke je 60 Halbbilder | bestanden |
| Kern-Test „axfx“: hoher Hall (Übersprechen 8500/1500, trockener Anteil sofort, kein Hall vor dem kürzesten Kamm, abklingender Nachhall, Surround still), normaler Hall mit 10 ms Vorverzögerung, Chorus auf Sinus, Delay 50 ms = 9 Blöcke | bestanden |
| Laufzeittest „stream“ (Streamcode des Spiels über das echte AX und den DSP-Nachbau) | bestanden |
| `whole_build.sh` i686 und armhf (qemu), normal und `RE4_OPT=2` | grün, 27 Platzhalter |
| native App-Bibliothek (`re4decomp_link_check`), Java gegen nachgebildete Android-Klassen | linkt, 11 JNI-Namen passen; übersetzt |

Nicht geprüft (Stand dieses Abschnitts): Ton mit echten Disc-Daten, Klang,
Lautstärke, Latenz, Rechenzeit auf dem Retroid; siehe Gerätelauf unten.

### Erster Gerätelauf mit Ton (16.10.2026, Retroid, Stand `34b60a3`)

`test-core-app.ps1 -Manual -Seconds 900 -Scale 3 -AudioWav 60`, etwa 360 s
(Titel, Intro, r100, r101, Untermenüs, Treffer, Soft-Reset). Laut
Projektinhaber klingt alles sauber und plausibel; nur der Schuss von Leons
Waffe bricht vereinzelt ab.

- **Ausgabe:** 70 000 Blöcke (350 s Spielzeit), 0 verloren, 12 Aussetzer,
  40–76 ms im Ring; AudioTrack 32 kHz mit 2568 Frames Puffer (80 ms), also
  etwa 120–160 ms vom Mischen bis zum Lautsprecher (nicht beanstandet; kürzer
  machen: M11). WAV der ersten 60 s (Titel, Intro): Pegel bis −0,5 dBFS,
  nichts übersteuert. Kern weiter 33,4 ms je Bild.
- **Abbrüche:** 116 × „ISS Voice Drop“ (der Rückruf des Tontreibers, wenn AX
  eine Stimme wegnimmt), gehäuft im Kampf. Ursache: das Budget von AX für den
  DSP (`__AXSyncPBs`): AX schätzt die DSP-Takte jeder laufenden Stimme
  (Umsetzung, Busse, Rampen) und stoppt alles über `OS_BUS_CLOCK / 400`
  (405 000 Takte, was der DSP des GameCube in 5 ms schafft), innerhalb einer
  Priorität zuerst die älteren Stimmen, also z. B. den ausklingenden Schuss
  neben neuen Geräuschen. Auf dem GameCube ist das dieselbe Grenze; der
  nachgebaute DSP hat sie nicht.
- **Behoben:** `dsp.cpp` gibt AX beim Start des DSP-Programms das Vierfache
  (1 620 000 Takte, mehr als 64 Stimmen auf allen Bussen brauchen);
  `RE4_DSP_ORIGINAL_LIMIT=1` behält die Grenze des GameCube. Kern-Test „ax“:
  40 Stimmen auf allen Bussen mit Rampen bleiben, mit 405 000 Takten stoppt
  AX welche. Die Protokollzeile „audio: …“ nennt jetzt auch die meisten
  gleichzeitig laufenden Stimmen, die höchste Schätzung von AX und die Blöcke
  über dem Budget; der Bericht von `test-core-app.ps1` zählt „ISS Voice
  Drop“. Auf dem Gerät nicht getestet.

## M9 Videos

### Bestandsaufnahme (16.10.2026, Cloud)

- **Vorher:** `runtime/movie.cpp` (K2) beendete jeden Film sofort (kein Bild,
  kein Ton), das Spiel machte danach weiter.
- **Filme des Spiels** (alle über `cSofdec`, `sofdec.cpp`): auf dem Titel nach
  10 s ohne Taste abwechselnd `movie/demo0.sfd` und `demo1.sfd`
  (`title.cpp`), `e3_jpn.sfd` (Messe-Zweig des Titels), `opening.sfd` vor der
  Autofahrt (`r120.cpp R120Event`), `r214_ev.sfd` (r217), `r229_ev.sfd`
  (r221), `r333_ev.sfd` (st3), `ending.sfd` (r333), die Ada-Enden
  `adaend_m.sfd`/`adaend_c.sfd` (r40e); fehlt eine Datei, nimmt das Spiel
  `dmy.sfd`. Dazu das Debug-Menü „MOVIE TEST“ (`t_movie.rel`).
- **Ablauf im Spiel:** Der Film läuft in Scheduler-Platz 1 (Platz 0 hält an),
  der Heap des Raums wandert für die Dauer ins ARAM (5 MB Film-Heap). Je Bild
  `cSofdec::appMain`: START oder B (`Joy.trg & 0x1200`) bricht ab, sonst
  `ADXM_ExecMain` (der CRI-Server: Lesen, MPEG-Dekodieren, ADX), das neueste
  Bild als Y8- und C4:4-Textur (`mwPlyFxCnvFrmY84C44`), gezeichnet mit vier
  TEV-Stufen (YUV → RGB über Konstantfarben, `drawTex`). In r120 überspringt
  ein Abbruch laut Originalcode auch die Autofahrt (`SCF_R120_EVENT_CANCEL`).
- **CRI-Quellen in `src/lib`:** 138 Dateien (ADX, ADXF, SJ, SVM, Sofdec
  SFD/SFH/SFX, MWPLY, MPEG MPV/MPS), C bis auf Gekko-Assembler: IDCT
  (`dct_fsri.c`) und Halbpixel-Mischen (`mpv_umc.c`) mit Paired Singles und
  GQR-Quantisierung, die Zeitbasis (`uty_tmr.c`), GQR-Sichern
  (`uty_ppc.c`), die Textur-Umwandlung (`cftyp422_ppc.c`); dazu viele
  Registerbindungen des Decompilats ohne Wirkung und Bewegungsausgleich, der
  Bildpunkte als Wörter in GameCube-Reihenfolge packt (`mpv_mc*.c`).
- **Uhr:** Sofdec taktet das Bild nach der Spielzeit des ADX-Tons, geglättet
  mit der Zeitbasis (`sfd_adxt.c sfadxt_UpdateTime`). Läuft das Spiel
  schneller als die Zeitbasis, bleibt die Uhr am Ende des Tons stehen und der
  Film endet nie (im Test gesehen).

### Umsetzung (16.10.2026, Cloud)

- **CRI unverändert mitkompiliert:** alle 138 Dateien (`RE4_CRI_UNITS`,
  Ziel `re4_cri_obj`, `-fsigned-char` wie CRIs `-char signed`), gelinkt in
  `libre4_main` (im Original Teil von `main.dol`). `runtime/movie.cpp`
  entfällt.
- **Registerbindungen** (`asm { mr r3, r31 }` ohne Wirkung auf C-Variablen)
  entfernt eine neue Regel von `whole_prepare.py` (Tabelle „Mechanische
  Umschreibungen“).
- **`port/whole/include/re4_cri_port.h`:** Gekko-Befehle als C (Intrinsics,
  big-endian Lesen, Paired Singles `re4_ps`, GQR-Quantisierung
  `re4_psq_l`/`re4_psq_st` mit den GQR-Werten der CRI-Kerne in
  `re4_cri_gqr`).
- **`port/tools/whole_ppc_to_c.py`:** übersetzt Gekko-Assembler Befehl für
  Befehl nach C (Register als Variablen, Sprünge als `goto`); damit entstand
  Patch 0050.
- **Patches 0048–0055** (Tabelle „Patches“): Plattformteile, Bewegungsausgleich
  als Byte-Schleifen, Assembler in C, Byte-Reihenfolge der Stromleser und
  ADX-Köpfe, Bildgröße aus `mwPlyGetHdrInf`, Werte, die im Original in
  Registern lagen, Fortschritt im Log und Konstantfarben in `sofdec.cpp`,
  Blendfarben in `title.cpp`.
- **Werte in Registern** (Patch 0053): gefunden durch Vergleich jeder
  Deklaration mit ihrer Definition in LLVM-IR (jetzt als Werkzeug
  `port/tools/whole_ir_signatures.py`): die Zahl übersprungener Makroblöcke
  (fehlte: Absturz im Bewegungsausgleich), die Bildrate für den
  Sofdec-Zeitgeber, der Player für die Nachschub-Prüfung, die Größe für den
  ADX-Kopf, zwei Ergebnisse, die Aufrufer prüfen.
- **Plattform:** `DVDCancel` (`dvd.cpp`, Abbruch über den verzögerten
  Rückruf), `__cvt_fp2unsigned` (`sn_runtime.cpp`, wie `fctiwz`),
  `re4_cri_timer` (`vi.cpp`: die Zeitbasis der CRI-Uhr; normal `OSGetTime`,
  mit `RE4_VI_FAST=1` zählt sie die zugestellten Halbbilder, damit Ton und Uhr
  im Schnelllauf zusammenpassen).
- **Farben:** `setTevPrm` schreibt die vier Konstantfarben als Wort über
  ein `GXColor` (Patch 0054, sonst grünstichig), der Titel ebenso seine
  Blendfarben (Patch 0055, sonst ist Schwarz ein durchsichtiges Rot).
- **Helligkeit** (Hinweis des Projektinhabers: der Regler wirkte nicht): Das
  Spiel stellt sie über den Kopierfilter ein (`Bg_brightness_set` →
  `GXSetCopyFilter`, Summe der sieben senkrechten Gewichte, 64 = normal). Der
  GX-Renderer der App (`gx_gl.cpp`) multipliziert das kopierte Bild jetzt mit
  Summe/64; das Weichzeichnen des Filters bleibt weg. Test `gx_gl_check`
  (`TestCopyFilter`).
- **Texturen:** Jeder Film lädt je Bild zwei neue Texturen hoch; der
  Texturspeicher von `gx_decode.cpp` begrenzt jetzt auch die entpackten Bytes
  (256 MB), damit sich Filmbilder nicht ansammeln.
- **Log:** `runtime/movie_status.cpp` (aus `sofdec.cpp`, Patch 0054): „movie:
  NAME started“, alle 5 s „… playing, Schritte, Bilder, Zeit, übersprungen
  Decode/Anzeige, Status“, am Ende „ended“ oder „skipped (START)“.
- **Prüfwerkzeuge:** `port/tools/make_test_sfd.py` (Sofdec-Muxer für
  Testfilme), `test_whole_movie.py` (Cloud-Test, in `whole_build.sh`),
  `movie_report.py` (Standbilder als PNG), `whole_r100_check.sh` Abschnitt 6
  (jeder Film der Disc ganz im Schnelllauf, Standbild alle 30 s und das letzte Bild in
  `bilder-film/`, Ton des Vorfilms als `film-d1-opening.wav`),
  `test-core-app.ps1` (Bericht: Filme gestartet/beendet, je Film Zeit, Bilder,
  ausgelassene Bilder, Abbruch mit START, fehlende Dateien, CRI-Fehler).

### Prüfungen (Cloud, 16.10.2026)

| Prüfung | Ergebnis |
|---|---|
| Kern-Test „ax“ mit SYN-Stimmen (Patch 0056), Laufzeittest „vsync“, Filmtest mit 60,0-Hz-Halbbildern (alle 120 Bilder genau zwei Halbbilder) | bestanden (i686; armhf: unter qemu zu langsam für Echtzeit, dort zählt nur das Ende) |
| Kern-Test „movie“: drei synthetische Packs, `mwPlyGetHdrInf` 320 × 240, Arbeitsspeicher ≥ 320 · 240 · 3 | bestanden |
| Filmtest (4 s, 320 × 240, 29,97 Bilder/s, MPEG-1 mit B-Bildern, ADX stereo 48 kHz) in Echtzeit | 120 von 120 Bildern in Reihenfolge, schlechtestes Bild 75,9 dB gegen ffmpeg, 2,03 Halbbilder je Bild, Ende (PLAYEND) nach 249 Halbbildern, 0 ausgelassen, 0 CRI-Fehler; Ton: beide Töne auf ihrem Kanal (0,97/0,93, Übersprechen 0,000), Ton 40 ms nach dem ersten Bild |
| Filmbild über die GX-Nachbildung (`drawTex` des Spiels durch `gx_chunk_render`) | 24,0 dB gegen die BT.601-Umrechnung, mittlere Abweichung R +0,3 G +0,5 B +0,8 (Kanten weichen ab: die Farbanteile filtert die Textureinheit bilinear) |
| derselbe Film im Schnelllauf (`RE4_VI_FAST`) | 120 Bilder, 2,03 Halbbilder je Bild, Ende nach 249 Halbbildern, Ton +40 ms |
| `whole_build.sh` i686 und armhf (qemu), normal und `RE4_OPT=2` | grün, 45 Platzhalter |
| `gx_gl_check` (mit `TestCopyFilter`), `re4decomp_link_check` | 0 Fehler; linkt |
| `whole_ir_signatures.py` (alle 1335 Dateien des i686-Builds) | 10 Stellen „liest Unsinn“, alle geprüft und ohne Wirkung: tote Funktionen (Z-Puffer-Umwandlungen prüfen das Ergebnis von `mwPlyGetCurFrm`), die nie aufgerufene Tabelle `mpvcmc_oneref`, die Sprungfunktionen `MPVUMC_*picSkipped` (Aufruf über den in Patch 0053 korrigierten Zeiger), Ergebnisse von `ADXT_SetOutVol/Pan`, die `mwsfdrna.c` verwirft; 26 harmlose (zusätzliche Argumente, ungelesene Ergebnisse) |

**Nicht geprüft (Stand dieses Abschnitts):** die echten Filme der Disc
(PC-Prüfung), alles auf dem Gerät (Rechenzeit des Dekodierens, Speicher,
START, Lippensynchronität, Demo-Filme des Titels). Der ADX-Pegel liegt im
Test etwas über der Quelle (Spitze 4836 bei etwa 4100 im Testton; Rundung des
ADX-Kodierers oder Mischung, nicht untersucht). Nicht übernommen, weil das
Spiel sie nicht nutzt: die ARGB8888-Umwandler (melden sich einmal im Log),
die WAV/AIFF/AU-Wege von ADX (ohne Byte-Reihenfolge-Korrektur). Aufrufe über
Funktionszeiger mit falscher Argumentzahl findet `whole_ir_signatures.py`
nicht.

### Erster Gerätelauf und PC-Prüfung mit Filmen (16.10.2026, Stand `af34ae0`)

- **Erster Versuch:** Der Android-Build brach nach fehlerfreiem Bauen bei der
  Platzhalter-Prüfung ab (alter Vergleichs-Ordner, siehe „Platzhalter“);
  behoben mit `port/whole/stubs-expected.txt`.
- **Gerätetest A** (`-Seconds 480 -Scale 3 -AudioWav 60`, r120): Vorfilm
  `opening.sfd` ganz gelaufen (76,0 s, 2278 Bilder, 4567 Schritte, 0
  ausgelassen, Status 3), danach Autofahrt und r100. Während des Films Kern
  16,7 ms je Bildstück (10 ms frei), Renderer 2–3 ms (höchstens 13), 60,0
  gezeigt je Sekunde; Ton des Films in `ton.wav` sauber (−18 dBFS, 3
  übersteuerte Werte). Laut Projektinhaber vereinzelte kleine Ruckler, auch
  außerhalb der Bildschirmfotos. **6 × „ISS Voice Drop“** in r100.
- **Gerätetest B** (`-Manual`): Demo-Film nach etwa 15 s auf dem Titel mit
  Ton (`demo0.sfd` ganz, 61,4 s, 1839 Bilder, 0 ausgelassen), der zweite
  (`demo1.sfd`) mit START abgebrochen („skipped (START)“), 0 Voice Drops.
  Helligkeit nicht geprüft.
- **PC-Prüfung** (`test-r100-core.ps1`, „ALLES OK“): alle 13 Filme beider
  Discs ganz im Schnelllauf (Disc 1: `adaend_c`, `adaend_m`, `demo0`,
  `demo1`, `opening`, `r214_ev`, `r229_ev`; Disc 2: `adaend_c`, `adaend_m`,
  `demo0`, `demo1`, `ending` mit 6475 Bildern, `r333_ev`), jeder mit Status 3,
  0 ausgelassen, 0 CRI-Fehler, 512 × 336; ein Schritt 0,5–0,8 ms im Mittel
  (i686). Standbilder richtig (Farben, Untertitel, Abspann). `e3_jpn.sfd` und
  `dmy.sfd` gibt es auf der Disc nicht; `demo0` und `demo1` sind gleich.

**Abgebrochene Töne: Ursache gefunden (Patch 0056).** Die SYN-Bibliothek
(Synthesizer für die Sequenz-Klänge des Spiels, `snd_seq*.cpp`) schrieb
32-bit-Werte als ein Wort über zwei u16-Felder des AX-Parameterblocks
(`synsample.c`: Schleifen-Kennung und Format, Abspiel-, Schleifen- und
Endadresse, Verstärkung; `synpitch.c __SYNUpdateSrc`: das
Abtastraten-Verhältnis). Auf little-endian waren die Hälften vertauscht: die
Adressen lagen weit außerhalb des ARAM (SYN-Stimmen blieben stumm), und der
Bruchteil des Verhältnisses wurde zum ganzzahligen Teil. AX liest damit
`__AXSrcCycles[ratioHi]` weit hinter der 5-Einträge-Tabelle (auf dem GameCube
hätte ein Index ≥ 5 die folgenden Mischtabellen getroffen) und schätzte über
3 Milliarden DSP-Takte („AX estimate at most 3678218915“ im Protokoll),
also stoppte AX Stimmen. Das erklärt auch die abgebrochenen Schüsse aus M8
(das vierfache Budget hatte die Folgen nur verringert). Prüfung: Kern-Test
„ax“ setzt SYN-Stimmen auf (ADPCM mit Schleife, PCM16, PCM8) und ändert die
Tonhöhe; ohne Patch 11 Fehler, mit Patch bestanden. Gefunden hat das nicht
der IR-Vergleich (die Typen stimmen), sondern die Schätzung im Protokoll.
Für M10: `CARDFormat.c` und `CARDUnlock.c` schreiben ebenso Wörter in
Byte-Felder.

**Ruckler: Bildtakt an den Bildschirm gekoppelt.** Die Protokolle zeigen
keinen Engpass (Kern und Renderer mit Reserve, 0 ausgelassene Filmbilder,
gleichmäßige Abstände). Der Kern lieferte aber 59,94 Bildstücke je Sekunde
(NTSC), der Bildschirm zeigt 60: Alle 16,7 s fehlte dem Renderer ein neues
Bild, ein Filmbild stand drei statt zwei Bildwechsel lang (im 76-s-Vorfilm
etwa viermal, im Spiel ebenso). Jetzt meldet die App jeden Vsync
(`Choreographer`, `CoreBridge.vsync` → `re4_core_vsync`), und `vi.cpp` legt
die Halbbilder auf den Bildschirmtakt, wenn er höchstens 0,5 % von 59,94 Hz
abweicht (60 Hz; 120 Hz mit zwei Vsyncs je Halbbild): eine Retrace je Vsync,
1 ms danach, die Lage wird sanft nachgeführt (höchstens 1/50 Halbbild je
Retrace). Das Spiel läuft dann 0,1 % schneller; die Tonausgabe der App gleicht
das aus, und die Filmuhr folgt dem Ton, also den Halbbildern (Filmtest mit
60,0-Hz-Halbbildern: jedes Bild genau zwei Halbbilder). Ohne Vsync für
250 ms (App pausiert, Linux) oder mit `RE4_VI_VSYNC=0` wieder der eigene
NTSC-Takt. Laufzeittest „vsync“: Rechnung (60/59,94/120 Hz angenommen, 50/90
Hz abgelehnt, Lage wandert zu 1 ms) und echter Takt (Vsync-Gitter 8 ms
neben den Retraces: die Retraces wandern auf 1 ms nach dem Vsync; mit
`RE4_VI_VSYNC=0` schlägt der Test fehl). Auf dem Gerät nicht getestet.

**FILES-Flackern** (Video des Projektinhabers, 37 Bilder bei 30 Bildern/s):
Ausgewertet zeigt sich ein Wechsel von Bild zu Bild genau im Rahmen der
Dateiliste (Holzrand oben, links, rechts, unten und die Leiste unter
„Village Files“): dieselben Bildpunkte springen jedes zweite Videobild um
etwa 15–25 Helligkeitsstufen hin und her (z. B. 85 ↔ 103). Leons Animation
und das pulsierende „Village Files“ sind gewollt. In den R3-Folgen aus dem
zehnten Gerätelauf (in der Cloud nachgezeichnet) war nichts davon zu sehen,
die Ursache liegt also vermutlich im Zeichnen auf dem Gerät (Treiber). Neu:
Eine R3-Folge speichert jetzt zusätzlich jedes Bild so, wie die GPU des
Geräts es gezeichnet hat (`gx-geraet-NN-KK.bmp` in `gx-geraet.zip`, auch über
`pull-gx-dumps.ps1`); der Vergleich mit dem Cloud-Bild derselben Daten zeigt
dann, welche Zeichnung abweicht.

### Zweiter Gerätelauf mit Filmen (16.10.2026, Stand `6c2e7d9`): M9 erreicht

- **Test A** (r120, `-Seconds 480 -Scale 3`): Vorfilm ganz (76,0 s, 2278
  Bilder, 0 ausgelassen), laut Projektinhaber **ohne Ruckler**. Je 5 s genau
  300 Schritte (der Takt folgt dem 60-Hz-Bildschirm), Ton 2000 Blöcke je
  9,99 s statt 10,0 s (0,1 % schneller, wie erwartet), „ran dry“ 19 statt 49.
  **Abgebrochene Töne: 0**, höchste AX-Schätzung 108 746 von 1 620 000 Takten
  (vorher 3,7 Milliarden). In r100 Kern 33,3 ms je Bild, 0 ms Warten auf den
  Renderer, 60 gezeigt je Sekunde; die R3-Folge im FILES-Menü kostete kurz
  Zeit (Renderer höchstens 843 ms beim Speichern der Bilder).
- **Test B** (`-Manual`): Demo-Film nach etwa 15 s mit Ton, Abbruch mit START,
  **Helligkeitsregler wirkt** (heller und dunkler). Gemeldet: Vor dem
  Demo-Film blendet der Titel aus, erscheint für einen Bruchteil einer Sekunde
  noch einmal, dann kommt der Film.
- **Titel blitzt vor dem Film auf → `VISetBlack`:** `title.cpp` schaltet vor
  dem Film mit `systemVISetBlack(1)` den Videoausgang schwarz und nimmt die
  Blende weg (`FadeKill`); der Film hebt das Schwarz mit seinem ersten Bild auf
  (`cSofdec::draw`). Im Port tat `VISetBlack` nichts, also war das eine Bild
  des Titels ohne Blende zu sehen. Jetzt gilt es ab dem nächsten `VIFlush`
  wie auf dem GameCube (`vi.cpp`); die Bildstücke tragen
  `RE4_GX_CHUNK_BLACK`, und der Renderer zeigt dann Schwarz (`gx_gl.cpp
  Present`), das Bild selbst wird weiter gezeichnet. Betrifft auch Raumwechsel
  und Filmende. Tests: `gx_probe` (Kennung gesetzt und wieder weg),
  `gx_gl_check` (schwarz gezeigt, danach wieder das Bild; ohne die Änderung
  schlägt der Test fehl). Auf dem Gerät noch nicht geprüft.
- **FILES-Flackern ist Filmkorn des Originals:** Die R3-Folge (12 Bilder,
  gx-geraet-00-00 bis -11) zeigt das Flackern auf dem Gerät **und** in der
  Cloud gleich (Bildpunkte im Rahmen der Dateiliste wechseln mit Periode 2,
  z. B. 93 ↔ 86; Gerät gegen Cloud im Mittel 0,2 Stufen Abweichung). Die
  Bildstücke unterscheiden sich in Zeichnung 7 und 15: Textur 0 ist abwechselnd
  0x721c60 und 0x719060, zwei I4-Texturen 320 × 224 mit Zufallsrauschen (zwei
  Bilder einer Texturanimation hintereinander in einer Datei). Die
  Texturanimation des ID-Systems (`id_sys.cpp idSysMove04`) schaltet jedes Bild
  eine Textur weiter, hier zwischen den beiden. Das ist der Körnungseffekt des
  Menüs, auf dem GameCube genauso; auf dem Röhrenfernseher (480i, unscharf)
  fiel er weniger auf. Die frühere Aussage „in den Daten nicht zu sehen“
  (zehnter Gerätelauf) war falsch. Kein Fehler des Ports; eine Option zum
  Beruhigen (die beiden Rauschbilder mischen) passt zu M11
  (Grafikoptionen).

## M10 Speichern/Laden

### Bestandsaufnahme (16.10.2026, Cloud)

- **Vorher:** `runtime/card.cpp` meldete beide Schächte leer, Patch 0039
  übersprang die Prüfung beim Start; Speichern ging nicht.
- **Dateien des Spiels** (`card.cpp`): Spielstände `bh4_data00` bis
  `bh4_data19` (je 8 Blöcke = 0x10000 Bytes, benutzt 0xEAFC) und die
  Systemdatei `bh4_system` (1 Block, benutzt 0x1E7C). Aufbau beider: Kommentar
  bei 0x0/0x20 („resident evil 4“, „FILE NN“ bzw. „Systemfile“), Banner 96 × 32
  RGB5A3 bei 0x40, ein C8-Symbol bei 0x1840 mit Palette bei 0x1C40 (aus der
  TPL des Untermenü-Archivs; die Bildpunkte bleiben big-endian wie auf der
  Disc, die Formatumwandler drehen nur die Köpfe). Spielstand: bei 0x2000 der
  Kopf `SaveInfo` (CRC über die folgenden 0x1FC Bytes, Kennung 0x116,
  Kalenderzeit, Art, Raum, Kapitel), ab 0x2034 der Spielzustand (aus `pG`
  0x36F8 Bytes), Gegenstände 0x572C, Räume 0x6930, Untermenü 0xE7D0, Händler
  0xE7D4, CRC-32 der Datei bei 0xEAF8. Systemdatei: `SYSTEM_SAVE_WORK` bei
  0x1E40 (Sprache, Land, Helligkeit, Steuerungsbelegung, Tonart, Söldner-
  Wertungen), CRC bei 0x1E78. CRC: MSB zuerst, Polynom 0x04C11DB7, Start 0
  (`CRCCalc`).
- **Byte-Reihenfolge:** Das Spiel kopiert seinen Speicher, wie er ist (im Port
  Host-Reihenfolge), und rechnet die CRC über die Bytes. Innerhalb des Ports
  passt das immer zusammen; ein GameCube-Spielstand sähe anders aus.
- **Wann:** Speichern an der Schreibmaschine (AT-Typ 8, `sce_at.cpp` →
  `CardSave`), am Kapitelende (`sce_com.cpp`), Söldner (`CardSysSave`).
  **Die Systemdatei (Optionen) schreibt das Spiel nur beim Start (wenn sie
  fehlt) und nach jedem Spielstand; gelesen wird sie nur beim Start.** Wer nur
  die Optionen ändert und neu startet, verliert sie also, wie auf dem
  GameCube. Laden: Titel („Load“), Pausemenü (`option.cpp`), Debug-Startmenü
  Zeile 1.
- **SDK:** 15 Dateien `src/lib/CARD*.c`; das Spiel nutzt nur Schacht A
  (Arbeitsbereich 0xA000) und verlangt 8-KB-Blöcke. `CARDFormat.c` und
  `CARDUnlock.c` schreiben 32-bit-Wörter in Byte-Felder (Hinweis aus M9): beim
  Formatieren landen Zähler, Sprache und Zeit in Host-Reihenfolge im
  Kartenkopf, und `CARDCheck.c` liest sie genauso zurück; `CARDUnlock.c` läuft
  nicht (siehe unten).

### Entscheidung des Projektinhabers (16.10.2026)

Kartenformat **nur für den Port**: Die Kartendatei hat die Byte-Reihenfolge
des Geräts und ist nicht mit GameCube/Dolphin austauschbar. Ein Umwandler für
`.gci` (Import/Export) kann später als eigener Schritt kommen.

### Umsetzung (16.10.2026, Cloud)

- **CARD-Bibliothek unverändert mitkompiliert** (`RE4_CARD_UNITS`, Ziel
  `re4_card_obj`, gelinkt in `libre4_main`) mit `re4_card_port.h` (die
  OS-Bytes 0x800030E3/0x800030C0 in MEM1, ein leerer VI-Registerblock für
  `CARDFormat`) und Patch 0057 (Befehlsbytes, siehe „Patches“).
- **Hardware darunter:** `runtime/exi.cpp` (EXI wie `EXIBios.c`),
  `runtime/memcard.cpp` (die Karte mit den Befehlen einer Memory Card 251, so
  wie Dolphin sie nachbildet), `runtime/alarm.cpp` (`OSAlarm` für die
  Zeitüberwachung der Befehle). Übertragungen geschehen sofort; was die
  Hardware per Interrupt meldet (DMA-Ende, „Karte fertig“), kommt an den
  Interrupt-Punkten (Leerlauf in `vi.cpp`, `OSSleepThread`) im
  Interrupt-Kontext, so lange, bis nichts mehr ansteht (ein Block lesen sind 16
  Übertragungen, schreiben 64 Seiten). Die Sperre gegen verschachtelte
  Zustellung gilt nur während des Rückrufs: am Ende des Interrupts darf auf
  eine andere Aufgabe umgeschaltet werden, die den Rest zustellt (sonst Hänger,
  wenn die schlafende Aufgabe gerade zustellte).
- **Die Karte:** Kennung 0x10 (16 Mbit, 8-KB-Blöcke), Status „entsperrt“ von
  Anfang an, also kein DSP-Handshake (`CARDUnlock.c` läuft nicht, wie bei
  Dolphin). Die Flash-ID im SRAM, die `CARDCheck` gegen den Kartenkopf prüft,
  wird beim Einstecken aus dem Kopf abgeleitet (wie Dolphins
  `SetCardFlashID`); eine unformatierte Karte bekommt eine feste. Jede
  geschriebene Seite und jeder gelöschte Block geht sofort in die Datei
  (`pwrite64`), eine Sekunde nach dem letzten Schreiben `fsync`.
  `RE4_MEMCARD` nicht gesetzt: `<Datenordner>/memcard/card-a.bin` (fehlt sie,
  entsteht eine neue Karte, seit 17.10.2026 formatiert und leer, mit
  `RE4_MEMCARD_NEW=blank` unformatiert); `RE4_MEMCARD=none`: Schacht A leer;
  sonst der angegebene Pfad. In der App ist das
  `Android/data/com.ast.re4decomp/files/game/memcard/card-a.bin` (bis
  17.10.2026 `com.ast.re4decomp.core`; ein neuer Disc-Import lässt sie stehen). Schacht B bleibt leer.
- **Startprüfung echt:** Patch 0039 und `runtime/card.cpp` entfallen. Die
  automatischen Läufe (`whole_r100_check.sh` Hauptlauf und Filme,
  `test-core-app.ps1` ohne `-Manual`) setzen `RE4_MEMCARD=none`: ihr Zeitplan
  ist auf den „keine Karte“-Dialog abgestimmt, und es entsteht keine
  Kartendatei im Disc-Ordner bzw. die Spielstände der App bleiben unberührt.
- **Uhr:** `OSGetTime` zählt jetzt in Ortszeit wie die Uhr der Konsole (die
  Spielstände zeigen das Speicherdatum).
- **Dateifunktionen im Spielcode:** Der Prelude macht `__asm__` leer; damit
  fallen in Dateien mit den Schaltern des Spiels auch die Umleitungen der
  C-Bibliothek auf die 64-bit-Dateifunktionen weg, `off_t` bleibt aber 64 Bit
  breit (`lseek` sah im Test die Datei als 0 Bytes groß). `memcard.cpp`
  nimmt deshalb `pread64`/`pwrite64` beim Namen. Die übrigen Dateien mit
  Spielschaltern nutzen nur `fopen`/`fseek` (unbetroffen).
- **Protokoll:** „memcard: …“ (Kartendatei, Dateien auf der Karte bei jeder
  Änderung), „card: …“ (Schritte des Kartenbildschirms, Patch 0058, mit
  Fehlercodes), „run: memory card check done … options …“ (Ende der
  Startprüfung mit den Optionen) und „run: save loaded from the memory card“
  (`state_trace.cpp`, immer an).
- **Testhilfen:** `RE4_TEST_CARD_YES=1` (Pfeil der Kartenfragen auf „Ja“),
  `RE4_STOP_AT_FRAME=N` (Ende bei Bild N), `RE4_TEST_SAVE="free+N[,B][,stop]"` öffnet N Bilder nach
  „Spieler frei“ den Speicherbildschirm wie eine Schreibmaschine (vorher
  Helligkeit = B; `stop`: Ende, wenn er wieder zu ist); `re4_host
  --memcard-format DATEI` legt eine formatierte, leere Karte an;
  `port/tools/memcard_list.py` listet eine Kartendatei (Dateien, Banner,
  Symbol, Kommentar, die CRCs und Inhalte von Systemdatei und Spielständen).
- **PC-Prüfung** (`whole_r100_check.sh` Abschnitt 7): Lauf A mit einer
  frisch formatierten Karte: Startprüfung legt die Systemdatei an (A = ja),
  neues Spiel in r100, dort speichern (Datei 1, Helligkeit vorher 72), Ende;
  Lauf B mit derselben Karte: Startprüfung liest die Systemdatei (Helligkeit
  72), Titel „Load“, Datei 1 laden, r100 läuft weiter; danach
  `memcard_list.py` mit `--expect-system --expect-save 0`.
- **Gerätetest** (`test-core-app.ps1`): holt nach jedem Lauf die Karte
  (`speicherkarte-a.bin`, auch in `senden.zip`) und listet sie im Bericht
  (über WSL), dazu die „card:“-Schritte, das Ende der Startprüfung mit den
  Optionen und geladene Spielstände; `-NewCard` startet mit einer neuen,
  unformatierten Karte (die alte wird vorher gesichert); `-SaveTest` öffnet
  den Speicherbildschirm von selbst kurz nach „Spieler frei“
  (`RE4_TEST_SAVE=free+120`).

### Prüfungen (Cloud, 16.10.2026)

| Prüfung | Ergebnis |
|---|---|
| Kern-Test „card“ (`tests/card_probe.cpp`): neue Kartendatei (2 MB, unformatiert, Mount meldet „kaputt“), Formatieren, `CARDCheck`, 251 Blöcke und Kodierung ANSI frei, Seriennummer; `bh4_system` (1 Block) und `bh4_data00` (8 Blöcke) anlegen, schreiben, Status wie `makeCardStatus` (Banner RGB5A3, C8-Symbol: Lagen 0x40/0x1840/0x1C40/0x1E40), zurücklesen; „existiert“, „keine Datei“; löschen und neu anlegen; Neustart (Datei neu gelesen, Flash-ID neu abgeleitet): Mount bereit, gleiche Seriennummer, Inhalte gleich; die synchronen Aufrufe (`CARDMount`, `CARDFormat`, `CARDCreate`, `CARDWrite`, `CARDRead`, `CARDDelete`) auf einem OS-Thread; Schacht leer: „keine Karte“ | bestanden |
| `memcard_list.py` auf der Karte des Kern-Tests (die Testdateien tragen CRCs aus `CRCCalc` des Spiels) | 2 Dateien, CRCs von Systemdatei, Spielstandkopf und Spielstand stimmen |
| `re4_host --memcard-format` | formatiert, 251 Blöcke frei; `memcard_list.py` liest sie |
| `whole_ir_signatures.py` für die CARD-Dateien und die Aufrufer des Spiels | 0 Abweichungen |
| Platzhalter | 47 Funktionen, 1 Datenobjekt (2 neue aus `CARDNet.c`), wie `stubs-expected.txt` |
| `whole_build.sh` i686 und armhf (qemu), normal und `RE4_OPT=2` | grün (alle Prüfungen, Kern-Test „card“, `memcard_list.py`) |

### Erste PC-Prüfung mit Karte (16.10.2026, Stand `6752926`): Lauf A ohne Spielstand, Lauf B hing

- **Gemeldet:** „Ergebnis A: FEHLER (Exit-Code 0)“, danach hing Lauf B und
  ließ sich weder mit Q noch mit Strg+C beenden; es entstanden keine Logs auf
  dem Desktop (die Prüfung kopiert sie erst am Ende).
- **Ursache (aus dem Code):** Die Ja/Nein-Fragen des Kartenbildschirms
  stellen den Auswahlpfeil auf die **zweite** Möglichkeit (`card.cpp`:
  `cMes.SetCursor(0, 1)`; `mes.cpp code08`: A wählt `m_cur + 1`), das ist
  meist „Nein“. Die Tastenfolge drückte nur A: beim Start „Systemdatei
  anlegen? → Nein“, beim Speichern „In diese Datei speichern? → Nein“ (zurück
  zur Liste, immer wieder), bis `RE4_STOP_AFTER` den Lauf mit Exit-Code 0
  beendete. In Lauf B war die Liste leer, und der Ladebildschirm wartete auf
  B (die Folge drückt nur A/START), bis zur Zeitgrenze von 30 Minuten.
  `timeout` ließ das Spiel in einer eigenen Prozessgruppe laufen, darum kam
  Strg+C nicht an.
- **Behoben:** `RE4_TEST_CARD_YES=1` (state_trace.cpp) hält den Pfeil in den
  Kartenbildschirmen auf der ersten Möglichkeit, A allein sagt „Ja“;
  `RE4_STOP_AT_FRAME=N` beendet einen Lauf bei Bild N, was das Spiel auch
  gerade tut (mit Schritt, Raum, Kartenbildschirm offen/zu). Die Kartenläufe
  laufen im Hintergrund mit Wachhund (alle 30 s der letzte Schritt, nach
  `RE4_CHECK_CARD_LIMIT` Sekunden, Standard 900, Abbruch), Lauf B entfällt,
  wenn A keinen Spielstand schrieb. Strg+C beendet jetzt die ganze Prüfung,
  beendet das laufende Spiel und kopiert Bericht und Logs bis dahin nach
  `RE4-Kern-Pruefung`. Die Logik der Kartenläufe ist mit einem nachgemachten
  `re4_host` geprüft (Erfolg, Hänger mit Zeitgrenze, Strg+C). Neu:
  `test-r100-core.ps1 -OnlyCard` (`whole_r100_check.sh --card-only`) baut
  und prüft nur den Speicherkarten-Teil.
- **Für das Gerät:** Auch dort steht der Pfeil zuerst auf der zweiten Antwort;
  vor A nach oben (bzw. links) auf „Ja“ gehen.

### Zweite PC-Prüfung mit Karte (16.10.2026, Stand `3fdd5a6`, `-OnlyCard`): „ALLES OK“

- Build (i686 `-O2`) und Cloud-Prüfungen grün (der synthetische Filmtest
  übersprungen: ffmpeg fehlte in WSL; `whole_setup_linux.sh` installiert es
  jetzt mit).
- **Lauf A:** Startprüfung mit leerer, formatierter Karte: Kartenprüfung,
  Ergebnis „Steckplatz A 0x0“, Systemdatei angelegt (Schritte 0–10, 320 Seiten
  geschrieben, 5 Blöcke gelöscht), Titel, Debug-Start r100. Spieler frei bei
  Bild 3043; Speicherbildschirm bei 3103 (Helligkeit 72): Kartenprüfung,
  Liste, Datei 1 gespeichert (Schritte 0–9), danach die Systemdatei (Schritte
  0–10), geschlossen bei Bild 3267. Karte danach: `bh4_system` (1 Block),
  `bh4_data00` (8 Blöcke), 242 Blöcke frei.
- **Lauf B (Neustart):** Startprüfung liest die Systemdatei („0x300 saves
  system file“), **Optionen: Helligkeit 72** (vorher 64). Titel „Load“,
  Datei 1 geladen (Schritte 0–5) bei Bild 768: Raum r100, 1-mal gespeichert;
  Hauptschleife ab Bild 794 in r100, Spieler frei, 600 Bilder ohne Fehler.
- **Karte** (`memcard_list.py`, auch die geschickte `speicherkarte-a.bin` in
  der Cloud gelesen): Kopf, Verzeichnis und FAT gültig; Systemdatei
  „resident evil 4 / Systemfile“, CRC richtig, Sprache 1, Land 1 (USA),
  Helligkeit 72; Spielstand „resident evil 4 / FILE 01“, beide CRCs richtig,
  Version 0x116, Datum 2026-10-16 23:55:57 (Ortszeit), Raum r100, Banner
  RGB5A3 und C8-Symbol an den erwarteten Stellen.
- Damit ist auf dem PC (i686) geprüft: Systemdatei anlegen, Speichern,
  Neustart, Optionen erhalten, Laden im Titel. Formatieren über den Dialog
  des Spiels und alles auf dem Gerät stehen aus.

### Erster Android-Build mit Karte (16.10.2026, Stand `3fdd5a6`): Linkfehler

- **Gemeldet:** Der Kern-Build für den Gerätetest brach ab:
  `ld.lld: undefined symbol: __open_real`, `__pread64_real`,
  `__pwrite64_real` (Plan: 50 statt 47 Platzhalter).
- **Ursache:** Das NDK baut mit `_FORTIFY_SOURCE` (Prüfung von Pufferlängen
  in der C-Bibliothek). Bionic ersetzt dann `open`, `pread64`, `pwrite64`
  durch geprüfte Inline-Fassungen, die die echten Funktionen über
  umbenannte Deklarationen (`__open_real` = `open` per `__asm__`) rufen.
  `memcard.cpp` wird mit den Schaltern des Spiels gebaut, deren Prelude
  `__asm__` leer macht; die Umbenennung fiel weg, und in `libre4_platform`
  (gelinkt mit `--no-undefined`) fehlten die Namen. Unter Linux tritt das
  nicht auf (glibc ruft dort echte `__*_chk`-Funktionen), darum blieb es in
  der Cloud unbemerkt.
- **Behoben:** Die Dateizugriffe der Karte (Öffnen mit Ordnern, Lesen,
  Schreiben, `fsync`, Schließen) liegen in `runtime/host_file.cpp`, gebaut
  mit den Schaltern der Plattform; `memcard.cpp` ruft sie über
  `re4_host_file_*` (`re4_runtime.h`) und nutzt nur noch Speicher-, Text-
  und Uhrfunktionen, die auf Android schon immer gelinkt haben.
  **Neue Prüfung in `whole_build.sh`:** Die Objekte von `re4_formats_obj`
  dürfen keine der von Bionic so umgeleiteten Funktionen rufen (`open`,
  `read`, `write`, `pread*`, `pwrite*`, `readlink`, `getcwd`, `umask`,
  `poll`, `recv*`, `send*`, `strlcpy/strlcat`); mit dem alten `memcard.cpp`
  meldet sie `open pread64 pwrite64`, mit dem neuen nichts.
  `whole_build.sh` i686 und armhf grün; der Android-Build läuft erst wieder
  beim Projektinhaber.

### Erster Gerätetest mit Karte (17.10.2026, Retroid, Stand `a43953a`)

- **Test A** (r120, Nachtest M9, ohne Karte): laut Projektinhaber keine Ruckler;
  „Abgebrochene Töne (Voice Drop): 0“. Der Android-Build lief nach der
  Verlegung der Dateizugriffe durch.
- **Test B** (`-Manual -NewCard -SaveTest`, damals unformatierte Karte):
  Formatieren über den Dialog des Spiels, Systemdatei angelegt (Schritte
  0–10), Vorfilm mit START übersprungen, im Titel „Load“ ohne Spielstand
  (Meldung, Code 0), neues Spiel; der Speicherbildschirm der Testhilfe
  speicherte Datei 1 und die Systemdatei. **Gemeldet:** Formatieren und die
  Lademeldung beim Start stören (Punkte 2 und 3); „Retry from a continue
  point“ führt zu einem schwarzen Bild (Punkt 4).
- **Test C** (`-Manual`, Neustart): Startprüfung las die Systemdatei (Ton
  „2“, Steuerung wie gespeichert), Titel „Load“, Datei 1 geladen (Bild 1593,
  r100), weitergespielt, an der Testhilfe erneut gespeichert. **Gemeldet:**
  nach dem Laden fehlte beim Betreten des Hauses die Szene mit dem ersten
  Ganado (Punkt 5); Laden aus dem Pausemenü führt zu einem schwarzen Bild
  (Punkt 6).
- **Punkte 4 und 6: Hänger, Patch 0059.** Beide Wege rufen `GameContinue`
  (`option.cpp`): der Raum startet neu, während seine Musik noch läuft. Die
  Protokolle enden nach `DataCtrl::setData(…r100_04…)`, auch die Tonzähler
  (der Kern stand). `SndRoomBgmLoad` wartet in einer Schleife, bis die Musik
  des Raums davor ausgeblendet ist (`stop_req`); das Ausblenden läuft im
  AX-Interrupt, den der Port nur an Unterbrechungspunkten zustellt. Die
  Schleife ruft jetzt `re4_wait_interrupt()` wie Patch 0045. Neue Testhilfe
  `RE4_TEST_CONTINUE=free+N` (state_trace.cpp: `GameContinue(1)` wie das
  Pausemenü, Log „run: after the retry: room … runs again“); die PC-Prüfung
  nutzt sie in Lauf B nach dem Laden. Die Kartenläufe erkennen jetzt einen
  Stillstand (2 Minuten ohne neue Logzeile) und schreiben die Rückverfolgung
  in den Bericht.
- **Punkt 5: kein Fehler des Ports.** r100 lädt das Ereignis der Szene
  (`evd/r100s03.evd`) vorab, sobald Leon Bereich 6 am Weg betritt, und setzt
  dabei das Raum-Speicher-Flag 0 (`R100Main`). Das Flag kommt in den
  Spielstand, das Vorladen nicht: Nach dem Laden ist Flag 0 gesetzt, also lädt
  `R100Main` nicht mehr vor, und `r100_Sce_look` findet beim Betreten des
  Hauses kein geladenes Ereignis (`readEvent(0, 1)`: `waitLoadOk` schlägt
  fehl, im Protokoll nur `setData("evd/r100s03.evd")` ohne Lesen). Die
  Testhilfe `-SaveTest` speicherte vor dem Haus; die Schreibmaschine von r100
  (Bereich 0x14, Typ 8, bei etwa x −53500, z −15100) steht dahinter, im
  Original ist dieser Spielstand also nicht möglich. Im Geräteskript
  vermerkt.
- **Punkte 2 und 3 (Entscheidung des Projektinhabers, 17.10.2026):** Eine
  neue Karte ist formatiert und leer wie eine aus dem Laden: `memcard.cpp`
  schreibt beim Anlegen die fünf Systemblöcke genau wie `CARDFormat.c`
  (Kopf mit Seriennummer aus der Flash-ID und der Uhrzeit, zwei leere
  Verzeichnisse, zwei FATs mit 251 freien Blöcken); die Frage zum Formatieren
  kommt nur noch bei einer kaputten Karte. `RE4_MEMCARD_NEW=blank` (Gerät:
  `-BlankCard`) legt eine unformatierte an. Die Meldung „Systemdatei wird
  geladen“ bei der Startprüfung entfällt, und eine fehlende Systemdatei legt
  das Spiel ohne Frage und ohne Meldungen an (Patch 0060, auf Nachfrage
  ergänzt; `RE4_CARD_BOOT_MESSAGE=1` verhält sich wie das Original). Kern-Test „card“:
  die neue Karte bindet ohne Formatieren ein, ihre Systemblöcke sind bis auf
  Zeit und Seriennummer gleich mit denen nach `CARDFormat`.

### Nachtests (17.10.2026, Stand `78aa0b9`): M10 erreicht

- **PC-Prüfung** (`-OnlyCard`, „ALLES OK“): Lauf A legt die Systemdatei
  jetzt ohne Frage an (Schritte 0, 2–10), speichert in r100; Lauf B liest
  die Systemdatei (Helligkeit 72), lädt im Titel und bringt r100 nach
  „Retry from a continue point“ nach 31 Bildern wieder in Gang (vor Patch
  0059 hing das Spiel dort), 2400 Bilder ohne Fehler.
- **Gerätetest 1** (`-Manual -NewCard`): laut Projektinhaber alles in
  Ordnung (Start ohne Kartenmeldung, Szene mit dem ersten Ganado, Speichern
  an der Schreibmaschine, „Retry“ und Laden im Pausemenü ohne schwarzes
  Bild); `senden.zip` entstand nicht, weil die umbenannte Datei schon
  existierte, kein neuer Lauf nötig.
- **Gerätetest 2** (Neustart): Startprüfung liest die Systemdatei
  (Helligkeit 63 wie eingestellt), im Titel auf 64 verstellt, „Load“ →
  Datei 1 geladen, r100 läuft; 0 abgebrochene Töne. Laut Projektinhaber
  Laden und Einstellungen in Ordnung.
- **Damit erreicht:** Speicherkarte in Schacht A als Datei, Startprüfung,
  Formatieren (nur noch für kaputte Karten), Speichern an der
  Schreibmaschine, Laden im Titel und im Pausemenü, Systemdaten über einen
  Neustart, Spielstände mit Banner und Symbol.
- **Offen / nicht nachgebildet:** `.gci`-Import/Export (geplant für M11,
  siehe unten), Schacht B, Karte während des Spiels wechseln, `OSResetSystem`
  (Neustart-Angebot bei „kein Platz“; bei 251 freien Blöcken nicht zu
  erreichen).

### Entscheidung des Projektinhabers (17.10.2026): Karten-Import/Export im SELECT-Menü

(Überholt durch die Entscheidungen unter „M11 Android-Feinschliff“: SELECT
kurz öffnet das Menü, SELECT gehalten das Debug-Menü; Austausch mit Dolphin
und GameCube.)

Der Import und Export von Spielständen (`.gci`, eine Datei je Spielstand
wie bei Dolphin) kommt in ein **Menü der App, das sich öffnet, wenn man
SELECT eine Sekunde lang hält** (wie im früheren Android-Port des
Projektinhabers). Eingeplant für **M11 Android-Feinschliff**; das Menü kann
dort auch die Grafikoptionen aufnehmen. Hinweise für die Umsetzung:

- SELECT ist keine GameCube-Taste, das Spiel braucht sie nicht. Im
  Kernmodus schaltet SELECT heute die Diagnose-Anzeige
  (`CoreInput.java`, `APP_OVERLAY`); kurzes Drücken kann das weiter tun,
  langes Halten öffnet das Menü. Vorbild für Menüs mit Controller-Bedienung:
  `CheatDialog.java`/`GameScreenDialog.java` (ARM64-Modus, dort öffnet
  SELECT das Cheat-Menü sofort, `ControllerActivity.java` „1 << 7“).
- Die Kartendatei liegt in der Byte-Reihenfolge des Geräts (Entscheidung
  16.10.2026). Für den Austausch mit Dolphin oder einer echten GameCube-Karte
  muss der Umwandler die Karten-Strukturen (Verzeichniseintrag) **und** den
  Inhalt der Spielstände (`bh4_system`, `bh4_dataNN`, Aufbau in
  `src/game/card.cpp`, CRCs neu rechnen) in GameCube-Reihenfolge bringen.
  `port/tools/memcard_list.py` kennt die Felder schon. Zu Beginn von M11
  klären: nur Sicherung zwischen Geräten mit dem Port oder auch Austausch mit
  Dolphin/GameCube.
- Vorschlag (noch nicht gebaut): Während das Menü offen ist, hält der Kern
  an; ein Import schreibt nur, wenn kein Kartenzugriff des Spiels läuft, und
  das Spiel liest die Karte bei der nächsten Prüfung neu (Lade-Bildschirm).

**Nicht geprüft (Stand des Abschnitts „Prüfungen (Cloud)“):** die PC-Prüfung mit Disc-Daten
(Abschnitt 7: die Bildschirme des Spiels beim Speichern und Laden, das
Formatieren über den Dialog), der Android-Build (das NDK ist in der Cloud
nicht erreichbar) und alles auf dem Gerät. Nicht nachgebildet: Karte während
des Spiels herausziehen/einstecken, Schacht B, `OSResetSystem` (bei „kein
Platz“ bietet das Spiel einen Neustart an; der Platzhalter kehrt zurück),
`.gci`-Import/Export.

## M11 Android-Feinschliff

### Entscheidungen des Projektinhabers (17.10.2026)

- **Reihenfolge:** M11 kommt vor dem Raum-Rundgang (10b); danach soll die
  APK an einige Tester gehen.
- **Diagnose-Anzeige** (SELECT, Text oben links) ausgebaut.
- **SELECT kurz** öffnet das Menü der App: Grafikoptionen, Import/Export der
  Speicherkarte, (Antwort auf die Frage nach einer Offline-Protokollfunktion)
  das Fehlerprotokoll.
- **SELECT eine Sekunde halten** öffnet das Debug-Menü des Spiels (bisher
  L + START, L2 + START auf dem Retroid).
- **`.gci`-Import/Export** soll mit Dolphin und einem echten GameCube
  gehen.

### Bestandsaufnahme (17.10.2026, Cloud)

- Das „alte“ SELECT-Menü ist das „Ingame Options“ des ARM64-Modus
  (`CheatDialog.java`: Cheats, Waffe hinzufügen, MSAA, AF, Mipmapping,
  Raum-FPS, Tiefenunschärfe, Kreiselsensor). Im Kernmodus übernehmen die
  Cheats das Debug-Menü des Spiels; Raum-FPS, Kreisel und Tiefenunschärfe
  gehören zu den eigenen Spielmodulen der App bzw. bräuchten einen Fokus aus
  dem Spielzustand (nicht in M11).
- Der GX-Renderer der App kannte nur die EFB-Stufe (`RE4_GX_SCALE`) und das
  4:3-Bild (`RE4_GX_ASPECT`), beides nur als Testschalter.
- Ein Spielstand ist eine Kopie von Spielvariablen (`card.cpp`
  `makeSaveData`): GLOBAL_WK 0x4F80..0x8678, Gegenstände, Räume, Menü-Wort,
  Händler, dazu Banner/Symbol (Texel bleiben im Port big-endian) und zwei
  CRCs. Für den GameCube muss jedes Feld nach seiner Größe umgedreht werden.
  Die Typen kommen vollständig aus den Headern; unbekannt sind nur
  Platzhalter (`pad_*`, 884 Bytes in GLOBAL_WK, je 2–4 Bytes in Gegenständen
  und Gegnerliste). In allen fünf Spielständen des Projektinhabers (PC-Prüfung
  und Retroid) sind diese Bytes 0.
- Der Kern lief im Hintergrund weiter (die App hielt nur den Ton an).

### Umsetzung (17.10.2026, Cloud)

- **Byte-Reihenfolge der Spielstände** aus den Headern statt von Hand:
  `port/tools/save_layout.py` übersetzt eine Probe mit den vorbereiteten
  Headern (Schalter wie `card.cpp`, also mit den umgedrehten Unions von Patch
  0016), liest die Debug-Information (`llvm-dwarfdump`) und schreibt
  `runtime/save_layout.inc`: je Struktur die Läufe der Felder breiter als ein
  Byte (Unions: das breiteste Mitglied) und die Platzhalter. `whole_build.sh`
  prüft mit `--check`, dass die Tabelle zu den Headern passt.
- **`runtime/gci.cpp`:** dreht `bh4_system` und `bh4_dataNN` Feld für Feld
  um, prüft vorher die CRCs in der Reihenfolge der Quelle (und die Version
  0x116) und rechnet sie danach neu (Kopf-CRC zuerst, sie liegt im Bereich
  der Datei-CRC). Der Verzeichniseintrag (`CARDDir`) wird ebenso umgedreht;
  eine `.gci` ist Eintrag + Blöcke wie bei Dolphin, Dateiname wie Dolphins
  GCI-Ordner (`08-G4BE-bh4_data00.gci`). Abgewiesen: andere Spiele, andere
  Version, falsche CRC, zu kurz. Bytes der Platzhalter, die nicht 0 sind,
  gehen als Bytes mit und werden geloggt.
- **`runtime/card_files.cpp`:** Dateien auf dem Kartenabbild von
  `memcard.cpp` wie die Bibliothek: aktuelle Verzeichnis-/FAT-Kopie
  (höherer Prüfcode), Blöcke wie `__CARDAllocBlock`, neue Kopie mit Prüfcode
  + 1 in die andere Hälfte (erst die Blöcke, dann FAT, dann Verzeichnis),
  Ersetzen gibt die alten Blöcke frei.
- **Kern (`core_api.cpp`):** `re4_core_pause` hält das Spiel in seinem
  nächsten `PADRead` an (Frame-Hook) und lässt es wieder laufen; die
  Kartenfunktionen des Menüs nur, wenn das Spiel steht **und** die
  Karten-Bibliothek die Karte nicht eingebunden hat (`__CARDBlock[0].attached`,
  also zwischen den Kartenbildschirmen des Spiels); sonst `RE4_CARD_BUSY`.
  `re4_core_debug_menu` + Patch 0061: das Debug-Menü auf Wunsch der App.
- **Renderer (`gx_gl.cpp`):** `GxOptions` – Mehrfachabtastung des EFB
  (MSAA 2×/4×; vor jeder Kopie aus dem EFB wird der Bereich in die Texturen
  aufgelöst, die die Kopien lesen) und anisotrope Filterung (nur Texturen mit
  Mipmaps und linearem Filter; Effekte mit Punktfilter bleiben). Grenzen des
  Geräts (`Limits`). `core_mode.cpp`: eine Änderung baut den Renderer beim
  nächsten Bild neu, sobald das Spiel wieder läuft (der Kern schickt dann
  den ganzen Zustand); Stufe „Auto“ = 336 Bildzeilen auf etwa die
  Bildschirmhöhe, höchstens 3×; 4:3 zur Laufzeit.
- **App:** `CoreMenu.java` (Grafik: Auflösung Auto/1×–4×, Kantenglättung,
  Texturfilter, Bildformat 16:9/4:3; Speicherkarte: Dateien mit Raum,
  Spielzeit, Datum, einzeln oder alle exportieren, importieren mit Frage vor
  dem Ersetzen; Fehlerprotokoll; Spiel beenden). Deutsch auf deutschen
  Geräten, sonst Englisch. `CoreActivity.java`: SELECT kurz (beim Loslassen)
  = Menü, 1 s gehalten = Debug-Menü (kurze Vibration); Spiel steht im Menü
  und solange die App nicht vorne ist; R3-Grafikaufnahme nur mit
  `RE4_APP_TOOLS=1` (Testskript) oder dem Schalter im Menü.
- **Fehlerprotokoll (`CoreLog.java`, `ReportProvider.java`):** zeichnet
  ab dem Start auf (Standard an): `logcat` der App in
  `Android/data/<app>/files/logs/log.txt` (2 × 4 MB; bis 17.10.2026 `protokoll.txt`); ein Absturz
  schreibt Signal, Stelle (Bibliothek + Versatz) und mögliche Rücksprünge in
  `crash.txt` (`core_mode.cpp`, danach der Absturzbericht des Systems wie
  bisher). Beim nächsten Start wandert alles nach `logs/previous/`, dazu was
  Android über das Ende sagt (`ApplicationExitInfo`, ab Android 11) und die
  Absturzzeilen des Systems. **Hänger:** stehen die Spielbilder 8 s lang
  still, ohne dass das Spiel angehalten ist (Menü, Hintergrund), schreibt
  die App dreimal im Abstand von 200 ms, wo der Spiel-Thread steckt
  (`re4_core_frame_count`, `re4_core_game_thread`, SIGUSR2 an diesen Thread,
  pc/lr/Stapel mit den Funktionsnamen aus `dladdr`), ebenfalls in
  `crash.txt`. „Bericht senden“ packt beides mit Gerätedaten
  und der Dateiliste der Karte (ohne Spielstände) in eine ZIP und gibt sie
  an Mail/Messenger; „Bericht speichern“ legt sie in einen Ordner.
- **Debug-Menüs nur mit Text** (Wunsch des Projektinhabers, 17.10.2026): die
  senkrechten, flackernden Balken links und rechts im Debug-Startmenü (und im
  Debug-Menü im Spiel) sind die Zeitbalken der Debug-Fassung (`debug.cpp`
  `DebugControl`: Prozess-Zeitbalken, Primitivpuffer, Heap, dazu der
  Controller-Monitor). `ConfigSet` schaltet `DBG_PROC_BAR` und `DBG_PAD_INFO`
  beim Start ein, gezeichnet werden sie, sobald `debug_mode` an ist – also
  genau, wenn ein Debug-Menü offen ist. `state_trace.cpp` schaltet beide
  Flags bei `RE4_DEBUG_HUD=0` (Standard der App) einmal ab, sobald sie das
  erste Mal an sind (Log: „run: the debug bars … switched off“); der
  Flag-Editor des Debug-Menüs kann sie wieder einschalten, `-DebugHud` des
  Testskripts lässt sie an.
- **Werkzeuge:** `re4_host --gci-export KARTE ORDNER`, `--gci-import KARTE
  DATEI.gci [--replace]`; `memcard_list.py DATEI.gci` liest eine `.gci` für
  sich in GameCube-Reihenfolge (ohne den Umwandler des Ports).

### Prüfungen (Cloud, 17.10.2026)

- Kern-Build i686 und armhf (unter qemu) mit allen Prüfungen grün. Neu im Kerntest „card“: ein
  Spielstand und die Systemdatei, mit der Bibliothek angelegt, als `.gci`
  exportiert – Felder gegen die Strukturen geprüft (Spielzeit, Geld, Raum
  mit dem Stage-Byte zuerst, Position als float, Statuswort, Gegenstände,
  Räume, Händler), Banner unverändert, CRCs wie ein GameCube sie rechnet –,
  wieder importiert (ohne Ersetzen abgewiesen, mit Ersetzen und als neue
  Datei angenommen); danach findet die Bibliothek Verzeichnis und FAT in
  Ordnung und liest genau ihre Bytes zurück, auch nach einem Neustart;
  andere Spiele, zu kurze Dateien, falsche CRC abgewiesen. Neuer Kerntest
  „pause“: das Spiel steht im `PADRead`, Kartenfunktionen nur dann, das
  Debug-Menü einmal je Wunsch. `memcard_list.py` liest die exportierte
  `.gci` unabhängig: CRCs in Ordnung, Version 0x116, Raum r100.
  `save_layout.py --check` passt.
- Die echten Karten des Projektinhabers (PC-Prüfung, Retroid): Export →
  `memcard_list.py` liest Optionen, Raum, Datum richtig in
  GameCube-Reihenfolge; Export → Import auf eine neue Karte ergibt
  byte-gleiche Dateien und Einträge.
- App-Renderer (`gx_gl_check`): alle bisherigen Prüfungen, neu mit 4× MSAA
  und 8× AF: Bildkopie, schräge Kante geglättet, EFB-Kopie als Textur und
  Palette über den aufgelösten EFB. `libre4decomp_link_check` (core_mode.cpp
  auf dem Host) gebaut.
- Java der App gegen die Android-Klassen (Robolectric `android-all`, API 35,
  mit einer erzeugten `R`-Klasse) mit `javac` übersetzt: fehlerfrei.
  `CoreInputTests` 69 Prüfungen bestanden.

**Nicht geprüft:** der Android-Build (NDK, Gradle, Lint), alles auf dem
Gerät (Menü, SELECT kurz/lang, Debug-Menü, Debug-Menüs ohne Balken, Grafikoptionen und ihre
Leistung, Import/Export mit der Dateiauswahl, Fehlerprotokoll und Bericht,
Absturzmelder), die PC-Prüfung mit Disc-Daten nach Patch 0061, und **ob
Dolphin und ein GameCube die exportierten Spielstände laden** (dafür
braucht es Dolphin mit der Debug-Disc auf dem PC).

### Erster Gerätelauf mit dem Menü (17.10.2026, Retroid, Stand `d637611`)

`test-core-app.ps1 -Manual`: Android-Build (NDK, Gradle, Lint) fehlerfrei.
Laut Projektinhaber und Protokoll: SELECT öffnet das Menü, das Spiel steht
(„core: paused“ … „running again“); SELECT gehalten öffnet das Debug-Menü;
die Debug-Balken sind weg; Grafikwechsel bauen den Renderer neu (Stufe 2/3/4,
4× MSAA, 16× AF, 4:3, „GX renderer ready … 4 samples (of 4), anisotropy 16
(of 16)“), r100 danach weiter flüssig (gezeigt 56,7 Bilder/s, Renderer
4,8 ms je Bild); „Alle exportieren“ schrieb `bh4_system`, `bh4_data00`,
`bh4_data18`. Die drei `.gci` liest `memcard_list.py` unabhängig in
GameCube-Reihenfolge richtig, auch Datei 19 (r200, Kapitel 6, Modus 2);
die Platzhalter-Bytes sind auch dort 0. Nicht geprüft: Import auf dem
Gerät, Bericht senden, Dolphin.

Aufgefallen: die Zahlen „GX-Arbeit je Bild“ im Protokoll liefen nach einem
Neubau des Renderers über (Zähler des alten Renderers abgezogen); jetzt
beginnen sie mit dem neuen Renderer bei 0.

**Wünsche des Projektinhabers zum Menü (umgesetzt, nicht auf dem Gerät
geprüft):** die Auflösung steht sofort bei der Wahl da (Bildpunkte, Stufe,
bei Auto wie `core_mode.cpp` aus der Bildschirmgröße berechnet, dazu die
Bildschirmgröße); kein „Weiterspielen“ (B schließt), stattdessen eine Linie
unter dem Einleitungstext; Überschriften mittig in Kapitälchen mit Linien
links und rechts; die beiden Schalter des Fehlerprotokolls nebeneinander;
neuer Schalter **FPS-Anzeige unten links** (Spielbilder je Sekunde, aus dem
Bildzähler des Kerns, jede Sekunde neu; steht während Menü/Hintergrund);
„Spiel“ und „Spiel beenden“ entfernt (Beenden weiter mit der Zurück-Taste).

### Absturz in r206 (PC-Prüfung 17.10.2026, Stand `d637611`): REL-Kopf

`test-r100-core.ps1 -Room 206`: Absturz in Bild 613 beim Stagewechsel
(`gameStageInit` → `StageSet` → `cRoomData::linkRelData` → `mem_alloc` →
`OSAllocFromHeap`, Fehleradresse 0x5cae18c8), kurz nach „Read File:
rel/st2_1.rel“. Ursache: Die Raumprogramme liegen als eigene Dateien auf der
Disc (`rel/st1_0.rel`, `rel/st2_1.rel` …). Der Formatumwandler ließ ihren
Kopf big-endian („OSLink converts its header“), aber `linkRelData` (und
`sscrn.cpp`) holt **vor** `OSLink` Speicher für `bssSize`. Big-endian gelesen
wird der Wert riesig; bei `st2_1.rel` ist er negativ, `OSAllocFromHeap`
legte dann den Rest-Block an einer Adresse weit unter dem Heap an
(0x80AE18.. − 0x24000000 ≈ 0x5CAE18..) und stürzte ab. Bei `st1_0.rel`
fiel es nicht auf. Die Raumprogramme in Archiven bekamen ihren Kopf schon
immer beim Laden umgedreht (`convertArchive`).

Korrektur: `rel/*.rel` hat einen eigenen Umwandler (`convertRel`,
`formats.cpp`), der den Kopf gleich beim Laden in Host-Reihenfolge bringt
(`re4_rel_header_to_host`; `OSLink` lässt einen umgedrehten Kopf in Ruhe).
Neuer Kerntest „rel“: ein Kopf mit einer `bssSize`, die big-endian negativ
wäre, kommt in Host-Reihenfolge an, ein zweiter Lauf ändert nichts, ein
späterer Teil der Datei bleibt unberührt; ohne die Korrektur schlägt der
Test fehl.

**Andere Räume** (Frage des Projektinhabers): Fast jeder Raum lädt eines von
15 Raumprogrammen als eigene Datei (`src/game/roomdata.cpp`, `rel_file`):
`st1_0` (r100, r120), `st1_1` (r101–r10b), `st1_2` (r10c–r10e, r119–r11b),
`st1_3` (r10f, r111–r113, r117, r118, r11c–r11f), `st2_0` (r200–r203, r207,
r208, r210, r222), `st2_1` (r204–r206, r209–r20f), `st2_2` (r211–r219),
`st2_3` (r21a–r22b), `st2_4` (r22c), `st3_0` (r300–r30e), `st3_1` (r30d,
r30f–r312), `st3_2` (r315–r31d), `st3_3` (r320–r333), `st4_0` (r400–r411),
`st0` (Testräume r000–r042). Vor der Korrektur hing es vom Speicherwert der
einzelnen Datei ab: niedrigstes Byte ≥ 0x80 → negativ → Absturz (sicher bei
`st2_1`), sonst eine zu große Anforderung, die still scheiterte (`st1_0`,
`st1_1`, `st2_0` liefen: r100, r101, Spielstand in r200). Dasselbe Lesen vor
`OSLink` machen auch das Untermenü-Programm (`sscrn.cpp`, `rel/Sscrn.rel`
über ARAM) und die Werkzeuge des Debug-Menüs (`db_menu.cpp`, `rel/<werkzeug>`);
die Korrektur deckt alle ab, weil jede `rel/*.rel` beim Laden umgedreht wird.
Der Datenblock „DRA“ ohne Umwandler (gemeldet in r206) gehört nur zum
Entwickler-Werkzeug `t_dr.cpp`.

**Raum-Rundgang in der PC-Prüfung** (neu, für die Nachprüfung und für 10b):
`test-r100-core.ps1 -Rooms LISTE` (`whole_r100_check.sh --rooms`, Abschnitt
8): nach dem Build jeder Raum über den Debug-Start, `-TourFrames` Bilder der
Hauptschleife (Standard 1500), je Raum eine Zeile (läuft / Absturz mit
Bibliothek, Versatz und Funktion / Stillstand mit Rückverfolgung), die
Einzelheiten in `rundgang-einzelheiten.txt`. `-Rooms set`: zwei Räume je
Raumprogramm (27), `-Rooms all`: jeder Raum der Tabellen (etwa 150). In der
Cloud nur mit Platzhalter-Daten geprüft (Ablauf und Bericht).

**Erster Rundgang mit Disc-Daten** (PC-Prüfung 17.10.2026, Stand `1af1916`,
`-Rooms set`, i686, je 1500 Bilder): 26 von 27 Räumen laufen, **r206
läuft** (die Korrektur oben wirkt). Absturz nur in **r311** (unten).

### Absturz in r311 (Rundgang 17.10.2026): Division durch null in `SeAtCheck`

Bild 619, gleich nach dem Raumstart: Signal 8 (`SIGFPE`) in `SeAtCheck`
(`src/game/se_at.cpp`, aus `SndWatcher`). Die Raumgeräusche (Datei „ESE“
im Raumarchiv) spielen ihr Geräusch und warten dann entweder eine feste
Zeit (`interval`) oder `rnd_base + Rnd() % rnd_interval` Bilder. r311 hat
ein Geräusch mit `interval` 0 **und** `rnd_interval` 0: eine Division durch
null. Der GameCube rechnet einfach weiter (`divw` durch 0 ergibt den
Quotienten 0, der Rest ist damit `Rnd()`, also 0–255), ein x86-Prozessor
bricht das Programm ab, ARM bricht nicht ab, die Divisionsroutine des
Compilers liefert aber keinen festgelegten Rest. Die Umwandlung der Datei
ist richtig (alle zehn 16-Bit-Felder ab 0x10 werden gedreht; ein Fehler dort
hätte schon die Versionsprüfung von `SeAtInit` scheitern lassen); die Null
steht wirklich in den Daten.

Korrektur: **Patch 0062** (`se_at.cpp`): bei `rnd_interval` 0 nimmt der
Port das Ergebnis des GameCube (`Rnd()`); `Rnd()` wird weiterhin genau
einmal aufgerufen, die Zufallsfolge bleibt gleich. Geprüft: Build i686 und
armhf in der Cloud. **Nicht geprüft:** r311 mit Disc-Daten (nächster
Rundgang). Ein Test ohne Disc-Daten fehlt bewusst: `SeAtCheck` erreicht die
Stelle nur, wenn `SndCall` wirklich spielt (Tonbank, Treiber).

Weitere Divisionen durch null, die der GameCube still übergeht, sind
möglich; der Rundgang über alle Räume (`-Rooms all`) findet sie auf dem PC
als Absturz mit Funktionsnamen.

### Rundgang über alle Räume (PC-Prüfung 17.10.2026, Stand `69bbc10`)

`-Rooms all`, i686 -O2, je 1500 Bilder: **122 von 126 Räumen laufen**, r311
läuft (Patch 0062 wirkt). Vier Fundstellen; die Adressen im Bericht passen
genau zum gleichen Build in der Cloud, die Befehle an den Stellen sind
nachgesehen:

| Raum | Was | Ursache | Stand |
|---|---|---|---|
| r404 | Stillstand beim Raumstart (`SINF` ← `cLightInfo::updateMatrix` ← `cModel::setAng` ← `r404_initEmSet`) | Der Söldner-Raum wählt einen von drei Startpunkten und holt den Winkel über `&pos + n * 12 + 0x28`: für den Prüfsummen-Build geschrieben, setzt voraus, dass CodeWarrior `rot` 0x28 Bytes über `pos` in den Stapel legt. Clang ordnet anders, Leon bekam einen Winkel von irgendwo im Stapel, das Zurechtrücken des Winkels (`LIMIT_ANGLE`) endete nie. Im übrigen Spielcode (ohne `src/lib`) gibt es diese Art Adressrechnung sonst nicht | **behoben** (Patch 0063); zweiter Lauf mit Disc-Daten (Stand `4042c81`): läuft |
| r215, r227 | Absturz in `cLightMgr::setEnv` (Lesen bei Adresse 8: kein Lichtschnitt), aus dem Licht-Paket einer Zwischensequenz (`Event::ExePacket_Lit` → `LightMgr.update` → `registCut`) | Erste Vermutung (gleichnamige Lichtdaten mehrerer Zwischensequenzen, `DatTbl::SetDat` behält die erste) war nicht die Ursache; die Änderung dafür bleibt (richtig, Kerntest „event“). **Ursache laut zweitem Lauf:** `evd/r215s02.evd` und `evd/r227s02.evd` (nachgeladene Zwischensequenzen) haben eine 32-Byte-Texttabelle (MDT), deren Block nach dem ersten Wort aufhört (Zwischensequenz ohne Text). Der MDT-Umwandler brach ab, und damit blieb **jede weitere Datei** der Zwischensequenz big-endian, auch ihre Lichtdaten. Dieselbe Tabelle steht als Eintrag 17 in `st2/r215.das` | **behoben:** MDT-Umwandler nimmt den abgeschnittenen Block hin (dreht das eine Wort, lässt den Rest); EVD und EVS wandeln nach einer abgebrochenen Datei die übrigen weiter um. Kerntest „event“ um diesen Fall erweitert (schlägt mit dem alten Umwandler genau so fehl). **Dritter Lauf mit Disc-Daten (Stand `11c4154`): beide laufen** |
| r229 | Absturz in `IdGeneralTrans` beim Funkgespräch (Codec, Bild 789, 96 Bilder nach dem Öffnen) | Die Zeichenschleife (`ExecOt`) ruft die Funktion eines Eintrags auf, deren Adresse **0x74 höher** war als `IdGeneralTrans`: die Gesichts-Animation (`CalculateShape_new`, `*vtx += delta`) schrieb in die Zeichenliste (gleicher Zwischenspeicher `GetPrimBuff`). **Ursache laut drittem Lauf** (Hinweise von Patch 0064): Ein Kanal des Partnergesichts (Modell mit 1576 Eckpunkten in `ss_oc205.dat`) nennt Eintrag 9 einer Formen-Tabelle mit weniger Einträgen. Das Spiel liest dann die Eckpunkt-Listen hinter der Tabelle als Eintrag. Auf dem GameCube ist dessen Anzahl big-endian 0xFFFF0000, also negativ, und es wird nichts addiert; in Host-Reihenfolge sind dieselben Bytes 65535, und die Werte gingen weit hinaus. (Geteilte Eckpunkt-Listen, die erste Vermutung, waren es nicht; die Änderung dafür bleibt, Kerntest „morph“.) | **behoben:** Patch 0064 gibt einem Kanal hinter der Tabelle keine Werte, wie der GameCube, und überspringt Eckpunkt-Nummern außerhalb des Modells; beides wird einmal gemeldet. Brücken-Test um den Fall erweitert (schlägt ohne die Korrektur fehl). Der dritte Lauf (nur mit der Nummernprüfung) lief schon durch; **vierter Lauf (Stand `0aa2337`): läuft**, nur noch die erwarteten Hinweise (Kanäle 5 und 6 nennen die Einträge 9 und 10 einer Tabelle mit 9 Einträgen), keine falschen Eckpunkt-Nummern mehr |

**Vierter Rundgang über alle Räume** (PC-Prüfung 17.10.2026, Stand
`0aa2337`, i686 -O2, je 1500 Bilder ab der Hauptschleife): **126 von 126
Räumen laufen**, ALLES OK. Damit ist jeder Raum der Raumtabellen einmal
gestartet und 1500 Bilder gelaufen (Debug-Start, ohne Spielen; was erst beim
Spielen passiert, prüft 10b). Auf dem Retroid nicht wiederholt.

Der Rundgang-Bericht zeigt bei Problemen jetzt zusätzlich die letzten 30
Zeilen **vor** dem Absturz (vorher nur die Rückverfolgung), die letzten
Meldungen des Formatumwandlers, **alle** abgebrochenen Umwandlungen und die
Hinweise der Port-Prüfungen (`check: …`, auch bei Räumen, die laufen). Das
PC-Prüfprogramm schreibt beim Absturz die Register und den Speicher um jedes
Register, das in den GameCube-Speicher zeigt.

Noch offen aus dem Lauf: `st2/r215.das` Eintrag 9 (`ITM`, 0x40 Bytes) wird
nicht vollständig umgewandelt; der Grund stand nicht im Bericht (der nächste
zeigt ihn). r215 lief trotzdem 750 Bilder.

### Vom Projektinhaber gemeldet (17.10.2026, ohne Protokoll) – für 10b

- Vereinzelt lässt sich ein Gegenstand nicht aufnehmen: das „A“-Symbol
  erscheint, danach reagiert das Spiel nicht mehr.
- Super Magnum: lädt nur nach, solange R2 gehalten wird; lässt man los,
  senkt Leon die linke Hand und die Patronen fliegen von selbst in die
  Waffe.
- Raum r206 springt zurück ins Startmenü der App (Disc-Import): ein Absturz
  des Kerns, gefunden und behoben (oben, „Absturz in r206“); läuft in der
  PC-Prüfung mit Disc-Daten und **auf dem Retroid** (17.10.2026).

### Zweiter Gerätelauf M11 (Retroid, 17.10.2026, Stand `3a9a8a6`, `-Manual`)

- Dritter Gerätelauf (17.10.2026): Zwischentext unter dem Titel auch
  zentriert; **alle Texte, die wir einbauen, nur Englisch** (Menü,
  Meldungen, Fehlerbericht und seine Dateinamen: `logs/log.txt`, `crash.txt`,
  `previous/`, `exit.txt`, `device.txt`, `re4-report-….zip`); neuer Schalter
  **„Depth of field“** (Tiefenschärfe, wie im ersten Android-Port; Standard
  an = das Spiel wie es ist; Patch 0065, `re4_core_set_dof`). Java gegen die
  Android-Klassen und die JNI-Bibliothek auf dem Host gebaut, Kerntest
  „pause“ prüft den Schalter; **auf dem Gerät nicht geprüft**.
- Vierter Gerätelauf (17.10.2026, Stand `b778140`): der Schalter kommt im
  Kern an (`core: depth of field off/on`), macht aber in r100 keinen
  sichtbaren Unterschied. Grund: das Original zeichnet Tiefenschärfe nur, wo
  ein Lichtschnitt des Raums (`FocusLevel`) oder eine Zwischensequenz
  (Fokuskurven) sie verlangt; die sichtbare Unschärfe der ersten App
  (RE4-Decomp, `depth_of_field_gl.cpp`) war ein **eigener Autofokus** der
  App, kein GameCube-Effekt. Wunsch des Projektinhabers (17.10.2026): bei
  „an“ der Hintergrund unscharf wie in RE4-Decomp. Umgesetzt als
  **Autofokus im Kernmodus** (Patch 0065 erweitert, `runtime/dof.cpp`,
  `gx_gl.cpp Autofocus`): wenn der Spieler frei ist (Hauptschleife, keine
  Zwischensequenz, kein Untermenü) und das Spiel selbst keinen Fokus
  anlegt, legt `Filter01Trans` einen Eintrag in die Filtertabelle (OT
  0x12, wo auch die Tiefenschärfe des Spiels läuft: nach der Szene, vor den
  Tabellen der Anzeigen 0x13–0x15). Er schreibt BP-Register 0x5A (gibt es
  auf dem GameCube nicht) in den GX-Strom; der GX-Nachbau der App zeichnet
  dort den Weichzeichner der ersten App: Entfernung in der Bildmitte
  (Median aus 9 Tiefen) = Fokus; was deutlich dahinter liegt, wird bis zu
  60 % weich, Näheres bleibt scharf, nur Punkte hinter dem Fokus werden
  gemischt; Fokus weiter als 6 m (Blick in die Ferne) = keine Unschärfe.
  Entfernungen aus der Perspektive der Szene und dem XF-Viewport. „Aus“
  schaltet beides ab (Spiel und App). `RE4_DOF_AUTOFOCUS=0` lässt den
  Autofokus weg (nur das Bild des Spiels). Das Protokoll schreibt, welche
  Tiefenschärfe läuft (`dof: from frame N: light cut …`, `event near/far`,
  `autofocus (app)`, `none`), die Zeile „GX:“ der App zählt die
  Autofokus-Durchgänge. Geprüft: `gx_gl_check` (echtes GLES3, mit und ohne
  4-fach-MSAA: ferne Kante weich 209/46, nahe Kante scharf, Alpha bleibt,
  Blick in die Ferne scharf), Kern-Laufzeittest „gx“ (die Markierung kommt
  jedes Mal im Strom an, Decoder-Prüfsumme unverändert), i686 und armhf.
- Fünfter Gerätelauf: Absturz im Debug-Menü (SELECT halten), Waffenseite,
  beim Navigieren über den ersten bzw. letzten Eintrag hinaus
  (`eprintf_main`, Fehleradresse 0, aus `tp_pl_weapon`): Patch 0066. Die
  anderen Seiten des Debug-Menüs (`t_option.cpp`, `t_util.cpp`
  `ToolMenu`, `t_flag.cpp`, `t_log.cpp`, `t_bugcheck.cpp`) laufen mit
  richtiger Anzahl im Kreis bzw. begrenzen.
- Fünfter Gerätelauf (17.10.2026, Stand `d929d4b`): der Autofokus läuft
  (941 Durchgänge im Protokoll), aber **im Stand ist alles unscharf**: die
  Bildmitte liegt dann auf Leon bzw. seiner Waffe, er wird zum Fokus und
  alles hinter ihm weich (in RE4-Decomp dasselbe Problem; dort wurde der
  Fokus vor dem Zeichnen Leons gemessen). Behoben: der Kern rechnet je Bild
  aus, wie tief Leon und seine Waffe ins Bild reichen (Tiefe ihrer Gelenke
  in Blickrichtung der Kamera, `cParts::mat`, plus 60 cm für Körper, Hand
  und Lauf; `re4_dof_player_depth`, `dof.cpp`) und schickt das als Wert der
  Markierung (BP 0x5A). Der GX-Nachbau nimmt den Fokus nur aus Punkten
  dahinter: die 9 Tiefen der Mitte, oder, wenn weniger als 5 davon hinter
  Leon liegen, 35 um die Mitte (15 % breit, 12 % hoch); keiner dahinter =
  keine Unschärfe. Geprüft: `gx_gl_check` mit einer Szene wie im Stand
  (Leon 1 m in der Mitte, Wand 3 m, ferne Kante 20 m): ohne Leons Tiefe wird
  die Wand weich (36/219, der gemeldete Fehler), mit ihr bleibt sie scharf
  (0/255) und die ferne Kante wird weich (209/46), Leon bleibt scharf;
  nichts hinter Leon = scharf; mit und ohne MSAA. **Auf dem Gerät nicht
  geprüft.**
- Menü (SELECT kurz): passt laut Projektinhaber („wunderbar“); Wünsche:
  die Linie unter dem Titel weg (die Linien der Überschrift „Grafik“ reichen),
  Titel „Resident Evil 4 Options“ zentriert, ohne Linien. Umgesetzt
  (`CoreMenu.java`), Java-Übersetzung gegen die Android-Klassen geprüft,
  **auf dem Gerät nicht geprüft**.
- r206 läuft auf dem Gerät.
- Super Magnum: die Nachlade-Animation ist weiter falsch (die Patronen
  fliegen von selbst in die Waffe, Leon lädt nicht). Wird vor den Testern
  angegangen (Entscheidung des Projektinhabers, 17.10.2026), siehe unten.

### Super Magnum: Nachladen (M11, geklärt)

Ablauf im Spiel: `wep15` (Broken Butterfly, im Debug-Menü „SUPER MAGNUM“)
lässt die Pistolen-Steuerung `wep/pl_handgun.cpp` laufen. Beim Nachladen
spielt Leon die Bewegung `0x2D/0x2E/0x2F` des Waffenarchivs (nach Stufe), die
Waffe selbst `0x33/0x36/0x35` (Trommel und Patronen), beide ab Bild 0.
`wep02_r2_reload` Schritt 1 lässt Leons Bewegung bis `PlReloadEndTbl[15]`
(96/80/52 Bilder) laufen, auch wenn das Zielen losgelassen wird; erst danach
senkt er die Waffe. Gemeldet: mit gehaltenem R lädt Leon, lässt man R los,
senkt er sofort die linke Hand und die Patronen fliegen allein in die Waffe.
Im Code (Tabelle, Waffennummer, Bildzähler, `joyKamae`, `keyReload`,
`EmRoutineSetW`/Patch 0023) ist ohne Lauf keine Port-Abweichung zu sehen.

Werkzeug dafür: **Nachlade-Prüfung** der PC-Prüfung
(`test-r100-core.ps1 -Reload 15`, `whole_r100_check.sh --reload W`):
Debug-Start in r100, sobald Leon frei ist, gibt `RE4_TEST_WEAPON`
(`state_trace.cpp`) ihm die Waffe wie das Debug-Werkzeug `t_option.cpp`
(in einer eigenen Aufgabe auf Platz 2, wie das Werkzeug-Menü; endlose
Munition), dann zweimal Nachladen (einmal mit gehaltenem R, einmal mit R
25 Bilder nach dem Beginn losgelassen). Je Bild eine `wep:`-Zeile (Leons
Zustand und Bewegung, Zielen, Neigung, Zustand und Bewegung der Waffe,
Abstand der linken Hand zur rechten), im Bericht je Nachladen eine
Zusammenfassung, ein Verlaufsauszug und GX-Bilder. Simulierte R/L-Tasten
setzen jetzt auch den Analogwert (`pad.cpp`, wie ein echter Controller).

**Ergebnis der Nachlade-Prüfung (PC mit Disc-Daten, Stand `d732568`):** Der
Kern lädt richtig. Leons Nachlade-Bewegung (Eintrag `0x2C/0x2D` des
Waffenarchivs, 111 Bilder) und die der Waffe (`0x33`) laufen gleichzeitig;
mit gehaltenem R bis zum Ende, mit losgelassenem R (Bild 27) weiter bis
Bild 96 (`PlReloadEndTbl`), dann senkt Leon die Waffe – wie der Code es
sagt. Die linke Hand bewegt sich in beiden Läufen Bild für Bild gleich
(zur Tasche, zurück an die Trommel), die GX-Bilder zeigen das Nachladen,
die Munitionsanzeige zählt hoch.

**Auf dem Retroid** (Projektinhaber, 17.10.2026, Sichtprüfung): sobald R2
losgelassen wird, lädt Leon nicht mehr, die Patronen fliegen in die Waffe,
werden aber nicht geladen. Im Geräteprotokoll begann das Nachladen mit B
**ohne R-Klick** (`0x0200`): R2 war nur halb gedrückt, das Spiel zählt den
Analogwert als R (`game/pad.cpp`, wie der GameCube-Controller); ein zweites
B kam 40 Bilder später. Unterschiede zum PC-Lauf: ARM-Kern statt i686, R nur
analog, das zweite B, die Nachlade-Stufe der Waffe (`weapon_lv_reload`;
nach einem Waffenwechsel im Debug-Menü die des Spielstands). Werkzeug für
den Gerätelauf: `test-core-app.ps1 -WeaponTrace` (`RE4_TRACE_WEAPON=1`): je
Bild beim Nachladen und 40 Bilder danach eine `wep:`-Zeile, dazu Munition,
Waffennummer/Nachlade-Stufe, Tasten (`Joy[0].on`) und R-Analogwert.

**Gerätelauf mit `-WeaponTrace` (Retroid, 17.10.2026, Stand `b778140`):
Ursache gefunden, kein Port-Fehler.** Die Magnum des Spielstands hat die
Stufen aus der Waffenseite des Debug-Menüs (`t_option.cpp
tp_pl_weapon`): Kraft Lv.4, Tempo Lv.3, Magazin Lv.4, **Nachladen Lv.5**
(`weapon_lv_reload` 4; der Spielstand speichert `pG` 0x4FA4..0x500C mit,
die Bytes stehen so auf der Speicherkarte des Laufs). `PlReloadEndTbl` hat
je Waffe nur 3 Stufen; bei Stufe 4 liest `wep02_r2_reload`
`PlReloadEndTbl[15][4]`, also den Eintrag der nächsten Waffe (`[16][1]`,
Messer: **1 Bild**). Damit darf Leon das Nachladen ab Bild 1 abbrechen:
im Protokoll senkt er die Waffe 8 Bilder nach dem Beginn (`pl 0/0/2/1`),
die Waffe geht in ihren Zustand „gesenkt“ (`obj 3/0`), aber
`cObjWep::move` spielt ihre Bewegung weiter (`moveDown` ist bei der Magnum
leer): Bild 8 → 110 der Nachlade-Bewegung `0x33` – die Patronen „fliegen
allein“. Die Munition kommt nur bei Waffen-Bild 34 in `moveReload`, im
gesenkten Zustand nie. Der GameCube rechnet mit derselben Tabelle und
denselben Bytes genauso (der Build trifft die SHA1). Mit einer echten Stufe
(Lv.1–3, wie beim Händler) lädt Leon immer fertig, auch wenn R2 losgelassen
wird, und senkt die Waffe erst ab Bild 96/80/52 (PC-Prüfung oben). Keine
Änderung am Spiel; neu ist der Hinweis `check: weapon … has reload level …`
(`state_trace.cpp`) im Protokoll, sobald eine Waffe eine Stufe über 3 hat.
**Auf dem Retroid bestätigt (Projektinhaber, 17.10.2026):** mit RELOAD
Lv.1 lädt Leon richtig („Reload passt mit Lvl. 1“).
- „Bei einem Gegner abgebrochen“: kein Absturz. Das Testskript beendet die
  App nach der Beobachtungszeit (240 s, Protokoll ohne Fehler). Mit
  `-Manual` beobachtet `test-core-app.ps1` jetzt 30 Minuten (Q beendet
  früher, `-Seconds` legt fest, Bilder alle 2 Minuten) und schreibt beim
  Ablauf „Beobachtungszeit abgelaufen … kein Absturz“.
- Leistung im Lauf: 60 Bilder/s angezeigt, Kern 33,3 ms je Spielbild (30 Hz
  wie das Original) mit 12,9 ms Leerlauf, GPU 17,7–19,7 ms je Bild, Ton ohne
  Verluste.
- **Abschluss M11 (Retroid, 17.10.2026, Stand `093aa7a`, `-Manual`,
  222 s von Hand beendet):** im Stand nicht mehr alles unscharf, der
  Autofokus läuft (`dof: … autofocus (app)`), das Debug-Menü (Waffenseite
  über das Listenende hinaus) stürzt nicht mehr ab, kein Absturz im Lauf
  („Sieht gut aus. Klappt auch das DEBUG Menü“, Projektinhaber). M11 ✅.
- Ein Tester hat ein Gerät, das nur 64-bit-Apps ausführt (kein
  `armeabi-v7a`): der Kernmodus läuft dort nicht. **Entscheidung des
  Projektinhabers (17.10.2026):** erst M11 abschließen (dazu jetzt die
  Nachlade-Animation der Super Magnum, vor den Testern), dann **M12 ARM64
  vor 10b** (eine APK für 32- und 64-bit-Geräte). Der Weg für 64 bit wird
  in der Bestandsaufnahme von M12 mit Zahlen vorgelegt und dann entschieden.

## M12 ARM64

Bestandsaufnahme und Vergleich der Wege: `port/docs/arm64-bestandsaufnahme.md`.
**Entscheidung des Projektinhabers (17.10.2026): Weg A, nativ 64 bit.** Alles,
was das Spiel adressiert, liegt unter 4 GB (wegen der Bit-31-Prüfungen, siehe
„Adressen der Spielbibliotheken“, genauer zwischen `0x80000000` und
`0xFFFFFFFF`); Strukturen mit fester Form (Disc-Daten, Spielstand, feste Plätze)
behalten ihre 32-bit-Form.

Schritte: A1 64-bit-Linux-Ziel baut und linkt → A2 Adressen unter 4 GB →
A3 feste Formen und feste Plätze → A4 Laufzeit- und Kerntests auf x86_64 →
PC-Prüfung mit Disc-Daten → arm64-Build der App, eine APK mit beiden ABIs →
Gerätetest.

### Schritt A1: 64-bit-Linux-Ziel (17.10.2026, Cloud)

`port/tools/whole_build.sh x86_64` (Ziel `x86_64-linux-gnu`,
`cmake/linux-x86_64.cmake`, steht für `arm64-v8a`): **0 Fehler, alle 600
Spieldateien, 114 Module gelinkt, 47 Funktions- und 1 Daten-Stub wie auf
ILP32** (`stubs-expected.txt` unverändert). `--self-test` (Sprungstellen
zwischen Modulen, neue x86_64-Vorlage) und `--check-modules` (114 Module, 0
offene Importe) laufen. Neu:

- Umschreibungen für LP64 (Tabelle „Mechanische Umschreibungen“): genaue
  Breiten in den Original-`types.h`, Alias-Namen mit `RE4_ASM_U32`/`RE4_ASM_NEW`,
  `size_t` in eigenen Deklarationen, Füllfelder, Strukturen mit fester Form
  (`fixed_form.json`, 42 Zeiger-Glieder in 20 Strukturen), `TaskFunc` 4 Bytes.
- Patch 0067 (variadische Aufrufe mit festen Zeigern, `SEQGetVolume`).
- Sprungstellen für x86_64 und arm64 in `whole_link_plan.py`;
  `libre4_main_lp64.ver` (Spiel-`new` heißt dort `_Znwm`), `game_heap.cpp`.
- Größenprüfungen der SDK-Nachbauten (`dvd.cpp`, `os_thread.cpp`) je
  Zeigerbreite; `OSModuleLink`/`ModuleInfo` mit 4-Byte-Verweisen.

Prüfung ILP32 nach den Änderungen: i686 und armhf (qemu) komplett grün
(Sprungstellen, Module, Laufzeit-, Kern-, Karten-, Spielstand-, Format- und
Filmtests). Auf x86_64 bricht
`--runtime-test` wie erwartet beim ersten Modul ab: die Bibliotheken liegen
über 4 GB (`OSLink` springt an eine gekürzte Adresse) → Schritt A2.
`save_layout.py --check` meldet auf x86_64 eine andere Form des Spielstands →
Schritt A3.

### Schritt A2: Adressen unter 4 GB (17.10.2026, Cloud, Linux)

Alles, was das Spiel sieht, liegt im **Fenster `0x80000000–0xFFFFFFFF`**: MEM1
wie bisher am Anfang, darüber Bibliotheken, Stacks und Speicher der
Plattformschicht.

- `runtime/game_window.cpp`: `re4_window_map` holt Adressraum ab `0x83000000`
  (mit `MAP_FIXED_NOREPLACE`, sucht freie Lücken in `/proc/self/maps`);
  `re4_check_window` bricht mit Logmeldung ab, wenn eine Adresse außerhalb
  liegt; `re4_check_game_address` prüft auf LP64 auch die Obergrenze.
- Stacks der Spiel-Threads (`port/src/stack_context.cpp`) kommen aus dem
  Fenster (schwacher Verweis auf `re4_window_map`, sonst wie bisher).
- Linux-Vorlader (`host/loader.c`): belegt auf 64 bit zuerst jede freie Lücke
  zwischen 4 GB und dem Stack (vor dem ersten `malloc`, damit dessen
  brk-Bereich nicht über 4 GB anfängt; `/proc/self/maps` ohne stdio gelesen),
  dann wie bisher alles unter `0x83000000`. Bibliotheken, Thread-Stacks und
  große `malloc`-Blöcke landen danach von selbst im Fenster (Bibliotheken
  oben bei `0xFE…–0xFF…`). Prüft `malloc` auf eine Adresse unter 4 GB; der
  Starter läuft auf einem eigenen Thread mit Stack im Fenster; eine
  `malloc`-Arena. Auf Android legt die App selbst fest, wohin Bibliotheken und
  Stacks kommen (späterer Schritt).
- Patch 0068: Adressen über vorzeichenbehaftete 32-bit-Werte (auf LP64 wird
  daraus `0xFFFFFFFF8…`): `OS_BASE_CACHED` (Absturz in `__AXVPBInit`),
  `cModel::modelInit` (zehn Aufrufer), Formtabelle in `shape.cpp`. Gefunden
  mit `-Wint-to-pointer-cast`: von 1049 Umwandlungen 32-bit-Zahl → Zeiger
  kommen 14 aus vorzeichenbehafteten Typen, davon sind vier Nullen.
- Patch 0069: `GXTexObj` hat im Spiel 32 Bytes; die SDK-Innensicht wuchs auf
  LP64 auf 40 und überschrieb Nachbarspeicher (Division durch null in
  `__GXGetNumXfbLines`). `userData` 4 Bytes, Größenprüfungen für die vier
  Innensichten.
- Testspiegel `TestFileInfo` je Zeigerbreite (`runtime_tests.cpp`).

Ergebnis x86_64: `--self-test` ✅, `--check-modules` ✅ (114 Module, 0
offen), **`--runtime-test` 10 von 10 ✅** (link, dvd, yz2, dvdq, stream, gx,
math, trace, tasks, vsync). `--core-test` bis `axfx` ✅, dann hängt die
Film-Bibliothek: `SFMPS_Init` bleibt absichtlich stehen, weil ihr
Arbeitsblock auf LP64 größer ist als die feste Grenze (CRI, nächster Punkt).

Prüfwerkzeug `port/tools/m12/fixed_layout_check.py`: vergleicht für jede
Struktur, die die Formatumwandler nennen, Größe und Lage aller Glieder
zwischen armhf (Vorbild, wie GameCube) und x86_64. Ergebnis: **83 von 83
gleich**.

### Schritt A3: Strukturen, die auf 64 bit auseinanderlaufen (17.10.2026, Cloud)

Der dekompilierte Code beschreibt manche Objekte mehrfach: als vollständige
Struktur, als Teilsicht mit Platzhaltern (`Uint8 pad[0x40]`, die
GameCube-Abstände festhalten), unter anderem Namen in einer anderen Datei,
als Byte-Tabelle fester Größe. Auf ILP32 stimmen alle Sichten Byte für Byte
überein, auf LP64 verschiebt jeder Zeiger die Glieder dahinter. Dafür gibt es
jetzt Prüfwerkzeuge in `port/tools/m12/` (vergleichen den armhf-Bau als
Vorbild mit dem x86_64-Bau; die mit libclang brauchen
`pip install libclang==18.1.1`, nur in der Cloud):

| Werkzeug | prüft | Ergebnis nach den Patches |
|---|---|---|
| `fixed_layout_check.py` | Strukturen, die die Formatumwandler nennen (Disc-Daten): Größe und Lage aller Glieder | 83 von 83 gleich |
| `view_check.py` | Strukturen gleichen Namens in mehreren Dateien: gleicher GameCube-Abstand → gleicher LP64-Abstand (mit Code-Erzeugung, damit jede benutzte Struktur angelegt wird) | 794 Strukturen mit mehreren Sichten, 0 auseinander (vorher 11) |
| `decl_size_check.py` | globale Variablen: Größe der Definition gegen die `extern`-Deklarationen (Byte-Tabellen) | 1351 Variablen, 0 (vorher 2) |
| `proto_check.py` | C-Funktionen, deren Dateien einen Parameter mit verschiedenen Strukturen beschreiben | 2112 Struktur-Parameter, 0 (vorher 7) |
| `cast_check.py` | Umwandlung eines Strukturzeigers in einen anderen (`SFD_TIM(sfd)` legt die Zeitarbeit über `SFD_OBJ.con`) | 44 Umwandlungen, 0 (vorher 3) |
| `raw_offset_check.py` | feste Byte-Abstände in Strukturen (`(u8*) p + 0x18`, auch in Makros, mit Basisklassen; Abstände über `&((T*) 0)->m` zählen nicht) | 0 (vorher 280 Stellen in 13 Dateien) |
| `overlay_check.py` | Arbeitsbereiche über festen Flächen (`cEm::free`, `cEspgen::Free`) und Klassen über Pool-Plätzen | 1564 Klassen, 2585 Umwandlungen, 0 (vorher 823 Fundstellen) |
| `group_check.py`, `gen_views.py`, `split_pads.py` | Hilfen für die LP64-Sichten der CRI-Bibliothek (Teilsichten an der vollen Struktur ausrichten, erzeugte Platzhalter prüfen) | alle Sichten von `SFD_OBJ`, `MPV_OBJ`, `MWPLY_OBJ`, `MWSFFRM_VFRM` stimmen |

Behoben:
- **CRI-Bibliothek (Patch 0070, 36 Dateien):** `Sint32/Uint32` bleiben
  32 bit (Regel `exact_width_types`; die Spielseite benutzt `int`);
  Adressen in `Sint32`-Werten über `Uint32` zurück (36 Stellen);
  Teilsichten des ADX-Dekoders, des Players, des Sofdec-Handles (`SFD_OBJ`
  über Puffer-, Zeitstempel- und Speicher-Sichten) mit 64-bit-Platzhaltern
  aus der vollen Struktur; Byte-Tabellen `adx_fini.c` und SFH-Arbeitsfläche
  in LP64-Größe mit Größenprüfung; Schleifen mit festem Abstand
  (`adx_tsvr.c`, `adx_sje.c`) über die Glieder. **MPEG-Dekoder `MPV_OBJ`:**
  fünf Sichten unter zwei Namen; `mpv.h` benennt die Zeiger, die die anderen
  Sichten in Platzhaltern führen, die übrigen vier Sichten bekommen
  erzeugte LP64-Platzhalter (alle 177 GameCube-Abstände stimmen überein),
  Handle-Schritt 0x1590, Tabellenbereich `MPV_IXA` auf denselben Abständen.
  **Zeitarbeit:** `SFCON` und `SFTIM_WORK` liegen übereinander (geprüft),
  die Uhrquellen-Tabelle (`sfd_tim.c`, GameCube-Abstand 0xD28) und die
  Bildzeit (Wörter 5/6 der Bildinformation) über die Glieder, der Bildplatz
  vor der Bildinformation über `offsetof`; die drei aus Assembler
  übersetzten Rechenkerne von `mpv_umc.c` lesen ihre Zeiger an den
  LP64-Abständen; `sfd_mem.c` führt die Puffernummer als Zahl.
- **Patch 0071:** die Spielseite des Filmbilds (`MWS_FRM`, `mwply.h`) folgt
  auf LP64 der Sicht der Bibliothek (`mwPlyGetCurFrm` schrieb sonst über
  `cSofdec` hinaus).
- **Patch 0072:** Debug-Werkzeuge (Fenster/Knöpfe des Sscrn-Debugmenüs
  `dbg_button.h`, Modellbetrachter `db_mod.h`).
- **Feste Plätze (Regel `lp64_sizes`, `port/whole/lp64_sizes.json`):** auf
  LP64 waren alle 54 Klassen im Gegner-Pool größer als ihr Platz
  (0xEA0 statt 0xDE0) und vier eigene Gegner-Arbeitsbereiche größer als
  ihre Fläche (bis 0xAB8 statt 0xA00), ebenso 55 Objektklassen und fünf
  Objekt-Arbeitsbereiche, dazu ein Effekt-Arbeitsbereich. Jetzt auf LP64
  Gegner-Platz 0xF60 (Fläche 0xAC0), Objekt-Platz 0x4B0 (Fläche 0xF0),
  Effekt-Fläche 0xD0; auf ILP32 unverändert.
- **Patch 0073:** feste Byte-Abstände im Spielcode über die Glieder
  (`RE4_OFS`, auf ILP32 beim Übersetzen gegen den GameCube-Wert geprüft).
  Dabei gefunden, **auch auf 32 bit falsch:** das Puzzle-Untermenü
  (`ss_pzzl.cpp`) las Zustand und Auswahl seines Nachrichtenfensters an den
  Abständen des GameCube-Compilers (MWCC legt den Tabellenzeiger einer
  Klasse hinter ihre Felder, die Compiler des Ports davor); jetzt über die
  Abfragefunktionen des Spiels. Im Rundgang 10b auf das Puzzle achten.
- Testprogramme: `card_probe.cpp` rechnet den Spielstand-Block vorzeichen-
  behaftet; `save_layout.py` findet den Block über
  `save_data_start_addr` (GameCube 0x4F80, LP64 0x4FB8; Inhalt und
  `.gci`-Form gleich, `--check` auf armhf und x86_64 bestanden).

### Schritt A4: alle Cloud-Tests auf 64 bit (17.10.2026, Cloud)

Neues Bauziel **`aarch64`** (`cmake/linux-aarch64.cmake`,
`whole_build.sh aarch64`): 64-bit-ARM-Linux, die Code-Erzeugung von
`arm64-v8a`, ausgeführt unter qemu-aarch64 (Werkzeuge in
`whole_setup_linux.sh`). Der Vorlader (`host/loader.c`) kommt mit der
Speicheraufteilung von qemu zurecht (Stapel unten, Bibliotheken oben, eigene
Bereiche von qemu unsichtbar): er füllt jetzt jede freie Lücke über 4 GB bis
zum Ende des Adressraums und lässt Platz unter dem Stapel nur, wo er wachsen
kann; `RE4_LOADER_MAPS=1` gibt die Aufteilung aus. Unter qemu liegt `malloc`
oberhalb von 4 GB wie auf Android; das Spiel bekommt solchen Speicher nicht
(nur Hinweis im Log). Der Spiel-Thread der App (`re4_core_start`) bekommt auf
64 bit seinen Stapel aus dem Fenster.

| Ziel | Build | Sprungstellen, Module, Laufzeit (10), Kern (15 Teile) | Karte, `.gci`, Spielstand-Form, Formatvergleich (2) | Filmtest |
|---|---|---|---|---|
| i686 | 0 Fehler | ✅ | ✅ | ✅ |
| armhf (qemu-arm) | 0 Fehler | ✅ | ✅ | ✅ |
| x86_64 | 0 Fehler | ✅ | ✅ | ✅ (die 60-Hz-Prüfung schlug einmal fehl, während qemu und ein Download den Rechner auslasteten; einzeln wiederholt bestanden) |
| aarch64 (qemu-aarch64) | 0 Fehler | ✅ | ✅ | ✅ |

**Nicht geprüft:** das ganze Spiel mit Disc-Daten auf 64 bit (PC-Prüfung,
`test-r100-core.ps1 -Lp64`, neu) und auf einem Gerät.

### Schritt A5: Android arm64 (17.10.2026, Cloud, ungeprüft)

Eine APK für 32- und 64-bit-Geräte: die App-Variante `core` hat jetzt
`abiFilters 'armeabi-v7a', 'arm64-v8a'`; `whole_build_android.sh` baut beide
(`--abi` wählt eine), die Bibliotheken liegen in `src/core/jniLibs/<abi>`.
Die Variante `classic` (eigene Spielmodule, arm64) bleibt unverändert.
Auf 64 bit legt die App (`core_mode.cpp`) zuerst einen Platzhalter über
MEM1 und lädt `libre4_platform.so` und `libre4_main.so` mit
`android_dlopen_ext` in Bereiche, die sie oben im Fenster reserviert hat;
der Kern lädt jedes Modul in einen eigenen Bereich des Fensters
(`module_loader.cpp`, 8 MB je Modul, das größte braucht 2 MB) und gibt ihn
beim Entladen zurück. Beim Start steht im Protokoll, wie viel des Fensters
belegt ist (`re4_window_report`).
`test-core-app.ps1 -Abi armeabi-v7a|arm64-v8a` installiert gezielt eine
Fassung (ohne `-Abi` wählt das Gerät, der Retroid nimmt 64 bit).

**Ungeprüft:** In der Cloud gibt es kein Android-NDK (der Download von
dl.google.com ist durch die Netzwerkregel gesperrt); der neue Android-Code ist
nur gegen einen nachgebildeten Header übersetzt (vor dem ersten 64-bit-Gerätelauf
am 17.10.2026 erneut: `module_loader.cpp`, `core_api.cpp`, `game_window.cpp`,
`os_thread.cpp` des Kerns und `core_mode.cpp`, `core_scene.cpp`, `gx_gl*.cpp`,
`gx_texture.cpp` der App für aarch64 mit `__ANDROID__` ohne Fehler und Warnungen). Offen und erst auf dem Gerät
messbar: ob ART oder die Grafiktreiber Teile des Fensters
0x80000000–0xFFFFFFFF schon belegen.

### Gerätefehler vom 17.10.2026 (Retroid, ARM32, Berichte 17:19 und 17:29)

| Meldung | Ursache laut Bericht | Stand |
|---|---|---|
| Nach der Glocken-Szene in r101 landet die App im Startmenü (Import) | Absturz (SIGSEGV) in `FuncPathParametrize` ← `IDSystem::set` ← `r101_Event30_TitleCall` (`libre4_st1_1.so`): der Kapiteltitel über dem Glocken-Ereignis liest `etc/eng/id101.eff` (Bilder, EFF-Form) und `etc/eng/event001.uwf` (Anordnung) **einzeln** über `DC.setData`; für einzeln geladene `.eff`/`.uwf` gab es keine Regel, beide blieben big-endian („no converter“) | **behoben** (Regeln in `formats.cpp`), Kerntest „single files“; **auf dem Gerät geprüft** (Retroid, ARM32, 17.10.2026: Stage 1 komplett durchgespielt, ohne Fehler) |
| Beim Aufheben eines Gegenstands bleibt die A-Anzeige stehen, das Spiel läuft nicht weiter (SELECT geht) | Die Aufheben-Anzeige lädt das Modell `ss/cmn/itm1a.bin` einzeln (`sce_at.cpp`, `DvdReadN`); keine Regel für einzelne `.bin`, das Modell blieb big-endian | **behoben**: Regeln für `ss/cmn/itm*.bin`, `ss/item/*.bin` (Untersuchen im Inventar) und `etc/*.bin` (Kostüm-Modelle in r300, `r300.cpp`); der Formatvergleich mit Disc-Daten nimmt diese Modelle jetzt mit (als einziger Eintrag eines Archivs, gegen den Modell-Leser der App), der synthetische Vergleich enthält eins (8 statt 7 Dateien); **auf dem Gerät geprüft** (Retroid, ARM32, 17.10.2026: Stage 1 komplett durchgespielt, ohne Fehler) |

Gesucht wurde in allen Ladestellen des Spiels (`DC.setData`, `DvdReadN`):
die übrigen einzeln gelesenen Dateien sind Archive (`ss/*/*.dat`, Regel
vorhanden), Texturen (`*.tpl`) und Ereignisse (`*.evd`). Im Rundgang 10b
achten auf: `st1/r101.das` Eintrag #24 `EVS` (32 Bytes) meldet „conversion
incomplete“ (wohl ein leeres Ereignis), dazu die bekannten Kürzel ohne
Umwandler `DRA` und `FNT`.

### Bildfolge am Anfang (Frage des Projektinhabers, 17.10.2026)

Kein Film: `title.cpp` lädt `SS/<sprache>/title.dat` (Archiv mit
Inhaltsverzeichnis), meldet dessen Bilddaten an (Eintrag 4, EFF-Form) und
spielt eine Anordnung des Bildschirm-Systems (Eintrag 6, mit allem
Freigeschalteten 7) über einen Bildzähler ab: Gesundheitshinweis ab Bild 0,
Capcom ab 105, CRI ab 230, Dolby ab 330, Titel ab 585, Menü ab 615
(`include/title.h`); das Nintendo-Logo ist Anordnung 0xB. Jedes Logo lässt
sich nach `TTL_CANCEL_*` mit START überspringen. Ob ein Logo ein eigenes
Bild über schwarzem Grund oder ein ganzes Bildschirmbild ist, zeigt
`port/tools/title_images.py` mit den Disc-Daten: es schreibt jedes Bild als
PNG und listet je Anordnung, welches Bild wo und wie groß in welchen Bildern
des Zählers steht (getestet mit einem nachgebauten Archiv; mit den echten
Daten noch nicht gelaufen). Der Projektinhaber verzichtet auf ein eigenes Logo
(17.10.2026); das Werkzeug bleibt für spätere Fragen zu den Titelbildern.

### Paketname und Startbildschirm (Entscheidung des Projektinhabers, 17.10.2026)

- **Paketname:** Die Kern-App heißt jetzt `com.ast.re4decomp` (vorher
  `com.ast.re4decomp.core`); die Variante `classic` mit den eigenen
  Spielmodulen weicht auf `com.ast.re4decomp.classic` aus
  (`app/build.gradle`). Eine Installation der Kern-App ersetzt eine noch
  installierte frühere „RE4 Decomp“ mit diesem Namen. `test-core-app.ps1`
  installiert mit `-d` (die frühere App kann eine höhere Versionsnummer haben),
  erklärt den Fall eines anderen Signierschlüssels und kopiert einmal aus
  `Android/data/com.ast.re4decomp.core/files/game` die vollständigen
  Disc-Importe (`sys/fst.bin` und `ready.properties`, über einen
  Zwischenordner `.copy-discN`) und `memcard/card-a.bin`, jeweils nur, wenn
  die neue App sie noch nicht hat (`-NoCopy` lässt das aus, etwa um den
  Startbildschirm ohne Discs zu sehen). Die Einstellungen der App (Grafik,
  Tastentausch) liegen im privaten Speicher und beginnen neu.
  `pull-gx-dumps.ps1` nutzt den neuen Namen.
- **App-Name** unter dem Symbol: „Resident Evil 4 Android“ (vorher „RE4 Kern“),
  ebenso im Betreff des Fehlerberichts.
- **Startbildschirm** (nur zu sehen, solange Disc 1 fehlt; sonst startet das
  Spiel sofort): zentriert „Resident Evil 4 Android“, darunter kleiner
  „By Andreas ‘Andiweli’ Stürmer“, eine Linie, „Start game“, dann
  „Controls“ und „Select ISO/GCM files of the November 25, 2004 debug build“
  (ohne Punkt) jeweils zentriert zwischen zwei Linien mit der Tastenbelegung
  und dem Tausch-Schalter bzw. den Import-Zeilen darunter: „Import Disc 1“
  und „Import Disc 2“ über der linken Hälfte, rechts daneben „Disc N Data is
  missing/ready“ („Cancel import“ ebenso, nur während eines Imports; die
  Statuszeile nur, wenn sie etwas meldet). Nach dem ersten Gerätelauf
  (17.10.2026: „zu hoch“) enger: kleinere Abstände oben, unten und an den
  Überschriften. Der Knopf „Menu“ entfällt im Kernmodus: sein
  einziger Eintrag war der Disc-Import, den der Startbildschirm jetzt immer
  zeigt (`classic` behält ihn). Der Java-Code übersetzt gegen die
  Android-Klassen (Cloud); auf dem Gerät noch nicht gesehen.

### Erster 64-bit-Gerätelauf (Retroid, 17.10.2026, Bericht 22:45, Stand `19f45a9`)

- **Ergebnis:** APK mit beiden Fassungen gebaut (der arm64-Kern mit dem echten
  NDK zum ersten Mal), als `arm64-v8a` installiert („Process: 64-bit“). Nach dem
  Import beider Discs bricht der Start ab: „The game core did not start: MEM1
  0x80000000..0x83000000 is taken in this process“. Der Kern lädt beim Start
  der App (`Re4Application` → `CoreBridge.load()` → `JNI_OnLoad`), also bevor
  irgendetwas der App Speicher anlegt; der Bereich ist damit schon von dem
  belegt, was der Prozess vom 64-bit-Zygote erbt oder was ART beim Start des
  Prozesses anlegt (im 32-bit-Prozess war er frei, siehe „Adressbereich auf
  dem Retroid“). Vermutung (ungeprüft): ART auf arm64 legt Bereiche, die unter
  4 GB liegen müssen (der Bereich für große Java-Objekte „free list large object
  space“ in der Größe von `dalvik.vm.heapsize`, der JIT-Code-Cache), in die erste
  passende Lücke unter 4 GB, und das ist hinter dem Boot-Image (~0x70000000)
  die Gegend um 0x80000000.
- **Messung eingebaut:** Die App hält beim Laden des Kerns fest, was unter
  4 GB liegt (MEM1 und Fenster markiert, größte freie Lücke, ob der Platzhalter
  über MEM1 und die beiden Bibliotheken geladen wurden), schreibt die Zeilen im
  Fenster auch ins Android-Protokoll (`RE4Core`) und legt alles als
  `address-map.txt` in den Fehlerbericht; die Meldung nennt den ersten Eintrag in
  MEM1 („… is taken in this process (by …)“). Der Bericht nennt die Java-Größen
  (`dalvik.vm.heapsize`, `heapgrowthlimit`). Ohne neuen Build liest
  `port/scripts/pull-address-map.ps1` (ADB-Sperre, `run-as`) die Belegung des
  laufenden App-Prozesses und schreibt `adressen-64bit.txt`.
- **Gemessen** (`pull-address-map.ps1`, 22:57, PID 14277, arm64-v8a;
  `dalvik.vm.heapsize` 512m, `heapgrowthlimit` 256m, Build `user`):
  `12c00000-32c00000` ART-Hauptbereich, `6f7e6000-6ff8a000` Boot-Image,
  `6ff8a000-73f8a000` Zygote- und Non-moving-Bereich,
  **`73f8a000-93f8a000 [anon:dalvik-free list large object space]` über MEM1**
  (512 MB, vom 64-bit-Zygote geerbt), `93f8a000-9bf8a000` JIT-Caches (Zygote
  und App), `fa000000`/`fe000000` die beiden Kern-Bibliotheken (oben im
  Fenster, wie geplant). Frei im Fenster: `9bf8a000-ebad6000` (1,3 GB) und
  darüber bis `fa000000`. Die Vermutung stimmt damit für den Bereich der großen
  Objekte; die JIT-Caches liegen im Fenster, aber nicht in MEM1.
- **Ausweg (umgesetzt, auf dem Gerät noch nicht gelaufen):** ART füllt diesen
  Bereich von unten, verschiebt dort nichts und fasst die Daten eines
  lebenden `byte[]` nie an. `core_mode.cpp holdMem1InLargeObjectSpace` legt
  beim Laden des Kerns Java-Arrays in den Bereich, bis eines auf der Seite vor
  0x80000000 beginnt und bis 0x83000000 reicht; dieses hält die App für die
  Lebensdauer des Prozesses (globale Referenz), die Arrays darunter gibt sie
  frei (ART bekommt den Platz zurück). Über die Daten dieses Arrays legt sie
  den Platzhalter, den MEM1 ersetzt (`RE4_MEM1_PLACEHOLDER`). Dauerhaft zählen
  48 MB als Java-Speicher, kurz beim Start bis etwa 240 MB; dafür
  `android:largeHeap="true"` (Grenze dann `heapsize`, 512 MB). Geht es nicht
  (Bereich schon über MEM1 hinaus gefüllt, Array woanders, Java-Speicher zu
  klein), bricht der Start wie bisher mit Meldung ab, und `address-map.txt`
  sagt warum. Geprüft: Übersetzung für arm64 und ARM32 (nachgebildete Header)
  und eine Nachbildung des Bereichs (beste Lücke, sonst Ende) mit fünf Fällen
  in Python (leer, mit Lücken, kurz vor MEM1, 300 KB vor der Seite, schon
  darüber: vier halten MEM1, der letzte bricht sauber ab).
- **Zweiter 64-bit-Gerätelauf (23:12, Stand `4e7e180`):** MEM1 sitzt im Bereich
  der großen Objekte („MEM1 held in ART's large object space …: the data of a
  byte[] at 0x7ffff000..0x83000000 (190 MB below let go)“), der Kern läuft:
  Kartenprüfung, Titel-Archiv geladen, 5 Tastendrücke kommen an. Nach 7 s
  Absturz in `primInit` (aus `titleWait`): `memset` auf `0xffffffff80d427c0`.
  `pG->prim_cnt` ist `s32`, `(void*) pG->prim_cnt` erweitert auf LP64 das
  Vorzeichen. Behoben mit der Regel `lp64_int_casts` (Abschnitt „Mechanische
  Umschreibungen“): `port/tools/m12/int_cast_check.py` fand mit libclang 145
  solche Umwandlungen (142 einfache, 3 in Makros), alle bekommen `RE4_ADDR32`;
  danach 0. Die frühere Suche mit `-Wint-to-pointer-cast` (Schritt A2, 14
  Stellen) hatte die meisten übersehen. Dazu `port/tools/m12/ptr_word_check.py`:
  Zeiger, die als 32-bit-Wort gelesen oder geschrieben werden (auf LP64 bliebe
  beim Schreiben die obere Hälfte stehen): eine Stelle, `*(u32*) &pAnotherRoute
  = 0` in `pl_npc.cpp`, harmlos (die obere Hälfte ist bei allem unter 4 GB
  null). Geprüft: i686, armhf, x86_64 und aarch64 (qemu) alle Cloud-Tests ✅
  (Selbsttest, Module, Laufzeit 10/10, Kern 16/16, Karte, Format-Vergleich,
  Film); `int_cast_check.py` auf x86_64 und aarch64: 0. Der Titel und die Räume laufen in
  der Cloud nicht (keine Disc-Daten): dafür die PC-Prüfung `-Lp64` mit den
  Disc-Daten (noch nie gelaufen) und der nächste Gerätelauf.
- **Dritter 64-bit-Gerätelauf (23:32, Stand `6b896b3`):** Kern und Ton laufen
  (4000 Tonblöcke, der START-Ton ist zu hören), der Titel wird gezeichnet
  (601 Bilder, 11383 Zeichenbefehle), bleibt aber **schwarz**; nach START
  Absturz in `primFree` (aus `titleExit`): `OSFreeToHeap` auf `0xffffffe0`
  (Adresse 0 − 0x20). Ursache: `GxWork` (trans.cpp) liegt über
  `pG->gxStage` bis `pG->prim_base`; sein Zeiger `prim` (dort `prim_base`) hat
  auf LP64 8 Bytes und rückt auf 8 Bytes Ausrichtung, also über `prim_rate`
  und `prim_cnt`: `SetPrimBuffPtr` schrieb dessen obere Hälfte (0) in
  `prim_cnt`, der Zeichenpuffer war damit weg (keine Grundformen, schwarzer
  Titel) und `primFree` gab 0 frei. Behoben: `GxWork` bekommt die feste Form
  (`fixed_form.json`, `prim` 4 Bytes); dazu Patch 0074 für dieselbe Art
  „Sicht mit GameCube-Abständen“: `cast_check.py` prüft jetzt auch C++ und
  Sichten über ein Glied (`(GxWork*) &pG->gxStage`; 29 Funde, davon die
  meisten Umwandlungen zwischen verwandten Klassen, die nur gemeinsame Glieder
  lesen, und zwei absichtlich tote Zuweisungen), dazu eine Suche nach
  Strukturen, die mit einem großen Füllfeld beginnen (24). Behoben: Skelett
  (`cPartsWk`), Speichern (`SaveDataPtrs` feste Form), Fenster (`EmWindowWork`),
  Ereignis-Kamera (`TexRender.cpp`), Raketenwerfer, `EmMarkView`, Lore,
  r300/r310/r30c/r316/r317/r318, Objekt-Betrachter. Geprüft und richtig wie
  sie sind: `FREE_EMSWITCH` (eigener Bereich), `OSLowMem` (in MEM1),
  `PrimBuffView` (Abstand gleich), `GlobalKeep` (Spielstand-Block, gleiche
  Form), `DbModState` (eigene Größe), `EmModule` (unbenutzt). Offen, nur im
  Effekt-Werkzeug: `DbToolWk` (`t_esp`). Wichtig dabei: auf LP64 beginnt der
  eigene Bereich einer abgeleiteten Klasse oft schon in den Füllbytes am Ende
  der Grundklasse (hinter `cEm` bei 0x49C, `sizeof(cEm)` ist 0x4A0; hinter
  `cEsp` bei 0x10C statt 0x110; hinter `cObj` bei 0x3C0 = `sizeof`), also nie
  `sizeof(Grundklasse)` nehmen, sondern `RE4_OWN(B, T, gc)` (Prelude: Abstand
  des ersten eigenen Glieds vom Typ T, auf ILP32 gegen den GameCube-Wert
  geprüft). Annahme bei r300/r318: das Feld liegt 8 Bytes in der eigenen
  Arbeitsfläche des Effekts, die mit 4-Byte-Gliedern beginnt (auf der GameCube
  0x100 = 0xF8 + 8). Geprüft: i686 und x86_64 alle Cloud-Tests ✅ (auf i686
  bestätigen `RE4_OFS`/`RE4_OWN` jeden GameCube-Abstand beim Übersetzen, auf
  x86_64 ein Übersetzungstest die LP64-Werte 0x49C, 0x3C0, 0x10C);
  `cast_check.py` danach: die behobenen Sichten sind weg (übrig bleiben die
  geprüften harmlosen Fälle und die Umwandlung beim Aufruf von
  `TexRenderCamAddOt`, die jetzt nur noch den Zeiger weiterreicht).
- **Vierter 64-bit-Gerätelauf (00:09, Stand `2d6c6e8`) und erste PC-Prüfung
  `-Lp64` (00:10):** Titel sichtbar, die Test-Tastenfolge drückt START (dabei
  wird der Titelruf „Resident Evil 4“ abgeschnitten, wie auf 32 bit mit der
  Tastenfolge), der Vorfilm läuft ganz (76 s, 2278 Bilder, mit Ton); alle
  Filme beider Discs laufen auch in der PC-Prüfung fehlerfrei. Dann:
  Gerät: „stack corruption detected“ in `EventMgr::EvtReadSub` aus `R120Event`
  (Autofahrt): `GetRead` ließ `ReadTbl.GetDat` den Zeiger in `int d` schreiben
  (8 Bytes in 4). PC: Absturz in `EspMove` beim Raumaufbau von r100 (und im
  Kartenlauf): der Effekt-Pool hat feste Plätze von 0x150 Bytes, die größte
  Effekt-Klasse `cEsp3f` hat auf LP64 0x1A8 (`cEsp03/06/0e/4e` 0x170), die
  Plätze überlappten. Behoben: Pool-Platz auf LP64 0x1A8 (`lp64_sizes.json`,
  12 Stellen in `esp.cpp`, gemessen über alle 46 Effekt-Klassen); Patch 0075
  für Zeiger in 4-Byte-Plätzen (neues Werkzeug `ptr_slot_check.py`, 18 Funde:
  `GetRead`, `Event::PFuncTbl`, Puzzle- und Händler-Modelle, Debug-Texte,
  r332, r20d behoben; die übrigen 5 geprüft und richtig). Der Absturzbericht
  der App las auf 64 bit über das Ende des Spiel-Stapels hinaus und stürzte
  selbst ab: liest jetzt mit `process_vm_readv` nur, was lesbar ist.
  Geprüft: i686, armhf, x86_64 und aarch64 alle Cloud-Tests ✅ (der 60-Hz-Teil des Filmtests
  war unter Last einmal rot, allein wiederholt grün); `int_cast_check.py` 0,
  `ptr_slot_check.py` nur die 5 geprüften.
- **Fünfter 64-bit-Gerätelauf (00:30, Stand `3756798`) und zweite PC-Prüfung
  `-Lp64` (00:39):** Die Autofahrt r120 läuft jetzt etwa 2 Minuten. Der
  Projektinhaber meldet: die Fahrt ruckelt und es fehlen immer wieder ein paar
  Sekunden, im Wald fehlen viele Bäume (nur die nahen sind da), Leons
  Spiegelung im Autofenster fehlt teilweise, dann bricht die Fahrt ab. Das
  Protokoll: „GX: display list at 0xfa345000 (96 bytes) is not in memory“
  (8-mal, dann nicht mehr gemeldet), nach 2 Minuten „the command stream lost
  its place“ in `Esp1b_SpTrans`, 8 s kein Bild, die App wird beendet. Die
  PC-Prüfung kommt bis zum Raumaufbau von r100: „OSCheckHeap: Failed
  cell->next->prev == cell“ beim Laden von `em12`, dann Absturz in
  `MotionMoveCore` (`cEm10::move` aus `R100Init`). Ursachen und Abhilfe:
  - **Zeichenliste im Programm:** `GXCallDisplayList(g_EspCommonDisplayList)`
    (das Viereck aller Sprite-Effekte, statische Daten von `libre4_main.so`)
    schreibt die Adresse selbst in den Befehlsstrom; der GX-Nachbau nahm solche
    Adressen nur auf 32 bit an. Mit Weg A liegen die Bibliotheken unter 4 GB:
    `re4gx::listMemory` nimmt sie jetzt auf beiden Breiten (gleiche Prüfung
    `re4_valid_port_ptr`). Alle Sprite-Effekte (Licht, Rauch, Funken …)
    fehlten bisher auf 64 bit.
  - **Effekt 0x1B (gebogenes Band):** Patch 0040 fand die Daten von `cEsp1b`
    über `sizeof(cEsp)`; auf LP64 beginnen sie bei 0x10C (Füllbytes am Ende von
    `cEsp`, 0x110), die Punktzahl war Unsinn, Millionen Vierecke, der Strom
    verlor seinen Platz. Jetzt `RE4_OWN(cEsp, Esp1bSpWork, 0xF8)`. Dieselbe Art
    in r117 (Lichtstrahlen, Byte 0xF9): Patch 0077. Gefunden, nachdem
    `raw_offset_check.py` jetzt auch Abstände hinter dem Ende einer Struktur
    meldet (vorher übersprungen; 4 Funde: r117, dazu der Kopf der Heap-Zellen
    in `main_mem.cpp`/`OSAlloc.c`, der auf beiden Breiten 0x20 Bytes hat).
    Neues Werkzeug `array_view_check.py` (Glied-Felder über ihr Ende hinaus
    indiziert): 3204 Fundstellen, nur 5 rücken auf LP64, alle geprüft und
    richtig (`cDataCtrl::m_DataUnit[32]` als Endmarke, `R203Work.em[6]` immer
    über dasselbe Feld).
  - **Zeichenaufträge (fehlende Bäume, Spiegelung):** Jeder Eintrag der
    Zeichenreihenfolge (`OtPrim`) kommt aus dem Bildpuffer des Raums
    (`GetPrimBuff`, Schritte von 32 Bytes, Größe aus den Raumdaten). Auf dem
    GameCube 0x14 Bytes, ein Schritt; auf LP64 0x28 Bytes, zwei Schritte. Läuft
    der Puffer voll, lässt das Spiel die letzten Zeichnungen des Bildes weg
    (meldet es nur auf dem Bildschirm). `OtData`/`OtPrim` haben jetzt die feste
    Form (`fixed_form.json`, Zeiger 4 Bytes), der Verbrauch ist wie auf dem
    GameCube. Noch nicht mit Disc-Daten bestätigt; dafür:
  - **Spielmeldungen im Protokoll (Patch 0076):** Was das Spiel mit
    `pLog->err/warn/mes` meldet (auf dem GameCube Bildschirm und Konsole), geht
    jetzt als „game: …“ ins Protokoll, jede Meldung die ersten 3 Male, dann
    jedes 1000. Mal mit Zählung (`re4_game_log`, `runtime/log.cpp`).
  - **Heap-Beschädigung (PC-Prüfung):** Neues Werkzeug `alloc_size_check.py`:
    Speicher mit fester GameCube-Größe für eine Struktur, die auf LP64 größer
    ist (auch Tabellen von Zeigern), und Löschen/Kopieren mit fester Länge.
    28 Stellen (9 weitere sind auch auf ILP32 kürzer, absichtlich nur ein
    Kopf; danach 0). Die Kamera eines Modells
    (`ATTACH_CAMERA`, 0x98 → LP64 0xA0, 14 Stellen) und vor allem
    `cModel::cModel`: `memclr_asm(&Motion, 0xD0)` ließ auf LP64 die letzten
    0x20 Bytes von `MOTION_INFO` stehen, darin `pAttachCam`, ein alter Zeiger
    aus dem vorigen Inhalt des Pool-Platzes, über den `MotionMoveCore`
    schrieb. Dazu der zweite Bewegungsbereich von em32, die Bewegungstabellen
    des Spielers (0x6D und 12 Zeiger), die 5 Wegpunkt-Zeiger der Ganados in
    r209 (mit 0x14 Bytes gelöscht, auf LP64 behielten die letzten zwei alte
    Adressen) und drei Debug-Listen. Alles in `lp64_sizes.json`
    (`RE4_LP64_SIZE`, auf ILP32 der GameCube-Wert). Andere Puffer mit fester
    Größe pro Bild (`GetDrawTmpBufAddr`, `IdGetBufferAddr`) halten nur Pixel.
  - **Berichte:** `test-core-app.ps1` und die PC-Prüfung zeigen jetzt die
    Spielmeldungen (je Meldung einmal), `test-core-app.ps1` dazu, wie oft der
    GX-Nachbau Daten nicht erreichte und ob der Befehlsstrom aus dem Tritt kam.
  - Geprüft: i686, armhf, x86_64 und aarch64 alle Cloud-Tests ✅ (auf i686
    und armhf bestätigt `RE4_OWN` beim Übersetzen den GameCube-Abstand 0xF8);
    `alloc_size_check.py` 0. Noch nicht geprüft: Gerät und PC-Prüfung mit
    Disc-Daten.
  - **Ruckeln:** Während der Fahrt wurden etwa 260 neue Grafikprogramme
    übersetzt (75 kamen aus dem Zwischenspeicher, am Ende 333), einzelne Bilder
    brauchten bis 0,64 s. Der Zwischenspeicher der Programme liegt im
    App-Ordner und war mit dem neuen Paketnamen leer; jedes neue Programm wird
    sofort gespeichert. Ein zweiter Lauf derselben Szene sollte ohne diese
    Pausen laufen; die „fehlenden Sekunden“ sind wahrscheinlich die Ereignisse,
    die nach einer Pause zum Ton aufschließen. Bestätigung mit dem nächsten
    Gerätelauf.
- **Sechster 64-bit-Gerätelauf (01:27, Stand `e60ddde`):** Die Autofahrt r120
  läuft jetzt bis zum Ende (Leons Spiegelung im Fenster, viel mehr Bäume, die
  Nebeleffekte sind da). Der Projektinhaber meldet: einige Schattierungen
  stimmen nicht, als ein Polizist austreten geht, sieht man kurz nur
  Grashalme, als das Auto wegfährt klebt eine Nebelwand am Auto und man sieht
  nur Gestrüpp; dann Absturz. Der Absturz kommt beim Aufbau von r100 (der erste
  Ganado, `cEm10::move` aus `R100Init`): `MotionMoveCore` liest
  `pEm->Motion.pHermite_data` = `0x00010000813d85b8`, die untere Hälfte
  stimmt, in der oberen steht 0x00010000. Derselbe Fehler war schon der
  Absturz der PC-Prüfung von `3756798` (auf x86_64 ist so eine Adresse nicht
  kanonisch: „fault address (nil)“). Die Heap-Beschädigung von damals ist weg.
  - **Effekt-Pool in est.cpp:** `EspDelete` (die Effekte eines Besitzers) und
    `EspDeleteEvent` (die Effekte eines Ereignisses an seinem Ende) gingen den
    Pool noch in Schritten von 0x150 durch: sie ließen die echten Effekte
    stehen und gaben Stücke anderer frei. Passt zu Nebel am Auto und Grashalmen
    vor der Kamera. Jetzt 0x1A8 (`lp64_sizes.json`). Gefunden mit der neuen
    Schrittweiten-Prüfung von `alloc_size_check.py` (`(T*) (Anfang + i * K)`;
    bei einer Klasse mit virtuellen Funktionen meldet sie jede feste Schrittweite,
    weil abgeleitete Klassen größer sein können): genau diese 2 Stellen.
  - **Löschen über das Ende hinaus:** Neues Werkzeug `span_check.py` (Löschen
    und Kopieren ab einem Glied über die folgenden): `systemRestartInit` löscht
    `pG` ab `Rno0` mit `sizeof(GLOBAL_WK) - 0x20` (auf LP64 liegt `Rno0` bei
    0x24: 4 Bytes hinter `Global`), jeder Raumwechsel löscht `Snd` ab `room_ok`
    mit `sizeof(SND_WORK) - 0x90` (auf LP64 0x98: 8 Bytes dahinter). Beide jetzt
    mit dem Abstand des Glieds (`lp64_sizes.json`). Dritter Fund im
    Licht-Werkzeug (`t_scroll.cpp`, Debug), gleich auf beiden Breiten.
  - **Wort-Abstände:** `raw_offset_check.py --words` prüft auch Sichten als 2-,
    4- oder 8-Byte-Felder (`((f32*) em)[0x784 / 4]`): die Kamera beim Öffnen
    einer Tür las so `base_mat` der Tür (Patch 0079, jetzt über den Namen); die
    übrigen Funde (`sfd_tim.c`) behandelt Patch 0070.
  - **Absturz in r100, Diagnose:** Statisch zuerst nicht gefunden. Neu:
    Hardware-Haltepunkte (`runtime/watch.cpp`, `re4_watch_write`:
    `perf_event_open` mit einem Schreib-Haltepunkt, je Treffer ein Signal, das
    Befehl und Rücksprungadressen ins Protokoll schreibt; Laufzeittest
    „watch“). Ein Diagnose-Patch ließ `MotionSetCore` die obere Hälfte von
    `pHermite_data` jedes Gegners überwachen. **PC-Prüfung 02:03 (Stand
    `ca46a7f`):** der Schreiber ist `HermiteInterpolation` aus
    `MotionGetSpeed`, Ziel `MOT_HIST(w, flip, n)` = `(u8*) w + (flip * 12 + 8 +
    n * 6)`: 8 ist die GameCube-Lage von `Key_hist`, auf LP64 liegt es hinter
    den zwei Zeigern bei 0x10, der Schlüssel-Verlauf landete in der oberen
    Hälfte von `pHermite_data` (0x000b0000 auf dem PC, 0x00010000 auf dem
    Retroid). Behoben mit Patch 0078 (jetzt `RE4_OFS(MOTION_INFO, Key_hist,
    8)`; der Diagnose-Patch ist entfernt, `watch.cpp` bleibt als Werkzeug).
    `raw_offset_check.py` zählt jetzt auch die festen Teile von Summen mit
    Variablen (`(u8*) w + (flip * 12 + 8 + n * 6)`): dabei derselbe Fehler in
    r320 (`R320_EM_OFS`, `em[]` bei 0x0C statt 0x10), ebenfalls Patch 0078.
    Noch nicht mit Disc-Daten geprüft.
  - Geprüft: i686, armhf, x86_64 und aarch64 alle Cloud-Tests ✅ (Laufzeit jetzt
    11/11 mit „watch“: 3 Treffer für 3 Schreibzugriffe); `alloc_size_check.py`
    und `span_check.py` danach 0. Noch nicht geprüft: Gerät und PC-Prüfung mit
    Disc-Daten.
- **PC-Prüfung 02:20 (Stand `39a2ace`, x86_64 -O2, Disc-Daten):** Formatvergleich
  ✅ (408 Dateien, 0 Fehler), Filme ✅. Der Spiellauf erreicht zum ersten Mal auf
  64 bit die Hauptschleife von r100 (Bild 621, 18 Gegner, Leon und Kamera an
  ihrem Platz): der Absturz in `MotionMoveCore` ist behoben. Danach Absturz im
  ersten gezeichneten Bild: `RefractShaderSetup` aus `commonModelTrans` /
  `ModelRender` / `ExecOt` („fault address (nil)“: eine nicht kanonische
  Adresse).
  - **Ursache:** `ModelTexInfo`, die Sicht des Zeichners auf `cModelInfo` ab
    `flagsDC` (`MODEL_TEX(info)`, Textur-Animation und Überblendung). Auf LP64
    liegt `flagsDC` bei 0x134 und `texBlendTbl` 0xC dahinter, die Sicht hatte
    den Zeiger bei +8 und las 4 Bytes Lücke plus die halbe Adresse. Behoben mit
    Patch 0080 (auf LP64 4-Byte-Packung, Lücke aus den Abständen von
    `cModelInfo`, `static_assert` für jedes Glied, auf allen vier Zielen
    übersetzt).
  - **Warum die Werkzeuge es übersehen hatten:** `cast_check.py` erkannte `&`
    am ersten Wort des Ausdrucks; in einem Makro ist das der Makroname
    (`MODEL_TEX`), die Sicht wurde nie angesehen. Jetzt fragen `cast_check.py`,
    `span_check.py`, `ptr_word_check.py` (`&`, `*`), `array_view_check.py` und
    `alloc_size_check.py` (`+`, `*`) den Operator bei libclang ab, auch in
    Makros. Neu dabei: zwei Zeigertabellen mit 4 Bytes je Eintrag
    (`MEM_ALLOC(n * 4)` im Makro): die Puzzle-Teile (`puzzle.cpp`) und die
    Schatten-Objekte jedes Raums (`ShdInit` in `shadow.cpp`: auf LP64 schrieb
    die zweite Hälfte der Zeiger hinter den Block in den nächsten Heap-Platz).
    Beide in `lp64_sizes.json`. `alloc_size_check.py` liest die Größe jetzt am
    richtigen Argument (`OSAllocFromHeap(heap, size)`, `realloc`) und prüft auch
    Löschen und Kopieren mit `n * K` (keine weiteren Funde). Die übrigen
    Werkzeuge melden nur bekannte, geprüfte Stellen.
  - Geprüft: i686, armhf, x86_64 und aarch64 bauen fehlerfrei (die
    `static_assert` von Patch 0080 halten auf beiden Breiten), Selbsttest,
    Module, Laufzeit 11/11, Kern 16/16, Karte, Format-Vergleich ✅; Filmtest
    auf armhf und aarch64 ✅. Auf i686 und x86_64 war im Gesamtlauf der
    60-Hz-Teil des Filmtests rot (4 Bilder nicht zwei Halbbilder auseinander),
    einzeln wiederholt x86_64 3 von 3 und i686 2 von 3 grün, im zweiten
    Gesamtlauf auf allen vier Zielen grün: ein Echtzeit-Test, der auf diesem
    Rechner wackelt (schon beim Stand `3756798` einmal), der Filmcode ist
    unverändert. Ursache noch nicht untersucht. `cast_check.py`, `alloc_size_check.py` 0. Noch
    nicht geprüft: PC-Prüfung mit Disc-Daten und Gerät.
- **Falls das auf anderen Geräten nicht reicht** (MEM1 in einem anderen Bereich
  von ART): Auswege wären ein frischer App-Prozess über `wrap.sh` (nur
  Debug-APKs) oder ein eigener Prozess für den Kern; beides ändert die Art des
  App-Starts und wird vorher mit dem Projektinhaber abgestimmt.
- Nebenbei: Der Bericht heißt jetzt `memory-card.txt` statt `speicherkarte.txt`
  und ist englisch (Regel: Dateinamen für Tester englisch).

### Neustart M12 (18.10.2026, Cloud): Bericht 11:54, Bestandsaufnahme, Lücken, Vergleichslauf

Branch `claude/exciting-bohr-6akwfd` (von `claude/re4-m12-arm64`, Stand `6a799f4`).

**PC-Prüfung 11:54 (Stand `6a799f4`, x86_64 -O2, Disc-Daten), Auswertung:**
- Build, alle Prüfungen ohne Disc-Daten, Formatvergleich (408 Dateien), alle Filme
  beider Discs: ✅.
- **Spiellauf r100 auf 64 bit: zum ersten Mal ohne Absturz**, 10.000 Bilder ab der
  Hauptschleife (Bild 621), Spieler frei bei Bild 3043, Karte (Z) auf und zu, Leon
  läuft 10.405 Einheiten, Soft-Reset bringt den Titel zurück. Patch 0080 und die
  Puzzle-/Schatten-Tabellen sind damit mit Disc-Daten bestätigt (r100).
- **Speicherkarte: Absturz** im Speicherbildschirm beim Aufbau der Dateiliste
  („card: save: file list“, SIGSEGV an Adresse 0x7). Der Bericht hatte keine
  Aufrufkette (die Kartenläufe zeigten nur ausgewählte Zeilen).
  - Ursache (**bestätigt durch das Protokoll**, siehe unten): 0x7 ist
    `FUNC_PATH_PTR::nPoint`, gelesen über einen Nullzeiger in `FuncPathParametrize`
    aus `CardID::wait` (genau beim Öffnen der Dateiliste). Leer ist `path0` dann,
    wenn `IDSystem::gameInit(0x100)` des Speicherbildschirms keinen Platz bekam und
    jede Einheit der leere Ersatz `tmpId` ist. Der Speicherbildschirm legt seinen
    Speicher in Heap 11 an, dessen Größe `cCard::getUseMemSize` aus GameCube-Größen
    summiert; darin wächst auf LP64 nur der Einheiten-Vorrat (256 × `ID_UNIT`,
    0x138 → 0x158 Bytes: +8 KB).
  - Behoben (Regel `lp64_sizes`): das Budget bekommt auf LP64 genau diese 8 KB dazu.
    **Bestätigt** (Protokoll `karte-speichern.log` des Laufs 11:54, Zeilen 419–435,
    vom Projektinhaber gelesen): „game: alloc[15800]:free[156a0] id_sys.cpp(66)“,
    „game: IDSystem::gameInit() malloc failed“, danach „IDSystem::set() work full“,
    „IDSystem::unitPtr(...): Not found.“ und SIGSEGV an Adresse 0x7. 0x15800 sind
    genau 0x100 × 0x158 (der Vorrat auf LP64), frei waren 0x156A0: es fehlten 0x160
    Bytes; die Regel gibt 0x2000 dazu. Ob der nächste Kartenlauf speichert, zeigt die
    nächste PC-Prüfung.
  - Neue Fehlerart **Speicherbudgets aus GameCube-Größen** (Heap 11 des
    Speicherbildschirms, Heap 10 der Türen, Heap 12 der Untermenüs, Heap 11 der
    Filme, `cDataSwap::SwapOut`): statisch kaum zu fassen (was hineinkommt,
    entscheidet der Ablauf). Darum Patch 0081: jede Anforderung ohne Platz kommt
    ins Protokoll („heap: no room …“) und in den Zustands-Trace, dazu alle 60 Bilder
    der freie Platz jedes Heaps (`OSCheckHeap`); der Vergleichslauf meldet jede
    Anforderung, die nur auf 64 bit scheitert.
- Bericht verbessert: die Kartenläufe zeigen bei einem Absturz Stelle, Register und
  Aufrufkette und alle Spielmeldungen; der Absturz-Handler gibt auch auf x86_64 und
  aarch64 die Register und den Speicher um sie herum aus.

**Schritt 1 – Bestandsaufnahme der Prüfwerkzeuge.** Token-Logik ersetzt (Regel:
Operatoren, Werte, Typen über libclang):
- `raw_offset_check.py`: geprüfte Abstände (`RE4_OFS`, `RE4_OWN`, `offsetof`)
  erkennt es am `offsetof` im Syntaxbaum (statt am Makronamen); Bytetypen über
  ihre Art statt ihren Namen.
- `alloc_size_check.py`: `sizeof` und geprüfte Größen über den Syntaxbaum.
  `RE4_LP64_SIZE` trägt dafür auf beiden Breiten den Aufzählungswert
  `re4_lp64_size` (0). **Die alte Wort-Logik hat Funde verschluckt:** in einem
  Makro gibt libclang für einen Ausdruck aus dem Makro-Rumpf die Wörter ab der
  Definition bis in die nächsten Zeilen zurück; stand dort zufällig „sizeof“,
  galt die Stelle als geprüft (Beispiel im Selbsttest: `CLEAR_WIDE(w)`).
- `int_cast_check.py`: „im Makro?“ über die Schreib-Orte und die Makro-
  Expansionen von libclang statt über Textvergleiche (Umwandlungen in
  Makro-Argumenten sind jetzt umschreibbar).
- `fixed_layout_check.py`: welche Strukturen ein Umwandler nennt, über die
  Typ- und Glied-Verweise im Syntaxbaum statt über Wörter im Quelltext; auch über
  `typedef`-Namen (`ID_FILE_HEADER` heißt in der Ausgabe `_ID_FILE_HEADER`, das
  hatte die Wortsuche nie verglichen): 89 statt 83 Strukturen, alle gleich.
- `cast_check.py` meldet jetzt **jede** Stelle eines Paares (vorher nur die erste)
  und je Stelle die Glieder des Zieltyps, die das Ergebnis benutzt.
- `ptr_slot_check.py`: Bytezeiger + Abstand aus dem Layout (`RE4_OFS`) ist das
  Glied selbst, kein 4-Byte-Platz.
- **Selbsttest** `port/tools/m12/selftest/run_selftest.py`: zwei kleine Dateien
  mit je einem Beispiel jeder Fehlerart (auch in Makros und verschachtelt) und den
  geprüften Formen; jedes Werkzeug muss genau seine Beispiele finden.
  Ergebnis: **17 von 17** Prüfungen finden genau ihre Beispiele.
- **Gesamtlauf** `port/tools/m12/run_all.py` mit der Liste der geprüften Funde
  `port/tools/m12/reviewed.json` (jeder Fund mit Begründung, genau verglichen;
  neue Funde und nicht mehr gefundene Einträge machen ihn rot).

**Schritt 2 – Lücken geschlossen** (jede Form ein Werkzeug, jedes im Selbsttest):
- a) Zahlen-Abstände in Makros und Inline-Funktionen: `raw_offset_check.py` folgt
  jetzt auch Adressen als Zahl (`(u32) em + 0x320`, auch durch mehrere
  Umwandlungen), den Konstanten ganzer Summen mit dem Zeiger in der Mitte
  (`i * sizeof(T) + (u32) w + 0x2C`) und Variablen, die so eine Summe halten und
  danach weiterzählen (`base += 0x30C`); es vergisst eine Sicht, wenn die Variable
  neu belegt wird (aus Assembler übersetzter Code nutzt „Register“ mehrfach).
  Adressmasken (`- 0x80000000`) zählen nicht als Abstand.
- b) Kopien zwischen Strukturen verschiedener Form: neu `copy_view_check.py`
  (`memcpy`/`memmove`/`memcpy_asm` zwischen zwei Strukturen und zwischen Struktur
  und Bytepuffer): 51 Kopien, 0 Funde.
- c) `void*`-Arbeitsflächen mit GameCube-Abständen: neu `work_view_check.py`
  (verbindet über die Deklaration des Zeigers die Stellen, die ihn als Struktur
  lesen, mit denen, die feste Abstände addieren, über alle Dateien): 9 Zeiger,
  16 Paare, 0 Funde.
- d) Zeiger-Rechnung mit `sizeof` eines anderen Typs: neu `sizeof_ptr_check.py`
  (Füllbytes am Ende einer Grundklasse, andere Größe auf LP64).
- e) Zeigertabellen in Disc-Daten: neu `reloc_check.py` (Glieder, die das Spiel an
  Ort und Stelle von Abstand in Zeiger umrechnet, müssen 4 Bytes bleiben): 3 Funde,
  alle Arbeitsstrukturen des Programms bzw. Ausrichtungen, keine Disc-Daten.
- f) Speicher-Anforderer: `alloc_size_check.py` findet sie jetzt selbst (jede
  Funktion, die einen Parameter als Größe an einen bekannten Anforderer
  weitergibt, bis zum Fixpunkt): `MemAlloc`, `memAlloc` der Manager,
  `operator new`, `__builtin_new`, `reverb_mem_alloc`, `__AXFXAllocFunction` …;
  dazu `new u8[K]`. Keine neuen Funde.
- **Gefunden und behoben (Patch 0082), fünf echte 64-bit-Fehler**, alle mit dem
  GameCube-Abstand an einer Zahl-Adresse oder in einer Summe:
  Szenen-Bedingung „Gegner tot“ (`sce_sys.cpp`, `hp` bei 0x320 statt 0x3B8),
  Kamera-Ecken (`sce_at.cpp`, `pCamAt` bei 0x120 statt 0x1B0, mit 4 statt 8
  Bytes geschrieben), das Schiebepuzzle in r20e (Teile bei 0x10 statt 0x18,
  Felder bei 0x178 statt 0x1C8), die Zäune in r20d (`fence[i].t`), die Fallen in
  r21d (`trap[]` bei 0x30C statt 0x380). Auf ILP32 prüft der Compiler jeden
  GameCube-Wert (`RE4_OFS`).

Ergebnis der Bestandsaufnahme (`run_all.py`, armhf gegen x86_64, Stand nach
Patch 0082):

| Werkzeug | prüft | Funde | geprüft (`reviewed.json`) | neu |
|---|---|---:|---:|---:|
| `fixed_layout_check` | Strukturen der Formatumwandler (Disc-Daten) | 0 (89 Strukturen) | 0 | 0 |
| `decl_size_check` | Definition gegen `extern`-Deklaration | 0 (1351 Variablen) | 0 | 0 |
| `view_check` | Sichten gleichen Namens | 0 (798 Strukturen) | 0 | 0 |
| `proto_check` | C-Funktionen mit verschiedenen Struktur-Parametern | 0 (2112) | 0 | 0 |
| `cast_check` (C und C++) | Umwandlung zwischen Strukturen, mit benutzten Gliedern | 26 Paare | 26 | 0 |
| `raw_offset_check` | feste Abstände (Bytes, Adressen als Zahl, Summen) | 6 | 6 | 0 |
| `raw_offset_check --words` | dto. als 2/4/8-Byte-Wörter | 8 | 8 | 0 |
| `overlay_check` | Arbeitsflächen und Pool-Plätze | 0 (1564 Klassen) | 0 | 0 |
| `int_cast_check` | vorzeichenbehaftete 32-bit-Werte zu Zeigern | 0 | 0 | 0 |
| `ptr_slot_check` | 4-Byte-Plätze für Zeiger | 2 | 2 | 0 |
| `ptr_word_check` | Zeiger als 32-bit-Wort | 1 | 1 | 0 |
| `alloc_size_check` | feste Größen beim Anfordern/Löschen/Kopieren, Schrittweiten (17 Anforderer, 9 davon selbst gefunden) | 0 | 0 | 0 |
| `span_check` | Löschen/Kopieren über mehrere Glieder | 1 | 1 | 0 |
| `array_view_check` | Glied-Felder über ihr Ende | 5 | 5 | 0 |
| `sizeof_ptr_check` (neu) | Zeiger ± sizeof eines anderen Typs | 19 | 19 | 0 |
| `work_view_check` (neu) | `void*`-Arbeitsflächen mit festen Abständen | 0 (9 Zeiger, 16 Paare) | 0 | 0 |
| `copy_view_check` (neu) | Kopien zwischen Strukturen verschiedener Form | 0 (51 Kopien) | 0 | 0 |
| `reloc_check` (neu) | an Ort und Stelle umgerechnete Zeiger-Glieder | 3 | 3 | 0 |

Geprüft und richtig (Begründung je Fund in `reviewed.json`), zusammengefasst:
- `cast_check` (26 Paare): Zeiger nur weitergereicht oder in einem Feld anderen
  Typs abgelegt (`pEmCatch`, `pTen[]`, `pCart`, Rückrufwerte), das Objekt hat
  wirklich den Zieltyp, oder es werden nur Glieder der gemeinsamen Grundklasse
  gelesen (`cModel`/`cCoord`/`cUnit`); zwei absichtlich tote Zuweisungen
  (COMPILER-DIFF). Genau angesehen: `subem10_NeckHang_Ashley` schreibt
  `cPlayer::m_Work0` auf ein Objekt, das als `cSubChar` ankommt – es ist aber
  immer `pPL` (die Routine läuft über `EmCatchPLSet` beim Spieler; spielt man
  Ashley, ist sie ein `cPlayer`).
- `raw_offset_check`: Kopf der Heap-Zellen (0x20 auf beiden Breiten), `getCap`
  in r22c (gibt eine Nummer als `cObj*` zurück); `sfd_tim.c` hat seine LP64-Fassung
  (Patch 0070).
- `ptr_slot_check`: `MWPLY_OBJ::x48` (4 Füllbytes dahinter, nie gelesen), r20d
  (`st` ist immer 0). `ptr_word_check`: `pl_npc.cpp` (obere Hälfte ist 0).
  `span_check`: Licht-Werkzeug (Debug). `array_view_check`: `m_DataUnit[32]`
  (Endmarke), r203 `em[6]` (gleiche Überlagerung wie auf dem GameCube).
- `sizeof_ptr_check`: Sofdec-Puffer (`SFBUF_HN`, Platzhalter aus Patch 0070),
  Kartenbibliothek (eigene Parameter, DSP-Abfrage übersprungen).

**Schritt 3 – Vergleichslauf 32/64 bit** (`test-r100-core.ps1 -Compare`,
`whole_r100_check.sh --compare`): baut i686 und x86_64, spielt beide mit derselben
Tastenfolge und `RE4_VI_FAST` (Start r120: Vorfilm, Polizeifahrt, r100; 30.000
Bilder), schreibt je Bild den Zustands-Trace und vergleicht mit
`port/tools/trace_compare.py`: das erste abweichende Bild mit den Werten beider
Seiten, das erste Bild je Bereich, die Drift der Positionen davor, das Ende beider
Läufe, die Heaps (wenigster freier Platz, Anforderungen ohne Platz); am ersten
Unterschied je ein kurzer zweiter Lauf mit GX-Bildern des Bildes davor und des
Bildes (`RE4_GX_DUMP_AT`, Ordner `bilder-vergleich`).
- Trace je Bild neu: Zufallsgenerator (`re4_rnd_state`, Patch 0081; er läuft als
  erstes auseinander, wenn zwei Läufe `Rnd` verschieden oft rufen), Routine und HP
  des Spielers, lebende Objekte, Ereignisse, Effekt-Plätze, Modell-Infos,
  Modellteile, Lichter, Schaden-, Kollisions- und Trefferobjekte, die Bilder der
  laufenden Ereignisse, die GX-Kennzahlen des Bildes (Befehle, Zeichenaufträge,
  Ecken, Listenaufrufe, XF/BP, Texturen, Texturbytes, Kopien, TLUT, unbekannte
  Befehle), alle 60 Bilder die Heaps.
- Gleiche Rechnung auf beiden Seiten: i686 rechnet Gleitkomma jetzt immer in SSE
  (`-msse2 -mfpmath=sse`, wie ARM32 und die 64-bit-Ziele; vorher x87 mit 80-bit-
  Zwischenwerten), und im Vergleich laufen die float-Funktionen der C-Bibliothek
  beider Seiten über ihre double-Fassung (`libre4_detmath.so`, `LD_PRELOAD`; glibc
  rechnet `sinf`, `atan2f` … auf i686 anders als auf x86_64). Positionen bis 1 Einheit
  toleriert.
- **Cloud-Test ohne Disc-Daten** (`port/tools/test_movie_compare.py`, läuft in
  `whole_build.sh`, wenn i686 und x86_64 gebaut werden): der synthetische Film durch
  die CRI-Bibliothek (der größte Teil mit eigenen 64-bit-Sichten, Patch 0070) auf
  beiden Builds, je Halbbild Prüfsummen des Bildes (Y8/C4:4) und des Tons und die
  GX-Kennzahlen: **250 von 250 Halbbildern gleich**. Das Vergleichsprogramm selbst:
  `port/tools/test_trace_compare.py`, 8 künstliche Fälle (gleich, Rauschen in der
  Toleranz, Abweichung bei bekanntem Bild in Zufall/GX/Gegner/Position, Absturz,
  Speicher voll): 8 von 8 richtig.
- **Mit Disc-Daten noch nicht gelaufen** (nur lokal möglich).

**Filmtest 60 Hz (offener Punkt) geklärt:** der Teil misst in Echtzeit; war der
Rechner ausgelastet (Bau, qemu), begann ein Halbbild zu spät, und weil die CRI-Uhr
echte Zeit misst, stand ein Filmbild 1 oder 3 Halbbilder. `movie_probe.cpp` zählt
jetzt verspätete Halbbilder (mehr als ein halbes zu spät), `test_whole_movie.py`
rechnet je verspätetem Halbbild zwei ungleiche Bilder nicht an.

Geprüft (Cloud, `whole_build.sh i686 x86_64 armhf aarch64`, 18.10.2026): alle vier Ziele
0 Baufehler, 47 Stubs wie erwartet, Selbsttest, 114 Module, Laufzeit- und Kerntest (0
Fehler), Speicherkarte und `.gci`, Spielstand-Layout, Formatvergleich (5283 Werte, mit
synthetischem Disc-Ordner 7090 Werte gleich), Filmtest bestanden (i686 und x86_64 0
verspätete Halbbilder; armhf und aarch64 unter qemu 71 bzw. 91 verspätete Halbbilder,
jetzt richtig angerechnet), **Filmvergleich i686 / x86_64: 250 von 250 Halbbildern
gleich**, Selbsttest der M12-Prüfungen 17 von 17, `run_all.py` 18 Prüfungen ohne neuen
Fund. **Nicht getestet:** Android-Bau (kein NDK in der Cloud), Vergleichslauf und
`-Lp64` mit Disc-Daten, Speichern auf 64 bit nach der Regel (Ursache bestätigt), Patch 0082 im
Spiel (r20d, r20e, r21d, Szenen-Treffer, Kamera-Ziel), Gerät.

**PC-Prüfung 15:50–15:56 (Stand `7afb2d0`, -O2, Disc-Daten): drei Läufe, Auswertung:**
- `-Compare` (Start r120), `-Lp64` (r100, x86_64) und 32 bit (r100, i686): Build, alle
  Prüfungen ohne Disc-Daten, Filmvergleich 250/250 ✅; Formatvergleich 408 Dateien ✅,
  alle Filme beider Discs ✅ (beide r100-Läufe).
- **Speicherkarte auf 64 bit: ✅** Systemdatei angelegt, Datei 1 gespeichert, Neustart,
  Optionen und Spielstand geladen, r100 läuft (auch nach „Retry“); 32 bit ebenso. Die
  Regel für das Budget des Speicherbildschirms (Einheiten-Vorrat) ist damit bestätigt.
- **Absturz in allen vier Spielläufen**, immer nach dem Soft-Reset im Titel beim Vorfilm
  `demo0.sfd` (i686 und x86_64, r100 und r120). **Ursache: der Zustands-Trace, nicht das
  Spiel** (Werkzeug: die O2-Bauten desselben Stands in der Cloud, `addr2line`/`objdump`):
  die Absturzstelle liegt in `state_trace.cpp` `frame()`, in der Schleife von
  `aliveCount` (neu mit dem Vergleichslauf, Commit `9863875`); die Fehladresse ist das
  Register + 8 (i686) bzw. ein Unsinn-Zeiger (x86_64), genau der Lesezugriff auf
  `cUnit::next`. Nach dem Soft-Reset läuft der Titel mit den Managern des letzten Raums,
  deren Felder mit den Heaps zurückgegeben sind; der Vorfilm lädt in diesen Speicher, und
  die Schleife folgte einem Zeiger aus den Filmdaten. Der Stand `6a799f4` (ohne diese
  Schleife) lief dieselbe Stelle ohne Absturz.
  - Behoben (`runtime/state_trace.cpp`): die Listen werden nur über Werke des eigenen
    Feldes des Managers verfolgt (`inArray`: Adresse im Feld, auf Werk-Grenze), und
    `plr`, `n` und `evt` kommen erst ab Spielschritt 1 in den Trace (in Schritt 0 sind
    Spieler und Manager Reste des letzten Raums).
  - Test (`--runtime-test` trace, neu, `tests/game_probe.cpp re4probe_stale_list`): eine
    Liste aus zwei Werken des Feldes und dahinter dem Zeiger 0xC7D14C2B; die alte
    Schleife stürzt im Test genau wie im Bericht ab (Fehladresse 0xc7d14c33), die neue
    zählt 2. Im Schritt 0 stehen `plr`, `n`, `evt` nicht im Trace.
- **Vergleichslauf: bis zum Absturz unterscheidet sich nur die Gruppe GX-Kennzahlen**
  (6637 gemeinsame Bilder: Zufall, Spieler, Gegner, Werke, Ereignisse, Positionen, Kamera,
  Tonspuren und Raum-Flags in jedem Bild gleich, durch Vorfilm, Polizeifahrt, r100 und
  den Soft-Reset). Erste Abweichung Bild 2 (Start): 2 Zeichenaufträge, 2 Befehle und 8
  Ecken mehr auf i686. Ursache (im Code belegt, mit dem nächsten Lauf zu bestätigen):
  die Debug-Anzeige des Debug-Builds; ihre Zeitbalken (`debug.cpp` `processBarDisp`)
  schreiben die mit `OSGetTick` gemessene Zeit jedes Teils des Bildes als Zahlen
  (`%4.0f`, `%5.0f`), also je nach Tempo des Rechners mehr oder weniger Ziffern (je
  Ziffer ein Viereck); die Heap-Zahlen der Anzeige sind auf 64 bit ohnehin andere.
  - Geändert: der Vergleichslauf startet beide Seiten mit `RE4_DEBUG_HUD=0` (wie die
    App); `trace_compare.py` zeigt jetzt je Gruppe, in wie vielen Bildern sie abweicht,
    und jeden Abschnitt des Laufs (Raum, Spielschritt) mit seiner ersten Abweichung; GX-
    Bilder auch an der ersten Abweichung in einem Raum (Schritt 3). Selbsttest 9 von 9
    (neu: Abweichung nur beim Start, Raum gleich), läuft jetzt in `whole_build.sh`.
- Die GX-Bilder der beiden r100-Läufe haben gleich viele Zeichenaufträge, die Dreiecke
  weichen um 2–50 ab (z. B. Bild 652: 78779 / 78783, Karte 3244: 4286 / 4328); das passt
  zur Textanzeige des Debug-Builds (2 Dreiecke je Zeichen), ist aber erst mit dem
  Vergleichslauf ohne Debug-Anzeige entschieden.
- Geprüft (Cloud, Stand `f59b9a4`): `whole_build.sh i686 x86_64 armhf aarch64` und
  `RE4_OPT=2 whole_build.sh i686 x86_64` (wie die PC-Prüfung): 0 Baufehler, alle Tests
  bestanden (Laufzeittest mit dem neuen Fall auf allen Zielen), Filmvergleich 250/250,
  `trace_compare` 9 von 9, M12-Selbsttest 17 von 17. **Nicht getestet:** die drei Läufe
  mit Disc-Daten (lokal), Gerät.

**PC-Prüfung 16:25–16:32 (Stand `0e9e653`, -O2, Disc-Daten): drei Läufe, Auswertung:**
- **`-Lp64` (r100, x86_64): ALLES OK** – Build, Formatvergleich 408 Dateien, Spiellauf
  10.000 Bilder ohne Absturz (auch nach dem Soft-Reset durch die Titel-Filme), alle Filme
  beider Discs, Speicherkarte (speichern, Neustart, laden). **32 bit (r100, i686): ALLES
  OK**, ebenso. Die Trace-Korrektur (`inArray`) ist damit mit Disc-Daten bestätigt.
- **Vergleichslauf (Start r120, 30.000 Bilder, Debug-Anzeige aus): beide Seiten ohne
  Absturz bis zum Ende; Vorfilm, Polizeifahrt r120, r100 und Soft-Reset in jedem Bild
  gleich** – Zufall, Spieler, Gegner, Werke, Ereignisse, Positionen, Kamera, Tonspuren,
  Raum-Flags und **alle GX-Kennzahlen** (Abschnitte r120 Schritt 0–4 und r100 Schritt 1–3:
  „keine Abweichung“). Die Abweichung der GX-Kennzahlen im ersten Lauf (Bild 2) ist damit
  als Debug-Anzeige bestätigt (ohne sie gleich).
  - Einzige Abweichung: „Leon“ im Titel nach dem Soft-Reset (ab Bild 6968, Schritt 0),
    Werte wie 3297810 oder 4,6·10^18: die Position las der Trace aus dem Spieler des
    letzten Raums, dessen Speicher der Titel wieder benutzt. Kein Spielwert. Behoben im
    Trace: `pl` und die Gegnerliste erst ab Spielschritt 1 (wie `plr`, `n`, `evt`).
  - **Bilder** (GX-Nachbildung, `bilder-vergleich`, Pixel für Pixel in der Cloud
    verglichen): Bilder 61, 601, 901, 1119, 1201 (r120), 1501, 1801, 2401, 2501, 5001
    (r100) und 6968 (Titel) **auf beiden Seiten Pixel für Pixel gleich**. Bild 2 war ein
    altes Bild des ersten Laufs (der Ordner auf dem Desktop behielt es). **Bild 301
    (WARNING-Bild) verschieden**: 136.914 Pixel, höchstens 35 von 255, Helligkeit im
    Mittel gleich, keine Verschiebung; die großen Unterschiede (über 20) nur an den Kanten
    der Schrift (Zeilen 189–278), dazu schwache Wellen daneben – das Muster einer
    Verzerrung über eine indirekte Textur.
  - Ursache (im Code gefunden, mit dem nächsten Lauf zu bestätigen): `IdShimmerTrans`
    (`id_sys.cpp`, der Flimmer-Effekt der Bildschirm-Elemente) liest die Tiefe `dot`,
    die die Stärke der Verzerrung bestimmt, aus `indMtx`, bevor `indMtx` beschrieben ist
    (so schon im Original; die Effekt-Fassung in `esp_sub.cpp` nimmt die echte Tiefe des
    Effekts). Was dort auf dem Stapel liegt, hängt vom Build ab. **Patch 0083**: `indMtx`
    beginnt mit Nullen, `dot` damit 0 → 1500 (die Untergrenze, die stärkste Verzerrung),
    auf jedem Build gleich. Was der GameCube dort las, ist nicht bekannt.
- Werkzeuge erweitert:
  - Der Vergleichslauf rechnet die Bilder beider Seiten jetzt immer (die GX-Daten der
    beiden Spielläufe, bis 24 Bilder je Seite, dazu die Bilder am ersten Unterschied der
    Traces) und vergleicht sie Pixel für Pixel (`port/tools/picture_compare.py`, nur
    Standardbibliothek). Verschiedene Bilder machen den Lauf rot; für jedes zeigt
    `gx_chunk_inspect.py diff` die ersten verschiedenen GX-Befehle mit den Werten beider
    Seiten (BP-Register, XF-Werte, Ecken, Texel, Paletten; Adressen bleiben außen vor,
    die Heaps liegen auf 64 bit anders), und die GX-Daten beider Seiten kommen in
    `gx-vergleich-daten.zip`.
  - Der Ordner auf dem Desktop: was ein Lauf neu schreibt, ersetzt den gleichnamigen
    Eintrag des letzten Laufs (`copy_out`), es bleiben keine alten Bilder stehen.
  - Selbsttest `port/tools/test_compare_tools.py` (Bildvergleich PNG/PPM, GX-Vergleich:
    gleich, nur Adressen verschoben, XF-Wert, Ecke, Texel): 6 von 6; läuft in
    `whole_build.sh`.
- Geprüft (Cloud, Stand `fbeb173`): `whole_build.sh i686 x86_64 armhf aarch64` und
  `RE4_OPT=2 whole_build.sh i686 x86_64`: 0 Baufehler (Patch 0083 auf allen Zielen), alle
  Tests bestanden (Laufzeittest: im Schritt 0 weder `pl` noch Gegner im Trace), Filmvergleich
  250/250, `trace_compare` 9 von 9, Vergleichswerkzeuge 6 von 6, M12-Selbsttest 17 von 17;
  der Bildteil des Vergleichslaufs mit nachgebauten GX-Daten. **Nicht getestet:** der
  Vergleichslauf mit Disc-Daten (Bild 301), Gerät.

**PC-Prüfung 16:48 (Stand `fbeb173`, -O2, Disc-Daten), Vergleichslauf 32/64 bit: ALLES OK.**
- Beide Seiten 30.000 Bilder ab der Hauptschleife ohne Absturz (Vorfilm, Polizeifahrt
  r120, r100, Soft-Reset, Titel mit den Filmen `demo0`/`demo1`).
- Trace: **alle 30.619 Bilder gleich** (Zufall, Spieler, Gegner, Werke, Ereignisse,
  Positionen, Kamera, Tonspuren, Raum-Flags, alle GX-Kennzahlen; jeder Abschnitt von r120
  Schritt 0 bis zum Titel nach dem Soft-Reset „keine Abweichung“); keine Speicheranforderung
  ohne Platz; der wenigste freie Platz je Heap auf 64 bit im Rahmen (Heap 4: 0xC6B80 frei).
- Bilder: **24 von 24 Pixel für Pixel gleich** (Start, Warnbild, Logos, r120, r100, Titel
  nach dem Soft-Reset). **Bild 301 (WARNING) jetzt gleich: Patch 0083 bestätigt** – die
  Abweichung kam aus dem nicht initialisierten `indMtx` in `IdShimmerTrans`.
- Damit ist Schritt 3 von M12 (Vergleichslauf) erledigt; der Kern rechnet und zeichnet auf
  64 bit wie auf 32 bit, so weit der Lauf reicht (r120, r100, Titel). Noch nicht
  verglichen: andere Räume (10b), Speicherbildschirm im Vergleichslauf (die Kartenläufe
  von `-Lp64` und 32 bit sind einzeln grün).

**Gerät, 64 bit, automatischer Lauf (Retroid, 18.10.2026 16:54, Stand `8f0c425`,
`test-core-app.ps1 -Abi arm64-v8a -Seconds 480`): ✅ ohne Auffälligkeiten** (Projektinhaber:
„ohne sichtliche Auffälligkeiten“). Kern arm64-v8a mit NDK r28c gebaut (0 Fehler, 114
Module, 47 Stubs wie erwartet), App läuft als arm64-v8a (64 bit). Vorfilm `opening.sfd`
ganz (2278 Bilder, 0 übersprungen, Status 3), Polizeifahrt r120, Funkspruch, Ankunft r100,
Gegner, Zielen mit Laser, Untermenüs auf/zu; 480 s ohne Absturz; GX: 0 fehlende Texturen,
0 Shader-Fehler, 59–60 Bilder/s angezeigt; keine Speicheranforderung ohne Platz; Ton 0
verlorene Blöcke, 0 übersprungene Stimmen (53 Mal lief der Tonpuffer leer, vor allem beim
Laden; mit dem 32-bit-Kontrolllauf zu vergleichen). Bildschirmfotos (15–430 s): Logos,
Vorfilm, Polizeiauto im Nebel, Funkspruch, Haus in r100 mit Ganado. Noch offen: Lauf von
Hand (64 bit), Kontrolllauf 32 bit.

**Gerät, 64 bit, von Hand (Retroid, 18.10.2026):** Projektinhaber: „es läuft scheinbar
alles, Stage 1-1 lief problemlos“ (kein Bericht mitgeschickt). Noch offen: Kontrolllauf
32 bit (`-Abi armeabi-v7a`); Speichern/Laden auf dem Gerät mit 64 bit ausdrücklich
bestätigen. Der Release-Bau (`coreRelease`, signiert) ist noch nie gebaut worden.

**Tester (18.10.2026):** „Beim Tester läuft alles soweit.“ Sein Rat: ein anderer App-Name
wegen der Marke „Resident Evil“.

**Neuer Name (Entscheidung des Projektinhabers, 18.10.2026): CubeEvil4, Paketname
`com.ast.cubeevil4`, eigenes Symbol.**
- App-Name unter dem Symbol, Titel des Startbildschirms, Überschrift des App-Menüs
  („CubeEvil4 Options“), Name der Fehlerbericht-Datei (`cubeevil4-report-….zip`); unter dem
  Titel der Hinweis „Requires your own game discs. Not affiliated with Capcom.“ Die Texte
  des Spiels selbst (von der Disc) bleiben, wie sie sind.
- Paketname nur der Variante `core` (`applicationId 'com.ast.cubeevil4'`); das Java-Paket
  (`namespace`, die Klassen und ihre JNI-Namen) bleibt `com.ast.re4decomp`, die Variante
  `classic` bleibt `com.ast.re4decomp.classic`. Version 0.38.0 (versionCode 56).
- Symbol: eigene Zeichnung (`port/android-decomp/docs/icon/`): ein Drahtgitter-Würfel mit
  Flügeln, Seitenhaken, Spitze und langer Klinge im Stil des Kult-Zeichens, auf rotem
  Grund; ersetzt das Zeichen der Los Illuminados (`icon_illuminati.jpg`, Capcom-Grafik,
  entfernt). Das Android-TV-Banner (vorher ein Vektorbild mit „R4“) zeigt jetzt Symbol und
  Namen.
- Android sieht `com.ast.cubeevil4` als neue App: die bisherige (`com.ast.re4decomp`) bleibt
  mit ihren Daten installiert. `test-core-app.ps1` kopiert beim ersten Lauf ihre Discs und
  ihre Speicherkarte einmal in die neue App (wie zuvor von `com.ast.re4decomp.core`); Tester
  exportieren ihre Spielstände vorher im App-Menü als `.gci` und importieren die Discs neu.
  `pull-gx-dumps.ps1` und `pull-address-map.ps1` nutzen den neuen Paketnamen.
- Getestet: XML-Dateien wohlgeformt; `tools/test_host.sh`: alle Java-Tests der App
  bestanden (Discs, Steuerung, Menüs, Start, Räume …), von 42 C++-Tests 40 bestanden. Rot
  sind `enemy_feedback_tests` („Invalid source input rejected“) und
  `inventory_case_contracts` (Assertion in `tests/inventory_case_tests.cpp:23`): beides
  Module der Variante `classic`, ihr Code und ihre Tests sind seit der Übernahme der App
  (Subtree-Merge `4b2df69`) unverändert, also schon vorher rot und unabhängig von der
  Umbenennung (eigene Aufgabe). Der Bau der APK und das Gerät noch nicht (kein Android-SDK
  in der Cloud).
- Repository auf GitHub umbenannt (Projektinhaber, 18.10.2026): `Andiweli/RE4` →
  **`Andiweli/CubeEvil4`** (der alte Name leitet weiter, solange kein neues Repository „RE4“
  angelegt wird). README beginnt jetzt mit CubeEvil4 (App, Stand, Bauen), die Beschreibung
  der Decompilation folgt darunter.
- Tester-Gerät: POCO X7 Pro (Android 16) läuft (Projektinhaber, README-Änderung 18.10.2026).
- Aufräumen der Branches (Entscheidung des Projektinhabers, 18.10.2026): Meilenstein-Tags
  `m4-boot`, `m5-r100`, `m6-gx`, `m7-controls`, `m8-audio`, `m9-movies`, `m10-save-load`,
  `m11-polish` (je die Spitze des Meilenstein-Branches); gelöscht werden
  `claude/re4-gamecube-android-port-97hbjj`, `claude/re4-android-decomp-core`,
  `claude/nifty-feynman-jv729h`, `codex`, `android-port-deprecated`, `claude/re4-m12-arm64`
  (alle vollständig im Arbeitsbranch enthalten); es bleiben `master`, der Arbeitsbranch,
  `re4-standalone` und die Branches von M6 bis M10. Tags und Löschen sperrt die Zugangsschranke
  der Cloud-Sitzung (HTTP 403; nur der eigene Arbeitsbranch ist beschreibbar): der
  Projektinhaber führt es vom PC aus.
- `master` übernimmt den Arbeitsstand (Wunsch des Projektinhabers, 18.10.2026): vorgespult
  von `15cf64f` auf `121b3dd` (208 Commits, ohne Zusammenführungs-Commit, kein Verlauf
  umgeschrieben); die GitHub-Startseite zeigt seitdem die README von CubeEvil4 mit Symbol.
  Weitergearbeitet wird im Arbeitsbranch; `master` bekommt neue Stände auf Wunsch oder am Ende
  eines Meilensteins.
- Repository erneut umbenannt (Projektinhaber, 18.10.2026): `Andiweli/CubeEvil4` →
  **`Andiweli/CubeEvil4-Android`** (beide alten Namen leiten weiter). README-Link „ALPHA
  RELEASE“ zeigt auf die neue Adresse; Text des ersten Alpha-Releases:
  `port/android-decomp/docs/releases/v0.38.0-alpha1.md`.
- Tester, FPS-Aufzeichnung mit der App „Scene“ (18.10.2026 22:24, POCO X7 Pro, Dimensity 8400,
  Android 16, Bildschirm 1080×2400 mit 120 Hz, CubeEvil4 0.38.0-core): Bilder je Sekunde an den
  Bildschirm im Mittel 105,6, höchstens 121, 5 %-Tief 22, tiefster Wert 16; 86,2 % der Zeit
  mindestens 45; Grafikchip 237 s lang zu mindestens 96 % ausgelastet (höchstens 100 %);
  Temperatur höchstens 43,9 °C, Leistung im Mittel 4,4 W. Die tiefen Einbrüche (16–25) häufen
  sich im ersten Teil der Aufzeichnung, bei noch niedriger Temperatur, später nur einzelne.
  Gemessen sind die an den Bildschirm gegebenen Bilder (bis 120 je Sekunde, das Spiel selbst
  liefert 60 Halbbilder); Ursache noch offen: die Aufzeichnung zeigt weder Einstellungen noch
  Spielstelle. Die App wählt bei „Auto“ auf diesem Bildschirm die 3-fache Auflösung
  (1920×1344, wie auf dem Retroid). Angefordert: Fehlerbericht der App direkt nach einem
  solchen Lauf (GX-Werte alle 10 s), Einstellungen, Spielstelle, ein zweiter Lauf derselben
  Stelle und ein Lauf mit 2-facher Auflösung und ohne Tiefenschärfe/Kantenglättung.

**Tester-Bericht 19.10.2026 01:00 (POCO X7 Pro, Android 16, 64 bit, CubeEvil4 0.38.0-core,
`cubeevil4-report-20261019-010055.zip`; 3-fache Auflösung, 2× MSAA, 8× anisotrop, Tiefenschärfe),
Auswertung:**
- **Absturz (64 bit) am 18.10. 22:58 in r104**, gleich nach einem Schuss (R+A): SIGSEGV an
  `0xf81017988` in `cObj04::move` (+52), gerufen aus `EfmSetObj04` (Effekt-Modelle, die bei
  einem Treffer wegfliegen: Splitter, Trümmer); Register `x8 = 0x0000000f81017980`, gelesen
  wird `be_flag` des Eltern-Modells (`pMod->isAlive()`, Abstand 8 auf LP64).
- **Ursache (Absturzbericht und Code):** `EfmSetObj04` (esp_efm.cpp) schreibt Eltern-Modell
  und dessen `guid` über einen Wortzeiger: `u32* link = (u32*) &w->pMod; link[0] = (u32) parent;
  link[1] = parent->guid;`. Auf ILP32 sind das `pMod` und `Guid_pMod`; auf LP64 hat `pMod`
  8 Bytes, `link[1]` ist seine obere Hälfte. So wurde `pMod = guid << 32 | parent`
  (`0xF` und `0x81017980`, genau der Registerwert) und `Guid_pMod` blieb alt.
  **Patch 0084:** die beiden Werte über die Felder selbst, auf ILP32 gleich.
- **Lücke im Werkzeug:** `port/tools/m12/ptr_word_check.py` kannte nur den direkt
  dereferenzierten Wortzeiger (`*(u32*) &p = x`), nicht den in einer Hilfsvariablen
  gehaltenen. Es verfolgt jetzt lokale Variablen, denen ein Wortzeiger auf einen Zeiger
  zugewiesen wird, und verschobene Wortzeiger (`*((u32*) &p + 1)`); „Schreiben“ heißt nur noch
  linke Seite von `=` oder `+=` … (vorher zählte auch die linke Seite von `==` oder `+`).
  Auf dem ganzen Spielcode findet es genau die zwei Zeilen von `EfmSetObj04` neu (nach Patch
  0084: nur noch die geprüfte Stelle in pl_npc.cpp). Selbsttest um drei Beispiele und ein
  Gegenbeispiel erweitert (17 von 17).
- **Gegner nicht geladen (64 bit) am 18.10. 23:12 in r107:** „alloc[3d2080]:free[3b2fa0]
  read.cpp(725)“, „DVD: Memory allocate failed“, „readEmData(): error! em/em12.drs“,
  „EmSetFromList() Em set failed, Id = 12“: `em12.drs` braucht 0x3D2080 Bytes in Heap 4, frei
  waren 0x3B2FA0, es fehlten 0x1F0E0 (124 KB); der Raum lief ohne diesen Gegner weiter.
  Auf 64 bit belegen Werke und Vorräte mit Zeigern mehr Platz in den Heaps (Gegner-Platz
  0xF60 statt 0xDE0, Objekt-Platz 0x4B0 statt 0x3D8, Effekt-Platz 0x1A8 statt 0x150 …): im
  Vergleichslauf 16:48 hatte Heap 4 in r100 auf i686 mindestens 0x115AC0 frei, auf x86_64
  0xC6B80, 0x4EF40 (316 KB) weniger. Ob `em12` in r107 auf 32 bit passt, ist noch nicht
  gemessen (Vergleichslauf mit Start r107, siehe unten).
  **Regel `lp64_sizes` (neuer Eintrag):** das Ende der Heaps (`SystemMemInit`,
  `Heap_end_addr`) liegt auf LP64 1 MB höher, 0x818F4000 statt 0x817F4000, in der Hälfte von
  MEM1, die nur eine Entwicklungskonsole hat (die Reservierung des Ports umfasst 48 MB). Heap 0
  und über `MemReplaceHeap` der Raum-Heap 4 enden dort. Die Laufzeit legt die Dateitabelle
  der Disc (FST) entsprechend höher (`RE4_MEM1_LP64_HEAP_EXTRA`, `re4_runtime.h`); der
  Laufzeittest `tasks` prüft, dass Heap-Ende des Spiels und FST-Platz der Laufzeit
  übereinstimmen (ILP32 0x817F4000, LP64 0x818F4000). Auf 32 bit bleibt alles wie beim GameCube.
- Weitere Meldungen ohne Fehler: `cDataSwap.cpp(64)` und `datactrl.cpp(452)` ohne Platz
  (Merkmal 0): das Spiel weicht dann wie vorgesehen auf das ARAM aus bzw. lädt neu;
  `Debug_alloc` ohne Platz beim Öffnen des Debug-Menüs (`rel/tools.rel`); „ESP : num max.
  retry“ (Effekt-Vorrat voll, eine Meldung des Spiels). Ob eine davon auf 32 bit seltener ist,
  zeigt erst ein Vergleichslauf derselben Stelle.
- **Speichern und Laden auf 64 bit auf dem Gerät des Testers bestätigt** (Protokoll):
  Laden von Spielstand 6 (22:40), Speichern (22:47, „card-a.bin saved“), insgesamt 14 Mal
  gespeichert; die Karte zeigt sieben Spielstände bis r10e (2:40 Spielzeit).
- **Ruckler und Shader (Frage vom 18.10.):** die App schreibt alle 10 s eine Zeile mit dem
  längsten Bild und der Zahl der Programme (übersetzt und aus dem Zwischenspeicher). Über alle
  834 Abschnitte des Berichts: in Abschnitten mit **neu übersetzten** Shader-Programmen hatte
  **99 %** ein Bild über 25 ms (Mittelwert des längsten Bildes 66 ms, schlimmster Fall 1057 ms);
  mit Programmen **nur aus dem Zwischenspeicher** 9 % (höchstens 37 ms), ohne neue Programme
  6 %. Die Ruckler kommen also vom erstmaligen Übersetzen; der Zwischenspeicher wirkt (230
  von 979 Programmen der zweiten Sitzung kamen aus ihm). Sonst zeigt die App im Mittel
  119–120 Bilder je Sekunde, der Kern 30 Spielbilder je Sekunde ohne Warten auf die Grafik.
- App-Version 0.38.1 (versionCode 57), damit Berichte des neuen Stands erkennbar sind.
- Zweites Alpha-Release (Wunsch des Projektinhabers, 19.10.2026: auf GitHub liegt noch 0.38.0):
  Text `port/android-decomp/docs/releases/v0.38.1-alpha2.md`, APK
  `CubeEvil4-v0.38.1-alpha2.apk`, signiert mit demselben Schlüssel wie 0.38.0 (sonst lässt
  Android das Update nicht über die alte App installieren).
- Getestet (Cloud): `whole_build.sh i686 armhf x86_64 aarch64` mit Patch 0084 und der neuen
  Regel: 0 Baufehler, alle Prüfungen bestanden (Laufzeittest mit `tasks` samt Heap-Ende, Kern-
  test, Speicherkarte, Formatvergleich, Filmtest, Filmvergleich 32/64 bit 250/250,
  M12-Selbsttest 17/17); `ptr_word_check` auf dem ganzen Spielcode wie oben; alle 18
  M12-Prüfungen auf den echten Builds (`run_all.py`, 26 min): 0 neue Funde. **Nicht
  getestet:** Disc-Daten (PC-Prüfung), Gerät; dass der Absturz in r104 weg ist, zeigt erst ein
  Lauf an derselben Stelle.

**PC-Prüfung 09:25 (Stand `2b260e2`, -O2, Disc-Daten), Vergleichslauf 32/64 bit mit Start r107,
3000 Bilder: NICHT BESTANDEN, Auswertung:**
- **Speicher in r107 bestätigt:** beide Läufe ohne Absturz, `em12` auf beiden Seiten geladen
  (Brücke: Gegner 0x12 mit Teilen aus `em/em12.drs`), keine Anforderung ohne Platz (i686 0,
  x86_64 0). Heap 4, wenigster freier Platz: i686 0x62BC0 von 0xD220C0, x86_64 0xE0EE0 von
  0xE1E820. Belegt sind auf x86_64 0xD3D940 statt 0xCBF500, 0x7E440 (505 KB) mehr. Ohne das
  zusätzliche MB wäre Heap 4 0xD1E820 groß (genau die Größe auf dem POCO X7 Pro), es fehlten
  0x1F120 Bytes; der Tester-Bericht nannte 0x1F0E0 (64 Bytes Unterschied: ein anderer Weg in den
  Raum). **Ursache und Regel damit bestätigt.** Reserve auf 64 bit jetzt 0xE0EE0 (900 KB) gegen
  0x62BC0 (395 KB) auf 32 bit. Heap 12 (Untermenü) hat auf 64 bit 0x14580 frei gegen 0x25D20:
  reicht, im Blick behalten (Raum-Rundgang 10b).
- **Erste Abweichung Bild 626** (r107, Schritt 3): GX-Kennzahlen (Befehle 29314 / 29416, XF 6418 /
  6520), in 35 von 3622 Bildern; 10 von 17 Bildern Pixel für Pixel gleich, die übrigen höchstens
  1–3 von 255 in kleinen Bereichen. Die GX-Daten zeigen den Grund: XF 0x0613 (die Farbe von Licht 1;
  `gx_chunk_inspect` zeigt das Wort als Gleitkommazahl) wechselt auf i686 (0x918C82, 0x858974,
  0x8C9079, 0x91957E, 0x8F927C: dieselbe Grundfarbe mal 0,83 bis 0,91) und steht auf x86_64 ab Bild
  653 fest auf 0xA0A48B (der Grundfarbe selbst): das Licht schwankt auf 64 bit nicht.
- **Ursache (Compiler und Gegenprobe):** Lichtart 5 (`Light05_Move`, Helligkeit nach einem
  Lichtpfad) legt `LIT05_MOVE_FREE` über `cLight::work`, das die Lichtsätze des Raums in
  GameCube-Form füllen (`cLight::operator=`: `sub = w.sub`). Pfadnummer `Id` bei 0xC, `Flag` bei
  0xD, davor `cLightPath` mit zwei Zeigern; auf LP64 ist `cLightPath` 0x18 Bytes groß und `Id`
  liegt bei 0x18 (gemessen mit den Bau-Flaggen: `offsetof(LIT05_MOVE_FREE, Id)` armhf 0xC,
  x86_64 0x18). Gelesen wurde so Pfad 0 statt des Pfads aus den Raumdaten. Gegenprobe mit dem
  neuen Laufzeittest `light` (eine Lichtart 5 mit Arbeitsbytes in GameCube-Form, Pfad 0: 200,
  Pfad 1: 50, 100): ohne Korrektur auf x86_64 „DispCol.r 200“ (Pfad 0), mit Korrektur 100 auf allen
  vier Zielen.
- **Behoben** (Regel `fixed_form_structs`): `cLightPath` (`include/lightPath.h`) in GameCube-Form,
  Zeiger 4 Bytes. Ebenso `TSceAtHide` (Debug-Werkzeug `t_sce_at`: seine Sicht auf
  `SCE_AT_DATA_HIDE` in den Bereichsdaten des Raums, Rückruf-Zeiger bei 0x30).
- **Lücke im Werkzeug:** `overlay_check.py` prüfte nur, ob eine Struktur in ihren Byte-Bereich
  passt, nicht, ob ihre Form zu den Daten passt, die dort liegen. Neu `--areas` (in `run_all.py`
  als `overlay_areas`, im Selbsttest): jeder Byte-Bereich, über den das Spiel eine Struktur legt,
  die auf LP64 eine andere Form hat. 76 Bereiche: 71 eigene Arbeitsflächen der Vorratsklassen
  (`free` der Abkömmlinge von cEm, cObj, cEsp; ihr Code füllt sie Feld für Feld, Kopien prüft
  `copy_view_check`) werden nur gezählt. Von den 5 übrigen bekamen 2 Daten von der Disc
  (`cLight::work`, `SCE_AT_DATA::data` im Debug-Werkzeug), beide behoben; 3 sind reine
  Laufzeit-Arbeit und in `reviewed.json` begründet (`cCtrl::work`, `cSceSys::SceTaskOt`,
  `tagESPGEN_FREE::buff`).
- Getestet (Cloud): `whole_build.sh i686 armhf x86_64 aarch64`: 0 Baufehler, alle Prüfungen
  bestanden, Laufzeittest `light` auf allen vier Zielen; M12-Selbsttest 18 von 18; alle 19
  M12-Prüfungen auf den echten Builds (`run_all.py`, 28 min): 0 neue Funde. **Nicht
  getestet:** der Vergleichslauf r107 nach der Korrektur (zeigt, ob Bild 626 jetzt gleich ist),
  Gerät.

**PC-Prüfung 10:34 (Stand `2b9a60d`, -O2, Disc-Daten), Vergleichslauf 32/64 bit mit Start r107,
3000 Bilder: ALLES OK.** Trace: alle 3622 Bilder gleich (jeder Abschnitt von r120 Schritt 0 bis
r107 Schritt 3 „keine Abweichung“); Bilder 16 von 16 Pixel für Pixel gleich (auch 653, 773, 901,
1373, 1501, 2401, vorher verschieden); keine Anforderung ohne Platz; Heaps wie im Lauf 09:25.
**Lichtart 5 in GameCube-Form damit bestätigt.** Nur `bericht.txt` im Ausgabeordner: Bilder und
GX-Daten legt der Vergleichslauf nur bei Unterschieden ab.

**PC-Prüfung 10:38 (Stand `2b9a60d`, -O2, Disc-Daten), Vergleichslauf 32/64 bit ab r120,
30000 Bilder: ALLES OK.** Trace: alle 30.619 Bilder gleich (r120, r100, Soft-Reset, Titel mit den
Filmen; jeder Abschnitt „keine Abweichung“); Bilder 24 von 24 Pixel für Pixel gleich; keine
Anforderung ohne Platz. Heap 4 auf x86_64 jetzt mindestens 0x1C6B80 frei von 0xE32F00 (vorher
0xC6B80 von 0xD32F00: genau das zusätzliche MB), i686 unverändert 0x115AC0 von 0xD367A0. Damit
ändern weder die Regel für das Heap-Ende noch die Lichtart 5 etwas am Ablauf von r120/r100 auf
64 bit. Der Bericht bat um den Ordner `bilder-vergleich` auch, wenn alle Bilder gleich waren;
jetzt nur noch bei Unterschieden (`whole_r100_check.sh`).

**Falsche Variante gebaut (Projektinhaber, 19.10.2026):** beim Bauen von 0.38.1 zeigte die
installierte App „RE4 Android Decomp · 0.37.0“. Das ist der Startbildschirm der Variante `classic`
(eigene Spielmodule, Paket `com.ast.re4decomp.classic`, Name „RE4 Decomp“); Android Studio nimmt
ohne Auswahl die erste Variante (`classicDebug` / `classicRelease`). Für CubeEvil4 ist es
`coreRelease`. Die feste Versionsangabe „0.37.0“ im Titel von `classic` stammte noch aus M11: der
Titel lautet jetzt „RE4 Android Decomp (classic, not CubeEvil4)“ mit der echten Version
(`MainActivity.versionName()`); ungeprüft, da in der Cloud kein Android-SDK.

**Hänger im Lichterrätsel der Kirche (r117), 64 bit** (Frage des Projektinhabers, 19.10.2026; Bericht
des Testers `cubeevil4-report-20261019-010055.zip`, POCO X7 Pro):
- **Protokoll:** r117 ab 00:50:48; nach dreieinhalb Minuten öffnet der Tester das Debug-Menü
  (00:54:29) und verlässt den Raum, ab 00:58:31 ist er wieder dort. Ab 00:58:35 läuft das Spiel
  weiter (30 Spielbilder je Sekunde, „GX core“ mit 26 ms Leerlauf), aber Bild und Ton stehen: ab
  01:00:02 alle 10 s genau 222 Zeichenaufträge und 260 Uniform-Uploads je Bild, 0 Stimmen; um
  01:00:47 SELECT und der Fehlerbericht.
- **Ursache (Code und Compiler):** `r117_LightMechanismMove` (Modus 2, „Muster prüfen“) wartet mit
  `SceSleep(1)`, bis `r117_espEnd(W->esp[0])` Bit 2 im Byte 0xF9 des Lichtstrahls sieht. Das setzt
  nur esp06 (der Effekt auf einem Pfad; `ESP06_WK.Flg`, Bit 2: am Pfadende angehalten): von allen
  45 Klassen, die von `cEsp` erben, setzt sonst keine Bit 2 im Byte 1 ihrer eigenen Arbeit.
  `ESP06_WK` enthält einen Zeiger (`pPath`) und beginnt auf LP64 8-ausgerichtet bei 0x110, nicht
  am Datenende von `cEsp` (0x10C). Patch 0077 las `RE4_OWN(cEsp, u8, 0xF8) + 1` = 0x10D. Gemessen
  mit den Bau-Flaggen: `Flg` bei 0xF9 (armhf), 0x111 (x86_64, aarch64); Patch 0077 jetzt
  `RE4_OWN(cEsp, void*, 0xF8) + 1` = 0x111 (vorher 0x10D), auf ILP32 weiter 0xF9. Von 36 Klassen
  mit `m_Free` beginnen 9 auf LP64 bei 0x110 (esp03, 06, 0d, 0e, 11, 12, 16, 3f, 4e).
- **Lücke im Werkzeug:** `RE4_OWN` galt den Prüfungen als geprüfter Abstand, gleich mit welchem
  `T`. Neu `port/tools/m12/own_start_check.py` (in `run_all.py`, Selbsttest 19 von 19): jede
  `RE4_OWN(B, T, gc)`-Stelle, bei der die Klassen, die von `B` erben, ihre eigene Arbeit an mehr
  als einem Abstand beginnen (cEsp: 36 bei 0x10C, 9 bei 0x110; cEm: 54 bei 0x49C, 2 bei 0x4A0).
  5 Stellen, alle mit Begründung in `reviewed.json`: r117 (esp06, jetzt richtig), r300/r318
  (Laser-Endpunkt als Vec 8 Bytes in die Arbeit: nur esp01, esp14, esp41 haben dort einen Vec,
  alle ab 0x10C), `esp_sub.cpp` (esp1b, echter Typ der Arbeit, 0x10C), `em3e.h` (nur cEmMark,
  0x49C).
- App-Version 0.38.2 (versionCode 58).
- Getestet (Cloud): `whole_build.sh i686 armhf x86_64 aarch64`: 0 Baufehler, alle Prüfungen
  bestanden; alle 20 M12-Prüfungen auf den echten Builds (`run_all.py`, 29 min): 0 neue Funde.
  **Nicht getestet:** das Rätsel selbst (braucht das Spiel bis r117 auf einem 64-bit-Gerät).

### Start erst mit beiden Discs (Entscheidung des Projektinhabers, 17.10.2026)

Im Kernmodus startet das Spiel (Knopf „Start game“ und der automatische Start)
erst, wenn Disc 1 und Disc 2 vollständig importiert sind
(`MainActivity.gameReady()`); bis dahin ist „Start game“ ausgegraut und der
Import sichtbar. `test-core-app.ps1` wartet entsprechend auf beide Discs. Die
Variante `classic` braucht weiter nur Disc 1.

### Touch-Steuerung und 60 FPS (Prüfung und Entscheidungen des Projektinhabers, 19.10.2026)

- **Touch-Steuerung mit RetroTouch** (`Andiweli/RetroTouch`, 1.0.0-beta.4, MIT, eigene Bibliothek
  des Projektinhabers): machbar als Ebene über der Spielansicht, eingespeist als weiteres Gerät
  in `CoreInput`; automatisch nur ohne Controller (`RetroTouchControllers`), im App-Menü
  „Use Touch Controls“. Belegung: linker Stick, Steuerkreuz (Spielmenüs), `A` `B` `X` `Y` `Z`
  `L` `R` `START`, `MENU` (wie SELECT). **C-Stick über die rechte Touchfläche** (Look-Zone,
  Entscheidung des Projektinhabers): RetroTouch meldet zwei Sticks ohne Kennung
  (`onMove(x, y)`), die Look-Zone dagegen getrennt (`onLook`). Umgesetzt mit 1.0.0-beta.5
  (nächster Abschnitt).
- **60 FPS: vorerst nicht** (Entscheidung des Projektinhabers). Befund: `SetSystemVcnt(1)`
  (60 Hz) ließe das Spiel doppelt so schnell laufen; die Spiellogik rechnet in Bildern, die
  Einstellung wird nur an 7 Stellen gelesen (Tasten-Wiederholung, DVD, Filme, Ausnahme,
  Debug-Balken). Echte 60 FPS bräuchten berechnete Zwischenbilder im Renderer.

### Touch-Steuerung eingebaut (RetroTouch 1.0.0-beta.5, 19.10.2026, Cloud, Branch `claude/re4-touch-controls`)

- **Bibliothek:** `port/android-decomp/app/libs/retrotouch.aar` ohne Versionsnummer im Namen
  (Wunsch des Projektinhabers: ein Update ersetzt nur die Datei), eingebunden in
  `app/build.gradle` (`implementation files('libs/retrotouch.aar')`). beta.5 bringt Farben
  (`RetroTouchColors`), sonst die API von beta.4.
- **Aufteilung:** `CoreTouch.java` (ohne Android, Host-Test) übersetzt RetroTouchs Meldungen in den
  GameCube-Controller und hält das Standard-Layout; `TouchControls.java` legt `RetroTouchView` über
  das Bild, `CoreActivity` reicht Berührungen durch (`dispatchTouchEvent`) und teilt die
  SELECT-Logik (`select()`), `CoreInput` hat ein eigenes Gerät `TOUCH` (`touchButton`, `touchStick`,
  `touchClear`).
- **Wann sichtbar:** automatisch, solange kein Controller angeschlossen ist
  (`RetroTouchControllers.isControllerConnected()`: eingebaute, USB- und Bluetooth-Controller;
  geprüft zweimal pro Sekunde und bei jeder Geräteänderung); mit dem Schalter **„Use Touch
  Controls“** im App-Menü (neuer Abschnitt „Controls“, gespeichert in `core_input/touchControls`)
  immer. Auf dem Retroid (eingebaute Tasten) also nur mit dem Schalter.
- **Belegung:** Knöpfe `A` (grün) `B` (rot) `X` `Y` (hellgrau) `Z` (lila) `L` `R` `START` `MENU`
  (grau), Steuerkreuz (auch für die Spielmenüs), linker Stick = Control-Stick, rechte
  Bildschirmhälfte = C-Stick. `MENU` = SELECT: kurz das App-Menü, 1 s halten das Debug-Menü.
  **`L` und `R` rasten ein** (Tippen hält, nächstes Tippen lässt los, RetroTouch zeigt sie dann
  kräftiger): mit zwei Daumen lässt sich R (zielen) sonst nicht halten, während A (feuern)
  getippt wird. Eigene Entscheidung, auf Wunsch leicht umstellbar.
- **C-Stick aus der Look-Zone:** RetroTouch meldet dort nur Bewegungen, kein Aufsetzen oder
  Abheben. Der C-Stick ist darum der Weg des Fingers seit dem Aufsetzen (0,15 der kürzeren
  Bildschirmseite = voll, längere Wege schieben die Mitte mit) und springt beim Abheben zurück.
  Welcher Finger schaut, bestimmt `TouchControls`: ein Finger, der in der Look-Zone aufsetzt und
  keinen Knopf/Stick/Steuerkreuz auslöst. Ein Daumen außerhalb aller Knöpfe nimmt die
  Spielfläche selbst (`setOnTouchListener`), sonst gäbe Android die weiteren Finger dieser
  Berührung an keine Ansicht mehr. Bekommt RetroTouch später „Finger berührt/abgehoben“ für die
  Look-Zone, ersetzt das diese Bestimmung.
- **Editor:** RetroTouchs Zahnrad oben rechts (verschieben, Größe, Deckkraft, speichern); das
  Spiel steht währenddessen wie im App-Menü. Das App-Menü hat zusätzlich „Back to the game“ zum
  Schließen per Touch.
- **Geprüft (Cloud):** `tests/CoreTouchTests.java` (232 Prüfungen, in `tools/test_host.sh`):
  Knöpfe, einrastendes L/R, MENU, Sticks, C-Stick-Weg und Zurückspringen, Finger, die nicht
  schauen, Abbruch, zusammen mit einem echten Controller; das Layout nach RetroTouchs Geometrie:
  keine Touch-Flächen überlappen, alles bei 4:3, 16:9 und 20:9 im Bild, Beschriftungen höchstens
  5 Zeichen (Gegenprobe: ein verschobenes B schlägt an). Alle App-Quellen kompilieren ohne
  Fehler und Warnungen gegen die Android-15-Klassen (`org.robolectric:android-all`, nur zum
  Prüfen, nicht im Repository) und die Klassen der AAR. `CoreInputTests` weiter 69 Prüfungen.
  Die übrigen Host-Tests wie bisher; weiterhin rot nur die bekannten `enemy_feedback_tests` und
  `inventory_case_contracts` (siehe oben).
- **Gerätetest (Retroid, Projektinhaber, 19.10.2026):** Build mit der AAR gelungen, die
  Touch-Steuerung „scheint alles zu klappen“. Danach in den M12-Branch übernommen.
- **Noch nicht getestet:** Lint (läuft erst beim `coreRelease`-Build), ein Handy ohne Controller
  (automatisches Einblenden), An- und Abstecken eines Controllers während des Spiels.

### Grafik-Auslastung: Analyse (19.10.2026, Cloud, nur Code und Berichte, noch nichts gemessen)

Anlass: beim Tester (POCO X7 Pro, Mali-GPU, 3-fach, 2× MSAA, 8× anisotrop, Tiefenschärfe) war
der Grafikchip 237 s lang zu mindestens 96 % ausgelastet, 4,4 W im Mittel. Auf dem Retroid
(Adreno, Zeitabfrage vorhanden) maß die App 8–15,6 ms GPU je Spielbild (16.10., 3-fach) und
17,7–19,7 ms nach M11 (17.10., mit den neuen Grafikoptionen). Gefunden im Renderer
(`port/android-decomp/app/src/main/cpp/gx_gl.cpp`, `core_mode.cpp`):
- **MSAA über eigene Mehrfach-Puffer und Blit-Auflösung:** jede EFB-Kopie (im Bericht des
  Testers 5,45 Kopien je Bild, dazu die Kopie ins XFB) und der Autofokus (in 49 % der Bilder,
  Farbe und Tiefe über das ganze EFB) lösen den MSAA-Puffer mit `glBlitFramebuffer` auf und
  wechseln den Framebuffer. Auf Kachel-GPUs (Mali, Adreno) schreibt jeder Wechsel den ganzen
  Puffer in den Speicher und lädt ihn danach wieder: bei 3-fach (EFB 1920×1584) mit 2 Proben
  Farbe und Tiefe zusammen etwa 46 MB, je Wechsel also bis etwa 90 MB hin und zurück
  (Schätzung ohne die Kompression der Treiber). Üblich auf Mobilgeräten:
  `EXT_multisampled_render_to_texture` (die Proben bleiben im Kachelspeicher, aufgelöst wird
  beim Zurückschreiben).
- **Kein `glInvalidateFramebuffer`:** auch Inhalte, die nicht mehr gebraucht werden (Tiefe am
  Ende des Bilds, Puffer vor einem vollständigen Löschen), werden gespeichert bzw. geladen.
- **Tiefenschärfe in voller Auflösung:** zwei volle Auflösungen (Farbe, Tiefe), ein voller
  Unschärfe-Durchgang und ein volles Zurückkopieren je Bild; die Unschärfe ginge in halber
  Auflösung.
- **Darstellung mit der Bildwiederholrate des Bildschirms:** `drawGx` löscht und zeichnet das
  XFB bei jedem Vsync auf den Bildschirm (POCO: 120/s), das Spiel liefert 30 neue Bilder/s. Mit
  `Surface.setFrameRate(60)` (ab Android 11) liefen Bildschirm und Choreographer mit 60 Hz;
  der VI-Takt folgt dem Choreographer (wie auf dem Retroid mit 60 Hz).
- Weitere Posten: 8× anisotrope Filterung (4× sieht fast gleich aus), Fragment-Shader ohne
  `mediump`, das EFB wird mit 528 Zeilen angelegt, das Spiel nutzt 448 (der Autofokus löst
  alle auf).
- Nächster Schritt (vorgeschlagen): GPU-Zeit je Durchgang (Szene, Kopien, Autofokus,
  Darstellung) mit der Zeitabfrage messen, auf dem Retroid mit einer Reihe von Einstellungen;
  dann in der Reihenfolge Nutzen/Risiko umsetzen und jeden Schritt mit `gx_chunk_render`
  Pixel für Pixel prüfen.

**Grafik-Schritte a–e im eigenen Branch `claude/re4-gpu-efficiency`** (Wunsch des Projektinhabers,
19.10.2026); Schritt (a) liegt außerdem schon im M12-Branch (Commit `715d577`, nur Messung, ohne
Wirkung auf das Bild; bleibt dort, Entscheidung des Projektinhabers, 19.10.2026: kein Umschreiben
des Verlaufs). Der Messlauf auf dem Retroid folgt später.

**GPU-Schritt (a): Messlauf gebaut (19.10.2026, Cloud).** `test-core-app.ps1 -GpuBench` startet in
r100 (Leon steht) und setzt `RE4_GPU_BENCH=1`; die App (`core_mode.cpp`, `benchTick`) schaltet nach
60 s sieben Einstellungen durch (3/2/8/an wie beim Tester, dann Tiefenschärfe aus, MSAA aus,
Anisotropie aus, Auflösung 2, alles aus bei 3, wieder 3/2/8/an), je 5 s Einschwingen und 10 s
Messung, und schreibt je Schritt eine Zeile „GPU bench N/7“: GPU-Zeit je Spielbild (Zeitabfrage
um `Run`), GPU-Zeit je Darstellung (neu: eigene Zeitabfrage um `Present`), Bilder und
Darstellungen je Sekunde, CPU-Zeit des Renderers, GPU-Takt und -Last, wo das Gerät sie lesen
lässt (Adreno: `/sys/class/kgsl/kgsl-3d0`). Danach gelten wieder die Einstellungen von vorher.
Eigene Schritte: `RE4_GPU_BENCH=S/M/A/D,...`, Wartezeit `RE4_GPU_BENCH_DELAY`. Grenzen: ohne
`EXT_disjoint_timer_query` (Mali beim POCO) keine GPU-Zeit; auf Kachel-GPUs kann die Zeit der
Darstellung zu klein ausfallen (der Grafikchip zeichnet den Bildschirm erst beim Bildwechsel,
nach der Abfrage), die je Spielbild nicht (die EFB-Kopien schließen jeden Durchgang innerhalb von
`Run` ab). Getestet: Host-Bau (`gx_gl_check` 0 Fehler, neu „every present counted“, „the presents'
GPU time measured“: 0,3 ms je Darstellung mit llvmpipe, „the chunks' GPU time measured“;
`re4decomp_link_check` mit `core_mode.cpp` gebaut). Nicht getestet: der Messlauf auf dem Gerät.

**GPU-Schritt (a): erster Messlauf auf dem Retroid (19.10.2026, Projektinhaber), Auswertung.**
- Alle sieben Einstellungen hielten 30 Bilder/s und 58–60 Darstellungen/s; die Darstellung kostete
  0,3–0,6 ms (Retroid mit 60 Hz).
- **Zu früh begonnen:** Leon war erst um 16:44:37 frei („run: player free“, Beginn von Schritt 4),
  der Autofokus lief erst ab 16:44:27. Die Schritte 1–3 lagen in Ankunft und Funkspruch (135–290
  Zeichenbefehle je Bild statt etwa 490); vergleichbar sind nur die Schritte 4–7.
- **Zeitabfrage nicht plausibel** in den Schritten 4, 5 und 7 (2× MSAA mit laufendem Autofokus):
  493, 113 und 452 ms je Bild bei 30 Bildern/s; mit 1 Probe oder ohne Autofokus 20–24 ms. Die
  Ursache zeigt das Protokoll nicht (offen).
- **Last mal Takt** (unabhängig von der Zeitabfrage), Schritte 4–7 an derselben Stelle: 3×, 2× MSAA,
  anisotrop aus, Tiefenschärfe an: 87 % bei 670 MHz (≈ 583 MHz Arbeit); 3×, 2×, 8×, an: 88 % bei
  670 MHz (≈ 590); 2×, 2×, 8×, an: 68 % bei 400 MHz (≈ 272); 3×, alles aus: 72 % bei 400 MHz
  (≈ 288). Also verdoppeln MSAA und Tiefenschärfe zusammen bei 3× etwa die Arbeit (der Retroid
  braucht dann den höchsten Takt und ist zu 88 % ausgelastet); 8× anisotrop kostet nichts
  Messbares; Auflösung 2 statt 3 gut die Hälfte.
- **Folgerung für b–e:** Die Darstellung ist auf dem Retroid klein, c (60 Hz) hilft nur auf
  120-Hz-Handys. Der große Posten ist MSAA zusammen mit dem Autofokus (d, e). Wie er sich auf beide
  verteilt, zeigt der zweite Lauf. Der Posten „anisotrop 4× statt 8×“ entfällt.
- **Messlauf verbessert (Cloud):** Der erste Schritt kommt 10 s, nachdem der Kern „Leon frei“
  meldet (`re4_trace_free_at`, schon exportiert, kein Kern-Neubau; ohne die Funktion wie bisher nach
  60 s). Neue Schritte: 3/1/1/0, 3/2/1/0, 3/1/1/1, 3/2/1/1, 3/2/8/1, 2/2/8/1, 3/1/1/0 (MSAA und
  Tiefenschärfe einzeln). Jede Zeile hat zusätzlich Last mal Takt, Zeichenbefehle je Bild (gleiche
  Stelle?), eine Markierung für Zeitwerte über dem Bildabstand und, wie weit die GPU hinterherhinkt:
  ein Fence nach jedem Spielbild, nach dem nächsten werden die noch nicht erreichten gezählt
  (`GxStats::behind*`, `gx_gl.cpp FenceAfterChunk`). Das zeigt ohne die Zeitabfrage, ob die GPU
  mitkommt. `test-core-app.ps1 -GpuBench` beobachtet jetzt 330 s. Getestet: `gx_gl_check` 0 Fehler
  (neu: jedes Spielbild geprüft, nach `glFinish` 0 hinterher), `re4decomp_link_check` gebaut,
  `core_mode.cpp` ohne neue Warnungen. Nicht getestet: der zweite Lauf auf dem Gerät.

**GPU-Schritt (a): zweiter Messlauf auf dem Retroid (19.10.2026, Projektinhaber), Ergebnis.** Alle
Schritte an derselben Stelle (Leon frei, 487–498 Zeichenbefehle je Bild), 30 Bilder/s,
60 Darstellungen/s, Darstellung 0,26–0,37 ms. **Die GPU hinkte nie hinterher** (Fences: 0 Bilder
im Mittel und höchstens 0 in allen Schritten): die Zeitabfrage-Werte von 199–438 ms mit 2× MSAA
und Autofokus sind ein Fehler der Zeitabfrage, keine echte Verzögerung. Mit MSAA allein zeigte
sie sogar weniger als ohne (17,7 statt 18,5 ms): mit MSAA taugt die Zeitabfrage des Retroid nicht.
Last mal Takt (Arbeit), 3-fach:

| Schritt | MSAA | anisotrop | Tiefenschärfe | Takt | Last | Arbeit | gegenüber nichts |
|---|---|---|---|---|---|---|---|
| 1 | aus | aus | aus | 400 MHz | 65 % | 260 | |
| 7 (Wiederholung) | aus | aus | aus | 400 MHz | 68 % | 272 | (Mittel 266) |
| 3 | aus | aus | an | 400 MHz | 79 % | 316 | +19 % |
| 2 | 2× | aus | aus | 587 MHz | 79 % | 462 | +74 % |
| 4 | 2× | aus | an | 670 MHz | 85 % | 568 | +114 % |
| 5 | 2× | 8× | an | 670 MHz | 85 % | 572 | +115 % |
| 6 (2-fach) | 2× | 8× | an | 400 MHz | 72 % | 286 | +8 % |

Folgerungen: **MSAA ist der große Posten** (+74 %), mit der Tiefenschärfe zusammen mehr als die
Summe (+114 % statt +93 %: der Autofokus löst bei MSAA Farbe und Tiefe des ganzen EFB auf und
zeichnet in den Mehrfach-Puffer zurück). Die Tiefenschärfe allein kostet +19 %, 8× anisotrop
nichts Messbares, die Wiederholung (Schritt 7) liegt 5 % neben Schritt 1 (Streuung der Messung).
Reihenfolge der Schritte danach: **b** (Wegwerfen erlauben, klein), **d** (MSAA auf Handy-Art,
der große Posten; braucht `EXT_multisampled_render_to_texture2`, damit die Tiefe als Textur
erhalten bleibt: welche Erweiterungen der Retroid hat, schreibt der nächste Lauf ins Protokoll),
**e** (Tiefenschärfe in halber Auflösung), **c** (60 Hz, hilft nur auf 120-Hz-Handys).

**GPU-Schritt (b): Wegwerfen erlauben (19.10.2026, Cloud, Branch `claude/re4-gpu-efficiency`).**
Vor jedem Zeichnen, das ein Ziel ganz übermalt, darf die GPU den alten Inhalt vergessen
(`glInvalidateFramebuffer`, `gx_gl.cpp ForgetColor`): das Ziel jeder EFB-Kopie und der Kopie ins
XFB (der Bildausschnitt ist die ganze Textur), der Fokus-Texel und das Unschärfe-Bild des Autofokus
(nur der Bildbereich wird gezeichnet und nur er zurückkopiert). Kachel-GPUs laden den alten Inhalt
sonst vor dem Zeichnen in ihre Kacheln. Nicht angefasst: das EFB selbst (das Spiel zeichnet nach
einer Kopie weiter, das Löschen nach einer Kopie betrifft nur den kopierten Bereich) und der
MSAA-Puffer (das ist Schritt d). Schalter: `GxOptions::invalidate` (an), `RE4_GX_INVALIDATE=0`
schaltet es aus. **Prüfmodus „magenta“** (`GxOptions::poison`, im PC-Werkzeug
`RE4_GX_INVALIDATE_POISON=1`): statt zu vergessen wird das Ziel magenta gefüllt; ein Bild, das den
alten Inhalt noch braucht, ändert sich dann. `gx_gl_check` läuft jetzt zweimal (normal und
magenta), die PC-Prüfung (`whole_r100_check.sh`, Abschnitt 5) zeichnet alle GX-Bilder ein zweites
Mal magenta und vergleicht sie Pixel für Pixel („Wegwerfen (GPU-Schritt b) …“). Der Renderer
schreibt einmal je Lauf den Grafikchip und die für Schritt d nötigen Erweiterungen ins Protokoll
(„GPU: …“, im Gerätebericht „Grafikchip“). Messreihe des GPU-Branch: 3/1/1/0 und 3/2/8/1, jeweils
ohne und mit Wegwerfen, alles zweimal (8 Schritte; `S/M/A/D/I`). Getestet: `gx_gl_check` 0 Fehler
in beiden Durchläufen; Gegenprobe: ein absichtlich falsches Vergessen des EFB nach einer Kopie gibt
im Magenta-Durchlauf 4 Fehler; `gx_chunk_render` und `re4decomp_link_check` gebaut; der neue
Abschnitt der PC-Prüfung mit einem Ersatz-Werkzeug (gleiche Bilder: „2 von 2 Bildern gleich“,
abweichende: FEHLER). Nicht getestet: die PC-Prüfung mit echten Spielbildern, der Messlauf auf dem
Gerät.

**GPU-Schritt (b): Ergebnis (19.10.2026, Projektinhaber).** PC-Prüfung (Stand `eb0d192e`): ALLES OK,
„Wegwerfen (GPU-Schritt b), das Vergessene magenta: 19 von 19 Bildern gleich“. Messlauf auf dem
Retroid (je zweimal, Arbeit = Last mal Takt): 3-fach ohne Extras ohne Wegwerfen 260 / 246 (der zweite
bei 305 MHz), mit 275 / 280; mit den Optionen des Testers ohne 593 / 588, mit 576 / 584. Bei gleichem
Takt (400 MHz) ohne Extras mit Wegwerfen etwa 7 % mehr (Arbeit 260 → 275/280, Zeitabfrage 18,5 →
19,6/20,0 ms), mit den Optionen des Testers 2 % weniger (innerhalb der Streuung). Der Adreno 650
zeichnet die Kopien offenbar ohnehin direkt in den Speicher. **Wegwerfen ist darum aus**
(`GxOptions::invalidate = false`, `RE4_GX_INVALIDATE=1` schaltet es ein); Code und Magenta-Prüfung
bleiben (für andere GPUs, und Schritt d braucht dieselbe Art Prüfung). **Der Retroid hat die
Erweiterungen für Schritt d:** „Adreno (TM) 650, OpenGL ES 3.2 V@0746.0;
EXT_multisampled_render_to_texture yes, ..._texture2 yes, EXT_shader_framebuffer_fetch yes,
QCOM_tiled_rendering yes“.

**GPU-Schritt (d): MSAA auf den Kacheln (19.10.2026, Cloud, Branch `claude/re4-gpu-efficiency`).**
Mit `EXT_multisampled_render_to_texture` und `..._texture2` nehmen die eigenen Texturen des EFB
(Farbe und Tiefe) die Proben selbst (`glFramebufferTexture2DMultisampleEXT`, `gx_gl.cpp TiledMsaa`):
die GPU hält die Proben im Kachelspeicher und schreibt am Ende eines Durchgangs (EFB-Kopie,
Autofokus, Bildende) nur Farbe und Tiefe aufgelöst hinaus; der nächste Durchgang liest sie in alle
Proben zurück, `Resolve` hat nichts mehr zu tun. Bisher schrieb jede Kopie die eigenen
Mehrfach-Puffer hinaus, löste sie per Blit auf und las sie wieder ein. Unterschied im Bild: was vor
einer Kopie gezeichnet wurde, behält danach seine aufgelöste Farbe und Tiefe statt der Proben (Kanten,
über die später noch gezeichnet wird). Ohne die Erweiterungen oder mit `GxOptions::tiledMsaa = false`
(`RE4_GX_MSAA_TILES=0`) der alte Weg. Die App schreibt ins Protokoll „... 2 samples (of 4) on the
tiles“ bzw. „in own buffers“, die Messzeilen ebenso. Messreihe des GPU-Branch: 3/1/1/0, dann
3/2/8/1 und 3/2/1/0 je in eigenen Puffern und auf den Kacheln, zweimal, am Ende wieder 3/1/1/0
(`S/M/A/D/I/T`). **Prüfen ohne Gerät:** Mesa `zink` auf `lavapipe` (`LIBGL_ALWAYS_SOFTWARE=1
GALLIUM_DRIVER=zink`, Paket `mesa-vulkan-drivers`, neu in `whole_setup_linux.sh`) hat beide
Erweiterungen. `gx_gl_check` prüft MSAA jetzt auf den Kacheln (wo die GPU es kann) und in eigenen
Puffern; die PC-Prüfung (Abschnitt 5) zeichnet alle GX-Bilder mit 2 Proben auf zink in beiden Arten
und vergleicht sie Pixel für Pixel (Unterschiede nur an Kanten erwartet; `gx_chunk_render`:
`RE4_GX_SAMPLES`). Getestet: `gx_gl_check` 0 Fehler mit llvmpipe (dort ohne die Erweiterungen: alter
Weg) und mit zink (Kacheln: „MSAA on the tiles where asked for and the GPU can“, Kante geglättet,
Kopie und Palette, Autofokus mit MSAA); `gx_chunk_render`, `re4decomp_link_check` gebaut. Nicht
getestet: die PC-Prüfung mit echten Spielbildern, das Gerät.

**Leons Haare fehlen / Flackern (Projektinhaber, 19.10.2026, Retroid, 64 bit).** Beim GPU-Messlauf
„stellenweise“, beim Spielen von Hand **dauernd, bei jeder MSAA-Einstellung**. Die Bildschirmfotos
des Messlaufs (Leon steht) zeigen die Haare. Ohne MSAA ist der Renderer des GPU-Branch gleichwertig
zu M12 (Schritt b aus, Schritt d nur mit MSAA); der Shader-Zwischenspeicher erkennt geänderte
Quelltexte (Hash beider Shader im Dateinamen und im Kopf). Ursache **offen**. Gefunden und behoben
dabei, ohne dass es als Ursache belegt ist:
- **Fused multiply-add nur im arm64-Kern:** Teile ohne die Spiel-Flags wurden ohne
  `-ffp-contract=off` gebaut; clang fasste dort auf aarch64 `a*b+c` zu einem Befehl mit einer
  Rundung zusammen (gezählt mit `llvm-objdump`: `C_MTX44Concat` 48, `C_MTXConcat` 24,
  `C_MTXInverse` 20, `C_QUATMultiply` 12, `C_VECNormalize` 2, `C_VECDotProduct` 2, Hall-Effekte des
  Tons …). armhf (VFPv3) und x86 haben keine solchen Befehle, darum konnte der 32/64-Vergleich am PC
  es nicht sehen. Jetzt `add_compile_options(-ffp-contract=off)` für den ganzen Kern
  (`port/whole/CMakeLists.txt`); `whole_build.sh` zählt die Befehle im aarch64-Build (muss 0 sein;
  ausgenommen `Sk1Loop` und `PsSeries`, die `fmaf` absichtlich wie das `ps_madd` des Originals
  rufen). Versuch dazu (qemu-aarch64, 2 Mio. fast parallele Vektorpaare, die SDK-Ausdrücke):
  `acosf(Skalarprodukt)` wird NaN in 20,8 % mit und 18,3 % ohne Zusammenfassen, x86_64 und armhf
  gleich 18,3 %. Die Haar-Simulation (`pendulum.cpp`) überspringt NaN-Winkel
  (`ang > spdRate && …` ist dann falsch) – als Ursache der fehlenden Haare also nicht belegt.
- Getestet (`whole_build.sh`): aarch64 (qemu) alle Kern-Prüfungen bestanden, gezählt 0; i686,
  armhf (qemu) und x86_64 alle Prüfungen bestanden (0 Bauschritte fehlgeschlagen, Selbsttest,
  Module, Laufzeit- und Kerntest, Formatvergleich, Filmvergleich i686/x86_64 250 von 250 Bildern
  gleich, M12-Selbsttest 19/19, libc++ 162/162). Nicht getestet: der neue Kern auf dem Gerät.
- **Ursache gefunden (19.10.2026, R3-Aufnahme):** GPU-Schritt d im Branch
  `claude/re4-gpu-efficiency` (MSAA auf den Kacheln: der Adreno 650 verliert nach jeder EFB-Kopie
  die Tiefe); dort ist Schritt d jetzt aus, Einzelheiten im Status jenes Branch. Der M12-Branch
  hatte Schritt d nie und ist nicht betroffen.

**GPU-Schritt (d): erster Gerätelauf (19.10.2026).** Arbeit (Last mal Takt), je zweimal: Optionen des
Testers in eigenen Puffern 590 / 596, auf den Kacheln 331 / 329 (−44 %); MSAA allein 453 → 284
(−37 %); ohne Extras 259 / 273. Die Zeitabfrage ist auf den Kacheln wieder plausibel (21 ms). Die
PC-Prüfung (zink, 2 Proben) findet aber in 16 von 19 Bildern Unterschiede zu den eigenen Puffern,
in den Ereignisbildern 8000–14700 Pixel (höchstens 66 von 255), bei Leon in r100 83 Pixel
(höchstens 94). Ob das nur Kanten sind, ist ohne die Bilder offen; Schritt d bleibt im GPU-Branch.

**Leons Haare / Flackern: Ursache gefunden – Schritt d (19.10.2026, R3-Aufnahme des Projektinhabers,
32 bit, 4× MSAA auf den Kacheln).** Der Projektinhaber bestätigt: Haare fehlen und Texturen flackern
in 32 wie in 64 bit, bei 2× und 4× MSAA (im GPU-Branch beides auf den Kacheln). Die 12 Bilder der
Aufnahme:
- **Die Daten des Kerns sind richtig:** in der Cloud nachgezeichnet (llvmpipe ohne MSAA, zink 4× in
  eigenen Puffern und auf den Kacheln) hat Leon seine blonden Strähnen; die Geräte-Bilder zeigen
  eine dunkle Kappe (die Innenschicht), das Haus im Hintergrund ohne Nebel.
- **Flackern gemessen:** von Bild zu Bild ändern sich auf dem Gerät sonst 137–275 Pixel (über 24
  von 255), in Bild 2 aber 59.886 (Leon samt Waffe fehlt) und in Bild 5 154.672 von 229.376 (fast
  alles graubraun, nur die Anzeige); in der Cloud 132–255 in jedem Bild.
- **Haar-Aufträge gefunden** (jeden der 521 Aufträge einzeln weggelassen): Nr. 464 u. a., 1252
  Punkte, Textur 64×128 CMPR, Licht, 4 TEV-Stufen, Rückseiten weg (`GX_CULL_FRONT`).
- **Beleg:** `EXT_multisampled_render_to_texture` lässt die Proben nach dem Ende eines Durchgangs
  undefiniert; zink lädt sie zurück, der Adreno offenbar nicht verlässlich. Im Versuch (Renderer mit
  nachgebildetem Verlust bei jedem erneuten Binden des EFB nach einer Kopie, zink, 4× auf den Kacheln)
  weichen vom Gerätebild ab: ohne Verlust 22.254 Pixel, **Tiefe verloren (0) 14.957** (mittlerer
  Unterschied 6,4 statt 8,7; dunkle Kappe und Haus ohne Nebel wie auf dem Gerät), Tiefe 1,0 33.713,
  Farbe verloren 168.944. Der Adreno 650 behält also die Farbe und verliert die Tiefe; was das Spiel
  nach einer EFB-Kopie mit Tiefenprüfung zeichnet (Haar, Nebel, ganze Figuren), fällt aus.
- **Behoben:** Schritt d ist aus (`GxOptions::tiledMsaa = false`); `RE4_GX_MSAA_TILES=1` schaltet
  ihn für Versuche ein (die PC-Prüfung tut das für ihren Vergleich, der Messlauf je Schritt). Der
  M12-Branch hatte Schritt d nie. Ein späteres „d2“ müsste die Tiefe nach jeder Kopie selbst
  zurückschreiben (oder `QCOM_tiled_rendering` mit Erhalt der Tiefe) und das mit nachgebildetem
  Verlust prüfen. Getestet: `gx_gl_check` 0 Fehler (llvmpipe und zink), `gx_chunk_render` zeichnet
  4× ohne Schalter in eigenen Puffern, mit `RE4_GX_MSAA_TILES=1` auf den Kacheln. Nicht getestet:
  das Gerät mit MSAA in eigenen Puffern (Haare müssen wieder da sein).

**Gerätetest danach (19.10.2026, Projektinhaber, Stand `c698b263`, Kern neu gebaut mit dem
FMA-Fix, 64 bit, von Hand in r100).** PC-Prüfung vorher ALLES OK (Magenta 19/19). **Haare und Nebel
wieder richtig:** das Foto nach 70 s fällt in die Phase mit 2× MSAA in eigenen Puffern (blonde
Strähnen, Haus im Nebel), das nach 147 s ohne MSAA ebenso. Bilder je Sekunde laut Projektinhaber
und Protokoll: ohne MSAA und mit 2× 30 (der Kern 33,3 ms je Bild); **mit 4× sichtbar ruckelig,
20–25** (Kern 49,1 bzw. 62,3 ms je Bild, davon 10 ms Warten auf den Renderer; 22,0 bzw. 25,2
gezeigte Bilder/s). 4× in eigenen Puffern ist bei Auflösung 3 zu viel für den Retroid; der
schnelle Weg (d) braucht dafür eine Fassung, die die Tiefe nach jeder Kopie selbst zurückschreibt
(„d2“). Die Standard-Messreihe misst jetzt 2× und 4× allein, die Tiefenschärfe, die Optionen des
Testers und 4× bei Auflösung 2, alles in eigenen Puffern. Danach: der GPU-Branch in den M12-Stand
übernommen (alle Schritte dort aus, nur die Messung und die Prüfwerkzeuge).

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
| `include/types.h`, `include/dolphin/types.h`, `src/lib/cri/cri_xpt.h`: `s32/u32` und `Sint32/Uint32` auf LP64 = 32-bit-Typen (`exact_width_types`, M12) | Spielheader holen mit `#include "types.h"` das Original (nicht den Ersatz-Header); `long` hat auf LP64 8 Bytes |
| Alias-Namen, die von der Breite von `u32`/`size_t` abhängen: `{u32}` in `GCC295_SYMBOLS` → `"…" RE4_ASM_U32` (`m` auf ILP32, `j` auf LP64), `_Znwj/_Znaj` → `RE4_ASM_NEW(_ARRAY)` (Prelude, M12) | `FlgOnStatus__5EventUl` und `__builtin_new` passen sonst auf LP64 nicht zur Funktion; `whole_asm_check.py` kennt die `RE4_ASM_*`-Makros als Teil eines Namens |
| `operator new(unsigned …)`, eigene `extern "C"`-Deklarationen von `memcpy/strlen …` mit `unsigned int` → `__SIZE_TYPE__` (`OPERATOR_SIZE`, `LIBC_DECL`, M12) | auf LP64 ist `size_t` 8 Bytes; sonst Widerspruch zu den System-Headern |
| Füllfelder `[A - sizeof(X)]` → `[((A) > sizeof(X) ? (A) - sizeof(X) : 0)]` (`PAD_TO_SIZE`, M12) | auf LP64 ist `X` größer als der feste Platz `A`: sonst negative Feldgröße. Die festen Plätze selbst werden in M12-A3 angepasst |
| Strukturen mit fester Form (`port/whole/fixed_form.json`): Zeiger-Glieder → `RE4_P32(T)` (`re4_ptr32.h`; ILP32: `T*`, LP64: 4 Bytes `Re4Ptr32<T>`), Datei bekommt `#include "re4_ptr32.h"` (`fixed_form_structs`, M12) | Daten, die das Spiel von der Disc liest und an Ort und Stelle umbiegt (Texturen, Kollision, Kamerabereiche, Modelle, Szenen-Treffer, Raumtabelle), der Kopf des Spielstands und die Modul-Verkettung: ihre Form ist durch die Disc bzw. die Speicherkarte festgelegt. Dazu `GxWork`, `SaveDataPtrs` (Sichten solcher Daten) und `OtData`/`OtPrim` (Zeichenreihenfolge: aus dem Bildpuffer des Raums, dessen Größe für die GameCube-Form gemacht ist). Dazu `cLightPath` (am Anfang von `LIT05_MOVE_FREE` über den Arbeitsbytes der Lichtsätze des Raums; Vergleichslauf r107, 19.10.2026) und `TSceAtHide` (Debug-Werkzeug, Sicht auf `SCE_AT_DATA_HIDE`). Bricht ab, wenn ein Glied nicht umschreibbar ist |
| `TaskFunc`: Klasse statt `void*` (`re4_task_func.h`, gleiche Größe; auf LP64 4 Bytes wie auf der GameCube, M12) | GCC 2.95 wandelte Funktionszeiger still in `void*` um, modernes C++ nicht |
| Feste Plätze und Arbeitsflächen (`port/whole/lp64_sizes.json`, `lp64_sizes`, nach den Patches, M12): die Größe in `EM_WORK_SIZE`, `OBJ_WORK_SIZE`, den 57 `u8 free[0xDE0 - 0x3E0]` der Gegner, den 25 `u8 free[OBJ_WORK_SIZE - 0x328]` der Objekte und `cEspgen::Free` → `RE4_LP64_SIZE(gc, lp64)` (Prelude; ILP32 der GameCube-Wert; auf beiden Breiten mit `+ re4_lp64_size`, einem Aufzählungswert 0, an dem die Prüfwerkzeuge eine geprüfte Größe im Syntaxbaum erkennen) | auf LP64 wird `cEm` 0xC0 Bytes größer, `cObj` 0x98; die eigenen Arbeitsbereiche mit Zeigern wachsen über ihre Fläche (bis 0xAB8 statt 0xA00). LP64: Gegner-Platz 0xF60, Fläche 0xAC0; Objekt-Platz 0x4B0, Fläche 0xF0; Effekt-Fläche 0xD0; Effekt-Pool-Platz 0x1A8. Dazu Speicher mit fester GameCube-Größe und Löschen mit fester Länge (`ATTACH_CAMERA`, `MOTION_INFO`, Bewegungstabellen des Spielers, Zeigertabellen der Puzzle-Teile und der Schatten-Objekte, Debug-Listen; `alloc_size_check.py`) und das Speicherbudget des Speicherbildschirms (`cCard::getUseMemSize`, + 8 KB für die 256 Bild-Bausteine). Geprüft mit `overlay_check.py`; eine andere Anzahl Fundstellen bricht den Build ab Dazu das Ende der Heaps (`SystemMemInit`, `Heap_end_addr`): auf LP64 1 MB höher (0x818F4000), die Laufzeit legt die FST darüber (`RE4_MEM1_LP64_HEAP_EXTRA`; Laufzeittest `tasks`); Anlass: `em12` fand in r107 auf dem POCO X7 Pro keinen Platz (19.10.2026) |
| `(u16*) &pG->stage_no` → `(u16*) &pG->room_id`, `stage_prev` → `room_id_prev` (`ROOM_ID_ADDR`) | `G_ROOM_ID` und `RsfCheck(*(u16*) &pG->stage_no, …)` lesen die Raumnummer über die Adresse ihres ersten Bytes; auf little-endian liegt `stage_no` nach Patch 0016 am anderen Ende (auf der GameCube dieselbe Adresse) |
| `new (p) T()` → `new (p) T` (`PLACEMENT_VALUE_INIT`) | C++11 füllt eine Klasse ohne eigenen Konstruktor bei `T()` erst mit Nullen; der GameCube-Compiler nicht. `em.cpp` setzt `subArc` vor `new (em) cEm10()` (Absturz in `Em12Set`, 14.10.2026). Prüfung: `port/tools/whole_value_init.py` |
| Prelude: `#define _ctype_ re4_newlib_ctype_` (nach den System-Headern) | Bionic exportiert `_ctype_` als Zeiger, newlib (das Spiel, `t_movie`) als Feld; die Tabelle heißt jetzt `re4_newlib_ctype_` (`runtime/newlib_ctype.c`), Bionics Zeiger bleibt unberührt (K2) |
| Prelude: `Re4BeBytes2` | Byte-Array-Sicht `b[0]` = hohes, `b[1]` = niedriges Byte eines Halbworts, unabhängig von der Host-Reihenfolge (Patch 0016: `cItem::lv8`, `SND_RND::b`) |
| CodeWarrior-`asm { … }`-Blöcke, die nur Register umkopieren (`mr`) und keine C-Variable ändern → `/* port: register pin */;` (`whole_asm_check.drop_register_pins`, seit M9) | Registerbindungen des Decompilats (Reihenfolge der Register wie im Original), ohne Wirkung auf das Ergebnis; clang kennt die Syntax nicht. Blöcke mit echten Befehlen bleiben und brechen den Build ab (dann Patch) |
| Vorzeichenbehaftete 32-bit-Werte, die zu Zeigern werden (`port/whole/lp64_int_casts.json`, `lp64_int_casts`, zuletzt, M12): der Operand jeder gelisteten Umwandlung `(T*) x` → `RE4_ADDR32(x)` (Prelude: `(unsigned int) (x)`) | auf LP64 wird aus einem `int`/`s32` mit gesetztem Bit 31 die Adresse `0xFFFFFFFF8…` (Absturz in `primInit` auf dem Retroid, 17.10.2026: `(void*) pG->prim_cnt`); vorzeichenlos gelesen behält der Zeiger das Bitmuster von ILP32, auch bei Zahlen, die keine Adressen sind (`(void*) -1`, Aufgabenparameter). Liste von `port/tools/m12/int_cast_check.py` (libclang, 142 Stellen, dazu zwei in Makros von Hand: `EM3D_GUN_SHOT`, `R332_TASK_SET`); eine Stelle, die nicht wie gelistet gefunden wird, bricht den Build ab |
| `GXWGFifo.T = x` / `GXWGFifo->T = x` → `re4_gx_put_T(x)` (`GX_FIFO_STORE`, in `GXVert.h` als `re4_gx_put_##T`), Datei bekommt `#include "re4_gx_fifo.h"` | Die Schreibadresse `0xCC008000` des Grafikprozessors gibt es im Port nicht; der FIFO des Ports (`runtime/gx_fifo.cpp`) bekommt jeden Wert (G1) |

Compiler-Schalter: `-std=gnu++11 -Wno-c++11-narrowing -fdelayed-template-parsing
-fno-strict-aliasing -ffp-contract=off -fwrapv -fno-strict-float-cast-overflow -w`.
Auf LP64 (M12) statt `-w`: `-fms-extensions -Wno-everything -Werror=class-varargs
-Wformat`. `-fms-extensions` macht aus den 3060 Zeiger-Kürzungen
(`(u32) ptr`) Warnungen statt Fehler; das ist nur richtig, solange alles, was
das Spiel adressiert, unter 4 GB liegt (M12-A2). `-Werror=class-varargs`
bricht ab, wenn ein `Re4Ptr32`-Glied an eine Funktion mit `...` geht (dort
käme es als 4-Byte-Objekt an statt als Zeiger; Abhilfe `re4_ptr(…)`, Patch
0067).
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
| ~~0039 `card.cpp`~~ | **entfernt in M10:** `CardFirstCheck` übersprang die Speicherkarten-Prüfung beim Start, solange der Port keine Speicherkarte hatte (`RE4_CARD_CHECK=1` behielt sie); seit M10 prüft das Spiel die echte Karte, die Test-Tastenfolgen setzen `RE4_MEMCARD=none` |
| 0040 `esp_sub.cpp` | `Esp1b_SpTrans` findet die Daten von `cEsp1b` über `RE4_OWN(cEsp, Esp1bSpWork, 0xF8)` statt über `&pad_EC[0xC]`: MWCC legt den vptr ans Ende von `cEsp`, die Compiler des Ports an den Anfang; vorher las es die Punktzahl 4 Bytes daneben (Absturz im GX-Decoder auf dem Retroid nach 153 s). Auf LP64 beginnen die Daten in den Füllbytes von `cEsp` (0x10C von 0x110; bis 18.10.2026 `sizeof(cEsp)`, Hänger in r120 auf 64 bit) |
| 0041 `at_sub.cpp`, `atari.cpp`, `card.cpp` | Das Attributwort einer Kollisionsfläche (`SAT_POLY::m_Status`) wird als Wort gelesen statt als zwei Halbwörter in GameCube-Reihenfolge (vorher vertauschte Hälften: Zäune als Fallkanten, Wände ohne Kollision, Boden als Wasser); `CardID::save` liest `nPoint` als Wert statt Byte 3 |
| 0042 `src/lib/GXGeometry.c` | `GXSetCullMode` übernimmt das Ergebnis von `__rlwimi` (`hwMode = __rlwimi(...)`): MWCC fügt bei diesem Befehl in den ersten Operanden ein, die C-Fassung des Ports gibt das Ergebnis zurück und die Zeile verwarf es. Aus `GX_CULL_FRONT` (Standard aller Modelle) wurde „keine Rückseiten weglassen“: alles wurde beidseitig gezeichnet, die Doppelschichten von Leons Haar verdeckten sich gegenseitig (dunkle Flecken) |
| 0043 `title.cpp` | Das Debug-Startmenü nach „New Game“ (`titleExit`, Raumauswahl) meldet sich bei der Laufzeit an (`re4_title_room_menu`, runtime/dolphin_os.cpp): solange es offen ist, lässt die Debug-Anzeigen-Sperre der App (`RE4_DEBUG_HUD=0`) `debug_mode` mindestens 1, damit seine Zeilen (Debug-Text) zu sehen sind; `RE4_ROOM_MENU=0` überspringt es wie ein sofortiges A. Übersprungen behält der „New Game“-Ton seine Kennung: das Ende von `titleExit` wartet wie in der Verkaufsfassung, bis er ausgeklungen ist (die Debug-Fassung setzt sie vor dem Menü auf 0; mit dem Menü klang der Ton aus, während es wartete, ohne Menü schnitt der Abbau des Titels ihn ab; gemeldet vom Projektinhaber, 17.10.2026) |
| 0044 `cam_ctrl.cpp` | `CameraControl::roomInit` füllt den Platz für Kamera-Objekte (`m_Free`, Muster 9) vor `Check()` statt danach: `Check()` kann dort die Fall-Kamera bauen, deren vtable-Zeiger das Füllen sonst überschrieb; der nächste Kamera-Schnitt (`switchCamera` → `delete m_pProc`) stürzte ab (Direktstart von r100, Totenköpfe untersuchen) |
| 0045 `snd.cpp` | `SndSoftReset` wartet auf die Bestätigung des Ton-Resets durch die Abspieler im Audio-Interrupt; die Warteschleife ruft `re4_wait_interrupt()` (runtime/vi.cpp), damit der Port die Interrupts ausliefert (vorher Stillstand beim Soft-Reset nach dem Tod, schwarzer Bildschirm) |
| 0046 `include/cam_extra.h` | `~cCamera` übergibt das Objekt an `re4_keep_vtable` (runtime/dolphin_os.cpp, tut nichts), damit clang den vtable-Zeiger von `cCamera` wieder schreibt, nachdem der Destruktor einer Unterklasse das Objekt mit Muster 9 gefüllt hat; `move()` hat einen leeren Rumpf statt rein virtuell zu sein, weil clang den löschenden Destruktor einer abstrakten Klasse als Haltebefehl erzeugt. `CameraControl` löscht eine beendete Kamera ein zweites Mal (`m_pProc` bleibt gesetzt); mit GCC 2.95 harmlos, mit clang Absturz 0x0909090d (Sprung aus dem Fenster, Totenköpfe) |
| 0047 SDK `chorus.c`, `delay.c`, `reverb_hi.c`, `reverb_hi_4ch.c`, `reverb_std.c` | C für die Gekko-Assembler-Funktionen der AXFX-Effekte (DoCrossTalk, HandleReverb ×2, HandleReverbDpl2, do_src1/2); die Vorverzögerung des hohen Halls wirkt wie im Original nur einen Abtastwert; `AXFXDelayInit` gibt das Ergebnis von `AXFXDelaySettings` zurück (im Original fehlt das `return`, der Gekko lieferte es in r3) |
| 0048 CRI `uty_tmr.c`, `uty_ppc.c`, `mpv_lib.c`, `mps_lib.c`, `dct_ac.c`, `sfx_zmv.c` | Plattformteile der CRI-Quellen: Zeitbasis über `re4_cri_timer` (runtime/vi.cpp), kein Sichern der GQR (die Kerne sind C), `HID2` ohne Locked Cache, keine Big-Endian-Selbstprüfung (die Leser stellt 0051 um), Adressen der IDCT-Tabellen in C, `sscanf` wie die C-Bibliothek |
| 0049 CRI `mpv_mc.c`, `mpv_mcy.c` | Bewegungsausgleich 8 × 8 und 16 × 16 als Byte-Schleifen mit denselben Ergebnissen (Original packte Bildpunkte als Wörter in GameCube-Reihenfolge) |
| 0050 CRI `mpv_umc.c`, `dct_fsri.c`, `cftyp422_ppc.c` | Paired-Single-Assembler in C (Halbpixel-Mischen, GQR-Vorbereitung, IDCT; übersetzt von `port/tools/whole_ppc_to_c.py`), Y8/C4:4-Texturumwandlung mit der Wortreihenfolge der GameCube; die ungenutzten ARGB8888-Umwandler melden sich einmal |
| 0051 CRI `mpv_bit.h`, `mpv_emp.c`, `mpvabdec.c`, `mps_dec.c`, `mpv_vlc.c`, `sfh_main.c`, `adx_dcd.c`, `adx_dcd5.c`, `adx_tlk.c`, `adx_sjd.c`, `adx_bsc.c`, `ax_rna.c` | Lesen in GameCube-Reihenfolge (`re4_be16/32`): Bitleser von MPEG-Video und Programmstrom, Sofdec-Kopf, ADX-Kopf und Skalen; Lauf/Stufen-Tabellen in Host-Reihenfolge; AXRNA legt PCM16 big-endian ins ARAM (so liest `dsp.cpp`) und liest die Stimmposition aus ihren zwei Hälften |
| 0052 `include/mwply.h` | `mwPlyGetHdrInf`: Breite und Höhe sind auf little-endian die unteren Hälften der Wörter, die CRI schreibt |
| 0053 CRI `mpv_dec.c`, `sfd_lib.c`, `mwsfdsvr.c`, `adx_sjd.c`, `adx_rnaa.c`, `mwstm.c` | Werte, die auf dem GameCube zufällig im Register standen: Zahl übersprungener Makroblöcke, Bildrate des Sofdec-Zeitgebers, Player der Nachschub-Prüfung, Größe für den ADX-Kopf; zwei Weiterleitungen geben das Ergebnis zurück, das die Aufrufer prüfen |
| 0054 `sofdec.cpp` | Fortschritt des Films ins Log (`re4_movie_status`/`re4_movie_end`, runtime/movie_status.cpp); die vier TEV-Konstantfarben der YUV→RGB-Umrechnung (`setTevPrm`) in GameCube-Byte-Reihenfolge (`RE4_BE`), sonst vertauschte Kanäle |
| 0055 `title.cpp` | Blendfarben der Titelbilder (Logos, Titel, Demo-Filme) als Wort in GameCube-Reihenfolge (`RE4_BE`) wie Patch 0034; sonst ist `0x000000FF` (deckendes Schwarz) ein durchsichtiges Rot und die Blenden fehlen |
| 0056 SDK `synsample.c`, `synpitch.c` | SYN schreibt 32-bit-Werte des AX-Parameterblocks (Adressen, Schleife/Format, Verstärkung, Abtastraten-Verhältnis) mit der hohen Hälfte zuerst wie das Wort des GameCube (`RE4_PB_WORD`, zwei Halbwörter); vorher vertauschte Hälften: SYN-Stimmen außerhalb des ARAM und eine riesige DSP-Schätzung, AX stoppte Stimmen („ISS Voice Drop“) |
| 0057 SDK `CARDBios.c`, `CARDRdwr.c`, `__os.h` | Die CARD-Bibliothek im Port (M10): die ein- und zwei-Byte-Befehle der Karte (0x81 Interrupt an, 0x83 Status, 0x85 Hersteller, 0x89 Status löschen, 0x87/0x88) baut das SDK als u32 und schickt die ersten Bytes im Speicher, auf little-endian die niedrigen (00 00): `re4_card_cmd` legt den Befehl zuerst; die Hersteller-ID kommt höchstes Byte zuerst (`re4_card_be16`). `CARDRdwr.c`: CodeWarriors Cast als Zuweisungsziel (`(u8*) p += n`) als normale Zuweisung; `__os.h`: die `.init`-Asm-Deklarationen nur für CodeWarrior |
| 0058 `card.cpp` | `cCard::MainLoop` meldet je Bild Modus und Schritt des Speicherkarten-Bildschirms an `re4_card_screen` (runtime/memcard.cpp), das Änderungen ins Log schreibt („card: save: saving (file 1, step 5)“, Fehlermeldungen mit Code); nur Diagnose |
| 0059 `snd.cpp` | `SndRoomBgmLoad` wartet beim Raumstart, bis die Musik des Raums davor ausgeblendet ist; die Schleife ruft `re4_wait_interrupt()` (runtime/vi.cpp) wie 0045, sonst hing das Spiel bei „Retry from a continue point“ und beim Laden aus dem Pausemenü (Retroid, 17.10.2026) |
| 0060 `card.cpp` | Die Startprüfung der Karte ohne die Fragen und Meldungen des GameCube (Entscheidungen des Projektinhabers, 17.10.2026): `firstCheck10` zeigt „Systemdatei wird geladen“ nicht mehr, `createSysfile` legt eine fehlende Systemdatei ohne Frage an (wie „Ja“) und ohne „wird gespeichert“/„gespeichert“; `RE4_CARD_BOOT_MESSAGE=1` (runtime/memcard.cpp) stellt das Original wieder her. Fehlermeldungen bleiben |
| 0061 `game.cpp` | Das Debug-Menü (`DbMenuExec`) öffnet auf Wunsch der App (SELECT eine Sekunde gehalten, `re4_core_debug_menu` → `re4_debug_menu_request`) statt mit L + START (Entscheidung des Projektinhabers, 17.10.2026: L2 + START öffnete es versehentlich; SELECT ist keine GameCube-Taste). Die übrigen Bedingungen bleiben (Werbefassung: Controller 2; `DBG_TEST_MODE`) |
| 0062 `se_at.cpp` | Raumgeräusche ohne feste Wartezeit: `rnd_base + Rnd() % rnd_interval` mit `rnd_interval` 0 (r311) ergibt wie auf dem GameCube `Rnd()` (`divw` durch 0: Quotient 0, Rest = Dividend) statt eines Abbruchs auf x86 (`SIGFPE`, PC-Rundgang 17.10.2026) bzw. eines unbestimmten Rests auf ARM; `Rnd()` weiter genau einmal |
| 0063 `r404.cpp` | Startpunkt im Söldner-Raum r404: `&pos[n]` / `&rot[n]` statt `&pos + n * 12 + 0x28`, das auf die Stapel-Anordnung von CodeWarrior baut (Clang legt `rot` anders; Stillstand in `LIMIT_ANGLE`, PC-Rundgang 17.10.2026) |
| 0064 `shape.cpp` | `CalculateShape_new`: ein Kanal, der einen Eintrag **hinter** der Formen-Tabelle nennt (Anzahl im Wort vor der Tabelle), bekommt keine Werte. Das Spiel liest dort die Eckpunkt-Listen als Eintrag; big-endian ist dessen Anzahl negativ (nichts passiert), in Host-Reihenfolge 65535 (r229: Codec-Gesicht schrieb in die Zeichenliste, Funktionszeiger um 0x74 verschoben). Außerdem wird eine Eckpunkt-Nummer außerhalb des Modells übersprungen. Beides einmal gemeldet (`check: shape: …`); richtige Daten haben beides nicht |
| 0065 `filter01.cpp` | Schalter „Depth of field“ des App-Menüs (M11, Wunsch des Projektinhabers, 17.10.2026): ist er aus (`re4_dof_enabled()`, `core_api.cpp`), legt `Filter01Trans` keinen Tiefenschärfe-Durchgang an und löscht die Anfragen dieses Bilds wie nach einem gezeichneten. Standard an. Ist er an und legt das Spiel keinen Fokus an, kommt bei freiem Spieler der Autofokus der App dazu (`re4_dof_autofocus`, OT-Eintrag `re4_dof_autofocus_mark` → BP 0x5A, `runtime/dof.cpp`; kein GameCube-Effekt, Wunsch des Projektinhabers „wie in RE4-Decomp“); `re4_dof_note` schreibt die Durchgänge eines Bilds ins Protokoll, wenn sie sich ändern |
| 0066 `t_option.cpp` | Waffenseite des Debug-Menüs (`tp_pl_weapon`): der Cursor lief über 46 Plätze, `strWepName` hat aber 45 Namen und ein NULL-Ende; über den ersten Eintrag nach oben oder über den letzten nach unten zeigte die Liste den NULL-Namen, `eprintf` las Adresse 0 (GameCube: was dort liegt; Port: Absturz, Retroid 17.10.2026, `eprintf_main`, Fehleradresse 0). Der Cursor läuft jetzt über die 45 Waffen (letzte Seite 20..44); Auswahl und Stufenseite unverändert. Nicht auf dem Gerät geprüft |
| 0067 `room_jmp.cpp`, `title.cpp`, `roomdata.cpp`, `seq.c` | 64 bit (M12): `eprintf` bekommt die Namen aus den Raumtabellen (Strukturen mit fester Form) über `re4_ptr(…)` als echten Zeiger; `re4_ptr` ist auf ILP32 ohne Wirkung. `roomdata.cpp`: die Fehlermeldung beim Lesen eines Raummoduls gibt für `%s` den Dateinamen statt der ganzen Tabellenzeile (die GameCube übergab deren Adresse, ARM und x86 kopieren die Struktur). `SEQGetVolume` mit `s32` wie seine Deklaration in `seq.h` (auf LP64 sonst zwei verschiedene Typen) |
| 0068 `os.h`, `model.h`, `model.cpp`, `shape.cpp` | 64 bit (M12): Adressen, die über einen vorzeichenbehafteten 32-bit-Wert laufen, werden auf LP64 zu `0xFFFFFFFF8…` (alles, was das Spiel erreicht, hat Bit 31 gesetzt). `OS_CACHED_REGION_PREFIX`/`UNCACHED` als `unsigned` (feste OS-Variablen wie `OS_BUS_CLOCK`), `cModel::modelInit` gibt den `cModelInfo*` als `u32` zurück statt `int`, die verschobene Formtabelle (`shape.cpp`) wird als `u32` gelesen. Auf ILP32 gleiche Werte |
| 0069 `GXTexture.c` | 64 bit (M12): `GXTexObj` ist im Spiel 32 Bytes (`u32 dummy[8]`), die SDK-Innensicht `__GXTexObjInt` wurde auf LP64 durch den `userData`-Zeiger 40 Bytes groß: `GXInitTexObj` schrieb über jedes Texturobjekt hinaus. `userData` bleibt 4 Bytes (`RE4_P32`), Größenprüfungen für die vier Innensichten (`_Static_assert`) |
| 0070 CRI-Bibliothek (36 Dateien in `src/lib`, darunter `cri/mpv.h`, `cri/sfd.h`, `cri/sfx.h`) | 64 bit (M12): Sofdec-Filme und ADX-Ton auf LP64. Adressen in `Sint32`-Werten über `Uint32` zurück (36 Stellen); Teilsichten mit GameCube-Platzhaltern (ADX-Dekoder, Player `MWPLY_OBJ`, Sofdec-Handle `SFD_OBJ`, MPEG-Dekoder `MPV_OBJ` in fünf Dateien) bekommen auf LP64 erzeugte Platzhalter an den Abständen der vollen Struktur (`port/tools/m12/gen_views.py`, geprüft mit `group_check.py`); Größen- und Abstandsprüfungen (`_Static_assert`) für Handle-Schritte, Arbeitsflächen und Tabellen; feste Byte-Abstände (`sfd_tim.c` Uhrquellen-Tabelle, `sfd_mpv.c` Bildplatz, Zeitwörter der Bildinformation, die drei übersetzten Rechenkerne in `mpv_umc.c`) über die Glieder; `sfd_mem.c` führt die Puffernummer als Zahl. Jede LP64-Variante steht in `#if __SIZEOF_POINTER__ != 4` oder behält auf ILP32 denselben Wert (Kopf des Patches listet alle Punkte) |
| 0071 `include/mwply.h` | 64 bit (M12): die Spielseite des Filmbilds `MWS_FRM` folgt auf LP64 der Glieder-Liste der Bibliothek (Zeiger `tblsrc`, `usrdat`); `mwPlyGetCurFrm` schrieb sonst über `cSofdec` hinaus |
| 0072 `dbg_button.h`, `db_mod.h`, `db_mod.cpp` | 64 bit (M12): Debug-Werkzeuge. Knopf-Sicht des Sscrn-Debugmenüs mit echtem Text- und Funktionszeiger; Platzhalter des Modellbetrachters `DbModState` je Zeigerbreite mit Abstandsprüfung |
| 0073 `em2d.cpp`, `em32.cpp`, `sce_com.cpp`, `r100.cpp`, `r103.cpp`, `r307.cpp`, `r318.cpp`, `t_camera.cpp`, `t_camera_draw.cpp`, `t_esp.cpp`, `ss_pzzl.cpp` | 64 bit (M12): feste Byte-Abstände in Strukturen, deren Glieder auf LP64 wandern (`(u8*) pe + 0x378`), über `RE4_OFS(T, glied, gc)` (Prelude): der Abstand des Glieds, auf ILP32 beim Übersetzen gegen den GameCube-Wert geprüft. Kamera- und Effekt-Werkzeug über ihre Makros (`TC_TRG`, `FSTORE_AT`). **Auch 32 bit:** `ss_pzzl.cpp` las Zustand und Auswahl von Nachricht 1 an den MWCC-Abständen (vptr hinter den Feldern); im Port lagen dort `be_flag` und ein Byte von `m_selTbl`. Jetzt über `GetMesStatus(1)`/`GetSelectMessage(1)` |
| 0074 `trans.cpp`, `emwindow.h`, `em3e.h`, `TexRender.cpp`, `light_area.cpp`, `objTrolley.cpp`, `r300.cpp`, `r30c.cpp`, `r310.cpp`, `r316.cpp`, `r317.cpp`, `r318.cpp`, `db_work.cpp` | 64 bit (M12): Sichten mit GameCube-Abständen (ein Füllfeld bis zu dem Glied, das sie benennen) über ein Objekt, dessen Grundklassen auf LP64 wachsen, erreichen das Glied jetzt über `RE4_OFS(T, glied, gc)` oder `sizeof` der Grundklasse (auf ILP32 der GameCube-Wert): Skelett (`cPartsWk` → `cParts::pList`), Fenster (`EmObjWork::eff`), `EmMarkView`, Lore (`FREE_EM10::Target_dir`), Ereignis-Kamera (`Event`-Glieder statt `TexRenderEvt`), Raketenwerfer (`cObjLauncher::pRocket`), r300/r30c/r310/r316/r317/r318, Objekt-Betrachter. `GxWork` und `SaveDataPtrs` stattdessen in `fixed_form.json`. Neu im Prelude: `RE4_OWN(B, T, gc)`, der Anfang der eigenen Arbeitsfläche hinter einer Grundklasse (auf LP64 in deren Füllbytes). Gefunden mit `cast_check.py` (jetzt auch C++) |
| 0075 `event.cpp`, `ss_pzzl.cpp`, `ss_shop.cpp`, `eprintf.cpp`, `r332.cpp`, `r20d.cpp` | 64 bit (M12): Zeiger in 4-Byte-Plätzen. `EventMgr::GetRead` (Absturz beim Laden des Autofahrt-Ereignisses r120) und `Event::PFuncTbl` über eine Zeiger-Variable; die bin/tpl-Wörter von `ITEM_PIECE_INFO::model` als u32 geschrieben und gelesen; Debug-Textzeiger in 4-Byte-Plätzen (`RE4_P32`); r332-Makros und r20d mit Index statt GameCube-Byteabstand. Gefunden mit `ptr_slot_check.py` |
| 0076 `db_log.cpp` | Die Spielmeldungen (`pLog->err/warn/mes`, auf dem GameCube Bildschirm und `printf`-Konsole) gehen über `re4_game_log` ins Protokoll des Ports: jede Meldung (nach ihrem Format) die ersten 3 Male, dann jedes 1000. Mal mit Zählung. Auf Android gab es keine Konsole, Meldungen wie „GetPrimBuff() : OVERFLOW“ gingen verloren |
| 0077 `r117.cpp` | 64 bit (M12): Byte 0xF9 der Lichtstrahlen (Byte 1 der eigenen Arbeitsfläche hinter `cEsp`) über `RE4_OWN(cEsp, u8, 0xF8) + 1`; auf LP64 beginnt die Fläche bei 0x10C. Gefunden mit `raw_offset_check.py` (Abstände hinter dem Ende einer Struktur) **Berichtigt 19.10.2026:** der Lichtstrahl ist esp06, dessen Arbeit (mit Zeiger) auf LP64 bei 0x110 beginnt; gelesen wird jetzt `RE4_OWN(cEsp, void*, 0xF8) + 1` = 0x111 statt 0x10D (das Rätsel wartete sonst beim Prüfen des Musters ewig; Tester, POCO X7 Pro). Nachgezogen: `own_start_check.py` |
| 0078 `motion.cpp`, `r320.cpp` | 64 bit (M12): GameCube-Abstände in Summen mit Variablen. `MOT_HIST` (Schlüssel-Verlauf der Wurzel für `MotionGetSpeed` und `MotionGetPosition`) über `RE4_OFS(MOTION_INFO, Key_hist, 8)`: vorher schrieb `HermiteInterpolation` auf LP64 in die obere Hälfte von `pHermite_data` (Absturz des ersten Ganados in r100, Gerät und PC-Prüfung; gefunden mit einem Hardware-Haltepunkt, `runtime/watch.cpp`). `R320_EM_OFS` über `RE4_OFS(R320Work, em, 0xC)` |
| 0079 `cam_extra.cpp` | 64 bit (M12): die Kamera beim Öffnen einer Tür liest die z-Spalte von `FREE_EMDOOR::base_mat` über den Namen statt als `((f32*) em)[0x784 / 4]`. Gefunden mit `raw_offset_check.py --words` |
| 0080 `include/trans.h` | 64 bit (M12): `ModelTexInfo`, die Sicht des Zeichners auf `cModelInfo` ab `flagsDC` (`MODEL_TEX`), legt jedes Glied dorthin, wo `cModelInfo` es hat: auf LP64 4-Byte-Packung und eine Lücke aus den Abständen von `cModelInfo` (auf dem GameCube 2 Bytes, auf LP64 6), `static_assert` je Glied. Vorher las `RefractShaderSetup` auf LP64 `blendTbl` 4 Bytes zu früh (Absturz im ersten Bild von r100, PC-Prüfung 18.10.2026). Gefunden mit `cast_check.py`, seit es den Operator `&` bei libclang abfragt (vorher im Makro übersehen) |
| 0081 `main_mem.cpp`, `rnd.cpp` | Diagnose (M12), ändert das Verhalten des Spiels nicht: jede Speicheranforderung ohne Platz (`mem_alloc` mit jedem `flag`, `Debug_alloc`) ruft `re4_heap_failed` (runtime/log.cpp: „heap: no room for … in heap N (datei(zeile), flag F)“, Zähler für den Zustands-Trace); `re4_rnd_state` gibt den Zustand des Zufallsgenerators für den Trace. Anlass: der Absturz im Speicherbildschirm auf LP64 (PC-Prüfung 18.10.2026), ein zu kleines Speicherbudget (bestätigt durch das Protokoll; seit der Regel speichert und lädt die PC-Prüfung auf 64 bit); Speicherbudgets aus GameCube-Größen sind statisch kaum zu finden, der Vergleichslauf 32/64 bit zeigt jetzt jede Anforderung, die nur auf 64 bit scheitert |
| 0082 `sce_sys.cpp`, `sce_at.cpp`, `r20d.cpp`, `r20e.cpp`, `r21d.cpp` | 64 bit (M12): GameCube-Abstände an einer Adresse als Zahl oder in einer Summe mit Index (`(u32) em + 0x320`, `i * sizeof(T) + (u32) w + 0x2C`, `base += 0x30C`) über `RE4_OFS(T, glied, gc)`: Szenen-Bedingung „Gegner tot“ (`cEm::hp`, LP64 0x3B8), Kamera-Ecken (`SceAtSysWork::pCamAt`, LP64 0x1B0, jetzt zeigerbreit geschrieben: `RAW_CAM_AT`), Schiebepuzzle r20e (Teile LP64 0x18, Felder 0x1C8), Zäune r20d (`fence[i].t`), Fallen r21d (`trap[]`, LP64 0x380). Gefunden mit `raw_offset_check.py` (Adressen als Zahl, Summen) und `sizeof_ptr_check.py` (18.10.2026) |
| 0083 `id_sys.cpp` | Nicht initialisierter Lesezugriff in `IdShimmerTrans` (Flimmer-Effekt der Bildschirm-Elemente, z. B. die WARNING-Schrift): die Tiefe `dot`, die die Stärke der indirekten Verzerrung bestimmt, kam aus `indMtx`, bevor es beschrieben war (so schon im Original), also aus Resten auf dem Stapel, die je Build andere sind (Vergleichslauf 18.10.2026: Bild 301 an den Schriftkanten verschieden, alles andere gleich). `indMtx` beginnt jetzt mit Nullen: `dot` 0 → 1500 (Untergrenze), auf jedem Build gleich; was der GameCube dort las, ist nicht bekannt. Bestätigung: der nächste Vergleichslauf (Bild 301 gleich) |
| 0084 `esp_efm.cpp` | `EfmSetObj04` schrieb Eltern-Modell und `guid` über einen Wortzeiger (`u32* link = (u32*) &w->pMod; link[0] = (u32) parent; link[1] = parent->guid;`): auf LP64 landete die `guid` in der oberen Hälfte von `pMod` (8 Bytes), `Guid_pMod` blieb alt; `cObj04::move` las das Eltern-Modell dann über `guid << 32 | parent` (Tester, POCO X7 Pro, 64 bit, r104, 18.10.2026: SIGSEGV an `0xf81017988`, `x8 = 0xf81017980`). Jetzt über die Felder selbst, auf ILP32 gleich. Gefunden vom erweiterten `ptr_word_check.py` (Wortzeiger in einer Hilfsvariablen). Bestätigung: Lauf an derselben Stelle auf 64 bit |

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

Seit M8 (16.10.2026): 27 Funktionen. Seit M9 (16.10.2026): 45 Funktionen und 1
Datenobjekt. Die mitkompilierten CRI-Quellen verweisen aus totem Code auf 22
Funktionen, die es im Original nicht mehr gab (der Linker hatte sie
entfernt: `ADXF_GetFnameRangeEx`, `ADXSJD_*` 4, `ADXSTM_GetNumSctIbuf`,
`MWSFCRE_ATTACH_PICUSRBUF`, `SFD_Finish`, `SFD_GetPaStat`, `SFX_*` 9,
`mwPlyEntryFnameCore/Sub`, `mwPlyReleaseSub`, `mwPlyStartSub`) und das
Datenobjekt `sjuni_vtbl` (`sj_uni.c`, nur von toten Funktionen benutzt); dazu
OS 8, SN-PC-Host-I/O 7, PPC 8. Im Filmtest wird keiner davon aufgerufen.

Seit M10 (16.10.2026): 47 Funktionen und 1 Datenobjekt. Dazu kommen
`__CARDGetStatusEx` und `__CARDSetStatusExAsync`: `CARDNet.c` ruft sie aus
Funktionen, die das Spiel nicht nutzt (der Original-Linker hatte beide
entfernt). Deshalb liegen die CARD-Dateien in `libre4_main` (wie die CRI-Dateien),
wo die erzeugten Platzhalter gelten. `OSGetFontEncode` (Platzhalter, gibt 0
= ANSI wie eine US-Konsole) liefert `CARDInit` die Kodierung der Karte.

Die erwartete Liste steht seit M9 in `port/whole/stubs-expected.txt`;
`port/tools/whole_stub_check.py` prüft jeden Build dagegen (`whole_build.sh`
streng, `whole_build_android.sh` nur auf zusätzliche Platzhalter). Vorher
verglich der Android-Build mit dem ersten Linux-Build-Ordner unter
`port/build`; auf dem PC war das ein alter Stand von vor M8 (76
Platzhalter), deshalb brach der erste Gerätelauf von M9 mit „only stubbed on
Android“ für die 22 CRI-Namen ab, obwohl der Kern fehlerfrei gebaut war.
Ändert sich die Liste absichtlich, im selben Commit
`whole_stub_check.py BUILD/generated/link_report.json --write` ausführen und
die neuen Einträge kommentieren.

**Echter Ersatz statt Platzhalter** (`runtime/sn_runtime.cpp`): `_ctors`/`_dtors`
(leer, weil der ELF-Lader Konstruktoren selbst ausführt), `__builtin_new`/
`__builtin_delete` (GCC-2.95-Namen von `operator new/delete`),
`memset_asm`/`memclr_asm`, seit M9 `__cvt_fp2unsigned` (Kommazahl → u32 in den
CRI-Quellen, Ergebnis wie die Gekko-Umwandlung `fctiwz`, auch ab 2^31).

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
| `dolphin_os.cpp` | `OSInit`, `OSReport` (zeilenweise ins Log), `OSPanic`, Konsolentyp/Speichergröße, `OSGetTime`/`OSGetTick` (40,5 MHz, Epoche 2000, seit M10 Ortszeit wie die Uhr der Konsole), Kalenderzeit, Interrupt-Sperren, Caches (no-op); seit G1 Unterbrechungs-Rückrufe des SDK (`__OSSetInterruptHandler`, `re4_os_hw_interrupt`), Kontexte, Reset-Funktionen, Versionsnamen und die Gekko-Register `HID2`/`WPAR` als No-op |
| SDK `OSAlloc.c`, `OSArena.c` | unverändert aus `src/lib`: Heap und Arena; `main_mem.cpp` liest die Heap-Deskriptoren direkt |
| SDK `Padclamp.c` (M7) | unverändert aus `src/lib`: `PADClamp`, die Totzonen und Bereiche des Original-Controllers; `PadRead` des Spiels ruft es auf den Rohwerten von `pad.cpp` auf |
| `os_thread.cpp` | `OSThread`, `OSSemaphore`: ein Host-Thread, strikte Prioritäten, Wechsel nur in OS-Aufrufen; jeder Thread hat einen Host-Stack (`port/src/stack_context.cpp` aus dem alten Port, neu mit ucontext für Linux x86, Boost.Context-Assembler für Linux ARM wie auf Android); ein Verteiler auf dem Stack des Startprogramms (`re4_os_run`). Spielsichtbar wie im SDK: `state`, `suspend`, Priorität, Warteschlangen, Stack-Magie `0xDEADBABE` |
| `vi.cpp` | VI-Retrace alle 1/59,94 s Echtzeit, zugestellt an Interrupt-Punkten (Verteiler im Leerlauf, `VIWaitForRetrace`, Patch 0012, später lange Hintergrundarbeit); Thread-Wechsel aus dem Rückruf erst am Interrupt-Ende. `RE4_VI_FAST=1`: ohne Warten (Tests). Seit M9 `re4_cri_timer`: die Zeitbasis der CRI-Uhr (`OSGetTime`, mit `RE4_VI_FAST` die zugestellten Halbbilder), die Halbbilder folgen dem Vsync des Bildschirms der App (`re4_vi_vsync`, 60/120 Hz; `RE4_VI_VSYNC=0` aus), und `VISetBlack` gilt ab dem nächsten `VIFlush` (Bildstücke mit `RE4_GX_CHUNK_BLACK`) |
| SDK AX, MIX, SYN, SEQ, AXART, AXFX (M8) | unverändert aus `src/lib` bis auf Patch 0047 (Effekte in C); ersetzt `ax.cpp` (K2) |
| `dsp.cpp` (M8) | der DSP mit dem AX-Microcode: Befehlslisten von `AXCL.c`, Stimmen dekodieren und mischen, Aux-Busse, Ausgabe (siehe „M8 Audio“) |
| `ai.cpp` (M8) | AI: 200 Tonblöcke je Sekunde Spielzeit mit den VI-Halbbildern, Ring für die Tonausgabe der App (`re4_audio_read`), `RE4_AUDIO_WAV` |
| CRI-Middleware (M9) | unverändert aus `src/lib` bis auf die Patches 0048–0053: ADX, ADXF, SJ, SVM, Sofdec, MPEG (siehe „M9 Videos“); ersetzt `movie.cpp` (K2) |
| `movie_status.cpp` (M9) | Fortschritt jedes Films ins Log (Aufruf aus `sofdec.cpp`, Patch 0054) |
| `gx_fifo.cpp` (G1) | GX-FIFO und Register-Blöcke CP/PE/PI/MEM: Display-Listen aufnehmen, PE-Token/fertig als Unterbrechung (`re4_gx_deliver_interrupts`), Speicher wie ihn der Grafikprozessor sieht (MEM1 und Fenster für Port-Speicher, `re4_gx_physical`), EFB-Lesen antwortet „leer“ |
| `gx_decode.cpp` (G2) | Befehlsdecoder: Zustand CP/XF/BP/TLUT, Punkte, Indizes, Texturen mit Inhalts-Schlüssel, Bildpuffer-Kopien, Bildstücke mit Warteschlange für den Renderer (`re4_gx_frame.h`, `re4_core_gx_acquire/release/detach`) |
| SDK `GX*.c` (G1) | unverändert aus `src/lib` bis auf Patch 0030: die gesamte GX-Bibliothek |
| `dvd.cpp` | `DVD*` über einen Ordner mit entpackten Disc-Dateien (siehe unten); Lesen sofort, Rückruf vor der Rückkehr; aus einem Interrupt heraus (Audio-Rahmen: Nachladen der Sprach-/Musik-Streams) bleibt der Auftrag „beschäftigt“, bis der Interrupt-Rückruf zurückgekehrt ist (`os_thread.cpp re4_os_defer`), Abbruch liefert dann −3; FST der eingelegten Disc oben in MEM1, Disc-ID bei `0x80000000`; Disc-Wechsel lädt den anderen Ordner; seit M9 `DVDCancel` (Abbruch über den verzögerten Rückruf, Ergebnis „abgebrochen“) |
| `aram.cpp` | ARAM (16 MB) als Host-Puffer, `ARInit`/`ARAlloc` (Stapel ab `0x4000`), ARQ kopiert sofort und ruft den Rückruf (aus einem Interrupt heraus erst nach dessen Ende, wie `dvd.cpp`) |
| `os_link.cpp` | `OSLink`/`OSUnlink` (siehe unten) |
| `yz2_decode.cpp` + `port/src/yz2_kernel.cpp` | `yz2Decode_Decode`: der geprüfte Kern des alten Ports unverändert, ein Adapter verbindet ihn mit `yz2code.cpp` des Spiels |
| `mtx_ps.c` + `sdk_c/` | Matrix-/Vektor-/Quaternion-Mathematik (C-Fassungen des SDK) |
| `byte_order.cpp` | Byte-Tausch-Helfer für Format-Umwandler |
| `newlib_ctype.c` | newlib-Zeichentabelle `_ctype_` (heißt seit K2 `re4_newlib_ctype_`, Prelude-Regel) |
| SDK `CARD*.c` (M10) | unverändert aus `src/lib` bis auf Patch 0057: die gesamte Speicherkarten-Bibliothek, gelinkt in `libre4_main` (wie im Original Teil von `main.dol`); ersetzt `card.cpp` (M4 B: beide Schächte leer) |
| `exi.cpp` (M10) | EXI (die Schnittstelle der Speicherkarten-Schächte) in C wie `EXIBios.c`: Sperren mit Warteschlange, Auswahl, Sofort- und DMA-Übertragungen, Probe, Anstecken, Kennung; das Ende einer Übertragung und der „fertig“-Interrupt der Karte kommen an den Interrupt-Punkten (`re4_exi_deliver`: Leerlauf, `OSSleepThread`), im Interrupt-Kontext |
| `memcard.cpp` (M10) | die Karte in Schacht A: Memory Card 251 (2 MB, 8-KB-Blöcke) als Datei mit den Befehlen der echten Karte (Lesen 0x52, Seite schreiben 0xF2, Block löschen 0xF1, alles löschen 0xF4, Status, Hersteller); Status „entsperrt“ (kein DSP-Handshake); jede Änderung sofort in die Datei, 1 s danach `fsync`; SRAM (`__OSLockSram*`, Flash-ID aus dem Kartenkopf abgeleitet); `RE4_MEMCARD` (siehe „M10“); Log der Dateien auf der Karte und der Schritte des Kartenbildschirms (Patch 0058) |
| `host_file.cpp` (M10) | Dateizugriffe für Code mit den Schaltern des Spiels (Kartendatei): Öffnen samt Ordnern, Lesen/Schreiben an einer Stelle, `fsync`, Schließen; dort wirken die Umbenennungen der C-Bibliothek (64-bit-Dateistellen, Android-Fortify) |
| `card_files.cpp` (M11) | Dateien auf dem Kartenabbild für das Menü der App: Liste, Lesen, Schreiben/Ersetzen wie die Bibliothek (zwei Kopien von Verzeichnis und FAT, Blöcke wie `__CARDAllocBlock`) |
| `gci.cpp` (M11) | `.gci` für Dolphin/GameCube: Spielstände und Systemdatei Feld für Feld in GameCube-Reihenfolge und zurück (`save_layout.inc` aus den Headern, `port/tools/save_layout.py`), CRCs neu |
| `alarm.cpp` (M10) | `OSAlarm` (Zeitüberwachung der CARD-Befehle: 100 ms je Seite, 2 s je Block), fällig an den Interrupt-Punkten |
| `pad.cpp` (M4 B) | Controller: Kanal 0 angeschlossen, Kanäle 1–3 leer (ein zweiter Controller ließe den Titel die Logos überspringen). Tasten aus der Testfolge `RE4_PAD_SCRIPT` (`TASTEN@BILD` drückt 3 Bilder lang, `TASTEN@VON-BIS/ALLE` wiederholt; Tasten `A B X Y Z L R START UP DOWN LEFT RIGHT`, mit `+` verbunden; Bild = Zahl der `PADRead`-Aufrufe). Seit K2 zusätzlich `re4_pad_live` (Controller der App, sobald kein Testskript läuft) und Bildhaken `re4_add_frame_hook` (bis 4, je `PADRead`). Seit M7: Rohwerte wie die Hardware (das eigene No-op-`PADClamp` ist entfernt, siehe SDK `Padclamp.c`), `PADControlMotor` merkt den Rüttelmotor je Bild (`re4_core_rumble`) |
| `core_api.cpp` (K2) | Einbettungs-Schnittstelle `re4_core.h` für die App: Kern-Thread, Status, Controller, Dreifachpuffer der Schnappschüsse; seit M11 Anhalten für das Menü (`re4_core_pause`), Debug-Menü auf Wunsch, Kartenfunktionen nur bei stehendem Spiel und freier Karte; Testausgaben `RE4_BRIDGE_DUMP`, `RE4_BRIDGE_SNAPSHOT`, `RE4_GX_DUMP` (GX-Bildstücke als Dateien, `re4_gx_frame_io.h`) |
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
| `MAP` (`SS/cmn/map_objNN.dat`) | Räume der Karte (Sscrn `ss_map.cpp` `mapRoomNum`/`mapBinAddr`/`mapHitAddr`): u32 Anzahl (Modelle + 2), drei Wörter, Offsets; Modelle wie `BIN`, zwei Böden wie `SAT` | `formats_scene.cpp` `convertMapRoom` |
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
