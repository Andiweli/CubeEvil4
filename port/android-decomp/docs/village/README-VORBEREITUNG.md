# Vollständige Vorbereitungsdateien für den Dorf-Ausbau

Aktueller Spielstand: [K15-DORF.md](../K15-DORF.md). Die folgenden Angaben beschreiben das frühere reine Vorbereitungspaket; dessen Audit bleibt als unveränderter Ausgangsbefund erhalten.

Basis dieses Vorbereitungspakets: unveränderter K14-Quellstand, `0.37.14-dev` / VersionCode 47.

Dieses Paket enthält ganze neue Dateien für Planung, Inventur und wiederholbare Originaldatenprüfung. Den enthaltenen Ordner `RE4-Android-Decomp` mit dem bestehenden K14-Projekt zusammenführen. Es werden nur `docs/village/`, `tools/audit_village.py` und die zugehörige Prüfsummenliste ergänzt.

- `DORF-AUSBAU-PLAN.md`: Umfang, Laufzeitlücken, 29 Raum-Arbeitspakete, Umsetzungsfolge und Abnahme.
- `VILLAGE-AUDIT.md` und `VILLAGE-ROOMS.tsv`: lesbare Bestandsübersicht.
- `VILLAGE-TRANSITIONS.tsv`: alle 66 gerichteten AEV-Türdatensätze mit Parts und Originalzielen.
- `VILLAGE-SOURCE.tsv`: Quellreferenzen zu Szenario, Kollision, Gegnern, Objekten, Flags und weiteren Abhängigkeiten.
- `VILLAGE-AUDIT.json`: detaillierte Dateninventur und native statische Prüfergebnisse.
- `VILLAGE-PREPARATION-VALIDATION.json`: Abschlussprüfung dieser Vorbereitung.
- `tools/audit_village.py`: vollständiges Auditwerkzeug; Aufruf und Voraussetzungen im Ausbauplan.

Die vollständigen Original-DVD-Dateien, APKs, Build-Ausgaben, Zugangsdaten und Signiermaterial sind nicht enthalten. Die Vorbereitung verändert das Spiel nicht. Eine Fertigstellung der Räume wird erst nach den im Plan getrennt geführten Implementierungs-, Laufzeit-, Geräte- und Originalprüfungen bestätigt.
