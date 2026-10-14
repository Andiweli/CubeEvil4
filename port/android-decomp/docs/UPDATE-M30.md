# M30: Treppen, Türen, Möbel und r100-Raumblöcke

Version **0.30.0-dev**, versionCode **31**, Paket `com.ast.re4decomp`, ARM64/API21/GLES3. Vollständige geänderte/neue Dateien über dem vollständigen M29-Stand. Der Projektordner bleibt `RE4-Android-Decomp`. Die bereits importierten USA-Discs G4BE08, Revision0, bleiben verwendbar; keine Originalspieldaten sind im ZIP enthalten. Leon behält verbindlich **unendliche Gesundheit**.

## Gemeldete Fehler und Änderungen

| Befund | Ursache und Korrektur |
| --- | --- |
| Treppe zur Schrotflinte bleibt hängen | Der große, zweiseitige Körper-Cast traf die nach unten gerichtete Rückseite des Treppenunterbaus. Statische Raumkulisse benutzt jetzt den ursprünglichen einseitigen Kugelkontakt; dynamische Möbel/Türen behalten die volle Körperhöhe. Die Snap-Grenzen kommen aus den ursprünglichen Scenery-/Bodenregeln. Die echte Logposition erreicht die obere Etage und kann dieselbe Treppe wieder hinuntergehen. |
| Öffnende/aufgetretene Tür hält Leon fest | Die bewegliche Tür kann den Körper überlappen. Der nächste Kontakt trennt ihn anhand des originalen Kreis-/Rechteckkontakts an einer realen, erneut geprüften Bodenposition. Die Trennung umgeht keine Raumwände und erfindet keinen Boden. Leon übernimmt diese aktualisierte Probe-Position vor seinem Tick. Ein Tritt setzt außerdem den ursprünglichen 150-Takte-Offen-Timer. |
| Laser und Punkt verlieren Rot | Nach Einführung von LIT/Nebel übernahm der separate Laserpass deren Uniforms. Linie und Punkt schalten Raumbeleuchtung und Nebel nun ausdrücklich ab. Ihre Farben bleiben die originalen CORE-EST-Farben; Tiefenprüfung und Ziel-/Waffenmarker bleiben erhalten. |
| Schränke durchgehbar; heller Untergrund | ET04/ET05 erhalten ihre ursprünglichen Körpermaße. Die schwarze ET04-Schattenfläche und die soliden Griffe teilen einen Original-Displaylist-Eintrag. Die weiche Bodenfläche wird separat nach der Raumkulisse, mit Tiefenprüfung ohne Tiefenschreiben gezeichnet. Dadurch mischt sie sich auf den echten Holzboden statt auf die Nebel-Löschfarbe. Kein Austausch der Originaltexturen. |
| Gelbe Linie während des Falls | Die alte projizierte Kontaktfläche gehörte zur Diagnosefigur. Beim normalen Leon und bei Leiter-/Sprungaktionen wird sie nicht mehr gezeichnet. Der ausdrücklich angeforderte Diagnosekörper bleibt nutzbar. |
| FPS im HP-Bereich | Unten links steht `FPS` neben der ersten HP-/Munitionszeile. Gemessen werden tatsächliche Renderer-Callbacks über ein halbes Sekundenfenster, unabhängig vom festen 30-Hz-Spielablauf. Ein längerer Stillstand setzt das Fenster zurück. |
| r100 fehlt große Raumgeometrie | Die App las nur `r100.das`. Originales `BLK` verweist auf fünf zusätzliche, bereits unkomprimierte Archive `r100_00.dat` bis `r100_04.dat`. Sie werden jetzt geprüft vorbereitet und mit eigenen SMD-Instanzen, gemeinsamen BIN-/TPL-Ressourcen und dem globalen SMX an die Szene angehängt. |

Die Scenery-Korrektur ist ein gemeinsamer Bewegungsweg, keine Liste von handgebauten Treppen. Kamera-/Diagnoseabfragen behalten ihre vorherige Sidedness. Das Android-Kollisionssystem bleibt ein nativer SAT-/BVH-Adapter; damit wird nicht behauptet, sämtliche ursprünglichen cAtariInfo-Zustände bereits vollständig portiert zu haben.

## Größerer neuer Spielbaustein: Möbel schieben

Die drei originalen r101-Möbel in ETS-Slots **13, 15, 17** benutzen ET05/ET04, ihre Originalhöhen, Drehungen und `R101Init`-Schiebegrenzen. Rack17 erhält weiterhin die im Originalscript angegebene Startposition statt der ungeänderten ETS-Position. Körperkollision, Türblockade und Modelltransformation verwenden denselben laufenden Möbelzustand.

Vor eine erreichbare Möbelseite stellen und **LS nach vorn halten**. Leon greift, schiebt und lässt beim Loslassen wieder los. Die Originalkurven aus PL00 steuern Greifen (`PL_ARC20`), Schieben (`21`), Stoppen (`31`) und Loslassen (`22`); das Möbel folgt `PL_ARC59`. Loslassen während des Greifens verwendet den ursprünglichen direkten Idle-Übergang. Animationen laufen im bestehenden 30-Hz-Takt; Originalton `SndCall(6, 0x55)` begleitet die Schiebeschritte. Hände, Waffe, Root-Bewegung und Handlungsausschlüsse werden gemeinsam behandelt.

Das Möbel hält an Originalgrenzen, Raumwänden, anderem Mobiliar und fehlendem Boden. Es wird weder durch die Wand geschoben noch mit einer synthetischen Bodenkollision weitergetragen. Die vorhandene Grafik erhält eine Modellverschiebung; kein neuer Raum-/Texturupload pro Schiebetakt. Pause und ein neuer EGL-Kontext erhalten den laufenden Möbel-/Spielerzustand.

**Noch offen:** die vollständigen emRack-Schadens-, Umfall-, Bruch- und Gegner-Einklemmroutinen sowie spezielle Kameraphasen und die Kopplung an das Hausereignis. M30 liefert den gemeinsamen Greif-/Schiebe-/Stop-/Loslassweg, keine vollständige Übernahme aller emRack-Routinen. Möbelpositionen werden nicht als neuer dauerhafter Spielstand gespeichert.

## r100: erster Spielbereich

`r100` ist der anfängliche Wald-/Haus-/Brückenbereich; `r101` das anschließende Dorf. Der Renderfehler war ein fehlender Ressourcenweg, keine falsche Disc-Version.

Die fünf originalen BLK-Dateien ergeben **297 zusätzliche starre Platzierungen und 331118 Dreiecke**, mit aufgelösten Originaltexturen. Der Decoder wird nicht auf diese `.dat`-Archive angewendet: Sie sind bereits unkomprimiert. `blocks.tsv` hält unabhängig aus der Disc-FST ermittelte Größen und SHA-256-Werte. Der Java-Arbeiter prüft/kopiert alle benötigten Blöcke auch dann, wenn die Hauptdatei schon im Cache liegt. Erst das vollständige Bundle darf eine Szene öffnen. Dieselbe Vorbereitung gilt bei Raumauswahl, direkter r100-Prüfung und Raumwechsel.

Die Cache-Dateien liegen als `room-r100.dat` und `room-r100_00.dat` bis `room-r100_04.dat` nebeneinander; sie sind abgeleitete Arbeitskopien. Die unveränderten importierten Quellen bleiben im bisherigen `Android/data/com.ast.re4decomp/files/game/disc1/files/St1/`-Pfad. Ein normaler bestehender Vollimport enthält die fünf Dateien bereits. Falls tatsächlich eine Blockquelle fehlt/verändert wurde und keine geprüfte Cachekopie existiert, meldet die App das Ladeproblem; der vorherige Raum bleibt erhalten. Dann Disc1 erneut importieren, ohne die App-Daten pauschal zu löschen.

Der native Loader prüft das originale BLK-Version100-/Fünfblockprofil, Tabellenbereiche, Nachbarindizes, Ressourcen und gemeinsame Budgets. Gemeinsame SMD-Platzierungen bleiben Ressourcenbeschreibungen; sie werden nicht zusätzlich als doppelte Rauminstanzen gezeichnet. Die fünf Blöcke bleiben in diesem ersten Android-Profil **gemeinsam resident**. Das ist nicht die ursprüngliche bewegungsabhängige MRAM-/ARAM-Streamingstrategie. Die GPU-Kosten müssen auf dem Gerät mit dem neuen FPS-Zähler geprüft werden; weitere Sichtbarkeits-/Streamingoptimierung ist ein eigener Arbeitsschritt.

LIT-Cut0 aus r100 ergänzt dessen bestehenden Himmel und Wasser: 20 Originallichter, GX-Nebeltyp5, Bereich −1089 bis 106857, RGB141/135/117. Der direkte Raumstart verwendet die tatsächlich spielbare Position nach `r100_StartEvent` (**−99685, −484, −1343; Yaw2.246**) statt des Debugpunkts an der Brücke. Die sichtbaren Brückenvarianten folgen wörtlich `R100Init` und Raumflag10/`r100_Car_pos_move`: vorher Work37/38 sichtbar, Work23/44 verborgen; danach umgekehrt.

Das Intro, der erste Haus-Ganado, die Polizisten, der Autounfall und die weiteren r100-EVD-/Raumtasks werden hier noch nicht ausgeführt. Die r101-Innenrauminteraktionen werden dadurch ebenfalls nicht automatisch auf die r100-Türen und Gegenstände ausgeweitet. Der direkte Teststart stellt die statische Raumgeometrie und den ursprünglichen Anfangszustand bereit; er startet keine bereits fertige Kampagne.

## Originalcode und Dateibelege

Upstream bleibt unverändert auf `9b76ff003081d6ece8b838453fb8310743a972c7`.

- `src/game/at_sub.cpp`: `At_poly_sphere_ck2`, ursprüngliche Dreiecksseite beim Kugelkontakt.
- `src/game/emdoor.cpp`: `Open_timer=0x96`, Türbewegung und Kick-/Öffnungsregeln.
- `src/game/emrack.cpp`, `src/game/pl_push.cpp`, `src/game/player.cpp::pl_R1_ObjPush`: Körpermaße, Greifseiten, Zielverschiebung, Kurven, Grenzen und Übergänge.
- `src/st1/r101.cpp`: Rack13/15/17 und die Schiebegrenzen; kein vorzeitig gesetztes Hausflag.
- `src/game/EtcModel.cpp`, originale `et04`-BIN/EFF: unveränderte Palette, schwarze Schattenfläche und gemeinsame Griff-Displaylist.
- `src/game/block.cpp`, `include/block.h`: BLK-Version, Linkstruktur, Dateinamen, `BlockCreate(...GetDataExt(...,"SMD",0))`.
- `src/st1/r100.cpp`: Anfangsposition, Himmel/Wasser und Sichtbarkeitswechsel.
- `src/game/scroll.cpp`, `SmdSetParam`: lokale Instanzen und gemeinsame Ressourcen sind unterschiedliche Rollen.

Datei-, API-, Test- und Quellcodepfade beziehen sich auf den enthaltenen Projektstand; Originalspieldaten müssen selbst importiert werden.

## Installation und kurzer kombinierter Gerätetest

1. Das ZIP über den **vollständigen M29-Projektstand** kopieren und die enthaltenen Dateien ersetzen. Nicht als neues, unvollständiges Projekt öffnen.
2. In Android Studio `./gradlew clean assembleDebug lintDebug`; unter Windows `gradlew.bat clean assembleDebug lintDebug`. Anzeige **0.30.0** prüfen. Die bisherigen importierten Discs bleiben bestehen.
3. r101 öffnen: die Treppe zur Schrotflinte vollständig hinauf/hinunter; Türen von beiden Seiten öffnen/eintreten und danach weggehen/durchgehen.
4. Die Schränke/Regale von einer freien Seite mit LS vorwärts schieben; LS loslassen; Wand-/Schiebegrenzen prüfen. Unter dem Schrank muss echter Boden mit weichem dunklem Schatten sichtbar sein. Der Körper darf nicht durchgehen.
5. LT/L2: rote Laserlinie ab Waffenmarker und roter Punkt an einem unterstützten verletzbaren/zerstörbaren Ziel. Ein echter Sprung darf keine gelbe Fläche hinterherziehen. Unten links stehen FPS und HP∞.
6. r100 direkt aus dem Raumkatalog oder über die Raumprüfung öffnen: Wald, Weg und Haus müssen zusätzlich zu Himmel/Hintergrund vorhanden sein. Zum Menü und zurück wechseln; FPS in r100 und r101 vergleichen.

Die neuen und bestehenden Host-Prüfungen sind in [VALIDATION.md](VALIDATION.md) dokumentiert. **Kein neuer Gradle-/NDK-/Lint-/Adreno-Gerätelauf** konnte in dieser Umgebung ausgeführt werden. Echte Host-JNI-/GLES-Ausgabe und Java-Repositoriumstests ersetzen diesen abschließenden Gerätelauf nicht.

EM15-Dorfgegner, mehrere aktive Gegner, EVD/Haus/Glocke, Waffenwechsel, vollständiges Inventar, Musik/Save und der Kampagnenablauf bleiben die nächsten größeren Portierungsaufgaben; siehe [R101-GAMEPLAY-STATUS.md](R101-GAMEPLAY-STATUS.md).
