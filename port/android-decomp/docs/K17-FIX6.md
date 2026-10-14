# K17 Fix6 – Austauschdateien, keine APK

Version 0.37.17-fix6, VersionCode 55. Das Paket enthält vollständige geänderte Dateien relativ zum hochgeladenen K17, einschließlich Fix1–Fix5. Den Ordner `RE4-Android-Decomp` über das vorhandene Projekt kopieren. Die Originaldateien unter `upstream/` sind unverändert. Spielassets und Binärdateien sind nicht enthalten.

## Rifle: Repetieren und Nachladen getrennt korrigiert

Das neue Log enthält WEP09-Schüsse mit Reservemunition 0. Der dabei sichtbare Repetiervorgang war in Fix5 nicht vollständig: Leon spielte `pl_rifle.cpp::wep09_r3_fire20` / Spieler-ARC1B, aber die Waffe behielt ihre Idle-FCV. Die Hand bewegt sich in dieser FCV stark; `wep09/objSniper.cpp::moveFire` / Waffen-ARC21 gleicht diese Bewegung im Original aus. Ohne die Waffen-FCV drehte sich das gesamte Gewehr mit dem Handgelenk.

Fix6 startet Spieler-ARC1B mit originalem Hokan 3/Flags 5 und Waffen-ARC21 gemeinsam bei Frame 0. Beide werden synchron ausgewertet. Nach dem Repetieren folgt wieder die Idle-Pose. Beim Loslassen ab Frame 25 wird der originale Reload-Absenkpfad Spieler-ARC1C/Waffen-ARC24 verwendet. Die zuvor getrennt korrigierte Magazin-Nachladung über Spieler-ARC17/Waffen-ARC22 bleibt erhalten. Base-Rifle und Zubehörvarianten sind abgedeckt; die Semi-Auto Rifle erhält keine Bolt-Repetieranimation.

## Inventar: Originaltakt und Darstellung

Die Spielwelt bleibt pausiert. Die Original-FCV läuft weiterhin mit 30 Hz; die Darstellung interpoliert die bereits gelösten Posen per Quaternion zwischen diesen Schritten. Körper, Kleidung, Hände und Waffenbefestigung verwenden dieselbe Phase. Zeichnen und Interpolation verändern weder die FCV-Historie noch Welt-/Gegnerzeit. Ein kurz verzögertes Bild holt mehrere Originalschritte nach, statt die Atemanimation zu verlangsamen.

Fix5 las pro Animationsbild die gesamte physische Bildschirmauflösung zurück. Bei 1920×1080 waren das über 8 MB Rohpixel je Bild, gefolgt von JNI-Konvertierung, Bitmap-Übertragung und asynchroner Anzeige. Fix6 rendert das Portrait mit der originalen 640×480-Kamera: 1,23 MB Rohpixel, also 85,2 % weniger Pixel bei 1080p. Android skaliert das Portrait wie die übrige Original-UI. Die Inventar-Neuzeichnung wird an Androids Displaytakt gekoppelt (`postInvalidateOnAnimation`), und Portraitabrufe haben keine zusätzliche unabhängige 33-ms-Grenze. Weiterhin ist höchstens ein Portraitabruf gleichzeitig aktiv.

## Heugabel-Ganado: Körperkontakt und Treffer

Die ursprünglichen sechs `em10_R1_SukiAtk`-Prüfpunkte (-1000, -700, -400, -100, 300, 600), Angriffsradius 250 und aktive SEQ-Trefferfenster stimmen mit dem Original überein. Der fehlende Gegenkontakt war das Problem: Leon bekam Ganados als Hindernisse, die Ganados bekamen jedoch keinen Leon-Körper. Zusätzlich benutzten Dorf-Angriffsanimationen nicht die vorhandene Ganado-Körperkollisionsauflösung.

Fix6 bindet Leon in Gruppen- und Einzelbegegnungen als Körperkontakt ein. Der horizontale Originalradius ist 200 (`player.cpp::atari.init`, `m_radius2`); beim Ganado ist er250. Die Dorf-Körperauflösung gilt auch bei Heugabel-/Axtangriffen. Ein bereits bestehender Kontakt wird nach Originalprinzip aufgelöst, statt den Gegner durch Leon weiterzubewegen. Gepaarte Griffsequenzen, liegende Gegner und Leons Leiter-/Sprungabläufe bleiben ausgenommen.

## Verifikation und Grenzen

Die neuen Quelltests vergleichen die komplette Bolt-Waffenpose gegen eine unabhängig geladene Original-ARC21. Die ehemalige Idle-Pose reproduziert die fehlende Kompensation. Alle 38 Inventar-Waffenprofile werden auf Zwischenposen ohne Zeitverbrauch geprüft. Acht stationäre Heugabel-Szenarien mit beiden Spiegelorientierungen und Abständen 900/1200/1600/1900 ergeben jeweils drei Angriffe und drei Treffer in 360 Originalticks; kein Durchlaufen durch Leon.

Die Produktions-JNI/GLES-Tests prüfen sichtbare Zwischenbilder, Originalbeleuchtung, unabhängige Inventar-/Weltzeit, Hintergrundpause, Wiederaufbau nach EGL-Verlust und die feste Portraitauflösung auch bei 1920×1080. Weitere Ergebnisse und Kompilierungsnachweise stehen in `K17-FIX6-PRUEFUNGEN.json` und `K17-FIX6-TESTAUSGABEN.txt`.

Geprüft wurde mit den bereitgestellten Originaldaten unter Linux/EGL/GLES und durch Java-/ARM64-Kompilierung. Ein physischer Android-/Adreno-/Controller-Test wurde hier nicht durchgeführt. Das Paket enthält keine APK. Den Gerätetest bitte insbesondere mit wiederholtem Rifle-Schuss, vollständigem Magazin-Nachladen, länger geöffnetem Inventar und stehender/ausweichender Figur vor einem Heugabel-Ganado durchführen.
