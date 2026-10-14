# K11 – LOAD/SAVE-Flackern und Sprachen auf beiden Discs

Stand: 7. Oktober 2026. Android-Version **0.37.11-dev**, Versionscode **44**. Grundlage ist der vollständige K10-Quellstand und das bereits übermittelte LOAD-Video.

## Ursache und Vergleich mit dem GameCube-Code

Die weißen, wechselnden Flächen im LOAD-Video stammen aus fremden Effekttexturen. Die K10-Korrektur der Animationsauswertung reichte nicht aus, weil der Port zuvor bereits die Texturen zweier unterschiedlicher Besitzer vermischt hatte.

`upstream/re4/src/game/card.cpp::CardID::init` ruft zuerst `IdTexRoomInit()` und danach `IdTexDataLoad(pTex, TEX_OWNER_ID_EVENT)` auf. Damit verwendet der originale CARD-Bildschirm ausschließlich seinen eigenen Texturensatz aus `ss/Cmn/save_e.dat`.

Der Port lud diesen Satz in `startup_data.cpp`, ergänzte dann jedoch jede vermeintlich fehlende Kombination aus Textur-ID und Bildnummer aus den gemeinsamen SS-/CORE-Ressourcen. Eine gleiche ID bezeichnet dort eine andere Textur. Konkret besitzt die CARD-Textur 39 genau ein Bild: die Beschriftung oberhalb der Spielstandliste. Die CORE-Textur 39 besitzt dagegen 15 Bilder für einen Effekt. Der Port ergänzte deren Bilder 1 bis 14 im CARD-Satz und überschrieb dabei auch die Animationsanzahl mit 15. Die Java-Darstellung spielte somit die fremden Effektbilder sichtbar ab. Die extrahierten Bilder entsprechen den weißen Mustern im Video.

K11 entfernt diese Ergänzung aus dem CARD-Satz. LOAD und SAVE erhalten ihre Bilder und Animationsangaben jetzt vollständig von ihrem ursprünglichen Besitzer. Die originalen Ziffern- und sonstigen ausdrücklich ausgewählten Bilder bleiben erhalten.

| CARD-Kopf, lokale Textur-ID 39 / Port-ID 551 | Originaldaten | K10 | K11 |
| --- | ---: | ---: | ---: |
| Veröffentlichte Bilder | 1 | 15 | 1 |
| Reguläre Animationsbilder | 1 | 15 | 1 |
| Fremde Effektbilder im CARD-Satz | 0 | 14 | 0 |

Diese Angaben wurden für die SAVE-Gruppe 2005 und die LOAD-Gruppe 2008 unmittelbar aus der veröffentlichten Layout-JSON geprüft. Die frühere K10-Prüfung suchte nach `:1` und akzeptierte dadurch auch `:15`. Der korrigierte Test prüft die vollständigen Zahlen einschließlich Trennzeichen und vergleicht zusätzlich die gesamte CARD-Texturzuordnung mit dem eigenen Originalarchiv: Bildabmessungen, sämtliche Pixel, Animationsanzahlen und fehlende Bilder. Mit der K10-Zuordnung schlägt dieser Test fehl; mit K11 besteht er auf den Ressourcen beider Discs.

Die Gesundheitsanzeige erhält in K11 keine weitere Änderung. Die weitere Überarbeitung des Inventars bleibt dem späteren Arbeitsschritt vorbehalten.

## Tatsächliche Sprachdaten beider DVDs

Geprüft wurden die vollständigen Images `RE4_2004-11-25_Disc1.gcm` und `RE4_2004-11-25_Disc2.gcm`, einschließlich ihrer originalen Dateiverzeichnisse und der darin enthaltenen Archive. Beide melden Spiel-ID `G4BE08`, Revision 0; das Disc-Nummernbyte ist 0 beziehungsweise 1. Disc 1 enthält 1.154 Dateien, Disc 2 enthält 1.090 Dateien.

**Auf beiden Discs sind neben Englisch echte japanische Text- und Schriftressourcen vorhanden. Eine vollständige deutsche, französische, spanische oder italienische Lokalisierung ist in diesen Daten nicht enthalten.** Die vorhandenen Sprachkennungen allein lassen sich nicht als funktionierende Übersetzungen freischalten.

| Sprache | Befund auf Disc 1 und Disc 2 |
| --- | --- |
| Englisch | Englische Menüarchive in `ss/Eng` und `etc/Eng`, englische Texte und LOAD/SAVE-Grafiken vorhanden. |
| Japanisch | Japanische Texte in den Nachrichtentabellen, eigene Schriftdateien wie `font/common_j.fnt` und japanische LOAD/SAVE-Grafiken in `save_j.dat` vorhanden. Ein vollständiger eigener Menüordner `ss/Jpn` fehlt; eine vollständig spielbare japanische Fassung wurde damit nicht nachgewiesen. |
| Deutsch | `save_g.dat` und Sprachslot vorhanden, aber keine vollständige deutsche Übersetzung. |
| Französisch | `save_f.dat` und Sprachslot vorhanden, aber keine vollständige französische Übersetzung. |
| Spanisch | `save_s.dat` und Sprachslot vorhanden, aber keine vollständige spanische Übersetzung. |
| Italienisch | `save_i.dat` und Sprachslot vorhanden, aber keine vollständige italienische Übersetzung. |

Die vier Archive `save_g.dat`, `save_f.dat`, `save_s.dat` und `save_i.dat` sind untereinander bytegleich. Ihre decodierten LOAD-/Listenbeschriftungen sind Englisch. Dagegen unterscheiden sich `save_e.dat` und `save_j.dat` tatsächlich, und die japanischen Beschriftungen sind in den decodierten Bildern sichtbar.

Das vollständige Dateiverzeichnis enthält unter `ss` ausschließlich `Cmn`, `Eng` und `Item`; sprachabhängige Unterordner unter `etc` existieren ausschließlich für `Eng`. Passende deutsche, französische, spanische und italienische Menüarchive fehlen auf beiden Discs.

Auch die Texte wurden geprüft, nicht nur die Dateinamen. Die Sprachreihenfolge im Originalcode lautet Japanisch, Englisch, Deutsch, Französisch, Spanisch, Italienisch. Nach Entfernen des Dateipaddings und Auswertung der Steuerwortlängen aus `mes.cpp::Message::CommandArg` sind die vier westlichen Sprachslots in sämtlichen geprüften Tabellen untereinander identisch:

| Textbereich | Disc 1 | Disc 2 | Ergebnis der vier Slots DE/FR/ES/IT |
| --- | ---: | ---: | --- |
| CORE-Menütexte | 153 Nachrichten je Slot | 153 Nachrichten je Slot | Identische Inhalte; zahlreiche Glyphenindizes außerhalb der westlichen gemeinsamen Schrift. |
| Memory-Card-Meldungen | 45 Nachrichten je Slot | 45 Nachrichten je Slot | Identische Inhalte; zahlreiche Glyphenindizes außerhalb der westlichen gemeinsamen Schrift. |
| Befüllte Raum-Nachrichtentabellen | 72 Tabellen | 45 Tabellen | In jeder Tabelle alle vier Slots identisch. |
| Leere Debug-Raumtabellen | 13 Tabellen | 12 Tabellen | Nur unbefüllte Platzhalter. |
| Funkdialogtabellen | 24 Tabellen | 24 Tabellen | In jeder Tabelle alle vier Slots identisch. |

Diese wiederholten Inhalte sind keine vier eigenständigen Übersetzungen. Die genaue Zusammenfassung, Archivprüfsummen und normalisierten Textprüfsummen stehen in `K11-SPRACHPRUEFUNG.json`. Doppelt auf beiden Discs vorhandene Daten werden in den jeweiligen Disc-Zahlen separat gezählt. Die Untersuchung prüft die Lokalisierungsressourcen; sie behauptet keine vollständige Hörprüfung sämtlicher Audiodateien.

## Validierung

| Prüfung | Ergebnis |
| --- | --- |
| Neuer CARD-Test gegen K10 | Fehler reproduziert: fremde SS-/CORE-Bilder im CARD-Satz. |
| Titel-/Options-/CARD-/Font-Ressourcen aus Disc 1 | 937 Prüfungen bestanden. |
| Dieselbe Prüfung mit separat aus Disc 2 extrahierten Daten | 937 Prüfungen bestanden. |
| Native CTest-Vertragstests | 43 von 43 bestanden. |
| Originales Speicherformat / native Zuordnung | 79 Prüfungen bestanden. |
| Originales Speicherformat / Java | 56 Prüfungen bestanden. |
| CARD-Ablauf / Rückfrage-Geometrie | 40 beziehungsweise 46 Prüfungen bestanden. |
| Android-ARM64-Build | Vollständiger Neuaufbau; APK-Inhalt auf aktuelle Version und geänderte native Bibliothek geprüft. |
| Signierte APK | Bisheriges Zertifikat; v1/SHA1-RSA und v2 sowie 16-KiB-Ausrichtung geprüft. |

Die Original-Texturzuordnung und der konkrete Fehler sind damit geprüft. Die neue APK wurde hier nicht auf einem Android-Gerät ausgeführt. Ein Bildvergleich der gesamten LOAD-Seite auf dem Zielgerät bleibt für den Nachweis der vollständigen GameCube-Darstellung erforderlich.

## Dateien und Einbau

`RE4-M37-K11.apk` kann als Update der bisherigen App installiert werden. Das bisherige Signaturzertifikat wird verwendet, sodass vorhandene App-Daten und Spielstände bei einer Update-Installation erhalten bleiben.

`RE4-Android-Decomp-R100-Korrekturen11.zip` enthält die vollständigen aktuellen Ersatzdateien der K5–K11-Korrekturen und `FILES-K11.sha256`. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem vorhandenen Projekt zusammenführen und die entsprechenden Dateien ersetzen. Das Archiv enthält vollständige Dateien, keine Diff-Fragmente.

`RE4-Android-Decomp-K11-Quellstand.zip` enthält alternativ das vollständige Projekt. Signaturgeheimnisse, Keystore, lokale SDK-Pfade, Build-Verzeichnisse und extrahierte DVD-Daten werden nicht mitgeliefert.
