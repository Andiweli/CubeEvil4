# OUYA: Machbarkeit eines Test-Builds

Stand 21.10.2026, Branch `claude/adoring-mendel-vclbu8`. Nur Bestandsaufnahme
in der Cloud: nichts gebaut, nichts auf einer OUYA geprüft. Die Angaben zum
Gerät stammen aus den bekannten technischen Daten der OUYA, nicht aus einer
Messung; was am Gerät zu prüfen ist, steht jeweils dabei.

## Kurz

Ein OUYA-Build ist **möglich, aber kein kleiner Umbau** der heutigen APK. Drei
Hürden, jede für sich größer als eine Sitzung:

1. **Android 4.1 (API 16):** Der Kernmodus verlangt heute Android 6.0
   (`minSdk 23`), der Kern wird für Android 5.0 gebaut (`android-21`), das NDK
   r28c kann Android 4.1 gar nicht mehr.
2. **Nur OpenGL ES 2.0:** Die GX-Nachbildung ist durchgehend GLES 3 (Shader
   mit Ganzzahl-Rechnung, Uniform-Blöcke, MSAA, Fences …). Für die OUYA
   braucht es einen eigenen, vereinfachten GLES-2-Weg.
3. **Leistung (größte Unbekannte):** Schon auf dem Retroid rechnet der Kern
   14–20 ms je Spielbild (von 33,3 ms). Ein Cortex-A9 mit 1,7 GHz ist grob
   geschätzt 4–6× langsamer je Kern als der große Kern des Snapdragon 865.

Vorschlag: **erst messen, dann umbauen** (Stufe O1 unten). Die Messung braucht
keinen neuen Renderer und zeigt, ob sich der Rest lohnt.

## Das Gerät

| | OUYA | Retroid Pocket Flip 2 (zum Vergleich) |
|---|---|---|
| Prozessor | Nvidia Tegra 3, 4× Cortex-A9, 1,7 GHz, NEON | Snapdragon 865 |
| Grafik | ULP GeForce, **nur OpenGL ES 2.0**; Fragment-Shader nur `mediump` (fp20); Tiefenpuffer 16 bit (`GL_NV_depth_nonlinear`); S3TC/DXT vorhanden | Adreno 650, GLES 3.2 |
| Speicher | 1 GB RAM, 8 GB Flash (frei etwa 5 GB), USB-Anschluss für Sticks | |
| System | Android 4.1.2 (API 16), Linux-Kernel 3.1.10, Dalvik (kein ART) | |
| Controller | Standard-Gamepad: O/U/Y/A, Sticks, Steuerkreuz, L1/R1, L2/R2 analog, L3/R3, Touchpad, OUYA-Taste (`KEYCODE_MENU`); **kein SELECT und kein START** | |

ADB geht über das Micro-USB-Kabel (unter Windows braucht `adb` vermutlich die
Herstellerkennung `0x2836` in `%USERPROFILE%\.android\adb_usb.ini`; am Gerät zu
prüfen).

## Was die App heute voraussetzt

| Bereich | Heute | OUYA | Aufwand |
|---|---|---|---|
| Android-Version | Flavor `core`: `minSdk 23`; `ndkVersion 28.2.13676358`; Kern mit `-DANDROID_PLATFORM=android-21` (`whole_build_android.sh`) | API 16 geht nur mit NDK **r23c** (das letzte NDK für API 16–18; r24 hob auf 19, r26 auf 21) | mittel: eigenes NDK für den OUYA-Build, Kern neu bauen, prüfen, ob der Spielkern mit dem älteren clang (LLVM 12) gleich übersetzt |
| Java-Teile | APIs ab 21–33 teils ohne Versionsabfrage: `AudioAttributes` (17×), `AudioTrack(AudioAttributes, …)`, `setBufferSizeInFrames`/`getUnderrunCount` (24), `Spatializer` (32), `Display.Mode` (23), `VibrationEffect`/`VibratorManager` (26/31), `OnBackInvokedCallback` (33), `SUPPORTED_ABIS` (21), Thema `Theme.Material` (21) | Versionsabfragen und alte Wege (Ton über `AudioTrack` mit Stream-Typ, `Vibrator.vibrate(ms)`, Thema Holo) | klein bis mittel, mechanisch; `lint` mit `minSdk 16` listet alles |
| Disc-Import, Speicherkarte | `ACTION_OPEN_DOCUMENT`, `ACTION_OPEN_DOCUMENT_TREE`, `DocumentsContract` (API 19/21) | gibt es nicht; `ACTION_GET_CONTENT` (braucht einen Dateimanager) oder ein fester Ordner, z. B. `/sdcard/CubeEvil4/` mit `READ_EXTERNAL_STORAGE` | klein |
| Laden des Kerns | 32 bit: MEM1 bei `0x80000000..0x83000000`, die Kern-Bibliotheken oberhalb von `0x80000000` (das Spiel erkennt eigene Adressen an Bit 31); Bibliotheken mit `dlopen` nach Namen | Kernel 3.1 legt Speicher auf ARM vermutlich von unten nach oben an (zu prüfen); dann landen Bibliotheken unter `0x80000000`. Der **Kernprozess** (`libre4_coreproc.so`, `host/low_memory.h`) belegt schon heute alle Lücken unter `0x83000000` vorab: das passt. Vor Android 4.3/6.0 sucht der Linker abhängige Bibliotheken nicht im App-Ordner: Bibliotheken der Reihe nach mit vollem Pfad laden | mittel: auf der OUYA immer im Kernprozess laufen |
| Gemeinsamer Speicher | `core_ipc.cpp`: `memfd_create` (Kernel 3.17) oder `ASharedMemory_create` (API 26) | beides fehlt; `/dev/ashmem` direkt (gibt es auf Android 4.1) | klein |
| Absturzbericht | `coreproc.cpp`: `process_vm_readv` (Kernel 3.2) | fehlt; der Bericht verliert nur die Stapel-Suche | keiner |
| Grafik | `glEsVersion 0x00030000` Pflicht; GX-Shader in GLSL ES 3.00 mit Ganzzahl-TEV (8-bit-Rechnung, Bitschieben, `ivec`/`uvec`), Texturkoordinaten in Festkomma (1/128 Texel), Uniform-Blöcke `std140`, `glDrawElementsBaseVertex`, Sampler-Objekte, MSAA-Renderbuffer, `glBlitFramebuffer`, Fences, `glMapBufferRange`, Programm-Binärdaten | eigener GLES-2-Weg der GX-Nachbildung (siehe unten) | **groß**: mehrere Sitzungen |
| Steuerung | Tasten und Achsen über `InputDevice`/`MotionEvent`; App-Menü auf SELECT, Pause auf START | Achsen und Tasten kommen als normales Gamepad an (am Gerät zu prüfen); SELECT/START fehlen: andere Belegung nötig (z. B. OUYA-Taste kurz = START, Touchpad oder L3+R3 = App-Menü) | klein |
| Ton | Kern mischt selbst (DSP/AX in C) | gleich; nur die Java-Ausgabe auf den alten Weg | klein |

## GLES-2-Weg der GX-Nachbildung (Stufe O2)

Was auf der OUYA nicht 1:1 geht und wie es ersatzweise ginge:

- **TEV in Gleitkomma statt Ganzzahl.** fp20 im Fragment-Shader stellt ganze
  Zahlen nur bis 8192 genau dar. Werte (0..255, Register mit 11 bit) passen,
  die Zwischenwerte der TEV-Interpolation (bis etwa 65 000) nicht: Interpolation
  normiert rechnen, Rundung der Hardware nur ungefähr. Vergleichsstufen
  (`comp16`/`comp24`) nur näherungsweise.
- **Indirekte Texturen** (Wasser, Hitzeflimmern, Verzerrungen) rechnen in
  Festkomma bis 131 072: in fp20 nicht darstellbar. Erst abschalten oder grob
  nachbilden; direkte Texturen nehmen die Koordinate unverändert aus dem
  Vertex-Shader (dort ist `highp` erlaubt).
- **Tiefe 16 bit** statt 24 bit: Flimmern bei entfernten Flächen möglich
  (`GL_NV_depth_nonlinear` hilft).
- **Uniform-Blöcke → Uniform-Felder** (XF-Block 208 `vec4`; der Grenzwert des
  Tegra 3 ist am Gerät zu prüfen), Matrix-Indizes als `float`-Attribute,
  `BaseVertex` über verschobene Attribut-Zeiger.
- **Ohne MSAA, ohne Blit, ohne Fences:** Kantenglättung, Tiefenschärfe und
  Helligkeits-Pass erst aus oder als einfache Zusatzpässe.
- **Auflösung** auf 640×480 (Spielauflösung, `-Scale 1`) festlegen.

## Leistung: was bekannt ist

Retroid, 32 bit (Status-Datei, „M11 Android-Feinschliff“, „Elfter
Gerätelauf“, zweiter Gerätelauf des Kernprozesses):

- Kern je Spielbild 33,3 ms (30 Hz wie das Original), davon 12,8–19,3 ms
  Leerlauf, also **14–20 ms Rechenzeit**. Gut zwei Drittel davon liegen in der
  Plattformschicht, vor allem im GX-Decoder (120 000–148 000 Punkte je Bild
  auf dem Prozessor umgerechnet); die Spiellogik selbst nur 4–6 %.
- GPU 14–20 ms je Bild bei etwa 1080p mit MSAA.

Grobe Schätzung für die OUYA (nicht gemessen): Kern 60–120 ms je Spielbild,
also nur 8–15 statt 30 Spielbilder je Sekunde, solange der GX-Decoder so
arbeitet wie heute. Die drei freien Kerne des Tegra 3 helfen nur, wenn
Decoder oder Ton auf eigene Threads gehen. Die GPU-Last bei 640×480 ohne MSAA
ist ganz offen.

**Gemessen (21.10.2026, Kern-Probe O1):** r100 10,6 Bilder/s (Kern 94 ms je
Bild), Dorf r101 13,7 (72 ms); gut 60 % davon die GX-Umrechnung, die Spiellogik
etwa 13 % (Status-Datei, Abschnitt „OUYA: Kern-Probe O1“). Entscheidung des
Projektinhabers: **erst die Umrechnung schneller machen (A, gleiche Ausgabe Bit
für Bit), dann in einen eigenen Thread verlegen (B, wie Dolphins „Dual Core“)**.
A ist gebaut (Cloud: −37 % für Modell-Listen, −56 % für Werte, die die CPU
einzeln schickt), auf der OUYA noch nicht gemessen.

## Vorschlag: in Stufen, erst messen

| Stufe | Inhalt | Wo | Ergebnis |
|---|---|---|---|
| **O1 Kern-Probe** | Der Kernprozess ohne Bild auf der OUYA: NDK r23c, Kern für `android-16`, `/dev/ashmem` statt `memfd`, per ADB nach `/data/local/tmp`, Disc-Ordner auf `/sdcard` oder USB-Stick, Tastenfolge wie `RE4_PAD_SCRIPT`, ohne Warten auf den Bildschirm (`RE4_VI_FAST=1`); misst Spielbilder je Sekunde mit GX-Decoder und Ton | Cloud (Code) + Windows-PC mit OUYA (Messung) | Zahl, die entscheidet: ab etwa 25 Bildern/s weiter, unter 15 eher nicht |
| O2 GLES-2-Renderer | GLES-2-Weg der GX-Nachbildung wie oben, prüfbar in der Cloud mit llvmpipe (GLES 2 erzwingen) | Cloud | Titel und r100 als Bild auf dem PC |
| O3 OUYA-APK | eigener Flavor (z. B. `ouya`: nur `armeabi-v7a`, `minSdk 16`, immer Kernprozess, eigene App-ID), Java-Versionsabfragen, Disc-Import ohne SAF, Tastenbelegung | Cloud + OUYA | erster Test auf dem Gerät |

Entscheidungen des Projektinhabers (21.10.2026): **erst die Kern-Probe O1**;
die OUYA bekommt später **eine eigene App** (die Haupt-App bleibt bei Android 6.0
und GLES 3); eine OUYA hängt per ADB am Windows-PC.

## Stufe O1: die Kern-Probe (21.10.2026, Cloud gebaut und geprüft, auf der OUYA gemessen)

**Was sie ist:** `port/whole/host/probe.cpp`, ein kleines Programm (`re4_probe`),
das den Spielkern wie der Kernprozess lädt und laufen lässt, aber ohne Bild. Es
übernimmt die Rolle der App: nimmt jedes GX-Paket des Kerns ab und kopiert es so,
wie der Kernprozess es in den gemeinsamen Speicher kopiert (`--no-copy`: nur
abnehmen), liest den Ton in Echtzeit mit 32 kHz und misst alle 5 s: Spielbilder je
Sekunde, Bildzeiten, Rechenzeit des Spiel-Threads je Bild, Leerlauf (Warten auf
das nächste Halbbild), Prozessor gesamt, GX-Menge und Kopierzeit, Ton, Speicher,
Takt jedes Prozessorkerns. Am Ende eine Zusammenfassung über die Zeit, in der Leon
frei war (ohne Zwischensequenz, ohne Menü), sonst über die Hauptschleife des
Raums: `probe-result: key=value ...`.

**Auf der OUYA:** `port/scripts/ouya-probe.ps1` (Windows, aus dem eigenen Klon
`E:\Development\Android\RE4-OUYA`, Entscheidung des Projektinhabers, 21.10.2026) baut in
WSL mit dem NDK r23c (`whole_setup_linux.sh --ouya`, `whole_build_android.sh --ouya
--work re4-ouya`: Quelltext und Build unter `~/re4-ouya`, getrennt von `~/re4-kern` der
App), legt Disc 1 als Ordner an (einmalig, `RE4-OUYA-Daten` neben dem Klon), kopiert alles
unter der ADB-Sperre nach `/data/local/tmp/re4-ouya` und misst drei Läufe mit der
Tastenfolge der PC-Prüfung: r100 in Echtzeit, r101 (Dorf) in Echtzeit, r100 ohne
Bremse (`RE4_VI_FAST=1`: so schnell der Kern kann). Bericht auf Deutsch in
`Desktop\RE4-OUYA-Probe\bericht.txt`, alles zusammen in `senden.zip`. Mit
`-Serial` läuft dieselbe Probe auch auf dem Retroid (Vergleich).

**Was Android 4.1 anders macht** (Quelltext des Linkers von Android 4.1.2,
`bionic/linker/linker.c`, und die Symbollisten von Bionic nachgelesen):

| Punkt | Android 4.1 | Lösung |
|---|---|---|
| Hash-Tabelle | nur `DT_HASH` (ohne stürzt die Suche mit Division durch 0 ab) | `-Wl,--hash-style=both` für API < 23 (`CMakeLists.txt`), `whole_build_android.sh --ouya` prüft jede Datei |
| Symbolsuche | erst die Bibliothek selbst, dann ihre direkten `DT_NEEDED`; `dlsym(handle)` nur in der Bibliothek selbst | Probe sucht in `libre4_main.so` und `libre4_platform.so`; Module brauchen beide direkt (geprüft); nur `re4_strstr`/`re4_strrchr` stehen doppelt in beiden (harmlos) |
| Bibliotheken je Prozess | höchstens 128 (`SO_MAX`) | Probe: etwa 10 + geladene Module; in einer App mit Zygote (60–80 Systembibliotheken) zu knapp: auf der OUYA immer Kernprozess |
| Suchpfad | `LD_LIBRARY_PATH`, dann `/system/lib`; schon geladene Bibliotheken nach Dateiname | Probe lädt `libc++_shared`, Plattform und Kern mit Pfad; das Skript setzt `LD_LIBRARY_PATH` |
| Adressen | Kernel 3.1 (vermutlich, die Adresskarte im Log zeigt es): positionsunabhängige Programme knapp unter `0x80000000` (2/3 des Adressraums), der Heap zufällig bis 32 MB dahinter, also womöglich in MEM1 | Probe startet sich einmal ohne Adress-Zufall neu (`ADDR_NO_RANDOMIZE`) und belegt alles unter `0x83000000` vorab (`host/low_memory.h`); die Adresskarte steht im Log |
| fehlende Funktionen (API 21) | `fegetenv`/`fesetenv` (ARM), `dl_iterate_phdr` (ARM), `__libc_current_sigrtmin` | `port/src/stack_context.cpp` sichert das FPSCR selbst (`vmrs`/`vmsr`), `os_thread.cpp` findet die Spielbibliothek über `dladdr` und ihre Programmköpfe, `watch.cpp` nimmt Signal 32 + 6 (nur Diagnose) |
| gemeinsamer Speicher | weder `memfd_create` (Kernel 3.17) noch `ASharedMemory` (API 26) | für O1 nicht nötig; für die OUYA-App (O3) `/dev/ashmem` |

Der Abgleich der Bibliotheken des armhf-Builds gegen die Bionic-Listen
(Android 11, mit API-Stufen) fand sonst nur glibc-eigene Namen (`__errno_location`,
`fopen64`, `backtrace`, …), die der Android-Build nicht benutzt. Fehlende
Funktionen im Spielcode würde `whole_stub_check.py --android` als neue Platzhalter
melden.

**Geprüft (Cloud):** `whole_build.sh i686 armhf` grün, dazu die Probe mit dem
Test-Einstieg des Kernprozess-Tests auf i686 und armhf (qemu): Bilder gezählt,
GX-Pakete abgenommen und kopiert, Ton gelesen, Zusammenfassung; die neuen
Android-4.1-Zweige mit `-D__ANDROID__ -D__ANDROID_API__=16` für ARM übersetzt
(FPSCR-Befehle im Objekt); `ouya-probe.ps1` mit PowerShell 7 auf Syntax geprüft
und mit einem nachgebauten `adb` (eine „OUYA“ neben einem „Retroid“) durchgespielt:
OUYA erkannt, Befehle, Bericht, `senden.zip`.

**Nicht geprüft (Stand des ersten Entwurfs):** der Build mit dem NDK r23c (in
der Cloud gesperrt: `dl.google.com`), Windows PowerShell 5.1, alles auf der OUYA.
Inzwischen auf dem Windows-PC gebaut und auf der OUYA gemessen (Status-Datei).
