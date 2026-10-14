# M29 — Dorfatmosphäre, Sprünge, ruhige Körperdrehung und Gegenstände

Version **0.29.0-dev**, versionCode **30**, vollständige geänderte/neue Dateien über dem vollständigen M28-Stand. Paket `com.ast.re4decomp`, Ordner `RE4-Android-Decomp`, ARM64, API21 und GLES3. Kein erneuter Import der Discs erforderlich. Keine Originalspieldaten im ZIP.

## Unendliche Gesundheit

Die Android-Spielerstarts und beide Test-Neustarts schalten die neue Testregel verbindlich ein. Leon behält volle 1200 HP; HUD und Vorrätemenü zeigen `HP ∞`. Raumwechsel und Grafikverlust behalten den Modus bei. Schaden ruft weiterhin die Originalberechnung mit ihrem Zufallsverbrauch auf, übernimmt aber keinen HP-Verlust. Explosions-, Griff-, Befreiungs- und Aufstehreaktionen bleiben bestehen; diese Regel entfernt keine Gegnerangriffe. Volle Gesundheit verbraucht kein Kraut. Die reine CPU-Klasse lässt den Modus abschaltbar, damit die bisherigen Original-Schadensregressionen weiter geprüft werden können; die App stellt dafür keine Abschaltfunktion bereit.

## Ursprünglicher Nebelschleier, Licht und Himmel

Die erste LIT-Datei der importierten USA-Disc enthält 20 Kameraschnitte, in Cut0 15 Licht-Records. Der bisherige Renderer setzte diese Raumbeleuchtung und den Nebel noch nicht um. M29 liest den BE-Offsetkatalog, die 260-Byte-Umgebung und 300-Byte-Lichtrecords begrenzt und ohne GameCube-Zeigercasts.

Für r101/Cut0 stammen diese Werte direkt aus der Disc:

| Wert | Originalwert |
| --- | --- |
| Nebelmodus | 2, linear |
| Start | −7074,842285 |
| Ende | 175030,1875 |
| Nebelfarbe RGB | 113 / 108 / 90 |
| Screen-Ambient RGB | 27 / 27 / 27 |
| Actor-Ambient RGB | 35 / 35 / 35 |
| Effect-Ambient RGB | 15 / 15 / 15 |

Der negative Start bleibt erhalten: auch nahe Flächen erhalten den leichten Schleier. Nebel benutzt die vorwärts gerichtete Kameratiefe, nicht den radialen Abstand, und verändert die Materialalpha nicht. Shader berücksichtigen die unterstützten Original-Lichttypen, Normalen, Klassen-/Auswahlmasken, alphaabhängigen Lichtfarben und Distanz-/Spotparameter. Vertex-Ambient und Register-Ambient bleiben getrennt. ITM-Modelle verwenden ihre ursprüngliche Lichtklasse `0x20`. Aufeinanderfolgende Materialien desselben beleuchteten Objekts teilen die Lichtbindung; bei jedem neuen Scene-/Actor-Draw werden kamerabezogene Werte erneuert, auch nach EGL-Verlust.

Das tatsächliche r101-Himmelsobjekt ist **SMD-Work0, BIN0, TPL1**. Seine originale Wolkentextur, Geometrie, Platzierung und unterstützte SMX-Bewegung werden verwendet. Es wurde zuvor nicht als Himmel klassifiziert. Feuer-/Rauchquads behalten die ursprüngliche eigene Effektbehandlung; ein pauschaler Nebel auf alle Spezialeffekte würde vom Original abweichen.

**Grenzen:** Cut0 ist das ursprüngliche Standardprofil beim Raumstart. Die vollständige Umschaltung über Kameraschnitte/Ereignisse, dynamische Id1-/Id7-Lichtsteuerung, parented/custom Lichttypen, die originale Schatten-/Specular-/DOF-/Filterkette und genaue Box-/Zylinder-Lichtvolumina fehlen. Die begrenzte Lichtauswahl verwendet Modellradien. Deshalb ist dies keine Zusage pixelidentischer Originalbeleuchtung. Andere Räume erhalten nicht pauschal die Dorf-LIT.

Quellen im eingefrorenen Upstream: `src/game/light.cpp`, `src/game/model.cpp`, `src/game/obj19.cpp`, die GX-Fog-/Light-SDK-Routinen und die tatsächlichen LIT/SMD/SMX-Daten.

## Hinunterspringen

Mit abgelegter Waffe an einer geeigneten Dach-/Turmkante ausrichten und bei der Anzeige **A/Kreuz: Hinunterspringen** drücken. Die bestehende Leiteraktion hat an ihrem Einstieg weiterhin Vorrang. Bloßes Vorwärtslaufen schaltet die Randprüfung nicht aus.

Der Ablauf übernimmt `cPlayer::actWallCheck`, `player.cpp::pl_R1_Fall` und das zehn Takte lange `cMotBase`-Alignment. Geprüft werden der erste Blickstrahl, ein zweiter normalgerichteter Strahl und beide ±350-Schulterstrahlen. Nur die ursprünglichen SAT-Attribute `0x100010` ermöglichen diese Aktion. Fall und Landung verwenden PL_ARC0x2d–0x30, die anfängliche MotBase-Angleichung, Seitenkorrekturen, Root-Bewegung und die echte Pelvis-/Bodenprüfung. Die Landung muss vor dem Angebot auf echtem unteren SAT-Boden mit gültiger Kapsel nachweisbar sein. Während der Aktion übernehmen die Originalkurven die Platzierung.

Die automatisierte Prüfung durchläuft 58 gültige Anläufe an 19 SAT-Kanten in r101 und 61 an 20 Kanten in r400. Alle 119 Anläufe landen gültig. Das zählt mehrere Annäherungen je Kante, **nicht 119 unterschiedliche Sprungstellen**. Pause und Grafik-Neuerstellung frieren Pose, Verlauf und Kamera ein. Bei einem unerwartet ungültigen Abschluss kehrt der Adapter zum geprüften Ausgangsstand zurück. Es wird kein Ersatzboden erzeugt.

Fenster-Durchsprünge, kleine Step-Down-Aktionen, Ashley und geskriptete Sprünge benötigen weitere Originalroutinen. Dieser Adapter bietet nur geprüfte Höhenunterschiede zwischen 600 und 50000 Originaleinheiten an.

## Linker Stick und Darstellung

Der Körpertransform sprang bisher in den ursprünglichen 30-Hz-Schritten, auch wenn der Bildschirm öfter zeichnete. M29 interpoliert Position und Körperrichtung im Renderpfad zwischen den letzten Originalticks, über den kürzesten Winkelweg auch beim Wechsel über ±PI. Figur, angehängte Modelle und Verfolgerkamera benutzen denselben Darstellungsstand. SAT, Drehsektoren, Drehgeschwindigkeit, FCV/SEQ und Kampfticks behalten ihre bisherigen Originalwerte. Waffen-/Leiter-/Sprungaktionen verwenden ihre eigene Aktionspose. Die Knochenkurven selbst werden weiterhin mit originalen 30 Hz ausgewertet.

Bei 120 Renderbildern pro Sekunde behalten Position, Richtung und Knochenwerte exakt dieselben geprüften Werte wie die Vergleichsinstanz ohne Interpolation, während die Körperrichtung öfter sichtbar fortschreitet. Nach Platzierungswechsel/Pause wird keine veraltete Transformhistorie über eine neue Aktion gezogen. Ein Adreno-Frametime-/Leistungsprofil ist damit noch nicht gemessen.

## Frei liegende r101-Gegenstände

Die ITA-Datei enthält 41 Records: **23 ohne Wartebesitzer, 13 mit Behälterbesitzer, fünf mit Gegnerbesitzer**. Die bisherige Behälterlaufzeit bleibt bestehen. Die neue Laufzeit aktiviert die 23 passenden Waiting-Type0-Records; Gegnerbeute wird nicht ohne den erforderlichen Gegner-/Todeszustand freigeschaltet.

Die 23 passenden ITM-Platzierungen liefern 6410 echte Modelldreiecke. Modelle bleiben an ihren Originalpositionen. Auch Records am gemeinsamen außerhalb des üblichen Dorfwegs liegenden Datenplatz bleiben dort; sie werden nicht als zusätzliche Dorfbeute umpositioniert. Aufnahme prüft Originalbereich, Frontpunkt, Blickrichtung, OT-Priorität, Diskreihenfolge und die drei ursprünglichen EAT-Sichtlinien. Die Schrotflinte behält ihren besonders gesetzten Aufnahmebereich. Gegenstände werden mit den MSB-first globalen oder Raum-Itemflags markiert; Tür-/Fenster-/ETC-Writeback kann diese Wörter nicht überschreiben.

Pistolenmunition geht in die Reserve; übrige Gegenstände bleiben im Sitzungsinventar. Geld benutzt `cItemMgr::get`-Umrechnung und die originale Obergrenze von 99.999.999 Pesetas, auch für Behälterbeute. 100 Einheiten des ITA-Gelditems 0x78 ergeben beispielsweise 1000 Pesetas. Die UI-Munitionsnamen sind anhand `item.cpp::wep_info` korrigiert: **0x18 ist Schrotmunition, 0x00 Gewehrmunition**.

**Noch offen:** die Schrotflinte ausrüsten/schießen/nachladen, weitere Waffenklassen, Granaten, originaler Inventarkoffer, Dokumentansicht, vollständige Itemnamens-/Nachrichtentabellen, Pickup-Dialog/Glüheffekte und dauerhafte Spielstände. Bei ungeklärter Originalbezeichnung zeigt die UI eine eindeutige Gegenstands-ID. „Aufgenommen“ wird nicht mit „bereits als Waffe spielbar“ gleichgesetzt.

## Installation und ein kombinierter Gerätetest

1. ZIP über den vollständigen **M28**-Projektordner entpacken und enthaltene Dateien ersetzen.
2. Android Studio: `bash ./gradlew clean assembleDebug lintDebug`; Windows: `gradlew.bat clean assembleDebug lintDebug`. Appanzeige **0.29.0** prüfen.
3. „Kampagnendorf r101 testen“ öffnen. Dorfhimmel und Fernsicht/Nebelschleier ansehen. LS im Stand und beim Laufen drehen, dann kurz LT/L2 und LB/L1 prüfen.
4. Turmleiter benutzen. Oben am Einstieg bleibt „Leiter hinunter“ verfügbar; an einer anderen geeigneten Kante bei entsprechendem Hinweis A/Kreuz drücken. Während eines Sprungs einmal über den Pfeil pausieren und fortsetzen.
5. Eine originale frei liegende Munitionspackung aufnehmen und Reserve/Vorräte ansehen. Geld aufnehmen; Schrotflinte am Originalplatz aufnehmen. Für deren Nutzung gibt es noch keinen Waffenwechsel.
6. Im Ganado-Test greifen lassen, abschütteln und ein rotes Fass auslösen: HP bleibt ∞, Treffer-/Befreiungsbewegungen bleiben sichtbar.

Bei Auffälligkeiten vollständiges Logcat mit `RE4Lighting`, `RE4Environment`, `RE4Jump`, `RE4Ladder`, `RE4Items` und `RE4Player` senden. Keine manuelle Prüfung jedes einzelnen Raums erforderlich.

## Validierung und nächster Spielschritt

Die aktuellen Prüfergebnisse und reproduzierbaren Befehle stehen in [VALIDATION.md](VALIDATION.md). Es gibt hier **keinen neuen Android-ARM64-/Gradle-/Lint-/Adreno-Gerätelauf**. Host-GCC/Mesa kompiliert und prüft die tatsächlichen neuen Produktions-JNI-/GLES-Pfade, ersetzt aber nicht den kurzen Gerätetest.

r101 ist weiterhin kein vollwertiger originaler Spiellevel. Der nächste gemeinsame Baustein ist die originale EM15-Laufzeit mit mehreren Instanzen und Waffenvarianten, anschließend die native Anbindung der r101-Tasks/EVD für Wahrnehmung, Haus, Nachschub und Glockenende. Der einzelne EM12-Kampftest bleibt ein getrennter Prüflauf. Die konkreten Daten-/Funktionsgrenzen stehen in [R101-GAMEPLAY-STATUS.md](R101-GAMEPLAY-STATUS.md).
