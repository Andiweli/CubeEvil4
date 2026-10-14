# Originalaudio in M32

M32 liest die musikalischen Sequenzen, Instrumente, ADPCM-Samples und Raum-
Umgebungsgeräusche aus den importierten Originaldateien. Es werden weder ein
Android-Soundfont noch Ersatzmusik oder künstlich erzeugte Dorfgeräusche benutzt.
Der Patch enthält keine Spieldaten.

## Quellen und Datenfluss

Referenz ist der eingebundene Decomp-Stand
`9b76ff003081d6ece8b838453fb8310743a972c7` in `upstream/re4`.

| Originalquelle | Übernommene Bedeutung | Android-Code |
| --- | --- | --- |
| `src/game/snd.cpp`, `SndBgmTblInit/Set`, `SndRoomBgmStartCheck/Start` | Raum-/Szenariotabelle, zwei BGM-Slots, Bank-/Sequenznummer und automatische Startflags | `sound_music.cpp`, `android_bridge.cpp` |
| `src/game/snd_seq1.cpp`, `snd_seq2.cpp`, `snd_seq3.cpp` | MIDI-Timing, Tempo, Programme, Noten, Controller und ursprünglicher Sequenzloop | `sound_music.cpp` |
| `src/lib/synenv.c`, `synmix.c`, `synctrl.c`, `synvoice.c`, `synpitch.c` | Originale DLS-Instrumente, EG1-Lautstärkehüllkurve, Stimmung, Controller, Pan und SYN-Lautstärke-/Attack-/Keytabellen | `sound_music.cpp`, `sound_synth_tables.h` |
| `src/game/snd_sub3.cpp`, `snd_str0.cpp`, `snd_str2.cpp`, `snd_str3.cpp`, `snd_str4.cpp` | SHD-/RIT-Streamheader, DSP-Historien, Stereo-Blockfolge und Loopkontext | `sound_music.cpp`, `sound_mixer.cpp` |
| `src/game/se_at.cpp` | ESE-Aktivierung, Positionen, Verzögerung, Aufrufanzahl und Original-Zufallsfolge | `sound_ambient.cpp` |
| `src/game/snd.cpp`, `SndSubScreenInit/Exit`, SE-Anforderungsflags | Subscreen-SE-Mastervolume 63/127, pausierbare Weltgeräusche und erlaubte Menüsounds | `sound_bank.cpp`, `sound_mixer.cpp` |

Die eigentliche Audiowiedergabe bleibt der vorhandene AudioTrack-Pfad mit
48-kHz-Stereoausgabe. Der Audio-Thread erhält ausschließlich unveränderliche
Sample-/Sequenzdaten und Mixerbefehle; er greift nicht auf den Spielzustand zu.
Dateien werden bei der Raumvorbereitung gelesen, nicht im Render-Callback.

## Benötigte Originaldateien

Alle folgenden Dateien liegen auf Disc 1 unter `files/bgm/`. Der Import erhält
ihre Ordnerstruktur; `configureMusic` bekommt dieses `bgm`-Verzeichnis. Vorherige
Installationen müssen den Disc-Import aktualisieren, falls die Dateien noch
fehlen. CORE-/Raum-Soundbanken bleiben zusätzlich erforderlich wie bisher.

| Datei | Bytes | SHA-256 der geprüften G4BE08-Disc-1-Datei |
| --- | ---: | --- |
| `bgmtbl.dat` | 9152 | `7739345cf4c9eab774267a8c7170cb69ecdd4d40cb29ef55669351b23e6d43a2` |
| `bio4midi.hed` | 96 | `44ae44850ab91fdad2e0ae49ecaf6be05b41562262ac6b96baa175b8493a7f8a` |
| `bio4midi.dat` | 9647776 | `7b1adae9937384b2660bffb65007cbee3def2187fd5ec64273b8f75cbb5dff95` |
| `bio4str.hed` | 44384 | `07b3a40edbcab0ff6ae78532c055523bab4f4ebebbeb32b7c330667a149a9220` |
| `bio4bgm.sbb` | 139395072 | `6c4fdd0833146022a6a7778bb7f960e7177ab28b38d8dcd681f432516bf8099c` |
| `bio4evt.sbb` | 193495040 | `11871acb4761df6f3f814799e898b072aa38d249273f369eb5e87e8c5184b544` |

`bio4evt.sbb` enthält Ereignis-/Zwischensequenzstreams; der Decoder unterstützt
diese Daten, M32 startet jedoch keine erfundenen Zwischensequenzereignisse.
Es wird jeweils nur der ausgewählte komprimierte SBB-Track eingelesen.
Dekodierte Streams können mehrere zehn Megabytes belegen. Der Raumcache hält
deshalb `weak_ptr`: fortlaufende Musik zwischen benachbarten Räumen behält ihre
Identität, nicht mehr benötigte Tracks werden freigegeben.

## Dorf und andere Räume

Der originale Standarddatensatz von r101 enthält BGM `0xc000`: Bank 0,
Sequenz 0, resident und automatisch. Die Bank enthält die tatsächlichen
Dorf-Instrumente und 1387 ausgespielte MIDI-Ereignisse. Der Originalloop beginnt
bei 24140 ms und erreicht sein Ende bei 85708 ms; die SIT-Lautstärke ist 90.
Die Banknummer liegt in den unteren acht Bits, die Sequenz in Bits 8–9.

Der r101-Stream `0x8002` ist resident, **aber nicht automatisch**. Er gehört zu
den skriptgesteuerten Kampf-/Begegnungsereignissen und läuft deshalb im Dorf
ohne Gegner nicht als Ersatz-Hintergrundmusik.

r101 enthält außerdem genau zwei aktivierte ESE-Datensätze: Bank 6,
Sounds `0x57` und `0x56`, an ihren Originalpositionen. Jeder Datensatz wird
einmal angefordert; ein gegebenenfalls vorhandener Dauerloop gehört zum
ursprünglichen Sample. Das vorhandene Scheiterhaufen-Geräusch ist Bank 6,
Sound `0x58`. Raumgeräusche verwenden weiterhin die originalen Akustikbereiche.

Alle Räume bekommen die vorhandenen ESE-Datensätze und die tatsächlich als
automatisch markierte Standardmusik. M32 interpretiert keine noch nicht
portierten Kampagnenflags oder Cutscene-Skripte. Der geprüfte Standarddatensatz
enthält automatische SHD-Musik in folgenden 21 Räumen:

| Räume | Originalstream |
| --- | ---: |
| r111, r113 | 9 |
| r20c, r30b, r30c, r30e | 20 |
| r304, r326 | 26 |
| r312 | 25 |
| r333 | 49 |
| r406, r407, r408, r409, r40a, r40b, r40c, r40d, r40f, r410, r411 | 39 |

Die 127 geprüften Raumarchive enthalten insgesamt 104 ESE-Emitter. Sieben
Datensätze in r311, r20b, r40c, r31c und r309 haben gleichzeitig Aufrufanzahl,
Intervall und Zufallsbereich null. Der referenzierte C++-Pfad würde dabei
`Rnd() % 0` ausführen. M32 fordert diese Originalsounds sicher einmal an und
erhält ihren Sampleloop, statt eine nicht belegte Wiederholungsfrequenz zu
erfinden. Diese begrenzte Behandlung wird über `unsupportedRepeats()` sichtbar;
die beiden r101-Emitter sind davon nicht betroffen.

## Koffer, Menüs und Android-Lifecycle

Der Subscreen pausiert vorhandene, nicht ausgenommene Welt-SE an ihrer aktuellen
Sampleposition. Die ursprüngliche SIT-Flagbit-2-Ausnahme lässt die CORE-
Menügeräusche weiter spielen; kurze auslaufende Einmalgeräusche dürfen wie im
Original enden. BGM und SHD-Musik laufen im Koffer weiter. Der SE-Master wird
dabei auf 63 gesetzt und beim Schließen auf 127 zurückgestellt.

Das Verlassen/Hintergrundsetzen der Activity pausiert den gesamten Mixer,
einschließlich Musik. Die EGL-Neuerstellung ersetzt ausschließlich GPU-Ressourcen:
sie startet weder Sequenzen neu noch feuert sie ESE erneut ab. Ein neuer
Spiel-Sessionstart leert die aktiven Mixerbesitzer.

## Prüfung und verbleibende Grenzen

`m32_audio_check` validiert alle 17 MIDI-Banken mit 23 originalen Sequenzen und
alle 307 SHD-/RIT-Referenzen: 67 BGM-Streams und 240 Ereignisstreams. Darunter sind
originale ADPCM-Koeffizienten, getrennte Stereo-Historien, 16-KiB-
Links-/Rechtsblockfolge, Loopausdehnung, SIT-Lautstärke, Puffergrößenunabhängigkeit
und Subscreen-Pause/Resume. Der vollständige Lauf erreicht 1648 Prüfungen.
Der verkürzte ASan-/UBSan-Lauf erreicht 76 Prüfungen ohne gemeldeten Fehler.
Die 127-Raum-ESE-Prüfung hat 104 Emitter und keine Parsefehler ergeben.

```sh
m32_audio_check DISC1_FILES R101_DAT --all-streams
m32_audio_runtime_check R101 PL00 WEP01 roominfo CORE EM12 emleon00 WEP02 WEP07 BGM_DIR R111 R113
```

Der Runtime-Test verwendet die tatsächliche JNI-/AudioTrack-Ausgabefunktion,
Raumvorbereitung, Raum-Audiocommits und GLES-Kontextneuerstellung. Sein Lauf
erreicht 68 Prüfungen. Er prüft
r101-Musik/ESE ohne Kampfmusik, Koffer-/Activity-Pause, originale Menügeräusche,
fortlaufende SHD9-Musik zwischen r111 und r113 sowie PCM-Freigabe nach dem Ende
aller aktiven Streambesitzer. Die Testprogramme benötigen lokal vorhandene
Originaldateien; diese gehören nicht zum Patch.

M32 ist keine bitgenaue Emulation des GameCube-Audiosystems. Die Ausgangssamples
und Instrumente sind original, aber die Software verwendet lineares Resampling
auf 48 kHz anstelle des AX-Hardware-SRC. AXFX-Reverb/Chorus ist noch nicht
portiert. Nicht unterstützte LFO-/Pitch-Hüllkurven, Keygroups oder Drum-
Artikulationen werden ausdrücklich abgelehnt; alle 23 Sequenzen der geprüften
Discs passieren die vorhandene Unterstützung. Ereignisabhängige Musikwechsel
und deren Fades benötigen später die zugehörigen Kampagnen-/Cutscene-Skripte.
