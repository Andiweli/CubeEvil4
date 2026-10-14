# K12 – Fest Englisch

Android-Version **0.37.12-dev**, Versionscode **45**. Grundlage ist K11; LOAD und SAVE wurden vom Nutzer als originalgetreu bestätigt.

Die Android-Laufzeit verwendet fest den originalen englischen Sprachslot 1. Titel, Menüs, Raumtexte und Funkgespräche können über die bisherigen JNI-Sprachargumente keine andere Sprache mehr aktivieren. Die App bietet keine Sprachwahl.

Der Disc-Installer und die eigenen Oberflächen-, Steuerungs-, Status-, Diagnose- und Fehlertexte sind ebenfalls vollständig auf Englisch. Das umfasst die Android-Stringressourcen, die zusätzlichen Java-Texte und die nativen Meldungen einschließlich Import, Raum-/Modellprüfung, Film-/Audioausgabe, Karte, Inventar und Bewegungsanzeigen. Bei der Übersetzung werden alle Formatplatzhalter überprüft; der Programmcode außerhalb der Textliterale bleibt gegenüber der bereits vorgenommenen Englisch-/Importumstellung unverändert. Die originale LOAD-/SAVE-Darstellung und die Inventar-Geometrie werden nicht verändert.

Neue Disc-Imports überspringen auf jeder der beiden bereitgestellten DVDs **19 Dateien mit insgesamt 5.173.920 Bytes**:

- Die 14 separaten japanischen Schriftdateien `font/*_j.fnt`.
- `ss/Cmn/save_j.dat`, `save_g.dat`, `save_f.dat`, `save_s.dat` und `save_i.dat`.

Englische Dateien wie `save_e.dat`, `font/common_p.fnt` und sämtliche gemeinsamen Spielarchive werden übernommen. Endungen anderer Ressourcentypen sind keine Sprachkennung; die englischen `*_f.fcv`-Animationen bleiben beispielsweise enthalten. Falls ein unterstütztes Image separate Verzeichnisse `ss/Jpn`, `ss/Ger`, `ss/Fra`, `ss/Esp`, `ss/Ita` beziehungsweise die entsprechenden `etc`-Ordner enthält, werden diese ebenfalls ausgelassen.

Die gemeinsamen Originalarchive enthalten neben benötigten englischen Daten auch eingebettete ungenutzte Sprachslots. Ihre Originalbytes und Offsettabellen bleiben erhalten. Die Laufzeit liest daraus ausschließlich Englisch. Die unveränderten Originalquellen und Werkzeuge zur Datenprüfung bleiben ebenfalls Bestandteil des Quellprojekts.

Bereits vorhandene Imports bleiben gültig; ein erneuter Disc-Import ist für die englische Laufzeit nicht erforderlich. Die Filterung greift beim nächsten Import. Bereits extrahierte, ungenutzte Sprachdateien werden bei einem regulären Neuimport durch den gefilterten Datenbestand ersetzt.

## Prüfungen

Beide vollständigen Originaldiscs wurden mit dem produktiven Importer neu importiert, gegen den Referenzkatalog geprüft und ihre Ausgabedateien anhand des SHA-256-Manifests überprüft. Die Prüfung kontrolliert die Abwesenheit aller 19 ausgeschlossenen Dateien, die Anwesenheit englischer CARD-/Schrift-/Animationsressourcen, die genaue Dateianzahl sowie Abbruch und Wiederherstellung eines unterbrochenen Imports. Die Titel-/CARD-Ressourcenprüfung wird mit den tatsächlich gefilterten Imports beider Discs ausgeführt. Android-ARM64-Build und APK-Signatur werden geprüft.

Abschließend bestanden **4.482 Disc-Prüfungen**, je **937 Titel-/CARD-Ressourcenprüfungen** und **46 originale Bestätigungs-Geometrieprüfungen** mit den gefilterten Daten beider Discs. Alle **43 nativen CTest-Gruppen**, die Java-Menü-/Steuerungs-/Raumtests, die **79 nativen SAVE-Prüfungen**, **56 Java-SAVE-Prüfungen**, **40 Card-Flow-Prüfungen** sowie **69.382 Helligkeitsprüfungen** bestanden ebenfalls.

Die Textprüfung erfasst **245 Produktionsdateien**: **2.127 Textliterale in 102 Dateien** und **62 Android-Stringressourcen** wurden übersetzt. Die Formatplatzhalter und der Code außerhalb der Textliterale bleiben erhalten. Für die endgültige APK wurden Android-Build-Ausgaben und Projekt-Caches vollständig entfernt; der anschließende ARM64-Build war erfolgreich. Versionscode 45, Version 0.37.12-dev und englische Installer-/Diagnosetexte wurden direkt im Paket überprüft. Die signierte APK enthält dieselben Programmdaten wie dieser frische Build, verwendet das K11-Zertifikat und besteht die V1-/V2-Signatur- und 16-KiB-Ausrichtungsprüfungen. Der maschinenlesbare Bericht steht in [K12-PRUEFUNGEN.json](K12-PRUEFUNGEN.json). Ein Gerätetest von K12 wurde in dieser Umgebung nicht ausgeführt.

## Dateien

`RE4-M37-K12.apk` verwendet dasselbe Signaturzertifikat wie K11 und kann darüber installiert werden. `RE4-Android-Decomp-R100-Korrekturen12.zip` enthält die vollständigen aktuellen Ersatzdateien K5–K12 einschließlich `FILES-K12.sha256`. Den enthaltenen Projektordner mit dem vorhandenen Projekt zusammenführen und die entsprechenden Dateien ersetzen. `RE4-Android-Decomp-K12-Quellstand.zip` enthält alternativ das vollständige Projekt. Signaturgeheimnisse, lokale Build-Dateien und extrahierte DVD-Daten sind nicht enthalten.
