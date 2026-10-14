# SMX-Objektparameter und Raumwahl — M13

Der Abschnitt am Ende beschreibt die in M16 zusätzlich aktivierten Renderzustände. Die ursprünglichen M13-Aussagen beziehen sich auf den damaligen Entwicklungsstand.

## Originalbelege und Datenlayout

`upstream/re4/include/scroll.h::cSmxData/cSmxWork` und `src/game/scroll.cpp::setObj/smxInit` definieren den Ladeweg. SMX-Version 0x10 besitzt einen 16-Byte-Header; Byte 1 zählt die anschließenden 0x90-Byte-Records. Archivpolster bleiben uninterpretiert.

| Offset | Inhalt |
| --- | --- |
| 0x00–0x03 | Work-ID, Objekttyp, Sortiertyp, CullMode |
| 0x04 | BE32 Licht-Auswahlmaske |
| 0x08 | BE32 SMX-Flags |
| 0x0c | BE32 Materialfarbe RGBA |
| 0x10–0x87 | 120 Byte ursprünglicher Objekt-Work einschließlich SpecularColor |
| 0x84 | BE32 SpecularColor |
| 0x88/0x8c | BE-Float UV-Geschwindigkeiten |

Der neue begrenzte Parser übernimmt alle diese Angaben ohne Zeigerrelokation. Abgeschnittene Records, falsche Version und nicht endliche UV-Geschwindigkeiten führen zu einem Fehler. Die Recordzahl ist durch das ursprüngliche Bytefeld auf 255 begrenzt.

Die Zuordnung erfolgt über **WorkNo**, nicht über die Position im SMD-Array und nicht über BinId. Der erste passende SMX-Record gewinnt. WorkNo 0xfe erhält gemäß `setObj` keine SMX-Anbindung. `smxInit` akzeptiert keine ModelNo über 0xf9; diese Records bleiben lesbar, beeinflussen aber keine Vorschaufarbe. Inaktive SMD-Works 0xff werden weiterhin nicht gezeichnet.

## Materialregister in der Vorschau

`smxInit` kopiert die RGBA-Materialfarbe. Ist RGB komplett null, werden die drei RGB-Bytes auf weiß gesetzt. Das ursprüngliche unterste Byte wird anschließend in `blend_mode` übernommen, und das Material-Alpha auf 255 gesetzt. **Dieses Byte ist daher keine Objekt-Deckkraft.**

Die Vorschau verwendet ausschließlich das so belegte RGB-Materialregister. `trans_lit.cpp::commonLightSet` setzt dieses Register auch im Vertex-Ambient-Zweig. Der bisherige Ambientvorschaupfad multipliziert Textur-RGB, vorhandenes Vertex-RGB und Material-RGB. Vertex-Alpha und separate Alphamasken behalten ihre bisherigen Quellen und Grenzwerte. Fehlende Texturen bleiben als bestehende Geometrie-Prüffarbe erkennbar. Die Normalen-Prüfansicht zeigt Richtungen, nicht Materialfarben; beim Rückschalten wird das Materialregister wieder benutzt.

Die benachbarte GPU-Batchzusammenfassung vergleicht nun zusätzlich Material-RGB. Unterschiedliche Registerfarben werden nicht zusammengelegt. Keine Änderung der ursprünglichen Reihenfolge.

Einige Objekte erscheinen gegenüber M12 dunkler oder anders eingefärbt. Das sind gespeicherte Materialfarben, keine neu erfundenen Lichter. Die Darstellung ist weiterhin eine Ambientvorschau ohne vollständige GX-Kanal-/TEV- und Lichtberechnung.

## Bewusst noch nicht aktive SMX-Laufzeitfunktionen

Objekttyp, Sortierung, CullMode, Lichtmasken, Flags, SpecularColor, Blendmodus und UV-Geschwindigkeit werden gelesen bzw. durch die Originalbytes geprüft. Ihre Runtime-Funktionen werden nicht als fertige Umsetzung ausgegeben. Insbesondere wird eine UV-Geschwindigkeit nicht als fester UV-Versatz benutzt, und ein Objekttyp wird nicht als pauschaler Unsichtbarkeitsschalter interpretiert. Culling bleibt im bisherigen zweiseitigen Prüfmodus. Bewegliche SMX-Objekte werden als dokumentierte starre Ausgangsgeometrie gezeigt, nicht animiert.

Die SMD-Gruppentabelle liefert logische Work-Zahlen. `setObj` verarbeitet dennoch nur `nModel` Platzierungsrecords; daraus können keine zusätzlichen gespeicherten Platzierungen abgeleitet werden. Skript-/Gruppeninstanzen fehlen weiterhin.

## Raumwahl

Die Android-Raumprüfung bietet zusätzlich den vollständigen, vorhandenen 142-Raum-Katalog. Der Nutzer wählt einen Raum, lädt ihn und öffnet danach die 3D-Teilansicht. Es werden keine 142 Räume gleichzeitig geladen.

Jeder Ladeversuch prüft die Quelldatei und die entpackten Bytes gegen die bereits vorhandenen Original-SHA256-Referenzen. `inspectScene` prüft primäre/gemeinsame SMD, Texturressourcen und SMX vor dem Aktivieren der Öffnen-Schaltfläche. Erst nach erfolgreicher Prüfung ersetzt ein vollständiges temporäres Dateiabbild atomar den gewählten Raumcache. Abbruch oder zerstörte Activity aktiviert keinen halbfertigen Cache. Es werden keine installierten Disc-Dateien verändert.

Ein fehlender Disc-Import oder ein nicht unterstütztes Modellformat bleibt mit einer Meldung im Auswahlmenü; die 3D-Schaltfläche wird nicht aktiviert. Vier primäre SMD-Versionen im Katalog sind weiterhin außerhalb des Parsers; 138 primäre SMDs bestehen die Datenprüfung. Raumname und geladener Katalogeintrag bleiben zusammengebunden, damit nach einer Auswahländerung nicht versehentlich der vorige Raum geöffnet wird. Androids Auswahl-/Schaltflächen behalten die normale D-Pad-Fokusnavigation; Gameplay-Mapping und RetroTouch sind nicht verändert.

## Prüfungen

`test_room_resources.py` vergleicht zusätzlich jeden SMX-Record einschließlich sämtlicher 120 Work-Bytes, UV-Floatbits und Originalmaterialwerte unabhängig gegen die Disc-Datei. Jede gezeichnete Primitive wird auf Work-ID und daraus abgeleitete RGB-Farbe geprüft. Quellfreie Szenentests prüfen ersten Treffer, Indexgrenzen, Null-RGB, Blendbyte-Trennung und fehlerhafte Dateien.

Der GLES-Test prüft getrennte rote/blaue Materialregister in angrenzenden Batches und den echten r100-Work 0: 94 Dreiecke, Material-RGB 42/42/42. Beide Prüfungen benutzen den tatsächlichen Shader; die Rückkehr aus der Normalenansicht ist framebuffergenau geprüft.

Kein Gameplay, keine komplette Raum-Sichtbarkeit, kein Originalengine-Start. Der Android-Raumwahl-/Lebenszyklustest auf dem Nutzergerät steht bei Auslieferung noch aus.

## M16 — belegte Blendmodi, OT und UV-Scroll

`trans.cpp::commonModelTrans` definiert fünf Einträge; die Vorschau verwendet diese Faktoren unabhängig davon, ob eine Alphamaske vorhanden ist. Der Fragment-Grenzwert entscheidet weiterhin zuerst, ob eine Fläche überhaupt geschrieben wird.

| SMX-Modus | GLES-Blendfaktoren |
| --- | --- |
| 0 | SRC_ALPHA / ONE_MINUS_SRC_ALPHA |
| 1 | SRC_ALPHA / ONE |
| 2 | ONE / ONE |
| 3 | DST_COLOR / ONE |
| 4 | Blending deaktiviert |

Die Prüfung sämtlicher 142 Räume fand zusätzlich drei gespeicherte Records mit **Modus 5**: r20a Work 13, r216 Work 97, r31d Work 70. Die Originaltabelle besitzt keinen sechsten Eintrag. Dieser Prototyp erhält den Rohwert in der Diagnose und verwendet ausdrücklich Modus 0 als Vorschau-Rückfall; eine vollständige Originalfunktion dafür wird nicht erfunden. Die Objektruntime dieser Stellen ist noch zu untersuchen. Werte über 5 werden abgelehnt.

SMX-Flag 8 übernimmt wie `SmxSetFlag` den Alpha-Grenzwert 128 für maskierte Materialien. Der Modus allein ändert die Vertex-/Maskenalphaquelle nicht. Tiefentest und Schreiben sind getrennte Zustände gemäß `ModelRender`: 0 = LEQUAL/Schreiben, 1 = LEQUAL/ohne Schreiben, 2 = ALWAYS/ohne Schreiben. Die statischen Raumobjekte verwenden weiterhin den Original-Initialwert 0; Transparenz schaltet Tiefenschreiben nicht pauschal ab.

Die unterstützten Modell-OT-Kategorien aus `ModelTrans` werden in Pass-/Bucket-Reihenfolge verarbeitet: Typen 2/3/4 in Pass 11 mit Buckets 4/2/1, Typ 0 in Pass 13 mit Tiefe ×0,01, Typ 5 in Pass 16, Typ 1 in Pass 17 mit Tiefe ×0,05, Typ 6 in Pass 20. Ganze Objekte behalten ihre Displaylistreihenfolge; Bucketgleichheit kehrt die gespeicherte Objektanmeldereihenfolge um. Die Tiefe kommt in der statischen Vorschau vom ersten Gelenk; der originale Lichtsphärenmittelpunkt für einige Kategorien ist noch nicht verfügbar. Typ 7 benötigt einen Laufzeit-Materialsplit und wird in der Vorschau nur im frühen Pass gezeigt; im geprüften Disc-Katalog kommt Typ 7 nicht vor. Vollständige gemeinsame OT-Sortierung mit Leon/dynamischen Objekten steht aus.

UV-Geschwindigkeiten werden jetzt über 30-Hz-Spielzeit akkumuliert. Die Basis-/Masken-Abtastung benutzt diesen Versatz. Ein eingefrorener Frame verändert die UVs nicht; Pause-/Reset-Zeit wird begrenzt. Das ist der belegte einfache Scrollzweig, keine vollständige TEXMTX-/TEV-Simulation sämtlicher Flags.

Der unabhängige Raumtest vergleicht zusätzlich jeden Draw auf Blend-Rohwert, OT-Typ, Alpha-Grenzwert und UV-Floatbits. Der tatsächliche GLES-Framebuffer prüft alle fünf Blendfaktoren, einschließlich maskiertem Modus 4 ohne ungewollte Durchsicht. r100 prüft zusätzlich sichtbaren UV-Scroll sowie die exakte Rückkehr aus der Normalenansicht bei eingefrorener Zeit. CullMode bleibt zweiseitig; Beleuchtung, dynamische Objekte, Reflexionen und Laufzeit-Sichtbarkeit sind weiterhin unvollständig.
