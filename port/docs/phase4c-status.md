# Phase 4c – Gemeinsame CPU-Consumer und sichere Raumfreigabe

Stand: 10. Oktober 2026. Geprüfter **Teilstand**, ausgehend von
`2f9c7db6a30881089d60b4977eb8e4bf08d40f7d` auf `android-port`.
Alle Änderungen sind als vollständige Dateien unter `port/` gespeichert.

Die gemeinsame Laufzeit beendet bekannte Raum-Tasks und Modul-Borrower vor der
Freigabe ihrer Consumer und ihres Work-Speichers. Aus den Originalquellen
extrahierte Modell-, Message-, Kamera- und Eventmethoden laufen auf Windows
x86/x64 und dem Retroid unter ARM32/ARM64. Die beiden echten r120-EVDs werden
über den vorhandenen EngineLoader geladen, geprüft und mit nativen CPU-Ansichten
verbunden. Die originalen r120-Abbruchcallbacks setzen das originale Szenariobit.

**Der vollständige Originalspielstart ist nicht erreicht.** `GameTask` stoppt an
`gameInit/ClothInit`; die separat aufgerufene originale `R120Init` stoppt an
`TexRenderInit`. Vollständige Modellinitialisierung, Kameraentscheidungen,
`Event::init`/`Run`/Paketwiedergabe und der originale Übergang nach r100 bleiben
offen. Die nachstehenden Prüfergebnisse dürfen nicht als erfolgreich gespieltes
Intro oder vollständig integrierte Phase 4c verwendet werden.

## Git, Daten und Ausgangslage

| Referenz | Unveränderter Ausgangsstand |
| --- | --- |
| `android-port` vor Phase 4c | `2f9c7db6a30881089d60b4977eb8e4bf08d40f7d` |
| lokaler `master` | `9b76ff003081d6ece8b838453fb8310743a972c7` |
| `origin/master` | `15cf64f80e08bbc64712c39be330b75d1512a0d0` |

Der Arbeitsbaum war anfangs sauber. Die README-Ergänzungen aus `origin/master`
wurden berücksichtigt: Wiederverwendung des Originalcodes, getrennte
Portstruktur, keine enthaltenen Assets und noch kein spielbarer Port. Die
README-Differenz wurde nicht automatisch zusammengeführt. Es gab keinen Merge,
Rebase oder Push. Der Abschlusscommit ist ein lokaler Nachfolger des bisherigen
Phase-4b-Stands; seine ID ist über `git log -1` und den Abschlussbericht der
Sitzung sichtbar. Master bleibt unverändert.

Die ursprünglichen Dateien unter `src/`, `include/`, `tools/`, `docs/` und
`config/` wurden nicht verändert. Quelle der echten Testdaten ist ausschließlich
`E:\Development\Android\RE4-Data\import-phase3a`. Die Images und importierten
Blobs wurden nicht verändert. Neue private Kataloge und Berichte liegen unter:

- `E:\Development\Android\RE4-Data\phase4c-host`
- `E:\Development\Android\RE4-Data\phase4c-device`
- `E:\Development\Android\RE4-Data\phase4c-modules-host`
- `E:\Development\Android\RE4-Data\phase4c-modules-device`
- `E:\Development\Android\RE4-Data\phase4c\consumer-runtime-summary.json`

Es wurden keine GameCube-Assets in Git oder die APK aufgenommen. Die finale
APK enthält beide JNI-Bibliotheken und kein `assets/`-Verzeichnis. Ihr SHA-256
laut ZIP-Prüfung und beiden Installationsprotokollen ist
`0a6f67e0a4948473113d08f5788c7e5fb0aada3f47ee3db72cdc9bc28af37434`.

## Herkunft des ausgeführten Codes

`tools/generate_consumers.py` prüft zehn Originaldateien gegen
`config/consumer-sources.json` und extrahiert 13 komplette Funktionskörper.
`tools/generate_runtime.py` prüft jetzt 20 Dateien und extrahiert insgesamt
38 Funktionskörper sowie die bestehende Original-PADClamp-Einheit. Der
Modulgenerator erhält zusätzlich die r120-Quelle und kennzeichnet ihre
Resident-Abhängigkeiten ausdrücklich. Die Originaldateien werden nicht kopiert
und anschließend von Hand umgeschrieben: Die Buildgeneratoren entnehmen die
geprüften Körper und protokollieren notwendige Anpassungen.

Die buildlokalen Dateien `original/consumer-manifest.json`,
`original/runtime-manifest.json` und `original/module-manifest.json` enthalten
Quellpfade, Zeilen, Signaturen, Fingerprints und Adaptierungen. `#line` verweist
auf die tatsächliche Originalquelle. Ein negativer Test verändert eine
temporäre Modellquellkopie und beweist die Ablehnung eines anderen Fingerprints.
Die vollständigen alten Engine-Translation-Units gelten damit nicht als nativ
portiert.

| Originalquelle / Funktion | Nachgewiesene Ausführung und Grenze |
| --- | --- |
| `src/game/model.cpp:219`, `setPartsParent`; `:258`, `setNoSuspend`; `:268`, `isTrans`; `:280`, `getPartsPtr`; `:481`, `updateOldPos` | Originale Hierarchieverknüpfung, Teilezugriff, Suspend-/Transparenzbits und Positionshistorie über geprüfte native CPU-Ansichten. Hierarchien aus 37 echten BINs sowie isolierte Fehlerfälle. Kein vollständiges `cModel::modelInit`. |
| `src/game/mes.cpp:239`, `MessageData::getAddr`; `:251`, `getMesNum`; `:260`, `getSpaceWidth` | Originale englische Tabellenadressierung und Zählung nach begrenztem BE-Import. Originaltextbytes bleiben erhalten; Textsteuerung, Layout und Fonts fehlen. |
| `src/game/cam_ctrl.cpp:208`, `Disable`; `:215`, `AreaCheckOnOff`; `:234`, `CurrentAreaNo`; `:240`, `CurrentCameraNo` | Originale skalare Kamerazustände. Die Zustandstests sind isoliert; echte CAMs werden separat geprüft. Keine ursprüngliche Flächenwahl, Kamerabewegung oder Interpolation integriert. |
| `src/game/event.cpp:1609`, `Event::ExeFunc` | Originale NoFunc-/CancelExe-Gates, Namensbildung, Callbacksuche und Modusübergabe. Tatsächliche r120-Callbacks im Abbruchmodus 3; Schlaf-/Lifecycle-Prüfungen mit ausdrücklich isoliertem Callback. Kein `Event::init`/`Run`. |
| `src/game/sce_com.cpp:1401`, `DebugTrg(int)` | Tatsächlicher Originalkörper liefert 0; kein erfundener Debug-Eingabeweg. |
| `src/game/game.cpp:1767`, `gameDebug` | Gesamter Originalkörper wird kompiliert. Normaler Bootzustand erreicht den folgenden `gameInit`-Dispatch. Debug-/Shooting-Playerzweige bleiben benannte fehlende Consumer. |
| `src/st1/r120.cpp:45`, `R120Init` | Originale Work-Allokation, Registrierung beider Eventcallbacks, DebugTrg-Entscheidung und SceExec-Anforderung; dann kontrollierter Fehler an `TexRenderInit/GX render target`. Fehlerbereinigung wird am nächsten Frame-Rand ausgeführt. |
| `src/st1/r120.cpp:58`, `R120Main` | Vollständig übernommener leerer Originalkörper; kein Ersatzraumcode. |
| `src/st1/r120.cpp:63`, `R120Event` | Separater Test führt den ersten originalen `SceSleep(1)` mit erhaltenem Stack aus. Nach Fortsetzung folgt der kontrollierte Fehler `systemVISetBlack/presentation adapter`. Movie, Eventstart und RoomJump wurden nicht ausgeführt. |
| `src/st1/r120.cpp:130,261`, `Evt_R120S00_Func`, `Evt_R120S01_Func` | Ganze Körper kompiliert; tatsächlich nachgewiesen ist ihr jeweiliger Modus-3-Zweig mit `ScfFlagOn(SCF_R120_EVENT_CANCEL)`. Andere Modell-/Licht-/HUD-/Renderzweige nicht als ausgeführt zählen. |
| `src/st1/r120.cpp:104,462,560`, `r120_setTrans`, `EventCarInit`, `EvtTexRenderCamTrans` | Ganze Körper kompiliert, benötigte Modell-/Motion-/Licht-/TexRender-Consumer fehlen. Keine Behauptung erfolgreicher Ausführung dieser Zweige. |
| bestehende `StageSet`, `readEmList`, Szenario-, Pad-, Scheduler- und Ladefunktionen | Erhalten und erneut getestet. Originale Orchestrierung mit isoliertem SharedScenarioConsumers-Prüfobjekt bleibt von produktiver Actor-/Eventinitialisierung getrennt. |

Die neuen Adaptierungen ersetzen Klassenbesitzer durch native Kontextbesitzer,
`void*`-Funktionspointer durch typisierte Callbacks und 32-Bit-Adressoperationen
durch geprüfte Offsets/native Pointer. EVD-Namen werden vor den originalen
`strcpy`/`strcat`-Aufrufen begrenzt. `R120Init` verwendet typisierte
Placement-Konstruktion im vorhandenen Modulheap; nichttriviale Work-Destruktoren
werden von diesem Adapter nicht akzeptiert. Der unbenutzte Parametername `cut`
in `EvtTexRenderCamTrans` wird für Clangs Warnungsprüfung entfernt. Die
Spielentscheidungen in den übernommenen Körpern bleiben erhalten.

## Raumfreigabe, Tasks und Modulzustände

Die gemeinsame Implementierung liegt in `src/game_runtime.cpp`,
`src/modules.cpp` und `src/consumers.cpp`. Lifecycle-Operationen laufen auf dem
Frame-Besitzthread; ein direkter Lifecycle-Aufruf innerhalb eines laufenden
Tasks oder Modulcallbacks wird abgelehnt. Aus Tasks werden Operationen
angefordert und mit dem vorhandenen stackerhaltenden Scheduler abgewartet.

Für bekannte native Raumressourcen ist die Reihenfolge:

1. Szenario-Tasks beenden und die owned/borrowed Stacks des Raummoduls
   quieszent machen; Modul-Lifetime ungültig machen.
2. Raum- und Ladeanforderungen abbrechen. Ein noch arbeitender Dateiworker
   behält seinen privaten Puffer bis zur Abbruchbestätigung.
3. Consumer-Epoch erhöhen, Eventaufruf-Ledger freigeben, Callbacktabelle,
   Event-/Modell-/Message-/Kameraansichten und deren internen Leases lösen.
4. Ursprünglichen Szenarioskalarspeicher zurücksetzen und den Resident-
   `r120_work`-Pointer löschen.
5. Modulwork zurücksetzen beziehungsweise das Modul deaktivieren; erst danach
   darf ein nachfolgender Heapwechsel alte Raumallokationen ersetzen.

`ResetRoom`, `ReleaseRoom`, `RetireRoom` und das Deaktivieren des aktuell
verknüpften Raummoduls verwenden diesen gemeinsamen Weg. Der StageSet-Adapter
fordert ihn vor einem Wechsel eines initialisierten Raums an: beim gleichen
Modul bleibt der Prolog bestehen, beim anderen Modul wird der alte Link
deaktiviert. Die originale `StageSet`-Funktion führt anschließend ihre
ursprünglichen Entscheidungen und ESL-Ladeanforderungen aus. Ein gestopptes
Modul erneut zu aktivieren bedeutet einen neuen Link; `Restart` bedeutet
dagegen Wiederherstellung und erhält dessen Work-/Taskzustand.

Consumer- und Callbackreferenzen enthalten volle native Adressen und 64-Bit-
Identität/Epoch/Index. Die native R120Work-Größe darf ABI-bedingt abweichen:
904 Bytes (`0x388`) unter ARM32, 912 Bytes (`0x390`) unter ARM64. Dies ist eine
native Work-Klasse, kein Overlay des Originaldatenspeichers.

Ein Eventaufruf-Ledger hält die Callbackfunktion außerhalb eines abbrechbaren
Taskstacks. Der Stack leiht lediglich einen Pointer. `clear()` verweigert die
Freigabe, solange ein solcher Callbackstack lebt. Schlafende verschachtelte
Callbacks werden im Modulkontext markiert; Stop/Restart erhält ihre lokalen
Variablen und Sleepzähler, Retire beendet sie vor der Freigabe. Kopierte
Callbacks prüfen Besitzthread, Besitzer und Epoch und lehnen veraltete oder
fremde Aufrufe ab. Ein Scheduler-Signal allein gibt keinen Speicher frei.

Die Verwendung bleibt an den Lifecycle-Vertrag gebunden: rohe C++-Referenzen
auf CPU-Ansichten dürfen keinen Raumwechsel überleben; Modulwechsel in der
gemeinsamen GameRuntime müssen deren Frame-Operationen verwenden. Der separat
verwendbare ModuleRuntime besitzt nicht automatisch alle Resident-Consumer.
Beim späteren Portieren von `gameStageInit` muss dessen StageSet-Bindung den
gemeinsamen Adapter durchlaufen. Der derzeit fehlende Dispatch kann diesen
Weg noch nicht produktiv umgehen.

Externe ResourceLeases dürfen Bytes weiter pinnen. Ein Heapwechsel mit solchen
lebenden Allokationen wird weiterhin abgelehnt und erzwingt keine Freigabe.
Diese implementierte Sicherheit ist kein Ersatz für die noch fehlenden
originalen Player-/Actor-/Sound-/Manager-Destruktoren in `gameDoordemo` und
`gameRoomMemInit`. Sie deckt die bisher registrierten nativen Ressourcen ab.

## Modelle, EVD, Kamera und englische Nachrichten

`EngineConsumers` verwendet unveränderliche ResourceLeases aus dem bestehenden
EngineLoader. Wire-Offsets und Big-Endian-Werte werden mit Bereichsprüfungen
gelesen; keine Binärstruktur wird auf eine ABI-große Pointerklasse gecastet.

Der Modelladapter validiert den originalen BIN-Header, die beiden durch
`cModInfoMgr::create` belegten Versionskennungen, Teilezahl, 16-Byte-Jointtabelle,
Endianness, finite Mittelpunkte und Elternindizes. Zyklen werden vor der
originalen Pointertraversierung abgelehnt. Eine native Registry besitzt stabile
CPU-Objekte und verknüpft die Hierarchie mit `setPartsParent`. Diese Registry
führt nicht die noch fehlende `cModInfoMgr::create`-Implementierung aus.
TPL, Skins, Matrix-/World-Berechnung, vollständige Partsinitialisierung und
MotionSetCore/Playback sind nicht integriert. FCV/SEQ/TPL behalten ihre
originalen Namen, Indizes und Byte-Leases; das ist Referenzerhalt, keine Animation.

Der EVD-Indexer prüft den tatsächlichen Header, Packetbereich, 64-Byte-Bintabelle,
Namen, Größen, Reihenfolge, Padding und Grenzen. Der Packetbereich bleibt
uninterpretiert. Die beiden vom r120-Originalcode referenzierten Pakete wurden
über die universelle Dateianforderung und Hintergrundverarbeitung geladen:

| FST-Datei | Größe | Bin-Einträge | Native CPU-BINs | Englische MDT |
| --- | ---: | ---: | ---: | --- |
| `Evd/r120s00.evd` | 2.515.456 | 94 | 18 | `event/r120/s00/etc/r120s00.mdt`, 9 Einträge |
| `Evd/r120s01.evd` | 2.973.472 | 120 | 19 | `event/r120/s01/etc/r120s01.mdt`, 2 Einträge |

SHA-256 der unveränderten Pakete:

- s00: `ecf46d255e69415449d8f5245f14ec1815635fd13207fba34df2259d27838513`
- s01: `5e05fa3e07beff13970373ec19c8e3577f05f27ae2a99a56b5212fbe2c39928d`

Alle 214 Einträge werden gegen die Phase-3a-Namen, Offsets, Größen und Hashes
verglichen. Die Elternindizes aller 37 BINs werden gegen ihre tatsächlichen
Bytes geprüft; die originalen Modellmethoden verknüpfen die nativen Teile.
Die Prüfanwendung aktiviert das im allgemeinen Katalog ausgewiesene Raummodul,
ruft dessen tatsächliche R120Init auf, beobachtet deren Fehler, ruft danach
gezielt den originalen Eventcallback im Modus 3 auf und verarbeitet die
Fehlerbereinigung am Frame-Rand. Das ist eine Diagnoseroute und kein neuer
Spielablauf. Sie behauptet weder `Event::init` noch reguläres Eventende.

Beide Abbruchcallbacks setzen das tatsächlich im Original verwendete Bit 59:
`Scenario_flg[1] |= 0x10`. Dieses Bit wird bei der Consumerfreigabe nicht als
erfundenes Intro-Abschlussflag ersetzt oder gelöscht. Eine vollständige
Savegame-Persistenz dieses Flags wird hier nicht neu implementiert.

Der logische Quellpfad `event/evd/r120s00.evd` ist nicht der FST-Pfad.
`EventMgr::EvtReadSub` in `src/game/event.cpp:2280` verwendet nachweislich
`strstr(pNameEvt, "evd/")`, `NameChange` und `DC.setData`. Die Auswahl im
Testtreiber nutzt die bereits aus diesen Quellreferenzen aufgelösten
Phase-3a-Dateien. Die vollständigen Körper von EvtReadSub/EvtReadExec werden
noch nicht als ausgeführt bezeichnet.

Der Messageadapter verwendet Englisch = Index 1, wie die originale
MessageData-Adressierung. Nur belegte Header-/Tabellenwörter werden in einer
ausgerichteten nativen Lookupkopie umgesetzt. Das originale `getAddr` erreicht
damit die richtige Nachricht; zurückgegeben werden geprüfte originale
Big-Endian-Payloadbytes, keine angenommenen Terminatoren oder erfundenen Texte.
Ungültige Typen, Sprache, Counts, Offsets und unvollständige Tabellen werden
abgelehnt. Negative Message-Indizes werden vor dem Originalzugriff begrenzt.

Für CAM B402–B404 werden bekannte Header-, CUT-, AREA-, CAMERA- und Linktabellen
mit ihren tatsächlichen Größen geprüft. Der Indexer ist kein Ersatz für
`CameraControl::calcAddr`, `areaHitCheck`, `Check` oder `Move`.

| Echter Raum | CAM-Verifikation | Englische Raum-MDT-Einträge |
| --- | --- | ---: |
| r120 | B403, 4 Bereiche, 0 Kameras; begrenzt indexiert | 0 |
| r100 | offen: `pFrame` außerhalb der Ressource | 52 |
| r101 | derselbe benannte offene Feldbereich | 23 |
| r400 | derselbe benannte offene Feldbereich | 15 |
| r405 | derselbe benannte offene Feldbereich | 1 |
| r402 | derselbe benannte offene Feldbereich | 16 |

Die Raum-MDT mit null Einträgen in r120 ist von den separaten englischen
Event-MDTs mit 9/2 Einträgen zu unterscheiden. Die anderen CAM-Bytes bleiben
unverändert verfügbar, werden aber nicht als funktionsfähige Kameras registriert.
Der tatsächlich gemessene Fehler lautet:
`CAM pFrame outside immutable resource; relocation semantics unresolved`.
Beispielsweise enthält die erste r100-Kamera einen `pFrame`-Wert außerhalb
ihres 7.744-Byte-Datenblocks. `CameraControl::calcAddr` (`cam_ctrl.cpp:282`)
reloziert dieses Feld; `HermiteExport` (`:53`, Nutzung ab `:109`) unterscheidet
NULL und eine explizite Frametabelle. Ohne Original-RAM-/Verwendungsnachweis
wird der fragliche Wert weder auf NULL gesetzt noch als GameCube-Adresse
erraten. B400/B401 bleiben ebenfalls unportiert.

Die 35 unvollständigen englischen MDT-Blöcke aus Phase 3a bleiben unverändert.
Keine Ersatztexte oder andere Sprache werden eingesetzt. Ein späterer echter
Raumzugriff geht über RoomMes/MessageControl/MessageData; Eventtexte werden
über `Event::ExePacket_SetMdt` (`src/game/event.cpp:1027`), `GetBin` und
`MesData.registData(..., 1)` gewählt. Da diese vollständigen produktiven
Start-/Paketpfade noch fehlen, ist der Zeitpunkt der Verwendung jedes einzelnen
unvollständigen Blocks weiterhin offen. Diese Phase prüft echte verfügbare
Tabellen und isolierte fehlerhafte Tabellen, nicht die Erreichbarkeit aller 35.

## Tatsächlich ausgeführte Tests

Vor der Implementierung wurden die vorhandenen 23 CTest-Gruppen auf Windows
x86 und x64 erfolgreich wiederholt. Nach der letzten Codeänderung wurden beide
Hostbuilds, beide Android-Native-Builds, die Debug-/Test-APK und Lint erneut
ausgeführt. Kompilierung verwendet Warnungen als Fehler.

| Prüfung | Windows x86 | Windows x64 | Retroid ARM32 | Retroid ARM64 |
| --- | ---: | ---: | ---: | ---: |
| Finale CTest-Gruppen | 24/24 | 24/24 | nicht CTest | nicht CTest |
| Gemeinsamer nativer Game-/Consumer-Selbsttest | 503, 0 Fehler | 503, 0 Fehler | 503, 0 Fehler | 503, 0 Fehler |
| Echte Raum-/Block-/ESL-Vorbereitungen, zwei Runden | 20 | 20 | 20 | 20 |
| Echte r120-EVD-Prüfungen, zwei Runden | 4 | 4 | 4 | 4 |
| Modul-Adapter-Selbsttest | 216, 0 Fehler | 216, 0 Fehler | 216, 0 Fehler | 216, 0 Fehler |
| Original-RELs je Runde, zwei Runden | 114 | 114 | 114 | 114 |
| APK/JNI-Instrumentierung nach finalem APK-Build | — | — | 19/19 | 19/19 |
| Native Basis-/Scheduler-/Background-/Yz2-Suite, je zwei Gerätestarts | in CTest enthalten | in CTest enthalten | 13.386, 0 Fehler | 13.387, 0 Fehler |

Die Basisprüfzahl ist unter ARM64 um eins höher, weil
`src/scheduler_verification.cpp:96` nur bei 64-Bit-Pointern zusätzlich nachweist,
dass das Callbackargument tatsächlich oberhalb `UINT32_MAX` liegt. Es ist kein
abweichender Gameplayzustand. Die 503 Game-/Consumerprüfungen und 216
Modulprüfungen stimmen auf allen vier Plattformen überein.

Die echten Raumprüfungen laden je Runde auf Disc 1 r120/r100/r101 und wiederholt
r120/r100, auf Disc 2 r400/r405/r402 und wiederholt r400/r405. Archivkörper,
Blöcke, ESL-Auswahl, CAM/MDT und Hashes werden über die bestehenden nativen
Engine-Schnittstellen geprüft. Insgesamt sind das 80 Raumvorbereitungen und
16 EVD-Prüfungen über alle vier Plattformen. Vollständige Szenarioinitialisierung
bleibt in diesen Datentests ausdrücklich blockiert.

Der allgemeine Modulkatalog enthält 114 Module und 122 Raumregistrierungen.
Alle REL-Strukturen, Hashes, Imports, BSS und sieben beschädigte Varianten pro
REL werden erneut geprüft: 798 erwartete Ablehnungen je Runde/Plattform.
Die bereits geprüfte partielle Stage-Registrierung bleibt bestehen. Vollständig
ausgeführte kleine originale Init/Main-Paare sind weiterhin R10d und R328.
Das zusätzliche kompilierte R120-Paar hat Resident-Imports: Der alleinstehende
Modultest verlangt daher den Fehler `Original room resident imports are unbound`
und zählt ihn separat, statt drei vollständig ausgeführte Paare zu behaupten.
R120-Residentcode wird über GameRuntime mit der oben beschriebenen Grenze geprüft.
Die REL-Zustands-/Hashspuren stimmen über alle Runden und ABIs überein.

Die neuen isolierten Regressionen prüfen insbesondere:

- zwölf originale R120Init-Versuche mit tatsächlicher SceExec-Anforderung und
  anschließender sicherer Frame-Bereinigung;
- 20 explizite Raumfreigaben und 20 Stage-Initialisierungsschritte mit
  19 nachfolgenden automatischen Cross-Stage-Wechseln, bereits initialisierten
  R10d/R328-Körpern und markierten Task-Borrowern;
- Release/Reset/Deaktivierung desselben Raummoduls, neue Callback-Epochen,
  Modulwechsel und Ablehnung alter Event-/Modellreferenzen;
- tatsächliches `Event::ExeFunc` mit isoliertem verschachteltem Callback,
  erhaltenen lokalen Variablen, mehreren TaskSleeps, Stop/Restart und Abbruch;
- Ablehnung einer Freigabe mit lebendem Callbackstack, eines fremden Threads
  und einer Callbackkopie nach Zerstörung ihres Besitzers;
- BIN-Elternzyklen, falsche Versionen/Offsets, EVD-Counts/Packetgrenzen/Namen,
  Kameraoffsets und fehlende/ungültige englische Message-Tabellen;
- unveränderte originale PadRead-Flanken, Sticks, Trigger, Stop/Resume und
  synthetische Verbindungs-/Trennungsereignisse über den vorhandenen JNI-Adapter.

Die synthetischen BIN/CAM/EVD-Bytes und SharedScenarioConsumers-Testobjekte
rekonstruieren kein Gameplay. Sie belegen nur die jeweils benannte
Adapter-/Originalmethodensemantik. Die echten Originaldatenprüfungen sind
separat in den privaten `game-runtime-*.json` und `consumers-*.json` dokumentiert.

`tools/summarize_consumers.py` prüft die vier ABIs, beide Runden, alle
Pointerbreiten, Hashes, 503 Assertions, native Spuren und reale Consumerdaten.
Der finale Bericht bestätigt identische relevante Übergänge:

| Semantische Daten | Identischer SHA-256 über vier Plattformen / Wiederholungen |
| --- | --- |
| Raumfolge Disc 1 | `8314ba491a155af7aca23efaf3378d9e87d284989d16cb6022e9c83ae17be10c` |
| Raumfolge Disc 2 | `098a821d4b8f699e84891c1bfd749330e41f2461776e36761ff7717147e39090` |
| EVD-Consumer, Abbruchbit und Freigabe | `fbfdd5b85f02229648730d0f060903127dda79be9da01744b6639e648258401d` |
| REL-Zustände Disc 1 | `8231f6214c798d25a32d9e33105899cf6673aa2af473df5e313775290467d368` |
| REL-Zustände Disc 2 | `355791248872517ac673bb3afa37ceaa58a25b944c7b565f91af5a1218b5fc51` |

Diese Gleichheit umfasst die tatsächlich getesteten Zustände; sie ist kein
Beweis vollständiger GameCube-Gameplay-Äquivalenz. Neue GameCube-Referenztraces
wurden nicht aufgenommen. ASan/UBSan wurden mangels verifizierter Fiber-Hooks
nicht ausgeführt; die bestehenden expliziten Grenzen und Lifecycleprüfungen
ersetzen keinen vollständigen Speichersanitizerlauf.

## Android und Controller

ADB-Gerät `dbdcacc`: `Retroid_Pocket_Flip2`, Android 13, API 33.
Gemeldete ABIs: `arm64-v8a,armeabi-v7a,armeabi`. Beide NDK-Programme wurden
tatsächlich auf diesem Gerät ausgeführt. Bei der APK wurde mit `adb install
--abi` getrennt installiert, `primaryCpuAbi` geprüft und eine erwartete
Pointerbreite an die Instrumentierung übergeben. Die 19 Tests bestanden in
beiden Appprozessen; ein Dual-ABI-Paket allein wurde nicht als ARM32-Nachweis
verwendet.

`dumpsys input` bestätigt weiterhin Device 9 `Retroid Pocket Controller`,
Descriptor `dc75afea56e3c3a269b97967aa26b8c93c0bd3fb`. Der bestehende
Android-InputManager/ControllerLoop und die gemeinsame originale PadRead-
Anbindung wurden nicht verändert. Es gibt keinen zweiten Eingabepfad.

Die physische Betätigung von Tasten, D-Pad, Sticks und Analogtriggern ist bereits
in Phase 4b durch die Benutzerbestätigungen und die korrigierten ARM32-/ARM64-
Messungen belegt. Diese privaten Capture-Berichte bleiben unverändert.
In Phase 4c wurden Controllerinventar und automatische native/JNI-
Adapterregressionen erneut geprüft, aber keine neuen physischen Tastendrücke
oder ein reales USB-Disconnect aufgezeichnet. Synthetische Disconnecttests
werden ausdrücklich nicht als frischer physischer Nachweis bezeichnet.

## Fehlversuche, Korrekturen und Grenzen

Die finalen oben aufgeführten Builds und Tests haben keine Fehler. Während der
Entwicklung traten folgende überprüfte Fehler auf:

| Befund | Korrektur / weiterhin gültige Grenze |
| --- | --- |
| Erste Flagprüfung erwartete nach einem r120-Abbruch null | Test korrigiert: Originalbit 59 ist `Scenario_flg[1] & 0x10`. Kein Originalverhalten geändert. |
| Ein StageSet-Test setzte den vorherigen Raum auf r120, obwohl noch kein Stageheap existierte | Initiale Sentinel-Voraussetzung auf den originalen Startzustand gesetzt; produktive Heap-/Stage-Entscheidung blieb erhalten. |
| Pending-Modulborrower im Test hatte noch keinen Lifetime-Token | Testanforderung mit tatsächlichem Modultoken versehen; aktive verschachtelte Aufrufe werden zusätzlich über den Modulkontext markiert. |
| `getPartsPtr`-Fehlertest erwartete null | Test an originales Log-/Fehlerverhalten angepasst; kein stiller Erfolg. |
| Alle-REL-Test behandelte kompiliertes R120 als vollständig eigenständig | Resident-Importgrenze in Katalog und Tool getrennt erfasst; ungebundener Residententry muss fehlschlagen. Danach alle vier Plattformen erneut geprüft. |
| Echte CAMs enthalten nicht begrenzte `pFrame`-Werte | Explizit offen; keine Nullersetzung oder angenommene Relokation. |
| Temporäre Verzeichnisse/Private-Data-Schreibzugriffe wurden im Sandboxlauf verweigert | Autorisierte Tests mit den erforderlichen Ausführungsrechten wiederholt; nur diese tatsächlichen erfolgreichen Läufe zählen. |

## Offene Originalabhängigkeiten und nächster Arbeitsumfang

| Quelle | Exakter noch fehlender Zusammenhang |
| --- | --- |
| `src/game/game.cpp:214,239` | GameTask erreicht nun gameDebug und gameInit-Dispatch. Der Körper von `gameInit` ist weiterhin nicht integriert; erste Originalabhängigkeit ist `ClothInit`, anschließend u.a. Message/Checkpoint/Item/Player/GameSave. |
| `src/game/game.cpp:292,354` | `gameStageInit` und `gameRoomInit`: Message-Auswahl, Save-/Discwechsel, System-/Roommanager, Light/Player/ObjMgr/EmMgr/PartsMgr/Scroll/Collision. Diese Dispatchs schlagen benannt fehl. |
| `src/game/game.cpp:1207,1288` | `gameDoordemo` und `gameRoomMemInit`: vollständige originale Managerfreigabe, Heap 3/4/10, ARAM-MemorySwap, absolute GameCube-Pools und GLOBAL-Roombereich. Die neue native Freigabe deckt nur bisher bekannte Ressourcen ab. |
| `src/game/sce_sys.cpp:49` | Produktives ScenarioRoomInit benötigt zuerst `SceAtSetSaveItem`; Condition-/Subchar-/Actor-/Area-/Mission-/Eventconsumer fehlen weiterhin. Ein Ordnungs-Testobjekt erfüllt diese Abhängigkeit nicht produktiv. |
| `src/game/model.cpp:99,930,1343` | `cModel::modelInit`, `calcModelAddr`, `cModInfoMgr::create`: TPL/GX-Abgrenzung, echte Parts-/Matrixinitialisierung, Skin-/Material-/Lichtzustände, Motionlayout und native Offsetfixups. Aktuelle CPU-Jointregistry ist nur ein belegter Unterbereich. |
| `src/game/cam_ctrl.cpp:53,282,1017` | Hermite-/CAM-Fixups, noch ungeklärtes pFrame, roomInit, Kamera-/Areaentscheidung und Move mit originalen Player-/Collision-/Matrixdaten. |
| `src/game/mes.cpp:406,421,454` | MessageControl gameInit/roomInit/stageInit, Fonts, Steuercodes, Layout und getSelect-Entscheidungen. Englische MDT-Tabellenadressierung allein reicht nicht. |
| `src/game/event.cpp:152,205,732` | Event::init/Run/ExePacket: ModTbl/native Modelle, CalMaxFrame, FCV/SEQ, Camera, Sound und Packetconsumer. Keine Eventinitialisierung nur durch Indexieren behaupten. |
| `src/game/event.cpp:2280,2393` | EvtReadSub/EvtReadExec: tatsächliche Read-/Bin-Registry, Em-MemorySwap, SetEvd, native Pointergrenzen und Managerbesitz mit Loader verbinden. Aktuelle echte EVD-Dateiladung ist ihre Vorarbeit. |
| `src/game/sce_com.cpp:82,133` | SceEventStart/SceEventEnd verändern Player-/Manager-/HUD-/Audiostatus; kein erfolgreich zurückkehrender Ersatz erlaubt. |
| `src/st1/r120.cpp:45`; `src/game/TexRender.cpp` | TexRenderInit und das originale 256×256-Rückspiegelziel benötigen wirkliche Render-Target-/Matrixbindung. Dieser produktive Blocker wird nicht weggenullt. |
| `src/st1/r120.cpp:63` | systemVISetBlack, Sofdec opening.sfd, SceEventStart, EvtReadAram/EvtReadExec, Eventende und SceAtExecRoomJump fehlen. Nach dem ersten SceSleep bleibt der Verlauf kontrolliert blockiert. |
| `src/st1/r100.cpp`, Katalog | r100 hat dasselbe primäre Modul 74 / REL-Fileindex 146 wie r120; sein Init/Main ist noch nicht als geprüftes natives Paar integriert. Echte Archive/Blöcke/ESL/52 englische Message-Einträge sind geladen, keine Spiellogik ersetzt. |

**Nachgewiesene Tatsachen:** die Originalmethoden, geprüften Assetspans,
Abbruchbits, ABI-Ergebnisse und sicheren bekannten Lifetimeübergänge oben.
**Technische Annahme für die weitere Integration:** die gleiche native
Besitz-/Offsetstrategie ist für zusätzliche Manager geeignet; das muss für
jede Klasse mit Originalheadern und Tests belegt werden.
**Offene Fragen:** tatsächliche pFrame-Nutzung/Relokation, alle Motion-/TPL-
Pointerbeziehungen und die Erreichbarkeit der unvollständigen englischen MDTs
im vollständigen Event-/Spielablauf. Keine dieser Fragen ist durch erfundene
Formatregeln beantwortet worden.

Der nächste konkrete Schritt innerhalb der noch offenen Phase 4c ist die
source-geprüfte native `cModInfoMgr::create`-/`cModel::modelInit`-Kette mit
getrenntem CPU-, Motion- und Materialbesitz. Erst wenn ein echtes BIN/TPL/SEQ-
Objekt samt originalen lokalen/world Matrizen und Freigabe funktioniert, sollen
Event::init/CalMaxFrame und die benötigten originalen Packetconsumer daran
angeschlossen werden. Parallel erforderliche CAMERA-pFrame-Belege müssen aus
Originalcode/-daten oder einer Referenzausführung kommen. Danach folgt die
gemeinsame Game-/Stage-/Roominitialisierung mit ihren tatsächlichen Abhängigkeiten
und der bereits implementierten sicheren Freigabe. Ein erfolgreicher r120-
Start erfordert schließlich reale Präsentations-/Movie-/TexRender-Abhängigkeiten
oder einen ausdrücklich belegten Originalpfad, der sie nicht benötigt.

Für den Übergang nach r100 müssen außerdem dessen echter Init/Main, vollständige
GameDoordemo-/RoomMemInit-Semantik, Kamera-/Player-/Collisionzustände und der
originale RoomJump-Task nachgewiesen werden. Die im Original vorhandene
Sprungposition wurde nicht durch einen neuen Raumablauf verwendet. Es wurde
keine weitere große Phase und keine vollständige Grafik-/Audioengine begonnen.

Die verbindlichen Grafikoptionen bleiben in `graphics-settings.md` erhalten:
MSAA Aus/2×/4×, AF 2×/4×/8×, DoF mit naher/anvisierter Fokussierung, persistente
Einstellungen, tatsächliche Hardwarevalidierung und backendunabhängiges Menü.
Diese Phase implementiert sie weiterhin nicht vorgetäuscht.

## Reproduktion

Werkzeuge der geprüften Sitzung: MSVC 19.51 (VS 18), NDK 28.2.13676358
(Clang 19), CMake 4.1.2, Ninja 1.12.1, Gradle 8.13, AGP 8.13.2, JBR aus Android
Studio, compileSdk 35 / minSdk 21. Native Android-Builds verwenden separate
Buildverzeichnisse für `armeabi-v7a` und `arm64-v8a`.

Auf dem jeweiligen MSVC Developer-Terminal, nach der bestehenden Hostkonfiguration:

```powershell
cmake --build port/build/host-ninja --parallel
ctest --test-dir port/build/host-ninja --output-on-failure
cmake --build port/build/host-x86 --parallel
ctest --test-dir port/build/host-x86 --output-on-failure
```

Dabei muss das Terminal für den jeweiligen Zielcompiler x64 beziehungsweise
x86 initialisiert sein. Androidbuild und Gerätesuiten:

```powershell
.\port\scripts\build-android.ps1 -SdkRoot 'C:\Users\andiw\AppData\Local\Android\Sdk' -JavaHome 'C:\Program Files\Android\Android Studio 2025\jbr' -Python 'C:\Users\andiw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' -Offline
.\port\scripts\test-app-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc
.\port\scripts\test-device.ps1 -Adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' -Serial dbdcacc -Repeat 2
```

Private Datentests; `python` bezeichnet den oben angegebenen Python-Interpreter:

```powershell
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-host' --host-exe port/build/host-ninja/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-host' --host-exe port/build/host-x86/re4_game_tool.exe --repeat 2
python port/tools/test_game_runtime.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-device' --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' --serial dbdcacc --repeat 2
python port/tools/test_modules.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-modules-host' --host-exe port/build/host-ninja/re4_module_tool.exe --repeat 2
python port/tools/test_modules.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-modules-host' --host-exe port/build/host-x86/re4_module_tool.exe --repeat 2
python port/tools/test_modules.py --data 'E:\Development\Android\RE4-Data\import-phase3a' --output 'E:\Development\Android\RE4-Data\phase4c-modules-device' --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' --serial dbdcacc --repeat 2
python port/tools/summarize_consumers.py --host 'E:\Development\Android\RE4-Data\phase4c-host' --device 'E:\Development\Android\RE4-Data\phase4c-device' --output 'E:\Development\Android\RE4-Data\phase4c'
```

Die finalen lokalen Build-/JUnit-Protokolle liegen im ignorierten `port/build/`:
`phase4c-ctest-host-64.txt`, `phase4c-ctest-host-32.txt`,
`phase4c-android-build.txt`, `instrumentation-armeabi-v7a.txt`,
`instrumentation-arm64-v8a.txt`, `device-<abi>-round1.txt` und
`device-<abi>-round2.txt`. Ältere unnummerierte Geräteprotokolle sind kein Beleg
für diesen neuen Lauf. Die privaten JSON-Berichte enthalten ausführbare
Dateihashes und tatsächlich gemessene Pointerbreiten.

## Vollständige Liste geänderter Projektdateien

Alle 28 Dateien bleiben unter `port/`; Originalcode und Assets liegen außerhalb
dieser Änderungsliste.

| Datei relativ zu `port/` | Änderung |
| --- | --- |
| `CMakeLists.txt` | Consumer-Generator, Library, Verknüpfung und Provenienztest. |
| `README.md` | Aktueller Phase-4c-Teilstand und Grenzen. |
| `docs/phase4c-status.md` | Dieser vollständige Bericht. |
| `config/consumer-sources.json` | Neue Original-Fingerprints für CPU-Consumer. |
| `config/runtime-sources.json` | Zusätzliche geprüfte r120-/Event-/ID-/SScrn-Quellen. |
| `config/module-sources.json` | Geprüfter r120-Residententry. |
| `include/re4/port/consumers.h` | Neue native CPU-Ansichten, Leases und Lifetime-API. |
| `include/re4/port/game_runtime.h` | Consumerzugriff, Raumfreigabe und Originalentry-Diagnose. |
| `include/re4/port/modules.h` | Typisierte Resident-Bindung, Quieszenz und Work-Konstruktion. |
| `src/consumer_original.h` | Neue minimale Originaltypen-/Log-Grenze. |
| `src/consumers.cpp` | BE-Prüfung, native Registry, Tabellen-/Callback-/Lifetimeadapter. |
| `src/runtime_original.h` | Vollständig typisierte r120-/gameDebug-Abhängigkeiten mit benannten Fehlergrenzen. |
| `src/game_runtime.cpp` | Zentraler Frame-Freigabeweg, Modulborrower und Residententry. |
| `src/modules.cpp` | Quieszenz, native Workbindung und Residentaufrufe. |
| `tools/generate_consumers.py` | Neuer fingerprintgeprüfter Originalfunktionsgenerator. |
| `tools/generate_runtime.py` | gameDebug, DebugTrg und vollständige r120-Körper. |
| `tools/generate_modules.py` | R120-Resident-Metadaten und native Bindung. |
| `tools/test_game_runtime.py` | Echte EVD-/BIN-/MDT-/CAM-Prüfungen und 503-Check-Untergrenze. |
| `tools/test_modules.py` | Residentblocker getrennt von vollständig ausgeführten Paaren. |
| `tools/summarize_consumers.py` | Neue Vier-ABI-Abdeckung und semantischer Vergleich. |
| `tests/consumer_verification.h` | Neue isolierte CPU-/Callback-/Raumlifetime-Regressionen. |
| `tests/test_consumer_sources.py` | Neuer vollständiger Körper-/Fingerprint-Negativtest. |
| `tests/game_runtime_verification.cpp` | Neue Suite angebunden und exakte Startgrenze geprüft. |
| `tests/game_runtime_tool.cpp` | Universelle Original-EVD-Diagnose und Raum-CAM/MDT-Auswertung. |
| `tests/module_verification.cpp` | Neues R120-Paar samt offener Resident-Abhängigkeit geprüft. |
| `tests/module_tool.cpp` | Separate echte Einstiegspaar-/Residentblocker-Ergebnisse. |
| `tests/test_modules.py` | Provenienz-/Katalogprüfung für drei kompilierte, teilweise gebundene Paare. |
| `android/app/src/androidTest/java/org/re4/port/GameRuntimeTest.java` | 503 Checks und neue Lifetime-/Callbackspuren im tatsächlichen Appprozess verlangt. |
