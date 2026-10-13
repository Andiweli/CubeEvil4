# Phase 2: Original-Scheduler und native Stacklaufzeit

Ergänzung Phase 2b: Slot 4 besitzt jetzt eine getrennte, begrenzte API. Die
historische Phase-2-Geräteprüfung bleibt erhalten; aktueller Entwurf:
[background-design.md](background-design.md).

## Entscheidung und belegte Originalsemantik

Eine gemeinsame C++-Scheduler-Policy steuert echte, getrennte Stacks auf einem
festen Engine-Thread. ARM32 und ARM64 verwenden die passenden, unveränderten
`make_fcontext`/`jump_fcontext`-Assembler aus Boost.Context 1.88.0. Windows x86/x64
verwendet Windows-Fibers mit FP-Sicherung. Es gibt keinen OS-Thread pro Spieltask.
Nicht unterstützte Pfade erzeugen Fehler; sie werden nicht erfolgreich simuliert.

Die folgenden Aussagen stammen aus den tatsächlichen Funktionskörpern, nicht
allein aus Kommentaren oder aus den Zahlen der ursprünglichen OS-Prioritäten:

| Originalquelle | Nachgewiesene Regel | Native Umsetzung |
| --- | --- | --- |
| `src/game/scheduler.cpp:TaskScheduler` (76) | Hauptslots 0,1,2,3; Slot 4 übersprungen | `NativeScheduler::runMain` |
| `TaskSchedulerMain` (102) | Kind-Gates zuerst, danach exakter Status; SUSPEND verhindert den Switch | `Impl::visit` |
| `TaskSleep` (220) | 0 unterbricht nicht; int wird u16; Countdown erst bei weiteren Scheduler-Besuchen | `NativeScheduler::sleep`, u16 `SleepCtr` |
| `TaskSuspend/TaskSignal` (306/315) | Verschachtelter u16-Zähler und Statusbit 0x80 | Unverändert extrahierte Originalfunktionen |
| `TaskChain/TaskExit` (242/258) | Nicht zurückkehrende Stackbeendigung; Chain im gleichen Slot erst beim nächsten Besuch | Rücktransfer zum unmittelbaren Caller, danach Freigabe |
| `TaskExec_hook` (173) | Haupttask suspendiert Parent; arg wird unzulässig auf int verkürzt | Stacktransfer und exakte typisierte Bindung |
| `src/game/sce_sys.cpp:SceExec` (250) | Direkter Aufruf bei Level 0; Autoallokation 17..6, Slot 5 ausgeschlossen | `execScenario` |
| `src/game/libgpu.cpp:AddPrim/ClearOTagR` (23/46) | Head-Insertion, rückwärts verkettete 16 OT-Einträge | Native Handle-Buckets, 15..0, LIFO |
| `src/game/sce_sys.cpp:cSceSys::scheduler` (181) | Nach jedem Task Neustart am Head; pro Slot exec_flag; bei Item/Chapter-Hold nur Slot 5 | `runScenarios`, Flag bleibt bei Slotreuse im gleichen Pass erhalten |
| `SceSleep/SceExit/checkCTaskRange` (316/324/366) | SceSleep und SceExit wirken nur in Slots 5..17 | Unverändert extrahierte Originalfunktionen |

Die Priorität `Priority` bleibt als u8-Metadatum erhalten. `TaskExec` setzt 0xF,
`SceExec` 0xE. Die ursprüngliche Semaphore bei Prioritäten >0xF unterstützt den
OS-Handshake; sie ist kein Auftrag, Hauptslots nach Priority zu sortieren. Der
native synchrone Stacktransfer stellt den gleichen Ausführungsabschluss bis zur
nächsten Unterbrechung her, ohne eine Schein-Semaphore einzuführen.

Kind-Bits stammen aus den originalen TASK-Methoden: 2 erlaubt Bewegung während
STA_SUSPEND, 4 während STA_DIEDEMO. Beide Getter/Setter und setKind/getKind werden
verbatim übernommen. Die Standardwerte und die Szenario-Kind-Vererbung folgen
SceExec. Pause/Gates werden derzeit über explizite Runtime-Methoden übergeben;
die vollständige Anbindung an pG und Szenario-Subsysteme ist noch erforderlich.

## Vergleich der Kontextmechanismen

| Mechanismus | Eignung für Originalcode | Entscheidung |
| --- | --- | --- |
| Stackful fcontext/Fibers | Erhält verschachtelte synchrone Aufrufe, lokale Variablen, Rücksprungadressen und ABI-Register; ein Owner-Thread | Gewählt und auf beiden Android-ABIs ausgeführt |
| OS-Thread pro Task mit Handshake | Könnte die Dolphin-Grundform abbilden, benötigt Synchronisation und zusätzliche Threads; löst sichere Android-Präemption nicht automatisch | Für kooperative Spieltasks nicht erforderlich |
| C++-Coroutines/Zustandsautomaten | Gewöhnliche verschachtelte C++-Aufrufe können nicht unverändert über stacklose Unterbrechungen erhalten werden | Erfordert umfangreiche Änderungen am Originalcode; verworfen |
| setjmp/longjmp plus eigener Stackwechsel | ISO-C erzeugt damit keinen neuen Taskstack; ABI-Assembler, FP-Register und Stackinitialisierung müssten selbst entwickelt werden | Kein Vorteil gegenüber geprüftem Kontextbackend |
| ucontext | Keine gemeinsame unterstützte Basis aus den getcontext/makecontext/swapcontext-Deklarationen in den lokal geprüften NDK-r28c-Headers ableitbar | Kein unbewiesener Android-Fallback |

Primärquellen zum gewählten Mechanismus:
[Boost.Context-Übersicht](https://www.boost.org/doc/libs/1_88_0/libs/context/doc/html/context/overview.html),
[versionierter ARM-Assembler](https://github.com/boostorg/context/tree/boost-1.88.0/src/asm),
[Windows-Fibers](https://learn.microsoft.com/en-us/windows/win32/procthread/fibers).
Die konkrete Eignung für RE4 wurde aus den oben genannten Originalfunktionen
abgeleitet und anschließend durch die gemeinsame Testsuite überprüft.

## ABI, Stack und Zustand

`re4_scheduler_original.h` definiert ausschließlich native Kontrollmetadaten.
Es ist keine Replik des 0x348-Byte-Dolphin-TASK und enthält keine erfundenen
OSThread-Objekte. Weder TASK noch SCE_TASK dürfen aus Disc-Dateien hinein kopiert
werden. Native Pointer bleiben Pointer, Stacks verwenden size_t, Handles und
Modullaufzeiten uint64-Generationen. Die originale OT-Adresse im u32-Tag mit
0x80000000 ist vollständig aus der nativen Taskverkettung entfernt.

`bindTask` bindet die tatsächliche Funktionssignatur. `CardMainTask(int)` aus
card.cpp und `gameDiedemo(DIEDEMO_WORK*)` aus game.cpp können nicht durch dieselbe
void(int)-Umdeutung aufgerufen werden. Bereits existierende TaskFunc-Casts und
TASK*/SCE_TASK*-Speicher müssen bei der Einbindung weiterer Originaleinheiten
an ihren tatsächlichen Deklarationen angepasst werden. Diese Einheiten sind
noch nicht Teil des Builds; die Testcallbacks sind Infrastrukturproben.

Stacks entstehen bei TASK_EXEC, nicht beim Vormerken. Default: 256 KiB, zulässige
Konfiguration 64 KiB..16 MiB. Android nutzt mmap mit zwei PROT_NONE-Guard-Pages
und einem RW-Bereich; die tatsächliche Seitengröße kommt aus sysconf. Native
Stackbudgets sind bewusst nicht die ursprünglichen 6/8/12 KiB. GetStackSize
wird unverändert verifiziert, bestimmt aber keine native Stackallokation.
Nach Yield ist immer der unmittelbare Caller zuständig; damit kann der Game-Task
eine Szenario-Iteration ausführen und anschließend selbst schlafen.

ARM32-Assembler wird in ARM-Mode kompiliert; Thumb-C++-Caller interworken über
die ABI. Nur für diese zwei Assemblerdateien wird __SOFTFP__ entfernt, damit die
upstream-bedingten d8-d15-Saves aktiv sind. Die C++-Softfp-Aufrufkonvention bleibt
unverändert. ARM64 sichert x19-x30 und die ABI-relevanten 64 Bit von d8-d15;
Androids reserviertes x18 wird nicht verändert. Android-FP-Control/Status wird
zusätzlich als fenv pro Kontext gesichert; Windows-Fibers übernehmen dies selbst.

Sleep behält den Stack einschließlich lokaler Variablen und tieferer Calls.
Cancel, TaskChain und explizites TaskExit verwerfen den alten Stack abrupt,
ohne automatische C++-Destruktoren auszuführen, wie die ursprünglichen OS-
Kontrolltransfers. Die Runtime-Entry-Captures und der Stack selbst werden erst
nach Rücktransfer auf dem Callerstack freigegeben. Neue Portressourcen müssen
außerhalb verworfener Stackframes verwaltet werden. Normale Rückkehr aus einem
Szenariotask sowie Exceptions rollen dessen C++-Frames dagegen regulär ab.

## Referenzen und Modulwechsel

TaskHandle enthält Runtime-ID, Slot und Generation. Ein freigegebener Handle
kann keine später neu belegte Instanz suspendieren, fortsetzen oder töten.
TaskChain bleibt eine Fortsetzung desselben logischen Tasks und behält daher
den Handle, ersetzt aber den Stack. Ein laufender Caller/Ancestor kann vom Child
nicht zerstört werden. Alle kontextführenden Methoden prüfen den Owner-Thread.
Phase 2b erlaubt zusätzlich ausschließlich atomare Hintergrundstatus-/Pause-/
Cancel-Requests von fremden Threads; diese führen keinen Stackwechsel aus.

Modultoken sind Laufzeitidentitäten, weder FileTbl- noch REL-IDs. Vor einem
Modulwechsel prüft invalidateModule alle aktiven Task-/Caller-Stacks und direkte
Callbacks. Bei Aktivität wird false geliefert und nichts freigegeben. Bei
Quieszenz wird das Token zuerst gesperrt, dann werden seine pending/sleeping
Tasks und OT-Links entfernt. Destruktoren der Entry-Captures können dadurch
keine neue Arbeit in einem bereits stillgelegten Token anmelden.

Das ist eine nachgewiesene Freigabegrenze für korrekt zugeordnete Tasks, kein
fertiger REL-/ELF-Loader. Ein tatsächlicher Registry muss auch alle transitiven
Cross-Modul-Aufrufe und Datenreferenzen einer Tasklebensdauer zuordnen. Direkte
Scenario-Aufrufe innerhalb einer Task können keine fremde Modullaufzeit übernehmen.
Bis diese Abhängigkeiten vollständig registriert sind, muss nativer Code resident
bleiben. Die Scheduler-API allein ist keine Genehmigung zum dlclose beliebiger
Module.

## Explizite Grenzen und Fehlerbehandlung

- Allgemeine exec/visit-Aufrufe für Slot 4 werden weiter abgelehnt. `iTaskSuspend` in
  scheduler.cpp:394 unterbricht einen laufenden Thread aus dem VI-Callback;
  `read.cpp:decodeData` läuft durch Yz2DecodeExec bis iTaskExit ohne kooperativen
  Sleep. Ein normaler Fiber würde damit das VSync-Budget nicht erhalten. Die
  Phase 2b liefert den separat geprüften Weg mit Checkpoints in Setup, Decoder
  und Lese-Pumpe; der vollständige VI-/Frameadapter bleibt offen.
- Die vollständige cSceSys-Klasse, ScenarioMove/RoomInit, EventCancel,
  cancel_flag/event_cancel_enable, Lade- und GXDrawDone-Wartepfade sind nicht
  simuliert oder als fertig portiert bezeichnet. Native SceTaskDelete entfernt
  den echten nativen OT-Link; die Event-Cancel-Metadaten müssen bei der späteren
  Einbindung von cSceSys ergänzt werden.
- Ein Haupttask, der ohne TaskExit/Chain zurückkehrt, ist im Original kein
  funktionierender kooperativer Abschluss: sein Parent bleibt suspendiert.
  Native Ausführung meldet diesen Fall als Fehler und räumt den Stack auf.
  Szenario-Rückkehr wird gemäß dem Original nach dem Dispatch freigegeben.
- TaskKill eines aktiven fremden Ancestors wird abgelehnt. Der ursprüngliche
  RUN-Zweig ruft TaskExit auf dem aktuellen Task auf; diese unsichere
  Fehlverwendung wird nicht reproduziert. Self-Kill beendet dagegen den Caller.
- API 21 ist die Builduntergrenze; tatsächliche Gerätetests erfolgten auf API 33.
  Weitere Geräte, 16-KiB-Page-Systeme, PAC/BTI und vollständige Engine-Stacklast
  sind noch nicht durch diese Geräteprüfung abgedeckt.
- Keine ASan-Fiber-Hooks, kein Thread-Migration-/Signal-Preemption-Support,
  keine PowerPC-Referenztrace und kein Gameplay-/Raumtest werden behauptet.

Die Grafikanforderungen aus `graphics-settings.md` bleiben unverändert:
backendneutrales Laufzeitmenü, MSAA Aus/2x/4x, AF 2x/4x/8x, DoF Ein/Aus mit
Nähe/Ziel-Fokus, persistente Einstellungen und echte Capability-Validierung.
Diese Phase implementiert keine Grafikfunktion.
