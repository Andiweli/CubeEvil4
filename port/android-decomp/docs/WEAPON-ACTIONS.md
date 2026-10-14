# Originale Handgun-Aktionen im nativen Vorschauablauf — M18

M18 ergänzt den bestehenden SAT-Bewegungs- und Türtest um originale Animationen für Waffe anlegen, Zielen, Schießen, Nachladen und Waffe ablegen. Leon verwendet weiterhin sein originales Skelett mit 119 Gelenken, MotionMove, IK und Hokan-Übergängen. Die Pistole besitzt ein separates originales BIN-Modell, eine eigene Textur und ein eigenes Skelett mit vier Gelenken. Körper und Pistole werden als getrennte GPU-Skinning-Instanzen gezeichnet.

Die neue Schussfunktion ist ein begrenzter Animations- und Munitionstest. Sie berechnet keine Gegnerverletzungen, Trefferfolgen, Geschosse, Geräusche, Hülsen, Mündungsblitze oder Inventarzustände. Die anfänglichen **10 geladenen und 60 Reservepatronen** sind ausdrücklich Testmunition. Die Magazingröße 10 stammt aus dem ursprünglichen Basismodell; die Reserve 60 ist ein Vorschauwert.

## Bedienung

| Eingabe | Verhalten in der Raumansicht |
| --- | --- |
| LT / L2 halten | Waffe anlegen und zielen |
| Rechter Stick beim Zielen | Mündung nach oben/unten bewegen; Taille drehen, danach Körper drehen |
| RT / R2 beim Zielen | Originale Schussanimation; eine Testpatrone verbrauchen |
| RT / R2 halten | Weitere Schüsse mit dem ursprünglichen Basisintervall |
| X / Quadrat beim Zielen | Nachladen, wenn das Magazin Platz und die Reserve Munition hat |
| LT / L2 loslassen | Laufende Schussphase abschließen, dann Waffe ablegen |
| Start | Bewegung, Aktionen, Animationsphasen und Munition pausieren |
| A / Kreuz in der Bewegung | Den vorhandenen freigegebenen Türbereich aktivieren |
| Expliziter Inspektor-Button | Zwischen Rauminspektor und Figurenansicht wechseln |

X / Quadrat wechselt nicht mehr in den Inspektor. Die Inspektoranforderung gelangt über `NativeBridge.requestInspector` in eine Mailbox und wird im GL-Draw verarbeitet. Beim Inspektorwechsel werden Figur, Pistole und Munition eingefroren; die Rückkehr behält die vorhandene Platzierung, Pose und Kamerasicht.

Android liefert Stick-Y nach unten positiv. `ControllerActivity` kehrt dieses Vorzeichen vor JNI um; die native Aktionsschnittstelle verwendet rechts und oben positiv. LT/RT verwenden die vorhandene Hysterese: Einschalten oberhalb 0,55, Halten oberhalb 0,35. Während Waffenaktionen steuern die Trigger keinen Kamerazoom und der rechte Stick keine zusätzliche Orbitbewegung.

## Daten und Originalcode

Die Modelle und Bewegungen kommen aus den lokal importierten Originaldateien `pl00.drs` und `wep01.drs`. Es werden keine daraus extrahierten Spieldateien mit dem Quellcode verteilt. Die unabhängig geprüften Dateien besitzen diese SHA-256-Werte:

| Datei | SHA-256 |
| --- | --- |
| `pl00.drs` | `5ea520d7026c48e33e12256ef82173140e72cf4019758167a7d5aa5b6e356090` |
| `wep01.drs` | `fb838948d4161ee04da306ff0910980ff1b9e3b5701b43983a36e1d21bca3689` |

Die Zuordnung und Regeln wurden aus diesen eingefrorenen Originalquellen abgeleitet:

- `upstream/re4/src/wep/pl_handgun.cpp`: Ready-, Set-, Fire-, Down- und Reload-Abläufe.
- `upstream/re4/src/game/pl_class.cpp` und `include/player.h`: `cMot3`, `cDelay`, `cPlWaist`, Schuss- und Reloadschwellen.
- `upstream/re4/src/game/pl_wep.cpp`: `PlSetLockPitch`, `PlWepLockCtrl` und die ursprünglichen Steuerungsraten.
- `upstream/re4/src/game/pl_body.cpp` und `src/game/model.cpp`: zusätzliche Taillenrotation und ihre Matrixreihenfolge.
- `upstream/re4/src/wep01/objFn57.cpp` und `src/game/objWep.cpp`: Waffenmodell, Elternteil, private Waffenanimation und Nachladeereignis.
- `upstream/re4/src/game/item.cpp`: Basis-Magazingröße und begrenzte Patronenübertragung.
- `upstream/re4/src/lib/Padclamp.c`, `src/game/pad.cpp` und `src/game/pl_sub.cpp`: Original-Stickbereich und gehaltene beziehungsweise neu gedrückte Feuertaste.

Originale `PL_ARC_PTR`/`WEP_ARC_PTR`-Slots enthalten vier Headerwörter. Der hier verwendete rohe Archivindex ist deshalb **Slot minus 4**.

| Spieleraktion | Originalslot → roher WEP01-Index | Originaler Start und Frameumfang |
| --- | --- | --- |
| Anlegen | FCV `0x22 → 30`, SEQ `0x23 → 31` | Frame 0, Hokan 3; 15 SEQ-Einträge mit Kurvenframes 0–14 |
| Ablegen | FCV `0x24 → 32`, SEQ `0x25 → 33` | Nach Set/Fire/Reload Frame 3, Hokan 5; 15 SEQ-Einträge 0–14 |
| Zielen | FCV `0x26/27/28 → 34/35/36` | Basis/positiver/negativer Blend; Hokan 3, 40 Loopframes 0–39 |
| Schießen | FCV `0x29/2a/2b → 37/38/39` | Basis/positiver/negativer Blend; Frame 0, Hokan 0, 21 Loopframes |
| Nachladen, Basislevel | FCV `0x2d → 41` | Frame 0, Hokan 3; maximale Kurvenframe 51 |

Die Daten enthalten außerdem Spieler-Reloadlevel `0x2e/2f → 42/43` mit maximaler Kurvenframe 44/27. Die ausführbare Vorschau verwendet nur das Basislevel. Sie behauptet keinen portierten Upgradezustand.

## Blend, Richtung und Taille

`cMot3` verwendet die Basismotion und eine zweite Motion mit Gewicht `abs(rate)`. Der tatsächliche Code wählt bei nichtnegativer Rate Slot `0x27` und bei negativer Rate Slot `0x28`; einige Kommentare im Originalbaum beschreiben diese Reihenfolge falsch. Beim Vorzeichenwechsel wird die zweite Motion an der aktuellen ganzzahligen SEQ-Frame mit Hokan 2 neu gesetzt.

Die unabhängige Original-MotionMove-/IK-Prüfung transformiert Waffenrichtung `(-1, 0, 0)` mit Leons roher Handmatrix 10. Für die Aimclips an Frame 0 ergibt sie:

| Blendrate | Normierte Richtung in Leons Modellraum | Bedeutung |
| --- | --- | --- |
| 0 | `(-0,00519; +0,00297; +0,99998)` | Nahezu waagerecht nach +Z |
| +1 | `(-0,00148; +0,94755; +0,31962)` | Nach oben |
| −1 | `(+0,01145; −0,87742; +0,47959)` | Nach unten |

Frame 20 bestätigt dieselben Vorzeichen. Die Blendrate ist kein zusätzlicher Euler-Pitch. Das Modell wird nicht noch einmal um ±90 Grad gedreht; die tatsächliche Richtung kommt aus der bereits ausgewerteten Handmatrix.

Das Original verwendet den Hauptstick, die Android-Vorschau den rechten Stick. Der moderne normierte Stick wird auf den ursprünglichen Bereich mit Achsenmaximum 72 und Diagonalwert 40 abgebildet. Die achteckige Begrenzung verwendet `40 * max(abs(x), abs(y)) + 32 * min(abs(x), abs(y)) ≤ 2880`. Der Beschleunigungszähler steigt bei Auslenkung pro 30-Hz-Tick von 1 auf maximal 7 und wird bei neutralem Stick null.

Die Basisraten lauten `pitchDelta = stickY * repCtr * 0,15 / 200 / 10` und `yawDelta = -stickX * repCtr * π / 40000`. Bei positiver aktueller Pitchrate wird die Pitchänderung mit 0,8 multipliziert. Der Pitchblend ist auf −1 bis +1 begrenzt und verwendet die Original-`cDelay`-Verzögerung 0. Die Taillenzielrotation ist auf +12 beziehungsweise −9,6 Grad begrenzt; weitere Auslenkung dreht den Körper. Die tatsächliche Taille folgt mit `0,6 * current + 0,4 * target`.

Die Körperkorrektur verwendet exakt rohe Teile 1 und 2 mit jeweils halber Taillenrotation und Teil 3 mit negativer voller Rotation. Sie folgt auf MotionMove und IK. Die Y-Korrektur wird wie in `partsWorldCalc` vor die bestehende Basis multipliziert; die Welttranslation des Teils wird anschließend wiederhergestellt. Die Neckmotions werden beim Anlegen deaktiviert, damit sie den Waffenblend nicht ersetzen.

Beim Ablegen wird nur einmal 40 Prozent der aktuellen Taille in die Körperrichtung übertragen. Die Restrotation bleibt wie im Original erhalten, bis Anlegen beziehungsweise Run-Eintritt sie zurücksetzt. Das ist kein wiederholtes vollständiges Zurückdrehen pro Ablegeframe.

## Pistolenmodell und Mündung

`cObjFn57::init` hängt die Pistole ohne zusätzlichen Elternoffset an **Leons rohes Teil 10**. Die Vorschau verwendet `actorPlacement * jointMatrices()[10]` und danach die eigene Pistolenpose mit deren eigener inverser Bindung. Leons Skinningpalette 10 wäre hier falsch, weil sie bereits die Körper-Bindeposition abzieht.

Die originale Mündungsposition ist der Handpunkt `(234,5; -24; 38,33)`; die Schussrichtung ist die transformierte lokale negative X-Achse. Die Pistole bleibt auch bei Fußbewegung sichtbar in der Hand. Ihre Animationen verwenden:

| Waffenmotion | Originalslot → roher Index |
| --- | --- |
| Idle / leerer Idle | `0x34/39 → 48/53` |
| Schuss / letzter Schuss | `0x32/37 → 46/51` |
| Reload mit Restpatronen | `0x36 → 50` |
| Reload aus leerem Magazin | `0x33 → 47` |

Die Kamera verwendet für den begrenzten Aimmodus die berechnete Mündungsrichtung. Ihr Auge bleibt bei Aim, Fire und Reload nahe der Schulterhöhe, seitlich um 450 Originaleinheiten versetzt. Eine SAT-geprüfte Kamerakapsel verkürzt den Abstand; das Hochzielen bewegt das Auge nicht auf einer vertikalen Kreisbahn unter den Boden. Das ist eine begrenzte Vorschaukamera, kein vollständiger Port der originalen `CamCtrl`-Zustände. Zusätzlich zeigt die Raumansicht eine Diagnoselinie von der Mündung, die an einer SAT-Linienkollision verkürzt wird. Diese Linie ist keine portierte originale Laser-/Treffer- oder Effektlaufzeit.

## Schuss, Reload und Unterbrechungen

Eine Runde wird genau beim Eintritt in die Schussphase verbraucht. Das originale Basisintervall ist **14 Ticks bei 30 Hz**. Im Original wird Kurvenframe 12 überquert, dann folgt SetFinish und erst danach der nächste Fire-Eintritt. Die Frame-12-Schwelle allein als 12-Tick-Intervall zu verwenden würde zu schnell schießen. Das Ablegen unterbricht die vorher gestartete Schussphase nicht vor deren Mindestdauer.

Gehaltenes RT wiederholt Schüsse, solange das Magazin Patronen besitzt. Ist es durch den letzten Schuss leer, löst bloßes weiteres Halten keinen automatischen Reload aus. Ein neuer RT-Druck mit leerem Magazin und vorhandener Reserve oder X beim Zielen kann nachladen. Das volle Magazin beziehungsweise fehlende Reserve verweigert Reload; Feuer hat wie im Original Vorrang vor gleichzeitig angefordertem Reload.

Beim Basisreload werden Patronen einmal bei der **Waffen-Kurvenframe 31** übertragen: `min(10 - loaded, reserve)`. Die Spielerfreigabe an Frame 36 ist davon getrennt. Bei deutlich geneigtem Aim kehrt der Spieler über einen Hokan-9-Blend mit acht Recoveryticks zurück; bei waagerechtem gehaltenem Aim kann die Reloadmotion bis zum Ende laufen. Die Vorschau verwendet keine originale Inventarverwaltung und keinen Nachladeinterrupt, der beliebig fehlende Patronen erzeugt.

Startpause, Inspektor und fehlender Renderkontext frieren Pose und Munitionsereignisse ein. Nach Resume, Eingabe-Reset oder neuem Raumeinstieg müssen LT und RT zunächst physisch neutral werden; gehaltene Trigger werden nicht als neuer Schuss ausgewertet. Der Aktionszeitakku und ausstehende Aktionsflanken werden dabei geleert. Das ist eine Schutzregel des nativen Hosts, keine behauptete Original-Szenarioregel.

Ein erfolgreicher vorhandener M17-Türwechsel behält Magazin, Reserve und Schusszähler, setzt aber Bewegungs-, Ziel- und Waffenphase am originalen Zieleinstieg zurück. Fehler vor dem Commit behalten den alten Zustand. Scheitert der Upload des neuen Körpers oder der Pistole, wird der vorherige CPU-Raum zurückgetauscht und die GPU-Szene einschließlich Kamera wiederhergestellt. Die alte Figur wird dabei nicht neu konstruiert und ihre aktive Waffenphase wird nicht zurückgesetzt.

## Reproduzierbare Prüfung

Die Originaldateien bleiben lokale Voraussetzungen. Aus dem Projektverzeichnis kann die Hostprüfung so gebaut werden; `RE4_JNI_INCLUDE_DIR` muss auf das vorhandene JDK zeigen:

```sh
cmake -S app/src/main/cpp -B ../tooling/m18/weapon-gl \
  -DRE4_BUILD_GL_CHECK=ON \
  -DRE4_JNI_INCLUDE_DIR="$PWD/../tooling/jdk17/include"
cmake --build ../tooling/m18/weapon-gl --parallel
ctest --test-dir ../tooling/m18/weapon-gl --output-on-failure
../tooling/m18/weapon-gl/player_action_tests \
  ../tooling/m16/pl00.drs ../tooling/m16/wep01.drs
python3 tools/test_weapon_data.py \
  ../tooling/m18/weapon-gl/weapon_tool \
  ../tooling/m16/pl00.drs ../tooling/m16/wep01.drs
python3 tools/test_action_poses.py \
  ../tooling/m18/weapon-gl/action_pose_tool \
  ../tooling/m16/pl00.drs ../tooling/m16/wep01.drs
../tooling/m18/weapon-gl/weapon_gl_check \
  ../tooling/m16/pl00.drs ../tooling/m16/wep01.drs \
  ../tooling/m18/weapon-gl/weapon.ppm
../tooling/m18/weapon-gl/yz2_tool \
  ../host-disc-install/disc2/files/St3/r301.das ../tooling/m18/r301.dat
../tooling/m18/weapon-gl/android_bridge_gl_check \
  ../tooling/m16/pl00.drs ../tooling/m16/wep01.drs \
  ../tooling/m11/r300.dat ../tooling/m18/r301.dat \
  ../tooling/m17/roominfo-disc1.dat ../tooling/m18/r301-bridge.ppm
```

`android_bridge_gl_check` nimmt tatsächlich PL00, WEP01, den entpackten Raum r300, den entpackten Raum r301, `roominfo.dat` und einen PPM-Ausgabepfad entgegen. Es enthält die echte Produktionsbrücke und verwendet minimale JNI-Zeichenfolgen sowie echtes surfaceless EGL/GLES 3. Die Hostuhr wird pro Testtick auf eine reproduzierbare 30-Hz-Grenze gesetzt; Aktionszustand und Munition werden nicht direkt manipuliert.

Die unabhängige Waffenreferenz bestätigte **1.815 BIN-Ecken, 93 Draws, 8.192 exakt decodierte RGBA-Pixel und 66 originale FCV-Posen**. Die größte Positions-/Matrixabweichung dieser Waffenposen beträgt 0,0000001 Originaleinheiten. Für 594 unabhängig an Hand 10 befestigte Ecken und Normalen liegt die größte Abweichung bei 0,0003802.

Die Aktionsverträge prüfen stationäres Draw/Aim/Down, Pitch und Taille, endliche beziehungsweise fehlerhafte Eingaben, identische 30/60-Hz-Kadenz, Nullzeitflanken, den Trigger-Neutralzustand, einmaligen Reloadtransfer, volle/fehlende Munition, leeres Magazin und Erhaltung der insgesamt 70 Testpatronen. Die GPU-Waffenprüfung vergleicht die tatsächlich vom Produktionsshader transformierten Positionen und Normalen mit einer getrennten CPU-Auswertung der Eltern- und Skinningkette.

Die unabhängige Aktionsreferenz verglich **612 Posen mit 119 Körper- und vier Waffengelenken** einschließlich SEQ, cMot3, IK, Hokan und Taille; die größte Abweichung beträgt 0,00097656 Originaleinheiten. Die synthetischen Hostverträge und die Tests mit echten Handgunassets liefen außerdem unter AddressSanitizer/UndefinedBehaviorSanitizer. Für diese Containerläufe war LeakSanitizer deaktiviert, weil der `/proc`-Taskscan mit der vorhandenen Prozessüberwachung scheitert. Daraus folgt kein Nachweis einer Speicherleckprüfung und keine Behauptung eines sanitisierten Android-Gerätelaufs.

Die Produktionsbrückenprüfung behält die M17-Fälle für echte A-Türen, Completion ohne EGL, fehlerhafte Dateien, veraltete Tokens, Kontextwechsel und Körper-GPU-Rollback bei. Hinzu kommen LT/RT/X über JNI, echte Hand-/Mündungsrichtung, Pause mit gehaltenem RT, Kontextneuerstellung während einer Schussphase, Inspektor-Freeze, Munition über echte Türen und ein gezielter Fehler beim zweiten Uniformbuffer-Upload der Pistole. Der Rollback wird über die exakten Körper-/Hand-/Waffenmatrizen, beide Animationsframes, Aktion, Munition, Platzierung und Kamerasicht geprüft.

Der abschließende Lauf mit beiden Originalräumen bestand **1.108 Prüfungen**. Er exportiert zusätzlich zur ursprünglichen Raumaufnahme die Bilder mit den Suffixen `-aim.ppm`, `-arrival-aim.ppm`, `-weapon-rollback.ppm` und `-final.ppm`. Der Test fand und überprüfte auch die Korrektur des Kontext-Resumes: Das Wiederherstellen der GPU allein darf den neutralen Triggerzustand nicht umgehen.

Diese Hostprüfungen belegen keinen M18-Test auf dem Android-Gerät des Nutzers. Vollständiger Kampf, originale Waffenwechsel, Inventar, Upgrades, Szenarioflags, Zielerfassung, Laser-/Mündungs-/Hülseneffekte und Audio sind weiterhin außerhalb dieses begrenzten Schritts.

## M19 — tatsächliche Treffer im einzelnen Ganado-Test

M18 wurde anschließend vom Nutzer auf dem Gerät bestätigt; das Log `Eingefügter Text(20261003-234535).txt` enthält Schüsse, Nachladen und erhaltene Munition. M19 führt den tatsächlich gelösten Rückstoß-Handstrahl in Originaltrefferkapseln und die statischen EAT-Daten. Originalmündung und Streuung folgen `pl_handgun.cpp`, der Schuss wird genau einmal pro Munitionsverbrauch am vorhandenen 30-Hz-Takt veröffentlicht. Gegner-Hitqueue/kurzer Timer, Typ0-Ausgangsschaden und HP sind in [ENEMY-ENCOUNTER.md](ENEMY-ENCOUNTER.md) abgegrenzt. Inventar, Waffenwechsel/Upgrades, Mehrgegner-KI, Griff-/Spielerschaden, Effekte und Audio bleiben offen.
