# K17 Fix3 – Minenwerfer, Zubehör, Dorf-KI und Glockentitel

Version **0.37.17-fix3 / VersionCode 52**, App-ID `com.ast.re4decomp`. Enthält sämtliche Fix1-/Fix2-Korrekturen. Vergleichsgrundlage sind die mitgelieferten Originalquellen unter `upstream/re4/src` und die originalen Disc-Archive. Der Originalbaum bleibt bytegleich. Zu dieser Nachmeldung liegt kein neuer Gerätelog vor.

## Installation

- **RE4-M37-K17-Fix3.apk:** über die vorhandene AST-signierte K17-/Fix1-/Fix2-App installieren. App-ID und Signatur bleiben gleich; vorhandene Disc-Imports werden weiterverwendet.
- **RE4-Android-Decomp-K17-Fix3.zip:** vollständiges Android-Studio-Projekt mit unverändertem Originalbaum.
- **RE4-K17-Fix3-Austauschdateien.zip:** den enthaltenen Ordner `RE4-Android-Decomp` über das vollständige K17-, Fix1- oder Fix2-Projekt kopieren und Dateien ersetzen. Enthält vollständige Dateien und alle Änderungen seit dem hochgeladenen vollständigen K17. Danach kein älteres Update darüberkopieren.

`FILES-K17-FIX3.sha256` verzeichnet die neuen/geänderten Dateien gegenüber K17, außer sich selbst. Keine zusätzlichen Disc-Daten, Signierschlüssel oder lokalen SDK-Pfade; die bereits mit K17 gelieferten App-Assets bleiben enthalten.

## Originalabgleich und Änderungen

| Meldung | Befund und Korrektur | Originalquelle |
|---|---|---|
| Minenwerfer-Laser nach oben | `objWep::getMarkerPos` verwendet für Waffen-Nr. 14 Modellteil **1**, nicht 0. Teil 0 gehört zum links gehaltenen Werfer; seine X-Achse zeigt in Ready nach oben. Der Laser benutzt jetzt die an der rechten Hand platzierte Zielachse von Teil 1. | `game/objWep.cpp`, `wep14/objMine.cpp::partsSet` |
| Minenflug fehlt | Das normale Projektil startet an der rechten Hand entlang deren negativer X-Achse. Die Abschussrichtung wird vor dem Rückstoß festgehalten. Gezeichnet wird das echte PL-Archiv-Minenmodell mit seiner Bewegung. Vier tatsächliche Flugrichtungen tragen sichtbare Pixel zur Szene bei. | `wep14/objMine.cpp::setBullet`, `moveFire` |
| Minen-Zusatzvisier | Zubehörkombinationen waren trotz vorhandener Originalrezepte pauschal gesperrt. Mine Thrower `0x36` + Scope `0xaa` ergeben nun Profil `0xab`, mit Scope-Kamera, eigener Darstellung und Scope-Abschussversatz. | `game/item.cpp::partsCombine`, `combination_info`; `wep14/wep14.cpp` |
| Rifle/Semi-Auto ohne Visier vermutet | Beide Grundwaffen rufen im gelieferten Originalcode `CamCtrl.startScope(0,0)` auf und schalten den Laser aus. **Auch `0x2e` und `0x2f` behalten daher ihre ursprüngliche Basis-Visieransicht.** Gekaufte Zielfernrohre wählen zusätzliche Varianten mit eigenen Archiven und Zoomdaten. | `wep/pl_rifle.cpp::wep09_r3_set00`, `wep09_r2_set`; `game/item.cpp` |
| Weitere Aufsätze | Rifle-/Semi-Auto-/Mine-Scope, Infrared Scope, Red9-/TMP-Stock und gültige Schalldämpferrezepte können in beiden Auswahlreihenfolgen kombiniert werden. Ein neuer Aufsatz löst den alten. Das Ausrüsten wählt Originalmodell, Griff, Aktionsprofil, Scope und Zoom der Variante. | `game/item.cpp`, `game/read.cpp`, WEP-Module |
| Zubehör in SAVE/LOAD | Grundwaffe und Zubehör behalten getrennte Kofferslots und Formen. Originalfelder `lv=1` und `bullet=Besitzerslot` speichern die Verbindung; die ausgerüstete ID enthält das abgeleitete Waffenprofil. Munition bleibt im Grundwaffenslot. **Remove** löst den Aufsatz und behält ihn im Koffer. | `game/item.cpp::partsCombine`, `partsRemove`, `weaponId`; Original-Save-Itemrecords |
| Ganados an Tür/Turm blockiert | Die bisherige 1600 Einheiten hohe Kapsel berührte auch obere Gebäudeteile. Normale Dorfbewegung benutzt jetzt die Umgebungskugel des Originals: Radius 400, animiertes Teil-0-X/Z, Y = Fußhöhe + 400, Atari-Maske `0x2001`. Zwischen Ganados gilt Radius 250 mit Auflösen von Überschneidungen. Die RTP-Abfrage übernimmt die ursprüngliche Atari-Maske. | `em10/em10.cpp::em10AtariInit`, `game/atari.cpp`, `game/route_ck.cpp` |
| Langsame Gegner | Walk→Dash fehlte. Originale Dash-FCV/SEQ, Wartezeit, Rang-/Distanz-/Blickbedingungen, nahe Gegner, Drehgrenzen und 5–7 Bewegungszyklen sind angebunden. `MotionMove` meldet Schleifengrenzen mit 1/2; ein nur auf 4/8 geprüftes „ended“ erreicht den Rennzweig nie. Gehschleifen werden jetzt als Wechselpunkt erkannt. Kein pauschaler Geschwindigkeitsmultiplikator. | `em10/em10.cpp::em10DashCk`, `em10_R1_Walk`, `em10_R1_Dash`, `em10SetDashMotion`; `game/motion.cpp` |
| Weiterhin wartende Gegner | Keeper der Original-Characters 1/3 verlassen ihren Posten erst innerhalb des Wachradius, standardmäßig 20000. Dachflag `0x400` verhindert im normalen Goto-Modus das Hinunterspringen. Nach blockierter Verfolgung wechselt die Dachwache zu RoofWait und reagiert auf Leons Richtungswechsel. Diese Regeln bleiben erhalten. Kettensägen-Ganados verwenden ihre speziellen gefundenen Gehclips. | `em10/em10.cpp` Keeper-, RoofWait- und Bewegungsroutinen |
| Spawnpoints | Originale ESL-X-/Z-Positionen bleiben erhalten. Anfangsgruppe und beide Nachschubgruppen werden jeweils 1800 Simulationstakte in Original-SAT/RTP geprüft. Alarmierte Boden-Ganados außerhalb der entfernten Keeper bewegen sich jeweils mehr als 20000 Einheiten vom Spawn fort. Dachwachen behalten ihre Sonderregeln. | `etc/emleon00.esl`, r101 SAT/RTP, `st1/r101.cpp` |
| Room-ID bei FPS | Unten links steht beispielsweise **`Room: r101 · FPS 30`**. Die ID kommt aus dem aktiven nativen Raum und wird bei Übergängen aktualisiert. | Port: `ControllerActivity`, `NativeBridge`, `android_bridge.cpp` |
| Fehlendes „Video“ am Glockenende | `r101_Event30_TitleCall` lädt `id101.eff` und `event001.uwf` und startet den animierten Titel ab Glockenframe **1100**. Der gelieferte Handler startet dafür keinen separaten Film. Originaltexturen, UWF-Bewegung und Blende sind verbunden. Die transparente View ist vorher aktiv, damit der 100-ms-HUD-Poll den Einblendungsbeginn nicht verzögert. Ende/Skip entfernen sie; danach folgt der vorhandene Originalfunk. | `st1/r101.cpp::r101_Event30_TitleCall`, `r101_Event30`; `etc/Eng/id101.eff`, `event001.uwf` |

Die bestätigten Granaten-, Launcher- und Leon-Reaktionskorrekturen aus Fix2 bleiben enthalten. Die aktuelle Regression prüft deren tatsächliche Flugpixel, vollständige Leiterabläufe, Glockentür und anschließenden Funk erneut. Fix1 prüft weiterhin Links/Rechts und den r100→r101-Funkübergang.

## Aufsätze bedienen

Im Testbetrieb Zubehör über **SELECT → Test cheats** hinzufügen. Im Koffer Waffen-/Zubehörartikel auswählen, **Combine** wählen und den kompatiblen Gegenstand bestätigen. Verbundenes Zubehör trägt **(attached)**; **Remove** löst es. Beim Anlegen steuert DPAD oben/unten den Scope-Zoom. Diese Ergänzung implementiert keinen Händler in späteren Räumen.

## Prüfungen und Grenzen

Endgültige Ergebnisse und Build-Metadaten: `K17-FIX3-PRUEFUNGEN.json`. Tatsächliche Ausgaben: `K17-FIX3-TESTAUSGABEN.txt`.

- Neue Originaldatenprüfung: zehn physische Waffen-/Zubehörpaare in beiden Reihenfolgen, Ersetzen/Entfernen, Munition und Save-Verbindungen; 24 Minen-Zielrichtungen; Original-Scope-Auswahl und Glockentitel; drei lange Dorfverfolgungen und tatsächlich erreichter Dash-Zustand.
- Neue Produktions-JNI/GLES-Prüfung: Zubehör aus SELECT, Combine/Remove und SAVE, Varianten im Scope-HUD, sichtbare normale Minen in vier Richtungen, native Room-ID und vollständiges Glockenende mit Titel/Funk/Skip.
- Regression: alle 38 Waffenprofile, Fix1-/Fix2-Originaldaten und Produktions-JNI/GLES-Wege, 43 CTest-Gruppen, reine Java-Vertragsgruppen und Originalgeometrie aller zehn Scope-Layouts.
- Android: frischer ARM64-NDK-Build, sämtliche Java-Quellen und Ressourcen gegen API 35 neu kompiliert, frischer DEX, gleiche AST-Signatur wie K17, v1/v2/v3, 16-KiB-APK-/ELF-Ausrichtung und übereinstimmende Java-/JNI-Deklarationen.

Grafiktests laufen auf Linux mit EGL/GLES 3 und Software-Renderer. Ein physischer Android-/Adreno-/Controllerlauf und die visuelle Prüfung der neuen Android-Canvas-Titeleinblendung auf dem Gerät stehen aus. Kein neuer Gradle-`assembleDebug`-/Lint-Lauf; SDK/NDK, Java, Ressourcen, DEX und Signierung werden separat gebaut und geprüft.

Dies ist kein vollständiger Port der Ganado-Zustandsmaschine. Spezielle Hindernisaktionen, vollständige Claw-/Parasiten-/Core-KI und sämtliche GX-Details bleiben in den bisherigen Ausbaugrenzen. Mine-Homing und der spezielle Thermographie-Beleuchtungsdurchgang des Infrared Scope sind noch nicht vollständig portiert; dessen Artikel-, Archiv-, Scope- und Save-Zuordnung ist berücksichtigt. Spätere Kampagnenräume bleiben teilweise angebunden, siehe `R101-GAMEPLAY-STATUS.md`.

Neue Hostziele im Paket:

```text
k17_fix3_check FIXTURE_ROOT
k17_fix3_runtime_check FIXTURE_ROOT [CAPTURE_DIR]
```

`FIXTURE_ROOT` enthält eigene Originaldaten unter `files/` und dekodierte Räume unter `decoded/`. Vorhandene CMake-Optionen für JNI/EGL/GLES gelten unverändert. Test-Discdaten werden nicht mitgeliefert.

## Kurzer Gerätetest

1. Fix3 über die vorhandene App installieren. Minenwerfer ohne Zubehör anlegen, links/rechts und hoch/runter zielen, mehrfach in freie Sicht schießen. Laser und Minenflug sollen dieselbe Zielrichtung haben.
2. Mine-, Rifle- und Semi-Auto-Scope kombinieren, anlegen, zoomen, entfernen und einmal speichern/laden. Grundgewehre behalten gemäß Original die Basis-Visieransicht.
3. In alarmiertem r101 die Strecke vom Turm Richtung r100 verfolgen lassen. Entfernung, Blickrichtung und Rang beeinflussen den Rennwechsel; entfernte Posten und Dachwachen besitzen eigene Wartebedingungen.
4. Glockensequenz normal beenden und einmal überspringen. Titel und anschließenden Funk prüfen, danach bewegen/anlegen. Raumwechsel sollen die `Room:`-Anzeige aktualisieren.
