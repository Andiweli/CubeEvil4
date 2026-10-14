# RE4 Android Decomp — Milestone 37

**K17 Fix6 / 0.37.17-fix6:** Synchronisierte Rifle-Repetieranimation einschließlich originalem Absenken, interpolierte und auf 640×480 begrenzte Inventardarstellung und gegenseitiger Körperkontakt beim Ganado-Heugabelangriff. Enthält Fix1–Fix5; VersionCode 55. Vollständige Austauschdateien: [K17-FIX6.md](docs/K17-FIX6.md).

**K17 Fix5 / 0.37.17-fix5:** Originaler Rifle-Nachladeablauf einschließlich Absenken, dezente originale Zielfernrohrbewegung, laufendes Leon-Modell im pausierten Inventar, reparierte Blutereignisse samt Rifle-/Maschinenwaffenfolgen und raumspezifischer Ganado-Schadensalarm in r101. Enthält Fix1–Fix4; VersionCode 54. Vollständige geänderte Quelldateien: [K17-FIX5.md](docs/K17-FIX5.md).

**K17 Fix3 / 0.37.17-fix3:** Minenwerfer-Zielachse und gespeicherte Abschussrichtung, kombinierbare Waffenaufsätze mit SAVE/LOAD, originale Ganado-Körperkollision und erreichbarer Walk→Dash-Wechsel, aktive Room-ID vor FPS sowie Original-Glockentitel ab Frame 1100. Rifle und Semi-Auto behalten gemäß Original ihre Basis-Visieransicht. Enthält Fix1 und Fix2; VersionCode 52, gleiche AST-Signatur. Originalbelege, Installation, Prüfungen und Grenzen: [K17-FIX3.md](docs/K17-FIX3.md).

**K17 Fix2 / 0.37.17-fix2:** Originale Waffenanhänge und Aktionsübergänge, Minen-/Raketenflug, Granatengriffe und die bildschirmfüllenden Zielfernrohre sind korrigiert. LOAD/SAVE folgen ohne zusätzliche Handler-Verzögerung dem originalen 30-Hz-Takt. Gewöhnliche Dorfgegner verfolgen Leon nach dem Alarm; beide Nachschubgruppen lösen ihre Anfangskollisionen. Die Glockentür behält ihre Weltposition und Hausleitern ihre passenden Sichtbarkeitsgrenzen. Gewöhnliche Gegnerbeute baut die ganze Szene nicht mehr neu auf. Enthält sämtliche Fix1-Korrekturen. Ursachen, Originalbelege, Prüfungen, verbleibende Grenzen und Installation: [K17-FIX2.md](docs/K17-FIX2.md). App-ID, Signatur und vorhandene Disc-Imports bleiben gültig; VersionCode steigt auf 51.

**K17 Fix1 / 0.37.17-dev:** Die Drehanimationen der geladenen Waffenprofile und der verletzten Figur folgen wieder der physischen Links-/Rechtsrichtung. Der Funkkontakt stellt auch die waffeneigenen Handmodelle wieder her und gibt das Spiel nach r101 zurück. Ursachen, vollständige Austauschdateien und aktuelle Prüfungen: [K17-FIX1.md](docs/K17-FIX1.md). Versionsnummer und vorhandener Disc-Import bleiben gültig.

**K17 / 0.37.17-dev:** Der erste frische L2/LT-Druck startet das Zielen auch nach Koffer und Waffenwechsel. Die ausrüstbaren Waffen besitzen ihre Schuss-, Ziel-, Nachlade- beziehungsweise Wurfabläufe; Zielfernrohre, Maschinengewehr-Dauerfeuer, Matilda-Salven und echte Geschosse sind angebunden. Gesundes Messerwegstecken verwendet die gesunde Originalbewegung. Waffenwechsel erhalten die GPU-Daten aller Gegner und Tiere. Einzelheiten, vollständige Austauschdateien, APK-Prüfung und der ausstehende Gerätetest stehen in [K17-KORREKTUREN.md](docs/K17-KORREKTUREN.md). Die folgenden Versionsnotizen beschreiben historische Stände.

**K16 / 0.37.16-dev:** Zusätzliche Cheatwaffen lassen sich im Koffer ausrüsten und verwenden ihre Originalmodelle, Griffe, Tragebewegungen und Menüposen. r101 besitzt die originale LOOK-Aktivierung mit Fernglassequenz und anschließendem steuerbarem Fernglas. Die verletzten Laufbewegungen sind angebunden; der Android-Absturz beim Bestätigen von CONTINUE und die falsche EM15-Typ-11-Stimmzuordnung sind korrigiert. Einspielhinweise, Geräteprüfung, Originalbezug und der weiterhin begrenzte Waffen-Kampfumfang stehen in [K16-KORREKTUREN.md](docs/K16-KORREKTUREN.md). Die folgenden Einträge beschreiben historische Stände.

**K15 / 0.37.15-dev:** Alle 66 originalen AEV-Verbindungen der 29 Stage-1-Räume sind registriert. r101 besitzt jetzt den Dorfkampf mit EM15-Population, Hausereignis, Nachschub, Turm-Brandflaschen und Glockenabschluss. CONTINUE YES lädt den Kontrollpunkt; SELECT öffnet englische Test-Cheats. Die Testknöpfe sind entfernt. Installation, geprüfter Umfang und verbleibende Grenzen: [K15-DORF.md](docs/K15-DORF.md). Die Bootsankunft r11b → r10d, weitere Raumskripte und der Kampf mit zusätzlichen Waffen bleiben offen; eine vollständige Originalgleichheit wird nicht behauptet. Die folgenden Meilensteine beschreiben historische Stände.

**K14 / 0.37.14-dev:** Androids automatische Fokusmarkierung ist für die Titelansicht deaktiviert. Sie konnte nach dem Demo-Rückweg beim ersten DPAD-Tastendruck die gesamte Menüfläche grau aufhellen. Die K13-Korrekturen wurden vom Nutzer bestätigt; dieser Nachtrag betrifft die Android-Fokusdarstellung. Dateien und Prüfungen: [K14-FOKUS.md](docs/K14-FOKUS.md).

**K13 / 0.37.13-dev:** Das Titelmenü beginnt auf START; das Beenden von LOAD und SAVE beginnt auf YES. Nach dem Demo-Video laufen die originalen Titelanimationen weiter, mit eigener schwarzer Rückblende und erneuerter Zeichenfläche. Die bislang fehlenden SCR_AT-Kollisionsflächen aus den Originaldaten blockieren Polizeiauto und Lastwagen in r100 und wechseln mit dem originalen Ereigniszustand. Vollständige Ersatzdateien, Prüfungen und verbleibende Grenzen: [K13-KORREKTUREN.md](docs/K13-KORREKTUREN.md). Der Gerätetest steht aus.

**K12 / 0.37.12-dev:** Spielsprache, Disc-Installer und alle eigenen Menü-, Steuerungs-, Status-, Diagnose- und Fehlertexte sind Englisch. Neue Disc-Imports überspringen die separaten japanischen Schriftdateien und die fünf ungenutzten Spracharchive für LOAD/SAVE. Gemeinsame Originalarchive liefern weiterhin ihre englischen Textslots. Vorhandene Imports bleiben gültig. LOAD und SAVE wurden vom Nutzer mit K11 als originalgetreu bestätigt. Vollständige Dateien und Prüfungen: [K12-ENGLISCH.md](docs/K12-ENGLISCH.md).

**K10 / 0.37.10-dev:** Titel-Fade nach Intro-Skip, statische SAVE-/LOAD-Schreibmaschinengrafiken, stabile Spinell-Darstellung, reine FPS-Anzeige mit gleichem linken/unteren Abstand sowie Original-Health-/Inventardarstellung sind korrigiert. Ashleys getrennte Lebensanzeige, Zustände und gespeicherte Lebenswerte sind für die spätere Partneranbindung vorbereitet. Vollständige Ersatzdateien über K5–K10 und Prüfbericht: [K10-KORREKTUREN.md](docs/K10-KORREKTUREN.md). Ein Android-Gerätetest steht aus.

**R100-Korrekturen 3 über Korrekturen 2:** Originale Spiegelungs- und CORE-Beleuchtungspässe, korrigierte Ambient-/Lichtzuordnung, GPU-Koffer-Leon, Original-Aufnahmeschrift, Tiefenkopie der Sprengfallen sowie gemeinsamer SAVE-/LOAD-Ablauf mit zwanzig Plätzen und Hauptmenü-LOAD. Vollständige Dateien, Einspielhinweise, Prüfungen und verbleibende Grenzen: [UPDATE-R100-KORREKTUREN3.md](docs/UPDATE-R100-KORREKTUREN3.md). Geräteprüfung und vollständige 1:1-Gleichheit bleiben ausstehend.

**M37 / 0.37.0-dev:** Originale Engine-Introfolge, Funkgespräche, r100-Nebenereignisse, Vögel, rettbarer Hund, Fallen, Gegenstands-/Treffereffekte, frühe Gegnerabläufe sowie Speichern und Laden an der Schreibmaschine sind ergänzt. Der Schussfehler mit anschließendem gelbem Diagnosemodell und das Verschwinden dauerhafter Modelle nach Ereignissen sind korrigiert. Das bereitgestellte Illuminados-Bild ersetzt das App-Icon, einschließlich runder und adaptiver Launcher. Dieses ZIP enthält vollständige geänderte/neue Dateien über **M36**. Umfang, Prüfungen und konkrete verbleibende Grenzen: [UPDATE-M37.md](docs/UPDATE-M37.md). **Vollständige 1:1-Gleichheit des ersten Levels ist weiterhin nicht nachgewiesen; ein Android-Gerätetest steht aus.**

**M36 / 0.36.0-dev:** Korrigierte Videokomposition, dauerhaft sichtbares Titelmenü, bildschirmfüllende Introflächen und gemeinsame Originalrahmen auf allen Ingame-Seiten. Die originalen r100s03/r100s20-Ereignisse und sieben tatsächlichen r100-Gegner sind angebunden. Originaler Wandabstand, GX-Wasser-Kopierfilter, Feuerwaffenschäden und Ladevorbereitung wurden korrigiert. Installation über **M35**, Prüfungen und verbleibende Grenzen: [UPDATE-M36.md](docs/UPDATE-M36.md). Vollständige Engine-/Leon-/Kampagnengleichheit und ein neuer Android-Gerätetest stehen weiterhin aus.

**M35 / 0.35.0-dev:** Geschlossene Fenster blockieren jetzt den Leiterzugriff; originale SEQ/FSE-Steuerung liefert Schritte und Landegeräusche im Wasser; Waffen beginnen beim Anlegen mit dem Original-Zielpitch; die weiten r10c-Sprünge verwenden PL90–93. Alle Kofferseiten nutzen den tatsächlichen Bildschirm. Der Android-Start zeigt originale Logos, Titel/Optionsgrafiken und Filme und beginnt danach den begrenzten r100-Erstkampf mit Original-Axt, regulären Lebenspunkten und Trefferreaktionen. **Vollständige Leon-, EVD-, Save- und Kampagnen-/Gegnergleichheit ist noch nicht erreicht.** Installation, Prüfungen und Grenzen: [UPDATE-M35.md](docs/UPDATE-M35.md). Das ZIP enthält vollständige geänderte/neue Dateien über **M34**, einschließlich aus den bereitgestellten Originalfilmen erzeugter Android-MP4s.

**M34 / 0.34.0-dev:** Wiederholtes Durchsteigen zerbrochener Fenster mit dem originalen kurzen Zaunablauf, Mündungsfeuer an der tatsächlichen Waffenmatrix, korrigierter Schrotflinten-Rückstoßabschluss und fokusgeschütztes Laden. Originale Esp42-/Esp45-Wassergitter einschließlich r10a und des in R10bInit gestarteten Sees werden simuliert und gezeichnet. Kompatible Fenster, Türen, Behälter und freie Funde werden raumübergreifend angebunden; Original-ITA-Flags verhindern doppelte Schätze zwischen Räumen. Der Koffer verwendet Originalgrafiken, vier Originalkoffermodelle und Leons Originalpose. HP∞ bleibt aktiv. [UPDATE-M34.md](docs/UPDATE-M34.md) erklärt Installation über **M33**, alle sieben Befunde und die verbleibenden Grenzen; [VALIDATION.md](docs/VALIDATION.md) enthält die Prüfungen. Die vollständigen Quelleninventare für Leon sowie alle Objekt-/Interaktionsregistrierungen sind verlinkt. **Vollständige 1:1-Gleichheit aller Aktionen, Raumskripte, GX-Effekte und Menüanimationen ist weiterhin nicht erreicht.**

**M33 / 0.33.0-dev:** Korrigierte Aktionskameras und geglättete Traversaldarstellung, Messerbruch von Glas, Fenstereintritt beim Leitersteigen, originales Leiterumwerfen/-aufstellen und SAT-basierte Zaunaktionen. Ein funktionales Ingame-Menü verbindet Koffer, Schlüssel/Wertgegenstände, Karte, Originaldokumente und originale3D-Gegenstandsansichten; Aufnahme und Platzschaffen erfolgen in der Spielansicht. Der einzelne EM12/cEm10-Testgegner erhält Originalstimmen und Blutaufträge. HP∞ bleibt aktiv. [UPDATE-M33.md](docs/UPDATE-M33.md) erklärt Installation über **M32+Fix1**, universale Bedienung und verbleibende Original-Engine-/Menügrenzen. Tatsächlicher ARM64-Android-Build und Lint sind durchgeführt; [VALIDATION.md](docs/VALIDATION.md) trennt Build, Originaldaten-/GLES-Hostprüfung und ausstehenden Gerätetest. Nur vollständige geänderte/neue Dateien, keine Spieldaten.

**M32 / 0.32.0-dev:** Direkte Startknöpfe für das gegnerfreie Dorf r101 und den einzelnen r400-Ganado-Test. Originale WEP02-Startpistole und WEP07-Dorfschrotflinte, ausrüstbarer nativer Koffer, bestätigte Gegenstandsaufnahme, originale Schuss-/Nachladeanimationen, Mündungsfeuer/Rauch/Lichtimpuls, korrigierte Laserfreigabe, Hausleitern und originale Fensterkamera/-splitter. Raumbezogene Originalmusik und ESE-Umgebungssounds; korrigierte LIT-Auswertung und originaler Fußschattenpass. HP ∞ bleibt aktiv. Das Dorf ist ein **Funktions-Testprofil**, noch kein vollständiger Originallevel: Haus-/Glockenskripte, EM15-Dorfpopulation, originale Sscrn-Menüoptik und vollständige GX-/TEV-/Schatten-/Filtereffekte bleiben offen. [UPDATE-M32.md](docs/UPDATE-M32.md) erklärt Installation, Bedienung, Quellen und Grenzen. Dieses Änderungs-ZIP enthält vollständige geänderte/neue Dateien über **M31**, keine Spieldaten. Native/GLES-Hostprüfungen ersetzen keinen Android-Gerätetest.

**M31 / 0.31.0-dev:** Ein Raumkatalog mit einem direkten Öffnen-Knopf ersetzt die bisherigen Testknöpfe im Einstieg; Import und Diagnosen liegen im Menü. Die 108 in den Originaltabellen registrierten Kampagnenräume sind mit ★ markiert, Zusatzmodi und fünf unbenutzte Raumdatensätze getrennt. „Erkunden ohne Gegner“ verwendet dieselbe native Darstellung. 123 Raumarchive sind auf dem Host über Produktions-JNI/GLES geöffnet und gezeichnet worden, 122 besitzen einen begehbaren Startboden; r30f bleibt vorerst eine klar gekennzeichnete 3D-Ansicht der Baggerfahrt. Vier unbenutzte Archive mit altem Modellformat sind gesperrt. Originale LIT-Cut0-/Nebelwerte sind jetzt raumübergreifend angebunden. **r100/r101 erhalten transparente Originalfenster und die drei originalen Fenstersprunganimationen**, Bruchmodelle, Tonzeitpunkte und sitzungsfeste Flags. Der Waffenlaser erscheint während des Ziehens ab Originalframe 4. HP ∞ bleibt aktiv. Der verfügbare r400-Ganado-Kampftest ist ausdrücklich kein vollständiger Original-Gegnerablauf. Spezielle Raumeffekte, dynamische Licht-Cuts und Kampagnenskripte sind weiter offen. Details und kurzer Gerätetest: [UPDATE-M31.md](docs/UPDATE-M31.md). Das ZIP enthält vollständige geänderte/neue Dateien über dem vollständigen **M30-Stand**, ohne Spieldaten. Kein neuer Android-/Adreno-Gerätetest wird behauptet.

**M30 / 0.30.0-dev:** Treppen-/Türkorrekturen, solide und mit Originalkurven schiebbare r101-Möbel, korrekt über dem Boden gezeichneter Schrank-Schatten, erneut originalrote Laserlinie/Zielpunkt und FPS neben `HP ∞`. Beim Hinunterspringen entfällt die alte gelbe Diagnosefläche. **r100 lädt erstmals alle fünf originalen BLK-Raumdateien:** Der zuvor fehlende Boden, Wald und die Gebäude werden mit ihren eigenen SMD-/Common-Ressourcen gezeichnet; Originalstart nach dem Intro, LIT-Cut0 und Brückenvarianten sind berücksichtigt. Die fünf Blöcke bleiben in diesem ersten Profil gemeinsam resident. Kampagnenereignisse und der originale Dorfkampf bleiben offen. [UPDATE-M30.md](docs/UPDATE-M30.md) enthält Ursachen, Originalcode-Belege und Gerätetest. Das Änderungs-ZIP wird über den vollständigen **M29-Stand** gelegt; Ordner `RE4-Android-Decomp/`, ausschließlich vollständige geänderte/neue Dateien, keine Spieldaten. Host-Prüfungen ersetzen keinen neuen Android-/Adreno-Gerätetest.

**M29 / 0.29.0-dev:** r101 bekommt das originale Himmelsobjekt, aus der Disc gelesene LIT-Beleuchtung und den ursprünglichen linearen Nebelschleier. A/Kreuz springt an unterstützten SAT-Kanten mit Originalkurven hinunter; die feste Turmleiter behält ihren bisherigen Rückweg. Zwischen den originalen 30-Hz-Takten werden Leons Position und Körperrichtung ausschließlich für die Darstellung interpoliert. Leon hat ab jetzt im Android-Testbetrieb **unendliche Gesundheit**; Trefferreaktionen und Gegenwehr bleiben aktiv. Die 23 frei platzierten ITA-Records in r101 erhalten originale Modelle, Mengen, Aufnahmebereiche und Sitzungsflags. Pistolenmunition und Pesetas sind nutzbar; die Schrotflinte lässt sich aufnehmen, ihr Waffenwechsel fehlt noch. **Der originale Dorfkampf mit mehreren EM15-Gegnern und EVD-/Glockenablauf ist noch nicht implementiert.** Installation, Tests und Grenzen: [UPDATE-M29.md](docs/UPDATE-M29.md). Das ZIP enthält nur vollständige geänderte/neue Dateien über **M28**, unter `RE4-Android-Decomp/`, ohne Spieldaten. Für M29 wird kein neuer Android-/Adreno-Gerätetest behauptet.

**M28 / 0.28.0-dev:** Laseransatz am animierten originalen Waffenmarker, originaler roter Zielpunkt auf unterstützten verletzbaren/zerstörbaren Treffern, geglätteter rechter Kamerastick und feste Turmleitern in r101/r400 mit originalen Auf-/Abwärtskurven und Geräuschen. **A/Kreuz** startet die angezeigte Leiteraktion bei abgelegter Waffe. Der Rückweg vom Turm ist eine ausdrücklich begrenzte Android-Anpassung; bewegliche Hausleitern bleiben ereignisabhängig. Installation, Prüfungen und Grenzen: [UPDATE-M28.md](docs/UPDATE-M28.md). Dieses ZIP enthält ausschließlich vollständige geänderte/neue Dateien über **M27**. Ein frischer Android-SDK-/NDK-Build konnte in dieser Umgebung nicht ausgeführt werden; native Produktions-JNI/GLES- und Originaldatenprüfungen sind dokumentiert.

**M27 / 0.27.0-dev:** Erster Baustein des zusammenhängenden r101-Spielabschnitts: gemeinsame originale Raum-/Objektflags, vier Innenraumtüren mit Originalaktionen und dynamischer Laufkollision, normales Türschloss und fünf Fenster mit Originaltrefferregeln und vorhandenen Bruchmodellen. **A/Kreuz** öffnet eine erreichbare Tür; erneut während des Öffnens gedrückt tritt Leon sie ein. Fenster-/Schlosszustände bleiben bei Raumwechsel und Grafikverlust innerhalb derselben Sitzung erhalten. Leitern, Fenstersprünge, Hausereignis, mehrere Ganados und Glockenablauf folgen. Installation, Originalcode-Belege und kurzer kombinierter Test: [UPDATE-M27.md](docs/UPDATE-M27.md). Dieses ZIP ersetzt ausschließlich geänderte/neue Dateien über dem vollständigen **M26-Fix1-Stand**.

**M26-Fix1 / 0.26.1-dev:** Messer, Tritt und Griffanimationen werden jetzt auch beim direkten Öffnen eines Raums geladen. L1/LB halten zieht das Messer, RT/R2 schneidet; Loslassen steckt es weg. Der Start lädt die vorhandenen originalen Spielerclips aus PL00/WEP01/EM12 und erzeugt dadurch keinen Gegner. Ursache, Logprüfung und kurzer Gerätetest: [UPDATE-M26-FIX1.md](docs/UPDATE-M26-FIX1.md). Dieser kleine Patch wird über den vollständigen M26-Stand kopiert.

**M26:** Vollständige geänderte/neue Dateien über dem vollständigen M25-Stand. Installation und kurzer Gerätetest: [UPDATE-M26.md](docs/UPDATE-M26.md).

Android-Studio-Quellprojekt, Paket `com.ast.re4decomp`, Ordner **RE4-Android-Decomp**.

**Aktueller Stand K17 Fix6:** Nativer Start über r100 und Dorfkampf r101 bis zum Glockenabschluss, originale Kuh und Hühner, LOOK/Fernglas, Original-SAVE/LOAD und Kontrollpunkt-CONTINUE, Koffer und erweiterter Waffenkampf. Die Stage-1-Tabelle enthält 29 Räume und 66 Originalverbindungen. In den folgenden Räumen laufen die gemeinsamen Raum-/Kollisions-/Interaktionssysteme; ihre vollständigen Gegner- und Storyabläufe sind noch nicht portiert. Besondere Boots-/Ferry-/Boss-/Begleiterabläufe bleiben offen. Die übrigen Entwicklungsnotizen unten erklären ihre jeweilige historische Ausbaustufe.

Über START und r100 gelangt Leon regulär nach r101. Der Dorfkampf verwendet die ursprünglichen Szenarioflags, Alarm-, Haus-, Nachschub- und Glockenabläufe. Scheiterhaufen, Türschloss, ursprüngliche Objektfreigaben und 13 Behälter mit festen/zufälligen Inhalten sind angebunden. Der r400-Kampftest erhält drei explosive Fässer: zwei Originaltakte nach einem Treffer folgen radialer Schaden, Kettenreaktionen und originale Leon-/Ganado-Reaktionen. Zerstörung benutzt originale EFM-Teile, Flug/Bodenkontakt und Ausblenden. Nahe Wände blenden nur störende Kopfbestandteile aus; Körper und Waffe behalten die Tiefenprüfung.

Originale DVD/ISS/DLS-/DSP-Daten liefern Schuss, Messer, Nachladen, Behälterzerstörung, Explosion und Dorfbrand. Raumkurven steuern Distanz und Stereoposition; AudioTrack läuft unabhängig vom Renderer. Vollständige Musik, Schrittsequenzen und Gegnerstimmen bleiben offen. Alle 142 verfügbaren Raumdateien wurden automatisch für Audio und STB geprüft; Gerätetests konzentrieren sich auf gemeinsame Systeme und besondere Raumszenarien.

Die App importiert beide Originaldiscs, entpackt und prüft alle 142 Raumarchive, liest die unterstützte starre SMD/BIN-Geometrie und zeigt originale TPL-Texturen, Mips, Masken, Vertexfarben und Normalen. Gemeinsame SMD-Ressourcen und SMX-Materialparameter werden verbunden. SAT-Boden-/Wandabfragen, Kapselbewegung, Wandgleiten und Randbegrenzung erlauben controllergesteuerte Tests.

Die sieben FCV/SEQ-Clips liefern Stand, Gehen, Laufen, Rückwärtsgehen, Drehungen und die schnelle 180°-Drehung. **M24 übernimmt die originalen PAD-Richtungssektoren und Drehregeln:** oberhalb der originalen Stickschwelle läuft die gewählte Animation mit ihrem vollen 30-Hz-Takt; die Stickauslenkung verlangsamt sie nicht mehr. Gehen/Rückwärtsgehen drehen um 2,4°, Rennen um 2,64° pro Originaltick. Drehungen im Stand und die schnelle 180°-Drehung behalten ihre Originalkurven. Leon behält 119 Gelenke, sieben Modellbestandteile und elf Originaltexturen.

Waffenmodelle, Griffe und Aktionen verwenden die Originalarchive und ihren festen 30-Hz-Takt. LT/L2 legt an, RS zielt, RT/R2 schießt oder wirft und X/Quadrat lädt geeignete Waffen nach. L1/LB hält das Messer, RT/R2 schneidet. Magazin, Reserve und aufgenommene Munition gehören zur jeweiligen Waffenfamilie. Die K17-Prüfung lädt alle 38 vorhandenen Waffen-/Zubehörprofile und bedient alle 25 ausrüstbaren Cheat-Gegenstände durch die Produktions-JNI.

**M21 erweitert den Kampf-Test um das originale Messer und den kontextuellen Kopftreffer-Tritt**, einschließlich Fall-, Boden-, Aufsteh- und Boden-Trefferkurven. Die horizontale Zieldrehung ist jetzt zusätzlich im Waffenpfad korrigiert. Die Spielansicht verwendet den ganzen Bildschirm; ein kleiner Pfeil öffnet das pausierte Testmenü und „Fortsetzen“ kehrt zur vorhandenen Szene zurück. Lange Diagnoseanzeigen sind standardmäßig ausgeblendet. Bedienung, Update und Grenzen: [UPDATE-M21.md](docs/UPDATE-M21.md).

**M24 ersetzt die frei drehende Laufkamera durch eine an Leon gebundene Verfolgerkamera.** Augen- und Zielpunkt stammen aus dem originalen `CameraQuasiFPS`-Übergangsprofil und drehen gemeinsam mit Leon; RS verändert einen begrenzten lokalen Blickversatz statt eine freie Weltrotation. Nach dem Loslassen klingt der Versatz mit der originalen 0,9-Regel im 30-Hz-Takt ab. Bewegung, Pistole und Messer verwenden bewusst dasselbe Profil und 50° Bildwinkel für die gewünschte einheitliche Bildkomposition. Die originale Kamera benutzt beim Waffenanlegen teilweise andere Profile; diese Anpassung ist ausdrücklich kein unveränderter Vollport ihrer gesamten Zustandsmaschine. M25 benutzt die originalen drei SEE-Sichtlinien, die ursprüngliche Ausschlussmaske und das tatsächliche Bildschirmseitenverhältnis; die Blickrichtung wird danach aus dem tatsächlichen Augen- und Zielpunkt neu berechnet.

**r400 erhält 33 originale ETS/ETM-Objekte**, darunter sechs Türen, fünf Fenster, vier Leitern und 18 Behälter. Modelle, Texturen und Platzierungen stammen aus der Disc. M25 verbindet 13 Kisten/Fässer mit ihren Originaltrefferflächen und ITA-Inhalten: Schuss oder Messer zerstören sie, originale ITM-Modelle werden sichtbar, A/Kreuz nimmt sie auf. Pistolenmunition ergänzt die vorhandene Reserve. Y/Dreieck öffnet das pausierte Menü mit Vorräten; ein grünes Kraut heilt bis zu 600 HP. Rote Kräuter, Gewehrmunition und Brandgranaten bleiben erhalten, ihre Nutzung ist noch nicht angebunden. Türen, Leitern und dynamische Fass-Laufkollision bleiben eigene Laufzeiten; M26 ergänzt die Behälterzerstörung mit Originalfragmenten. Zusätzlich erscheinen die sechs originalen EM28-Hühner aus `omake01.esl`, mit beiden Modellvarianten und fünf Wait-/Walk-Kurven. Bodenfähige Tiere bewegen sich auf SAT; eine Platzierung ohne passenden statischen Boden bleibt animiert am Originalplatz. Erste globale r400-EFF-Effekte zeigen Feuerstellenglut und Umgebungslichtstrahlen. Das Scheiterhaufenfeuer mit Rauch erscheint im neuen r101-Test. Weitere Fackel-/Gebietseffekte bleiben gesonderte Laufzeiten.

M23 verlegte den Ganado-Nahkampftest nach r400 und korrigierte Gegenwehr sowie die vertikale Blickachse. Kurze LS-Wechsel bleiben unabhängig von der Menüsteuerung erhalten; M25 stellt den rechten Y-Eingang wieder auf natürliche Richtung: physisch rauf schaut/zielt rauf, runter schaut/zielt runter. Die M23-Kamera wird durch die oben beschriebene M24-Verfolgerkamera ersetzt. Historische Details: [UPDATE-M23.md](docs/UPDATE-M23.md).

**M22 ergänzt Originalhimmel und animiertes Wasser in r100/r300/r301.** Die Himmelsobjekte behalten ihre ursprüngliche Geometrie und die unterstützten SMX-Bewegungen. Wassertexturen werden aus den originalen EFF-/ANM-/TPL-Daten erzeugt; r300 erhält eine eigene Spritzerschicht. Pause und Grafik-Kontextwiederherstellung sichern den Effektzustand. [UPDATE-M22.md](docs/UPDATE-M22.md) beschreibt Daten, Update und Gerätetest. Dies bleibt ein begrenzter nativer Testlauf, kein vollständiger Kampagnenstart.

Der M20-Griff mit gekoppelten Gegner-/Leon-Kurven, HP, Gegenwehr und Aufstehen bleibt erhalten. Im Griff LS rasch links/rechts bewegen oder A/Kreuz und B/Kreis wiederholt drücken. Details: [GRAB-COMBAT.md](docs/GRAB-COMBAT.md).

Der einzelne unbewaffnete EM12-Ganado aus M19 behält 34 Gelenke, drei Texturen und den originalen ESL-Datensatz 0x25 mit 1.000 HP. Leons tatsächliche Schüsse prüfen die animierten Originaltrefferzonen und EAT-Weltkollision; normale Körper-/Kopftreffer verursachen 135/162 Schaden. M21 benutzt dieselben vorhandenen Disc-Dateien. Gegnerbeute, weitere Angriffe und vollständige Szenario-KI fehlen weiterhin. [ENEMY-ENCOUNTER.md](docs/ENEMY-ENCOUNTER.md) beschreibt die bisherigen Trefferregeln.

Originale Debug-Raumstarts aus `Debug/roominfo.dat` ersetzen den bisherigen Zufalls-/Testboden, sofern sie mit der Kapsel gültig sind. Die unveränderten AEV-Türen 0 zwischen **r301 und r300** bilden den ersten geprüften nativen Raumwechsel. „Türtest: originalen r301-Einstieg laden“ führt zu einem erreichbaren Einstieg; A/Kreuz löst die angezeigte Türaktion bei abgelegter Waffe aus. Der Nutzer hat den Lauf **r301 → r300 → r301** auf dem Gerät bestätigt; das Log `Eingefügter Text(20261003-225513).txt` enthält beide erfolgreichen Transfers. Hintergrundladen, Original-Prüfsummen, Zielkollision, Grafik-Commit und Rückfall auf den bisherigen Raum sind getrennt geprüft. Details: [UPDATE-M18.md](docs/UPDATE-M18.md), [WEAPON-ACTIONS.md](docs/WEAPON-ACTIONS.md), [ROOM-FLOW.md](docs/ROOM-FLOW.md), [PLAYER-PREVIEW.md](docs/PLAYER-PREVIEW.md) und [WALK-PROBE.md](docs/WALK-PROBE.md).

Der vollständige unveränderte Upstream liegt in `upstream/re4/`. Geprüfte native Portierungen werden ausdrücklich in CMake eingebunden. Die vollständige ursprüngliche Spieler-/Szenariozustandsmaschine, sämtliche Gegner-KI, die gesamte Audioengine und alle Kampagneneffekte sind noch nicht vollständig übertragen. Unterstützte Raumwechsel folgen den Originalzielen und ihren Szenariosperren; ein fehlgeschlagener Sonderweg erhält den bisherigen Raum. Der aktuelle Umfang und die Prüfungen stehen in `docs/K17-KORREKTUREN.md`; `docs/UPDATE-M02.md` bis `docs/UPDATE-M18.md` dokumentieren frühere Daten-/Rendererstufen.

## Anforderungen und Build

- Android 5.0 / API 21 oder höher, **ARM64**, OpenGL ES 3.0.
- Android Studio 2025, beispielsweise Narwhal 2025.1.x; Gradle-JDK 17.
- Android SDK Platform 35, Build Tools 35.0.0, NDK **28.2.13676358**, CMake **3.22.1**.
- Android Gradle Plugin **8.9.2**, Gradle **8.11.1**; Wrapper im Projekt enthalten.
- Internet für den ersten Gradle-Sync und fehlende SDK-Werkzeuge.

Den Ordner **RE4-Android-Decomp** öffnen, Gradle synchronisieren und `app` bauen/starten.
Android Studio erzeugt die lokale `local.properties`. Keine SDK-Pfade aus einem anderen Rechner übernehmen.
Alternativ: `bash ./gradlew assembleDebug lintDebug` beziehungsweise `gradlew.bat assembleDebug lintDebug`.
Signierschlüssel sind nicht enthalten; ein Debug-Build nutzt den lokalen Android-Debugschlüssel.

API 21 folgt aus ARM64 und dem gewählten modernen NDK. GLES 3.0 erfordert keine höhere API.
Es werden keine AAudio-, Vulkan- oder anderen neueren APIs zwingend vorausgesetzt.
Das ist die Mindestanforderung dieser implementierten Basis, **noch keine gemessene Leistungs- oder Kompatibilitätszusage für das fertige Spiel**.
Eine spätere Mindestversion darf nur durch eine konkrete benötigte Engine-Funktion erhöht werden.
OUYA, ARM32 und Android 4.1.2 sind ausdrücklich außerhalb dieses Projekts.

## Erster Start / Spieldaten

1. Die geteilten Upload-Archive auf dem Rechner zusammensetzen und entpacken.
2. Die beiden **rohen GCM- oder ISO-Dateien** auf das Android-Gerät kopieren. ZIP, 7z, RVZ und NKit werden nicht gelesen.
3. In der App zuerst „Disc 1 importieren“, anschließend „Disc 2 importieren“ wählen.
4. Den Import im Vordergrund beenden lassen. Bei Abbruch wird kein halbfertiger Import aktiviert.
5. Einen Kampagnenraum wählen und mit „Gewählten Raum in 3D öffnen“ direkt starten. Das Menü enthält weiterhin den Disc-Import und die Grafik-/Controller-/Archivdiagnosen.

Unter dem mit `getExternalFilesDir(null)` ermittelten Verzeichnis entstehen:

```text
Android/data/com.ast.re4decomp/files/game/
  disc1/files/...          # Disc 1, Originalpfade und Schreibweise
  disc1/sys/main.dol       # Referenzdatei, auf Android nicht ausgeführt
  disc1/manifest.tsv       # SHA-256 aller extrahierten Dateien
  disc1/ready.properties
  disc2/files/...          # Disc 2 bleibt separat, keine Überschreibungen
  disc2/sys/main.dol
  disc2/manifest.tsv
  disc2/ready.properties
```

Die Quelle wird während des Imports kurzzeitig lokal zwischengespeichert, damit auch nicht seekbare Dokumentanbieter funktionieren.
Einzelimporte benötigen freien Speicher für **eine GCM-Datei plus ihre extrahierten Dateien und 64 MiB Reserve**.
Der temporäre GCM-Zwischenspeicher wird anschließend gelöscht. Bei Neuimport wird zusätzlich der alte Disc-Datenbestand bis zum Commit behalten.
Die Speichergröße der gesamten Dateien wird aus der Disc-Dateitabelle berechnet und vor dem Entpacken geprüft.
Seit K12 werden dabei nur die tatsächlich zu importierenden Dateien gezählt. Die separate japanische Schrift und die japanischen/deutschen/französischen/spanischen/italienischen CARD-Archive werden nicht entpackt. Englisch und gemeinsame Ressourcen bleiben erhalten; die App bietet keine Sprachwahl.
Deinstallation entfernt Android-App-Daten; die ursprünglichen GCM/ISO-Dateien außerhalb des App-Verzeichnisses behalten.

Der Import prüft `G4BE08`, Discnummer, Revision 0, FST-Grenzen und Originalcode-Prüfsummen:
Disc 1: Hauptprogramm + 110 REL-Module; Disc 2: Hauptprogramm + vier Inselmodule.
Die Referenzen stammen aus dem Upstream-Hashkatalog und wurden mit den bereitgestellten Discs abgeglichen.
`references.tsv` enthält dazu 116 geprüfte Dateiausschnitte. REL-Code innerhalb von DRS-Dateien wird ebenfalls geprüft.

Der Import erhält die für diese native Testbasis vorgesehenen Dateien einschließlich Debug-Symboldateien und Objekt-/Tierdateien, mit der oben beschriebenen Ausnahme der nicht verwendeten separaten Sprachressourcen.
Eine minimale, sprach-/modusabhängige Assetliste kann erst nach erfolgreichem Engine-Start und Analyse aller Laufzeit-Ladewege seriös festgelegt werden.
Die App prüft die Extraktion, konvertiert aber noch keine Modelle, Archive, Spielmodule oder Videos in fertige Laufzeitformate.
`ready` bezeichnet einen abgeschlossenen Disc-Import, nicht ein spielbares RE4.

## Controller

| Funktion | GameCube, Standardbelegung | Universaler Controller |
|---|---|---|
| Bewegung, Menünavigation | Hauptstick / D-Pad | Linker Stick / D-Pad |
| Kamera / Zielen | C-Stick / Hauptstick bei angelegter Waffe | Rechter Stick: Kamera beim Gehen, Zielen mit LT/L2 |
| Aktion / Bestätigen | A | A / Kreuz |
| Rennen / Abbrechen | B | B / Kreis |
| Nachladen | R halten + B | X / Quadrat als eigene Aktion |
| Inventar | Y | Y / Dreieck |
| Waffe anlegen | R | LT / L2 |
| Feuer | R halten + A | RT / R2 bei gehaltenem LT/L2; im Ganado-Test mit Treffern und Testmunition |
| Messerhaltung | L | LB / L1 |
| Messerangriff | L halten + A | RT / R2 bei gehaltenem LB/L1 |
| Ashley-Kommando | X | RB / R1 |
| Pause | Start | Start / Menu |
| Karte | Z | Select / View |

Originalbelege: `include/joy.h`, `src/game/pad.cpp` (`Key_type_tbl[0]`),
`pl_class.cpp::keyReload`, `pl_sub.cpp::PlSetWhistle`, `sscrn.cpp::SubScreenCall`.
Das sind semantische Aktionen: Die native Runtime hält keine Original-PAD-Taste künstlich gedrückt.
Pistolen-, Messer- und Griffaktionen sind für den begrenzten Test bereits angebunden; weitere QTEs benötigen ihre zusätzliche Engine-Anbindung.

Androids `BUTTON_A/B/X/Y` werden als Standardpositionen interpretiert. Bei umgeschalteten Nintendo-Layouts steht „A/B und X/Y tauschen“ zur Verfügung.
Beide Sticks werden normalisiert; radialer Deadzone-Wert 0,15; rechter Stick über Z/RZ oder RX/RY.
Analogtrigger haben Hysterese: Aktivierung >0,55; Freigabe <=0,35. Digitale Trigger werden alternativ akzeptiert.
D-Pad über Tasten und Hat-Achsen. Eingaben werden bei Pause, Fokusverlust und Controller-Trennung gelöscht.
Kurze Tastendrücke zwischen Renderframes werden als Flanken aufbewahrt; Key-Repeat erzeugt keine neuen Press-Flanken.
Geräte mit echten Stickachsen haben Vorrang vor virtueller Navigation. Ein gleichrangiger zweiter Controller überschreibt den aktiven nicht. Virtuelle Eingaben bleiben als Rückfall möglich, wenn kein echter Stickcontroller verfügbar ist.

In der Raumansicht startet Leon automatisch auf dem originalen Debug-Raumstart, sonst auf einem ausdrücklich bezeichneten SAT-Teststart. Linker Stick/D-Pad: vor/zurück und drehen; B/Kreis halten: laufen; zurück + B/Kreis drücken: schnelle 180°-Drehung. A/Kreuz bedient bei abgelegter Waffe die verfügbare Tür, Start pausiert. Der rechte Stick verändert beim Gehen den begrenzten Schulterblick und bei angelegter Waffe die Zielrichtung. LT/L2 halten legt die Waffe an, RT/R2 schießt, X/Quadrat lädt nach; LT/L2 loslassen legt die Waffe wieder ab. Nach Pause oder Controllerwechsel beide Trigger freigeben und anschließend erneut anlegen. Augen- und Zielpunkt der Verfolgerkamera drehen stets gemeinsam mit Leon; nach RS-Freigabe kehrt der lokale Blickversatz zur Neutralstellung zurück.

Die Schaltfläche „Modellprüfung / Leon-Steuerung“ wechselt zur Modellprüfung; X/Quadrat bleibt der Nachladeaktion zugeordnet. Y/Dreieck öffnet nun das pausierte Vorrats-/Testmenü; Select schaltet Normalen. Nur in der Modellprüfung ändern LT/RT den Kameraabstand, LB/RB die Basis-Platzierung und Start die Gesamtgeometrie. Die Tabelle oben beschreibt auch zukünftige Spielaktionen; Ashley-Kommandos, der vollständige Inventarkoffer, Spielmenüs und weitere QTEs sind noch nicht an ihre Originalruntime angebunden.

Vorhandene vollständige Disc-Imports bleiben verwendbar. Der Türtest benötigt r301/r300 von Disc 2; Leon benötigt PL00/WEP01 von Disc 1; der Ganado-Test zusätzlich EM12/emleon00.esl und r400 von Disc 1. Die Tiere verwenden `em/em28.drs` und `etc/omake01.esl` aus demselben Disc-1-Import. Bestehende vollständige Imports müssen dafür nicht neu ausgeführt werden. `RoomRepository.java` prüft gepackte Quellen und entpackte Räume gegen `rooms.tsv`. Aufrufe mit veralteten Lade-IDs ändern die laufende Szene nicht. Ein neuer Aufruf oder Prozessneustart beginnt erneut am gewählten Raum. GL-Kontextverlust und Bildschirmrotation erhalten Raum, Pose und Kamera, solange die native Session im Prozess weiterläuft.

Original-QTEs und Button-Grafiken benötigen eigene Engine-Änderungen; sie sind hier nicht implementiert. Rechtsstick-Zielen verwendet bereits die belegten Originalkurven, ersetzt aber nicht die vollständige originale Lock-on-/Kameraruntime.
RetroTouch ist absichtlich nicht enthalten. Android-Zurück verlässt den Controller-Test.

## Native Teile

- `archive.cpp`: begrenzter Big-Endian-Archivleser; 5.005 Einträge und 13.075 Suchabfragen gegen das PowerPC-Original geprüft.
- `ports/yz2.cpp`: tatsächlich portierter Raumdaten-Decoder; alle 142 Raumarchive beider Discs bytegenau mit Original-PowerPC-Ausgaben verglichen. Android-Test und Grenzen: `docs/YZ2.md`.
- `ports/rnd.cpp`: tatsächlich portierter Original-RNG, feste Integergrößen, sichere Float-Bitkonvertierung, unveränderte Rechenreihenfolge.
- `texture.cpp`: eigener begrenzter TPL-Basismip-Decoder für I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8, C14X2 und CMPR.
- `input.cpp`: plattformunabhängige Aktionszustände einschließlich Press-/Release-Flanken.
- `scene.cpp`: begrenzte starre Weltgeometrie aus originalen Positionen, Ruhepose und Platzierungsmatrizen.
- `model_skin.cpp`, `animation.cpp`, `ports/motion64`: begrenzte BIN/FCV/SEQ-Adapter und native Original-Kurven-, IK- und Gelenkauswertung.
- `leon_preview.cpp`, `weapon_preview.cpp`, `actor_gl.cpp`: Original-Leon, eigenständiges Waffenskelett und Ganado als drei GPU-Instanzen; begrenzte Bewegungs-/Waffenaktionen und Testmunition.
- `enemy_preview.cpp`, `enemy_spawn.cpp`, `hit_geometry.cpp`, `enemy_encounter.cpp`: originale EM12-Geometrie, ESL-Leser, portierte Trefferprimitiven und begrenzter Gegner-/Schadensadapter.
- `turn_rules.cpp`: originale PAD-Sektoren, feste Bewegungsdrehung und Waffenanlege-Übergang.
- `follow_camera.cpp`: originale lokale Übergangspunkte, begrenzter 30-Hz-Blickversatz und SAT-SEE-Kugelsweep.
- `room_objects.cpp`: ETS-Platzierungen, ETM-Ressourcen und ursprüngliche Texturpaletten für starre Objektposen.
- `ambient_animals.cpp`: sechs originale EM28-Hühner mit zwei Modellvarianten und begrenztem Wait-/Walk-Adapter.
- `environment.cpp`, `environment_gl.cpp`, `world_effects.cpp`: unterstützte Originalhimmel-, Wasser- und weltbezogene EFF-/ANM-Teilpfade.
- `scene_gl.cpp`: GLES-3-Prüfansicht mit Tiefentest und Verfolgerkamera; Modellprüfung mit eigener Orbitkamera; noch ohne vollständige TEV-Materialien.
- `room_flow.cpp`: begrenzte RoomInfo-/AEV-Leser und originale Bereichs-/Richtungstests für den geprüften Türlauf.
- `RoomRepository.java`: geprüfter Raumkatalog und atomarer Cache für Hintergrundladen.
- `android_bridge.cpp`: JNI, animierte Raumansicht und native Raumwechsel. **Kein vollständiger GX-/TEV-Spielrenderer.**
- `GameCubeDisc.java` / `DiscInstaller.java`: unabhängiger, auch auf dem Desktop testbarer Disc-Reader und transaktionaler Import.

## Nächster echter Engine-Meilenstein

Der einzelne Ganado verbindet jetzt Schüsse und frontalen Nahkampf mit Leons Lebenspunkten. Nach dem Gerätetest dieser Kette folgen weitere Gegnerzustände, Szenarioaktivierung und Inventar. Weitere Raumübergänge brauchen Szenarioflags, native Raum-Module und bewegliche Tür-/Leiterobjekte. Die sichtbaren r400-Objekte brauchen ihre Aktionen und Szenarioflags; Tiere benötigen weitere Originalzustände. Vollständige GX-/TEV-Lichter, Sichtbarkeit, gebietsabhängige Effekte und dynamische Objekte bleiben ebenfalls offen.

OS-Scheduler und native Modulregistrierung müssen die Original-Laufzeitsemantik erhalten. REL-Dateien enthalten PowerPC-Code und können nicht mit Android-`dlopen` gestartet werden; wiederkehrende globale Symbolnamen verlangen eine gezielte Modultrennung. Audio, Videos, Memory-Card-Speichern und RetroTouch folgen auf einer überprüften Spielruntime.

Die Analyse und der aktuelle Prüfstand stehen in `docs/PORTING.md` und `docs/VALIDATION.md`.
