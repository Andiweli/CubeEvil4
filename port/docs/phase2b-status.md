# Phase 2b – überprüfter Hintergrund-Scheduler

Stand: 2026-10-09. Projekt: `E:\Development\Android\RE4-Fixed`.
Branch: **android-port**. Sämtliche gelieferten Dateien liegen vollständig im
Projekt. Originalcode, master und GameCube-Assets wurden nicht verändert.
Es wurde nichts gepusht und der vollständige Assetimport wurde nicht begonnen.

## Gesicherter Ausgangspunkt

Vor der Entwicklung wurden die Phase-2-Tests erneut ausgeführt: Windows x86/x64
je 11/11 CTest-Gruppen, Android-Dual-ABI-Build/APKs/Lint erfolgreich, Retroid
ARM32 13.143 und ARM64 13.144 native Prüfungen ohne Fehler sowie je 9/9 JUnit-
Tests in beiden App-Prozessen. Danach wurde Phase 2 lokal gesichert:
**3c2dcfbc79bd787df1b45c7e700f475f86555ca7**.

master bleibt auf **9b76ff003081d6ece8b838453fb8310743a972c7**.
Phase 2b wird als eigener lokaler Nachfolgecommit gesichert; seine Identität
steht in der Git-Historie und im Sitzungsabschluss, nicht als Selbstreferenz
im Inhalt dieses Commits.

## Erreichte Funktion

Slot 4 besitzt einen eigenen, stackerhaltenden Kontext und läuft kontrolliert
am Frameende nach den Gameplay-Tasks und Watchern. Exec ist verzögert, genau
ein Hintergrundtask ist aktiv. Budget-/Retrace-Requests pausieren am Checkpoint;
Fortsetzung erhält verschachtelte Aufrufe und lokale Variablen. iTaskExit,
iTaskKill, Self-Kill, TaskSleep, Modulinvalidierung, Exceptions, alte Handles
und erneuter Start sind geprüft. Es gibt kein erzwungenes OS-Thread-Suspend.

Originale iTask-Namen werden durch typisierte Native-Adapter bedient. Ihr
gesamter SDK-abhängiger scheduler.cpp-Körper wird nicht als unverändert nativ
kompiliert behauptet. Die Policy folgt den tatsächlichen Originalfunktionen.
Framefolge und Gate-/Taskverhalten bleiben aus Phase 2 erhalten.

Echtes Dateilesen arbeitet auf einem einzigen dauerhaften I/O-Worker mit
privaten Job-Puffern, atomarem Fortschritt und Terminalstatus. startFileReadPump
pollt dieses Ergebnis begrenzt auf Slot 4. Ein Taskabbruch fordert I/O-Cancel
an und kehrt sofort zum Owner zurück; der Worker behält seinen Speicher bis
zur Bestätigung. Neue Reads werden erst danach angenommen.

Die originale C-Initialisierung Yz2DecodeSet/Yz2DecodeExec und ihre Setup-Makros
werden aus der unveränderten Quelldatei mit benannten ABI-/State-/Bounds-/
Checkpoint-Adaptionen generiert. Der PPC-Assemblerkernel wird durch eine
gesonderte native Übersetzung ersetzt. Dictionary-Referenzen sind geprüfte
32-Bit-Ausgabeoffsets; native Pointer werden nie auf u32 reduziert. Input, Work
und Output gehören der Decoderinstanz; Teiloutput wird nicht publiziert.

Eine Gegenüberstellung von Worker und kooperativem Hintergrundpfad sowie
belegte Originalaufrufstellen, Zustände und Grenzen stehen in
[background-design.md](background-design.md).

## Tatsächlich ausgeführte finale Prüfungen

Gerät: **Retroid Pocket Flip2**, USB-Serial **dbdcacc**, Android **13 / API 33**.
Gemeldete ABIs: arm64-v8a, armeabi-v7a, armeabi. Es wurden echte ARM32- und
ARM64-Prozesse gestartet. Kein Emulator und keine bloße Crossbuild-Annahme.

| Umgebung | Ausführung | Ergebnis |
| --- | --- | --- |
| Windows MSVC x86, Release/Ninja | 15 CTest-Gruppen; vollständige native Suite | 15/15, **13.257 Checks, 0 Fehler** |
| Windows MSVC x64, Release/Ninja | 15 CTest-Gruppen; vollständige native Suite | 15/15, **13.258 Checks, 0 Fehler** |
| NDK armeabi-v7a, Release | Testexe und JNI, vollständiger Link | Erfolgreich |
| NDK arm64-v8a, Release | Testexe und JNI, vollständiger Link | Erfolgreich |
| Gradle Debug, beide ABIs | APK, Test-APK, lintDebug | Erfolgreich; 0 Lint-Fehler, 8 bestehende Warnungen |
| Retroid armeabi-v7a, 32-Bit-Prozess | Finale native Suite **3 vollständige Läufe** | Je **13.386 Checks, 0 Fehler** |
| Retroid arm64-v8a, 64-Bit-Prozess | Finale native Suite **3 vollständige Läufe** | Je **13.387 Checks, 0 Fehler** |
| Retroid APK erzwungen armeabi-v7a | 12 JUnit-Gruppen im App-Prozess | **12/12**, primaryCpuAbi und Pointer=32 geprüft |
| Retroid APK erzwungen arm64-v8a | 12 JUnit-Gruppen im App-Prozess | **12/12**, primaryCpuAbi und Pointer=64 geprüft |
| Python 3.12, vorhandene yz2.py-Referenz | Fixture-Reproduktion/Manifestcheck, auch CTest | 3/3 bytegenau |
| CMake Original-Source-Guard | Identität und 10 einzeln mutierte Kopien | Unveränderte akzeptiert, alle Änderungen abgelehnt |

Die drei neuen Gruppen haben auf **allen vier ABIs** dieselbe Prüfzahl:
background **164**, yz2 **53**, background_io **26**, zusammen **243**.
ARM64 hat zusätzlich die bereits bestehende Assertion, dass ein echter
Heap-Pointer über UINT32_MAX liegt. Das erklärt exakt den Unterschied von
einem Check. Android führt außerdem die bestehenden 129 zusätzlichen ARM-
Register-/Stackprüfungen aus. Windows LLP64 und Android ARM64 LP64 werden
explizit ausgegeben; u32 bleibt überall 32 Bit.

Das Geräteskript hat alle **sieben kanonischen Traces** zwischen sämtlichen
Wiederholungen und beiden ARM-ABIs bytegleich verglichen. Sie beschreiben die
wirklich geprüften Reihenfolgen/Zustände, sind keine GameCube-Aufzeichnung.
Die Anzahl tatsächlicher I/O-Pollframes ist laufzeitabhängig und wird nicht
als deterministisch gleich behauptet.

Die neuen Tests prüfen:

- mehrfaches verschachteltes iTaskSuspend/Resume, Locals und Stackadressen;
- Status pending/RUN/SUSPEND/Complete, Veröffentlichung vor Idle und Freigabe;
- Reihenfolge 0,1,2,3, Watcher, Hintergrund bei gleichzeitigem Gameplay;
- 32 Cross-Thread-Cancel/Restart-Zyklen pro Suite, Generationen, fremder Retrace;
- aktiven und ruhenden Modulbesitz, Self-Kill, Sleep, Fehler, Zeit-/Einheitenbudget;
- exakte Yz2-Ausgabe für alle Literale und überlappende/gespeicherte Matches;
- 16-/24-/32-Bit-Längen, beide Modellhalbierungen und Dictionary-Ringumlauf;
- gesliced/synchron gleiche Ausgabe und Decoderverbrauch, unabhängige Instanzen;
- Decoderabbruch während tatsächlicher Verarbeitung und acht Folgeaufträge;
- begrenzte Header/Input/Output-Zugriffe, ungültige Referenzen und Fehlerzustände;
- echte Dateilesen gegen unabhängige Referenz, acht Folgeanforderungen, Fehler;
- reale In-flight-I/O an einer ausdrücklich gekennzeichneten Testbarriere:
  gleichzeitig belegter Reader, unveröffentlichter Puffer, nichtblockierender
  Taskabbruch, Workerbesitz bis Cancel-ACK, Neustart nach Freigabe.

### Präzise Trennung Originalcode / Adapterprüfung

Die bisherigen RNG-/Flag-/Mathe-/Scheduler-Kontrollfunktionen bleiben echte
Originalfunktionen im Build. Yz2-C-Setup ist Originalcode mit expliziten
Portierungsänderungen; der Kernel ist notwendiger PPC-Ersatz, kein unverändert
ausgeführter PPC-Assembler. Die Referenz stammt aus tools/motion/yz2.py.

DvdReadProc wird **verbatim extrahiert und als isolierte Adapterprobe** gegen
DvdProbe::ReadProc ausgeführt. DvdProbe ist ausschließlich Verifikation, keine
Fake-cDvd-Implementierung der Laufzeit. Die Probe delegiert an tatsächliches
Native-I/O-Polling, simuliert aber keine DVD-SDK-/Queue-/ARAM-Regeln.
Frame-/Watcher-/Retrace-Callbacks im Test sind ausdrücklich Infrastrukturproben.
Es wird kein fertiger Originalraum oder vollständiger Gameplaylauf behauptet.

Die synthetischen Eingaben sind 366, 57 und 2.749 Bytes; Referenzausgaben 256,
26 und 70.002 Bytes. Generator und SHA-256-Manifest liegen im Projekt. Native
Tests vergleichen jeden Ausgabebyte, keine bloßen Größen oder Plausibilitätswerte.

## Gefundene Fehler und Korrekturen

Zwei erste MSVC-Buildversuche scheiterten im neuen Testcode: fehlendes array-
Include und eine unter /WX abgelehnte int-zu-u8-Füllkonstante. Beide wurden
korrigiert; es wurde keine Warnung unterdrückt, um den Build passend zu machen.

Der erste ARM32-Lauf meldete **13.373 Checks, einen Fehler**: Nach Decoderabbruch
blieb ein Completion-Capture erhalten. libc++ darf kleine std::function-Targets
beim Move kopieren und das Quellobjekt nicht leeren. Explizites Leeren nach Move
behebt diese ABI-/Bibliotheksabweichung. Dasselbe wurde beim TaskChain-Capture
abgesichert. Der ursprüngliche Fehlerlog bleibt lokal in
port/build/phase2b-initial-arm32-failure.txt erhalten. Danach bestanden drei
native Läufe je ABI; nach Ergänzung der Pumpe und Veröffentlichungssicherung
wurden die finalen sechs Geräteläufe und beide App-/Hostprüfungen wiederholt.

Die erste synthetische Encoder-Selbstprüfung schlug vor dem Oracle-Aufruf fehl,
weil die Rückrechnung eine Vollrange statt der belegten Halbrange voraussetzte.
Die Rückrechnung wurde korrigiert; sämtliche Fixtures wurden erst nach
bytegenauer Referenzprüfung übernommen. Keine fehlerhafte Fixture blieb im Stand.

AGP meldet weiterhin CXX5304 (SDK-XML-Versionen 3/4). Lint hat acht Warnungen:
OldTargetApi, AGP-/Dependency-Versionen, ChromeOS-ABI, DataExtractionRules,
fehlendes Diagnose-App-Icon und SetTextI18n. Keine neue Renderer-/SDK-Funktion
wurde deswegen behauptet. Die finalen Builds und Tests sind erfolgreich.

## Reproduktion und lokale Evidenz

Vom Projektroot, vorhandene MSVC-Developer-Umgebung jeweils x86/x64 aktivieren:

```powershell
.\port\scripts\test-host.ps1 -BuildName host-x86
.\port\scripts\test-host.ps1 -BuildName host-ninja
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Offline
.\port\scripts\test-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc -Repeat 3
.\port\scripts\test-app-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
```

Host-CTest fand Python hier mit explizitem Python3_EXECUTABLE auf den vorhandenen
Codex-Python-3.12-Runtimepfad. Eine andere Installation genügt; dieser Rechnerpfad
ist nicht in der Buildkonfiguration verdrahtet. Ohne Python bleiben die 14
nativen/Source-Guard-Gruppen verfügbar; --check ist dann separat auszuführen.
Gepinnte SDK-/NDK-/Gradle-Versionen stehen in port/README.md.

Lokale vollständige Logs (Buildausgaben bleiben bewusst ignoriert):
port/build/device-{armeabi-v7a,arm64-v8a}-round{1,2,3}.txt,
port/build/instrumentation-{armeabi-v7a,arm64-v8a}.txt und
port/build/{host-x86,host-ninja}/Testing/Temporary/LastTest.log.
Gerätelogs enthalten Modell, OS, tatsächliche ABI und Hash des ausgeführten
Binaries; App-Logs zusätzlich primaryCpuAbi und APK-Hash.

Finale SHA-256:

| Getesteter Output | SHA-256 |
| --- | --- |
| armeabi-v7a Testexe | 0B1D9F4C3C5BF9576AF3AA9816868A186277C2AC7153F61587EC96A28DF23BAD |
| arm64-v8a Testexe | 6181CCF8DF34561FA4EC86AF22AC16BA262C2F294E0D5543F962767D8AD91FD9 |
| Dual-ABI Debug-APK 0.2.1 / Code 3 | D0464752EB94DD99D0C8D6449C64ACE99E5A8CD7EFEFFEF586C28FA0F7F5A387 |

## Vollständige Projektdateien dieses Meilensteins

Alle folgenden Pfade liegen unter dem Projektroot; keine Datei wird nur als
Patch-Snippet geliefert. Neu:

- port/cmake/Yz2Sources.cmake
- port/include/re4/port/async_file.h
- port/include/re4/port/yz2.h
- port/src/async_file.cpp
- port/src/yz2.cpp
- port/src/yz2_internal.h
- port/src/yz2_kernel.cpp
- port/src/background_verification.cpp
- port/src/background_verification.h
- port/src/dvd_adapter_probe.h
- port/tests/yz2_fixtures.h
- port/tests/yz2_fixtures.json
- port/tools/generate_yz2_fixtures.py
- port/docs/background-design.md
- port/docs/phase2b-status.md

Geändert:

- port/CMakeLists.txt
- port/README.md
- port/cmake/OriginalSources.cmake
- port/compat/include/re4_scheduler_original.h
- port/include/re4/port/scheduler.h
- port/src/scheduler.cpp
- port/src/verification.cpp
- port/tests/source_guard_negative.cmake
- port/tests/test_main.cpp
- port/scripts/test-device.ps1
- port/scripts/test-app-device.ps1
- port/android/app/build.gradle.kts
- port/android/app/src/androidTest/java/org/re4/port/OriginalCodeTest.java
- port/docs/abi-strategy.md
- port/docs/runtime-boundaries.md
- port/docs/scheduler-design.md

Generated Sources unter port/build/*/original sind nachvollziehbare Buildoutputs,
keine zusätzliche manuell gepflegte Originalcodekopie. Keine Originalassets
oder Binaries werden getrackt.

## Verbleibende Abweichungen und Voraussetzungen für Phase 3

**Nachgewiesen:** Stack-/Slot-/Cancel-/Modul-/Publikationssemantik der nativen
Infrastruktur, Dual-ABI-Übereinstimmung, echte begrenzte Datei-I/O und geprüfte
Yz2-Dekomprimierung einschließlich adaptiver Pfade.

**Entwurfsannahme:** Das gewählte 2-ms-/Einheitenbudget ist für die spätere
Frameintegration sinnvoll. Es muss an echten privaten Archiven profiliert
werden. Checkpoints ersetzen VI-Hardwarepräemption und liefern dieselben
geprüften Zustandsübergänge, keine identische Unterbrechungsadresse/-zeit.

**Offen:** vollständiger VI-/Android-Lifecycle-Frameadapter; cDvd-Queue, Retry,
Discwechsel, MRAM/ARAM und SDK-Abbruchregeln; Mem-/pG->pRoom-Anbindung von
decodeData; SAF-/FD-Zugriff auf private Originaldaten; echte Containergrenzen
und vollständige Differentialprüfung gegen Originalarchive/GameCube-Läufe.
ASan-Fiber-Hooks, andere Androidgeräte, 16-KiB-Seiten/PAC/BTI und volle Engine-
Stacklast wurden nicht durch diesen Meilenstein bewiesen.

Der konkrete nächste Schritt ist die sourcebasierte Integration der bestehenden
cDvd-/ReadAreaData-Steuerlogik an die geprüften Read-/Decode-Jobs und eine
native Modulregistrierung mit transitive Lifetime-Zuordnung. Zuerst müssen
Queue-/Flags-/Fehler-/Cancel- und Heap-/Room-Besitz aus Originalquellen abgeleitet
werden. Danach kann ein privat bereitgestelltes Originalarchiv als Prüfinput
dienen. BSS/Pointer-/BE-Konvertierung und Modulgrenzen sind vor einem vollständigen
Raumtest nachzuweisen. Keine manuelle Rekonstruktion von Räumen oder Gameplay.

Grafik bleibt unimplementiert. Die verbindlichen Anforderungen bleiben:
backendneutrales Ingame-Menü, MSAA Aus/2×/4×, AF 2×/4×/8×, DoF Ein/Aus mit
Nähe-/Zielfokus, persistente Speicherung und echte Hardwarevalidierung.
Phase 3 und kompletter Assetimport wurden nicht begonnen.
