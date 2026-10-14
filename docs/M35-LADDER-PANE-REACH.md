# M35: Leitertritt durch ein intaktes Fenster

## Korrigierter Portpfad

`RoomLadders::offer` behält die ursprüngliche lokale Reichweitenbox,
Halbdrehungsabfrage, die tippezifische X/Z-Ausrichtung und die Original-FCVs.
Vor einem Leitertritt prüft es jetzt die automatische Ausrichtung und den
Weg vom Spieler zum Leitertop gegen die aktuelle gemeinsame Objekt-SAT.
Dabei verwendet es dieselben Körpermaße wie die Objektkollision des
Spielers. Eine intakte oder nur angeschossene Scheibe verhindert einen
Tritt durch die Scheibe. Nach ihrem wirklichen Bruch entfernt der nächste
Welt-Tick die Scheibe aus dieser Kollisionsschicht; derselbe Tritt ist dann
wieder erlaubt. Türen und Möbel in dieser Schicht bleiben physische
Hindernisse. Die unveränderliche EAT wird dafür nicht benutzt: Ihre
statischen Effektflächen beschreiben nicht den aktuellen Glaszustand.

`RoomLadders::setInteractions` bindet den bestehenden Interaktionsbesitzer
als geliehenen Zeiger. Der Produktionsaufbau muss diese Bindung neben der
bereits vorhandenen umgekehrten `RoomInteractions::setLadders`-Bindung
setzen. Es wird kein zweiter Kollisionsbesitzer und kein Glaszustand in
der Leiter dupliziert.

## Was der Originalcode tatsächlich prüft

- `obj13.cpp::objLadderDownActEvtCk` (907–959) prüft stehende Leiter,
  Player-Routine, Blickrichtung und lokale Grenzen (X ±800, Z −1000 bis
  +500, Y ±500). Es registriert den Leitertritt in ActBtn-Slot 5.
- `pl_class.cpp::cPlayer::actionSelect` (605–624) registriert
  Fenstersprünge in Slot 2. `act_btn.cpp::cActionButton::move` verarbeitet
  den höchsten Slot zuerst. Diese Reihenfolge bleibt erhalten.
- `obj13.cpp::plobjLadderDown` (974–1009) setzt vor der Trittanimation X/Z
  auf Leitertop plus −700 lokale Z-Einheiten; anschließend wird die
  Spielerkollision ausgeschaltet.
- `obj13.cpp::LadderNearCk` und `emwindow.cpp::ChkWindow` reservieren ein
  nahes Fenster bei einer stehenden Leiter. Die Quellregel bleibt
  unverändert; die Korrektur erfindet keinen zusätzlichen Fenstersprung.
- `be_flag & 4` / `ACTCTR_NO_EXEC` sperrt den Leitertritt während einer
  Partneraktion. Es ist keine Glassperre. Die DownActEvtCk-Quellfunktion
  enthält selbst keinen Raycast und keine Abfrage auf intaktes Glas.
- Die neue Erreichbarkeitsprüfung ist deshalb ausdrücklich eine Korrektur
  des Port-Akteur-/Kollisionsadapters. Sie wird nicht als zusätzliche
  Original-Quellbedingung dargestellt. Das ist keine Behauptung, die
  gesamte originale cAtari-/Partner-/Kampagnenausführung sei portiert.

## Regression mit tatsächlichen Originaldaten

`tools/test_m33_context_actions.cpp::intactLadderPane` lädt das unveränderte
Original-r101-Archiv (SAT, EAT, ETS und ETM) sowie PL00/WEP01-DRS. Es prüft
die obere Scheibe ETS0 und die Luke-/Hausleiter ETS7:

1. Der reale Ansatz steht auf der Original-SAT und erfüllt die
   ursprünglichen lokalen Leitergrenzen.
2. Die Original-Objekt-SAT trennt Leons Körper vom Leitertop. Mit
   intakter Scheibe gibt es weder Angebot noch ausführbaren Tritt; HP,
   ETC-Flag und Spieleraktion bleiben unverändert.
3. Ein Pistolenschuss senkt die Original-HP von 1000 auf 500. Die
   angeschossene Scheibe bleibt eine Trittbarriere.
4. Der zweite getrennte Pistolenschuss bricht die Scheibe. Nach dem
   Original-Welt-Tick funktioniert exakt derselbe Ansatz mit gleicher
   Ausrichtung; die echte Tritt-FCV endet, und es entsteht insgesamt nur
   ein Glasbruchereignis.
5. Die normale Dachleiter ETS5 erhält ihren gültigen Tritt auf derselben
   Seite ohne trennende Scheibe auch mit gebundenem Live-Besitzer.

Der übrige Originaldaten-Test läuft mit dieser Bindung ebenfalls weiter:
Leiteraufstieg, Kippen und Aufstellen in r101/r400, unabhängige FCV-/SEQ-
Posen, Original-SAT-Fences und 24 wiederholte Fensterdurchgänge durch
bereits zerbrochene Erdgeschoss-/Obergeschossfenster.

Private fokussierte Verifikation: `m33_context_actions_check` **9093
Prüfungen bestanden**; `context_actions_tests` **bestanden**. Das
Produktions-APK und die JNI/GLES-Regressionssuite werden durch den
integrierenden Hauptagenten separat gebaut und geprüft. Hier wurde kein
Android-Gesamtbuild ausgeführt.

Unverändert begrenzt bleibt der bekannte Leiterbesitzer auf die bereits
geprüften r101-/r400-Initialisierungen. Diese Korrektur behauptet keine
neue Unterstützung anderer Raumskript-Leiter, der Partnersteuerung oder
der vollständigen Original-Aktionsmaschine.
