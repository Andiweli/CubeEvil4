# Lokale und gemeinsame SMD-Ressourcen — M12

## Originalbelege

- `upstream/re4/src/game/game.cpp`: Raumstart übergibt SMD[0], SMX und SMD[1] an `SmdInit`.
- `scroll.cpp::SmdSetParam`: WorkNo 0xff ist inaktiv; BIN-/TPL-ID 0xff wird für aktive Objekte durch 0 ersetzt. Flag 0x10 wählt den gemeinsamen BIN. Dieser verwendet **DmyZeroTpl**, keine lokale TPL. Die erste TPL der gemeinsamen SMD wird auch lokalen Modellen hinzugefügt. Flag 0x40 wählt gemeinsame Motion-Daten.
- `model.cpp::cModelInfo::addTplAddr`: gemeinsame Deskriptorzahl wird als `nAddTex` gespeichert.
- `trans.cpp::commonModelTrans`: zuerst lokale TPL-Deskriptoren, danach gemeinsame mit Index `i - lokaleAnzahl`; bei leerer lokaler Palette direkt gemeinsamer Index `i`.

Die zweite SMD wird als Ressourcenquelle behandelt. Ihre eigenen Platzierungsrecords werden **nicht** zusätzlich in den Raum gezeichnet. Sie dient auch nicht als allgemeine globale Core-Texturregistrierung (`room_tex.cpp`).

## Implementierung

`Smd::commonRequirements()` leitet benötigte gemeinsame BIN-/Motion-Tabellenlängen aus aktiven primären Platzierungen ab. Mindestens die erste gemeinsame Palette wird gelesen. Der Parser berücksichtigt zusätzlich die eigenen Ressourcenverweise der gemeinsamen SMD, ohne mutierte GameCube-Zeiger zu übernehmen. Extern verlangte Tabellen dürfen höchstens 256 Einträge besitzen; Bereichs-, Sektions-, Geometrie- und Palettenbudgets bleiben aktiv.

`BuildStaticScene` erhält optional die gemeinsame SMD. Gemeinsame BINs benutzen weiterhin die belegten starren Ruhepose- und Welttransformationen. Motion-, Mehrgelenk- und Skinning-Modelle bleiben außerhalb der Vorschau. Fehlt eine gemeinsame BIN-Quelle, zählt der Bericht die ausgelassene Platzierung ausdrücklich. Ist eine vorhandene Quelle beschädigt oder ein Index ungültig, wird abgebrochen.

`AttachSceneTextures` löst Basisbild und separate Alphamaske unabhängig auf. Lokale Deskriptoren haben Vorrang. Der gemeinsame Bereich beginnt exakt hinter der lokalen Deskriptorzahl. Nur gemeinsame Palette 0 wird angehängt; TplId wählt keine beliebige gemeinsame Palette. Gemeinsame BINs beginnen wegen DmyZeroTpl bei gemeinsamer Bild-ID 0.

Der Texturcache unterscheidet Quelle, Palette, Bild-ID und Wrap-Verhalten. Lokales Bild 0 und gemeinsames Bild 0 können daher nicht versehentlich dieselbe Textur erhalten. Original-Mipmaps, Vertexfarben, Alphagrenzwerte und Normalenansicht verwenden dieselben vorhandenen Rendererpfade. Der Szenen-Texturhaushalt bleibt insgesamt auf 128 MiB dekodierte Basis-/Mipdaten begrenzt.

IDs ab 0xf8 bleiben ungelöste Laufzeit-/Renderzielverweise. Es werden keine Ersatzbilder oder künstlichen Lichter eingeführt.

## Originaldaten und Grenzen

Der vollständige Katalog umfasst 142 Räume. Nur r100 enthält eine zweite SMD. r100 hat eine leere lokale TPL: Seine 6.020 primären starren Dreiecke beziehen ihre Basisbilder vollständig aus der gemeinsamen Palette. 3.543 davon verwenden gemeinsame Alphamasken. Die zusätzliche 3D-Schaltfläche benötigt nur den vorhandenen Disc-1-Import.

r300 enthält keine gemeinsame SMD. Seine bisherige Vorschau und Controllerbelegung bleiben erhalten. SELECT schaltet die Normalenansicht; LB/RB wählen eine Basis-Platzierung, Start zeigt die starre Gesamt-Teilansicht.

Gemeinsame BIN-Platzierungen mit Flag 0x10 sind in diesem geprüften Raumkorpus nicht enthalten; ihr Resolver wird mit künstlichen, quellfreien Testdaten geprüft. Das ist kein Originaldaten-Nachweis für einen aktiven Common-BIN-Spielpfad.

Weiterhin fehlen SMX-/Gruppeninstanzen, Laufzeit-Sichtbarkeit, Skriptobjekte, Animation, vollständige TEV-/Lichtzustände und die eigentliche Spielsimulation. M12 macht den statischen Raumladeweg vollständiger, ist aber kein spielbarer Engine-Start.

## Reproduzierbare Prüfungen

- `tools/test_host.sh`: Parser- und Szenentests einschließlich leerer lokaler Palette, lokaler/gemeinsamer Indexgrenze, separater Masken, Cache-Trennung, fehlender Quelle, Common-BIN und extern verlangter Tabellen.
- `tools/test_room_resources.py`: liest Originaltabellen, Platzierungen, Primitive und TPL-Deskriptoren unabhängig; vergleicht jede gezeichnete primitive Gruppe und aufgelöste Bildmetadaten mit `room_resource_tool` für den gesamten Katalog.
- `tools/test_material_data.py`: bestehende lokale Material-/Mip-/Farbprüfung bleibt als Regression erhalten.
- `scene_gl_check R300 OUTPUT.ppm R100`: bestehende GLES-Prüfungen plus tatsächliche gemeinsame r100-Texturen, 6.020/3.543 Gesamtzähler und exakte Rückkehr aus der Normalenansicht.

Originaldaten werden nicht mitgeliefert.
