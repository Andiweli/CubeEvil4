# M37 – Leons ursprüngliche Zielabweichung, Augen und Radiogriff

Referenzen: `game/pl_wep.cpp::PlWepLockRand`, `game/pl_class.cpp::moveEyeNormal/moveEyeMotion`, `include/player.h::cDelay`, `game/player.cpp::cPlayer::move`, `game/pl_leon.cpp::setLeftHand` sowie die tatsächlichen Disc-1-Archive PL00/WEP01/WEP07.

## Zielabweichung und Schussreihenfolge

Der native Spieler führt den ursprünglichen `PlWepLockRand`-Pfad beim Zielen aus. Die Fähigkeit des normalen Waffenobjekts verwendet die ursprünglichen Gradwerte 5,73/2,86/0,2864/0,2864 und deren ursprüngliche Umrechnung in Radiant. Bewegung setzt die Zentren ohne Zufallsaufrufe. Stillstand verwendet zwei ursprüngliche Zufallssamples und die ursprünglichen Grenzen; auch die Yaw-Grenze verwendet in diesem Zweig den ursprünglichen Pitchbereich. Der Frischsetzen-Zweig nach einem Schuss verbraucht ebenfalls beide Samples. Der ursprüngliche `lockRandCtr` startet mit Null; der Adapter erfindet keinen neuen Handgun-/Shotgun-Rückstoßfaktor.

Die Körpermotion verwendet den beim Lösen vorhandenen Dreifachblendwert; der nachfolgende Steueraufruf setzt den Wert für die nächste Pose. Beim Handgun-Schuss erfolgen zuerst die beiden Streuungssamples, dann die beiden Frischsetzen-Samples, dann die beiden Stillstandssamples und schließlich die ursprünglichen Augen-Zufallsbytes. Die Befeuerungsaktion und der Steueraufruf behalten ihre beiden ursprünglichen Waist-Lösungen. Der Shotgun-Pumpzweig erhält die eigene Reihenfolge mit Steuerung ab Frame10. Diese native Auswertung läuft auch ohne einen registrierten Schussbeobachter.

`AnimationRig::posedBlend3Rate()` liefert den wirklich zur zuletzt gelösten Motion verwendeten Wert. Er beschreibt die gelöste Pose und nicht den später gesetzten Wert für den nächsten Frame.

## Augen

`PlayerEyeState` trägt die ursprünglichen drei statischen Besitzerzustände aus `moveEyeNormal`: `cDelayF eyeDir`, Blinktimer und Augenbewegungstimer. Die ursprüngliche Verzögerung bleibt Null. Die vollständigen normalen und zweiten Blinkfolgen, ihre Konstanten, das Zurücksetzen bei Tick30, die Zufallsentscheidung bei Tick88, die Augenbewegung alle zwei bis vier Ticks sowie Begrenzung und `cDelayF::move()` bleiben in ursprünglicher Floatreihenfolge erhalten. Fälle ohne neue Lidzuweisung behalten den tatsächlichen Lidwinkel aus der aktuellen Motion.

Nach jedem nativen Spielertick verändern die Augen die wirklichen Teile: Lid-X in Parts0x1C und Eye-Y in Parts0x20/0x21. Der Aktualisierungszeitpunkt entspricht `MotBase/Neck → moveEye → Body/partsWorldCalc → Kontakt`. Tod und pausierter Spielertakt führen diese Augenlogik nicht weiter.

`setRoomScriptEyeMode(1)` aktiviert den ursprünglichen Motionmodus: Der vollständige Winkel von Parts0x20 wird nach der tatsächlichen FCV-Auswertung nach Parts0x21 kopiert. Normaler Blink-/Augentimer und dessen Zufallsaufrufe sind währenddessen ausgesetzt. `setRoomScriptEyeMode(0)` nimmt deren gehaltenen Zustand wieder auf. Das ist die ursprüngliche Radio-Gesichtssteuerung und verändert keine Kamera.

Die ursprünglichen statischen Augenwerte sowie die Player-Zufallsfolge bleiben beim Rekonstruieren des Spielers für einen anderen Raum erhalten. Der native Spieler besitzt diese Folge, sodass Vorschau-/Testakteure sich nicht gegenseitig beeinflussen. Innerhalb Leons bleibt die Reihenfolge von Waffen- und Augenaufrufen originalgetreu; die komplette globale RNG-Verkettung mit sämtlichen Gegnern und Effekten ist weiterhin eine separate Portierungsgrenze.

## Radiogriff

Der linke Griff2 wird einmal aus dem tatsächlichen PL_ARC0x16, also DRS-Index18, mit TPL_ARC0x11, also Index13, geladen. Seine echten Ecken und Texturen stehen auch ohne ausgerüstete Schrotflinte zur Verfügung. `setRoomScriptLeftHand(2)` wählt diesen Besitzer über die vorhandene sichtbare Teileauswahl. Der ursprüngliche Wert0x63 stellt `oldLhandNo` wieder her und aktualisiert anschließend Alt-/Neuwert in der ursprünglichen Reihenfolge. Eine zweite Wiederherstellung kann dadurch wieder zum zuletzt verlassenen Griff führen; der Adapter ersetzt diese Semantik nicht durch einen festen Handgun-Griff.

## Nachweis

| Prüfung | Ergebnis |
|---|---:|
| Wörtlicher `PlWepLockRand`-Quellcode, unabhängiger Byte-RNG und tatsächliches natives Zielen/Schießen/Radiogriff | 393.831 Prüfungen bestanden |
| Wörtlicher `moveEyeNormal`/`cDelay`-Quellcode, 64 Seeds × 8.192 Ticks, tatsächliches R100-Leon-Skelett und Radio-FCV | 1.576.049 Prüfungen bestanden |
| M34 tatsächliche Shotgun-FCV/SEQ, einmaliger Schuss, Pumpablauf, Loslassen und Rückkehr | 1.010 Prüfungen bestanden |
| Tatsächliche frühe Waffen, Sound-Aufrufzeitpunkt/-position und dekodierte PL/WEP-Samples | 907 Prüfungen bestanden |

Die Quellcode-Orakel sind getrennt von der Produktionsimplementierung. Das Augen-Orakel übernimmt den wörtlichen Funktionskörper und die originale `cDelay`-Implementierung; nur die drei statischen Speicherdeklarationen werden für unabhängige Testbesitzer zu Feldern. Die tatsächliche Radio-FCV besitzt unterschiedliche Teilewinkel und benötigt nachweislich die ursprüngliche Parts0x20→0x21-Kopie. Der FCV-Waffenvergleich übernimmt ausschließlich die separat nachgewiesenen Augenwinkel und den separat nachgewiesenen, wirklich gelösten Blendwert; er prüft danach weiterhin die tatsächlichen Körpermatrizen.

Diese Änderungen übertragen die genannten Routinen. Vollständiges Konfigurations-Lock-on/Debug-Verfolgung und die ursprünglichen aktionsgesteuerten Crouch-Routinen sind damit nicht vollständig portiert. Die geprüfte `PlWepLockCtrl`-Routine setzt die kommentierten Ready-Turn-Unterzustände nicht selbst; ein Kommentar allein wird nicht als aktivierender Quellcode behandelt. Ein vollständiger Android-Spieltest ist weiterhin erforderlich.

## Explizite frühe Waffen-Sounds

`drainWeaponSounds()` liefert einmalig die ursprüngliche Block-/Cue-Nummer und die tatsächliche Teileposition zum Zeitpunkt des `SndCall`. Der Raumadapter spielt diese Ereignisse ab und entfernt seine vorherigen Draw-/Knife-/Ready-End-Duplikate. Die Besitzerqueue bleibt getrennt von den tatsächlich dekodierten SEQ-Soundbytes und deren ursprünglichem `SourceFootSoundCue`-Pfad.

| Erreichbarer trockener r100-Pfad | Ursprünglicher Aufrufzeitpunkt und Besitzer |
|---|---|
| Handgun-/Shotgun-Draw | Block1/Cue0x28, Parts0, Seq_frame2 vor MotionMove |
| Shotgun-Ready-End | Block2/Cue9, Parts0xA, Seq_frame10 nach MotionMove |
| Nicht nachladbarer leerer Handgun-Abzug | Block2/Cue0x17, Parts4, tatsächlicher Fire-Trigger |
| Nicht nachladbarer leerer Shotgun-Abzug | Block2/Cue3, Parts4, tatsächlicher Fire-Trigger |
| Letzte Shotgun-Patrone, Aim und Fire weiterhin gehalten | Block2/Cue3, Parts4, Pump-Ende Seq_frame40 |
| Messerschwung | Block1/Cue3, Parts4, ursprünglicher Mot_frame3-Cross vor MotionMove |

Der leere Abzug klickt nur bei einem neuen Trigger. Gehaltener Trigger wiederholt diesen Sound nicht; nachladbare leere Waffen wechseln ohne Leerklick zur ursprünglichen automatischen Nachladeaktion. Die letzte Shotgun-Patrone behält zusätzlich beide ursprünglichen Held-Input-Bedingungen. Die Messermotion besitzt beim Kurven-Cross3 bereits Seq_frame4; der Adapter verwechselt diese beiden Framearten nicht.

Die Prüfung vergleicht die gespeicherte Soundposition mit den unabhängigen tatsächlichen Körpermatrizen vor beziehungsweise nach dem ursprünglichen Motionaufruf. Sie deckt alle drei frühen Waffenprofile bei30/60/120 Renderframes pro Sekunde ab, prüft die native Tickzuordnung, Queue-Verbrauch, Leer-/Nachlade-/Pumpbedingungen und erzeugt hörbares PCM aus den tatsächlichen PL00/WEP01/WEP02/WEP07-Soundbanken.

Die Quellenprüfung zeigt keinen expliziten Sound beim trockenen Messer-Wegstecken. Block1/Cue0x51 gehört zum Wasserspritzer bei einem Messerschwung mit Handkontakt und wird nicht als erfundener Sheath-Sound abgespielt. Der Draw-Sonderwert0x29 gehört zu `m_Work2==1` aus dem Wasser-/Schadenspfad; der hier übertragene trockene Standardpfad verwendet den ursprünglichen Wert0x28.

Die Augen- und Eventteile-Setter verändern nur `cCoord::matCalc()` des betroffenen Originalteils und bauen danach Weltmatrizen einschließlich der bestehenden Waist-Korrektur auf. Sie rekonstruieren keine unveränderten lokalen Motionmatrizen: Dadurch bleiben deren Hokan-Blends erhalten. Der unabhängige Soundpositionsvergleich hat diese Grenze nachgewiesen; die abschließenden Augen-, RNG- und FCV-Prüfungen bestehen mit dieser Implementierung.
