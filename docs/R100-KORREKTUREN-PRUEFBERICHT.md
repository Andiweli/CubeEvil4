# r100-Korrekturen: Ergebnis und Prüfgrenzen

Stand: 6. Oktober 2026. Grundlage: M37 einschließlich Buildfix, GameCube-Quellstand `9b76ff003081d6ece8b838453fb8310743a972c7` und bereitgestellte Disc1. Paketkennung `com.ast.re4decomp`, Version `0.37.0-dev`/38 und das vorhandene Illuminados-Icon sind erhalten.

Die reproduzierten Render-/Übergabefehler wurden korrigiert. Karte und Beschriftungen wurden überarbeitet; zusätzlich sind Original-HUD und normale Leon-Continue-/Checkpointpfade integriert. Der vollständige Gesamtauftrag ist damit **noch nicht als 100 Prozent originalgetreu abgeschlossen**. Die verbleibenden gemeinsamen Engineabhängigkeiten und die fehlenden Geräte-/Konsolennachweise stehen am Ende dieses Berichts und in `SPIELBEGINN-MATRIX.md`.

## Die acht gemeldeten Punkte

| Nr. | Ergebnis / Ursache | Quellabgleich und Prüfung |
|---|---|---|
| 1 | Die r120-Raumunterdrückung war verkehrt herum: DPF_SCR bedeutet Hintergrund unterdrücken. Bei r120s00 ist nur Cut0 unterdrückt; ab Cut1 ist der Raum sichtbar. EFM-Lichtklassen, Lichtmaske0 und unbeleuchtete Materialien berücksichtigen nun die Originalflags. Die sechs transparenten Autoscheiben werden nach opaken Akteuren ohne Tiefenschreiben gezeichnet und nach Kameratiefe sortiert. | `trans.cpp::objTrans`, r120-Callbacks, `EspgenEfmSet`/`EfmSetObj04`, `EventCarInit`; Original-Introtests und tatsächliche GLES-Framebuffer. Keine frei gewählte Abdunkelung. |
| 2 | Die Kampagnenübergabe zeigt eine schwarze Fläche ohne eigenen Ladeindikator, Text oder Abbrechen-Schaltfläche. Die Ereignisse verwenden ihre vorhandenen Source-Fades. Im Titel wird die Launch-Fläche schwarz, im Controller bleibt der Diagnose-Ladeindikator für ausdrücklich ausgewählte Inspektorpfade erhalten. | Titel-/Raum-Ereignisquellen; r120s00→r120s01→r100s40, Skip/Pause/EGL und Raum-Mailbox nativ ausgeführt. Die Android-View-/MediaCodec-Kette wurde gebaut, aber nicht auf dem Zielgerät abgespielt. |
| 3 | Funkdialog und Quellressourcen bleiben erhalten. Die Rückkehr und das Speichern der gehörten Nachrichten werden weiter durch native Besitzer geführt. | JNI-Titel-/Kampf-/Radio- und Nebenereignisprüfungen erfolgreich. |
| 4 | Zusätzlich zur Hintergrundunterdrückung wurde die unbeleuchtete Materialbehandlung korrigiert: keine diffuse Vertexbeleuchtung bei Source-Maske0 bzw. entsprechendem Scrollmodell-Flag. EFM-Lichtklassen16 behalten die Originalmasken4/8. | Originalflags und LIT-/Materialdaten; Intro-/Kampf-/Beute-Renderer ohne GLES-Fehler. Vollständige GX-/Konsolen-Pixelparität ist nicht belegt. |
| 5 | Die offene Haustür wird nicht künstlich geschlossen. `r100_DoorCk` öffnet und verriegelt ETS2 zunächst mit `setOpenLock(0)`, bis RSF3 erreicht ist. Die nachfolgende Schließ-/Sperrfolge hängt von Raumflag und Ganados ab. | `src/st1/r100.cpp::r100_DoorCk`, `emdoor`, unveränderter M37-Source-Fiberbeleg und neuer Haus-/Ambushpfad. |
| 6 | Das Lastwagenereignis übergibt direkt an Funkphase12. Zwischen Ereignisende und Radiobeginn wird kein normaler Raumframe mit Leon am Fenster mehr veröffentlicht. Die vom Funkgerät vorgeschriebene spätere Positionsrückkehr bleibt bestehen. | `OpeSetOpenTerm` und r100-Todesereignis; tatsächlicher JNI/GLES-Test prüft die unmittelbare Übergabe und anschließende Rückkehr ins spielbare r100. |
| 7 | Aktive und inaktive Tabtitel samt Auswahlgrafik skalieren gemeinsam; bei4:3 bleiben die Source-Maße, breite Displays erhalten mehr Beschriftungsfläche. Die Übersicht verwendet echte BIN-Modelle, TPL-Texturen/UVs, Source-Palette nach ID-Marke, Besuchs-/Szenarioflags, Source-Kamera/-Viewport und den Originalpfeil. SAT wird für die baryzentrische Positionszuordnung benutzt. | 7.434 Java-Menügeometrieprüfungen und16.714 Original-Subscreenprüfungen. Noch offen: alle Map-Tür-/Ziel-/Itemmodelle, vollständiges MapModelLight4 und sämtliche Menü-/Map-Animationen. Kein Gerätetest der Canvas-Darstellung. |
| 8 | Der Flackerpfad wurde reproduziert: nach dem Itemmodell-Zeichnen blieb das Actor-Shaderprogramm aktiv. Anschließend bekamen dessen Uniformpositionen Weltmatrizen; GLES meldete1282/GL_INVALID_OPERATION wegen Matrixgrößen. `SceneGl` bindet vor seinen abschließenden Uniformupdates das Weltprogramm und VAO erneut. Die Effekt-Diagnose nennt jetzt den tatsächlichen Besitzer und unterscheidet bereits anstehende GL-Fehler. | Hausgegner, s20/Radio, Ganados3/4/5, Beute und vier GPU-Neuaufbauten:1.800 Prüfungen ohne GLES-Fehler. Das beigefügte Log zeigt keinen SIGSEGV/Java-Fatal; ein unabhängiger gerätespezifischer Prozessabsturz ist damit nicht bewiesen oder pauschal ausgeschlossen. |

## Zusätzliche gemeinsame Systeme

Das HUD lädt die originalen CORE-ID-Gruppen1F/20/26 mit eigenem Textur-Namensraum. Lebensanzeige und Munition lesen veröffentlichte native Spielwerte. Die Source-Grenzen800/400, maximale Nadelrotation und0,9-Interpolation laufen mit dem nativen30-Hz-Takt, unabhängig von Displayrefresh. Vorhandene Angebote für Waffen-/Raumaktionen, Fenster-Richtung, gebrochene Fenster, Hunde-Rettung, AEV-Nachrichten und die Schreibmaschine zeigen Source-Labels/Buttons. Die alte Textanzeige bleibt im Diagnosemodus. Ein vollständiger Ersatz von `cActionButton` und dessen Original-OT-Verwaltung ist dies noch nicht.

Continue besitzt einen separaten Checkpoint im Speicher. Neue Raumeinstiege speichern die unterstützten Besitzer vor RoomInit; ein Schreibmaschinensave aktualisiert den Checkpoint. Ein geladener Kartenstand bleibt gemäß SYS_LOAD_GAME die Vorlage. `GameContinue(0)` restauriert Inventar, Lebensenergie, Platzierung, Szenario-/Raum-/Gegner-/Radioflags und unterstützte persistente Beute. Aktuelle Spielzeit und Continue-Zähler bleiben erhalten; die drei u16-Zähler werden erhöht, LVADD_DIE senkt den Source-Rang unter Berücksichtigung von Modus und Ländergrenze. Ein normaler Continue-Raumeinstieg aktualisiert den Speichercheckpoint erneut; ein Kartencheckpoint behält seinen Source-Punktwert.

Der normale Leon-Todespfad wartet90 Weltticks. Die Continue-Präsentation benutzt `option.dat`-Gruppen5/6 und den tatsächlichen abgeschlossenen Spiel-Framebuffer, keine Ersatzgrafik.270Ticks bzw. Confirm bis zur Auswahl,15Ticks Eingabesperre, initial YES und177/150Ticks bis Retry/Softreset stammen aus `gameDiedemo`. Die Auswahl benutzt Source-Cues0/6,0/8 und0/5; bestehendes Audio wird über2Sekunden ausgeblendet, der Originalstream0/0 gestartet. PCM-SE-/Sequenzhüllkurven bleiben ein Plattformadapter. Auswahl-/Interpolationstakte pausieren beim Hintergrundwechsel. Die bestätigte Auswahl unterdrückt Spieler-/Objektdarstellung und aktualisiert das Hintergrundbild. Eine misslungene Wiederherstellung erhält den bisherigen Besitzer und lässt einen erneuten Versuch zu.

## Ausgeführte Prüfungen

Prüfzähler messen Assertions, nicht den Anteil des vollständig portierten Spiels.

| Prüfung | Ergebnis |
|---|---|
| Android-arm64 `assembleDebug` / `lintDebug` | Beide erfolgreich;0 Lint-Fehler,33 Warnungen. Kein APK im Korrekturpaket. |
| `tools/test_host.sh` | Alle43 nativen Vertragstests erfolgreich; Java-Verträge einschließlich33 HUD/Continue- und7.434 Menü-Geometrieprüfungen erfolgreich. |
| `m37_opening_event_check` |71.652.856 Prüfungen; r120s00:1970Frames/9Cuts, r120s01:2360Frames/15Cuts, r100s40:1175Frames/8Cuts. Originalressourcen tatsächlich ausgeführt. |
| `m35_title_assets_check` / `subscreen_data_tests` mit Disc1 |691 bzw.16.714 Prüfungen; Original-Titel/Optionen/Save/HUD/Death-Assets,58 Itemmodelle,256 Namen,156 Dokumentseiten sowie17 Gebietslisten/113 Kartenraumzeilen. |
| `m37_startup_audio_check` |82.091 Prüfungen; START GAME:2Layer,226.080 Ausgabe-Frames/4,71s; Quellfade3s. |
| `m37_campaign_path_check` |23 Prüfungen; tatsächlicher AEV0-Ausgang r100→r101. |
| `m37_secondary_event_check` |14.541.503 Prüfungen; komplette Original-Nebenereigniszeitlinien, Kameras und persistente Akteure. |
| `m37_dog_traps_check` / `m37_r100_cliff_check` |285.653 bzw.713 Prüfungen; echte ESL-/EM-/Raumdaten. |
| `m37_trap_effects_check` / `m37_room_cloth_check` / `m37_room_area_effects_check` |62.246 /184 /7.918.830 Prüfungen; Source-Fallen, Cloth9×10×3 und236SST-Records/14EAR-Bereiche einschließlich Originalteich. |
| `m37_entry_runtime_check` / `m37_secondary_runtime_check` |67 /199 tatsächliche JNI/GLES-Prüfungen: Opening/Skip/Raumtransaktion, Nebenereignisse, Spieler/Kamera/MDT, Pause und EGL. |
| `m37_save_runtime_check` / `m37_save_stats_runtime_check` |249 /48 JNI/GLES-Prüfungen: tatsächlicher Schreibmaschinen-Hinweis und Save/Load, tödlicher Treffer,90-Tick-Verzögerung, Framebuffer, Continue, Ladefehler/Rollback, Wiederherstellung, Zeit/Zähler und EGL-Erhalt. |
| `m37_runtime_check` |1.016 tatsächliche JNI/GLES-/Audio-Prüfungen: Titel, Kampagnenloadout, AEV, Kampf, Pause/EGL und isolierter Inspektormodus. |
| `r100_render_runtime_check` |1.800 Prüfungen; realer Original-Intro-/Haus-/Radio-/Drei-Ganado-/Beutepfad und vier GPU-Neuaufbauten ohne GLES-Fehler. |
| `m37_save_game_check` |78 Originalformat-/Offset-/Rank-Prüfungen, einschließlich Continue-Feldern, Modus/Ländergrenze und u16-Wrap. |
| Gefrorenes `card.cpp::makeSaveData/CRC` als externe Quelloracle |65.659 Byte-/Source-Prüfungen erfolgreich. |
| Gefrorene `Init/Get/SetGameTime` / Schusszähler als externe Quelloracle |13.840 Prüfungen erfolgreich. |
| Inventar-/Leon-Sourceinventar |58 Stücke,43 Waffenzeilen,77 Rezepte geprüft;91 Dateien,1820 Definitionen,54 Dispatchtabellen inventarisiert. Der bestehende Audit benennt128 partielle Adapter und1692 nicht übertragene Definitionen; viele liegen außerhalb r100. Dies ist keine Fertigstellungsquote. |

Der Save/Load-Test vergleicht die rekonstruierten Koordinaten mit den tatsächlich gespeicherten Source-f32-Werten und einer sehr kleinen SAT-Platzierungstoleranz0,02. Ein Vergleich gegen unquantisierte native Double-Koordinaten hatte einen falschen Testfehler ausgelöst; das originale Kartenformat speichert keinen Double-Vektor.

## Grenzen der Fertigstellung

- Vollständige r100-relevante Leon-Zustands-/Schadensdispatches, Originalkamera, Hand-/Hals-/Boden-IK und sämtliche Todes-/Griffvarianten sind nicht übertragen.
- EM10-Hindernisaktionen, Knie-/Gliedmaßen- und weitere Spezialreaktionen sind unvollständig. Ein sichtbarer Ganado und der durchspielte Testkampf belegen diese Zweige nicht.
- Allgemeine dynamische SAT/EAT-/Objekt-/Aufgabenmanager und die gemeinsame globale Original-RNG-Reihenfolge sind weiterhin begrenzte native Adapter.
- Die echte Kartenraumgeometrie ist integriert; die vollständigen Map-Tür-/Ziel-/Itemmodelle, MapModelLight4, Zoom-/Animationszustände und alle Menüeffekte fehlen teilweise.
- Checkpoint/HUD/Save restaurieren die tatsächlich übertragenen Besitzer. Nicht portierte ursprüngliche Enginebesitzer, spätere Upgrades/Waffen-/Partnerzustände werden dadurch nicht automatisch vollständig unterstützt.
- Die Native-GLES-Ausführung unter Mesa ist kein Adreno-/Retroid-Nachweis. Android-Canvas, vollständiger App-Kaltstart, MediaCodec-/Controller-Dialogfolge und kompletter Level einschließlich sämtlicher optionaler Varianten wurden nicht auf dem Zielgerät durchgespielt. Es gibt keinen Konsolen-Referenzfilm für Pixel-/Audiovergleich.
- Generisches Einlesen von LIT/Nebel/Effekten beweist nicht sämtliche401 Räume; auch der bestehende r101-Adapter meldet weiterhin fehlende spezielle Esp46-/EAR-/SST-Effekte.

Die aktuelle Matrix enthält alle31 r100-Modulroutinen und18 gemeinsame Systeme. Die vollständigen Original-Recordlisten stehen im vorhandenen `R100-SOURCE-INVENTORY-M37.md`.

## Einspielen und Reproduzieren

Das ZIP enthält vollständige neue/geänderte Dateien unter `RE4-Android-Decomp/`. Neben dem vorhandenen Projektordner entpacken und die enthaltenen Dateien in M37 plus Buildfix übernehmen. Es löscht keine Dateien, ändert keine Versionsnummer und enthält keine Disc-/Video-/Bildassets, APKs, privaten Toolchains oder absoluten Arbeitsrechnerpfade. `FILES-R100-KORREKTUREN.sha256` listet alle ausgelieferten Dateien außer sich selbst.

Android wie im bestehenden Projekt mit `./gradlew assembleDebug lintDebug` bauen. Hostverträge über `bash tools/test_host.sh`. Die Originaldatenprogramme erhalten ausdrücklich lokale, rechtmäßig vorhandene Disc-/Raum-/Modellpfade als Argumente; die jeweiligen Usage-Zeilen nennen die Reihenfolge. Insbesondere `r100_render_runtime_check` benötigt R120, R100, R101, PL00, WEP01, WEP02, WEP07, ROOMINFO, CORE, EM12 und ESL. Originaldateien sind nicht im ZIP enthalten.
