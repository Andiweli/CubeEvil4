# M26 — Dorf r101, Zerstörung, Explosionen und Originalaudio

Vollständige geänderte/neue Quelldateien über M25. Ordner `RE4-Android-Decomp`, Paket `com.ast.re4decomp`, ARM64, API21/GLES3 und die vorhandenen Disc-Imports bleiben erhalten. Version `0.26.0-dev`, Code26. Es handelt sich weiterhin um einen nativen Testlauf; die vollständige Kampagne ist noch nicht gestartet.

## Gemeinsamer Ausbau

„Kampagnendorf r101 testen“ lädt den originalen Punkt1 aus `Debug/roominfo.dat`: Position `(2684,0,-8473)`, Richtung `-1.2`. Das Ziel wird wie bei anderen Raumwechseln gegen die ursprüngliche SAT-Kapsel geprüft. Leon erhält Pistolen-/Messerbewegungen und übernimmt Munition sowie Angriffsserien. Der ausdrücklich gewählte neue Test startet mit HP1200; Vorräte und bereits zerstörte Behälter bleiben innerhalb derselben Session erhalten.

Das Testprofil entspricht dem USA-Zweig von `R101Init` vor Haus-/Turm-/Glockenereignissen, mit Raumflags6/7/8 zunächst aus. Es zeigt25 der28 ETS-Objekte; drei Leitern sind durch den Originalstartzustand ausgeblendet. Regal17 bekommt die tatsächliche Skriptposition `(6656,902,8925)`. Zusätzlich erscheinen originaler Scheiterhaufen und Schloss von Tür11. Die Skriptmodelle benutzen BIN28/TPL29 bzw. BIN26/TPL27 des Originalarchivs; sie werden nicht als zerstörbare ETS-Kisten behandelt.

Der Scheiterhaufen steht bei `(-2880,50,2810)` mit ursprünglicher Y-Drehung. Seine EST0-Flammen und Rauch starten ohne den200-Tick-Vorlauf der globalen SST-Effekte. Originale ANM-/TPL-Bilder, Emitterintervalle, Lebensdauer, Größen-/Farbentwicklung und Esp0b-Kameraoffsets sind verbunden. Pause und Kontextverlust sichern die unabhängigen Effektströme. Es werden keine erzeugten Ersatztexturen ausgeliefert.

Die Dorfwellen, Glocke, Haus-/Turmsequenzen, Tiere von r101 und geskripteten Türen benötigen noch die vollständigen Szenario-/Gegnerlaufzeiten. r101 ist deshalb ein begehbares Testgebiet mit diesen Originalsystemen, kein abgeschlossener Kampagnenlevel. Der bestehende einzelne Ganado bleibt im expliziten r400-Kampftest. Die bereits geprüften r301↔r300-Türen bleiben verfügbar; r101-Ausgänge werden nicht ohne ihre Skripte freigeschaltet.

## Behälter und explosive Fässer

r400 hat weiterhin13 Holzbehälter mit festen ITA-Inhalten und zusätzlich drei originale rote ET2D-Fässer an Slots39–41. M25 hatte deren eigene `cEmBarrel`-Klasse noch nicht angebunden; deshalb reagierten sie nicht auf Pistolenstrahlen. M26 verwendet ihre originalen Treffervolumen und EAT-Regeln. Treffer starten einmalig Explosionssequenz und Sound. Zwei spätere30-Hz-Takte lösen den ursprünglichen6000-Einheiten-Blast aus; neu gezündete Nachbarfässer beginnen unabhängig von der Slotreihenfolge mit ihrer eigenen Verzögerung.

Für Leon werden Teile0-Weltposition, Nah-/Fernbereich, EAT-Sichtprüfung, Originalschaden und Lebens-/RNG-Regeln verwendet. Nahe Treffer führen zur originalen Fall-/Aufstehanimation oder tödlichen Fallkurve; der äußere Bereich zeigt den originalen Stolperzustand ohne Lebensverlust. Der Ganado verwendet seine posierten Kapseln, Waffenklasse0x13, Schadens-/Kopfregeln und Original-Wegschleuderbewegungen. Der vorhandene statische SAT-Bewegungsadapter bleibt begrenzt; komplette Körperabtrennungen sind nicht angebunden.

Kisten und Fässer zerbersten mit echten EFM-BIN/TPL-Teilen. r400 verwendet31 Teile mit1335 vollständig texturierten Dreiecken, r10120 Teile mit923 Dreiecken. Die Originalbewegung enthält Geschwindigkeit, Rotation, Gravitation, Bodenkontakt/Abprall und Farb-/Alphaausblenden. Metallteile berücksichtigen zusätzlich die ursprüngliche SAT-Wandreflexion. Die separate Splittersimulation läuft einmal pro tatsächlichem Spieltick; Renderbilder erzeugen keine zusätzlichen Physiktakte. Ihre16-Bit-Lebensdauer und Snapshot-Prüfung berücksichtigen den ursprünglichen Überlauf.

Zerstörte Holzbehälter erhalten beim Wiederbetreten den ursprünglichen ruhenden Restzustand, ohne neue Explosion oder Beute. Kontextverlust erhält eine noch laufende Zerstörung. Modelle/Texturen werden einmal hochgeladen; die einzelnen Instanzen liefern ihre Weltmatrizen. Die GLES-Tiefenschreibunterdrückung beim Alphaausblenden ist eine ausdrücklich dokumentierte Renderanpassung; das Original verändert zunächst seine OT-Sortierung.

r101 besitzt13 ITA-gebundene Behälter, davon sieben mit `RandomItemCk`-Inhalt0x1001. Zufallsbeute wird erst beim Zerstören bestimmt, mit dem aktuellen Normal-Leon-Testprofil, Gesundheit, Munitions- und Heilvorräten. Auch kein Fund ist ein gültiges Originalergebnis. Das Laden der möglichen echten ITM-Modelle verbraucht keinen Zufall. Nur die tatsächlich aufgelöste ID wird gezeichnet;62 vorbereitete Varianten verwenden10 originale Modelle. Aufnahme und Vorratsverwaltung behalten die bereits vorhandenen Einschränkungen des Testinventars.

## Originalaudio

Die App liest die ISS-/PCM-Blöcke aus den bereits importierten DVD/DRS-Dateien. Die dekodierten Geometriearchive enthalten diese Audioblöcke nicht. DSP-ADPCM wird mit ursprünglichen Koeffizienten, Hardware-Rundung und Schleifenkontext in PCM16 dekodiert. Keine Tondateien werden im Quellpatch gebündelt.

| Aktion | Originaler Soundblock/Eintrag |
|---|---|
| Pistole | 2/0 mit ursprünglicher verketteter2/1, zusätzlich2/2 |
| Nachladen | 2/0x16 |
| Messer | 1/3 am ursprünglichen Motion-Frame3 |
| Kiste | 1/0x1a |
| Holzfass | 1/0x3c mit ursprünglicher Zufallsgruppe |
| Rotes r400-Fass | 6/0x56 |
| Dorfbrand r101 | 6/0x58 bei `(-2880,1550,2810)` |

STB-Kurven steuern ursprüngliche Distanzlautstärke, Tonhöhe, Stereoposition und Distanzfilter. Quelllautstärketabelle, Sampleverkettung, Schleifen, Fassvarianten und Tonhöhenzufall bleiben erhalten. Die Ausgabe benutzt einen eigenen AudioTrack-Thread mit48kHz-Stereo, API21-kompatibel. Native UI- und Spielgates halten den Mixer bei Menü, Hintergrund, Raumladen und Start-Pause an. Pause liefert Null-PCM und verschiebt keine Samplepositionen; der Java-Thread leert gepufferten Ton beim UI-Pausieren.

Neue Raumbänke werden erst nach erfolgreichem Grafik-Commit eingesetzt. Eine fehlgeschlagene CPU-/GPU-Ladung behält die vorherige Bank und ihre Stimmen. Derselbe GL-Kontextneustart lädt Grafik erneut, ohne den Audiozustand zurückzusetzen. Ein neuer Testprozess besitzt weiterhin kein dauerhaftes Savegame.

Das ist ein erster Original-Soundeffektpfad. Musik/MIDI-/Streaming-BGM, Hall, vollständige Schrittsequenzen, Gegnerstimmen und Innenraum-/Verdeckungslogik fehlen noch. Die native Mischung verwendet lineares Resampling statt des vollständigen GC-AX-Ausgabefilters.

## Wandkamera

Der Akteurrenderer zeichnete zuvor Kopf-/Haar-Rückseiten. M26 verwendet die an den tatsächlichen Modellen geprüfte Flächenorientierung und einseitige Ausblendung. Bei sehr naher Wand reicht das allein nicht, weil die Kamera tatsächlich in den Kopfbereich rücken kann.

Nur Leons ursprüngliche Kopfattachments BIN2/4/5 erhalten dann eine kontinuierliche, räumlich begründete Coverage-Ausblendung anhand ihrer animierten Kopfgrenze und der Nahfläche. Schulter, Körper, Arme und Pistole bleiben mit regulärer Tiefenprüfung sichtbar. Die Kamera wird nicht durch die Wand geschoben. Dies ist eine gezielte Portanpassung für die gemeinsame Schulterkamera, keine behauptete Original-Kopfregel. Die Kamera-/Stickrichtungen aus M25 bleiben erhalten.

## Effektgrenzen

Die ET2D-Explosion zeigt Originalfeuer, Flash-/Rauch-Sprites und die gesonderten EFM-Holz-/Metallteile. Filter03-Alpha-Glow, dynamisches Licht, Bodendecals, Trails, Screen-Flare und Kameraerschütterung sind noch eigene Backends. Sie werden nicht durch erfundene Vollbildeffekte ersetzt. Weitere gebietsabhängige r101-EAR/SST-Effekte bleiben offen.

Der Sprite-Trigger hängt an der zuletzt gezeichneten30-Hz-Szenenuhr; seine Phase kann gegenüber dem Gameplay-Callback um einen Tick versetzt sein. Schaden/Verzögerung und Splitterphysik hängen dagegen am tatsächlichen Spieltick. Visueller und Audiozufall sind vom Kampfzufall getrennte Streams. Die vollständige Original-Verarbeitungsreihenfolge der gesamten Engine wird nicht behauptet.

## Einspielen und kurzer Gerätetest

1. ZIP über das vollständige M25-Projekt entpacken, Dateien unter `RE4-Android-Decomp/` ersetzen. `app/.cxx` und `app/build` entfernen; Gradle-Sync und `clean assembleDebug`. Die bisherigen Disc-Imports weiterverwenden.
2. „Kampagnendorf r101 testen“: Feuer/Rauch ansehen, eine Kiste mit Messer oder Pistole zerstören und einen angebotenen Fund aufnehmen. Schuss-, Messer- und Behälterton prüfen. An einer engen Wand sollen Kopf-Rückseiten den Blick nicht mehr verdecken.
3. Im Ganado-Nahkampftest r400 ein rotes Fass treffen. Explosion, Splitter und Ton prüfen; Abstand halten bzw. einmal die Schadensreaktion testen. Menü/Start-Pause und Hintergrund/Vordergrund sollen den laufenden Zustand erhalten.

Ein solcher kurzer Systemtest genügt für diesen Patch. Generische Parser-/Audio-/Physik-/Rendererregressionen werden automatisiert geprüft; manuelle Sondertests gelten besonders für neue Skripte, Raumstarts und Ausgänge. Das vollständige vorhandene142-Raum-Audiokorpus wurde bereits automatisch geprüft.

Bei einem Fehler vollständiges Logcat mit `RE4Flow`, `RE4Player`, `RE4Weapon`, `RE4Enemy`, `RE4Destruction`, `RE4Audio`, `RE4WorldEffects` und `RE4Scene` senden. Abschlussprüfungen und Ausführungsgrenzen: [VALIDATION.md](VALIDATION.md). Originalquellen und deklarierte Anpassungen: [SOURCE-NOTES-M26.md](SOURCE-NOTES-M26.md).
