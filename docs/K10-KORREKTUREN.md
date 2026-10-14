# K10 – Titel, SAVE/LOAD, Spinell, Health und Inventar

Nachtrag K11: Die K10-Korrektur von LOAD/SAVE war unvollständig. Die hier beschriebenen 15 Bilder gehörten zu einer fremden CORE-Effekttextur, die irrtümlich in den CARD-Satz übernommen worden war. Der CARD-Kopf selbst besitzt nur ein Bild. Ursache, Korrektur und der verschärfte Regressionstest sind in [K11-LOAD-SAVE-UND-SPRACHEN.md](K11-LOAD-SAVE-UND-SPRACHEN.md) dokumentiert.

Stand: 7. Oktober 2026. Android-Version **0.37.10-dev**, Versionscode **43**. Grundlage ist der vollständige K9-Quellstand. Verglichen wurden das bereitgestellte LOAD-Video, beide Screenshots, die eingefrorenen Originalquellen und die tatsächlichen Disc-Ressourcen.

## Die sechs Korrekturen

1. **Intro überspringen:** START, A oder X führen weiterhin unmittelbar zum Titelmenü. Dabei werden jetzt Titelhintergrund und Menü eingeblendet. Der Skip-Tastendruck bestätigt keinen Menüpunkt; weitere Eingaben starten den Fade nicht erneut. Die Einblendung übersteht eine Wiederherstellung der Activity.
2. **LOAD und SAVE:** `id_sys.cpp::idSysMove04` verwendet die Anzahl der animierten Texturen aus `TexAnm.numTex`. Der Schreibmaschinenkopf besitzt 15 physische Bilder, aber nur ein reguläres Animationsbild. Der Port hatte auch die zusätzlichen Effektbilder nacheinander gezeigt. Beide CARD-Seiten verwenden jetzt die richtige Animationsanzahl. Explizite Ziffern-/Hebelbilder bleiben zugänglich. Der Bildschirm-Fade arbeitet außerdem auf dem tatsächlichen Bildpuffer, damit die ursprünglichen Spezialmischungen korrekt lesen können.
3. **Rotierender Spinell:** Die Gegenstandsansicht berücksichtigt die ursprüngliche Rückseitenunterdrückung, Tiefen-/Mischmodi, Material-/Vertexfarben und getrennten Alphamasken. Ungenutzte Alphawerte einer reinen Farbtextur erzeugen keine Löcher mehr. Höhere Shader-Präzision und modellabhängige Tiefengrenzen stabilisieren nahe Edelsteinflächen. Der GLES-Test verwendet den tatsächlichen Spinell-Pickup aus r100, nicht ein Ersatzmodell.
4. **Anzeige unten links:** Im Spiel wird nur noch FPS angezeigt. Der untere Abstand entspricht dem linken Abstand einschließlich des sicheren Bildschirmbereichs.
5. **Gesundheit unten rechts:** Die ursprünglichen CORE-Grafiken, Leon-/Ashley-Segmente, Farben, Lebensschwellen, Munition und Namensgrafiken werden ausgewertet. Die Wurzel bleibt wie bei `LifeMeter::disp(1)` im sichtbaren Zustand; ihre zuvor weiterlaufende Austrittsanimation hatte die Anzeige aus dem Bild geschoben. Die Darstellung erhält ihre Proportionen und folgt dem sicheren rechten/unteren Rand. Im Inventar laufen Heilung und Farbübergänge weiter mit 30 Hz, während Spieler und Gegner pausieren.
6. **Inventar:** Der gemeinsame Arbeitsbereich wird mit dem ursprünglichen sichtbaren Ausschnitt Y=60 bis 420 dargestellt. Zeichnung und Touch-Auswahl verwenden dieselbe Transformation. Koffer, Leon-Porträt, Name, Fußzeile und Health-Anzeige liegen in diesem Bereich. Der gemeinsame Rahmen wird entsprechend der Original-OT-Reihenfolge vor Koffer und Porträt gezeichnet. Die vier ursprünglichen Koffergrößen sind 10×6, 11×7, 12×8 und 15×8. Der Auswahlhinweis verwendet die Originalschrift und die zum Port passende Y-Belegung.

## Ashley-Anbindung

Die native Cockpit-Veröffentlichung enthält getrennte Partnerpräsenz, Lebenspunkte, Maximum, Zustandsbits und Spielertyp. Ashley erscheint nur bei tatsächlicher Partner-ID 3; Luis oder ein bloßes Szenarioflag aktivieren ihre Anzeige nicht. Ashleys originale Lebens-/Maximalwerte und der Spielertyp werden im Speicherformat erhalten. Separate Glättung, Lebenssegmente und Zustandszeichen sind implementiert und geprüft.

Die spätere Ashley-Figur und ihre Kampagnenskripte sind damit nicht neu portiert. Ihre zukünftige Actor-Verwaltung muss die vorbereiteten nativen Partnerwerte setzen und bei ihrem Austritt löschen.

## Prüfungen und Grenzen

| Prüfung | Ergebnis |
| --- | --- |
| Android-ARM64-Build mit allen Produktionskorrekturen | Erfolgreich, NDK 28.2.13676358 |
| Native CTest-Vertragstests | 43 von 43 bestanden |
| Startablauf einschließlich Skip und Fade | 1.378 Prüfungen bestanden |
| Originale Titel-/Options-/CARD-/Font-Ressourcen | 693 Prüfungen bestanden |
| Native Original-Speicherformatprüfung | 79 Prüfungen bestanden |
| Produktiver JNI-/GLES-Speicher-/Continue-Ablauf, Partnerdaten und Heilung im pausierten Menü | 337 Prüfungen bestanden |
| Originale Gegenstandsansicht, vier Koffer, Leon-Porträts und Spinell-Drehung | 2.508 Prüfungen bestanden; 58 tatsächliche IDM-Szenen |
| Spinell-Pickup aus r100 | 120 Drehwinkel; wiederholtes Zeichnen stabil; ungenutztes Farbtextur-Alpha ohne Bildänderung |
| APK | Vorgegebenes Zertifikat, v1/SHA1-RSA und v2, 16-KiB-Ausrichtung geprüft |

Zusätzlich wurden die Java-Verträge für Bildschirm-/Touch-Geometrie, Menübedienung, Cockpit/Ashley, CARD-Rückfragen und Speicherverwaltung ausgeführt. Die Java- und GLES-Prüfungen verwenden Originaldaten als externe Testfixtures; diese Daten werden nicht in den Quellpaketen ergänzt.

Die Darstellung wurde mit Originalressourcen auf dem Host-GLES-System geprüft. Die K10-APK wurde hier nicht auf einem Android-Gerät ausgeführt; ein Gerätetest der sechs gemeldeten Punkte bleibt ausstehend. Der vollständige Offline-Lint-Lauf war wegen eines fehlenden gecachten Lint-Artefakts nicht verfügbar. Der erfolgreiche Android-Build und die beschriebenen Tests sind kein Nachweis einer vollständigen 1:1-Portierung des Spiels.

## Pakete und Einbau

`RE4-M37-K10.apk` ist mit demselben Zertifikat wie K9 signiert. `RE4-Android-Decomp-R100-Korrekturen10.zip` enthält die vollständigen aktuellen Ersatzdateien der K5–K10-Korrekturen und `FILES-K10.sha256`; es enthält keine Patchfragmente. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem vorhandenen Projekt zusammenführen und die entsprechenden Dateien ersetzen. Danach synchronisieren und neu bauen oder die signierte APK als Update installieren.

`RE4-Android-Decomp-K10-Quellstand.zip` enthält alternativ das vollständige Projekt. Keystore, Passwörter, lokale SDK-Pfade, Build-Verzeichnisse und extrahierte Disc-Dateien werden nicht mitgeliefert. Ein Update mit dem vorhandenen Zertifikat erhält die App-Daten und Spielstände.
