# M33 – Aktionskameras, Glas, Leitern, Ingame-Menü und Gegnerfeedback

VersionCode34 / **0.33.0-dev**, Paket **com.ast.re4decomp**, ARM64, Android5.0/API21, GLES3. Dieses Änderungs-ZIP wird über den vollständigen **M32+Fix1**-Stand entpackt. Es enthält vollständige geänderte/neue Quelldateien unter `RE4-Android-Decomp/`, keine Spieldaten und keine lokalen Werkzeugpfade. Keine Dateien müssen gelöscht werden. HP∞ bleibt aktiv, einschließlich der bisherigen Trefferreaktionen.

## Aktionen und Kamera

Die Objektleiterkamera folgt nun dem animierten Original-Part0: Augenoffset `(0,300,-1800)`, Zieloffset `(0,300,0)`, Bildwinkel55° aus `obj13.cpp`. Fenster behalten ihre originalen FCV-Kameras; der Absprung übernimmt die tatsächliche Ausgangskamera. Die Darstellung interpoliert Gelenke, Root und Aktionskamera zwischen den Originaltakten. Die Simulation und Ereigniscounter bleiben30Hz. Angeheftete Beleuchtung und Fußschatten verwenden dieselbe dargestellte Pose.

Ein Leiterausstieg durchquert Glas nicht mehr unbemerkt: Vorher-/Nachherposition des animierten Part0 werden gegen die echten Scheibenflächen geprüft. Der Bruch ist sitzungsfest und spielt die vorhandenen Originalsplitter/-töne. Das Messer wird in `GetWepSizeGroup` korrekt als Gruppe2 behandelt und zerstört die unterstützten Scheiben wie im Quellcode.

Umwerfen und Wiederaufstellen sind eigene Aktionen mit den Originalkurven pl00024/pl00025 und obj13-Timings. Modelltransform, solider Leiterkörper und Aktionsangebot ändern sich gemeinsam. Oben wird die Umwerfaktion vor einem gewöhnlichen Absprung angeboten. Die feste Turmleiter bleibt ein eigener Weg.

Zaun-/Hindernisübertritte verwenden die Quellpfade PL_ARC53/54 bzw.55/56. Das Angebot entsteht aus originalen SAT-Flächen mit Bit0x20, Körper-/Schulterprüfung und gültigem Landeboden. Eine beliebige Wand wird nicht zur Sprungstelle. r400 installiert nun dieselben Fensterinteraktionen wie die unterstützten r100/r101-Fenster; sie sind auch im Ganado-Test erreichbar.

## Ingame-Menü und Gegenstandsaufnahme

Y/Dreieck öffnet den **Koffer**, Start/Options oder der kleine Pfeil das Ingame-Menü, View/Select die **Karte**. Steuerkreuz/LS wählt, A/Kreuz bestätigt, B/Kreis geht zurück; LB/RB wechselt die Originalreihenfolge **Schlüssel/Wertgegenstände → Koffer → Karte → Dateien → Beenden**. App-Diagnosen/Raumauswahl sind getrennt über Optionen erreichbar. Alle geöffneten Spielmenüs halten die Spielsimulation an; der Renderer arbeitet für Gegenstandsansichten weiter.

Im Koffer lassen sich vorhandene unterstützte Waffen ausrüsten, Gegenstände verschieben und drehen, kombinieren, untersuchen und nach eigener Ja/Nein-Bestätigung wegwerfen. Y nimmt ein Teil zum Verschieben auf, RB dreht, A setzt es ab. Die Untersuchung zeigt das originale IDM-Modell und TPL mit Quellrotation und CORE-Lichtprofil sowie originalen Specular-TPLs und Materialparametern; der rechte Stick dreht die Untersuchung. Aufnahmen verwenden eine eigene Spielansicht mit rotierendem Originalmodell und Ja/Nein, keinen Android-AlertDialog. Die Quellkamera-Fassung aus F2/F3 wird für das Android-Modellfenster zentriert und um eine Sichtbarkeitsgrenze ergänzt, damit lange Gegenstände beim Drehen nicht abgeschnitten werden. Bei vollem Koffer öffnet **Y / „Koffer ordnen“** den pausierten Koffer; danach kehrt dieselbe noch unbestätigte Aufnahme mit neu geprüftem Platz zurück. Abbruch behält den Fund in der Welt.

Die Karte zeigt die vollständige aktuelle Originalbereichsgeometrie aus den17 Bereichslisten mit113 Raumzuordnungen, einschließlich Grenzräumen. Der aktuelle Raum wird hervorgehoben, Leon über die originalen gekoppelten SAT-Dreiecke projiziert. Besucht-/erledigt-/Türschlosszustände werden ohne ausgeführte Kampagnen-Tasks nicht erfunden.

Die Menügrafiken kommen aus `ss/Eng`/`ss/Cmn`; Kofferteile aus den originalen Puzzlemodellen und deren TPL-Aliaszuordnung. Schlüssel, Schätze und Dateien folgen den Originaltypen. Gesammelte Dateien zeigen Originalseiten einschließlich Glyphen und Bildern aus `font` und `ss/Item`. Englische USA-Discdaten werden nicht durch erfundene deutsche Dokumenttexte ersetzt. Die Bedienhinweise sind deutsch und verwenden universale Tastenbezeichnungen.

Dies ist ein funktionaler Android-Canvas/GLES-Adapter der Quellenabläufe. Die vollständige originale ID-/GX-Menüanimation, Leon-Menüportrait und das originale Überlaufbrett sind noch nicht umgesetzt. Es gibt weiterhin keinen vollständigen Kampagnen-Save, Händlerablauf oder alle Waffen-/Gegenstandsaktionen. Diese Funktionen werden nicht als fertiger Originalport ausgegeben.

## Ganado

Der vorhandene Testgegner verwendet **EM12-Ressourcen mit cEm10-Laufzeit**. Seine ursprüngliche Soundbank8 wird geladen; Treffer-/Schmerz-/Atem-/Kontaktaufträge folgen den übertragenen Quellereignissen. Eigene Stimmkanäle werden beim Ersetzen/Stoppen und Raumwechsel beendet, ihre räumliche Position folgt dem Gegner. Ein gesonderter Audiozufall verändert den bisherigen Kampfzufall nicht.

Sechs Original-EST-Aufträge verwenden22 Quellanimationen für Nah-/Ferntreffer, Messer und Schrotflinte: Esp00/0b-Spritzer, Esp01-Spuren und Esp10-Bodenblut mit SAT-Auflage. Sie haben eigene GPU-Ressourcen und behalten ihren CPU-Zustand bei EGL-Neuaufbau. Die fünf Refraction-/Shimmer-Einträge besitzen noch keinen Backendpfad. Vollständige Dorfpopulation, sämtliche Angriffe, Körperzerteilung und korrekte Mehrpellet-Schrotflintenschäden am Gegner bleiben weitere Enginearbeit.

## Installation und kurzer Gerätetest

1. M32+Fix1 sichern, ZIP darüber entpacken. Den vorhandenen vollständigen Discimport weiterverwenden; für Menü/Feedback sind insbesondere `ss`, `font`, EM12 und CORE nötig.
2. Android Studio2025 mit JDK17: `gradlew.bat clean assembleDebug lintDebug` oder `./gradlew clean assembleDebug lintDebug`. In der App Version **0.33.0** prüfen.
3. Dorf r101: Hausleiter durch intaktes Fenster betreten; Scheibe muss sofort zerbrechen. Leiter umwerfen/wiederaufstellen, Turmabsprung und Zaunaktionen prüfen. Kamera soll Leon während der Aktion zeigen. Scheiben auch mit LB+RT/Messer testen.
4. Schrotflinte aufnehmen; einmal Nein, dann Ja. Y → Ausrüsten; Gegenstand untersuchen, drehen, verschieben und eine zulässige Kombination prüfen. Bei vollem Koffer direkt Platz schaffen und zur selben Aufnahme zurückkehren. Start/View/Koffer schließen und Bewegung sofort wieder aufnehmen.
5. r400-Ganado-Test: Fenster benutzen; Nah-/Ferntreffer und Messer auf Stimme/Blut prüfen. Während einer Aktion oder offenen Aufnahme die App kurz in den Hintergrund schicken; keine verdoppelten Gegenstände oder fortlaufenden Spielaktionen.

Die Host-/Buildnachweise stehen in [VALIDATION.md](VALIDATION.md). Ein Adreno-/Android-Gerätetest dieses neuen Codes wurde hier nicht ausgeführt. Vollständige CameraControl-/dynamische Kamerakollisionen und alle Raum-/Kampagnenskripte sind weiterhin nicht portiert.

Nach der Prüfung dieses Stands folgt **Start-/Intro-/Titelmenü**, danach **Neues Spiel → r100 einschließlich zugehörigem Intro und originalen Gegner-/Raumereignissen**. Diese Abläufe werden nicht bereits durch dieses ZIP freigeschaltet.
