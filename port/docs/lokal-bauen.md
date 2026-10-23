# Lokalen Stand mit GitHub abgleichen und die App bauen (Windows-PC)

Wunsch des Projektinhabers (22.10.2026): Jede Sitzung schickt nach Änderungen, die in die App
gehören, diese Befehle mit. Ordner: Repository `E:\Development\Android\RE4-Fixed`, Branch
`claude/re4-start-controls`.

## 1. Stand von GitHub holen (PowerShell)

```powershell
cd E:\Development\Android\RE4-Fixed
git status
git pull --no-rebase origin claude/re4-start-controls
```

- `git status` zeigt geänderte Dateien (`modified: …`)? Dann arbeitet gerade ein anderer Chat in
  diesem Ordner. Ihn bitten, seine Änderungen zu committen, mit `git pull --no-rebase origin
  claude/re4-start-controls` zu mischen und zu pushen; danach `git pull` hier noch einmal.
  Unversionierte Dateien (`Untracked files: AGENTS.md, core/, …/src/core/assets/`) stören nicht.
- Steht beim `git pull` **CONFLICT** in der Ausgabe: nicht weitermachen, die Ausgabe an den Chat
  schicken.

## 2. Den Spielkern bauen (PowerShell, läuft in WSL)

```powershell
wsl --exec bash -lc "if [ ! -d ~/re4-build/.git ]; then git -c safe.directory='*' clone -q /mnt/e/Development/Android/RE4-Fixed ~/re4-build; fi && cd ~/re4-build && git -c safe.directory='*' fetch -q /mnt/e/Development/Android/RE4-Fixed HEAD && git checkout -q --detach FETCH_HEAD && git log -1 --format='Stand: %h %s' && port/tools/whole_build_android.sh --into /mnt/e/Development/Android/RE4-Fixed/port/android-decomp/app --abi all"
```

- Baut den Kern aus dem Stand, den Schritt 1 geholt hat, in einem eigenen WSL-Ordner
  (`~/re4-build`): der Raum-Rundgang (`~/re4-kern`) wird nicht gestört. Das erste Mal 10–60
  Minuten, danach nur das Geänderte (ohne Änderung am Kern wenige Minuten).
- Die erste Zeile `Stand: …` nennt den Commit; er muss der sein, den der Chat genannt hat.
- Gut ist: zweimal `0 failed build steps` und zweimal `Kern-Bibliotheken …`. Steht dort
  **FEHLER**, die Ausgabe an den Chat schicken.
- Immer ausführen, auch wenn sich nur die App geändert hat: ohne Änderung ist er schnell fertig.

## 3. App bauen und auf den Retroid bringen

In Android Studio wie gewohnt bauen (Variante „core“) und auf den Retroid installieren (Run mit dem
verbundenen Retroid). Laufen gerade Gerätetests eines Chats über ADB, erst danach installieren
(`port/docs/adb-coordination.md`).

## Anderer Branch (z. B. `performance`)

Arbeitet ein Chat auf einem anderen Branch (seit 23.10.2026 `performance`: Grafik auf mehreren
Kernen, kleinere Uniform-Uploads), in Schritt 1 einmal auf diesen Branch wechseln und dann von dort
holen; Schritt 2 und 3 bleiben gleich (der Kern-Build nimmt den Stand, der im Ordner ausgecheckt
ist):

```powershell
cd E:\Development\Android\RE4-Fixed
git status
git fetch origin performance
git switch performance
git pull --no-rebase origin performance
```

Zurück zum bisherigen Stand: `git switch claude/re4-start-controls`.

Seit 0.45.0 (23.10.2026) steckt `performance` vollständig in `claude/re4-start-controls`. Wer noch
auf `performance` steht, wechselt mit `git switch claude/re4-start-controls` zurück und holt dann
wie in Schritt 1.
