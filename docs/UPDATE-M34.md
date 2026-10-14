# M34 – Fenster, Waffen, Laden, Wasser und Originalkoffer

VersionCode35 / **0.34.0-dev**, Paket **com.ast.re4decomp**, ARM64, Android5.0/API21, GLES3. Das Änderungs-ZIP wird über den vollständigen **M33-Stand** entpackt. Es enthält vollständige geänderte/neue Quelldateien unter `RE4-Android-Decomp/`, keine Spieldaten, Werkzeugketten, lokalen SDK-Pfade oder Signierschlüssel. Keine Dateien müssen gelöscht werden. HP∞ bleibt einschließlich Trefferreaktionen aktiv.

## Die sieben gemeldeten Punkte

| Punkt | Befund und Änderung |
| --- | --- |
| Bereits zerbrochene Fenster | Das Original nimmt für gleichhohe Durchgänge Richtung0/1 den kurzen Zaunablauf PL_ARC55/56 mit r3=5. Dieser Zweig, seine Aktionsfläche und die Landung am tatsächlichen SAT-Boden sind jetzt angebunden. Glas wird beim erneuten Durchgang nicht neu erzeugt. Die Originalpriorität einer aufgestellten Leiter vor einem nahen Fenster bleibt erhalten. |
| Aufblinken neben dem Scheiterhaufen | Zeit0-Partikel wurden vor dem Setzen der Waffenmatrix an der Identitätsmatrix erzeugt. Die Effektinstanz entsteht jetzt erst mit der gelösten Waffen-Part3-Matrix. Verzögerter Rauch bleibt an seine tatsächliche Elternmatrix gebunden. |
| Schrotflinte legt nach einem Schuss erneut an | Rückstoßende und set40-Nachlauf folgen den ursprünglichen Sequenzframes. Nach Loslassen der Zieltaste geht Leon direkt ins Senken; ein zusätzliches Anlegen entfällt. Die originale Pumpbewegung bleibt erhalten. Die Schrotflinte erhält außerdem ihre eigenen Ziel-/Körperkonstanten. |
| Zwei Pistolenschüsse auf Fenster | Für gewöhnliche ET00-Hausfenster ist dies Originalverhalten:1000HP,500 Schaden pro Pistolenhit. ET25-Schrankglas bricht beim ersten Treffer; Messer und Schrotflinte zerbrechen das Hausglas sofort. Diese Unterschiede bleiben erhalten. |
| Android bietet Warten/Beenden an | Das bereitgestellte Log enthält einen Fokus-ANR. Der schwere Szenenaufbau startet jetzt erst nach dem ersten GL-Bild und bestätigtem Activity-Fokus. Ein fokussierter Ladebildschirm bleibt bedienbar; Abbrechen und Hintergrundwechsel warten auf native Arbeit außerhalb des UI-Threads. Frühe HUD-/Audio-/Menüaufrufe greifen nicht mehr synchron in den laufenden Aufbau ein. Der Fix ist gebaut und automatisch geprüft; ein Zielgerätetest steht aus. |
| Wasser in r10a | Die originalen Esp42-/Esp45-Generatoren erhalten eigene bewegliche Wassergitter, Quelltexturen, Normalen und GLES-Darstellung. Die Freigabe folgt SST und den übertragenen ursprünglichen Raumstarts. |
| Fehlender See in r10b | R10bInit startet EST2 ausdrücklich; dieses Wasser fehlt in der automatisch gestarteten SST-Liste. Der explizite Start einschließlich originaler Größe, Randgeometrie, Maske, Farbe und Wellenvorlauf ist jetzt übernommen. Boots-/Bossereignisse sind dadurch noch nicht implementiert. |

Zusätzlich wurden r400s bereits beim Originalraumstart zerbrochenes Fenster, leere ITM-Modellpakete, Mengen aus der vollständigen Item-Tabelle und die globalen/lokalen ITA-Aufnahmeflags korrigiert. Ein in einem anderen Raum bereits eingesammelter gemeinsam markierter Schatz wird nicht erneut angeboten oder gezeichnet. Leere Kisten bleiben zerstörbar. Fehlende optionale Zufallsfund-Modelle werden wie im Original einzeln übersprungen; vorhandene Modelle, Kisten, Zufallsregeln und Aufnahmebereiche bleiben erhalten. Der ursprüngliche Leuchteffekt für modelllose Funde ist noch nicht vollständig übertragen.

## Beleuchtung, Nebel und Shader in allen Räumen

**Die grundlegende LIT-Cut0-Beleuchtung und der Cut0-Nebel sind raumübergreifend angebunden, nicht auf r101/r400 beschränkt. Vollständige Originaldarstellung aller Räume ist damit nicht erreicht.** Beide Discs wurden für die127 ausgewählten eindeutigen Raumarchive geprüft; der Katalog enthält142 Disc-Einträge. Die vier alten unbenutzten Modellformate bleiben gesperrt.123 Archive sind über den Produktions-JNI-/GLES-Weg darstellbar; r30f ist weiterhin nur eine Ansicht ohne begehbaren Start.

Ein gemeinsamer GLES-Shader verarbeitet die übertragenen Material-, Textur-, Licht- und Nebelwerte. Das vollständige GX-/TEV-System, alle projizierten Schatten, dynamischen Kamera-/Licht-Cuts, TexRender-Ziele, Postfilter und geskripteten Raumereignisse fehlen noch. Ein vorhandener LIT-Datensatz oder ein erfolgreich gezeichneter Raum belegt diese Funktionen nicht. Himmelsobjekte und spezielle Objektzustände bleiben teilweise ausdrücklich übertragene Raumprofile.

Die Wasserauswertung umfasst alle127 ausgewählten Archive einschließlich Disc2. Im übertragenen Startzustand sind20 Wassergitter aktiv:18 über globale SST-Einträge sowie die ausdrücklichen Starts in r10b und r31b. Später durch Ereignisse aktivierte, sofort wieder eingefrorene oder nur für TexRender erzeugte Quellenflächen werden nicht willkürlich eingeschaltet. Einzelheiten und offene Rendererpfade stehen in [M34_ENVIRONMENT_PARITY.md](M34_ENVIRONMENT_PARITY.md).

## Alle Leon- und Objektquellen prüfen

[PLAYER-ACTION-COVERAGE-M34.md](PLAYER-ACTION-COVERAGE-M34.md) und die zugehörige JSON inventarisieren91 Quelldateien,1820 aufrufbare Definitionen und54 Dispatchtabellen. Alle21 obersten Bewegungsslots sind aufgeführt.122 Definitionen besitzen ausdrücklich geprüfte Zuordnungen zu **Teiladaptern**;1698 sind konservativ als nicht übertragen oder nicht nachgewiesen markiert. Diese Zahlen sind ein Quelleninventar und keine Fertigstellungsquote.

[M34-INTERACTIONS-AUDIT.md](M34-INTERACTIONS-AUDIT.md) enthält alle104 Etc-Initialisierungen,64 Objektmanager-IDs,21 Szenario-Aktionshandler und sämtliche ETS-/ITA-/AEV-Daten der127 Archive. Die passenden nativen Profile lassen sich für alle127 Archive konstruieren:410 Kisten,62 explosive Fässer,918 voraussetzungsfreie lose Funde,68 gemeinsame Fenster und26 Innentüren. Das ist ein Nachweis der Daten-/Profilkompatibilität, kein vollständiger visueller Kampagnendurchlauf.

Noch fehlende Originalpfade werden nicht durch Ersatzaktionen als fertig ausgegeben. Dazu gehören insbesondere ledge-climb/-descent, Bootssteuerung, Ducken, der gesonderte lange Sprung, Partneraktionen, alle übrigen Waffen-/Tuningklassen, vollständige Boden-IK, die allgemeine Ereignissteuerung, Truhen, bewegte Fässer und sämtliche gegner-/ereignisgebundenen Fundabläufe. Eine vollständige 1:1-Portierung dieser Systeme ist weitere Enginearbeit.

## Originalkoffer

Die Ansicht verwendet originale Reiter, Peseta-Ziffern, Rahmen, Namens-/Hinweisleisten, alle vier Koffermodelle und Leons Originalkörper mit der ausgerüsteten Waffenpose. ID-Pfadendpunkte, aktivierte Masken, ursprüngliche Ebenenreihenfolge und additive Auswahl wurden korrigiert. Disc-Zugriff und Modellbilder entstehen außerhalb der Canvas-/UI-Zeichenfunktion; Originalmotion für das Portrait läuft auf dem GL-Thread.

Bewegen, Drehen, Ausrüsten, Kombinieren, Untersuchen und bestätigtes Wegwerfen bleiben verfügbar. Die ursprüngliche Kombination einer ausgewählten Munition mit der passenden Waffe funktioniert in beiden Auswahlreihenfolgen. Bei vollem Koffer kann weiterhin Platz geschaffen und zur offenen Aufnahme zurückgekehrt werden.

Universale Bedienung: **Y/Dreieck** nimmt ein Teil zum Verschieben auf und dreht es während des Verschiebens; **X/Quadrat** startet Kombinieren, **A/Kreuz** bestätigt/platziert und **B/Kreis** bricht ab. **LB/L1 und RB/R1** wechseln den Reiter. Der in einer Originalgrafik fest eingebaute GameCube-X-Verschiebehinweis wird ausgeblendet; die Namensleiste und der passende deutsche Bedienhinweis bleiben erhalten.

**Leons Portrait ist noch ein statischer Originalframe.** Vollständige Idle-/Öffnen-/Schließen-/ID-Animationen, bewegliche 3D-Modelle jedes Kofferstücks und sämtliche GX-Specular-Details sind nicht vollständig übertragen. Weitere Untermenüs behalten teilweise ihre bisherige Canvas-Anordnung. [MENU-M34.md](MENU-M34.md) trennt die eingebundenen Originaldaten von diesen Grenzen.

## Installation und Gerätetest

1. Vollständigen M33-Projektstand sichern und das ZIP darüber entpacken. Den bestehenden Discimport beider Discs weiterverwenden.
2. In Android Studio mit JDK17 bauen: `gradlew.bat clean assembleDebug lintDebug` beziehungsweise `./gradlew clean assembleDebug lintDebug`. In der App Version **0.34.0** prüfen.
3. r101: intaktes Fenster durchspringen, Leiter bei Bedarf umwerfen und mehrfach in beide Richtungen durch das zerbrochene Fenster steigen. Bereits zerbrochene Fenster dürfen keinen erneuten Glasbruch auslösen. SAT-Landungen unten/oben prüfen.
4. Neben dem Scheiterhaufen mit Pistole und Schrotflinte schießen. Blitz/Rauch müssen an der Waffe erscheinen. Bei einem einzelnen Schrotflintenschuss Zielen loslassen: Pump-/Rückstoßfolge, danach direktes Senken prüfen.
5. r10a/r10b öffnen, Blickrichtung zum Wasser ändern und sichtbare Wellen prüfen. Pause, Hintergrundwechsel und Fortsetzen dürfen den Wasserzustand nicht neu starten.
6. Mehrere große Räume nacheinander laden; beim Laden abbrechen, App in den Hintergrund schicken und das Gerät drehen. Fokus und Bedienbarkeit prüfen. Bei einem neuen Warten/Beenden-Dialog das vollständige Android-/Logcat-Log sichern.
7. Koffer öffnen, Gegenstände verschieben/drehen, Waffen ausrüsten, passende Munition kombinieren und alle Menüreiter durchgehen. Keine verdoppelten Gegenstände nach Abbruch oder Raumwechsel.

Build- und Hostnachweise stehen in [VALIDATION.md](VALIDATION.md). Mesa-/Quellvergleiche ersetzen keinen Android-/Adreno-Gerätelauf. Der vollständige Titel-/Neues-Spiel-/Kampagnenablauf und Speichern bleiben außerhalb dieses Teststands.
