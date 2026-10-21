# GX nachbilden – Bauplan

Entscheidung des Projektinhabers (15.10.2026): Die App zeigt, was das Spiel
selbst über GX zeichnet. Bisher zeichnet die App die Räume und Figuren aus dem
Spielzustand nach (Zustandsbrücke, `re4_core.h`); alles, was das Spiel nur über
GX zeigt – Logos, Titelmenü, Untermenüs, Funk, Inventar, Lebensanzeige,
Untertitel, Effekte, Spiegel, Wasser –, fehlt, und Modelle weichen ab (Auto im
Intro). Fortschritt steht in `whole-program-status.md`.

## Wie die GameCube zeichnet

Das Spiel ruft die GX-Bibliothek des SDK (`src/lib/GX*.c`, liegt dekompiliert
vor). Die Bibliothek schreibt Befehle in den **GX-FIFO** (eine
Schreibadresse, `GXWGFifo` bei `0xCC008000`): Register des Grafikprozessors
(CP: Punktformate, Feld-Adressen; XF: Matrizen, Licht, Texturkoordinaten,
Bildausschnitt; BP: TEV-Stufen, Texturen, Mischen, Tiefe, Nebel, Kopieren)
und Zeichenbefehle mit den Punktdaten. Modelle bringen fertige
**Display-Listen** mit (dieselben Befehle als Datenblock, `GXCallDisplayList`).
Am Ende eines Bildes kopiert `GXCopyDisp` das fertige Bild in den
Bildspeicher, `VISetNextFrameBuffer` zeigt es an. Render-to-Texture
(`GXCopyTex`: Spiegel, Wasser, Schatten, Filter) kopiert in eine Textur.

## Weg im Port

Die GX-Bibliothek wird **unverändert mitkompiliert**; nur ihre
Hardware-Zugriffe werden beim Bauen umgelenkt (`whole_prepare.py`). Ein
**Nachbau des Grafikprozessors** liest den FIFO-Strom und erzeugt daraus
Zeichenaufträge, die der GL-Faden der App mit GLES3 ausführt. Damit gibt es
einen einzigen Weg für alles, was das Spiel zeichnet, und die Registerwerte
stammen aus dem Original-SDK.

```
Spiel ─GX-Aufrufe→ GX-Bibliothek (src/lib) ─FIFO-Bytes→ gx_fifo (Kern-Faden)
   → Befehlsdecoder (CP/XF/BP, Punkte, Display-Listen) → Bildliste
   → (Dreifachpuffer wie der Schnappschuss) → App, GL-Faden: GLES3-Ausgabe
```

Punkt- und Texturdaten werden beim Dekodieren auf dem Kern-Faden kopiert
(das Spiel überschreibt seine Puffer im nächsten Bild); der GL-Faden liest
keinen Spielspeicher.

**Eigener Faden für den Decoder (Schritt B der OUYA-Arbeit, 21.10.2026,
`gx_thread.cpp`, eingeschaltet mit `RE4_GX_THREAD=1`):** die FIFO-Bytes gehen
in einen Ring von 512 KB (der FIFO des Spiels hat 448 KB), ein eigener Faden
dekodiert sie wie der Grafikprozessor neben der CPU der GameCube (Dolphin:
„Dual Core“). Die Aufrufe des Ports zwischen den Bytes (`VISetNextFrameBuffer`,
`VISetBlack`, `VIFlush`, Markierungen, Host-Reihenfolge-Listen) reihen sich
an ihrer Stelle ein. Seine Unterbrechungen (`GXDrawDone`, Draw-Sync) erreichen
das Spiel wie bisher beim Warten und Bildwechsel; warten alle Spiel-Threads,
endet der Leerlauf auch, sobald der Faden eine meldet (`re4_gx_idle_wait`).
Das Spiel wartet nicht bei jedem Bildwechsel auf ihn, so dekodiert er Bild N,
während das Spiel Bild N+1 bewegt; vor dem Lesen von Zustand in Tests holt er
auf (`re4_gx_sync`). Unterschied: der Decoder
liest Spielspeicher etwas später als gesendet, wie die echte Hardware; ein
Spiel, das einen Puffer überschreibt, bevor der Grafikprozessor ihn gelesen
hat, sähe dann die neuen Daten. Gleiche Ausgabe Bit für Bit
(`re4probe_gx_fingerprint`, `whole_build.sh` prüft beide Betriebsarten).

## Schritte

| | Inhalt | Test (Cloud, ohne Disc-Daten) |
|---|---|---|
| G1 ✅ | FIFO einsammeln: Schreibzugriffe auf `GXWGFifo` (Spiel `include/gx.h`, SDK `GXVert.h`, `__gx.h`) werden beim Bauen zu `re4_gx_put_*`; GX-Bibliothek aus `src/lib` im Build, Register-Blöcke (CP/PE/PI/MEM) auf Speicher des Ports, Zeichen-fertig/Token-Unterbrechung des PE nachgebildet; die rund 97 GX-Platzhalter entfallen | GX-Aufrufe erzeugen die erwarteten Befehlsbytes (BP/XF/CP-Schreiben, Zeichenbefehl mit Punkten); `GXDrawDone` kehrt zurück |
| G2 ✅ | Befehlsdecoder (`gx_decode.cpp`): CP (Punktbeschreibung, Formate, Feld-Adressen), XF (Speicher und Register, auch per Index geladen), BP, Zeichenbefehle direkt und indiziert, Display-Listen (auch CPU-gebaute in Host-Reihenfolge, Patch 0032), Texturen mit Inhalts-Schlüssel, Paletten, Bildpuffer-Kopien; Ausgabe als Bildstücke (`re4_gx_frame.h`). Transformation, Licht und Texturkoordinaten-Erzeugung macht der Renderer (G3, Shader) | synthetisches Bild über die SDK-Bibliothek → erwartete Punkte, Indizes, Texel, Paletten, Zustände (`runtime-test gx`) |
| G3 ✅ | GLES3-Ausgabe (App, `gx_gl.cpp`): Shader aus dem TEV-Zustand (bis 16 Stufen, Konstanten, Tauschtabellen, später indirekte Texturen), Licht pro Punkt, Texturkoordinaten-Erzeugung, Texturformate (Leser der App), TLUT, Mischen/Tiefe/Alpha/Nebel/Seite, Kopien in den Bildspeicher und in Texturen | Bildvergleich synthetischer Szenen (Mesa, offscreen): `gx_gl_check` |
| G4 ⏳ | In der App: Kernmodus zeigt das GX-Bild statt der Zustandsbrücke (umschaltbar, Brücke bleibt für Diagnose); PC-Prüfung zeichnet GX-Bilder | lokal: Logos, Titelmenü, r120/r100 wie im Original; Gerätetest |
| G5 | Rest: Bildeffekte (Filter, Unschärfe), `GXPeekZ`, Leistung (Zustands- und Shader-Cache), was die Bilder zeigen | lokal, Gerät |

## Offene Punkte

- Adressen: Der Grafikprozessor arbeitet mit physischen Adressen (MEM1 ab 0).
  Spieldaten liegen im Port bei `0x80000000 + physisch`. Daten außerhalb von
  MEM1 (statische Felder der Bibliotheken) bekommen ein Fenster ab
  `0x03000000` (`re4_gx_physical`, Patch 0031); Display-Listen-Aufrufe tragen
  den Zeiger selbst.
- Zeichen-fertig: `GXDrawDone` wartet auf die PE-Unterbrechung; der Nachbau
  arbeitet die Befehle sofort ab (mit eigenem Faden: holt auf, bevor die
  Unterbrechungen zugestellt werden) und meldet „fertig“ im Unterbrechungsweg
  des Ports (`re4_os_interrupt_*`).
- Bildtakt: das Spiel zeigt mit `VISetNextFrameBuffer`; die App holt die
  Bildliste des zuletzt gezeigten Bildes.
