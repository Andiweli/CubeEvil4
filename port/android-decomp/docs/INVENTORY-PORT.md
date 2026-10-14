# M32: Koffer und Vorräte

`InventoryCase` ist der native Besitzer der Sitzungsgegenstände. Das Android-
Dialogfenster liest nur einen JSON-Snapshot. Auswahl, Ausrüstung, Kombinieren,
Verwendung, Verschieben und Ablegen werden auf dem Spielthread geprüft. Eine
fehlgeschlagene Aufnahme verändert weder einen Stapel teilweise noch die
Packanordnung. Die Welt setzt ihre Aufgenommen-Flags erst nach erfolgreicher
Transaktion.

## Vergleich mit der eingefrorenen USA-Quelle

- `game/puzzle.cpp::piece_info`: sämtliche 58 Gegenstandsformen sind volle
  Rechtecke. Der native Port übernimmt ihre Breite/Höhe, einschließlich
  Schrotflinte 8 × 2, Pistole 3 × 2 und Munitionsschachteln 2 × 1.
- `pzlPlayer::init`: Koffergrößen 10 × 6, 11 × 7, 12 × 8 und 15 × 8.
- `PutInCase`: bestehende Munitionsstapel werden in absteigender Füllung ergänzt.
  Für neue Teile wird zuerst ohne Drehung zeilenweise gesucht und dann um 90°
  gedreht. Die Portschnittstelle unterstützt zusätzlich ganze Mengen über
  mehrere Schachteln, ohne ursprüngliche Stapelobergrenzen zu überschreiten.
- `game/item.cpp::wep_info`: 43 Zeilen für die ursprünglichen Munitions-IDs und
  Magazinkapazitäten der Grundstufe. Ausrüstung ist auf die tatsächlich
  integrierten Pistolenprofile und die Dorfschrotflinte beschränkt.
- `cItemMgr::construct`: aufgenommene Waffen beginnen mit vollem Grundmagazin;
  die Dorfschrotflinte mit sechs Patronen.
- `cItemMgr::reload`/`minimumSearch`: beim Nachladen werden zunächst die kleinsten
  Reserve-Munitionsstapel aufgebraucht. Das native Magazin bleibt getrennt von
  der im Koffer verpackten Reserve.
- `combination_info`: alle 77 Rezeptzeilen sind übernommen. Die ausführbare
  Verbindung unterstützt gleiche Munitionsstapel, Kräuter und passende
  Schätze; Waffenanbauten und alternative Munition benötigen ihren eigenen
  Waffenport und werden nicht vorgetäuscht.
- `cItemMgr::set_game`: Startpistole 0x23 mit zehn Patronen, 20 Reservepatronen,
  Erste-Hilfe-Spray und Startgegenstand 0x7c. Die gespeicherten Halbzellenzentren
  (2,1), (7,0), (6,3) entsprechen den nativen Rechteckursprüngen (0,0), (3,0)
  und (3,1). Die alte Testbeladung 10+60 bleibt eine getrennte Testfunktion.
- `cItemMgr::use`: volle Gesundheit verhindert den Verbrauch von Heilmitteln.
  Das gilt auch für den vom Benutzer verlangten Modus mit unendlicher HP.

## Bedienung

Steuerkreuz oder linker Stick wählen den Gegenstand. A/Kreuz öffnet Aktionen.
Y/Dreieck nimmt einen Gegenstand zum Verschieben auf; R1/RB dreht ihn, A/Kreuz
bestätigt den gewählten Platz. L1/LB wählt den ersten Kombinationspartner, danach
wählt man den zweiten und bestätigt mit A/Kreuz. B/Kreis verwirft den laufenden
Schritt oder schließt den Koffer. Select/View wechselt zwischen Koffer und der
Liste außerhalb des Koffers. Die sichtbaren Schaltflächen sind zusätzlich
anklickbar.

## Noch nicht vollständig portiert

Die Android-Darstellung zeigt die echten Packmaße und spielwirksamen
Transaktionen, verwendet aber noch nicht die originalen GX/ID-Widgets,
3D-Gegenstandsmodelle, Menügrafiken oder Nachrichtentexte aus `Sscrn`. Das
6 × 12 Ausweichbrett mit originalem Tauschen, Waffenanbauten, gelbe-Kraut-
Maximalgesundheitssteigerung sowie kampagnenspezifische Schlüssel- und
Dokumentereignisse sind eigene Folgearbeiten. Gelbe Heilmixturen werden bis
zur Übernahme der Maximalgesundheitszustände nicht verbraucht. Ein Neustart der
App ist noch kein Laden eines originalen Spielstands; die Kofferverwaltung
beschreibt den laufenden Testsitzungszustand.

## Prüfung

`tools/check_inventory_source.py` vergleicht Formen, Kapazitäten/Munitions-IDs
und Rezepte direkt mit der mitgelieferten Originalquelle. Die nativen
`inventory_case_tests` prüfen Startanordnung, Aufnahme bei vollem Koffer,
fehlgeschlagene Teilstapeltransaktionen, Waffenwechsel, Magazine/Reserve,
Kräuter, außerhalb des Koffers geführte Gegenstände und 5.000 gemischte
Operationen auf Grenzen und Überlappungen. Die Tests wurden zusätzlich mit
AddressSanitizer und UndefinedBehaviorSanitizer ausgeführt.

`tools/test_m32_inventory_runtime.cpp` prüft zusätzlich den produktiven
JNI-/GLES-Pfad mit den lokal extrahierten Originaldaten: den A/Kreuz-Fund der
Dorfschrotflinte, Abbrechen und Bestätigen samt Quellenflags, veraltete
Dialogantworten, exakte Bildgleichheit beim EGL-Neuaufbau eines offenen
Funddialogs, Ausrüsten aus der Zielhaltung im pausierten Koffer, CPU-Aktionen
ohne EGL-Kontext sowie echtes Feuern und Nachladen mit einer einzigen
Munitionsverwaltung. Ein dokumentierter Kompatibilitätsfall verwendet die
unveränderten Original-ITA-Datensätze ohne die optionale ITM-Modellsektion;
der Dokumentfund benötigt dadurch kein sichtbares Modell. Die erhaltenen
Gegenstände bleiben beim Raumwechsel r101 → r100 → r101 erhalten. Diese
627 Laufzeitprüfungen wurden auf dem Host mit Mesa ausgeführt; sie ersetzen
noch keinen Android-Gerätetest.
