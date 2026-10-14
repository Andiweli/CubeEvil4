# M37 – originale Itemeffekte im ersten Gebiet

Referenz: `upstream/re4`, Stand `9b76ff003081d6ece8b838453fb8310743a972c7`, insbesondere `sceAtItemEffSet`, `sceAtItemDisappearEffSet`, `SceAtCreateItemAt`, `sceAtCheckItemEffectCol`, `sceAtCheckSaveItem`, `esp0c.cpp`, `espgen02.cpp`, `EfmSetObj04` und `obj04.cpp`. Die Produktionsdaten stammen aus dem tatsächlichen Disc-1-`etc/core.das` und `r100.dat`.

Die sichtbare Beute benutzt die originalen CORE-EST-/ANM-Daten und das originale CORE-Modell `0x25`. Es wurde kein Ersatzstern, künstlicher Lichtkreis oder synthetisches Partikelsystem hinzugefügt. Die 15 verwendeten CORE-Sequenzen enthalten 41 unveränderte 300-Byte-Records, darunter 13 Efm04-Modelle, zwei Esp0c-Steuerungen und sechs originale Pfadgeneratoren. Die Esp0c-Steuerungen von CORE21 erzeugen die ursprünglichen Kindsequenzen24/25; die Pfadfunken verwenden den originalen Pfad1, seine Tangentenmatrix und ANM13.

## Welche Funde tatsächlich funkeln

Die Farben richten sich nach der ausgeführten Funktion und den tatsächlichen `itemInfo`-Typen. Einige erläuternde Kommentare im eingefrorenen Original benennen die Typen anders als der Code. Geld `0x78` hat tatsächlich Typ8, Spinell `0x57` Typ12: beide wählen Effekt3, CORE2C. Kräuter haben Typ6 und wählen Effekt4, CORE31. Waffen, Munition und Granaten wählen Effekt5, CORE2F. Schlüssel vom Typ5 wählen Effekt2, CORE33+21. Die Sonder-ID8C wählt Effekt8.

Alle 24 ITA-Records in r100 wurden geprüft. ITA9, gebunden an Krähe16, schreibt Effekt3 für den Spinell vor. ITA10, gebunden an Krähe32, schreibt Effekt5 für die Granate vor. Alle anderen r100-Records schreiben Effekt0 vor; das betrifft auch die festen Funde in Kisten. Deren Originalmodell bleibt sichtbar, ohne einen zusätzlich erfundenen Glüheffekt. Die beiden unveränderten Y-Effektversätze von50 liegen in ITA18/22. `eff_offset` liegt im Record bei0x6C, `eff_type` bei0x80.

Laufzeitbeute verwendet die originale Unterscheidung: Effekt−1/0/6 wählt automatisch eine Farbe, während statischer Effekt0 ausgeschaltet bleibt. `item_pos` erhält bei Laufzeitbeute +100Y, bei Effekt8 +1000Y; der Aufnahmebereich bleibt an der ursprünglichen Drop-Position. Das Laufzeit-Itemmodell bleibt, wie in `SceAtCreateItemAt`, außer bei Effekt8 verborgen. Die Effekte werden bei Aufnahme vollständig einschließlich Kindsequenzen und Modellen entfernt. Der normale Inventar-/Itemmenüablauf verwaltet weiterhin den aufgenommenen Gegenstand.

Temporäre Gegnerbeute übernimmt den Timer61 und den Umschaltpunkt6 aus `sceAtCheck_main`: die letzten sechs Timerperioden ersetzen den Dauereffekt durch CORE2E/30/32/34/47. Die native Welt läuft mit30 Originalticks pro Sekunde. Der globale Original-Framephasenversatz wird nicht emuliert; die Zeit wird ab Drop-Erzeugung gezählt. Die Originalfunktion `sceAtCheckSaveItem` verwendet Typ5/7. Geld vom tatsächlichen Typ8 gehört daher zu temporärer Beute. Beim tatsächlichen Raumwechsel bleiben nur die Original-SAVE_ITEM-Typen5/7 bestehen. Temporäres Geld und Spinell verschwinden dabei; ein Neuaufbau desselben Raums für Ereignis-/GPU-Wechsel behält lebende temporäre Funde. Gespeicherte Laufzeitfunde behalten Höhe, Farbe und Restzeit; die Wiederherstellung addiert die100/1000 Einheiten nicht erneut.

## Eigentümer und Darstellung

`ItemEffects` besitzt eine unveränderliche CORE-Bank und eine eigene kontrollierte visuelle RNG-Kette. EST-, Generator-, Modell- und Kindzustände laufen einmal pro30-Hz-Welttick. Ein `sync` beim Zeichnen oder im pausierten Menü entfernt aufgenommene Funde, treibt jedoch Animation und RNG nicht voran. Die visuelle Kette verändert den Spiel-/Kampf-RNG nicht. Die native Reihenfolge zwischen sämtlichen globalen Original-Effektsystemen wird dadurch nicht als vollständig identisch ausgegeben.

Die Raumvorbereitung bindet Quellen aus `RoomPickups` und freigeschalteten `ObjectActions`; Krähen und Ganados geben ihre tatsächlichen Drop-Effekttypen weiter. `SceneProbe` reicht den CPU-Eigentümer an die produktive GLES-Pipeline durch. Originale Modellfarbe,15-Tick-Einblendung, Blend-/Z-Modus, ANM-Texturen und Pfad-/Billboard-Geometrie werden gezeichnet. Raum-/GPU-Eigentümerwechsel und EGL-Verlust verwerfen GPU-Namen und laden sie erneut; der CPU-Zustand bleibt erhalten. Ungültige Snapshots werden vor dem Commit verworfen, einschließlich nichtendlicher Modellparameter und fremder Pfadanimationen, ohne den aktuellen RNG zu verändern.

Der übertragene Umfang umfasst die hier verwendeten CORE-Itemsequenzen und nicht die zusätzlichen, raumspezifischen Parent-/Parts-Varianten der Räume21B/30F. Die vollständige globale Original-EST-Verwaltung und alle Item-Sonderfälle anderer Gebiete werden damit nicht als übertragen behauptet.

## Nachweise

- `m37_item_effects_check CORE.DAS R100.DAT`:4.508 Prüfungen bestanden; tatsächliche Record-Inventare, alle r100-ITA-Effektwerte und Versätze, originale feste Kistenmodelle, normaler Kisten-/Gegner-Pickup, Quellentfernung, Ablauf, Wiederherstellung, Efm04-Einblendung und Pfad-/Kindanimation über mehrere hundert Originalticks.
- Die Farbauswahl für alle256 IDs wird gegen den vollständigen eingefrorenen Funktionskörper `sceAtCheckItemEffectCol` geprüft. `python3 tools/test_item_effect_source.py` verifiziert, dass die beiden Oracle-Körper `sceAtCheckItemEffectCol` und `sceAtCheckSaveItem` bytegleich mit der Referenz sind. Der Oracle verwendet dieselben bereits separat übertragenen `itemInfo`-Daten; er behauptet keine zweite unabhängige Itemtabelle.
- `m37_item_effects_gl_check CORE.DAS OUTPUT.PPM`:24 Prüfungen bestanden. Tatsächliche Modell-/Sprite-Pixel, exaktes pausiertes Bild, getrennte CPU-/GPU-Eigentümer, vollständige Entfernung nach Pickup und Wiederaufbau nach echtem EGL-Kontextwechsel werden geprüft. Der Frame liegt in `/tmp/m37-item-glow.ppm`.

Diese Host- und GLES-Nachweise ersetzen keinen Android-Gerätetest.
