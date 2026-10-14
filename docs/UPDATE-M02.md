# M02 installieren und prüfen

Die im ZIP enthaltenen vollständigen Dateien über den vorhandenen Projektordner
**RE4-Android-Decomp** kopieren und ersetzen. Es muss nichts gelöscht werden.
Danach Gradle synchronisieren und die App erneut bauen/installieren.
Paket `com.ast.re4decomp`; ARM64; minSdk 21; Version 0.2.0-dev / Code 2.
Bei gleichem lokalem Android-Signierschlüssel ist ein Update über M01 möglich.
**Die App nicht deinstallieren; vorhandene Disc-Imports bleiben verwendbar.**

1. Im Hauptmenü „Yz2-Raumdaten prüfen“ öffnen.
2. „Prüfung starten“ drücken; r100/Disc 1 und r300/Disc 2 sollen jeweils PASS melden.
3. Grafik-/Controller-Test öffnen. Beide Sticks und LT/RT betätigen; Werte müssen sich ändern.
4. Gerätename/ID prüfen. Echte Stickgeräte haben Vorrang vor virtueller Navigation.
   Ein zweiter gleichrangiger Controller darf die Eingabe nicht übernehmen.
5. Test verlassen/wieder öffnen und Controller trennen/wieder verbinden.

Bitte bei Fehlern die vollständige Statusanzeige und Logcat schicken.
Der Decoder-Test zeigt noch keinen Raum. Nächster Engine-Baustein ist die begrenzte
Dateitabelle der entpackten Raumarchive, anschließend die Modell-/Displaylist-Verarbeitung.

Enthalten: neuer nativer Yz2-Decoder, Originalvergleichskatalog, Android-Raumprüfung,
korrigierte Controller-Auswahl, vollständige neue/geänderte Tests und Dokumentation.
Originaldiscs, extrahierte Assets und APK sind nicht Bestandteil dieses Quellcode-Updates.
