# R100 / R120 – Korrekturen 7

Stand: 7. Oktober 2026. Android-Version **0.37.7-dev**, Versionscode **40**, `com.ast.re4decomp`, ARM64, Android API21+. Dieses Paket ergänzt **R100-Korrekturen 6**. Den Ordner `RE4-Android-Decomp` aus der Korrektur-ZIP mit dem vorhandenen Projekt zusammenführen und die vollständigen Dateien ersetzen.

Die bestätigten Ursachen für fehlenden Nebel bei Vegetation und den Helligkeitssprung im Autoinnenraum sind korrigiert. Der Android-Build und die unten aufgeführten Prüfungen bestehen. Eine vollständige visuelle 1:1-Abnahme aller Frames auf dem Android-Gerät steht noch aus. Die Vergleichsclips haben unterschiedliche Startpunkte und Kompression; die gezeigten GameCube- und Hostbilder sind deshalb keine pixelgenau synchronisierten Abnahmen.

## Korrigierte Quellabweichungen

**Normale Effekte hatten keinen Nebel.** Die beiden Sprite-Renderpfade zeichneten Baum-/Laubflächen ohne GX-Distanznebel, obwohl `EspCommonTrans` und `Esp08_Trans` die Fog-Register des ursprünglichen `LightMgr` verwenden. Das betrifft insbesondere die tatsächlichen r120-Bäume (Typ0, Animation1, Flags `0x700001`) und normale Raum-SST-Effekte. Jetzt erhalten diese Effekte dieselben originalen LIT-Cutwerte und denselben GX-Nebelpfad wie die übrige Landschaft. Die Farbskalierung und Alphaprüfung erfolgen vorher; transparente Laublöcher bleiben transparent. Explizit nebelfreie Spiegel-/Negativ-, Esp18-Blur- und Esp0f-Linsenpfade behalten ihre ursprünglichen Ausnahmen. Normale Bildschirmsprites behalten ihren ursprünglichen Z-Wert.

**Die Lichtbox des Autos wurde mit seiner Animation verschoben.** Im Original aktualisieren `cModel::setPos/setAng` die inverse Lichtboxmatrix. `MotionMove` und `cObj18::move` bewegen anschließend die animierten Teile, ohne diese Matrix neu aufzubauen. Der Port berechnete stattdessen pro Bild eine neue Box am bewegten Wurzelpart. Dadurch gelangte das Auto erst später in den Radius der Cut6-Lichtquelle7; Kopfstützen und Sitzbezüge wurden plötzlich heller. Die Lichtbox wird nun zu den ursprünglichen Positionsbefehlen gespeichert. Der diffuse Lichtbezug bleibt am aktuell animierten Wurzelpart.

**Beim Schnitt fehlte `MotionClear(model,1)`.** `Event::MotClear` und `ExePacket_Mot` setzen im Original die Teile auf ihre Bindpositionen und Nullrotationen zurück und behalten die durch Rückrufe gesetzten Teilmaßstäbe. Der Port deaktivierte nur seine Bewegungsmarkierung. Die neue Routine verwendet die bereits portierte originale `MotionClear`-Implementierung. Dadurch entsteht die Lichtbox des nächsten Schnitts aus der richtigen Ruhepose. Die ausgeblendeten Autoteile bleiben ausgeblendet.

**Angehängte Ereignismodelle verwendeten die falsche Lichtform.** `ExePacket_Pos` mit Flag `0x40000000` setzt `LightInfo.Flag=1`. Diese Modelle verwenden jetzt die originale Kugelprüfung mit live animiertem Mittelpunkt und unverändertem `Size.x`. `LightSetModel` erhält außerdem `max(Size.x,Size.y)` statt der Boxdiagonale und verwendet die zur Lichtform gehörende Randabschwächung. Gemeinsame Modellteile übernehmen weiterhin die Lichtinformation ihres Besitzers. Diese Auswahl gilt auch für den Spiegelpass.

**Die Filmfläche enthielt den ursprünglich verdeckten Bildbereich.** `Draw_cinesco` verdeckt im nativen 512×448-Bild die Bereiche außerhalb der Zeilen56..393. Für die gewünschte bildschirmfüllende Darstellung wird jetzt diese aktive Fläche auf den Bildschirm abgebildet. X-Kamera, Tiefenwerte und Fog-Projektion bleiben erhalten. Die ursprünglichen Texturgradienten beziehen sich dabei auf die aktive native Höhe337. Der Spiegel-RTT behält seine eigene unbeschnittene Originalprojektion und 592×512-Fläche.

## Szenen aus den bereitgestellten Paaren

| Szene | Ergebnis dieses Stands |
| --- | --- |
| auto-interieur | Konkrete Lichtbox-/MotClear-Ursache korrigiert. Cut6 geprüft bei Frame30/90/150/210; Lichtquelle7 bleibt zugeordnet. [Vergleich](render-vergleich-k7/auto-interieur-korrektur.png), [Zeitpunkte](render-vergleich-k7/auto-interieur-zeitpunkte.png). |
| autofahrt-beleuchtung | Die Landschaftsbox-/Lichtkanalkorrekturen aus K6 bleiben erhalten. Der bislang fehlende Sprite-Nebel ist ergänzt. Die endgültige Helligkeitsabnahme aller vorbeifahrenden Stämme bleibt visuell zu prüfen. |
| autofahrt | Das kontrastreiche entfernte Laub durchläuft jetzt den ursprünglichen Distanznebel. [Isolierter Nebelvergleich ohne neue Bildbeschneidung](render-vergleich-k7/autofahrt-nebel-vorher-nachher.png), [neues Gesamtbild](render-vergleich-k7/autofahrt-k7.png). |
| entfernte-objekte1 | Normale Raum-Sprites erhalten ebenfalls Nebel. Originale BLK-Sichtbarkeit aus K6 bleibt aktiv. [Neues r100-Einstiegsbild](render-vergleich-k7/r100-einstieg-k7.png); der Standpunkt entspricht nicht exakt der markierten Referenz. |
| entfernte-objekte2 | Derselbe korrigierte Raum-Nebelpfad gilt an der Brücke. Eine neue, exakt standpunkt- und ereignisgleiche Brückenaufnahme ist noch nicht als 1:1-Nachweis vorhanden. |
| leon-auto | Originale Trennung von Spiegelklon und Hauptbild aus K6 bleibt erhalten; Lichtform und Positionsgröße sind jetzt zusätzlich korrigiert. [Neues Bild](render-vergleich-k7/leon-auto-k7.png). |
| leon-fenster-entfernte-objekte | Vegetationsnebel ergänzt. Der vollständige natürliche Fenster-/Funkübergang besteht weiterhin ohne Rücksprung der nicht wiederholten FCV. [Letzter Fensterframe](render-vergleich-k7/fenster-letzter-frame-k7.png). |
| leon-spiegelung1 | Gesamte Klonfamilie bleibt im RTT; keine zusätzliche dunkle Klongeometrie im Hauptpass. Die neuen Lichtregeln gelten auch für den RTT. Vollständiger Android-Bildvergleich ausstehend. |
| leon-spiegelung2 | Originalcuts2/6 und Klonanhänge erneut geprüft. [Neue Scheibenszene](render-vergleich-k7/scheibe-k7.png). Der berichtete einzelne Ellenbogen-Grenzframe ist noch nicht unabhängig synchronisiert nachgewiesen. |

## Prüfungen des endgültigen Codes

| Prüfung | Ergebnis |
| --- | --- |
| Android ARM64 `:app:assembleDebug` | Erfolgreich, 3m13s, 37 Aufgaben |
| Native CTest-Grundsuite | 43/43 bestanden |
| Distanz-/Sprite-Nebel, Alpha, LOD, Specular, Filmprojektion und echte Ereignis-Lichtvolumen | **3.642 Prüfungen bestanden**, alle neun r120s00-Cuts |
| Tatsächlicher JNI-/GLES-Spielablauf | **4.102 Prüfungen bestanden**: Einleitung, Hauskampf, vollständige Todessprache, Fenster-/Funkübergang, drei Außengegner, Drops und 240 Schritte nach der Sprengfallenexplosion |
| Tatsächlicher r120-Spiegel-RTT | **107 Prüfungen bestanden**, Originalcuts0/2/6, Anhänge, GL-Zustand, Pause, Kontextneuanlage |
| Lichtkanal/GI/Filter/Schatten | **101 Prüfungen bestanden** |
| Normaler Effektrenderer | **60 GLES3-Prüfungen bestanden** |

Die Bildprüfungen verwenden Mesa EGL/GLES und die bereitgestellten Originalspieldaten. Es wurde kein Android-Gerät installiert oder ferngesteuert. Bestandene Ablauf- und Shaderprüfungen belegen die genannten Korrekturen, ersetzen aber keine vollständige Bildabnahme des ersten Levels.

## APK und Dateien

`RE4-M37-K7.apk` wurde mit dem bereitgestellten Conversion-Zertifikat signiert. Die klassische v1-Signatur verwendet SHA1-Datei-/Sektionsdigests, SHA1withRSA, PKCS#7 ohne signedAttrs und `rsaEncryption`; der RSA-Block liegt unkomprimiert im ZIP. Zusätzlich liegt eine v2-Signatur desselben Zertifikats vor, damit aktuelle Android-Versionen den bestehenden targetSdk35-Build akzeptieren. Alle 19 Dateidigests und 19 SF-Sektionsdigests, die PKCS#7-Signatur, der Fingerprint, beide APK-Signaturverfahren, die ZIP-Struktur und 16-KiB-Bibliotheksausrichtung wurden geprüft. Die klassischen Signaturdateien blieben beim letzten Signaturschritt unverändert; danach wurde die APK ausschließlich gelesen.

Conversion-Zertifikat SHA256: `31ac6bef27dc5dd0f27a4d6463ef8615277b67e2ae5eaa87daf5f3cbb335a6e5`.

APK SHA256: `fc1654cf23198ae0a41c568e0932233d5449a0d2e8ef30b7a36770350502998b`.

Die APK bleibt ein ARM64/API21+-Build; die klassische Signatur allein macht sie nicht auf der 32-Bit-OUYA lauffähig. Originalspieldaten, Signaturschlüssel, Zugangsdaten und lokale Build-Caches sind nicht Bestandteil der Quellpakete. `K7-SHA256SUMS.txt` dokumentiert die vollständigen Dateien der Korrektur-ZIP.
