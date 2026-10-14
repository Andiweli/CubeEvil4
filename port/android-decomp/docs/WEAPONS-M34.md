# Waffenprüfung M34

Die Prüfung verwendet den lokalen Originalcode und die echten WEP01-, WEP02-, WEP07- und CORE-Archive der Disc. Sie deckt die drei derzeit ausrüstbaren Waffenprofile ab. Sie ist kein Nachweis einer vollständig portierten Waffen- oder Kampagnensteuerung.

## Korrigierte Abweichungen

### Mündungsfeuer am Weltursprung

`EnvironmentSimulation` erzeugt EST-Einträge mit Zeit 0 bereits im Konstruktor. `MuzzleEffects::trigger` konstruierte diese Simulation, bevor die gelöste Waffenmatrix bekannt war. Deshalb wurden die sofortigen Blitze an der Identitätsmatrix und damit nahe dem Weltursprung erzeugt; der spätere `tick` konnte diese bereits freigegebenen Partikel nicht mehr verschieben.

Die Simulation wird jetzt erst im ersten 30-Hz-Tick konstruiert, nachdem die tatsächlich gelöste Matrix von Waffenpart 3 gesetzt ist. Verzögerte Rauchgeneratoren verwenden weiterhin die jeweils aktuelle Elternmatrix. Freigegebene Partikel behalten ihre Weltbewegung. Das entspricht der Erzeugung mit Elternpart in `cObjFn57::moveFire`, `cObjShotgun::moveFire`, `EstSet` und der Matrixfreigabe der Effektsteuerung.

Ein noch nicht simulierter Schuss ist im Snapshot ausdrücklich als ausstehend gekennzeichnet. Pause und Kontextverlust vor dem ersten Tick erzeugen damit weder einen Blitz am Ursprung noch einen zweiten Schuss. Ungültige ausstehende Zustände werden atomar abgewiesen.

### WEP02: Freigabe nach Rückstoß

`wep/pl_handgun.cpp::wep02_r3_fire10` verwendet `PlShotFrameTbl[weapon_no][weapon_lv_speed] - 2`. Die Tabellenzeilen für WEP01 und WEP02 beginnen beide bei 14; beide ungetunten Pistolen benötigen deshalb Schwelle 12. Das WEP02-Profil verwendete bislang den ungekürzten Wert 14. Die gemeinsame Schwelle ist korrigiert.

`wep/pl_shotgun.cpp::wep07_r3_fire10` verwendet eine eigene Schwelle von Sequenzframe 40. Dieser Wert bleibt erhalten. Das Original blendet den Laser während des Feuerzustands aus und gibt ihn im folgenden Set-Zustand wieder frei; die quellendefinierte Rückstoßphase wird nicht entfernt.

## Abgeglichene vorhandene Profile

| Bereich | Originalquelle | Port |
| --- | --- | --- |
| WEP01/WEP02-Modelle und Normal-/Leermagazin-FCVs | Waffenmodule, `cObjFn57`, `objWep.cpp` | Originalarchive und getrennte lokale Waffenskelette |
| Pump-Schrotflinte | `wep07/wep07.cpp`, `wep/pl_shotgun.cpp` | Originalmodell, Feuer-/Pump- und Nachlade-FCVs; sechs Patronen |
| Waffenanbindung | `cObjWep::init`, `cObjShotgun::init` | Gelöste rechte Hand, Part 10, getrennte Inverse-Bind-Matrizen |
| Laserposition | `objWep.cpp::getMarkerPos` | Eigener gelöster Waffenpart 0 mit profilspezifischem Originaloffset |
| Blitz und Rauch | Originales WEP-EST0 und CORE-EFF | Originaltexturen, Muster, Lebenszeiten, Blending und 30-Hz-Simulation |
| Kurzzeitlicht | `game/esp11.cpp` und CORE-LIT Cut 8 | Quellenfarbe, Radius, Intensität und profilspezifische Lebenszeit |
| Trefferstrahlen | `pl_handgun.cpp::fire00`, `pl_shotgun.cpp::fire00` | Ein Pistolenstrahl bzw. 13 Schrotstrahlen aus den Originalpositionen |
| Munition | `game/item.cpp`, Waffenmodule | Getrennte Magazine, Reserve, Aufnahme-/Koffertransaktionen |

Die Sequenzsteuerung von Leon, einschließlich Rückstoßende und direktem Senken der Schrotflinte, wird zusätzlich in der M34-Aktionsprüfung behandelt. Originale Pumpbewegung und Geräusch bleiben erhalten.

## Regressionsprüfung

`tools/test_m32_muzzle_gl.cpp` prüft nun zusätzlich für alle drei echten Waffenprofile:

- Ausstehenden Schuss vor dem ersten Tick sowie Wiederherstellung und Abweisung erfundener Zustände.
- Jede sofortige Blitz-/Rauchposition nach Translation um 30.000/8.000/−21.000 Welteinheiten.
- Positionen und Geschwindigkeiten nach einer echten 90°-Drehung der Elternmatrix.
- Tatsächliche GLES-Pixel bei einem vom Weltursprung entfernten Waffen-/Kamerastand.
- Reproduzierbare Darstellung nach Pause und neuem EGL-Kontext.

Die frühere Identitätsmatrixprüfung allein konnte die fehlerhafte Erzeugungsposition nicht erkennen.

Der Lauf mit allen drei Disc-Waffenarchiven und echtem Mesa/EGL/GLES3 besteht mit **52.148 Prüfungen**. Die Positionsprüfungen umfassen die ersten 25 Originaltakte; freigegebener Rauch und verzögerte Generatoren bleiben dabei unabhängig vom Weltursprung. Die zusätzlichen Waffen-/Munitions-/Lichtprüfungen mit PL00 und r101 bestehen mit **5.345 Prüfungen**. Dies ersetzt keinen Adreno-Gerätetest.

## Noch offene Waffenpfade

Nicht vollständig portiert sind insbesondere die weiteren Waffenmodule und deren Tunings, die vollständige `PlWepLockRand`-Zielschwankung, Hülsenauswurf als `obj10`, die originalen szenenabhängigen Einschlag-/Wassereffekte und die komplette Waffenobjekt-/Gegnerkollision. Die vorhandenen Quellenanimationen und Effektarchive allein belegen deshalb keine vollständige 1:1-Gleichheit aller Waffenaktionen.
