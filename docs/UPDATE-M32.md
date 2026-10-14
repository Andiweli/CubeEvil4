# M32 – Waffen, Koffer, Aktionen und Originalton

**VersionCode33 / 0.32.0-dev; com.ast.re4decomp; ARM64, Android 5.0/API21, GLES3.**
Über den vollständigen M31-Stand entpacken. Nur geänderte/neue vollständige Dateien unter `RE4-Android-Decomp/`, keine Originalgrafiken, Musik, ISO/GCM oder extrahierten Spieldaten. Keine Dateien müssen gelöscht werden. Unendliche HP bleiben aktiv; Trefferreaktionen bleiben erhalten.

## Einstieg und Bedienung

Der Einstieg bietet unmittelbar **„Dorf-Test · r101 ohne Gegner“**, **„Kampftest · r400 mit einem Ganado“** und den einklappbaren Browser **„Weitere Räume erkunden“**. Der Browser lädt die native 3D-Ansicht direkt. Import und Diagnosen bleiben im Menü. Start/Options oder der Pfeil öffnen das Pausenmenü; Y/Dreieck öffnet im Spiel den Koffer.

| Universaler Controller | Aktion |
| --- | --- |
| Linker Stick | Originalbewegung und Körperdrehung |
| Rechter Stick | Weich geregelter Blick/Zielen; oben schaut oben, unten schaut unten |
| LT/L2 | Waffe anlegen |
| LB/L1 | Messer halten |
| RT/R2 | Schießen bzw. Messerschlag |
| X/Quadrat | Nachladen |
| A/Kreuz | Angezeigte Kontextaktion bzw. Aufnahme bestätigen |
| B/Kreis | Zurück/Abbrechen im Menü |
| Y/Dreieck | Koffer öffnen |
| Start/Options | Pause/Testmenü |

Im Koffer: Steuerkreuz/LS wählen, A öffnet Aktionen; Schrotflinte wählen → „Ausrüsten“. Y nimmt ein Teil zum Verschieben auf, RB/R1 dreht, A setzt ab; LB/L1 beginnt Kombinieren. Select/View wechselt zur Schlüssel-/Schatz-/Dokumentliste. Der Koffer bleibt ein nativer Sitzungszustand; App-Neustart lädt noch keinen originalen Save.

## Gegenstände und Waffen

Die bestätigte Aufnahme bindet den derzeitigen nativen Transaktionsdialog an `ItemExam`: Spiel und Leon-Pose pausieren, Ja übernimmt die Koffertransaktion und setzt erst danach das Weltflag; Nein behält das Objekt. Die Originalquelle spielt hierbei keine allgemeine „Bücken“-FCV ab. Eine solche Ersatzanimation wird nicht erfunden. Originale rotierende Gegenstandsansicht und Nachrichtentexte sind noch nicht portiert. Geld und Schlüssel werden derzeit ebenfalls bestätigt; die Quelle verwendet dafür teils eine reine Mitteilung mit früherer Gutschrift. Bei vollem Koffer abbrechen, Platz schaffen und erneut aufnehmen; das originale Ausweichbrett fehlt noch.

Die Startausrüstung folgt `cItemMgr::set_game`: WEP02-Handgun (Item0x23), zehn geladene und zwanzig Reservepatronen, Erste-Hilfe-Spray und Startgegenstand0x7c. Die Dorfschrotflinte (Item0x2c, ITA.no138) beginnt mit sechs Patronen. WEP01/Punisher bleibt ein getrenntes bestehendes Diagnoseprofil.

Die Koffermaße, alle 58 Teileformen, 43 Waffen-/Kapazitätszeilen und 77 Rezeptzeilen sind gegen die eingefrorene Originalquelle geprüft. Magazine, Reserve, Stapel, Pesetas und ausgerüstete Waffe haben einen gemeinsamen Besitzer. Eine fehlgeschlagene Aufnahme packt keine Teilmenge. Originale Waffen-FCVs, Handbindung, Feuer-/Nachladeabläufe und Schrotflintenstreuung mit 13 Einzelstrahlen sind angebunden. Alle Ziele eines Schusses werden vor Objektmutationen bestimmt; Mehrfachpellets beschädigen dasselbe Objekt nicht mehrfach mit derselben Schussnummer. Behälter verwenden die originale Nah-/Fern-Bruchregel.

Der Laser wird nach Ziehen/Feuern anhand des aktiven Waffenablaufs freigegeben. CORE-Laser und Trefferpunkt behalten Originalrot. Das Mündungsfeuer verwendet originale Esp00-TPL/ANM und Blending, Rauch und den Esp11-Impuls aus **CORE-LIT-Cut8/Licht0**, ein bzw. zwei Originaltakte lang. Pause/Rendern erzeugen keine neuen Schüsse oder Effekttakte; Raumwechsel räumt alte Mündungseffekte auf, EGL-Neuaufbau behält sie.

## Fenster und Leitern

Die drei Fenster-FCVs537/538/536 und ihre Bruchzähler15/16/13 bleiben maßgeblich. Neu angebunden sind die **Original-Ereigniskameras** einschließlich Roll sowie die originalen Glassplittermodelle, Verzögerungen und Transformationsregeln. Der Bruch erfolgt am Quellenereignis, nicht erst nach Ende des Sprungs. Bei manchen Posen läuft der Kopf vor dem Oberkörper durch die Ebene; der zeitliche Vergleich verwendet den tatsächlichen Quellencounter, keinen geratenen früheren Bruch.

Drei Hausleitern in r101 und vier Objektleitern in r400 verwenden ET06-/ET08-Bewegung, originale Sprossenzahlen und solide Körper. Endpositionen werden gegen SAT-Boden geprüft. Die originale Objektleiter bietet oben kein normales Hinunterklettern: dort dienen Fenster-/Absprungaktionen oder das noch fehlende Umwerfen. Bereits vorhandene feste Turmleiterwege bleiben erhalten.

**Der direkte Dorf-Test setzt RSF8 ausdrücklich als Leiter-Aktionsfreigabe.** Das ist kein vollständig ausgeführtes Hausereignis: dessen weitere Fenster-/Möbel-/Gegneränderungen werden nicht behauptet. Normaler Raum-Browser bleibt davon getrennt. Leiterumwerfen/Wiederaufstellen und die zugehörigen speziellen Kameras sind Folgearbeit.

## Atmosphäre und Ton

LIT wird weiterhin aus jedem Raum gelesen. Behoben sind das 32-Bit-Big-Endian-Kamera-Richtungsflag, das irrtümliche Ausblenden nichtnull Kind-Gruppen, Leon-Elternlichter und originale Type4-/Type6-Abschwächung. Der neue Fußschattenpass nutzt die originale CORE-Esp0x13-Textur, sechs Originaljoints und die vorhandenen LIT-Id4/workKind5-Schattenlichter. 104 der 127 Anfangs-Cuts besitzen diesen Modus; sonst wird kein Ersatzschatten erfunden. Weitere Schatten-/TEV-/Filterpfade und dynamische Cuts fehlen weiterhin. Himmel/Wasser/EFF gelten nur für belegte Raumprofile; nicht alle Räume sind schon visuell1:1 fertig.

Der Audioadapter liest `bgm/bgmtbl.dat`, `bio4midi.hed/.dat`, `bio4str.hed`, `bio4bgm.sbb` und `bio4evt.sbb` aus dem vorhandenen Discimport. Er dekodiert Original-DSP-Samples/Streams und spielt originale MIDI-Sequenzereignisse sowie ESE-Umgebungsaufträge. r101 startet die in der Tabelle automatische MIDIbank0/Sequenz0; der Combat-Stream0x8002 wird ohne Kampagnen-Task nicht willkürlich gestartet. Der Adapter respektiert die Auto-Startbits pro Raum. Musik läuft im Koffer weiter; pausierbare bestehende Effekttöne halten an, neue Menüklänge dürfen laufen. Hintergrundpause stoppt alle Audiozähler. AX-Hardwarefilter/Reverb/Chorus sind noch nicht1:1 übernommen.

## Installation und gezielter Gerätetest

1. Vollständigen M31-Ordner sichern, das M32-ZIP darüber entpacken. Bestehender vollständiger Discimport bleibt verwendbar. Fehlende WEP02/WEP07/BGM-Dateien gegebenenfalls über den eigenen Discimport ergänzen.
2. Android Studio: `gradlew.bat clean assembleDebug lintDebug` bzw. `./gradlew clean assembleDebug lintDebug`. Versionsanzeige **0.32.0** prüfen. Kein APK oder neuer Gradle-/NDK-/Lint-/Adreno-Erfolg aus dieser Umgebung wird behauptet.
3. Dorf-Test direkt öffnen: Fenster vorwärts/rückwärts/abwärts, die Hausleitern und Schrotflintenaufnahme prüfen. Aufnahme einmal abbrechen, dann bestätigen; Y/Koffer → Schrotflinte → Ausrüsten. Schießen, nachladen, Messer halten und Fenster-/Leiteraktionen wiederholen.
4. Während Zielbewegung, Fenstersprung und offener Aufnahme kurz in den Hintergrund gehen. Keine zweite Aufnahme, keine neue Munition und keine fortlaufende Aktion während Pause. Über Start/Pfeil zum Menü und direkt zur Raumauswahl zurückkehren; r400-Kampftest ist separat erreichbar.
5. Logcat ab Appstart mit Tags `RE4Player`, `RE4Weapon`, `RE4Flow`, `RE4Ladder`, `RE4Audio`, `RE4WorldEffects`, `RE4Destruction` und `AndroidRuntime` erfassen. Zum eingesandten Log ist kein App-Absturz erkennbar; bekannte Hinweise auf fehlende Glow-/EAR-/SST-/Staubpfade sind dokumentierte Portgrenzen.

Prüfergebnisse: [VALIDATION.md](VALIDATION.md). Einzelheiten: [INVENTORY-PORT.md](INVENTORY-PORT.md), [M32-ACTIONS-SOURCE.md](M32-ACTIONS-SOURCE.md), [LIGHTING-M32.md](LIGHTING-M32.md) und [AUDIO-M32.md](AUDIO-M32.md).

M32 ist ein umfangreicher gemeinsamer Funktionsschritt, **noch kein vollständig originales Dorflevel ohne Gegner**. Originale Sscrn-Menügrafiken/Modelle, weitere Standardaktionen/Effekte, Kampagnen-Tasks und dauerhaftes Speichern müssen fertig werden. Die EM15-Mehrgegnerlaufzeit und der Original-Haus-/Glockenablauf folgen als eigener nächster Schwerpunkt; Intro/Titelmenü bleiben ebenfalls offen.
