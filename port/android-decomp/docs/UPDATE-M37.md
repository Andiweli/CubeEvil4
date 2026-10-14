# M37 – Originale r100-Abläufe, Nebenereignisse und App-Icon

**0.37.0-dev / VersionCode38**, ARM64, `com.ast.re4decomp`, Android21+, GLES3. Das ZIP über den vollständigen **M36-Stand** entpacken. Es enthält vollständige geänderte und neue Dateien unter `RE4-Android-Decomp/`, keine Diffs. Bestehende Dateien ersetzen, neue ergänzen und anschließend `./gradlew assembleDebug` bauen. Die bereits importierten Originaldiscs und die Filme aus M35 bleiben erforderlich. Lokale SDK-/Signiereinstellungen und Original-Spieldaten werden nicht mitgeliefert.

## Korrekturen und Ergänzungen

| Bereich | Änderung |
| --- | --- |
| START GAME | Die originale zweischichtige Sprache spielt vollständig:226.080 PCM-Frames,4,71Sekunden. Vorbereiten und Abspielen der Originalstreams sind getrennt. |
| Nach dem Film | Die tatsächliche Engine-Folge r120s00→r120s01→r100s40 benutzt Originalmodelle, Bewegungen, Kameras, Effekte und Ton. Natürlicher Ablauf:5.505 Originalframes. Skip setzt die vorgesehenen Szenarioflags. |
| Erstes Haus | Die Originalbefehle steuern die Sichtbarkeit der tatsächlichen FN57. Sichtbar in s03-Cuts0–4 und13–20, verborgen in5–12. Fehlerhafte Kommentare im Original ersetzen nicht die ausführbaren Befehle. |
| Kampf/gelbes Modell | Ein Fehlschuss bleibt ein begrenzter Strahl. Er übergibt keine ungültige Maximaldistanz an Objektaktionen. Fehlerbehandlung erhält die bestehenden Spieler-/Gegnerbesitzer. |
| Dauerhafte Modelle | EVD-Abbau entfernt ausschließlich seine eigenen GPU-Ressourcen. Autos, Polizisten, Tiere und Funkmodelle werden dadurch nicht mehr gelöscht. |
| Vögel | Alle fünf tatsächlichen EM23-Datensätze, Flucht-/Flug-/Todesabläufe, Zielgeometrien, Töne und Originalbeute sind angebunden. |
| Hund/Fallen | Rettbarer EM21-Hund, acht tatsächliche Hund-/Fallen-Einträge, Quellfreigabe nach dem Truckereignis, Rettungsbewegungen, Töne, Fang-/Sprengabläufe und Persistenz. |
| Weitere Ereignisse | Polizistengespräche, Brücke, Haus-Schreck, dynamische Untersuchung des toten Ganado und Schädel-Musikabläufe; wiederholbare Ereignisse bleiben wiederholbar. |
| Funk | Tatsächliche Hunnigan-/Leon-Modelle, Gesichtsmotions, Originalgrafiken, Nachrichten, Stimmen und zwei erreichbare r100-Anrufe; originaler Radiogriff und Rückkehrposition. |
| Frühe Ganados | Sieben tatsächliche Hinterhalt-Datensätze, Original-RTP mit66 Punkten, Quellwahrnehmung, Gruppenreaktionen, frühe Waffen-/Treffer-/Todes-/Schrittsequenzen und Beute. Drei Klippenfiguren behalten ihren separaten HP0-Skriptfall. |
| Türen/Fenster | Quellgetreue r100 DoorCk-/WindowBreakCk-Freigaben, Haus-/Zaunflags, Glasbruch und erneutes Durchsteigen. |
| Leon | Frühe Waffen-Soundaufrufe samt Originalzeitpunkt und Teilposition, Waffenabweichung, Schusszufall, Radiogriff und Augenbewegungen. Teilkorrekturen erhalten Hokan-Blending und Waist-Korrektur. |
| Funkeln | Original-ITA-Effekttypen und dynamische CORE21/2C/2F/31/33-Effekte. Nicht jeder statische Fund funkelt: Effekttyp0 bleibt entsprechend dem Original ohne erzwungenen Glanz. Geld/Spinel verwenden Blau, Kräuter Grün. |
| Raum-/Treffereffekte | Sämtliche236 tatsächlichen SST-Records/17 Bänke/14 EAR-Volumen, drei Cloth-Flächen, Original-ROOM20/0E- und CORE1F/20/11-Treffer. Ganadotreffer erzeugen keinen zusätzlichen falschen Wandtreffer. |
| Schreibmaschine | Tatsächlicher r100-Save-AEV, Originalgrafiken/-texte,20 Slots und validierte64KiB-Kartenbilder. Laden stellt den unterstützten nativen r100/r101-Zustand wieder her; beschädigte Dateien werden vor Besitzerwechsel zurückgewiesen. |
| Zähler/Zeit | Originale Erststrahl-Trefferzählung, Abschüsse und absolute Spielzeit. Ereignisse/Koffer/Ladephasen zählen; explizite Pause und Android-Hintergrund zählen nicht. |
| Raumwechsel | Authored AEV0 führt mit Originalposition/-richtung von r100 nach r101. Kein erfundener Kill- oder Truckflag-Zwang. |
| App-Icon | Das bereitgestellte512×512-JPEG ist unverändert als Ressource eingebunden. Klassische, runde und adaptive Android-Launcher verwenden es. |

## Genauigkeit und verbleibende Arbeit

Referenz ist der eingefrorene Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7` mit den bereitgestellten Disc1-Daten. [R100-SOURCE-INVENTORY-M37.md](R100-SOURCE-INVENTORY-M37.md) inventarisiert wirklich alle31 Modulroutinen,40 AEVs,25 raumbezogenen ESL-Einträge,24 ITAs und236 SST-Records einschließlich ihrer Abhängigkeiten.

**Der erste Level ist damit nicht als100% identische Originalengine abgeschlossen.** Noch offen sind insbesondere vollständige dynamische SAT/EAT-Manager, sämtliche EM10-Route-/Hindernis-/Knie-/Gliedmaßen-/Spezialangriffszweige, globale Manager-/Zufallsreihenfolge, vollständige Quellkamera/IK, einzelne Explosiveffektprofile und Wasser-Schusswellen sowie vollständiger globaler Spielstand aller Enginebesitzer. Das64KiB-Format ist quellgeprüft; der unterstützte native Ladeumfang ist r100/r101, kein allgemeiner GameCube-Spielstandimport. GLES-Darstellung verwendet RGBA8 statt des ursprünglichen GX-RGBA6-Pfads und beweist keine pixelidentische Konsolenausgabe.

Grundlegende LIT-Cut0-Beleuchtung und Nebel sind bereits raumübergreifend angebunden, nicht ausschließlich r101/r400. M37 prüft gezielt die neuen r100/r120-Pfade. Es behauptet keinen neuen vollständigen Durchlauf aller Räume oder aller Leon-Aktionen. Weitere Details: [R100-EM12-AUDIT-M37.md](R100-EM12-AUDIT-M37.md), [EVENT-RENDERER-M37.md](EVENT-RENDERER-M37.md), [CAMPAIGN-PATH-FOCUS-M37.md](CAMPAIGN-PATH-FOCUS-M37.md).

Prüfungen und ihre praktischen Grenzen stehen in [VALIDATION-M37.md](VALIDATION-M37.md). Host-/GLES-Prüfungen und Android-Build ersetzen keinen neuen Lauf auf dem tatsächlichen Android-Gerät.
