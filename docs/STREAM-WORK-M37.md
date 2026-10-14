# M37 – Gleichzeitige Originalstreams und Sound-Fades

Referenzen: `game/snd.cpp::SndStrReq/SndRoomStrStart/SndRoomStrStop`, `game/snd_str1.cpp::str_req_nml_fade_set/str_fade_check/str_fade_new_vol_set`, `game/snd_str3.cpp::Snd_str_ax_voice_play`, `game/snd_main.cpp::cb_audio_frame` und `st1/r100.cpp`.

Die ursprüngliche Engine besitzt vier Stream-Arbeitsplätze. Streams werden nach Block und Nummer adressiert; Block0 ist Raummusik, Block1 ist Eventaudio. Die Zahl3 in `SndRoomStrStop(3)` ist kein Streamkanal. Der zu stoppende Raumstream ist die niedrige Byte-Nummer aus `room_str_tbl[0]`.

`SoundMixer` registriert unveränderliche PCM-Daten mit `installStreamAssets()` und startet sie erst mit `requestStream(block,number,flags,time,volume)`. Kommando1 bereitet einen eigenen residenten Arbeitsplatz vor, ohne PCM oder dessen Fade weiterzuführen; `streamReady()` meldet diesen vorab dekodierten Besitzer. Erst Kommando2 startet die tatsächlichen Stimmen am ersten Sample. Kommando3 verbindet beide Anforderungen. `streamPlaying()` unterscheidet einen nur vorbereiteten Besitzer von laufendem PCM. Damit erhält das Radio den ursprünglichen Request → WaitReady → Play-Ablauf. Die vier Arbeitsplätze besitzen jeweils eigene PCM-Phase, Loopzustand, Ausgangsframezählung, Originallautstärke und Fadezustand. Ein Stream ersetzt beim Start keinen anderen. Wiederholte signierte Raum-Startanforderungen behalten dessen Position. Der bestehende `installStream()`-Aufruf kontrolliert nur seinen ausgewählten Stream und lässt andere adressierte Raummusik-/Eventstreams weiterlaufen; damit bleibt die bisherige Titel-/Sprechsound-Eigentümerschaft erhalten.

Die Fade-Zeit zählt AX-Audiocallbacks zu jeweils5ms. `SndRoomStrStop(3)` übergibt600 dieser Callbacks. Die r100-Anforderungen für die Eventstreams Block1/Nummer4,5,6 übergeben400, ungefähr zwei Sekunden. Die Lautstärke wird wie im Original als vorzeichenbehafteter8.8-Festpunktwert behandelt. Der Schritt ist die ganzzahlige Division `(Ziel-Ist)/Zeit`, bei einem Nullschritt auf±1 begrenzt. Jeder Audiocallback verschiebt und klemmt diesen Wert. Nach dem Erreichen von Null beendet erst der folgende Callback den Arbeitsplatz. Deshalb kann die wirkliche Dauer etwas über der nominalen Zeit liegen: von127 nach0 bei Zeit400 sind es402 Lautstärkeschritte und ein weiterer Beendigungs-Callback. Kommando8 verwendet die originale50-Callback-Ausblendung. Fade-In behält die separate ursprüngliche Division des Playback-Starts bei.

Die Ausgabe benutzt den ursprünglichen Lautstärketabellenwert für das obere Byte, nicht einen linear interpolierten PCM-Gain. Die Gain-Berechnung erfolgt bei Lautstärkeänderungen; der Audio-Samplepfad besitzt keine neue Exponentialberechnung pro Sample. Der5ms-Takt läuft unabhängig von der Größe des Android-PCM-Puffers. Lifecycle-Pause friert Playback und diesen Takt ein. Sämtliche Kontroll- und PCM-Zugriffe bleiben unter demselben kurzen Mixer-Mutex; es gibt beim Rendern keinen Disc-Zugriff.

Für den Raumskriptbesitzer entsprechen folgende Aufrufe den verwendeten Originalroutinen:

| Originalaufruf | Nativer Adapter |
|---|---|
| Radio: Vorbereitung ohne Playback | `requestStream(1,eventNumber,1)` |
| Radio: vorbereitete Sprache abspielen | `requestStream(1,eventNumber,2)` |
| Neuer residenter Raumstream | `requestStream(0,roomNumber,0x80000003)` |
| Laufender, bereits ausblendender Raumstream wird wieder benötigt | `requestStream(0,roomNumber,4,600,defaultVolume)` |
| `SndRoomStrStop(3)` | `requestStream(0,roomNumber,4,600,0)` |
| r100 Eventstream-Ausblendung | `requestStream(1,eventNumber,4,400,0)` |
| Originales Stop-Kommando | `requestStream(block,number,8)` |

`streamActive()`, `streamStopping()`, `streamRenderedFrames(block,number)` und `streamVolumeFixed()` stellen den nötigen Zustand ohne Zugriff auf die unveränderliche PCM-Datei bereit. `replaceRoom` ersetzt die residenten Block0-Daten und lässt eine bereits laufende, auch im Zielraum vorhandene Nummer bestehen. Eventdaten bleiben nach ihrem eigenen ursprünglichen Eigentümer getrennt.

`m37_streams_check` besteht87.681 unabhängige Prüfungen ohne Disc-Dateien. Mit dem tatsächlichen Disc-1-Dateiverzeichnis besteht es88.910 Prüfungen. Geprüft sind245 vollständige Original-Festpunktfadekombinationen, Zeit-/Nullgrenzen, PCM-Summen aller vier Arbeitsplätze, Phasenerhaltung, Pause, Begrenzung auf vier aktive Streams, Stop/Resume sowie die tatsächlichen r100-Raum-/Eventstreams aus `bgmtbl.dat`, `bio4str.hed`, `bio4bgm.sbb` und `bio4evt.sbb`.

Die vorhandenen Audio-CPU-Verträge bestehen362 Prüfungen. Der tatsächliche ursprüngliche START-GAME-Sprechsound besteht82.091 Prüfungen und spielt seine beiden verbundenen Schichten vollständig über226.080 Ausgangsframes beziehungsweise4,71 Sekunden ab. Die ursprüngliche90-Frame-Übergangsphase ist kürzer als diese Sprache und beendet sie nicht vorzeitig.

Dieser Adapter enthält die für die portierten r100-Routinen benötigten Start-/Fade-/Stop-Anforderungen. Er ersetzt weder die asynchrone GameCube-DVD-/ARAM-Pufferverwaltung noch die vollständigen System-Master-/Fehlerfadezustände. Die PCM-Daten werden vorab originalgetreu dekodiert. Für direkten Fade mit Zeit0 verwendet der native Adapter eine sofortige Lautstärkeänderung, statt die originale Division durch Null nachzubilden; die r100-Aufrufe verwenden positive Fadezeiten. Die Ausgabe muss weiterhin auf dem Android-Gerät geprüft werden.
