# Portierungsbasis und Grenzen

Upstream: https://github.com/adonis-singh/re4

Fixierter Commit: **9b76ff003081d6ece8b838453fb8310743a972c7**. Kein automatisches Folgen von `master`.
`upstream/re4` ist der vollständige Referenzstand ohne Git-Metadaten, Originaldiscs oder Build-Ausgaben.
Der Code ist eine Rekonstruktion der GameCube-Debugversion, keine bereits portable Engine.

## Konkrete Belege

| Quelle | Befund | Erforderliche Änderung |
|---|---|---|
| `include/types.h`, `include/dolphin/types.h` | `u32/s32` sind `unsigned/signed long` | Auf ARM64 acht Byte; feste Integergrößen, alle Binärlayouts prüfen. |
| `src/game/model.cpp`, u. a. `Relocate` / `RelocateTPL` | Datenoffsets als Zeigerfelder und Casts `(u32)` | On-disc-Layouts mit 32-Bit-Offsets von nativen Laufzeitstrukturen trennen. |
| `src/game/main_mem.cpp` / `read.cpp` | Feste Speicherbereiche `0x80350000`, `0x80578000`, `0x817F4000` | Allokationen/Arenen statt GameCube-Adressen und Vorzeichenprüfungen auf Zeigern. |
| `src/game/read.cpp::GetDataExt` | Big-Endian-Offset- und Tagtabellen, mehrere Vorkommen | In M03 begrenzt portiert; 13.075 Original-Suchabfragen geprüft. M04 liest SMD-Platzierungen und lokale BIN-Vertex-/Blockgrenzen; siehe SMD.md. |
| `include/gx.h`, `dolphin/gx/GXVert.h` | Direkte FIFO-Schreibzugriffe, `0xCC008000` | M05 dekodiert belegte VAT-0-Displaylists; M06 zeigt starre Weltgeometrie; M07 ergänzt lokale Basistexturen/UVs mit GLES 3. Vollständiges GX-/TEV-Materialbackend fehlt noch. |
| `src/game/main_sub.cpp` | `DLL_Link`, `OSLink`, `OSUnlink`, REL-Prolog/Epilog | Native Modulregistrierung, Lebensdauer und Exportnamen; kein Ausführen von PowerPC-REL. |
| `src/game/scheduler.cpp` | Nintendo-Threads und kooperatives Suspend/Wakeup | Scheduling-Semantik auf Android erhalten; Threads nicht beliebig parallelisieren. |
| `src/game/yz2code.cpp`, `yz2asm.cpp` | Kontextlayout, PowerPC-Assembler und spezielle adaptive Dekompression | In M02 portiert; 142 Raumarchive bytegenau mit Originaldecoder verglichen. Danach Raumarchiv-Dateitabelle und native Datenstrukturen. |
| `src/game/pad.cpp` | Register-Pins, GameCube-PAD, `Joy`- und `Key`-Tabellen | Portable Eingabeschicht plus kontextspezifische Spiel-/Menü-/QTE-Anbindung. |
| `src/game/pl_class.cpp` | `keyReload` liest direkt `Joy`; alternative Steuerungsmodi | Aktionsschnittstelle muss auch direkte PAD-Zugriffe ersetzen. |
| `src/game/pl_sub.cpp`, `cam_qfps.cpp` | Getrennte Waffen-/Messerhaltung und Originalkamera | Rechtsstick-Zielen verlangt gezielte Spieler-/Kameraänderungen. |
| `src/game/snd*.cpp`, `src/lib/AX*`, CRI-Code | Konsolenaudio und Video-Decoder | Native PCM-Ausgabe und geprüfte Decoderwege. |
| `src/game/card.cpp` | GameCube Memory Card | Dateien, Speicherstandformat und sichere Commit-Strategie. |

`tools/audit_upstream.py` erzeugt ein statisches Inventar und optional echte ARM64-Syntaxproben.
Das Ergebnis `upstream-audit.json` zählt Suchtreffer; ein Treffer ist ausdrücklich **keine automatisch bestätigte Fehlerstelle**.
Eine erfolgreiche Syntaxprobe wie beim Original-RNG beweist keine richtigen 64-Bit-Datentypen oder Laufzeitfunktion.

## Projektentscheidung

Der vollständige Referenzcode wird erhalten. Nur geprüfte Portierungen kommen nach `app/src/main/cpp/ports` beziehungsweise in eigene Plattformdateien.
Damit bleibt erkennbar, welche Funktionen wirklich für ARM gebaut werden. Der gesamte Referenzbaum ist ausdrücklich außerhalb des CMake-Builds.
Es gibt keinen Emulator, keine PowerPC-JIT und keinen versteckten Dolphin-Start.
GLES 3.0 ist der erste Grafikpfad; dessen Texturvorschau darf nicht mit der Portierung des GX-Renderers verwechselt werden.

## Offizielle Build-/API-Referenzen

- https://developer.android.com/build/releases/agp-8-9-0-release-notes — AGP 8.9.2, Gradle 8.11.1, JDK 17, SDK 35.
- https://developer.android.com/ndk/guides/abis — ARM64 und Little-Endian.
- https://developer.android.com/ndk/guides/stable_apis — GLES 3.0 ab API 18; höhere APIs nicht allein für diese Basis nötig.
- https://developer.android.com/training/data-storage/app-specific — `getExternalFilesDir`.
- https://developer.android.com/games/sdk/game-controller/controller-input — Controller-Tasten und Achsen.

## Lizenzen

`upstream/re4/LICENSE` bleibt unverändert. CC0 betrifft dort Tools, Konfiguration und Dokumentation, nicht den rekonstruierten Spiel-/SDK-/Middleware-Code.
Für den angepassten Original-RNG, Yz2 und die neue native Motion-/IK-/SDK-Mathematik in `ports/motion64` gilt dieselbe Herkunft. Die neu geschriebenen Plattform-/Importer-/Testdateien sind in `LICENSE-PLATFORM.txt` beschrieben.
Keine ISOs/GCMs, Grafik-, Audio- oder Videodateien werden im Quellcodepaket mitgeliefert.

## M16 — Originalanimation als native Teilruntime

`ports/motion64` baut die originale Kurvenauswertung, IK, Hokan und Gelenkmathematik mit getrennten diskbasierten/nativen Strukturen. `generate_motion64.py` hält diese Ableitung reproduzierbar. FCV/SEQ und BIN-Gewichte sind mit den echten Disc-1-Dateien sowie der unabhängigen Upstream-Hostauswertung geprüft; Details in PLAYER-PREVIEW.md. Das liefert eine konkrete Grundlage für die vollständige Spieleranbindung, ersetzt aber weder deren Zustandsmaschine noch Szenario-/Modulruntime.

## M17 — Bewegungs- und Raumteilruntime

`MotionGetSpeed` liefert den originalen Weg genau einmal pro 30-Hz-Schritt; der Actor behält eine lokale Pose, und nur die eigene SAT-Kapsel übernimmt die Weltplatzierung. Sieben geprüfte WEP01-Clips liefern die Bewegung einschließlich Rückwärts-/Dreh-/Quickturn-Zuständen. `room_flow.cpp` liest 32-Bit-Disc-Offsets byteweise, statt GameCube-Pointer zu relokieren, und reproduziert die belegten AEV-Bereichstests. Die erste Türanbindung ist ausdrücklich auf die zwei nicht durch Skriptcallback überschriebenen Türen r300/r301 begrenzt.

Das Laden verifiziert Originaldaten auf einem Worker. Eine tokenisierte CPU-Nachricht wird ausschließlich auf dem GL-Renderthread übernommen; Daten-/Ziel-/GPU-Fehler rollen den Raumwechsel zurück. Das ist eine geprüfte Android-Ladebasis, kein Ersatz für den vollständigen kooperativen Originalscheduler oder dessen native Modulregistrierung. Details: `ROOM-FLOW.md` und `UPDATE-M17.md`.

## M18 — Waffen- und Aktionsbasis

`weapon_preview.cpp` lädt das normale originale WEP01-BIN/TPL und sechs eigene Waffen-FCVs. Handbefestigung erfolgt über die rohe Modellmatrix von Leon-Teil 10, anschließend die eigene Viergelenk-Skinningpalette. Der native Animationsadapter unterstützt optionales SEQ, originale cMot3-Mehrfachbewegung, Hokan und cPlBody-Hüftrotation. Der begrenzte `LeonPreview`-Aktionsadapter verbindet originale Anlege-/Ziel-/Rückstoß-/Nachladekurven mit 30-Hz-Eingaben und ausdrücklich bezeichnetem Testmunitionsbestand. Modellinspektion, Pause, Raumtransfers und GL-Wiederaufbau erhalten diesen Bestand; Grafikfehler rollen auch die Waffenressourcen zurück.

Die Android-Belegung ist LT/L2 anlegen, RS zielen, RT/R2 Schussanimation, X/Quadrat nachladen. Ursprüngliche GC-Hauptstick-Zielwerte werden auf den rechten Stick umgesetzt, inklusive axialem/diagonalem Clamp und Anlauf. Das ist noch nicht der komplette `cPlayer`, `ItemMgr`, `cObjFn57` oder Kampfruntime: Zielkamera und SAT-Strahl sind eigene Diagnoseadapter, Treffer/Schaden/EAT/Gegner/Effekte fehlen. Primärquellen und Prüfbefunde stehen in `WEAPON-ACTIONS.md`, `UPDATE-M18.md` und `VALIDATION.md`.

## M19 — originale Treffergeometrie und einzelner Ganado

`enemy_preview` verwendet originale EM12-BIN/TPL und ausgewählte FCV/SEQ, einschließlich nativer Root-Motion, Hokan und Spiegelung. `enemy_spawn` liest das separate Big-Endian-ESL mit 255 Datensätzen. `hit_geometry` überträgt Originalkapsel-/Box-/Kugelabfragen mit unabhängiger SDK-/Upstream-Referenz. Dessen Originalableitung ist von der MIT-Lizenz der neuen Integrationsdateien ausgenommen.

Die EAT-Daten werden unabhängig vom Bewegungs-SAT besessen und mit den originalen SEck1/Flag0/Maske400000-Ausschlussregeln abgefragt. Das statische Dreieck-BVH bleibt der bisherige eigene native Geometrieadapter; ein vollständiger `EatMgr` mit dynamischen Szenarioobjekten ist damit nicht portiert. Der tatsächliche RT-Schuss benutzt die rohe Handmatrix der Rückstoßpose mit Originalmündung und Streuung. Der Einzelgegner erhält die originalen typabhängigen FN57-Ausgangsschäden und die belegte verzögerte Schadensverarbeitung im gemeinsamen 30-Hz-Takt.

Originalgegner- und Spielerposition bleiben erhalten; Aktivierung, Blickrichtung, direkte Annäherung, Stoppkontakt und Auswahl einzelner Front-/Todesreaktionen sind ausdrücklich eigene begrenzte Testregeln. Kein `cEm10`-Kampagnenstart; Gegenstände, Parasiten, Audio und Speichern fehlen. Quellen, RNG- und Zustandsgrenzen: `ENEMY-ENCOUNTER.md`, `UPDATE-M19.md`.

## M20 — Frontgriff, Spielerleben und Gegenwehr

Der ausgewählte frontale Catch-/NeckHang-Zweig verwendet EM12-Griffkurven für beide Figuren. Originale Fangbox, Rang-5-HP/RNG, Eingabegruppen und Typ-2-Ausrichtung werden getrennt vom begrenzten Annäherungs-/SAT-Adapter übertragen. Fehlende Gegenwehr führt über Leons Niederwerfen zur PL00-Aufstehkurve; erfolgreiche Gegenwehr führt über den Ganado-Abwurf zum Bodenwarten und Aufstehen. Die originale Gegner-vor-Spieler-Reihenfolge und der verzögerte Release-Start laufen im gemeinsamen 30-Hz-Takt. Getrennte Handmodelle werden im vorhandenen Gegner-VBO ausgewählt.

Die horizontale RS-Achse wird einmal im InputMapper für Kamera und Zielen umgekehrt. Gesundheit, Phase und Eingabepuffer besitzen explizite Pause-/Kontextregeln; ein neuer Ganado-Test bereitet Leon und Gegner transaktional vor und übernimmt Munition. Kein Kampagnen-Scheduler oder vollständiger Spieler-/Gegnermanager: Originaleffekte, Audio, Headloss, weitere Angriffe und Szenariosteuerung fehlen. Quellen, Geräteprüfung und aktuelle Validierungsgrenzen: `GRAB-COMBAT.md`, `UPDATE-M20.md`, `VALIDATION.md`.
