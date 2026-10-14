# K14 – Graue Fokusfläche nach dem Demo-Video

Android-Version **0.37.14-dev**, Versionscode **47**, auf Grundlage von K13.

Der Nutzer bestätigt den korrekten Demo-Rückweg und die übrigen K13-Korrekturen. Nach dem ersten weiteren DPAD-Tastendruck wurde die Menüansicht erneut grau.

`TITLE_MENU_MOVE` im Originalcode setzt nur die Timer der ausgewählten Menüschrift zurück. Auch der vorhandene Android-Menüablauf setzt bei DPAD lediglich den Cursor-/Leerlaufzähler zurück. Die ursprüngliche Titelanimation und ihre Helligkeitskurven laufen weiter. Die drei ausgewählten Schriftflächen sind jeweils kleine Textquads und können keine bildschirmfüllende graue Fläche zeichnen.

Die Android-Titelansicht war fokussierbar und verwendete die standardmäßig eingeschaltete Fokusmarkierung. Der Android-Code zeichnet diese Markierung nach `onDraw` über die volle Fläche der fokussierten View. Ein DPAD-Tastendruck kann den Touch-Modus verlassen und diese Markierung aktivieren. Dieser zusätzliche Android-Zeichenpfad erklärt das erneute Aufhellen nach dem Tastendruck; eine direkte Aufzeichnung dieses Fokuszustands vom Gerät liegt nicht vor.

Die Korrektur ruft `setDefaultFocusHighlightEnabled(false)` im Konstruktor der Titelansicht auf, bevor diese erstmals Fokus erhält. Der Aufruf wird für API 26 und höher ausgeführt; ältere Android-Versionen verwenden den bisherigen Zeichenweg. Die originale Menüschrift übernimmt weiterhin die sichtbare Auswahl.

Referenzen: [Android View-API](https://developer.android.com/reference/android/view/View#setDefaultFocusHighlightEnabled(boolean)), [AOSP: Einführen der Fokusmarkierung einschließlich Touch-Modus und View-Grenzen](https://android.googlesource.com/platform/frameworks/base/+/18d993bb72306bcdff8744d9b717247f0747637f%5E1..18d993bb72306bcdff8744d9b717247f0747637f/) und [AOSP: Fokusmarkierung am Ende des View-Zeichenablaufs](https://android.googlesource.com/platform/frameworks/base/+/6dea70b9fafcb36ec9da1e19afae89c491b965d8%5E!/core/java/android/view/View.java).

## Prüfung und Installation

Die vorhandenen Java-Startablaufprüfungen (1.378) und Titel-/Filmpräsentationsprüfungen (123) bestehen. Der ARM64-Android-Build ist erfolgreich. Der tatsächliche APK-Inhalt, der deaktivierte Fokus im Titelansicht-Konstruktor samt API-26-Abfrage, Signaturen V1/V2 und die 16-KiB-Ausrichtung sind geprüft. Die native Bibliothek ist bytegleich mit K13; auch die nativen Quellen, Originalgrafiken und Java-Menüabläufe sind byteweise verglichen. Lint konnte in dieser Offline-Umgebung nicht ausgeführt werden, weil `com.android.tools.lint:lint-gradle:31.9.2` nicht im Abhängigkeitscache liegt. Der maschinenlesbare Bericht steht in [K14-PRUEFUNGEN.json](K14-PRUEFUNGEN.json).

Ein Android-Gerätetest von K14 wurde hier nicht ausgeführt. Zur Geräteprüfung beide Demo-Videos beenden und anschließend mehrfach DPAD DOWN/UP drücken, einschließlich eines Wechsels zwischen Touch und Controller.

`RE4-M37-K14.apk` verwendet dasselbe Signaturzertifikat wie K13 und kann darüber installiert werden. Ein erneuter DVD-Import ist nicht erforderlich.

`RE4-Android-Decomp-R100-Korrekturen14.zip` enthält die vollständigen aktuellen Ersatzdateien K5–K14 einschließlich `FILES-K14.sha256`. Den enthaltenen Projektordner mit dem vorhandenen Projekt zusammenführen und die entsprechenden Dateien ersetzen. `RE4-Android-Decomp-K14-Quellstand.zip` enthält alternativ das vollständige Projekt. Die Archive enthalten ganze Dateien; private Signaturdateien und lokale Build-Ausgaben sind ausgeschlossen.
