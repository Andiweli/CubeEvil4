# M32: gemeinsame Raumbeleuchtung und originale Fußschatten

Die Beleuchtung wird weiterhin aus dem **eigenen LIT-Archiv des geladenen Raums** gelesen. Der Dorfnebel wird nicht auf andere Räume übertragen. Die Unterstützung ist noch keine vollständige Nachbildung der GameCube-GX-/TEV-Pipeline.

## Gegen den eingefrorenen Originalcode geprüft

- `game/light.cpp::cLightMgr::init`, `roomInit`, `checkKind`, `setModel2`: Die 256 Lichtgruppen sind anfangs eingeschaltet. Eine von null verschiedene `Kind`-Nummer ist keine ausgeschaltete Lichtquelle. M31 hatte solche Lichter fälschlich übersprungen. Typ-7-Ambientlichter verbrauchen ebenfalls einen der acht Einträge des ursprünglichen Modell-Lichtarrays.
- `include/light.h::LIT_TYPE05_FREE`, `game/trans_lit.cpp::lightSetParallel`: Das Kamera-Richtungsflag ist ein **Big-Endian-32-Bit-Wort**. Das erste Byte dieses Wortes war in M31 irrtümlich als Flag gelesen worden. Die Kameraabhängigkeit gilt für die Parallelrichtung, nicht zusätzlich für die lokale Elternmatrix.
- `light.cpp::cLight::calcPos`, `getNormal`: `ParentType=1`, `parent.no=0` bindet an Leon. Aktuelle, vollständig transformierte Parts-Matrizen ersetzen den bisherigen vollständigen Ausschluss dieser Elternlichter. Andere Eltern benötigen ihre noch nicht vollständig portierten Besitzer und werden weiterhin nicht als beliebige Weltlichter umgedeutet.
- `trans_lit.cpp::lightSetCustom`, `include/light.h::LIT_TYPE04_FREE`: Licht-**Type 4** verwendet die sechs originalen GX-Koeffizienten `A0,A1,A2,K0,K1,K2`; sie werden geprüft gelesen und im GLES-Shader ausgewertet. Dies ist unabhängig von Licht-**Id 4**, dem Schattenhandler.

## Neuer gemeinsamer Fußschattenpass

`foot_shadow.cpp::DrawFootShadow`, `drawShadowParts`, `foot_shadow_tbl.cpp::pl_fsd_dat` und `pl_class.cpp::shadowCtrl` bilden die Grundlage:

- Nur tatsächlich vorhandene, aktive **LIT-Id4/workKind5**-Lichter erzeugen Fuß-/Körperschatten; keine erfundene pauschale Kreisscheibe.
- Sechs Originaljoints `0x15,0x13,0x12,0x19,0x17,0x16`, Originalgrößen 300/450/700 und Originalfarben; die Originalverbindungen erzeugen insgesamt zwölf Sprites pro passendem Licht.
- Projektion mit den Originalwinkeln beziehungsweise der punktförmigen Lichtrichtung auf den tatsächlich vorhandenen SAT-Boden, mit der vorzeichenbehafteten Quellen-Höhenkorrektur `Tex_no*10+50`.
- Originale CORE-Effekttextur **Esp 0x13**, aus den importierten Daten gelesen. Das Patch-ZIP enthält diese Grafik nicht.
- Stärke aus Licht-Rotwert, quadratischem Quellen-Distanzgesetz und `Shd_color`. Die Kamera-/Bodenneigungsprüfung und das Auf-/Abblenden um 16 pro 30-Hz-Tick folgen `shadowCtrl`; Leiterfahrt und Sprungabstieg deaktivieren den Schatten wie die entsprechenden Spieleraktionen im Original.
- Schwarze Alphablend-Sprites, `LEQUAL`, keine Tiefenschreibvorgänge. Posen, Animation, Ton und Kampfrandom werden vom Renderpass nicht weitergeschaltet. Pause hält den Schattenzustand; EGL-Wiederherstellung lädt die Originaltextur erneut und stellt die gespeicherte Stärke wieder her.

Die 127 gelieferten primären LIT-Archive bleiben auswertbar. **104 Räume** enthalten in ihrem Anfangs-Cut einen Fußschattenmodus 5. Die anderen Räume erhalten keinen erfundenen Ersatzschatten. R120 enthält weiterhin 26 ausdrücklich als unbrauchbar ausgewiesene spätere Quellen-Cuts; der gültige Anfangs-Cut bleibt erhalten.

## Originales Licht des Mündungsfeuers

Zusätzlich zu den originalen Esp00-Flammen-/Rauchsprites steuert der begrenzte Waffenadapter den eigentlichen **Esp11-Lichtimpuls** aus `game/esp11.cpp::SetFreeWork/SetParam`. Esp11 zeichnet selbst keine Spritegrafik:

- `Work8={0,8,0,1}` bindet **CORE-LIT-Cut8/Licht0**, nicht den Dorf-LIT-Cut8. Dessen originale quadratische Lichtfunktion und Intensität bleiben erhalten.
- Position aus der aktuellen Original-Waffenpart3-Matrix und dem lokalen WEP-Offset; Radius aus WEP-Größe mal zehn, Originaltönung und halbierter Basisalpha. Keine Lichtposition auf Leons Brust.
- Der Waffenbesitzer schaltet den Impuls einmal pro 30-Hz-Tick weiter: ein Tick für WEP01/WEP02, zwei für WEP07. Rendern und Pause verlängern oder starten ihn nicht neu. Die gespeicherte Waffenmatrix gehört zum wiederherstellbaren Impulszustand.
- Die gemeinsame Beleuchtung verwendet diesen tatsächlichen Impuls für Umgebung, Leon und die sichtbare Waffe. Poolreihenfolge und die acht ursprünglichen GX-Einträge bleiben maßgeblich; es wird keine neue Prioritätsregel erfunden.
- `cLightMgr::update` reserviert nach den Cut-Lichtern mindestens zehn lebende unsichtbare Slots. Deshalb liegt der erste Impuls in r101 auf Poolslot25, in r100 auf Slot30; die Modell-`SelectMask` wird mit diesem Quellenindex geprüft. `cLit::getMaxLight/versionUp` liefern die größte deklarierte Quellen-Lichtzahl.

## Offen und nicht als 1:1 behauptet

Kamera-/Szenario-Wechsel der LIT-Cuts; zeitabhängige und geschaltete Lichtgruppen; Eltern von Gegnern, ETS und Scrollobjekten; die vollständigen Modell-Lichtvolumen; Schattenlichter mit festem/parallel angepasstem Cast-Pass und Self-Shadow; originale Schattenreceiver; sämtliche TEV-Funktionen, Filter, Tiefenunschärfe, Glow und Kontrast. Wasser/Himmel/EFF sind weiterhin nur für nachgewiesene Quellenprofile aktiv. Der neue Fußschattenpass ersetzt diese anderen Systeme nicht.

## Prüfbarkeit

`tools/test_m32_lighting_gl.cpp` verwendet die echten CORE-/PL00-/WEP01-/r101-Archive außerhalb des Projekts und echte EGL/GLES3-Shader. Es prüft nichtzero Kind, Type4- und K0-Bildausgabe, originale Fußschattentextur/-Joints, Pause und GPU-Wiederherstellung sowie die gesamten 127 LIT-Archive. Für Esp11 prüft es CORE-Referenz, Waffenpose, Farbe/Radius/Intensität, tatsächliche Bildaufhellung ohne Billboardpass, Pause/Wiederherstellung, Modellmaske auf Quellenpoolslot25 und vollständiges Erlöschen nach dem Originaltick. Zusätzliche Hostverträge prüfen das BE32-Richtungsflag, rohe GX-Koeffizienten, Quellengrößen/-Joints/-Unterteilungen und vorzeichenbehaftete Schattenhöhe.
