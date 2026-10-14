# M22 — Originalhimmel und animiertes Wasser

Änderungspaket auf **M21**: vollständige neue/geänderte Dateien, ausschließlich unter `RE4-Android-Decomp/`. Paket `com.ast.re4decomp`, Version22/`0.22.0-dev`. ARM64, minSdk21 und GLES3 bleiben erhalten.

## Sichtbares Ergebnis

- **r100:** vorhandene Himmelsgeometrie und ihr SMX-Texturversatz; beide Teichflächen nutzen denselben originalen 64×64-Effektkanal. Wasserbewegung, Alpha240, Brechungsstärke30 und Mischverhältnis128 stammen aus `r100.cpp`.
- **r300:** zwei originale Himmelsobjekte mit unterschiedlichen Gelenk-Drehgeschwindigkeiten; Meerwasser mit128×128-Ziel und eigener32×32-Spritzerschicht. Letztere startet ihre originalen Effektgruppen nach30 und70 Spieltakten.
- **r301:** zwei originale Himmelsobjekte, eines davon mit Gelenkrotation; Meerwasser aus EST3 und den60 originalen TPL-Animationsbildern.

Der Himmel wird im Hintergrund an der fernen Tiefe gezeichnet. Seine ursprüngliche Geometrie, Platzierung und Kameraübersetzung bleiben erhalten; die normale Raumprojektion und deren Tiefenpräzision werden nicht durch eine pauschale größere Sichtweite verschlechtert. Boden/Wände und Figuren schreiben ihre Tiefe vor dem neuen Wasserpass. Wasser prüft dieselbe Tiefenverdeckung. Der M21-Vollbildmodus, Zurückpfeil, pausiertes Menü, Controller-Mapping und horizontale Zieldrehung bleiben Teil des Projekts.

## Daten und Originalbezug

Keine zusätzlichen Downloads oder neuen Originaldateien nötig. Der vorhandene Disc-1-Import enthält `disc1/files/etc/core.das` (2.525.120 Bytes, SHA-256 `018c6ddd4aa65a3f6325e2fbda4fecc1afd2b7fbb2695146a828f3c641abfaad`). Die Datei ist trotz ihrer Endung ein DRS-Container. Java prüft die Datei; JNI übernimmt den **ersten** EFF-Block. Räume liefern SMD, SMX und ihren ersten EFF-Block aus den bereits geprüften Raumarchiven. r301 hat mehrere EFF-Blöcke; die spätere Sequenzsammlung ist kein Ersatz für den primären Wasserblock.

Die Zuordnungen der Objekt-Work-IDs und EST-Nummern kommen aus den unveränderten `st1/r100.cpp`, `st3/r300.cpp`, `st3/r301.cpp`. Die Bewegung folgt den unterstützten Zweigen von `esp_sub.cpp`, `esp08.cpp`, `esp47.cpp`, `espgen00.cpp` und `obj02.cpp` im eingefrorenen Upstream. Ressourcen werden byteweise mit Big-Endian-Offsets gelesen; GameCube-Zeiger werden nicht auf ARM64 umgebogen.

Unterstützt werden die benötigten Bildschirm-Effekte Esp00, Esp08 und Esp47 sowie wiederholende Emitter00: Set_time, Zufallsintervalle, feste Emitter-Seeds, Set_num+1, Bewegung/Beschleunigung/Dämpfung, Skalierung, Farb-/Alpha-Hüllen, ANM-Zähler und Schleifen, UV-Versatz und Bildschirm-Wiederholung. Die ANM-Bilder werden direkt durch Ptn_no ausgewählt; die ungenutzten Bytes vor der optionalen Dauertabelle sind keine Bildindexliste. 8-/16-Bit-Zähler behalten ihre ursprünglichen Überläufe.

Nur `Tool_flg 0x10000` gehört in das jeweilige Wasser-Texturziel. r300 EST4 enthält zusätzlich Welt-Effekte, die dafür ausgelassen werden müssen. Die originalen Bildschirm-OT-Reihenfolge, sechs Blendmodi und Farb-/Alpha-Verstärkungen werden berücksichtigt. Offscreen-Ziele werden bei doppelter Auflösung gezeichnet und auf die Originalgröße reduziert; die Bildschirmkopie für r100 erfolgt vor dem Wasserpass in eine separate Textur. Dadurch entsteht keine Rückkopplung zwischen gelesener und beschriebener Wassertextur.

Die Animation läuft mit der vorhandenen30-Hz-Szenenuhr. Ein separater visueller Zufallszustand beeinflusst weder Waffen-/Treffer-RNG noch Gegnerverhalten. CPU-Snapshots sichern Himmel, Emitter, Sprites und Sequenzfortschritt für Pause, EGL-Kontextwiederherstellung und Raumwechsel-Rücknahme. Zugehörige GL-Namen werden im verlorenen Kontext nur vergessen; neue Texturen/FBOs werden im neuen Kontext aufgebaut. Fehlende oder nicht unterstützte Effektdaten werden mit `RE4Environment` gemeldet und behalten die ursprüngliche Basistextur.

## Update und Gerätetest

1. ZIP über den vorhandenen **M21**-Ordner entpacken, Dateien ersetzen. Keine doppelte Projektordner-Ebene anlegen.
2. Vor dem Build die generierten Verzeichnisse **`app/.cxx` und `app/build` löschen**. Die ZIP-Zeitstempel liegen bewusst in der Vergangenheit; allein ein inkrementeller Build kann ältere Ausgaben behalten.
3. In Android Studio mit JDK17 synchronisieren; anschließend `clean assembleDebug lintDebug` bauen. Daten nicht deinstallieren oder neu importieren.
4. r300 und r301 im Bewegungs-/Türtest ansehen: nach oben und Richtung Meer drehen, Wasser mindestens10 Sekunden beobachten; r301→r300→r301 laufen. In r100 beide Teichflächen ansehen.
5. Mit dem Pfeil ins Testmenü wechseln, warten, „Fortsetzen“ wählen. Danach die App kurz in den Hintergrund schicken und zurückkehren. Animation und Controller sollen ohne Zeitsprung/Neustart weiterlaufen.
6. Bei Abweichungen Logcat der Tags **`RE4Environment`, `RE4Flow`, `RE4Scene`, `RE4Player`** einschließlich Raumstart schicken; Screenshot mit Raum und Kamerarichtung hilft beim Bildvergleich.

Erwartete Datenmeldungen: r100 „Himmel:1 … Wasser:1 … /2 Flächen“, r300 „Himmel:2 … Wasser:2 … /2 Flächen“, r301 „Himmel:2 … Wasser:1 … /1 Flächen“. Aktuelle Prüfungen und reproduzierbare Testaufrufe: [VALIDATION.md](VALIDATION.md).

## Grenzen

Dies erweitert den nativen Testlauf in diesen **drei** Räumen. Andere Himmel-/Flüssigkeitsprofile, Welt-Spritzer, Objekt-/Figur-Reflexionen, dynamische Flüssigkeitskollision und komplette Effekt-/Szenariosteuerung sind nicht allgemein portiert. Der GLES-Brechungspfad, Filterung und Sprite-Pool-Reihenfolge sind begrenzte Adapter; vollständige bitgenaue GX-TEV/EFB-Wiedergabe wird nicht behauptet. Das isolierte visuelle RNG startet reproduzierbar und ist nicht der globale Originalzustand eines laufenden Kampagnenspiels. M22 ersetzt keine noch fehlende Kampagne, KI, Audio, Inventar oder Speicherfunktion.
