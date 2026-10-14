# M31 – Raumkatalog, Fenstersprünge und gemeinsame Atmosphäre

**VersionCode32, 0.31.0-dev; ARM64/API21/GLES3; Paket com.ast.re4decomp.** Dieses ZIP enthält vollständige geänderte/neue Dateien über M30 unter `RE4-Android-Decomp/`, keine Spieldaten. Leon behält unendliche Gesundheit; Schadensreaktionen bleiben erhalten.

## Einstieg und Raumauswahl

Raum auswählen und „Gewählten Raum in 3D öffnen“ drücken. Derselbe Arbeiter prüft/dekodiert das vollständige Raumbundle einschließlich der benötigten r100-Blöcke und öffnet anschließend die native Szene. Ein vorbereitender zweiter Knopf entfällt im normalen Einstieg. Import und Grafik-/Controller-/Archivtests bleiben unter „Menü“. Im pausierten Spiel liegen Tests und ausführliche Statistiken unter „Diagnosen und Tests“. Der Pfeilbutton bzw. Start führt ins Pausenmenü, „Zur Raumauswahl“ zurück in den Einstieg. Die normalen HUD-/FPS-Anzeigen bleiben sichtbar. A/Kreuz bestätigt im Einstieg, B/Kreis geht zurück.

Der Katalog dedupliziert 142 geprüfte Disczeilen zu 127 Räumen. Die Markierung folgt den registrierten Init/Main-Paaren aus `src/st1/st1.cpp`, `st2/st2.cpp`, `st3/st3.cpp` und `st4/st4.cpp` des eingefrorenen Upstreams:

| Auswahl | Einordnung |
| --- | --- |
| ★ Hauptkampagne | 108 registrierte Räume der Dorf-, Burg- und Inselabschnitte |
| Zusatzmodi | 14 Stage4-Räume, einschließlich Mercenaries und Assignment Ada |
| Unbenutzte Raumdaten | r313, r314, r322, r323, r324: auf Disc vorhanden, in St3 `setTbl()` nicht registriert |

r313/r314 mit SMD-Version0 und r322/r323 mit Version0x31 sind im Einstieg gesperrt. Sie werden nicht als Kampagnenlevel oder unterstützte Laufkarten ausgegeben. r324 lässt sich als unbenutzter Datensatz öffnen. **r30f ist die Baggerfahrt:** Sein anfänglicher SAT-Teil hat keinen begehbaren Boden; der Original-Task installiert weitere Teile über `AreaSet` und die Fahrzeugroutinen. Diese Laufzeit fehlt noch. Die Auswahl zeigt deshalb „3D ansehen · Fahrzeugszene“ mit Erklärung.

„Erkunden ohne Gegner“ ist der Standard. Nur r400 bietet zusätzlich den vorhandenen Ganado-Kampftest mit einer originalen EM12-Testinstanz. Ein neuer Erkundungsstart übernimmt keinen vorherigen Testgegner. EGL-Wiederherstellung innerhalb derselben Sitzung erhält eine bewusst gewählte Testpopulation. Der Modus mit allen Originalgegnern wird erst angeboten, wenn deren Assetvarianten, Aktivierung, KI und Ereignisse tatsächlich laufen. Es gibt keinen erfundenen universellen Spawner.

## Originalfenster und Fensteraktionen

Die Basisbilder und Alphamasken wurden bereits korrekt aus den Original-TPLs gelesen. ET00/ET25 wurden jedoch mit der starren Standardreihenfolge und Tiefenschreiben gezeichnet. Nach `WindowData.type2=1` kommen sie jetzt in die sortierte transparente Reihenfolge, mit Tiefentest und ohne Tiefenschreiben. Die Originalfarben und Texturen bleiben maßgeblich; Holzfenster bekommen keinen geratenen Glas-Alpha-Wert.

r100 und r101 besitzen eigene `RoomInteractions` und eigene Raum-/ETC-Flags. Bei abgelegter Waffe zeigt ein erreichbares, passend angeblicktes Fenster „A/Kreuz: Durch das Fenster springen“. Das Ereignis hat Vorrang vor einer generischen SAT-Abwärtskante am selben Durchbruch. ET07-Holzplanken müssen entsprechend der Originalregel zuerst zerstört sein. Dekorative Fenster ohne Originalfeld starten keine Aktion.

`src/game/emwindow.cpp::ChkBreakDir/ExeWindowEvent` liefert drei unterschiedliche Originalwege:

| Richtung | FCV aus ETM | Original-Bruchzähler |
| --- | --- | --- |
| Vorwärts | pl00537.fcv | 15 |
| Rückwärts | pl00538.fcv | 16 |
| Sprung nach unten | pl00536.fcv | 13 |

Originalausrichtung, Startposition, `MotionSetCore`-Flags0x201, Rootbewegung und Tonzeitpunkte werden verwendet. Vor Beginn wird die reale Endposition aus den Kurven berechnet und gegen SAT-Boden und verbleibende Objektkörper geprüft. Kein Ersatzboden wird erzeugt. Die Originalanimation bleibt während der Aktion Eigentümer von Bewegung und Waffenanbindung; Schießen, Messer und normales Laufen übernehmen sie nicht. Pause und Grafikverlust ändern weder Frame noch Flags noch Tonzähler. Die Clips werden einmal an den Spieler gebunden, ohne dessen laufende Blend-/Hermite-Historie zu verändern.

Getestet sind sieben echte Wege: drei in r100, vier in r101; alle drei Animationsrichtungen sind vertreten. Der obere r100-Weg landet auf tatsächlichem tieferem Gelände. Weitere Fensterarten, Fensterpartikel, die spezielle Original-Ereigniskamera und das vollständige Hausereignis bleiben offen.

## Waffenlaser

Der bisherige Renderer zeigte den Laser erst im Zustand `Aim`, also nach Ende der Ziehanimation. Die Originalpistole schaltet ihn bereits während des Ziehens ab Frame4 frei (`src/wep01/wep01.cpp`). Diese Freigabe ist jetzt übernommen. Sie schaltet keinen Schuss vorzeitig frei. Waffenmarker, roter CORE-Laser/Zielpunkt, nächste EAT-/Objekt-/Gegnertreffer und die bisherigen Abschirmungsregeln bleiben erhalten.

## Atmosphäre und Räume

r100 hatte bereits mit M30 eigene Lichter, Himmel, Wasser und Nebel. Das neue Nutzerlog bestätigt 20 Startlichter, exponentiellen Nebeltyp5, ein Himmelsobjekt und zwei animierte Wasserflächen; es enthält keinen erkennbaren App-Absturz. Das allgemeine Beleuchtungsbackend liest nun vorhandene LIT in jedem Raum statt nur in r100/r101.

Der Disc-Audit umfasst alle 127 eindeutigen Räume. Er berücksichtigt die tatsächlich vorhandenen LIT-Versionen0x27–0x2c, leere Cut-Offsets und bis zu 128 gespeicherte Lichter pro Cut. Das Original-`cLit::versionUp` aktualisiert alte EnableMask-Felder und veraltete Headerzählungen. Ungültige Parent-Arten werden entsprechend `cLight::calcParent` abgekoppelt. **r120 enthält 26 spätere Cuts mit unbrauchbaren Float-/Richtungsfeldern:** Sie werden als nicht verfügbar gemeldet und niemals mit erfundenen Werten hochgeladen. Sein gültiger Start-Cut bleibt verwendbar. Ein ungültiger Start-Cut wird weiterhin abgelehnt.

123 unterstützte Modellarchive lassen sich im automatisierten Produktions-JNI/GLES-Lauf zeichnen, davon 122 mit geerdetem Spielerstart. Das belegt die gemeinsame Lade-/Renderbasis, **keine vollständige Originaldarstellung oder Kampagne**. Aktuell wird Cut0 verwendet; dynamische Kamera-/Event-Lichter, vollständige GX-/TEV-/Schatten-/Filterregeln und spezielle Raumobjektzustände fehlen teilweise. Himmel-/Wasser-/Effektprofile sind weiterhin nur dort aktiv, wo sie anhand von Originalcode und Daten gebunden sind: bestehende Profile insbesondere r100/r101/r300/r301 sowie die bisherigen r400-Welteffekte. Es werden keine r101-Werte pauschal auf andere Räume kopiert. Audiozugriffe auf separat extrahierte DVD-Dateien sind im Standalone-Katalogtest nicht vorhanden; dieser Lauf ist kein Voll-Audiotest.

## Installation und kurzer Gerätetest

1. Den vollständigen M30-Projektordner sichern und dieses ZIP darüber entpacken. Keine Dateien müssen gelöscht werden; bisherige Disc-Importe bleiben verwendbar.
2. In Android Studio `./gradlew clean assembleDebug lintDebug` bzw. unter Windows `gradlew.bat clean assembleDebug lintDebug`. Anzeige **0.31.0** prüfen. Ein bestehender `.cxx`-Cache kann bei abweichenden alten CMake-Zuständen vorher entfernt werden.
3. Im neuen Einstieg r100 direkt öffnen, ein unteres Fenster von beiden Seiten und das obere Fenster nach unten verwenden. Danach r101 mit durchsichtigem Glas/Fenstersprung prüfen. A/Kreuz ist die angezeigte Aktion; LT/L2 anlegen zeigt den Laser bereits während des Ziehens.
4. Während eines Fenstersprungs Start/Pfeil drücken, fortsetzen und einmal die App kurz in den Hintergrund legen. Pose und Bruchmodell sollen erhalten bleiben. Zur Raumauswahl zurückgehen, einen weiteren Kampagnenraum ohne Gegner öffnen und optional unter Zusatzmodi den r400-Kampftest auswählen.

Die Originaldaten-, Java-, JNI/GLES-, Speicher- und Regressionsergebnisse stehen in [VALIDATION.md](VALIDATION.md). **Kein neuer Gradle-/NDK-/Lint-/Adreno-Gerätelauf** wurde in dieser Umgebung ausgeführt. Die hostseitigen Prüfbilder zeigen tatsächlich gezeichnete Originalfenster, Laser und Landung; sie ersetzen den Gerätetest nicht.

Der nächste Schritt zum vollständigen Spiel bleibt die native EM15-Mehrgegnerlaufzeit mit r101-Aktivierung, Wahrnehmung, Nachschub und Haus-/Glocken-Tasks, danach weitere Waffen, Inventarkoffer, Musik und dauerhaftes Speichern. Stand: [R101-GAMEPLAY-STATUS.md](R101-GAMEPLAY-STATUS.md).
