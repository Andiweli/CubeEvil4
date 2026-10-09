# Phase 2b: Hintergrundslot 4, Datei-I/O und Yz2

## Belegte Originalsemantik

Die tatsächlichen Originaldateien bleiben unverändert. Zusätzliche normalisierte
SHA-256-Fingerprints schützen Decoder, Python-Referenz und Frame-/Ladeablauf.

| Quelle / Funktion | Nachgewiesener Ablauf | Native Umsetzung |
| --- | --- | --- |
| `src/game/main.cpp:167–195` | TaskScheduler, ID/Trans, Dvd.Watcher, weitere Watcher, iTaskScheduler, danach Render-/VSync-Warten | Hintergrund-Slice erst am Frameende |
| `main.cpp:postVSyncCallback` (226) | Ab erreichtem VSync-Zähler iTaskSuspend | Atomarer Request; kein fremder Stackwechsel |
| `scheduler.cpp:iTaskScheduler` (347) | Nur bei aktivem Flag; Slot 4 ohne Parent; SUSPEND löschen; TaskSchedulerMain; Caller zurücksetzen | Eigener erhaltener Stack, Fortsetzung je Frame |
| `iTaskExec` (364) | Nur bei Flag 0; TaskExec(4), Priorität 0, kein Argument | Genau ein Auftrag; Start führt Callback noch nicht aus |
| `iTaskKill/iTaskExit/iTaskStatus` (379/386/406) | Kill/Exit heben aktives Flag auf; Status ist das Flag | Atomare Aktivität, Freigabe/Abschluss auf Ownerstack |
| `iTaskSuspend` (394) | VI setzt SUSPEND und stoppt laufenden OS-Thread | Sichere Checkpoints statt erzwungenem Thread-Suspend |
| `read.cpp:decodeData` (119) | Header lesen, gemeinsame Heap-/Roomzustände ändern, Yz2DecodeExec, iTaskExit | Room-/Heapintegration bleibt auf Engine-Owner |
| `ReadAreaData` (147 ff.) | DVD-Lesen abwarten, Decoder starten, iTaskStatus mit TaskSleep(1) abfragen, danach GetDataExt | Abschluss darf erst nach Veröffentlichung sichtbar sein |
| `dvd.cpp:DvdReadProc/cDvd::ReadProc` (1170/1178) | Queue abarbeiten, Zeiger/Flags abschließen, iTaskExit oder TaskExit | Echte Terminalzustände und eigener I/O-Abbruch-Handshake |
| `cDvd::readProcMain` (1204) | Read() wiederholen; Intr-Pfad ohne TaskSleep | Intr-Polling braucht begrenzten Yield |
| `cDvd::Watcher` (1339) | iTaskExec oder Slot 3; Queue erst nach erfolgreichem Exec veröffentlichen | Callbackstart muss bis zum Scheduler verzögert bleiben |
| `ReadNblk2Blk/ReadCancelAll` (1221 ff./1390 ff.) | Hintergrundtask stoppen; DVD-Kommando separat behandeln | Taskabbruch ersetzt keine I/O-Abbruchbestätigung |

## Entscheidung

Ein dedizierter Slot-4-Worker könnte Yz2 parallel dekodieren, würde aber bei
unveränderten decodeData-/DVD-Callbackkörpern pG, Heap und Queue mit dem
Spielthread teilen. Zusätzliche Sperren müssten diese gesamte Nutzung abdecken
und könnten Frames blockieren. Erzwungenes Suspendieren könnte Allocator-/Mutex-
Zustände anhalten. Dieser Mechanismus wird nicht verwendet.

Slot 4 nutzt deshalb das bereits geprüfte stackerhaltende Backend auf demselben
Engine-Owner wie die Spieltasks. Checkpoints in C-Setup, PPC-Kernelübersetzung
und Lese-Pumpe geben Kontrolle an das Frameende zurück. Verschachtelte Aufrufe
und ihre lokalen Variablen bleiben erhalten. Gameplay- und Szenario-Reihenfolge
werden nicht umsortiert. Eine Infrastrukturprobe prüft die Framefolge explizit.

Nur tatsächliches Betriebssystem-Dateilesen läuft auf einem einzigen dauerhaften
AsyncFileReader-Worker. Er liest in 64-KiB-Schritten in FileRead-eigene Speicher,
publiziert atomaren Fortschritt/Terminalstatus und ruft keine Enginefunktion auf.
read() verwendet try_lock und nimmt nur einen offenen Auftrag an. Ein belegter
Reader muss in einem späteren Frame erneut angefragt werden; es gibt keine
versteckte parallele Queue. startFileReadPump pollt diesen Zustand einmal pro
Slice und fordert bei Taskabbruch nichtblockierend I/O-Cancel an.

Ein blockierter Dateisystemaufruf kann erst nach seiner Rückkehr abbrechen. Bis
zur Bestätigung besitzt der Worker seinen Puffer selbst; ein neuer Read wird
erst nach Freigabe angenommen. Nur beim endgültigen Reader-Shutdown wird gejoint.
Ein späterer SAF-/Asset-FD-Adapter muss dieselbe Lebensdauer bewahren.

## Zustände, Budget und Sicherheit

execBackground vormerkt Slot 4. runBackground gehört an das Frameende; direkte
visit(4)/allgemeine exec(4) sind unzulässig. Die iTask-Namen sind typisierte
Adapter in re4_scheduler_original.h; kein TaskFunc-Cast oder Pointer-zu-int-Pfad.

| Ereignis | Kontrollmetadaten | Aktiv |
| --- | --- | --- |
| Exec akzeptiert | TASK_EXEC, Priority=0, Kind=6 | 1 |
| Dispatch | TASK_RUN; Stack entsteht | 1 |
| Budget/Retrace-Checkpoint | TASK_RUN \| TASK_SUSPEND | 1 |
| Nächstes Frameende | SUSPEND löschen, Stack fortsetzen | 1 |
| TaskSleep(n) | TASK_SLEEP, originaler u16-Countdown | 1 |
| iTaskExit | Owner übernimmt Stackfreigabe und Completed-Veröffentlichung | danach 0 |
| Kill/Modulwechsel/Cancel | Stack sicher verwerfen, Cancelled veröffentlichen | danach 0 |
| Exception/unerwartete Rückkehr | Failed veröffentlichen, Slot freigeben, Fehler auf Owner melden | danach 0 |

Standardbudget: 1.048.576 Einheiten oder 2 ms bis zum nächsten Checkpoint. Das
ist eine erste Ressourcenentscheidung, keine gemessene Performancegleichheit.
Dictionary-Setup bearbeitet zwischen Checkpoints höchstens eine 0x1004-Byte-
Zeile; Lookup/Kopie prüfen mindestens alle 256 Einträge/Bytes. Allocator-/OS-
Latenz ist damit keine garantierte Echtzeitgrenze. Die vollständige Frame-
integration und reale Archive müssen später profiliert werden. Deterministische
Tests setzen feste Einheiten ohne Zeitdeckel; ein eigener Test prüft den Deckel.

TASK-Metadaten bleiben auf dem Owner. Nur backgroundActive und die expliziten
Suspend-/Cancel-Requests sind cross-thread-fähig. Runtimelebensdauer muss durch
die Callbackanbindung gesichert werden. Generationen verhindern, dass verspätete
Requests eine neue Slotbelegung stoppen. Acquire/Release ist auch auf ARM32
explizit. Inaktiv wird erst nach Ergebnis-/Cancelcallback und dessen Freigabe
publiziert. Der Callback darf nicht werfen oder denselben Slot sofort ersetzen.

Abrupter Taskabbruch führt wie im Original keine automatischen Stackdestruktoren
aus. Portressourcen gehören in Entry-/Completion-Captures und externe Jobobjekte.
Decoderframes enthalten nur triviale Kontext-/Schleifenzustände. std::function-
Quellobjekte werden nach Move ausdrücklich geleert: libc++ darf bei kleinen
Callbacks eine Kopie behalten. Stack und Captures werden auf dem Owner freigegeben.

## Originaler Yz2-Code und notwendige PPC-Übersetzung

cmake/Yz2Sources.cmake generiert Yz2DecodeSet/Yz2DecodeExec und die Setup-Makros
aus src/game/yz2code.cpp. Benannte Änderungen: per-Instanz-in_ev-Parameter,
uintptr_t-Ausrichtung statt Pointer-Trunkierung, native Dictionary-Arithmetik,
begrenzter Byte-/Bumpzugriff und Checkpoints. Modellinitialisierung und ihre
Rechenausdrücke bleiben aus der Originaldatei erhalten.

Tabellen benötigen 7.680+1.536 Bytes Frequenz/Cum, 2×0x20000 Bytes Lookup sowie
256×0x1004 Bytes Wörterbuch: **0x142800**, passend zu READ_BUFF_OFS in read.cpp.
Die kommentierten PPC-Pointeroffsets werden nicht als Native-Layouts behauptet.
Nur die pointerfreie Wörterbuchzeile behält nachweislich Größe 0x1004.

yz2_kernel.cpp übersetzt die tatsächlichen Integeroperationen aus yz2asm.cpp:
Halbrange-Coder, 1/2/3-Byte-Normalisierung, zwei adaptive Modelle, Cum vor der
Frequenzhalbierung, 512er-Ringe, gespeicherte/zusätzliche Längen und überlappende
Bytekopie. Dictionary-Einträge enthalten geprüfte 32-Bit-Ausgabeoffsets statt
Pointer. Arbeitswörter sind native Werte; zusätzliche Längen werden explizit
byteweise in der originalen Reihenfolge zusammengesetzt. Ungültige Codes,
unbeschriebene Referenzen und Pufferüberschreitungen liefern Fehler.

Vor strtoul werden beide Textfelder und die originale 32-Byte-Ausrichtung
innerhalb des tatsächlichen Inputs validiert. Das erste Größenfeld begrenzt
den Decoder nicht, da der Originaldecoder es dafür nicht nutzt. Seine weitere
Containerbedeutung wird nicht erfunden. Default 64 MiB ist ein explizites natives
Ressourcenlimit und kein behauptetes Originalformatlimit.

Drei selbst erzeugte Fixtures werden durch das unveränderte tools/motion/yz2.py
geprüft. Manifest-SHA-256 schützt Eingabe und vollständige Referenzausgabe. Native
Tests vergleichen jeden Byte sowie Decoderverbrauch und Modellzustände: alle
Literale, Überlappung, gespeicherte/16/24/32-Bit-Längen, beide Modellhalbierungen,
Ringumlauf, synchron/gesliced und zweite Instanz zwischen Hintergrund-Slices.
Diese Daten sind keine GameCube-Assets.

## Offene Integration

main.cpp/VI/Surface sind noch nicht vollständig eingebunden; die Framefolge
wird als Infrastrukturprobe ausgeführt. Ein späterer VSync-/Lifecycle-Adapter
muss Slice und atomaren Request mit kontrollierter Runtimelebensdauer verbinden.
Die [Android-Game-loop-Dokumentation](https://developer.android.com/games/develop/gameloops)
beschreibt Choreographer/VSync als Zeitquelle, ersetzt aber keine RE4-Semantik.

decodeData benötigt Mem, pG->pRoom und die Archivpipeline und wird noch nicht
als vollständiger Callback kompiliert. DvdReadProc wird unverändert als
**isolierte Adapterprobe** gegen einen Native-Delegate kompiliert. cDvd-Queue,
Retry-/Discwechsel-/ARAM-/DVD-SDK-Zustände bleiben offen. Originalarchive und
GameCube-Referenzläufe sind für breitere Differentialprüfung erforderlich.
Kein Raum, Gegner, Renderer oder Assetimport gehört zu diesem Meilenstein.
