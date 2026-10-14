# M21 — Messer, Tritt, Zieldrehung und Vollbild

Voraussetzung: vollständig eingespieltes M20-BuildFix-Projekt. Das ZIP enthält ausschließlich vollständige neue/geänderte Dateien unter `RE4-Android-Decomp/`. Paket `com.ast.re4decomp`, ARM64, Android API21 und GLES3 bleiben erhalten. Version: `0.21.0-dev`.

## Einspielen

Alle Dateien ersetzen. Die ZIP-Zeitstempel liegen absichtlich in der Vergangenheit; deshalb die bisherigen nativen Buildausgaben entfernen und vollständig neu bauen:

```powershell
Remove-Item -Recurse -Force app\.cxx, app\build -ErrorAction SilentlyContinue
.\gradlew.bat clean assembleDebug lintDebug
```

Die auf dem Gerät importierten Discs bleiben verwendbar; keine erneute Extraktion erforderlich. Originaldaten und eine APK sind im Patch-ZIP nicht enthalten.

## Ansicht und Bedienung

Die Renderansicht verwendet den ganzen Bildschirm. Die bisherigen Testbuttons und langen Status-/Logtexte erscheinen im Menü. Der kleine Pfeil links oben öffnet dieses Menü; „Fortsetzen“ führt zur vorhandenen Szene zurück. Die Android-Zurücktaste öffnet ebenfalls zunächst das Menü. „Zur Testauswahl“ führt zurück zur vorherigen App-Oberfläche. Im Menü ist die Simulation angehalten und die Eingabe neutralisiert. Diagnose-Texte lassen sich bei Bedarf einschalten. Das kleine Spiel-HUD zeigt HP, Munition und kontextuelle Tür-/Trittaufforderungen über der Renderfläche.

| Funktion | Universeller Controller |
|---|---|
| Bewegen / drehen | Linker Stick |
| Laufen | B / Kreis halten |
| Kamera / Zielrichtung | Rechter Stick |
| Handfeuerwaffe anlegen | LT / L2 halten |
| Schießen | RT / R2 |
| Nachladen | X / Quadrat |
| Messer ziehen / bereit halten | LB / L1 halten |
| Mit Messer schlagen | LB / L1 + RT / R2 |
| Kontextueller Tritt | A / Kreuz bei sichtbarer Trittaufforderung |
| Tritt aus angelegter Waffe | RT / R2 bei sichtbarer Trittaufforderung |
| Pause | Start / Options |

Die zusätzliche horizontale Vorzeichenumkehr im Waffen-Zielpfad ist korrigiert. Physisch rechts am rechten Stick richtet Leon sowohl beim normalen Kameratest als auch beim Zielen nach rechts aus. Messerhaltung folgt derselben Steuerkonvention. Vertikale Achsen und linker Stick werden dabei nicht invertiert.

## Kampf-Test

„Ganado-Nahkampf: r100 / Neustart“ aktiviert weiterhin den ausgewählten unbewaffneten originalen ESL-Eintrag 0x25 mit1000HP. Leon beginnt mit1200HP. Das ist eine ausdrücklich gestartete Testbegegnung; es wird nicht der Original-Kampagnenablauf ausgeführt.

M21 ergänzt das originale Messermodell, Ziehen, Bereitschaft, Schlag und Ablegen. Die Trefferstrahlen entstehen aus den tatsächlich gelösten Körper-/Handgelenken. Die ursprüngliche Trefferphase liegt bei den fünf SEQ-Frames6–10; ihre Flags und drei beziehungsweise sieben Strahlen bleiben erhalten. Messer verbraucht keine Munition. Beim normalen Ganado gelten100 Körper- bzw.120 Kopfschaden; der ursprüngliche Messer-Schadensschutz dauert17 Simulationstakte nach einem angenommenen Treffer.

Ein lebender, am Kopf zurücktaumelnder Ganado bietet einen Tritt an, wenn Reichweite, Höhenabstand, Blickwinkel und freie SAT-Verbindung passen. Es gelten die Originalgrenzen:1500 inXZ,700 vertikal,60Grad Blickwinkel. Der Tritt benutzt Leons Originalkurve und die beiden Original-Trefferkugeln. Der Treffer wird beim17.Handleraufruf vor der nächsten Posenberechnung erfasst und im folgenden Gegnertakt verarbeitet. Normale Körper-/Kopftreffer verursachen200/240 Schaden. Der Tritt gibt die ursprüngliche vorübergehende Schadenssperre; ein ausgeschalteter Gegner bleibt liegen.

Der Ganado kann durch den Tritt fallen, eine Bodenpose halten und in der passenden Richtung wieder aufstehen. Lebende und tödliche Treffer am liegenden Gegner verwenden eigene Originalkurven statt einer stehenden Trefferanimation. Die neuen Aktionsereignisse laufen im vorhandenen festen30-Hz-Simulationstakt. Pause, Menü, Fokus-/Controllerwechsel und Kontextwiederherstellung dürfen keine wartenden Schüsse oder Tritte auslösen.

Die Schadensanzeige unterscheidet Schuss, Messer, Tritt und Griffabwehr. Ein noch wartender Treffer wird getrennt gekennzeichnet; eine Griffabwehr übernimmt keine veraltete Kopf-/Kritisch-Markierung mehr.

## Was noch zum ersten Original-Level fehlt

Dies ist ein zusammenhängender Bewegungs- und Kampf-Prototyp mit einem Ganado. Ein regulär spielbarer erster RE4-Level benötigt insbesondere Original-Raumereignisse und Gegneraktivierung, mehrere Gegner einschließlich ihrer bewaffneten Angriffe, Gegenstände/Inventar und Fortschrittszustand. Audio, Kampagnenspeichern, Ashley, Parasiten/Kopfverlust und RetroTouch fehlen weiterhin.

Die direkte Gegnerannäherung, Testkamera und statische SAT-Auflösung bleiben begrenzte Adapter. Nicht-Kopf-Treffer behalten ihre originalen Trefferzonen und Schadenswerte; ihre stehenden Reaktionen verwenden die originale vordere/hintere Torsoauswahl. Spezielle Arm-, Knie- und Beinstates sind noch nicht angeschlossen. Die Fallbewegung hält die Original-Rootkurve über dem SAT-Boden; der separate Sturz über Geländeabgründe, Wasser und dynamische Raumkollision sind noch nicht portiert. Die Kamera führt keinen Original-Tritt-Kameraschnitt aus. Die Aktionsauswahl ersetzt nicht die gesamte cPlayer-/cEm10-Zustandsmaschine.

Nach dieser Stufe ist eine quellbasierte kleine Raumsequenz mit regulärer Gegneraktivierung, Gegenständen und klarer Zustandsverwaltung sinnvoll. Erst deren erfolgreiche Prüfung rechtfertigt die Bezeichnung „erster spielbarer Original-Level“.

## Gerätetest

1. Ganado-Test öffnen: Die Renderansicht sollte den Bildschirm füllen; lange Diagnosetexte sind standardmäßig verborgen.
2. LT/L2 halten und RS nach rechts/links bewegen: Körper und Zielrichtung müssen sich zur richtigen Seite drehen.
3. LB/L1 halten und RT/R2 betätigen: Messer wird sichtbar, Leon schlägt, Munition bleibt erhalten. Aus großer Entfernung dürfen keine Treffer entstehen.
4. Einen Kopftreffer aus kurzer Entfernung setzen. Bei Trittaufforderung A/Kreuz drücken; der Ganado fällt und steht bei verbleibenden HP wieder auf.
5. Messer gegen den liegenden Gegner testen. Er darf dadurch nicht unmittelbar in eine stehende Kopftrefferpose wechseln.
6. Pfeil öffnen, im Menü warten und fortsetzen: Position, HP und Aktionen dürfen in der Zwischenzeit nicht fortschreiten. Diagnose bei Bedarf einschalten. Android-Zurück ebenfalls prüfen.
7. App in den Hintergrund wechseln und Controller trennen/wiederverbinden; gehaltene Angriffstasten dürfen beim Wiederaufnehmen keinen alten Angriff auslösen.

Prüfungen und tatsächliche Grenzen dieses Builds stehen im aktuellen M21-Abschnitt von `VALIDATION.md`. Für einen Folgepatch Logcat mit `RE4Player`, `RE4Enemy`, `RE4Weapon`, `RE4Flow`, `RE4Actor` und `RE4Scene` schicken.
