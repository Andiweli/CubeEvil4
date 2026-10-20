# 32 bit: der Spielkern als eigener Prozess – Übergabe (20.10.2026)

**Entscheidung des Projektinhabers (20.10.2026): Weg A** – der Spielkern läuft auf
32-bit-Geräten in einem eigenen Prozess ohne Java (ART). Diese Datei ist die Übergabe an die
Sitzung, die das umsetzt. Stand des Codes: Branch `claude/re4-start-controls` (Alpha 4,
v0.38.3, versionCode 59).

## Das Problem

Der Kern braucht MEM1 fest bei `0x80000000..0x83000000` (48 MB, GameCube-Hauptspeicher;
Spielzeiger sind diese Adressen). Die App lädt den Kern heute **im eigenen Prozess**
(`port/android-decomp/app/src/main/cpp/core_mode.cpp`, `loadCore`: Platzhalter mit
`MAP_FIXED_NOREPLACE` vor `dlopen`, `RE4_MEM1_PLACEHOLDER=1`, `re4_mem1_init` mappt darüber).

Tester-Bericht `cubeevil4-report-20261020-100743.zip` (App 0.38.2): Mobicel P11 Plus,
Android 14, **nur 32 bit** (`armeabi-v7a armeabi`), PowerVR GE8322, 2 GB RAM, Bildschirm
576x1152. Meldung: „core not started: MEM1 0x80000000..0x83000000 is taken in this process
(by 80000000-90000000 rw-p [anon:dalvik-main space])“; Adresskarte: „MEM1 placeholder: NOT
reserved“, direkt darunter `7f7ff000-80000000 [anon:dalvik-Concurrent mark-compact compaction
buffers]`.

- Dort liegt der **Java-Speicher von ART** (Hauptfläche 256 MB, genau ab 0x80000000). Der
  Müllsammler „Concurrent mark-compact“ (CMC, userfaultfd) ist der neue von ART (Android 14,
  auch per Mainline-Update); der Retroid (Android 13, alter Sammler CC) hat 0x80000000 frei,
  dort läuft 32 bit.
- Der Java-Speicher entsteht im **Zygote**, bevor die App einen Befehl ausführt; jede App erbt
  ihn per `fork` an derselben Stelle. Aus der App heraus nicht zu ändern (auch nicht mit
  `largeHeap`, `android:process`, App-Zygote: alle stammen vom Zygote ab).
- 64 bit ist nicht betroffen: dort liegt die Hauptfläche tief (z. B. `02000000-42000000
  [anon:dalvik-main space (region space)]`, Samsung Galaxy M23 Android 14), MEM1 und das
  Fenster bis 4 GB sind frei. Belegt bisher nur für ein 32-bit-Gerät; vermutlich trifft es
  viele reine 32-bit-Geräte mit Android 14+.
- Mehr RAM hilft nicht (Adresslage, nicht Menge).

Verworfen: (B) MEM1 auf 32 bit an eine andere Adresse (jede Stelle, die 0x80000000
annimmt, müsste umrechnen: Spielcode, SDK, GX-Adressen, ARAM, Spielstände – zu riskant);
(C) nicht unterstützen (steht vorerst als „Known issue“ im Releasetext der Alpha 4).

## Was es schon gibt

- **Kern-Schnittstelle** `port/whole/runtime/re4_core.h` und `re4_gx_frame.h`: reine
  C-Aufrufe und flache Daten. `Re4CoreSnapshot` (Zustandsbild, große flache Struktur ohne
  Zeiger), `Re4GxChunk` (ein Bild der GX-Nachbildung: Kopf mit Zeigern auf `ops`,
  `vertices`, `indices`, `blob`; der Renderer liest **keinen** Spielspeicher, alles steckt im
  Chunk), Ton als PCM-Ring (`re4_core_audio`, 32 kHz stereo).
- **Linux-Host** `port/whole/host/loader.c` + `main.cpp` (`re4_host`): startet den Kern
  schon heute als eigenständiges Programm (MEM1 vor dem Laden reserviert, Module laden,
  `--core-test`, `--runtime-test`, Filmtest). Vorbild für das Programm des Kernprozesses.
- **App-Seite**: `core_mode.cpp` ruft den Kern nur über die Tabelle `CoreApi api`
  (`dlsym` auf `libre4_main.so`). Alles andere (JNI `CoreBridge_*`, Java) spricht nur mit
  `core_mode.cpp`. Ein Fernaufruf-Ersatz für genau diese Tabelle hält den Umbau klein.

### Die Aufrufe von `CoreApi` (core_mode.cpp)

| Aufruf | Richtung, Takt |
|---|---|
| `mem1Init`, `reserve(dataDir, moduleDir)`, `start`, `state`, `message` | Start, Zustand |
| `pad(const Re4CorePad*)` | App → Kern, jede Eingabe |
| `rumble()` | Kern → App, je Bild |
| `audio(int16_t*, n)`, `audioStats` | Kern → App, Tonthread laufend |
| `acquire()` / `release()` (Re4CoreSnapshot) | Kern → App, je Bild |
| `gxAcquire()` / `gxRelease()` / `gxDetach()` (Re4GxChunk) | Kern → App, je Bild, bis einige MB |
| `vsync(ns)` | App → Kern, je Bildschirmtakt |
| `pause(on, …)`, `debugMenu()`, `setDof(on)`, `gyro(yaw, pitch)` | App → Kern |
| `cardList`, `cardFree`, `cardName`, `cardExport`, `cardImport` | Menü, selten |
| `frameCount`, `gameThread`, `freeAt`, `logTail` | Diagnose |

Dazu außerhalb der Tabelle: Absturzdatei (`CoreBridge.setCrashFile`, Signal-Handler im
Kern), Umgebungsvariablen als Einstellungen (`RE4_*`, `coreSettings`), das Fehlerprotokoll
(`CoreLog.java` liest `logcat` der eigenen UID; ein Kindprozess hat dieselbe UID).

## Umsetzung (Vorschlag, in der neuen Sitzung prüfen und mit dem Projektinhaber klären)

1. **Kernprogramm**: ein ausführbares Programm (PIE) im Bibliotheksordner der APK, Name
   `lib*.so` (z. B. `libre4_coreproc.so`), gebaut wie `re4_host` für Android
   (`port/tools/whole_build_android.sh`). Reserviert MEM1 vor dem Laden der Module.
   - **Achtung Paketierung:** `app/build.gradle` hat `packaging { jniLibs { useLegacyPackaging
     = false } }` – die Bibliotheken bleiben unentpackt in der APK. Ausführen geht nur aus
     dem entpackten `nativeLibraryDir` (Android 10+: W^X, aus dem Datenordner verboten).
     Also `useLegacyPackaging = true` (größere Installation) oder ein anderer Weg (z. B.
     `/system/bin/linker <pfad>` – zu prüfen).
   - Start über `ProcessBuilder` (Java) oder `posix_spawn` (kein `fork` im JVM-Prozess);
     im Kind `prctl(PR_SET_PDEATHSIG, SIGKILL)`; Adresskarte des Kindes ins Protokoll.
2. **Geteilter Speicher**: ein Bereich (`ASharedMemory` ab API 26; darunter `/dev/ashmem`
   oder `memfd_create` per syscall – minSdk der App ist 23) für den GX-Chunk-Ring (die
   Puffer, in die der Kern ohnehin baut, liegen gleich dort; Zeiger im Chunk-Kopf als
   Abstände, die App rechnet sie auf ihre Abbildung um), das Zustandsbild, den Ton-Ring, die
   Eingabe und kleine Zähler (Rumble, Bildzähler). Wecken über `eventfd` oder Futex im
   geteilten Speicher.
3. **Befehle**: ein `socketpair`, beim Start vererbt, für die seltenen Aufrufe (Start,
   Pause, Debug-Menü, DoF, Gyro, Speicherkarte, Meldungen, Protokollende) und das Ende
   (Absturz des Kindes = Socket schließt → Fehlerdialog, Absturzdatei des Kindes lesen).
4. **Wann der eigene Prozess**: Vorschlag: nur wenn MEM1 im App-Prozess nicht frei ist
   (Rückfall), sonst wie heute; Schalter zum Erzwingen für Tests (z. B. `RE4_CORE_PROCESS=1`,
   Testskript-Parameter). Projektinhaber fragen.
5. **Risiken**: „Phantom-Prozesse“ (Android 12+ beendet Kindprozesse, die im Hintergrund
   CPU brauchen; der Kern pausiert im Hintergrund ohnehin), Hersteller, die Kindprozesse
   beenden, SELinux (Ausführen aus `nativeLibraryDir` ist für Apps erlaubt), Leistung
   (Kopien vermeiden), schwache Geräte (das Testgerät hat nur 2 GB und eine PowerVR GE8322).

## Testen

- **Cloud:** 32-bit-Linux (`-m32`/i686): das Kernprogramm als Kindprozess, ein Testprogramm
  als „App“ (Chunks, Zustandsbild, Ton, Eingabe, Speicherkarte über die neue Schnittstelle;
  dieselben Prüfungen wie `--core-test`), `whole_build.sh` muss weiter alles bestehen.
- **Lokal (Retroid):** 32 bit erzwingen (`test-core-app.ps1 -Abi armeabi-v7a`) und den
  eigenen Prozess erzwingen (Schalter); Spiel bis r100, Ton, Film, Speichern/Laden, Menü.
- **Tester:** das Mobicel-Gerät (Android 14, nur 32 bit).
- Ein Meilenstein gilt erst nach dem echten Gerätetest.

## Bezug

Status-Datei `port/docs/whole-program-status.md` (Einträge „32 bit startet nicht“ vom
19./20.10.2026, MEM1-Diagnose der Alpha 3), `CLAUDE.md`, `port/docs/arm64-bestandsaufnahme.md`
(warum MEM1 fest bei 0x80000000 liegt).
