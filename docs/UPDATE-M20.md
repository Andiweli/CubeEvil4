# M20 — Ganado-Nahkampf und rechter Stick

Voraussetzung: vollständig eingespielter M19-Stand. Dieses ZIP enthält ausschließlich vollständige neue/geänderte Dateien unter `RE4-Android-Decomp/`; keine Diffs und keine Spieldaten. Paketname bleibt `com.ast.re4decomp`, ARM64/API21/GLES3.

## Installation

ZIP über das vorhandene Projekt entpacken und Dateien ersetzen. Im Projektordner unter Windows:

```powershell
.\gradlew.bat clean assembleDebug lintDebug
```

### M20-BuildFix: ZIP-Zeitstempel

Das zuerst gelieferte M20-ZIP verwendete eine andere lokale Zeitzone. Alle Einträge trugen 2026-10-04 11:18:20, obwohl die Auslieferung um 08:18:20 UTC / 10:18:20 Wiener Zeit erfolgte. Unter Windows konnte dadurch die entpackte `CMakeLists.txt` in der Zukunft liegen. Das Nutzerlog von 10:20 Uhr zeigt die dazu passende Ninja-Regenerationsschleife: `manifest 'build.ninja' still dirty after 100 tries`. Es enthält keinen C++-Compilerfehler.

`RE4-Android-Decomp-M20-BuildFix.zip` ersetzt das ursprüngliche M20-ZIP. Es enthält denselben M20-Spielcode und dieselben 34 vollständigen Dateien; die Patchhinweise sind ergänzt. Die ZIP-Zeitstempel liegen ausdrücklich in der Vergangenheit. Nach Ersetzen aller Dateien im Projektordner den unten gezeigten vollständigen Cache-Neuaufbau durchführen. Das ist wegen der zurückgesetzten Zeitstempel erforderlich. Es werden nur lokale Buildausgaben entfernt; Disc-Importe auf dem Gerät bleiben erhalten.

Bei wiederverwendeten alten nativen Objekten nach ZIP-Updates zusätzlich vor dem Build:

```powershell
Remove-Item -Recurse -Force app\.cxx, app\build -ErrorAction SilentlyContinue
.\gradlew.bat clean assembleDebug lintDebug
```

M20 verwendet dieselben schon importierten PL00/WEP01/EM12-Dateien und dieselbe ESL-Liste. Ein vollständiger bestehender Import benötigt keine Wiederholung. Version: 0.20.0-dev, Laufzeitstatus M20.

## Gerätetest

1. „Ganado-Nahkampf: r100 / Neustart“ wählen. Leon beginnt mit1200HP, der Ganado mit1000HP. Testmunition wird aus dem bisherigen Lauf übernommen.
2. Zunächst ohne Gegenwehr stehen bleiben. Der Ganado nähert sich, greift frontal zu, Leon verliert Lebenspunkte, fällt und steht anschließend auf. Am Ende eines vollständigen ersten fehlgeschlagenen Griffs sollten810HP bleiben.
3. Den Test neu starten. Während „Gegriffen“ den linken Stick rasch links/rechts bewegen oder A/Kreuz und B/Kreis wiederholt drücken. Mindestens10 Widerstandspunkte wehren den Griff ab. Halten einer Taste zählt nicht wiederholt. Der Ganado wird abgeworfen; ein überlebender Gegner wartet am Boden und steht wieder auf.
4. Während eines Griffs Start drücken, kurz warten und weiterlaufen lassen. HP/Animation müssen pausieren. Auch App-Hintergrund und Modellprüfung erzeugen keine zusätzlichen Grifftakte.
5. Mit LT/L2 und rechtem Stick zielen, RT/R2 schießen, X/Quadrat nachladen. RS horizontal ist jetzt in beiden Ansichten gegenüberM19 umgedreht; vertikal und LS bleiben erhalten.
6. Nach Leons Tod den Ganado-Test neu starten. Das ist ein Test-Neustart; Kampagnen-Checkpoints/Game-over-Menü sind noch nicht portiert.

Hilfreiche Logcat-Tags: `RE4Player`, `RE4Enemy`, `RE4Weapon`, `RE4Flow`, `RE4Walk`, `RE4Actor`, `RE4Scene`. `RE4Player` meldet HP und Aktionswechsel; `RE4Enemy` Treffer, Griffe und erfolgreiche Gegenwehr.

## Umfang und Prüfung

Der Patch überträgt einen zusammenhängenden frontalen Griffzweig aus dem eingefrorenen Originalcode. Direkte Gegnerannäherung, statische SAT-Auflösung, Testkameras und explizite Aktivierung des ESL-Eintrags bleiben begrenzte Adapter. Weitere Waffenangriffe, Seit-/Rückgriffe, DashCatch, Ashley, Parasiten/Kopfverlust, Beute, Originalaudio und vollständige Szenario-KI fehlen.

Originalregeln für HP/RNG/Ausrichtung wurden unabhängig gegen extrahierte Originalfunktionen geprüft. Host-Kompilation und native Regressionen werden in `VALIDATION.md` getrennt von echten Asset- und Android-Prüfungen geführt. In diesem Durchlauf waren die gespeicherten Original-Discs nicht abrufbar und die Android-SDK-/NDK-Buildumgebung nicht verfügbar. Deshalb wird weder ein neuer Android-APK-Build noch eine erneute Original-Asset-/Geräteprüfung behauptet. Synthetische Fixtures prüfen Zustandsabläufe und Rendering, nicht die Richtigkeit der neuen Originalposen.
