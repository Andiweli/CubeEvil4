# Phase 4c – Originale Message-, Font- und Core-Initialisierung

Stand: 10. Oktober 2026. Grundlage: `93e175162fdad72f03eba558867964d5c5307d83`
auf `android-port`. Fortsetzung von [Event/Motion/Grafik-Bootstrap](phase4c-event-status.md).
Keine Phase 5, keine manuelle Raumrekonstruktion und keine simulierte Spielfreigabe.

## Erreichtes Ergebnis

**`cMes.gameInit()` wird mit den englischen Originaldaten vollständig ausgeführt.**
Der originale `GameTask` durchläuft `gameDebug`, `gameInit`, `ClothInit`,
`GXSetCopyClear` und die gesamte Message-Initialisierung. Er erreicht anschließend
`SubScreenGameInit()` in `src/game/game.cpp:258` und meldet dort die nächste fehlende
gemeinsame Engine-Funktion. `IsMessageInit = 1` stammt aus dem Originalkörper
von `MessageControl::gameInit`; `Rno0` bleibt 0. Ein fertiger Spielstart wird nicht behauptet.

Davor laufen der originale `CoreDataRead` einschließlich `SpecularInit` und
`GlobalIlmTexInit`, sowie der originale Bootpfad `MessageControl::init` mit Common-
und System-Font, Sprachzuordnung und drei Glyphenwarteschlangen. Die Reihenfolge
Boot-Fonts vor Game-Message-Initialisierung bleibt erhalten. Die native API verlangt
diese Vorbereitung ausdrücklich; fehlende Bootdaten führen weiterhin zu einem Fehler.

Alle neuen Funktionen gehören zur bestehenden `re4_game_runtime`-Bibliothek.
Es gibt kein separates Ersatzspiel oder neues Raumladeverfahren. Der vorhandene
Runtime-Testtreiber führt den echten Bootpfad mit privaten Disc-Daten aus.

## Git und Originaldaten

- Branch ausschließlich `android-port`; der Ausgangsarbeitsbaum war sauber.
- `master`: `9b76ff003081d6ece8b838453fb8310743a972c7`, unverändert.
- Lokal verfügbares `origin/master`: `15cf64f80e08bbc64712c39be330b75d1512a0d0`.
  Die 25 README-Ergänzungen wurden gelesen: frühe, noch nicht spielbare Android-
  Portierung, beide ABIs, private Originaldaten und geplante Grafikoptionen.
  Keine Zusammenführung, kein Rebase, kein Push.
- Änderungen ausschließlich unter `port/`; `src/`, `include/`, Originalwerkzeuge,
  GCM-Images und Importblobs werden nicht verändert. Keine Assets in Git oder APK.
- Verbindliche Ressourcen: `E:\Development\Android\RE4-Data\import-phase3a`.
  Die bestehenden Kataloge und SHA-256-Werte werden vor dem Test, beim Engine-
  Ladevorgang und nach der ADB-Übertragung überprüft.

## Tatsächlich übernommene Originalfunktionen

`port/tools/generate_consumers.py` erzeugt vollständige, über Quellhashes gesicherte
Funktionskörper in `original/message_control.cpp`. Die bestehenden 87 Consumer-
Körper bleiben erhalten; hinzu kommen **33 Originalkörper**:

- `src/game/mes.cpp`: `MessageControl::init`, `gameInit`, `loadCommonFont`,
  `loadSystemFont`, `loadFont`, `setupFont`, `releaseFont`, `setLanguage`,
  `setLayout`, `setFontSize`, `setState`, `unsetState`, `checkState`, `Delete`.
- `src/game/mes.cpp`: `MessageFont::create`, `destroy`, `getSize`.
- `include/mes.h`: `MessageData::setLanguage`, `getLanguage`, `registData`;
  `Message::setDie`, `setFontSize`, `setLineGap`, `setFontGap`, `registQueue`;
  `MessageControl::Clear`, `MesRegistQueue`, `MesReleaseQueue`, `setLineGap`,
  `setFontGap`.
- `src/game/read.cpp:192`: `CoreDataRead`.
- `src/game/trans.cpp:2587,2625`: `SpecularInit`, `GlobalIlmTexInit`.

Die bereits übernommenen `MessageData::getAddr`, `getMesNum`, `getSpaceWidth`
werden weiterverwendet. Die bestehenden Runtime- und TexRender-Körper bleiben
erhalten. Der Generator dokumentiert jede Anpassung im Consumer-Manifest.
Der Quellschutz umfasst jetzt auch `trans.cpp`, `GXTexture.c`, `tpl.h` und die
originalen Core-Indexdefinitionen; geänderte Originalquellen werden abgelehnt.

### Native Adapter und ABI

Die folgenden Teile sind abgegrenzte Plattform-/Speicheradapter, keine zusätzlich
erfundene Spiellogik:

1. Die feste GameCube-Adresse `0x80578000` wird durch einen Besitzer des tatsächlich
   geladenen Core-Bodys ersetzt. FileTbl-Index 3, Modus `0x8001`, Core-Indizes,
   Größenbegrenzung `0x234000` und die anschließenden Originalaufrufe bleiben erhalten.
2. FNT enthält laut `setupFont` zwei BE-u32-Offsets: TPL und Glyphenbreiten.
   Beide werden begrenzt gelesen. TPL-Header, Deskriptoren und CLUT-Header erhalten
   native Ansichten; die Originalbytes behalten ihre Offsets und `unpacked = 0`.
   Der GameCube-Pointertest und die Inplace-Relokation in `MessageFont::create`
   werden durch eine geprüfte Relokation vor dem Original-Sheet-Loop ersetzt.
3. `GXInitTexObj`, `GXInitTexObjCI`, `GXInitTlutObj` speichern echte, validierte
   backendneutrale CPU-Zustände mit gepinnten Pixel-/Paletten-Spans. Dimensionen,
   Format, Wrap, TLUT-ID, CI-Kennzeichnung und die ursprünglichen Linearfilter-
   Defaults werden erhalten. Das sind **keine GPU-Uploads** oder leeren Erfolgs-Stubs.
   Originale GX-Adressregister werden nicht mit abgeschnittenen ARM64-Pointern gefüllt.
4. MDT-Header und englische Wortströme werden in einer ausgerichteten nativen
   Ansicht endian-konvertiert. Die Originalfunktion löst weiterhin dieselben
   relativen Nachrichtenreferenzen auf. BE-Bytes und Sprachblockgrenzen bleiben
   zusätzlich unverändert zugänglich. Kontrollcodes werden weder in Unicode
   umgedeutet noch durch erfundene Texte ersetzt.
5. Der Sprachwert verweist auf den gemeinsamen nativen `pSys->language`-Speicher.
   Englisch wird entsprechend dem Auftrag auf Original-Systemwert 1 eingestellt.
   Andere Sprachblöcke werden nicht als unterstützt ausgegeben.

Datei-Offsets bleiben u32; native Adressen und Größen werden nicht in u32 gepackt.
Die Fontpuffer kommen aus den vorhandenen 32-Byte-ausgerichteten Gameplay-Heaps.
TPL/MDT werden nicht als GameCube-Strukturoverlay auf ARM32 oder ARM64 verwendet.
Eine Texture-Ansicht mit fehlender Palette, ungültigen Grenzen, unbekanntem Format
oder derzeit nicht unterstützten Mip-Levels scheitert ausdrücklich.

### Lebenszeiten und Ladesemantik

Die originalen Fontpuffergrößen werden aus `common_j.fnt` und `system_j.fnt`
ermittelt, auch im englischen Zweig. Die englische Common-Datei passt in den
original vorgesehenen größeren Puffer. Slot 1 verwendet im englischen Zweig
denselben Common-Puffer; die separate System-Font-Allokation wird wie im Original
beibehalten. Beide Font-Slots besitzen eigene native Headeransichten und gemeinsame
Buffer-Leases. Die Freigabe von Slot 0 beschädigt Slot 1 nicht.

`MessageFont::destroy` gibt die native Ansicht frei. Es schreibt keine Pointer
zurück in die Originalbytes. Externe Snapshots pinnen ihre Rohdaten auch nach einem
vollständigen Runtime-Reset. Die normale Slot-Freigabe benutzt `Clear`/`Delete`.
Ein Reset/Freigeben aus einem laufenden Task wird abgelehnt; GameRuntime beendet
zuerst seine Stacks. Verwaiste DVD-Anforderungen werden generation-sicher abgebrochen
und am Frame-Rand abgeholt. Dateiworker schreiben nicht in Fontobjekte.

**Synchron bedeutet hier tatsächlich synchron:** `0x8001` und `0x11` setzen in
`cDvdQueue::Initialize` das Flag `0x40000000`; `cDvd::ReadReq` ruft `blockRead`
und `ReadProc` auf. Diese Startinitialisierung wartet innerhalb des Aufrufs auf die
Datei. Sie wird nicht in einen abweichenden Hintergrundmodus umgeschrieben.
Ein Suspend oder Abbruch vor Dispatch wird geprüft; ein Abbruch mitten in einem
solchen synchronen Aufruf vom selben Ownerthread wird nicht behauptet. Die vorher
implementierte asynchrone Hintergrundverarbeitung bleibt unverändert.

## Nachgewiesene Ressourcen

Die relevanten Dateien sind auf beiden Discs byte-identisch:

| Datei/Teil | Größe bzw. Inhalt | SHA-256 |
| --- | --- | --- |
| `etc/core.das` | 2.525.120 Bytes, originaler DRS-Container mit Soundteil | `018c6ddd4aa65a3f6325e2fbda4fecc1afd2b7fbb2695146a828f3c641abfaad` |
| Core-BODY | 2.295.616 Bytes, 36 originale Einträge | `eaa87634766195eb7a10b4afdcf310ca356d2b523021bb97d54d7f72c16db9aa` |
| `font/common_p.fnt` | 98.848 Bytes | `d4a9a30717f1a4d176ece16ed090d7413e5244f9aceba70497b93126eec5b35d` |
| `font/common_j.fnt` | 230.688 Bytes; Größenreferenz für Common-Allokation | im vorhandenen Importinventar geprüft |
| `font/system_j.fnt` | 33.184 Bytes; Größenreferenz für System-Allokation | im vorhandenen Importinventar geprüft |

Englischer Common-Font: TPL-Offset 32, Breitenoffset 98.464, eine CI4-Textur
1024×192, 16 RGB5A3-Paletteneinträge, 192 s8-Breitenpaare, Zellgröße 32×32.
Glyphenindex 0 verwendet entsprechend `MessageFont::getSize` das Paar von Index 1.
Beide Font-Slots, sämtliche Breitenpaare und alle neun englischen Layouts werden
mit einem unabhängigen Python-Referenzleser verglichen.

Core-Wortslot `0x0A` liefert 153 englische Nachrichten für die drei Registrierungen
0/1/2. Wortslot `0x15` liefert 256 Nachrichten für Typ 3. Das sind 715 geprüfte
Registrierungsreferenzen auf 409 Tabellen-Einträge; keine zusätzlichen Nachrichten.
`SpecularInit` und `GlobalIlmTexInit` initialisieren zusammen acht Texturzustände
(fünf Specular, zwei Indirect, eine Global-Illumination) sowie die Thermo-TLUT.
Alle Pixel-/Paletten-Spans werden gegen die tatsächlichen Core-TPLs gehasht.
Nicht benötigte Core-Blöcke und der Soundteil bleiben erhalten.

Die bekannten **35 unvollständigen englischen Raum-MDT-Blöcke** werden weiterhin
auf Zugriff als offen erkannt. Diese gültigen Core-Tabellen reparieren sie nicht.
`Message::Move`, `Trans`, die Message-Commandausführung und die fertige Menüausgabe
sind noch nicht angebunden. Typ 4 der DVD-Fehleranzeige gehört nicht zur geprüften
Game-Registrierung 0..3.

## GameTask, R120Event und verbleibende Abhängigkeiten

1. **GameTask:** `cMes.gameInit()` abgeschlossen; nächste Grenze
   `SubScreenGameInit()` (`src/game/sscrn.cpp:143`). Dieser Originalkörper benötigt
   `sscrnSetLanguage`, `SubScreenAramRead` und `SubScreenRoomInit`.
   `SubScreenAramRead` (`sscrn.cpp:73`) lädt `rel/Sscrn.rel`, `SS/eng/ss_cmmn.dat`
   und `ss_pzzl.dat` in den ursprünglichen ARAM-Bereich ab `0xD00000`, mit
   Offset-/Lebenszeitverwaltung. Danach benötigt `SubScreenRoomInit` die echten
   `ItemMgr.search`-Ergebnisse und `MapMgr.roomInit`. Diese UI-/Item-/Map-Initialisierung
   wird nicht durch fiktive Inventarwerte oder Default-Erfolge ersetzt.
2. **R120Init:** bestehende originale CPU-Initialisierung samt TexRender bleibt
   erfolgreich. Alle 37 r120-BIN-Vorkommen, externen pl00-TPLs, Originalregistrierungen,
   Motion-Initialisierungen und Abbruchcallbacks bestehen weiter.
3. **R120Event:** mit echter Android-Präsentation erreicht es nach `SceSleep`
   und dem originalen `systemVISetBlack` weiterhin
   `Sofdec.Initialize("movie/opening.sfd", 0)`. Headless bleibt VI-Präsentation eine
   ausdrückliche fehlende Abhängigkeit. Das Opening wird nicht übersprungen.

### Sofdec-Analyse

`src/game/sofdec.cpp:534` → `cSofdec::initSub` (`:547`) suspendiert für Flags 0
den Gameplay-Slot 0, startet Slot 1 mit `ThreadMove` und stoppt Audio über
`SndAllStop`. `ThreadMove` (`:585`) benötigt vor jeder Freigabe von Slot 0 echte
`initWork`, `initApp`, `startApp`, `Move` und `finishMovie`. Ohne diese darf ein
Adapter den Gameplay-Task nicht als Movie-Ende fortsetzen.

- `SofdecInit` (`sofdec.cpp:55`) setzt 59,94 Hz, den ADXM-Fehlercallback und ruft
  `mwPlyInitSfdFx` auf. `src/lib/mwsfdlib.c:195` initialisiert bereits ADXT,
  SJRBF/SJMEM/SJUNI, MWSTM, SFD und CRI-Servercallbacks. Das ist kein isolierter
  parameterloser Grafik-Initializer.
- `initWork` (`sofdec.cpp:508`) prüft die echte Filmdatei bzw. den Originalfallback
  `movie/dmy.sfd`, sichert Stop/Display-Flags, verschiebt gegebenenfalls 5 MiB des
  Gameplay-Heaps nach ARAM und verwendet Heap 11. Diese Originalfunktionen benötigen
  eine korrekte Native-Heap-/ARAM-Leihsemantik für lebende Game-Ressourcen.
- `initApp` (`:356`) liest 0x5000 Headerbytes, ruft `initDraw`/`setCamera` und
  `mwPlyGetHdrInf` auf. `src/lib/mwsfdcre.c:234` benötigt wiederum
  `SFD_AnalyCreInf` und `MWSFFRM_AnalyTotalFrmNum`.
- `startApp` (`:380`) berechnet den echten CRI-Workbuffer, erstellt den Sofdec-
  Handle und startet den Dateistream. `appMain` benötigt ADXM-Serverausführung,
  PTS/Frameverwaltung und dekodierte Video-/Audiodaten.
- `drawTex`, `setTevPrm`, `loadMvFrmFx` und `finishMovie` benötigen Y/UV- oder
  RGBA-Texturen, GX-Projektion/TEV-Geometrie, A/V-Synchronisation, Bildschirm-/VI-
  Umschaltung und die vollständige Ressourcenrückgabe. Der Clear-Bootstrap erfüllt
  diese Draw-Aufrufe noch nicht.
- Die CRI-Quellen sind vorhanden, aber noch nicht nativ integriert. Beispielsweise
  verwenden `mwsfdcre.c:946,993`, `sfd_ply.c:482` und `sfd_mpv.c:306` Pointer-
  Arithmetik über `Uint32`; CRI-Workbufferlayouts und ADXT-Abhängigkeiten müssen
  für beide ABIs einzeln geprüft werden. Ein direkter ARM64-Build wäre unsicher.

**Entscheidung:** Sofdec bleibt eine konkrete fehlende Funktion. Es gibt in dieser
Aufrufkette keinen sinnvoll als fertig anzubindenden gemeinsamen Initializer, der
ohne die genannten CRI-/Audio-/Renderabhängigkeiten einen gültigen Wiedergabestart
bewirken könnte. Die neuen backendneutralen Texture-/TLUT-Zustände sind später
wiederverwendbar; sie erzeugen keine Filmframes. Eine umfangreiche Decoder- oder
Audioportierung wurde entsprechend dem Auftrag nicht begonnen.

## Tatsächlich ausgeführte Builds und Tests

Werkzeuge: MSVC 19.51, CMake 4.1.2/Ninja, NDK 28.2.13676358/Clang 19,
Gradle 8.13/AGP 8.13.2; vorhandene Gradle-Caches im Offline-Build.
Zielgerät: **Retroid Pocket Flip 2, `dbdcacc`, Android 13 / API 33**;
ADB bestätigt `arm64-v8a,armeabi-v7a,armeabi`. Nur dieser Retroid wurde verwendet.

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | ---: | ---: | ---: | ---: |
| Host CTest | 25/25 | 25/25 | – | – |
| Bestehende Runtime-Selbsttests je Lauf | 634, 0 Fehler | 634, 0 Fehler | 634, 0 Fehler | 634, 0 Fehler |
| Neuer Original-Boot je Disc/Lauf | 960, 0 Fehler | 960, 0 Fehler | 960, 0 Fehler | 960, 0 Fehler |
| Original-Bootläufe, 2 Discs × 2 Runden | 4 | 4 | 4 | 4 |
| Tatsächliche modelInit-Aufrufe, kumuliert | 180 | 180 | 180 | 180 |
| Originale SetPartsSub-Aufrufe | 76 | 76 | 76 | 76 |
| Event-/Zusatz-MotionSetCore | 132 | 132 | 132 | 132 |
| pl00-MotionSetCore | 264 | 264 | 264 | 264 |
| Reale RELs, je Runde | 114 | 114 | 114 | 114 |
| Ressourcen / Raumarchive, je Runde | 308 / 142 | 308 / 142 | 308 / 142 | 308 / 142 |
| Native Core-Suite, je Runde | CTest | CTest | 13.386, 0 Fehler | 13.387, 0 Fehler |
| APK-Instrumentierung | – | – | 20/20 | 20/20 |
| Vulkan GPU-Clears / Presents | – | – | 15 / 15 | 15 / 15 |

Originaldaten-, Modul-, Ressourcen- und Core-Gerätetests wurden jeweils zweimal
ausgeführt. Die zusätzliche ARM64-Core-Prüfung kontrolliert einen Pointer oberhalb
von 4 GiB. Neue Message-Prüfzahlen und relevante Zustandsfolgen sind identisch.

Der neue Test prüft die tatsächlichen Originalinitialisierungen, alle 715 Message-
Referenzen einschließlich sämtlicher nativer BE-Wörter, Glyphenbreiten, Layouts,
Queuezuordnung, Font-Aliasing, Freigabe/Reload, vollständigen Reset und weiterhin
gültige Snapshots. Suspend/Signal/Kill vor Boot-Dispatch und verbotene Freigabe
aus einem aktiven Stack werden geprüft. Kurze TPLs, zu viele Font-Deskriptoren,
ungültige BE-Offsets und fehlende CI-Paletten sind ausdrücklich isolierte
Adapter-Grenzprüfungen an kopierten Daten im RAM.

Der Vergleich `summarize_consumers.py` validiert die Berichte aller vier ABIs.
Gemeinsamer Message-Semantikhash:
`b5b73016b40aedc00e97177997bd5aff9bdfe8fd833b3f7373e77a40b16d34d3`.
Modell-, Motion-, Raum- und bisherige Runtime-Traces stimmen ebenfalls überein.
Die privaten Berichte enthalten die konkreten Executable-Hashes.

Die APK-Prüfung verwendet weiterhin die **assetfreien isolierten Fixtures** mit
634 Runtime-Checks und erreicht dort ohne privaten Bootkatalog bewusst die
fehlende Message-Bootvorbereitung. Die neuen Originaldaten-Checks und der
GameTask-Fortschritt bis SubScreen wurden in den echten Android-CLI-Prozessen
ausgeführt; sie werden nicht als GPU-Textausgabe ausgegeben. Vulkan-Clears,
Readback und Präsentation stammen aus den echten APK-Prozessen, Adreno 650 / Vulkan
1.1.128. Aktive Optionen bleiben Samples 1, Anisotropie 1, DoF aus.

Controller-/Lifecycle-/Frameadapter-Tests bestehen. Neue physische Tastendrücke
wurden in dieser Fortsetzung nicht angefordert; der frühere physische Nachweis
und die Triggerkorrektur bleiben erhalten.

### Fehlversuche und Korrekturen

- Der erste CTest-Aufruf in der Sandbox scheiterte bei realen Windows-Pfadprüfungen
  mit `WinError 5`. Derselbe Test außerhalb der Sandbox bestand vollständig;
  dies war kein als erfolgreich gewerteter Lauf.
- Ein früher Originaldaten-Test wollte den synchronen Boot nach einem Frame
  abbrechen. Der Task war bereits fertig; die Prüfung schlug korrekt fehl.
  Der Test wurde auf die nachgewiesene synchrone Originalsemantik korrigiert.
- Die endgültigen oben genannten Läufe enthalten keine fehlgeschlagenen Prüfungen.

## Reproduktion und Nachweise

Die bestehenden Skripte werden weiterverwendet; keine zusätzlichen privaten Assets
im Projekt. In einer MSVC-Developer-Shell:

```powershell
cmake --build port/build/host-ninja --parallel
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
ctest --test-dir port/build/host-x86 --output-on-failure
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python '<Python>' -Offline
.\port\scripts\test-device.ps1 -Adb '<ADB>' -Serial dbdcacc -Repeat 2
.\port\scripts\test-app-device.ps1 -Adb '<ADB>' -Serial dbdcacc
```

Für beide Host-Executables einzeln, danach ADB mit `--serial dbdcacc`:

```powershell
<Python> port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-message-host' --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
<Python> port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-message-device' --adb '<ADB>' --serial dbdcacc --repeat 2
<Python> port/tools/summarize_consumers.py --host 'E:\Development\Android\RE4-Data\phase4c-message-host' --device 'E:\Development\Android\RE4-Data\phase4c-message-device' --output 'E:\Development\Android\RE4-Data\phase4c-message'
```

`test_modules.py` und `test_resources.py` wurden mit denselben `--data`, `--repeat 2`,
jeweils beiden Host-Executables sowie `--adb`/`--serial` ausgeführt. Ihre Outputordner
sind `phase4c-message-modules` und `phase4c-message-resources` außerhalb des Repos.
Build-/CTest-/Device-Logs liegen ausschließlich im ignorierten `port/build/message-*`.
Die finale Debug-APK hat SHA-256
`b407b3841ec7bb4880078429efbd8c7bc0829559360ead6340e242f55f9ae87c`.

## Vollständig gespeicherte Projektdateien

Neu: `port/include/re4/port/messages.h`, `port/src/message_original.h`,
`port/src/messages.cpp`, dieser Bericht.

Geändert: `port/CMakeLists.txt`, `port/config/consumer-sources.json`,
`port/include/re4/port/consumers.h`, `port/include/re4/port/game_runtime.h`,
`port/src/game_runtime.cpp`, `port/src/runtime_original.h`,
`port/tests/game_runtime_tool.cpp`, `port/tests/test_consumer_sources.py`,
`port/tools/generate_consumers.py`, `port/tools/generate_runtime.py`,
`port/tools/test_game_runtime.py`, `port/tools/summarize_consumers.py`.
Generated C++ und private Nachweise werden reproduzierbar erzeugt und nicht eingecheckt.

## Nächster konkreter Schritt

Innerhalb einer gesondert beauftragten Fortsetzung zuerst die originale
`SubScreenGameInit`-Kette absichern: englische SS-Ressourcen/REL, ARAM-Offsets,
Ressourcenlebenszeiten und tatsächliche Item-/Map-Initialisierung. Keine Default-
Inventarwerte. Erst danach den nächsten originalen GameTask-Aufruf bestimmen.
Für R120Event bleibt ein separater CRI-/Audio-/Movie-Integrationsauftrag nötig.

Die geplanten backendunabhängigen Grafikoptionen bleiben verbindlich:
MSAA aus/2×/4×, AF 2×/4×/8×, DoF mit Nah-/Zielfokus und persistente Speicherung.
Hardwareunterstützung muss bei Renderzielen/Samplern validiert werden, bevor eine
Option als aktiv erscheint. Diese Fortsetzung implementiert weder das Grafikmenü
noch vollständige GX-Geometrie, Shader, MotionMove oder Movie-Wiedergabe.
