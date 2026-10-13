# Phase 3a – universeller Originaldatenimport

Stand: 9. Oktober 2026. Branch `android-port`. Ausgangsstand war der saubere,
bereits lokal gesicherte Phase-2b-Commit
`31940b6e8a2195e1c65c683e58fb08916ffdef7e`. Die ursprünglichen Spielquellen,
Werkzeuge, Header und `master` wurden nicht geändert. Alle Originaldaten und
generierten Inventare liegen ausschließlich in
`E:\Development\Android\RE4-Data\import-phase3a`.

## Nachgewiesener Disc-Bestand

| Eigenschaft | Disc 1 | Disc 2 |
|---|---|---|
| Eingabe | `RE4_2004-11-25_Disc1.gcm` | `RE4_2004-11-25_Disc2.gcm` |
| Image-Größe | 1.459.978.240 Byte | 1.459.978.240 Byte |
| Tatsächliche Kennung | G4BE08 | G4BE08 |
| `DVDDiskID.diskNumber` | 0 | 1 |
| Revision | 0 | 0 |
| Titel im Header | resident evil 4 disc1 | resident evil 4 disc2 |
| FST-Dateien | 1.154 | 1.090 |
| Raumarchive | 85 | 57 |

Die beiden DOLs sind bytegleich, SHA-256
`beadba706b97a509ace8db468942d23c3fc1e84aa2d143ba9372b914c39b14fe`.
Beide enthalten am DOL-Dateioffset 2.370.660 die tatsächliche Zeichenfolge
`Nov 25 2004`, unmittelbar gefolgt von `10:19:49`. Die SDK-/Middleware-
Datumszeichenfolgen im gleichen DOL haben teilweise andere Daten und werden
nicht als Datum des Spiels ausgegeben. `include/dolphin/dvd.h:DVDDiskID`
belegt die Kennungsfelder; `tools/extract_orig.py:Gcm` belegt die Disc-/DOL-
Dateisystemzugriffe. Auch die vorhandenen Debug-Symboldateien bleiben erhalten.
Ohne externe authentifizierte Referenzprüfsumme ist dies ein belegter
Kennungs-/Versionsabgleich, kein kryptografischer Herkunftsnachweis.

Image-SHA-256, vor und nach dem vollständigen Import identisch:

- Disc 1: `b7fcbf121cf7c527aae23838e9c3f0818e31115bb597eb77a2c49c9e8fa46492`.
- Disc 2: `118640eb02953d5a8234d3f9e27ea535e75b0346b9cf22aa764b65c3d76f1b12`.

Gesamt: 2.244 Dateieinträge, 1.223 unterschiedliche Dateiinhalte und 945
Gruppen mit bytegleichen Dateien. Identische Inhalte werden im SHA-256-
Speicher dedupliziert; sämtliche ursprünglichen Pfade, FST-Indizes,
Verzeichniseltern, Namensbytes, Disc-Zugehörigkeiten und Varianten bleiben im
Manifest erhalten. `opening.bnr` hat unter demselben Pfad unterschiedliche
Disc-Inhalte; keine Variante überschreibt die andere.
983 Dateipfade kommen auf beiden Discs vor, davon 982 mit gleichem SHA-256.

## Ausgeführte Pipeline und Formatabdeckung

`port/tools/import_original.py import` öffnet die Images ausschließlich mit
`rb`. Es validiert FST-Verzeichnisgrenzen, Namen, Dateispannen, DOL-Sektionen,
Kennungen und Datumsnachweise, extrahiert alle FST-Dateien unverändert und
vergleicht das vollständige FST-/DOL-Ergebnis mit der unverändert übernommenen
Klasse `tools/extract_orig.py:Gcm`. Deren Klasse wird aus dem AST geladen,
damit der Import keine CLI-spezifische PyYAML-/DOL-Splitting-Abhängigkeit und
keine Ausgabe in `orig/` benötigt. Es gibt keinen Ersatz für deren API durch
Dummy-Module. Bootheader, BI2, Apploader, FST und DOL werden separat erhalten.

Die Originalwerkzeuge bleiben unverändert:

- `tools/drs.py:read_records/Drs/SoundBank/rel_size`: äußere Container,
  Sound-Records und bytegenaue Container-Roundtrips. Vorher prüfen begrenzte
  Leser alle Spannen und REL-Relokationslisten.
- `tools/motion/archive.py:parse_body/parse_event/parse_etm/smd_motions`:
  Body-Container, benannte Event-Bins, ETM-Unterarchive und SMD-Motiontabellen.
- `tools/motion/yz2.py:parse_header/decode`: unabhängiger Vergleich aller
  eindeutigen komprimierten Bodies mit dem nativen Decoder aus Phase 2b.
- `tools/motion/fcv.py:parse/serialise/parse_seq/serialise_seq` und
  `modelbin.py`/`meshbin.py`: Motion-/Sequenz-/Modelldaten, einschließlich
  bytegenauer Roundtrip-Prüfung, soweit der vorhandene Parser sie unterstützt.
- `include/tpl.h` und `tools/motion/gxtex.py:parse_tpl/base_size`: TPL-
  Deskriptoren, Dimensionen, Offsets und Basis-Texturspannen ohne Pixelkonvertierung.

126 unterschiedliche komprimierte Bodies decken die 142 komprimierten
Raumarchivinstanzen ab. Jeder wurde mit der Python-Referenz bytegenau
verglichen, einschließlich tatsächlich verbrauchter Codebytes. Der erste
Yz2-Headerwert ist **keine verlässliche harte Begrenzung** des Codebyte-
Verbrauchs: bei r100 sind Headerwert und tatsächlich gelesene Codebytes
unterschiedlich. Die Record-Spanne begrenzt die Eingabe, der geprüfte zweite
Headerwert begrenzt die Ausgabe. Es werden keine GameCube-Zeiger in native
Pointer umgedeutet.

Der native `AssetIndex` implementiert `read.cpp:GetDataExt`-kompatible
Dreibuchstaben-/Vorkommenssuche und erhält jeden Arrayindex sowie seinen
32-Bit-Dateioffset. Die Python-Metadaten erhalten zusätzlich den
Offsettabellen-Wortindex `index + 4`, wie die Original-ARC-Makros. Größen/
Offsets werden explizit Big Endian gelesen; native Pointer bleiben ABI-breit.
Ein Aufrufer muss den unveränderlichen Buffer während jeder Span-Nutzung
besitzen. Es werden keine GX-, cDvd-, REL- oder Gameplay-Funktionen simuliert.

Korrekt behandelte Originalvarianten:

- Komprimierte DAS-Bodygrößen sind nicht notwendigerweise 32-ausgerichtet.
  Der nächste Record beginnt dagegen am nächsten 32-Byte-Rand.
- 22 ESL-Dateiinstanzen enthalten 255 Records; vier enthalten 256. Die
  `GLOBAL_WK::Em_list[256]` ist eine Laufzeitkapazität. Der Import erhält die
  tatsächliche Anzahl und erfindet keinen zusätzlichen Record.
- 140 BLK-Einträge enthalten null Blocks und unbenutzte, teilweise
  `0xCDCDCDCD`-gefüllte Offsetfelder. `block.cpp:cBlock::roomInit` beendet
  diesen Pfad ohne deren Dereferenzierung. Nur Tabellen mit positiver Anzahl
  benötigen gültige Spannen. Das aktive BLK von r100 hat fünf vollständig
  aufgelöste Zusatzdateien.
- Deduplizierung identischer Bytes aliasiert keine Ressourcentypen:
  gleiche FCV-/TPL-Platzhalterbytes behalten getrennte logische Tags.

Die Matrix unterscheidet `roundtrip`, `indexed`, `empty`, `opaque` und
`open`. `indexed` ist ein geprüfter Container/Referenzindex, kein Nachweis
einer bereits ausführbaren Spielszene. `opaque` erhält unveränderte Daten;
`open` benennt eine Variante, Parserlücke oder einen defekten beziehungsweise
platzhalterartigen Originaleintrag. Auch solche Ressourcen bleiben erhalten.
Die zwei offenen Dateiinstanzen `bgm/bgmtbl.dat` sind kein bestätigter
Tagged-Body; ihr Audio-Tabellenformat ist weiter offen.

## Alle Räume sowie r120/r100

Alle 142 vorhandenen Raumarchive wurden indexiert, entsprechend 127 Raum-IDs.
Die 15 wiederholten Raumarchive beider Discs bleiben mit Disc-Zugehörigkeit
sichtbar. Die Pipeline verwendet keine Sonderregeln für r120 oder r100.
Sie erzeugt für jeden Raum denselben Bericht unter `rooms/rXXX-discN.json`.

| Raum | Archivdatei | Dekomprimierter Body | Top-Level-Einträge | Event-Dateikandidaten |
|---|---:|---:|---:|---:|
| r120 | 1.569.888 Byte | 6.086.688 Byte | 13 | 2 |
| r100 | 3.766.720 Byte | 4.669.568 Byte | 51 | 8 |

r120-Body SHA-256:
`8c39abc0499273d8f516bf03f5e1c806a10472176c0e7feca7330a9d9f26d6a9`.
r100-Body SHA-256:
`8cb4cbd4f7f85f925c76703405d0f164891cf068dc6e815d0a842fccdd3146dc`.

`src/game/main.cpp:systemWorkInit` startet mit Room-ID 0x120 und Sprache 1.
`src/st1/r120.cpp:R120Event` lädt `movie/opening.sfd`, die tatsächlichen
r120s00/r120s01-Events und springt mit `SceAtExecRoomJump(0x100, ...)` weiter.
`src/game/event.cpp:EvtReadSub` schneidet den `evd/`-Pfad aus dem Eventnamen
aus; `NameChange` enthält zusätzliche kostümabhängige r-/s-Varianten. Diese
Regeln werden im Referenzbericht als belegt beziehungsweise bedingt vermerkt.
Die Eventdatei-Zählung allein ist keine vollständige Ausführungsabhängigkeit.

r100 hat die originalen Zusatzdateien `St1/r100_00.dat` bis `_04.dat`,
nachgewiesen durch BLK und `block.cpp:cBlock::getBlockWork`. Alle fünf
Referenzen sind aufgelöst. `roomdata.cpp:St1_data_tbl` ordnet sowohl r100
als auch r120 über FileTbl-Index 146 `rel/st1_0.rel` zu. ESL-Records werden
mit unveränderten Indizes, IDs, Flags, Room-IDs, Position-/Rotations-Rohwerten
und HP dokumentiert. Die Auswahl der Liste hängt von
`stage.cpp:checkEmListNo` und Spielmodus-/Szenarioflags ab; der Import
ersetzt sie nicht durch eine vermutete Standardliste.

## Englisch und zusätzliche Sprachen

Pro Disc existieren 84 Dateien in den tatsächlichen `SS/Eng`-/`etc/Eng`-
Verzeichnissen und 133 in `SS/Cmn`-/`etc/Cmn`. `Item` ist eine Ressourcengruppe,
keine Sprache. Keine weiteren lokalisierten Verzeichnisse wurden gefunden.
Japanische Schriftdateien und benannte Entwicklungsdateien werden trotzdem
bewahrt; ihre Existenz beweist keine vollständige japanische Spielversion.

`read.cpp:OptionDataRead`, `option.cpp:setLangExt3` und
`sscrn.cpp:sscrnSetLanguage/sscrnDataFilename` belegen die englischen Pfade.
`mes.cpp:MessageControl::setLanguage` bildet Systemsprache 1 auf MDT-Index 1
ab; Systemsprache 2 ebenfalls. Die weiteren expliziten Zuordnungen sind
0 -> 0, 3 -> 2, 4 -> 3, 5 -> 4 und 6 -> 5. `getAddr/getMesNum` lesen
`tbl[m_language+1]`. Die vorhandenen sechs Indizes werden separat untersucht;
aus dem unbekannten Zweck von MDT-Wort null wird keine Sprachanzahl erfunden.
Englisch verwendet `Font/common_p.fnt`; `loadSystemFont` benutzt dafür den
gemeinsamen Font erneut. Japanisch hat einen anderen Fontpfad.

390 MDT-Instanzen einschließlich Disc-Duplikaten wurden untersucht:
313 englische Blöcke enthalten begrenzte Nachrichtenspannen, 42 sind leer
und 35 bleiben offen. Die 35 offenen englischen Blöcke sind 32-Byte-
Header mit allen Sprach-Offsets auf 0x1C; dort fehlt der vom Laufzeitcode
benötigte vollständige Blockheader. Sie sind **Platzhalterkandidaten**,
keine vom Import reparierten Texte. Ein weiterer italienischer Block ist
ebenfalls zu kurz. German-/French-Nachrichtenspannen sind in 354 Instanzen
bytegleich, German-/Spanish-Spannen in 353: zusätzliche Sprachen werden
deshalb nicht als vollständige Übersetzungen angeboten.

Nachgewiesen sind vorhandene englische Ressourcen und ihre Ladezuordnung.
Noch nicht nachgewiesen ist die Vollständigkeit aller englischen Texte,
Kontrollcodes oder jedes tatsächlich abgespielten Voice-/Untertitelpfads.
Gemeinsame SFD-, Soundbank- und BGM-Dateien bleiben vollständig erhalten;
ein Dateiname oder binärer Header beweist keine gesprochene Sprache.
Die Pipeline löscht oder filtert keine anderen Sprachressourcen.

## Tatsächlich ausgeführte Prüfungen

| Prüfung | Ergebnis |
|---|---|
| Ausgangsstand Host x86/x64 | je 15/15 bestehende CTests bestanden |
| Erweiterter Host x86/x64 | je 17/17 CTests bestanden |
| Python-Import-/Fehleingabetests | 18 isolierte Tests bestanden |
| Native Archivindex-Selbstprüfung | 51 Prüfungen je ABI/Host |
| Vollständiger echter Datenkorpus Host x86/x64 | je 758/758 bestanden |
| Echter Datenkorpus Retroid ARM32 | zweimal 758/758 bestanden |
| Echter Datenkorpus Retroid ARM64 | zweimal 758/758 bestanden |
| Bestehende native Android-Prüfungen | zweimal 13.386 / 13.387 Checks, null Fehler |
| Debug-APK-Instrumentierung | ARM32 und ARM64 je 12/12 bestanden |
| Android-NDK-/Gradle-/Lint-Build | beide ABIs, Debug- und Test-APK erfolgreich |

Der echte native Korpus enthält 373 Bodyindizes, 259 Containerindizes und
126 Yz2-Dekodierungen. Er prüft alle solchen unterschiedlichen Container-
Inhalte und vergleicht Offsets, Größen, Tagvorkommensindizes, Codebyte-
Verbrauch und SHA-256 der dekodierten Ausgabe. Das Android-Testprogramm
überträgt ausschließlich private CAS-Dateien nach `/data/local/tmp`, prüft
Transfer-SHA-256 und die tatsächliche Prozess-Pointerbreite. Kein Original-
Asset wird in APK oder Repository aufgenommen.

Gerät: Retroid Pocket Flip2, Serial `dbdcacc`, Android 13, API 33,
gemeldete ABIs `arm64-v8a,armeabi-v7a,armeabi`. Beide getesteten Prozesse
führten die jeweilige ARM-Binärdatei aus. Die vier Corpus-Traces haben
denselben SHA-256:
`d3bead1d4b296186cdb2e7b1f2075019c7c54217437da2455274884cad5741a8`.
Die Python-Inventarisierung und semantischen Python-Parser laufen auf dem
Host; auf Android laufen die native Container-/Bodyprüfung und Yz2-
Dekomprimierung, keine behauptete native ESL-/MDT-/Texturinterpretation.

Die größere ARM64-Prüfzahl der bisherigen Suite ist die bereits dokumentierte
zusätzliche Pointerprüfung. Die neuen 51 Selbstprüfungen und 758 Corpus-Fälle
sind zahlenmäßig identisch. Alte Scheduler-/Hintergrund-Traces bleiben gleich.

Entwicklungsfehler wurden korrigiert und erneut geprüft: anfangs zu strenge
DAS-Ausrichtung, bytearray-Tags als Python-Dictionary-Key, Windows-CP1252-
Ausgabe japanischer FST-Namen und falsche Gleichsetzung von ESL-Dateilänge
mit Pufferkapazität. Der zusätzliche Cache-Test verhindert Tag-Aliasing.
Die Gradle-Warnung zur SDK-XML-Version besteht weiter; der Build ist erfolgreich.

## Private Ergebnisdateien und Wiederholung

`manifest.json` und `fst-inventory.csv` enthalten sämtliche Dateien und
Verzeichnisse. `resource-coverage.csv` führt jeden indexierten Untereintrag;
`room-coverage.csv`, `rooms.json` und `rooms/*.json` decken alle Räume ab.
`languages.json` beschreibt die Sprachblöcke. `source-references.json`
enthält Originalpfade/Zeilen, FileTbl, Raum-/REL-Tabellen und statische
GetDataExt-Aufrufe. `data-dependencies.json` enthält ESL und BLK-Referenzen.
`inventory.md` ist der automatisch erzeugte Gesamtbericht.

`native-cases.json`, `host-x86-import-tests.json`,
`host-x64-import-tests.json` und `android-import-tests.json` enthalten die
tatsächlich ausgeführten Corpus-Nachweise mit Prüfprogramm-Hashes.
Die Berichte haben deterministische Datenfelder ohne Laufzeitstempel.
Geräte-/Buildnachweise enthalten ausdrücklich plattformabhängige Angaben.
Der vollständige Import wurde mit dem endgültigen Code erneut ausgeführt.
Beide Durchläufe ergeben dieselben 758 nativen Fälle; deren Manifest-SHA-256
ist `fca0bfef1aeaf59d2169ace496e26877a4909d390438621f68fba2c232379666`.
Die abschließende Ressourcenmatrix enthält 85.088 Datenzeilen und die
Raumberichtsserie 142 JSON-Dateien. Vorhandene Referenzprüfsummen wurden
nicht durch ungeprüfte Cache-Treffer ersetzt: Yz2 wurde vollständig erneut
dekodiert und mit der Python-Referenz verglichen.

Voraussetzungen: Python 3.12 (Standardbibliothek, kein neues Python-Paket),
die bestehende MSVC-/CMake-/Ninja-Basis und Android SDK/NDK aus Phase 2b.
Beispiel aus dem Repository-Wurzelverzeichnis, mit `python` auf diese
Python-Installation gebunden:

```powershell
python -u port/tools/import_original.py import `
  --disc 'E:\Development\Android\RE4-Data\RE4_2004-11-25_Disc1.gcm' `
  --disc 'E:\Development\Android\RE4-Data\RE4_2004-11-25_Disc2.gcm' `
  --output 'E:\Development\Android\RE4-Data\import-phase3a' `
  --native-tool port/build/host-ninja/re4_asset_tool.exe --oracle-all

python port/tools/import_original.py verify-native `
  --data 'E:\Development\Android\RE4-Data\import-phase3a' `
  --native-tool port/build/host-x86/re4_asset_tool.exe `
  --report 'E:\Development\Android\RE4-Data\import-phase3a\host-x86-import-tests.json'

python -u port/tools/test_import_device.py `
  --adb 'C:\Users\andiw\AppData\Local\Android\Sdk\platform-tools\adb.exe' `
  --serial dbdcacc --data 'E:\Development\Android\RE4-Data\import-phase3a' --repeat 2
```

`report --data ...` regeneriert Sprach-/ESL-/BLK-/Raumberichte aus dem
geprüften lokalen Manifest und unveränderten Blobs. Ein regulärer Import
erzeugt diese automatisch. Bereits vorhandene Blobs werden auf SHA-256
geprüft. Ausgabe innerhalb des Repositorys oder oberhalb der Eingabeimages
wird abgelehnt; Originaldateinamen gelangen nicht in Shellbefehle oder
ungeprüfte Extraktionspfade. JSON/CSV-Ausgaben werden atomar ersetzt.

## Vollständige geänderte Projektdateien

Alle Dateien liegen vollständig im Projekt; keine Originalassets sind darin:

- `port/CMakeLists.txt`, `port/.gitignore`.
- `port/include/re4/port/asset_index.h`, `port/src/asset_index.cpp`.
- `port/tests/asset_tool.cpp`, `port/tests/test_import.py`.
- `port/tools/import_original.py`, `port/tools/test_import_device.py`.
- `port/tools/original_import/__init__.py`, `gcm.py`, `formats.py`,
  `pipeline.py`, `source_refs.py`, `reports.py`.
- `port/docs/phase3a-status.md`.

## Verbleibende Arbeit und Voraussetzungen für Phase 3b

Belegt: unveränderte Rohdateien aller FST-Einträge, vollständige Raum-
Containerabdeckung, Yz2-Gleichheit, native Body-/Containeransichten auf vier
Zielarchitekturen und aufgelöste aktive BLK-Dateireferenzen.

Offen: semantische Leser für AEV/ITA/SAT/EAT/SAR, CAM, LIT, EFF, ITM,
RTP, CNS, STB und weitere Tags; vollständige SMD-Geometrie; Text-
Kontrollcode-/Fontinterpretation; Event-Pakete und deren dynamische
Abhängigkeiten; Audio-/Videoformate und Wiedergabe. TPL-Prüfung umfasst
die unterstützten Basisformate, keine vollständigen Mipketten/CLUTs.
Die vorhandenen Modell-/FCV-/SEQ-Parser haben dokumentierte nicht
bytegleiche beziehungsweise fehlerhafte Originaleinträge. Es werden keine
Ersatzmodelle, erfundenen Nachrichten oder reparierten Gameplaydaten erzeugt.

Für Phase 3b benötigt die Laufzeit einen Manifest-/Disc-Resolver mit stabilen
IDs und einem Buffer-Besitzvertrag. `AssetIndex` liefert überprüfte
Dateispannen statt in-place relocierter 32-Bit-Pointer. `cDvd` muss später
FileTbl-/Pfadauflösung, Discwechsel, MRAM/ARAM-Zustände, Queue-Completion und
Fehlercodes mit dem realen Dateileser verbinden. `decodeData` kann dabei
den bereits geprüften nativen Yz2-Pfad nutzen; sein ursprüngliches Heap- und
Lebensdauerverhalten ist gesondert zu portieren. Der Frame-/VI-Adapter muss
die geprüfte Hintergrundbudget-/Retrace-Semantik erhalten.

Technische Annahme: diese Grenzen ermöglichen den Anschluss der vorhandenen
Original-Raummodule ohne Raumrekonstruktion. Noch kein Nachweis besteht,
dass deren gesamte pointerabhängige Engine-/REL-Anbindung bereits nativ
ausführbar ist. Phase 3b und Renderer wurden nicht begonnen. Die verbindlichen
späteren Grafikoptionen MSAA Aus/2x/4x, AF 2x/4x/8x, DoF mit Nah-/Zielfokus,
Hardwarevalidierung und persistentes backendunabhängiges Menü bleiben gültig.
