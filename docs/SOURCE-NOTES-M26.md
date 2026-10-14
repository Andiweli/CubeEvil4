# M26 — Originalquellen und Adaptergrenzen

Referenz ist der unveränderte Upstream-Stand `9b76ff003081d6ece8b838453fb8310743a972c7` unter `upstream/re4/`. Die nativen Module werden ausdrücklich durch CMake ausgewählt; die gesamte GC-Engine wird nicht einfach mit einem Android-Compiler gebaut. Alle Modelle, Texturen, Kurven und Sounds kommen zur Laufzeit aus den bereits importierten Originaldiscs.

| System | Primäre Originalquelle | Anbindung in M26 |
|---|---|---|
| Dorfstart | `src/st1/r101.cpp::R101Init`, `Debug/roominfo.dat` | Originaler Debugpunkt1, USA-Pyre, Ausgangsflags6/7/8 aus, Regal17, Leiterfreigaben und Tür11-Schloss |
| Schloss/Objekte | `src/game/emdoor.cpp::setLock`, `obj13.cpp::setOff`, `EtcModel.cpp` | Tatsächliche ETS/ETM und Skript-BIN/TPL, getrennte Objektidentitäten |
| Kisten/Beute | `src/game/embox.cpp`, `sce_at.cpp`, `item_model.cpp`, `item.cpp` | ITA-ctrl, RandomItemCk beim Bruch, echte ITM-Varianten, Aufnahme und Sessionzustand |
| Explosive Fässer | `src/game/embarrel.cpp::emBarrelSetBomb`, `em_sub.cpp::GetWepTargetListBomb` | EST0, Originalsound, Bomb_wait2, Waffenklasse0x13, Radius6000 und Kettenreaktion |
| Spielerexplosion | `src/game/em_sub.cpp::PlBombHitCk`, `pl_dmg.cpp`, `player.cpp`, `pl_sub.cpp` | Nah-/Fernschaden, LifeDown/RNG, ursprüngliche Fall-/Stolper-/Aufsteh-/Todeskurven |
| Gegnerexplosion | Originale Em10-Schadensregeln, `src/game/em_sub.cpp`, `melee_rules`-Referenz | Posenkapseln, r2-Begrenzung2000, EAT, Schaden/Kopfregel und Dm_Blow |
| Splitter | `src/game/eff_sys.cpp`, `esp_efm.cpp`, `espgen00.cpp`, `obj05.cpp` | Authored EFM-Teile, Generator/ObjMgr-Takt, Originalflug/Floor/SAT-Reflexion/Fade |
| Feuer/Rauch | `src/game/esp00.cpp`, `esp0b.cpp`, `espgen00.cpp`, `eff_sys.cpp`, `src/st1/r101.cpp` | Originale ANM/TPL, getrennte SST-/EST-Uhren, reale Welt-/Kameraoffsets |
| Audiobanken | `include/dvd.h`, `include/dolphin/syn.h`, `src/game/dvd.cpp`, `snd.cpp`, `snd_sub3.cpp` | Originale nested DVD-ISS/PCM, WTREGION/WTSAMPLE/WTADPCM und SIT-Verkettung |
| Audioparameter | `src/game/snd.cpp`, `snd_sub0.cpp`, `snd_sub3.cpp` | STB-Kurven, Originallautstärke, Pan, Tonhöhe/RNG und SBL-Varianten |
| Einzelne Soundaufrufe | `src/wep01/objFn57.cpp`, `src/game/pl_knife.cpp`, `embox.cpp`, `embarrel.cpp`, `src/st1/r101.cpp` | Schuss, Nachladen, Messer-Frame3, Bruch, Explosion und Dorfbrand |
| Akteurflächen | `src/game/model.cpp`, `trans.cpp`, `pl_leon.cpp::setModel` | Geprüfte einseitige Flächen; ursprüngliche Kopfattachments BIN2/4/5 |

## Datenbindung statt Ersatzgeometrie

Die `ROOM_ARC_PTR`-Indizes zählen vier Archivheader-Wörter mit. r101-Pyre0x20/0x21 entspricht deshalb den tatsächlichen Directory-Einträgen28/29; Schloss0x1e/0x1f den Einträgen26/27. Es wird keine alternative Modellzuordnung geraten.

r101 hat28 ETS-Platzierungen; nach dem ursprünglichen Startprofil bleiben25 sichtbar. Die13 ITA-gebundenen Behälter sind Slots20–32. Sieben enthalten die ursprüngliche RandomItem-ID0x1001. Feste Zuordnungen umfassen Gewehrmunition, Blitz-/Brandgranate, gelbes/rotes Kraut und Geld. `ctrl bit0` entscheidet zwischen ausdrücklicher ITA-Position und Objektposition+50Y. Varianten-Vorausladen verbraucht keinen Drop-RNG.

Der EFM-Splitterpfad behält BIN-Teile, Bindzentren, TPL, Source-Kurven und Matrices. EFM0x44 liefert elf originale Metallteile; EFM0x0e vier Holzteile für die ET2D-Explosion. r400 nutzt zusammen31 Teile/1335 Dreiecke. Die Welt-Spritebank ergänzt die echten Feuer-/Rauch-Records derselben EST0.

## Deklarierte Anpassungen

- Die gemeinsame Schulterkamera ist weiterhin eine gewählte Komposition. Für extremen Wandabstand wird ausschließlich der animierte Kopfbereich per Coverage reduziert. Dafür wurde keine entsprechende Originalregel gefunden.
- Der transparente GLES-Fade unterdrückt Tiefenschreiben. Original-Obj05 wechselt ab Alpha250 zunächst die OT-Sortierung; Source-Z_Mode und Android-Renderpass sind nicht identisch.
- Der Welt-Sprite-Trigger hängt an der letzten Renderuhr und kann gegenüber dem Gameplay um einen Tick versetzt sein. Blastverzögerung, Schaden und Splitterphysik folgen den tatsächlichen30-Hz-Callbacks.
- Visueller und Audiozufall sind eigene Streams. Die vollständige originale EmMgr/ObjMgr/Player/Sound-Reihenfolge und sämtliche dynamischen Kollisionen sind noch nicht angebunden.
- Normales Leben/Drop-Ranking benutzt im Testprofil Rang5. Das dynamische Kampagnenranking und andere Charakter-/Waffeninventare sind keine behauptete Implementierung.
- Bewegungs-/Blow-Kurven verwenden den bestehenden statischen SAT-Adapter. Abtrennungsakteure, komplette Kampagnen-KI und Ledgestates fehlen.
- Native AudioTrack-Mischung verwendet48kHz und lineare Interpolation. Vollständiger GC-AX-Filter, Musik, Hall, Schritte, Stimmen und Innenraum-Soundlogik bleiben offen.
- Filter03 ist ein Alpha-Glow mit Feedback/Blur. Ein pauschaler orangefarbener Vollbildfilter wäre falsch und wird nicht erzeugt. Weitere Effektfamilien bleiben ausdrücklich diagnostiziert.

Prüfergebnisse und reproduzierbare Toolaufrufe stehen in [VALIDATION.md](VALIDATION.md). Installation und kurzer gemeinsamer Gerätetest stehen in [UPDATE-M26.md](UPDATE-M26.md).
