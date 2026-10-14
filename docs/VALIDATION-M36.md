# M36: abschließende Prüfung

Stand: 2026-10-05, `0.36.0-dev`, VersionCode 37, `com.ast.re4decomp`, ARM64/API21+/GLES3. Die Originalreferenz bleibt Commit `9b76ff003081d6ece8b838453fb8310743a972c7` unter `upstream/re4`. Die folgenden Prüfungen verwenden die tatsächlich bereitgestellten Disc-Daten, keine nachgebauten Testmodelle.

## Build und gemeinsame Verträge

Der Android-Stand besteht `clean assembleDebug lintDebug`; nach der abschließenden kleinen Fog.Type0-Hintergrundkorrektur besteht erneut `assembleDebug lintDebug`. Verwendet wurden Gradle 8.11.1, AGP 8.9.2, Java 17, SDK/BuildTools 35, NDK 28.2.13676358/Clang 19 und CMake 3.22.1. AAPT bestätigt Version 37/0.36.0-dev und Mindest-API21.

| Prüfung | Ergebnis |
| --- | --- |
| Android-Lint | 0 Fehler, 0 Fatal, 27 Hinweise |
| Java-NativeBridge gegen exportierte JNI-Namen | Alle 51 Methoden vorhanden, keine zusätzliche oder fehlende Methode |
| APK-Bibliotheken | Nur `arm64-v8a`, native Bibliothek unkomprimiert |
| ELF-/APK-Ausrichtung | Alle drei LOAD-Segmente `0x4000`; `zipalign -c -P 16 4` bestanden |
| Kanonisches `tools/test_host.sh` nach letzter Produktänderung | Vollständig bestanden |
| Native Debug-Vertragsgruppen | 38/38 bestanden |
| Frisch gebautes optimiertes natives Profil | 38/38 bestanden |
| Inventar gegen Originaltabellen | 58 Teile, 43 Waffenzeilen, 77 Rezepte bestanden |
| Generierter Leon-Quellinventarvergleich `--check` | Bestanden: 91 Dateien, 1.820 Definitionen, 54 Dispatchtabellen |
| Tatsächliche kombinierte Titel-/Raum-JNI-/GLES-Prüfung | 598 Prüfungen bestanden |

Die 27 Lint-Hinweise bleiben sichtbar. Sie betreffen unter anderem API-abhängige Manifestattribute, feste Ausrichtung, ARM64 ohne ChromeOS-x86, Ressourcen, Allokationen beim Zeichnen, Sicherungsregeln und Touch-Zugänglichkeit. Die neue MovieView wird programmgesteuert konstruiert; der Lint-Hinweis auf einen XML-Konstruktor ist keine fehlende Laufzeitkonstruktion. Ein fehlerfreier Build ersetzt keinen Android-Gerätelauf.

Der kombinierte Produktionstest lädt Titelgrafik und Ton über JNI sowie r100 mit realem CORE/EM12. Er prüft beide EVDs, getrennte Sprache/BGM, eingefrorenen Spieler während Ereignissen, stummes Pausieren, unveränderte Ereigniszeit und identische Pixel nach EGL-Verlust, den ersten tatsächlichen Kampf, den ursprünglichen ACTIVE-Todesabschluss, s20-Cut2-Aktivierung, Skip-Endflags und anschließend die sieben Ganados. Die Gruppe aktualisiert Leon einmal pro Welttakt; ein anschließender Explore-Raum erhält keinen Kampagnenzustand.

## Originaldaten und sichtbare Ausgabe

| Bereich | Ergebnis und Aussage |
| --- | --- |
| Titelsteuerung/Optionen/Präsentation | 18 + 17 + 86 Java-Prüfungen bestanden; Eintrittsblende, Cursorwechsel, Bildschirmabbildung und Decoder-Lifecycle |
| Native Titelkomposition | 27 Prüfungen bestanden; originale Verschiebungskanäle und negative Bildblitze |
| Filmdateien | `opening`, `demo0`, `demo1` tatsächlich dekodiert; 12 Rand-/Formatprüfungen bestanden. Normale Inhalte reichen bis an alle vier 512×336-Kanten, keine fest eingebrannten schwarzen Streifen |
| Koffer und übrige Menüseiten | 7.432 Layout-, 80 Navigations- und 1.070 Originalgrafik-/Pixelprüfungen; fünf Reiter, vier Bildschirmformate einschließlich 21:9 und Hochformat |
| EVD-Kamera, Modelle, Motion, Morph, LIT, Pakete und EST | 24.122.025 CPU-Prüfungen über sämtliche 2.035 Ereignisframes und 26 Cuts bestanden |
| Tatsächliches EVD-GLES-Rendering | 487.929 Prüfungen bestanden; alle 26 Cuts, Morph-Uploads, Effekte, Originalschrift, GL-Zustand und Kontextverlust |
| Ereigniston | 14 Prüfungen mit originalen DSP-Daten; bestehende Audioverträge mit 353 Prüfungen bestanden |
| Sieben r100-Ganados | 5.879 CPU- und 252 GLES-Prüfungen bestanden; ursprüngliche ESL-Datensätze, vier Körperfamilien, drei Werkzeuge, Freigaben und getrennte GPU-Eigentümer |
| WEP01/02/07-Treffer | 173.376 exakte Vergleiche über 43.344 Fälle gegen den Originalaufruf; Schaden, kritische Treffer und RNG |
| Ganado-Motion gegen OriginalMotionMove | 488.872 Floatvergleiche über neun Profile und 1.153 Posen ohne Abweichung; maximale starre Werkzeugabweichung 0,000245 Originaleinheiten |
| Leon an der gemeldeten r101-Wand | 3.930.779 Prüfungen, 1.080 Originalposen und 27 GLES-Aufnahmen bestanden; maximale GPU-Skinabweichung 0,001194 Originaleinheiten |
| Lazy-Motion-Metadaten | 2.594 Verlaufprüfungen, 922 Abfragen und 64 Originalkameraframes; zukünftige Posen, Sequenzen, Sounds und Kamera entsprechen dem vorher vorbereiteten Kontrollspieler |
| r10a-Wasser | 221 Prüfungen in 640×360 und 222 in 1920×1080 bestanden, einschließlich Fog.Type0-Hintergrund, Original-EFB-Kopie und GL-Zustandswiederherstellung |
| Helligkeit | 69.382 GLES-Prüfungen und 567 originale Kopierkoeffizienten bestanden |
| Gesamter Raum-/Wasserkatalog | 1.216.930 Prüfungen über 127 Räume, 20 Wassergrids, keine Environment-Warnung; vier bestehende nicht unterstützte SMD/BIN-Räume bleiben ausdrücklich ausgewiesen |
| Originale Wasserbewegung | 3.687.300 exakte Floatvergleiche ohne Abweichung |
| Originale CommonMove/Color/Anm-Effekte | 399.768 Vergleiche ohne Abweichung |

Die ursprüngliche Fußarbeit, M34-/M35-Aktionsabläufe, Kollisionen, Nahkampf, Greifen und Trefferfeedback bestehen weiterhin ihre jeweiligen Verträge. Der Coverage-Bericht behält die tatsächlichen Grenzen bei: 1.692 Definitionen sind noch nicht übertragen, 128 sind teilweise angebunden. Erfolgreiche Einzelprüfungen erklären diese Definitionen nicht pauschal für fertig.

## Ladezeit und bereitgestelltes Log

Im bereitgestellten Log benötigt der erste AEV-Gegnerwechsel rund 2,17 Sekunden für einen erneuten vollständigen Welt-/Geometrieupload; dessen Tod löst einen weiteren solchen Aufbau aus. M36 bereitet Gegner und Ereignisressourcen beim Raumladen vor und erzeugt beim Ende von s03 nur die benötigten Gegner-GPU-Ressourcen. Ein echter Szenenwechsel mit veränderter Raumgeometrie bleibt ein eigener Upload. Neue `RE4Load`-Zeilen trennen Raum/Ereignisse, Leon/Waffen und Geometrie/GPU.

Die gleichartige Vorbereitung von Leon und WEP01/02/07/EM12 wurde auf dem Host jeweils sechsmal gemessen:

| Profil | Median | Originaldaten-Fingerprint |
| --- | --- | --- |
| Vorheriger Debugstand, eager Motion-Metadaten | 294,776 ms | `8956647433431119642` |
| M36 Debug ohne Optimierung, lazy Metadaten | 179,883 ms | identisch |
| M36 mit `-O2 -fno-fast-math -ffp-contract=off` | 40,683 ms | identisch |

Die Metadatenänderung reduziert diese gleichartige Hostvorbereitung um etwa 39 Prozent. Die zusätzliche native Optimierung lässt Symbole und Assertions erhalten; strikte Gleitkomma- und Originalmotion-Regeln bleiben bestehen. Diese Messung umfasst keine komplette Android-Raumladung und ist keine Zusage für ein Telefon oder OUYA.

Der aktuelle Log meldet beim Film bereits Decoder-Bildbeginn, obwohl die Anzeige schwarz war. Das passt zu einer überdeckten Decoderoberfläche; M36 ersetzt diesen Anzeigeweg durch TextureView und protokolliert zusätzlich empfangene Texturframes. Alle drei vorhandenen MP4s lassen sich vollständig dekodieren. Die Hardwaredecodierung und sichtbare Ausgabe auf dem Zielgerät sind damit noch nicht nachgewiesen.

## Verbleibende praktische Grenzen

Es gab keinen neuen Lauf auf einem Android-/OUYA-Gerät. Insbesondere Filme, Rückkehr aus dem Hintergrund, tatsächliche Bildschirmaufteilung, Laufzeit und Gerätesound benötigen dort die abschließende Prüfung. Die Host-GLES-Bilder verwenden den Produktionsrenderer und Originaldaten; sie sind keine Android-Screenshots.

Grundlegende LIT-Cut0-Beleuchtung und Nebel gelten raumübergreifend. Fog.Type0 verwendet nun wie `bio4_AddBgColor` schwarzen Hintergrund trotz gespeicherter Farbdaten; dies betrifft 45 der 127 Cut0-Datensätze. Beliebige Originalkamera-/Licht-Cut-Wechsel, vollständige GX/TEV-Effekte, Schatten und einige Raummodelle bleiben offen. Die beiden EVDs binden ihre jeweiligen tatsächlichen Licht-Cuts und Lichtklassen ein; die allgemeine Lichtvolumenprüfung benutzt weiterhin einen Bounding-Radius.

Weitere Grenzen stehen in `UPDATE-M36.md` und den verlinkten Fachnotizen: vollständige cEm10-KI und Schadensreaktionen, alle Leon-Zustände, sämtliche r100-Nebenereignisse, vollständige Menüanimationen und Spielstandpersistenz. Die vorhandenen NTSC-Daten enthalten keine nutzbare deutsche Ereignisschrift; Englisch folgt dem Original, Japanisch benutzt die Originalglyphen. Der skalare Tiefenparameter der Titelverzerrung ist weiterhin nur begrenzt aufgelöst.
