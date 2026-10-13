# Phase 2 – überprüfbarer Scheduler-Meilenstein

Stand: 2026-10-09. Branch: `android-port`.

Die kooperative Spieltask-Laufzeit ist auf echten ARM32- und ARM64-Prozessen
nachgewiesen: getrennte Stacks, mehrfache Unterbrechung/Fortsetzung, lokale
Variablen, verschachtelte Aufrufe, ursprüngliche Reihenfolgen und wesentliche
Zustandsübergänge funktionieren. Der interruptpräemptierte Hintergrundslot 4
und die vollständige Einbindung der Engine sind ausdrücklich noch offen.
Die nächste große Entwicklungsphase wurde nicht begonnen.

## Gesicherter Phase-1-Stand

Vor den Scheduler-Änderungen wurden Arbeitsbaum und Branch geprüft, beide
Hostbuilds mit jeweils 7/7 CTest-Fällen und der vollständige duale Android-Build
einschließlich APK, Test-APK und Lint wiederholt. Auf dem inzwischen verbundenen
Retroid bestanden die unveränderten Phase-1-Executables jeweils 12.412 Checks
unter ARM32 und ARM64. Die installierte ursprüngliche Debug-APK bestand ihre
fünf Instrumentierungstests unter der automatisch gewählten ARM64-ABI.

Anschließend wurde der komplette geprüfte Phase-1-Stand lokal gesichert:

```text
ec1b162a4cb982f6c06b7e9a1094f9c3959fc594
Add verified dual-ABI Android native foundation
```

`master` bleibt auf `9b76ff003081d6ece8b838453fb8310743a972c7`.
Alle neuen/angepassten Projektdateien liegen unter `port/`. Der Vergleich mit
master außerhalb dieses Verzeichnisses ist leer. Es gab keine Pushes.
Die Phase-2-Änderungen liegen vollständig im Arbeitsbaum und sind noch nicht
committet. Originalspielcode, SDK-Quellen und ursprüngliche Konfigurationen
wurden nicht verändert; GameCube-Assets wurden nicht übernommen.

## Tatsächliches Testgerät

| Eigenschaft | ADB-Ergebnis |
| --- | --- |
| Gerät | Retroid Pocket Flip 2, ADB-Modell `Retroid Pocket Flip2` |
| Seriennummer | `dbdcacc` |
| Android | 13 |
| API | 33 |
| Unterstützte ABIs | `arm64-v8a,armeabi-v7a,armeabi` |
| Ausgeführte Test-ABIs | `armeabi-v7a` und `arm64-v8a` |

Beide nativen Executables wurden getrennt nach `/data/local/tmp/re4-port-tests`
übertragen und tatsächlich ausgeführt. Die Debug-APK wurde außerdem nacheinander
mit `adb install -r --abi armeabi-v7a` und `--abi arm64-v8a` installiert.
`dumpsys package` bestätigte die jeweilige primaryCpuAbi. Ein JUnit-Test prüft
zusätzlich die erwartete Pointerbreite aus dem tatsächlichen JNI-Ergebnis.
Der letzte installierte Appstand ist Version 0.2.0 / ARM64.

## Endgültige Ergebnisse

| Prüfung | Ergebnis |
| --- | --- |
| Windows x86 / MSVC 19.51 | 11/11 CTest-Fälle; 13.014 native Checks, 0 Fehler |
| Windows x64 / MSVC 19.51 | 11/11 CTest-Fälle; 13.015 native Checks, 0 Fehler |
| Retroid / armeabi-v7a / Release-Executable | 13.143 Checks, 0 Fehler; tatsächliche 32-Bit-Pointer |
| Retroid / arm64-v8a / Release-Executable | 13.144 Checks, 0 Fehler; tatsächliche 64-Bit-Pointer |
| Retroid / ARM32-Debug-App-Prozess | 9/9 Instrumentierungstests bestanden |
| Retroid / ARM64-Debug-App-Prozess | 9/9 Instrumentierungstests bestanden |
| NDK / beide ARM-ABIs | Executable, Scheduler-Archiv und JNI-Library gebaut und gelinkt |
| Gradle assembleDebug / assembleDebugAndroidTest | Erfolgreich, duale JNI-APK und Test-APK |
| Gradle lintDebug | Erfolgreich; 0 Fehler, 8 bestehende Warnungen |
| Native ARM32/ARM64-Traces | Automatisch verglichen, identisch |
| Source Guard | Alle verwendeten Originalquellen geprüft; Änderungen an RNG, Scheduler, SceSys und TASK-Header in Buildkopien nachweislich abgelehnt |

Die Checkzahlen unterscheiden sich absichtlich: 64-Bit-Prozesse prüfen zusätzlich
eine Pointeradresse oberhalb von UINT32_MAX. Android prüft zusätzlich acht
fest gebundene FP-Register in zwei Tasks über jeweils 64 Sleeps und deren
Stackfreigabe (129 zusätzliche Checks). Alle gemeinsamen semantischen
Erwartungen und Ablaufspuren sind gleich.

| Gruppe | Host32 | Host64 | ARM32 | ARM64 |
| --- | ---: | ---: | ---: | ---: |
| Foundation ABI/RNG/Flags/Math/Endian | 12.412 | 12.412 | 12.412 | 12.412 |
| scheduler_stack | 26 | 27 | 155 | 156 |
| scheduler_order | 27 | 27 | 27 | 27 |
| scheduler_lifecycle | 537 | 537 | 537 | 537 |
| scheduler_modules | 12 | 12 | 12 | 12 |

Die numerischen Traces sind auf allen vier Plattformen gleich:

```text
TRACE scheduler_stack: 10 11 12 13
TRACE scheduler_order: 0 1 2 3 8 9 7 6
TRACE scheduler_lifecycle: 1 2 3
TRACE scheduler_modules: pending-cancel sleep-cancel expired-reject ancestor-protect direct-protect foreign-reject
```

Das beweist die geprüften Übergänge und Reihenfolgen, keine vollständige
GameCube-/Gameplay-Äquivalenz. Eine unabhängige PowerPC-Referenztrace steht noch
nicht zur Verfügung.

## Was ausgeführt und übernommen wurde

Aus scheduler.cpp werden unverändert extrahiert und kompiliert:
GetStackSize, TaskSuspend, TaskSignal, TaskStatus und SetTaskModelPtr.
Aus sce_sys.cpp: SceSleep, SceExit und cSceSys::checkCTaskRange.
Die sechs TASK-Kind-Methoden werden unverändert aus scheduler.h übernommen.
Die Phase-1-Originalfunktionen für RNG, FlagBitChk, hermite und IPOW bleiben
Teil derselben ausführbaren Suite.

SceSleep wird tatsächlich aus dem Original aufgerufen: Testtask, verschachtelte
Testfunktion, unverändertes SceSleep, nativer TaskSleep-Adapter, echter Stackwechsel.
Die Tests prüfen Werte UND die Adresse eines lokalen Objekts über mehrere Sleeps,
äußere und innere Frames, Stackalignment und den identischen OS-Owner-Thread.
SceExit läuft über den Originalcode zur tatsächlichen OT-Entfernung und Stackexit.

TaskSchedulerMain und die Scheduler-/SceExec-Policy sind in einem separaten
NativeScheduler adaptiert. Die kompletten Originaldateien mit sämtlichen
Engine-, Dolphin-, GX- und Event-Abhängigkeiten werden nicht als portiert
ausgegeben. Testcallbacks sind Scheduler-Proben, keine erfundene Spiellogik.
Typisierte Bindings transportieren Pointer und Integer ohne Funktions-Casts
oder Pointer-Trunkierung. Mehrfaches Suspend/Signal, Kill vor Start und im Sleep,
Self-Kill, TaskChain, scenario return und unerwartete main return werden geprüft.
512 Stack-Erstellungs-/Sleep-/Abbruchzyklen prüfen die Freigabe. Modul-Tests
prüfen aktive Ancestors, direkte/nestende Calls, abgelaufene Tokens und
erneute Anmeldung aus einem Entry-Capture-Destruktor während Stilllegung.

Der Vergleich der Kontextstrategien, alle Originalbelege, Daten-/ABI-Entscheidungen
und expliziten Abweichungen stehen in [scheduler-design.md](scheduler-design.md).

## Fehlgeschlagene Zwischenprüfungen und Behebung

- Der erste Windows-Lauf hatte in scheduler_stack zwei FP-Fehler: gleichzeitige
  manuelle fenv-Sicherung und die Windows-Fiber-Sicherung überschrieben den
  erwarteten Kontext. Windows verwendet nun ausschließlich seine Fiber-Sicherung;
  Android behält die separate fenv-Sicherung. Der unveränderte FP-Test besteht.
- Der erste ARM32-Build wurde durch -Werror abgebrochen, weil das Windows-only-
  Feld converted_ auf Android unbenutzt war. Das Feld ist nun plattformspezifisch
  deklariert; keine Warnung wurde abgeschaltet.
- Der erste Download der kleinen Boost-Quellmenge scheiterte an sandboxbedingter
  DNS-Auflösung. Der anschließend genehmigte Download gelang. Builds benötigen
  seitdem keinen Netzwerkzugriff für diesen Backendcode.
- Nicht unterstützt und nicht als bestanden gezählt: ISR-Präemption, komplette
  Engine-/Raumtests, ASan/UBSan-Fiberinstrumentierung, weitere Android-Versionen.

Im endgültigen Stand bestehen alle oben aufgeführten tatsächlich ausführbaren
Tests. Die SDK-Metadatenwarnung CXX5304 und acht Android-Lint-Warnungen bleiben
sichtbar; sie betreffen Tool-/SDK-Versionen und die bereits bekannte Diagnose-App.
Keine geforderte Prüfung wurde durch Erfolgs-Stubs oder ein Lint-Baseline ersetzt.

## Reproduzierbare Befehle und lokale Nachweise

Im Repository und mit installiertem SDK/JDK:

```powershell
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Offline
.\port\scripts\test-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
.\port\scripts\test-app-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
```

Host: VS-Developer-Environment jeweils mit `-arch=x86` und `-arch=x64`, dann
`test-host.ps1 -BuildName host-x86` beziehungsweise `host-ninja`, oder die
in README dokumentierten Ninja-CMake-/CTest-Aufrufe. Keine MSBuild-Umstellung.
Host-CTest-Protokolle liegen unter dem jeweiligen Buildverzeichnis in
`Testing/Temporary/LastTest.log`.

Gerätenachweise, mit ABI, Pointerbreite und Binary-/APK-Hash:

```text
port/build/device-armeabi-v7a.txt
port/build/device-arm64-v8a.txt
port/build/instrumentation-armeabi-v7a.txt
port/build/instrumentation-arm64-v8a.txt
port/android/app/build/outputs/apk/debug/app-debug.apk
port/android/app/build/outputs/apk/androidTest/debug/app-debug-androidTest.apk
port/android/app/build/reports/lint-results-debug.html
```

Finale Debug-APK, SHA-256:
`02C759251D15D4F11614E16163B6C6F023B63FE792C4E396A8CE0960CB58324A`.
ARM32-Executable:
`F1097B80A8F42617C7425FBFF437DC9ED5B0908E1FECBF10F0D2DED9863C43E0`.
ARM64-Executable:
`D9DD4D9CBCC26DFC060D398200ABFD8DA1D573288C984295A003A420F6770517`.
Buildprodukte und Logs sind lokal vorhanden und werden durch port/.gitignore
vom öffentlichen Repository ausgeschlossen. Debug-Hashes beschreiben diese
lokalen Artefakte, keinen Anspruch auf byteidentische Builds an anderen Pfaden.

## Vollständige Projektdateien dieses Meilensteins

Die folgenden Dateien liegen vollständig im Projekt, nicht nur als Diffs.
Neue Dateien:

```text
port/.gitattributes
port/cmake/ContextSources.cmake
port/compat/include/re4_scheduler_original.h
port/include/re4/port/scheduler.h
port/src/scheduler.cpp
port/src/stack_context.h
port/src/stack_context.cpp
port/src/scheduler_verification.h
port/src/scheduler_verification.cpp
port/scripts/test-app-device.ps1
port/docs/scheduler-design.md
port/docs/phase2-status.md
port/third_party/boost_context/README.md
port/third_party/boost_context/LICENSE_1_0.txt
port/third_party/boost_context/make_arm_aapcs_elf_gas.S
port/third_party/boost_context/jump_arm_aapcs_elf_gas.S
port/third_party/boost_context/make_arm64_aapcs_elf_gas.S
port/third_party/boost_context/jump_arm64_aapcs_elf_gas.S
```

Geänderte vorhandene Dateien:

```text
port/CMakeLists.txt
port/README.md
port/cmake/OriginalSources.cmake
port/include/re4/port/contracts.h
port/src/verification.cpp
port/tests/test_main.cpp
port/tests/source_guard_negative.cmake
port/scripts/test-device.ps1
port/android/app/build.gradle.kts
port/android/app/src/main/java/org/re4/port/FoundationActivity.java
port/android/app/src/androidTest/java/org/re4/port/OriginalCodeTest.java
port/docs/abi-strategy.md
port/docs/runtime-boundaries.md
```

Nur im Buildverzeichnis erzeugt: original/scheduler_control.cpp und
original/include/scheduler_methods.inc. Beide werden reproduzierbar aus den
geprüften Originaldateien generiert. Das ist keine manuell gepflegte Kopie der
Spiellogik. Die Boost-Lizenz und Quellprovenienz sind enthalten.

## Verbleibende Fragen und nächster begrenzter Schritt

Nachgewiesen ist die kooperative Laufzeit. Technische Annahme für die spätere
Engine-Anbindung: jeder Modultoken umfasst die gesamte Task-Aufrufkette und
ihre Datenreferenzen; dies muss der tatsächliche Registry erst erfüllen.
Offen sind ISR-Präemption, vollständige Event-Cancel-/GX-/Ladesynchronisation,
transitive Modullaufzeiten, realer Engine-Stackverbrauch und PPC-Vergleich.

Als nächster Schritt innerhalb der Scheduler-Arbeit sollte die Grenze des
Hintergrundslots anhand von iTaskScheduler/iTaskSuspend, decodeData/Yz2DecodeExec
und DvdReadProc separat spezifiziert und getestet werden. Danach können die
echten Callback-Signaturen und Taskreferenzen zentral an die geprüfte Runtime
gebunden werden. Hier wurde weder dieser Hintergrundpfad vorgetäuscht noch die
nächste große Phase begonnen.

MSAA Aus/2x/4x, AF 2x/4x/8x, DoF Ein/Aus mit Nahe-/Zielfokus, persistente
Einstellungen, backendneutrales Menü und Hardwarevalidierung bleiben verbindlich
gemäß [graphics-settings.md](graphics-settings.md). Keine Grafikfunktion wurde
in diesem Meilenstein implementiert.
