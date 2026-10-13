# Projekt: Resident Evil 4 (GameCube-Debug G4BE08) → Android

## Zusammenarbeit

- Der Projektinhaber programmiert nicht selbst. Antworten auf Deutsch, per Du,
  ohne Fachjargon. Fachbegriffe kurz erklären.
- Am Ende jeder Aufgabe in wenigen Sätzen sagen: was erreicht wurde, was getestet
  wurde (mit Ergebnis), was als Nächstes kommt.
- Vor jeder Grundsatzentscheidung (Architektur, Regeln dieser Datei ändern)
  nachfragen. Kleine technische Entscheidungen selbst treffen.
- Am Ende jedes Meilensteins den fertigen Startprompt für den nächsten
  Meilenstein als Codeblock zum Kopieren ausgeben. Er nennt den Branch, die
  empfohlene Intelligenzstufe und ob die Arbeit in der Cloud oder lokal auf dem
  Windows-PC (mit Retroid/Disc-Daten) laufen muss.

## Ziel und Strategie (verbindlich)

Ziel: das komplette Originalspiel nativ auf Android, mit dem Originalcode aus `src/`.

**Endprodukt:** ein Android-Studio-Projekt (Android Studio 2025, Gradle/AGP 8.13),
das der Projektinhaber öffnet, baut und auf den Retroid installiert. Grundlage ist
`port/android/`; der Ganzprogramm-Build (`port/whole/`) wird dort eingebunden.

**Ganzprogramm-Ansatz:** Der gesamte Spielcode (`src/` außer `src/lib/`) wird
auf einmal kompiliert. Ersetzt wird nur die Plattformschicht
(GX, VI, OS, DVD, PAD, AX, CARD, ARAM, CRI-Video). Nicht mehr Funktion für
Funktion extrahieren. Die Generatoren und Fingerprint-Manifeste in
`port/tools/generate_*.py` und `port/config/*-sources.json` werden nicht
erweitert. Sie werden abgelöst, sobald der Ganzprogramm-Build sie abdeckt.

**ARM32 zuerst:** Erst `armeabi-v7a` spielbar machen, danach ARM64.
Begründung und Zahlen: `port/docs/whole-program-probe.md`
(ARM32: 133 Fehler, ARM64: zusätzlich ~2900 Zeiger-Kürzungen).

**Stubs sind erlaubt und erwünscht:** Fehlende Plattformfunktionen werden
automatisch als Stub erzeugt, der beim ersten Aufruf einmal ins Log schreibt.
Dann ausführen, Absturzstelle finden, beheben, wiederholen.

## Regeln

- `src/` und `include/` nicht verändern. Der GameCube-Build (`ninja`) muss
  weiterhin die SHA1-Prüfsummen treffen. Anpassungen für den Port laufen als
  mechanische Umschreibungen beim Bauen (`port/tools/whole_prepare.py`),
  als Ersatz-Header (`port/whole/include/`) oder als kleine geprüfte Patches
  (`port/whole/patches/`). Jede neue Regel und jeder Patch wird in
  `port/docs/whole-program-status.md` eingetragen.
- Port-Code liegt in `port/`. Bewährte Teile weiterverwenden: Scheduler
  (`port/src/scheduler.cpp`, `stack_context.cpp`), Datei-I/O, Yz2,
  Vulkan-Renderer, Android-App (`port/android/`), ADB-Skripte.
- Disc-Daten sind big-endian: beim Laden umwandeln, nie stillschweigend annehmen.
- Keine Spieldateien und keine Disc-Images ins Repository.

## Testen

- **Cloud (Linux):** 32-bit-Host-Build mit `-m32` (gleiche Zeigerbreite wie
  ARM32). Die Compiler installiert das Startskript
  `.claude/hooks/session-start.sh` automatisch bei jeder Cloud-Sitzung.
  Spiellogik, Laden und Abstürze werden hier geprüft. Disc-Daten gibt es in der
  Cloud nicht. Tests, die sie brauchen, laufen lokal.
- **Lokal (Windows-PC + Retroid Pocket Flip 2 per ADB):** Gerätetests nur über
  die vorhandenen Skripte mit der Sperre aus `port/docs/adb-coordination.md`.
- Ein Meilenstein gilt erst als erreicht, wenn der zugehörige Test wirklich
  gelaufen ist. Nicht Getestetes ausdrücklich so benennen.

## Meilensteine

Fortschritt und Ergebnisse stehen in `port/docs/whole-program-status.md`.
Jede Sitzung liest diese Datei zuerst und aktualisiert sie am Ende.
Bauen und prüfen: `port/tools/whole_build.sh`.

1. ✅ Probelauf Ganzprogramm-Kompilierung (`port/docs/whole-program-probe.md`)
2. ✅ Alle 600 Spieldateien kompilieren für ARM32 und 32-bit-Linux fehlerfrei
3. ✅ Linken: Stub-Generator für alle fehlenden SDK/CRI-Funktionen, lauffähiges
   32-bit-Linux-Programm, jedes REL-Modul als eigene Bibliothek
4. Booten mit echten Disc-Daten bis `gameRoomInit` (vorhandene Datei/Yz2/Scheduler nutzen)
   — Teil A ✅ (Cloud: Plattformschicht ohne Disc-Daten), Teil B offen (lokal mit Disc-Daten)
5. Erster Raum (r100): Spielschleife läuft ohne Absturz, noch ohne Bild
6. Grafik: GX → Vulkan (vorhandenen Renderer anbinden), Titelbild und r100 sichtbar
7. Steuerung (Retroid-Tasten, später Touch)
8. Audio (CRI-ADX-Code aus `src/lib/` möglichst direkt mitkompilieren)
9. Videos (SFD/MPV)
10. Speichern/Laden
11. Android-Feinschliff: Grafikoptionen (`port/docs/graphics-settings.md`), Leistung
12. ARM64: Zeiger-Kürzungen beheben

## Git

Arbeitsbranch nach Vorgabe der Sitzung. Nach jedem abgeschlossenen Schritt
committen und pushen. Commit-Nachrichten auf Englisch, kurz und beschreibend.
