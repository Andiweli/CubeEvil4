# M16 — Original-Leon mit Animationen und Kamerakollision

Voraussetzung: vollständiges Projekt bis **M15**. Dieses ZIP enthält nur vollständige neue/geänderte Dateien. Den enthaltenen Ordner **RE4-Android-Decomp** über den bisherigen Projektordner kopieren. In Android Studio mit JDK 17 `clean assembleDebug` bauen und mit demselben Signaturschlüssel über die bisherige App installieren. VersionCode **16**, VersionName **0.16.0-dev**, Paket **com.ast.re4decomp**, ARM64/API21/GLES3. Vorhandene Disc-Imports bleiben verwendbar.

## Der größere Schritt

- Original-Leon aus dem bereits importierten `em/pl00.drs`: normales Kostüm, Kopf, Haare, Augen und Hände. **119 Gelenke, sieben Modellbestandteile, 9.096 Dreiecke, elf Originaltexturen.**
- Stillstehen, Gehen und Laufen aus den originalen FCV-/SEQ-Dateien in `em/wep01.drs`. Die eigentliche Kurven-, IK-, Gelenk- und Übergangsauswertung stammt aus dem Originalcode und wurde für native 64-Bit-Zeiger angepasst. Keine PowerPC-Ausführung.
- GPU-Skinning: Raum, Texturen und Modellgeometrie bleiben auf der GPU; pro Animationsframe wird der 8-KiB-Gelenkblock aktualisiert. Positionen und Normalen behalten ihre getrennten Gewichtstabellen.
- Die Followkamera prüft jetzt eine Kugel gegen **Wände, Böden und Schrägen**, mit dem originalen SEE-Attributfilter. Bei sehr kurzem Abstand wird Leon ausgeblendet, damit sein Körper den Blick nicht vollständig verdeckt. Die Kamera wird dafür nicht durch ein Hindernis verschoben.
- Die Raumansicht benutzt die fünf belegten originalen Blendmodi, unterstützte OT-Kategorien, die SMX-Alpha-Grenzwertüberschreibung und echte UV-Geschwindigkeiten. Eine Alphamaske erzwingt kein Blending. Das behebt belegte Fehler im bisherigen Renderer; es ist keine Zusage, dass sämtliche sichtbaren Lücken jedes Raums verschwunden sind.

## Steuerung und Test

| Standardcontroller | Funktion im aktuellen Test |
| --- | --- |
| A / PlayStation Kreuz | Leon-Test ein-/ausschalten |
| Linker Stick | Bewegen, abhängig von der Kamerarichtung |
| B / PlayStation Kreis halten | Laufen |
| Rechter Stick | Kamera drehen/kippen |
| LT / RT | Kamera weiter weg / näher heran |
| Y / PlayStation Dreieck | Texturen / Kollisionsansicht |
| SELECT / Back / View | Normalenansicht |
| Start / Menu | Teststartpunkt zurücksetzen |
| LB / RB | Basis-Modell wechseln, nur außerhalb des Bewegungstests |

1. Zuerst **r300** öffnen und A/Kreuz drücken. Statt der Drahtkapsel erscheint Leon. Der Fußmarker bleibt zur Bodenprüfung sichtbar. Leon benötigt den vorhandenen **Disc-1-Import**, auch in Räumen von Disc 2.
2. Stillstehen, Gehen, Laufen und Anhalten prüfen. An Wänden müssen Bewegung und Laufanimation anhalten, sobald keine tatsächliche Bewegung mehr stattfindet. Es wird kein Nintendo-Controller vorausgesetzt.
3. Die zuvor auffälligen Stellen am Boden/Wasser erneut ansehen. Kamera nach unten und oben drehen und näher heranführen. In engen Bereichen kann die Kamera stark verkürzt werden; der Status erklärt das Ausblenden des Modells.
4. Y/Dreieck und SELECT umschalten. Danach müssen Texturen, Masken und Leon weiter dargestellt werden. Mit Start zurücksetzen; mit A/Kreuz zur bisherigen Modellansicht wechseln.
5. Controller trennen/neu verbinden, App pausieren und wieder öffnen. Position und Animation dürfen keine lange Pause nachholen. Anschließend r100 und weitere Räume prüfen.

Fehlt die erwartete Disc-1-Datei oder stimmt ihre Größe/SHA-256 nicht, bleibt die bisherige Kapseldiagnose verfügbar. Keine neue Extraktion ist notwendig, wenn beide originalen Importdateien vorhanden sind.

Für den nächsten Abgleich bitte **Logcat einschließlich RE4Player, RE4Walk, RE4Scene, RE4Collision und RE4Room** sowie den betroffenen Raumnamen/Stelle senden. RE4Player protokolliert das Laden und die Wechsel Stand/Gehen/Laufen. Für verbliebene Durchsicht hilft zusätzlich ein Screenshot des Kamerawinkels.

## Stand gegenüber dem fertigen Spiel

Dies ist ein animierter Bewegungsprototyp in Originalräumen. Originaler Raum-Spawn, vollständige `cPlayer`-Zustandsmaschine, Kampf, Waffenmodell, Gegner, Türen/Szenario, Inventar, Audio und Speichern fehlen. Die Hände verwenden bereits die Original-Griffmodelle, halten aber noch keine gerenderte Waffe. Raumbeleuchtung, Schatten, dynamische Objekte und vollständige Laufzeit-Sichtbarkeit sind weiterhin offen. Die Kollisionsbewegung ist noch die M15-Testphysik.

Der nächste Schritt zum tatsächlichen Gameplay ist die native Spieler-/Raumzustandslogik mit den originalen Start- und Interaktionsdaten. M16 liefert dafür erstmals die geprüfte animierte Figur. Herkunft, Portierungsdetails und verbleibende Renderergrenzen: **PLAYER-PREVIEW.md**, **WALK-PROBE.md**, **SMX.md** und **VALIDATION.md**.
