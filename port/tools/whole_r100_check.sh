#!/usr/bin/env bash
# Lokale Prüfung des Spielkerns mit den Disc-Daten (WSL2 oder Linux). Started
# by port/scripts/test-r100-core.ps1 on Windows, or directly:
#
#   port/tools/whole_r100_check.sh --data DIR [--room 100] [--frames 10000]
#       [--out DIR] [--source REPO] [--work DIR] [--arm] [--no-build]
#
# --data DIR   the disc data, in one of these forms:
#              - the port's folder (disc0/sys, disc0/files, disc1/...),
#                as port/tools/whole_disc_folder.py writes it
#              - one extracted disc (sys/ and files/, Dolphin "Extract Entire Disc")
#              - the app's import folder (disc1/..., disc2/...)
#              - a folder with the disc images (*.iso, *.gcm): laid out once
#                into WORK/disc (never into the repository)
# --source     a checkout to build from (the Windows checkout under /mnt/c):
#              cloned into WORK/RE4 at its current commit, so the build runs on
#              the Linux file system; default: this script's checkout
# --work       working folder, default ~/re4-kern
# --out        where the report and the logs go, default WORK/runs/<time>
# --arm        also build ARM32 and run the game under qemu-arm (slow)
#
# Steps: build and the cloud checks (whole_build.sh), the format comparison
# with the disc files (re4_format_compare --data), then the game from the
# title screen: debug start into the room (RE4_START_ROOM, patch 0026), the
# test script presses UP/A/START on the title screens until the room loads
# (RE4_PAD_UNTIL_ROOM), the opening event and the radio call end on their own,
# then Leon walks a few steps (RE4_PAD_FREE_SCRIPT, once the player is free),
# and the run ends after FRAMES frames of the main loop
# (RE4_STOP_AFTER). The report (bericht.txt) lists: crash or not, frames,
# GetDataExt coverage lines with "nein", converter messages, called stubs,
# the last state trace line. Then every movie of the discs through the CRI
# middleware (re4_host --movie-test): status, frames, stills, the opening
# movie's sound. Then the memory card (M10): a system file made at the boot
# check, a save in r100, a new start that loads it (run_memcard). Exit code 0
# when everything passed.
set -u

room=100
frames=10000
data=""
out=""
source=""
work="$HOME/re4-kern"
arm=0
build=1
while [ $# -gt 0 ]; do
    case "$1" in
    --data) data="$2"; shift 2 ;;
    --room) room="${2#r}"; shift 2 ;;
    --frames) frames="$2"; shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --source) source="$2"; shift 2 ;;
    --work) work="$2"; shift 2 ;;
    --arm) arm=1; shift ;;
    --no-build) build=0; shift ;;
    *) echo "unbekannte Angabe: $1" >&2; exit 2 ;;
    esac
done
if [ -z "$data" ]; then
    echo "Aufruf: $0 --data ORDNER [--room 100] [--frames 10000] [--out ORDNER] [--source REPO] [--arm]" >&2
    exit 2
fi
room=$(echo "$room" | tr 'A-F' 'a-f')
mkdir -p "$work"
stamp=$(date +%Y%m%d-%H%M%S)
run="$work/runs/$stamp"
mkdir -p "$run"
report="$run/bericht.txt"
: > "$report"
say() { echo "$@" | tee -a "$report"; }
section() { say ""; say "== $* =="; }
overall=0

# ---- source ------------------------------------------------------------------------
if [ -n "$source" ] && [ -d "$source/.git" ]; then
    # the checkout's current commit, cloned once and then fetched
    src="$work/RE4"
    git_s() { git -c safe.directory='*' "$@"; }
    if [ ! -d "$src/.git" ]; then
        echo "Quelltext wird nach $src kopiert (einmalig) ..."
        rm -rf "${src:?}"
        git_s clone -q "$source" "$src" || { echo "git clone von $source fehlgeschlagen" >&2; exit 1; }
    fi
    git_s -C "$src" fetch -q "$source" HEAD || { echo "git fetch von $source fehlgeschlagen" >&2; exit 1; }
    git_s -C "$src" checkout -q --detach FETCH_HEAD || exit 1
elif [ -n "$source" ]; then
    # a folder without git (a downloaded ZIP): copied as it is, without builds
    src="$work/RE4-copy"
    echo "Quelltext wird nach $src kopiert ..."
    rm -rf "${src:?}"
    mkdir -p "$src"
    (cd "$source" && tar --exclude=./port/build -cf - .) | (cd "$src" && tar -xf -) || exit 1
else
    src="$(cd "$(dirname "$0")/../.." && pwd)"
fi
commit=$(git -C "$src" -c safe.directory='*' log -1 --format='%h %s' 2>/dev/null || echo "(ohne git: $source)")
# parallel compile jobs: about 1.5 GB of memory each (clang on the big units)
mem_kb=$(awk '/MemTotal/ {print $2}' /proc/meminfo)
jobs=$((mem_kb / 1500000))
[ "$jobs" -gt "$(nproc)" ] && jobs=$(nproc)
[ "$jobs" -lt 1 ] && jobs=1
export RE4_JOBS="$jobs"

say "RE4-Kern: lokale Prüfung mit Disc-Daten, Raum r$room"
say "Zeit:    $(date '+%Y-%m-%d %H:%M:%S')"
say "Stand:   $commit"
say "System:  $(uname -srm), $(nproc) Kerne, $((mem_kb / 1048576)) GB Speicher, $jobs Build-Jobs"
say "Daten:   $data"

# ---- disc data -------------------------------------------------------------------------
layout=""
kind=""
game_ok=1
if [ -f "$data/disc0/sys/fst.bin" ]; then
    layout="$data"; kind="Port-Ordner (disc0, disc1)"
elif [ -f "$data/sys/fst.bin" ] && [ -d "$data/files" ]; then
    layout="$data"; kind="eine entpackte Disc (sys, files)"
elif [ -d "$data/disc1/files" ]; then
    # the app's import folder counts the discs from 1
    links="$work/disc-links"
    rm -rf "${links:?}"
    mkdir -p "$links"
    ln -s "$data/disc1" "$links/disc0"
    [ -d "$data/disc2" ] && ln -s "$data/disc2" "$links/disc1"
    layout="$links"; kind="App-Import-Ordner (disc1, disc2)"
    if [ ! -f "$data/disc1/sys/fst.bin" ]; then
        game_ok=0
    fi
else
    mapfile -t images < <(find "$data" -maxdepth 2 -type f \( -iname '*.iso' -o -iname '*.gcm' \) | sort)
    if [ ${#images[@]} -gt 0 ]; then
        layout="$work/disc"; kind="Disc-Abbilder: ${images[*]##*/}"
        if ! python3 "$src/port/tools/whole_disc_folder.py" --check "$layout" > /dev/null 2>&1; then
            echo "Disc-Abbilder werden einmalig nach $layout entpackt ..."
            args=()
            for i in "${images[@]}"; do args+=(--disc "$i"); done
            rm -rf "${layout:?}"
            if ! python3 "$src/port/tools/whole_disc_folder.py" "${args[@]}" --out "$layout" > "$run/disc-folder.log" 2>&1; then
                say "FEHLER: Disc-Abbilder nicht lesbar:"
                sed 's/^/  /' "$run/disc-folder.log" | tail -20 | tee -a "$report"
                exit 1
            fi
        fi
    fi
fi
if [ -z "$layout" ]; then
    say "FEHLER: In $data liegen weder entpackte Disc-Dateien (sys/, files/) noch Disc-Abbilder (.iso/.gcm)."
    exit 1
fi
say "Form:    $kind"
if [ "$game_ok" -eq 0 ]; then
    say "Hinweis: sys/boot.bin und sys/fst.bin fehlen; der Spiellauf braucht sie (Disc-Abbilder angeben oder"
    say "         mit Dolphin \"Gesamte Disc extrahieren\"). Der Formatvergleich läuft trotzdem."
fi

# ---- build and cloud checks --------------------------------------------------------------
# optimised like the Android build (port/tools/whole_build_android.sh); RE4_OPT=
# (empty) checks the unoptimised build instead
export RE4_OPT="${RE4_OPT-2}"
opt="${RE4_OPT:+-O$RE4_OPT}"
targets=(i686)
[ "$arm" -eq 1 ] && targets+=(armhf)
if [ "$build" -eq 1 ]; then
    section "1. Build und Prüfungen ohne Disc-Daten (whole_build.sh ${targets[*]})"
    if ! command -v clang > /dev/null || ! command -v ninja > /dev/null; then
        say "FEHLER: Werkzeuge fehlen. Einmal ausführen: sudo $src/port/tools/whole_setup_linux.sh"
        exit 1
    fi
    "$src/port/tools/whole_build.sh" "${targets[@]}" 2>&1 | tee -a "$report"
    [ "${PIPESTATUS[0]}" -ne 0 ] && overall=1
fi

# ---- format comparison with the disc files ---------------------------------------------
section "2. Formatvergleich App-Leser gegen Kern-Umwandler mit Disc-Daten"
cmp="$src/port/build/whole-i686$opt/re4_format_compare"
if [ -x "$cmp" ]; then
    "$cmp" --data "$layout" --room "r$room" > "$run/formatvergleich.txt" 2> "$run/formatvergleich.log"
    code=$?
    tee -a "$report" < "$run/formatvergleich.txt"
    # converter messages: what was left undone or left as it is
    notes=$(grep -h "formats:" "$run/formatvergleich.log" |
        grep -E "no converter|incomplete|outside|left |no field list|blocks in its chain|more than|unknown" |
        sed 's/^\[re4\] //' | sort | uniq -c | sort -rn)
    if [ -n "$notes" ]; then
        say "Umwandler-Meldungen beim Vergleich ($(echo "$notes" | grep -c .) verschiedene):"
        echo "$notes" | head -80 | sed 's/^/  /' | tee -a "$report"
    fi
    [ "$code" -ne 0 ] && overall=1
else
    say "FEHLER: $cmp fehlt (Build fehlgeschlagen?)"
    overall=1
fi

# ---- the game: debug start into the room ---------------------------------------------------
# Every 60 frames UP, 15 frames later A, 15 frames later START: the timing of
# the boot test of 13.10.2026 (M4 part B), which got through the memory card
# dialog (UP+A picks "continue without saving", A alone retries). UP keeps the
# title menu on "new game" (patch 0026 puts it there); START skips the logos;
# A or START leaves the debug start menu (title.cpp titleExit). There UP moves
# the cursor (21 lines, wrapping; line 1 loads, 2 forces r120, 0x12 hard mode +
# reset): at most one UP can arrive before the next A/START, so the cursor is
# line 0 or 20, both the normal start. RE4_MEMCARD=none leaves memory card slot
# A empty, so that the boot check shows the "no memory card" dialog this
# timing was made for (and no card file is made in the data folder; since
# milestone 10 the check is the game's own with a real card), RE4_ROOM_MENU=1
# the debug start menu (patch 0043; the default, named because this timing
# needs it).
pad_script="UP@100-60000/60 A@115-60000/60 START@130-60000/60"
# No key in the room until the player is free: the opening event and the
# radio call that follows it must end on their own (the radio waits for its
# voice stream; on the Retroid it hung there, 14.10.2026, and no key worked).
# Once free (no event, no sub screen for 300 frames; "run: player free" in the
# log), Leon walks ahead for 10 s: the report shows how far he got.
room_script=""
# Before walking: the map (Z, as R1 on the Retroid) opens at free+30, a GX
# picture is taken at free+200 (RE4_GX_DUMP_FREE; on the Retroid the map
# stayed black) and B closes it at free+300; then Leon walks from free+450.
# At free+5000 the soft reset of "game over" -> "no" (RE4_TEST_SOFT_RESET;
# on the Retroid the screen stayed black): the title must come back.
free_script="Z@30 B@300 STICKUP@450-750/1"
map_dump=200
soft_reset=5000
# r100 only: once free the player is put in front of the first house, where
# the room script puts him after the s03 event (r100.cpp r100_Sce_look),
# facing the house; the walk then leads into the s03 trigger (the Retroid
# crashed there, 14.10.2026). Starting in r120 (a new game: the opening movie,
# played since milestone 9, here at the pace of the fields, and the car ride)
# ends in r100 as well, the player first free there (use --frames 30000).
# Other rooms: no placing.
place=""
case "$room" in 100 | 120) place="free+1:-82910,860,-38480,1.75" ;; esac

# "Bild N, Spielschritt R, Raum 0xXXX" from the last complete trace line
trace_state() {
    local line f r rm
    line=$(tail -n 2 "$1" 2>/dev/null | head -n 1)
    f=$(echo "$line" | grep -o '"frame":[0-9]*' | cut -d: -f2)
    r=$(echo "$line" | grep -o '"Rno0":[0-9]*' | cut -d: -f2)
    rm=$(echo "$line" | grep -o '"room":"0x[0-9a-fA-F]*"' | cut -d'"' -f4)
    if [ -n "$f" ]; then
        echo "Bild $f, Spielschritt $r, Raum $rm"
    else
        echo "noch kein Bild"
    fi
}

# target, overall limit, limit until the main loop, stall limit (seconds)
run_game() {
    local t="$1" limit="$2" main_limit="$3" stall="$4" dir="$src/port/build/whole-$1$opt"
    local log="$run/spiel-$t.log" trace="$run/trace-$t.jsonl" cov="$run/abdeckung-$t.tsv"
    local bridge="$run/bruecke-$t.jsonl" snaps="$run/schnappschuesse-$t" gxdump="$run/gx-$t"
    local runner=() bad=0
    [ "$t" = armhf ] && runner=(env QEMU_LD_PREFIX=/usr/arm-linux-gnueabihf qemu-arm)
    section "3. Spiellauf r$room ($t): Titelbild, Debug-Start, $frames Bilder ab der Hauptschleife"
    if [ ! -x "$dir/re4_host" ]; then
        say "FEHLER: $dir/re4_host fehlt (Build fehlgeschlagen?)"
        overall=1
        return
    fi
    echo "  (Fortschritt alle 30 s; Abbruch bei $stall s ohne neues Bild, ohne Hauptschleife nach $main_limit s, spätestens nach $limit s)"
    rm -rf "$cov" "$trace" "$trace.gz" "$bridge" "$snaps" "$gxdump" "$run/ton-$t.wav"
    mkdir -p "$snaps" "$gxdump"
    local start=$SECONDS
    (cd "$dir" && exec env RE4_VI_FAST=1 RE4_WAIT_TRACE=1 RE4_START_ROOM="0x$room" RE4_PAD_UNTIL_ROOM=1 \
        RE4_MEMCARD=none RE4_ROOM_MENU=1 RE4_PAD_SCRIPT="$pad_script" RE4_PAD_ROOM_SCRIPT="$room_script" RE4_PAD_FREE_SCRIPT="$free_script" RE4_TEST_PLACE="$place" RE4_STOP_AFTER="$frames" \
        RE4_GX_DUMP_FREE="$map_dump" RE4_TEST_SOFT_RESET="free+$soft_reset" \
        RE4_TRACE="$trace" RE4_COVERAGE_LOG="$cov" RE4_BRIDGE_DUMP="$bridge" RE4_BRIDGE_SNAPSHOT="$snaps" RE4_GX_DUMP="$gxdump" \
        RE4_AUDIO_WAV="$run/ton-$t.wav" RE4_AUDIO_WAV_SECONDS=240 \
        "${runner[@]}" ./re4_host --data "$layout") > "$log" 2>&1 &
    local pid=$!
    # watchdog: progress, stall (no new trace data), no main loop, overall limit
    local size last_size=-1 last_change=$SECONDS last_note=$SECONDS reason="" state
    while kill -0 "$pid" 2> /dev/null; do
        sleep 5
        size=$(stat -c %s "$trace" 2> /dev/null || echo 0)
        if [ "$size" != "$last_size" ]; then
            last_size=$size
            last_change=$SECONDS
        fi
        if [ $((SECONDS - last_note)) -ge 30 ]; then
            echo "  ... $((SECONDS - start)) s: $(trace_state "$trace")"
            last_note=$SECONDS
        fi
        if [ $((SECONDS - last_change)) -ge "$stall" ]; then
            reason="STILLSTAND: seit $stall s kein neues Bild"
        elif [ $((SECONDS - start)) -ge "$main_limit" ] && ! grep -q "run: main loop reached" "$log"; then
            reason="ABBRUCH: Hauptschleife nach $main_limit s nicht erreicht"
        elif [ $((SECONDS - start)) -ge "$limit" ]; then
            reason="ABBRUCH: Zeitlimit von $limit s erreicht"
        fi
        if [ -n "$reason" ]; then
            # backtrace of the running context into the log (host/main.cpp), then stop
            kill -USR1 "$pid" 2> /dev/null
            sleep 3
            kill -KILL "$pid" 2> /dev/null
            break
        fi
    done
    wait "$pid" 2> /dev/null
    local code=$?
    local secs=$((SECONDS - start))
    local stopped reached
    stopped=$(grep -m1 "run: stopped after" "$log" | sed 's/^\[re4\] //')
    reached=$(grep -m1 "run: main loop reached" "$log" | sed 's/^\[re4\] //')
    if [ -n "$reason" ]; then
        say "Ergebnis: $reason ($secs s)"
        bad=1
    elif [ "$code" -eq 0 ] && [ -n "$stopped" ] &&
        { echo "$stopped" | grep -q "room r$room" || grep -q "run: soft reset requested" "$log"; }; then
        # after the requested soft reset the run ends at the title (checked below)
        say "Ergebnis: OK, kein Absturz ($secs s)"
    elif grep -q "fatal signal" "$log"; then
        say "Ergebnis: ABSTURZ ($(grep -m1 'fatal signal' "$log" | sed 's/^\[re4\] //'))"
        bad=1
    else
        say "Ergebnis: FEHLER (Exit-Code $code, Ziel nicht erreicht)"
        bad=1
    fi
    say "Letzter Stand:  $(trace_state "$trace")"
    say "Hauptschleife: ${reached:-nicht erreicht}"
    say "Ende:          ${stopped:-kein regulärer Stopp}"
    local placed free
    free=$(grep -m1 "run: player free" "$log" | sed 's/^\[re4\] //')
    if [ -n "$free" ]; then
        say "Spieler frei:  $free"
    else
        say "Spieler frei:  NEIN - Ereignis oder Untermenü (Funkspruch) endete nicht von selbst"
        bad=1
    fi
    placed=$(grep -m1 "run: player placed" "$log" | sed 's/^\[re4\] //')
    [ -n "$placed" ] && say "Testhilfe:     $placed"
    # the camera of the debug start (patch 0044): the Retroid crashed at the
    # first camera cut (examining the skulls in r100)
    local camera
    camera=$(grep -m1 "run: camera after the room start" "$log" | sed 's/^\[re4\] run: //')
    if [ -n "$camera" ]; then
        say "Kamera:        $camera"
        case "$camera" in *DESTROYED*) bad=1 ;; esac
    fi
    # sub screens (radio, map with Z): the map stayed black on the Retroid and
    # could not be closed
    if grep -q "run: sub screen" "$log"; then
        say "Untermenüs:    $(grep "run: sub screen" "$log" | sed 's/^\[re4\] run: sub screen //' | head -n 8 | paste -sd ',' - | sed 's/,/, /g')"
        if grep -q "Read File: SS/.*/ss_map.dat" "$log"; then
            say "Kartendatei:   gelesen (ss_map.dat)"
        elif [ -n "$free" ]; then
            say "Kartendatei:   NICHT gelesen (Karte nicht geöffnet oder Laden hängt)"
        fi
        if [ "$(grep "run: sub screen" "$log" | tail -n 1 | grep -c ' open ')" = 1 ]; then
            say "Untermenü am Ende noch OFFEN (ließ sich nicht schließen)"
            bad=1
        fi
    fi
    # the soft reset of "game over" -> "no": the title must come back (it
    # reads title.dat anew); on the Retroid the screen stayed black
    local reset
    reset=$(grep -m1 "run: soft reset requested" "$log" | sed 's/^\[re4\] //')
    if [ -n "$reset" ]; then
        say "Soft-Reset:    $reset"
        grep "run: after the soft reset\|SOFT_RESET" "$log" | sed 's/^\[re4\] /  /' | head -n 10 | tee -a "$report"
        if sed -n '/run: soft reset requested/,$p' "$log" | grep -q "Read File: SS/.*/title.dat"; then
            say "Titel nach dem Soft-Reset: ja (title.dat neu gelesen)"
        else
            say "Titel nach dem Soft-Reset: NEIN (das Spiel kehrt nicht zum Titel zurück)"
            bad=1
        fi
    elif [ -n "$free" ]; then
        say "Soft-Reset:    nicht erreicht (Lauf endete vorher; mehr Bilder: --frames)"
    fi
    if grep -q "run: area " "$log"; then
        say "Auslöse-Bereiche des Raums (AEV, für RE4_TEST_PLACE):"
        grep "run: area " "$log" | sed 's/^\[re4\] run: /  /' | head -n 80 | tee -a "$report"
    fi
    if grep -q "backtrace of the running context" "$log"; then
        say "Rückverfolgung beim Abbruch (wo das Programm gerade war, dann jede Spielaufgabe mit ihrem Warteort):"
        sed -n '/backtrace of the running context/,$p' "$log" | grep -v "pad: frame" | head -n 400 |
            sed 's/^/  /' | tee -a "$report"
    fi
    # coverage: every GetDataExt access, converted yes / no
    if [ -f "$cov" ]; then
        local total no room_no
        total=$(wc -l < "$cov")
        no=$(grep -c $'\tnein' "$cov")
        room_no=$(grep -i "r$room" "$cov" | grep -c $'\tnein')
        say "Abdeckung (GetDataExt): $total Zugriffe, davon \"nein\": $no (im Archiv von r$room: $room_no)"
        if [ "$no" -gt 0 ]; then
            say "  Zeilen mit \"nein\" (Archiv, Kennung, Nummer, Status):"
            grep $'\tnein' "$cov" | awk -F'\t' '{print $1 "\t" $2 "\t" $3 "\t" $5}' | sort | uniq -c | sort -rn |
                head -40 | sed 's/^/    /' | tee -a "$report"
            bad=1
        fi
        say "  Kennungen im Archiv von r$room: $(grep -i "r$room" "$cov" | cut -f2 | sort | uniq -c | tr -s ' ' |
            tr '\n' ',' | sed 's/,$//')"
    else
        say "Abdeckung: keine GetDataExt-Zugriffe protokolliert"
    fi
    # converter messages that point at something left undone
    local msgs
    msgs=$(grep -h "formats:" "$log" | grep -E "no converter|incomplete|outside|left big-endian|left as|no field list|blocks in its chain|more than|overlaps|suspended|back again" |
        sed 's/^\[re4\] //' | sort | uniq -c | sort -rn)
    if [ -n "$msgs" ]; then
        say "Umwandler-Meldungen:"
        echo "$msgs" | head -40 | sed 's/^/  /' | tee -a "$report"
    else
        say "Umwandler-Meldungen: keine"
    fi
    # the radio archives: which tags their message and sequence halves carry
    if grep -q "codec archive" "$log"; then
        say "Funkarchive:"
        grep "codec archive" "$log" | sed 's/^\[re4\] formats: /  /' | sort -u | head -5 | tee -a "$report"
    fi
    local stubs
    stubs=$(grep -h "stub called:" "$log" | sed 's/^\[re4\] stub called: //' | sort -u)
    say "Aufgerufene Stubs (Funktionen ohne Umsetzung): $(echo -n "$stubs" | grep -c .)"
    [ -n "$stubs" ] && echo "$stubs" | head -60 | sed 's/^/  /' | tee -a "$report"
    if [ -f "$trace" ]; then
        local lines enemies last
        lines=$(wc -l < "$trace")
        last=$(tail -n 1 "$trace")
        enemies=$(echo "$last" | grep -o '"slot"' | wc -l)
        say "Zustands-Trace: $lines Bilder; im letzten Bild $enemies Gegner"
        say "  letzte Zeile: $(echo "$last" | cut -c1-600)"
        # the sound streams (status be, state rno, requests, volume, DVD
        # busy/requests/status, DMA, errors): the last frame, then every frame
        # where the set of streams (id, status, state) changes, up to 40 lines
        # (the radio call: ring tone stopped, voice ready, playing, ended)
        python3 - "$trace" <<'PY' | tee -a "$report"
import json, sys
last, prev, lines = None, None, []
for raw in open(sys.argv[1], encoding="utf-8", errors="replace"):
    try:
        row = json.loads(raw)
    except ValueError:
        continue
    streams = row.get("str", [])
    last = streams
    key = [(s.get("w"), s.get("id"), s.get("be"), s.get("rno")) for s in streams]
    if key != prev:
        lines.append("    Bild %s: %s" % (row.get("frame"), json.dumps(streams, separators=(",", ":"))))
        prev = key
if last:
    print("  Tonspuren im letzten Bild: " + json.dumps(last, separators=(",", ":")))
print("  Tonspuren-Verlauf (Bilder mit Änderungen):")
print("\n".join(lines[:40]) if any(l.split(": ", 1)[1] != "[]" for l in lines) else "    (keine Tonspur im ganzen Lauf)")
PY
        # control: how far Leon walked while the free script held the stick
        # (until an event took over, e.g. s03 at the first house)
        local freeframe walk
        freeframe=$(echo "$free" | sed -n 's/.* at frame \([0-9]*\).*/\1/p')
        if [ -n "$freeframe" ]; then
            walk=$(python3 - "$trace" "$freeframe" <<'PY'
import json, math, sys
free = int(sys.argv[2])
start, end, pos, why = free + 452, free + 750, {}, "Ende der Tastenfolge"
for line in open(sys.argv[1]):
    try:
        r = json.loads(line)
    except ValueError:
        continue
    f = r["frame"]
    if f < start or f > end or "pl" not in r:
        continue
    if "E" in r.get("cam", {}).get("sw", "") or "U" in r.get("cam", {}).get("sw", ""):
        why = f"Ereignis ab Bild {f}"
        break
    pos[f] = r["pl"]
if len(pos) < 2:
    print("0 keine Daten")
else:
    a, b = pos[min(pos)], pos[max(pos)]
    d = math.dist((a[0], a[2]), (b[0], b[2]))
    print(f"{d:.0f} Leon lief {d:.0f} Einheiten in {max(pos) - min(pos)} Bildern (Stick nach vorn, bis {why})")
PY
)
            if [ "${walk%% *}" -ge 500 ] 2> /dev/null; then
                say "Steuerung:     OK, ${walk#* }"
            else
                say "Steuerung:     FEHLER, ${walk#* }"
                bad=1
            fi
        fi
        # the sound (M8): what the core mixed, as a WAV of the first 240 s of game
        # time (to listen to on the PC) and its levels
        local audio
        audio=$(grep "audio: [0-9]* blocks" "$log" | tail -1 | sed 's/^\[re4\] //')
        say "Ton:           ${audio:-keine Tonblöcke im Protokoll}"
        if [ -s "$run/ton-$t.wav" ]; then
            say "Ton-Aufnahme:  ton-$t.wav (zum Anhören)"
            python3 "$src/port/tools/audio_wav_report.py" "$run/ton-$t.wav" 10 | tee -a "$report"
        fi
        # the camera over the run: every change of its switches, and every 2500 frames
        python3 - "$trace" <<'PY' | tee -a "$report"
import json, sys
def v(a):
    return "(" + ", ".join(f"{x:.0f}" for x in a) + ")"
prev, out = None, []
for line in open(sys.argv[1]):
    try:
        r = json.loads(line)
    except ValueError:
        continue
    c = r.get("cam")
    if not c:
        continue
    key = (r["Rno0"], r["room"], c["sw"], c["r0"])
    if key != prev or r["frame"] % 2500 == 0:
        out.append(f"  Bild {r['frame']:6d} Schritt {r['Rno0']} Raum {r['room']} Schalter [{c['sw']}] r0 {c['r0']}: "
                   f"Auge {v(c['eye'])} Ziel {v(c['at'])} Winkel {c['fov']:.0f}, CamCtrl-Auge {v(c['ctrl'])}"
                   + (f", Leon {v(r['pl'])}" if "pl" in r else ""))
        prev = key
print("Kamera-Verlauf (Schalter: C Kamera an, D Debug-Kamera, S Kamera gestoppt, E Ereignis, U Untermenü, X Sonderkamera):")
for l in out[:40]:
    print(l)
if len(out) > 40:
    print(f"  ... {len(out) - 40} weitere Wechsel")
PY
        gzip -f "$trace"
    fi
    # core bridge (what the app would draw): models and their disc sources
    if [ -s "$bridge" ]; then
        python3 - "$bridge" "$room" <<'PY' | tee -a "$report"
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1]) if l.strip()]
room = "0x" + sys.argv[2].lower()
inroom = [r for r in rows if r["Rno0"] == 3 and r["room"] == room]
import re
def summary(last):
    models = last["models"]
    drawn = [m for m in models if m["drawn"]]
    # the room's scenery objects made from SMD works (cObjScr, ObjMgr id 2, "smd" = block and
    # work): the app draws them with the room scene; scenery a room script made (SetObjSmd, like
    # r100's police car) is drawn like the other models
    scenery = [m for m in drawn if m["mgr"] == 1 and m["id"] == "0x02" and "smd" in m]
    drawn = [m for m in drawn if not (m["mgr"] == 1 and m["id"] == "0x02" and "smd" in m)]
    def found(src):
        return bool(src) and not src.startswith("?")
    def known(m):
        return m["infos"] and all(found(i["bin"]) and found(i["tpl"]) for i in m["infos"])
    unknown = [m for m in drawn if not known(m)]
    screen = last.get("screen", 0)
    print(f"Brücke zur App (Bild {last['frame']}, Schritt {last['Rno0']}, Raum {last['room']}): "
          f"{len(models)} Figuren, davon {len(drawn) + len(scenery)} sichtbar ({len(scenery)} Raumkulisse, die die App "
          f"mit dem Raum zeichnet), {len(drawn) - len(unknown)} von {len(drawn)} übrigen mit bekannter "
          f"Modell-Herkunft auf der Disc; {last['parts']} Körperteile; Kamera {last['camera']}"
          + ("; Untermenü offen (Kamera = Weltkamera)" if screen & 1 else "") + ("; Ereignis läuft" if screen & 2 else "")
          + (f"; {last['dropped']} Figuren über der Obergrenze" if last.get("dropped") else "")
          + (f"; {last['droppedInfos']} Modellteile über der Obergrenze" if last.get("droppedInfos") else ""))
    # scenery per SMD (room -1, scroll blocks 0..): shown and hidden works
    blocks = {}
    for m in models:
        if m["mgr"] == 1 and m["id"] == "0x02" and "smd" in m:
            b = blocks.setdefault(m["smd"][0], [0, 0])
            b[0 if m["drawn"] else 1] += 1
    if blocks:
        print("  Raumkulisse nach SMD: " + ", ".join(
            f"{'Raum' if k < 0 else f'Block {k}'} {v[0]} sichtbar" + (f" / {v[1]} versteckt" if v[1] else "")
            for k, v in sorted(blocks.items())))
    script = [m for m in models if m["mgr"] == 1 and m["id"] == "0x02" and "smd" not in m]
    if script:
        print(f"  Kulisse aus Raumskripten (nicht im SMD, z. B. das Polizeiauto): {len(script)}, "
              f"davon {sum(1 for m in script if m['drawn'])} sichtbar")
    def plain(src):
        # unknown sources carry their address: leave it out when grouping
        return re.sub(r"0x[0-9a-f]{6,8}|\+0x[0-9a-f]+", "…", src or "?")
    seen, lines = {}, []
    for m in drawn:
        def how(i):
            if not i.get("shown", 1):
                return " (versteckt)"
            r = i.get("rigid", -1)
            morph = f" (Form-Animation, {i['morph']} Punkte)" if i.get("morph") else ""
            return (f" (starr an Teil {r})" if r >= 0 else " (am Modell)" if r == -2 else "") + morph
        srcs = "; ".join(f"BIN {i['bin'] or '?'} TPL {i['tpl'] or '?'}{how(i)}" for i in m["infos"]) or "kein Modell"
        key = (m["mgr"], m["id"], m["parts"], plain(srcs))
        if key in seen:
            seen[key] += 1
            continue
        seen[key] = 1
        lines.append((key, f"  {'Gegner' if m['mgr'] == 0 else 'Objekt'} {m['id']} Typ {m['type']}, {m['parts']} Teile: {srcs}"))
    for key, text in lines[:40]:
        print(text + (f"  (×{seen[key]})" if seen[key] > 1 else ""))
    if len(lines) > 40:
        print(f"  ... {len(lines) - 40} weitere Arten")
    if unknown:
        print(f"  Hinweis: {len(unknown)} sichtbare Figuren ohne bekannte Herkunft (die App kann sie noch nicht zeichnen)")
# early in the room (before a sub screen may hide the world) and at the end
picks = inroom[:1] + inroom[-1:] if len(inroom) > 1 else (inroom or rows[-1:])
for row in picks:
    summary(row)

PY
    else
        say "Brücke zur App: keine Ausgabe (RE4_BRIDGE_DUMP)"
    fi
    if [ "$bad" -ne 0 ]; then
        say "Letzte 60 Zeilen des Spiel-Logs (ohne Tastenzeilen):"
        grep -v "pad: frame" "$log" | tail -n 60 | sed 's/^/  /' | tee -a "$report"
        overall=1
    fi
}
# PPM -> PNG (python's zlib), for viewing on Windows
ppm_to_png() {
    python3 - "$1" <<'PY'
import struct, sys, zlib
from pathlib import Path
for ppm in sorted(Path(sys.argv[1]).glob("*.ppm")):
    data = ppm.read_bytes()
    parts = data.split(b"\n", 3)
    w, h = map(int, parts[1].split())
    pixels = parts[3]
    raw = b"".join(b"\0" + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + \
        chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    ppm.with_suffix(".png").write_bytes(png)
    ppm.unlink()
PY
}

# ---- pictures: the core's snapshots drawn as the app's core mode draws them ---------------
# (the app's renderer on a headless GLES3 context, port/android-decomp/tools/
# core_snapshot_render.cpp; needs the Mesa EGL/GLES packages of
# whole_setup_linux.sh)
render_pictures() {
    local snaps="$run/schnappschuesse-i686" pics="$run/bilder" tool_build="$src/port/build/app-host-gl"
    section "4. Bilder der App-Brücke (wie der Kernmodus der App zeichnet)"
    if ! ls "$snaps"/*.bin > /dev/null 2>&1; then
        say "Keine Schnappschüsse (Hauptschleife nicht erreicht?)"
        return
    fi
    mkdir -p "$pics"
    if ! cmake -S "$src/port/android-decomp/app/src/main/cpp" -B "$tool_build" -G Ninja \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release \
        -DRE4_BUILD_GL_CHECK=ON > "$run/bild-werkzeug.log" 2>&1 ||
        ! ninja -C "$tool_build" ${RE4_JOBS:+-j "$RE4_JOBS"} core_snapshot_render >> "$run/bild-werkzeug.log" 2>&1; then
        say "FEHLER: Bild-Werkzeug nicht gebaut (Log: bild-werkzeug.log; Pakete: sudo $src/port/tools/whole_setup_linux.sh)"
        tail -n 20 "$run/bild-werkzeug.log" | sed 's/^/  /' | tee -a "$report"
        return
    fi
    # the discs as the app imports them: disc1, disc2
    local approot="$run/app-disc"
    rm -rf "$approot"
    mkdir -p "$approot"
    if [ -d "$layout/disc0" ]; then
        ln -s "$layout/disc0" "$approot/disc1"
        [ -d "$layout/disc1" ] && ln -s "$layout/disc1" "$approot/disc2"
    else
        ln -s "$layout" "$approot/disc1"
    fi
    "$tool_build/core_snapshot_render" "$approot" "$pics" "$snaps"/*.bin 2> "$run/bild-werkzeug-lauf.log" |
        sed 's/^/  /' | tee -a "$report"
    rm -f "$pics"/room-*.dat
    # which models the app could load from the disc, and why not
    local loaded failed
    loaded=$(grep -c "^\[RE4Core\] model .*: [0-9]* joints$" "$run/bild-werkzeug-lauf.log" 2> /dev/null)
    failed=$(grep -h "^\[RE4Core\] model " "$run/bild-werkzeug-lauf.log" 2> /dev/null | grep -v ": [0-9]* joints$" |
        sed 's/^\[RE4Core\] //' | sort | uniq -c | sort -rn)
    say "  Modelle der App geladen: ${loaded:-0}; nicht ladbar: $(echo -n "$failed" | grep -c .)"
    [ -n "$failed" ] && echo "$failed" | head -25 | sed 's/^/    /' | tee -a "$report"
    grep -h "^\[RE4Core\] room " "$run/bild-werkzeug-lauf.log" 2> /dev/null | sort -u | head -5 | sed 's/^\[RE4Core\] /  /' |
        tee -a "$report"
    # where the characters' model infos land (one line per info of the player and
    # of models with more than 20 parts, from the first snapshot that has them)
    say "  Lage der Modellteile (Spieler und Figuren mit vielen Teilen; darunter je Info, was Dreiecke"
    say "  unsichtbar macht: auf 0 skalierte Teile, Alpha-Test, Mischmodus/Tiefe/Seite/Ausblenden des Spiels):"
    grep -h "^\[RE4Core\] bounds \|^\[RE4Core\]   info " "$run/bild-werkzeug-lauf.log" 2> /dev/null |
        sed 's/^\[RE4Core\] bounds //; s/^\[RE4Core\]   info /      info /' |
        awk '/^      info / {if (keep) print; keep=0; next}
             {keep=0; key=$4" "$6" "$8; if (!(key in seen) && ($6 == "00" || $0 ~ /joints [2-9][0-9]|joints [0-9][0-9][0-9]/)) {seen[key]=1; keep=1; print "    " $0}}' |
        head -80 | tee -a "$report"
    rm -rf "$approot"  # links to the disc folder: not copied with the report
    ppm_to_png "$pics"
    say "Bilder: $(ls "$pics"/*.png 2> /dev/null | wc -l) PNG-Dateien im Ordner bilder (bitte mitschicken)"
}

# ---- pictures through the GX emulation: what the game itself draws (logos,
# title, menus, displays, effects), drawn with the app's GX renderer
# (port/android-decomp/tools/gx_chunk_render.cpp; frames from RE4_GX_DUMP)
render_gx_pictures() {
    local dump="$run/gx-i686" pics="$run/bilder-gx" tool_build="$src/port/build/app-host-gl"
    section "5. Bilder über die GX-Nachbildung (was das Spiel selbst zeichnet)"
    if ! ls "$dump"/*.bin > /dev/null 2>&1; then
        say "Keine GX-Bilder aufgezeichnet (Spiel nicht weit genug gelaufen?)"
        return
    fi
    mkdir -p "$pics"
    if ! cmake -S "$src/port/android-decomp/app/src/main/cpp" -B "$tool_build" -G Ninja \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release \
        -DRE4_BUILD_GL_CHECK=ON > "$run/gx-werkzeug.log" 2>&1 ||
        ! ninja -C "$tool_build" ${RE4_JOBS:+-j "$RE4_JOBS"} gx_chunk_render >> "$run/gx-werkzeug.log" 2>&1; then
        say "FEHLER: GX-Bild-Werkzeug nicht gebaut (Log: gx-werkzeug.log; Pakete: sudo $src/port/tools/whole_setup_linux.sh)"
        tail -n 20 "$run/gx-werkzeug.log" | sed 's/^/  /' | tee -a "$report"
        return
    fi
    if ! "$tool_build/gx_chunk_render" "$pics" "$dump"/*.bin 2> "$run/gx-werkzeug-lauf.log" | sed 's/^/  /' |
        tee -a "$report"; then
        say "FEHLER: GX-Bilder (Log: gx-werkzeug-lauf.log)"
        overall=1
    fi
    grep -i "shader failed" -A 3 "$run/gx-werkzeug-lauf.log" 2> /dev/null | head -20 | sed 's/^/  /' | tee -a "$report"
    ppm_to_png "$pics"
    # a few frames as data for the renderer work in the cloud (gx_chunk_render
    # draws them there; game data: never into the repository): the first
    # three and the first in the room
    local keep
    # the picture with the map open (free + map_dump, the dump starts one or two frames later)
    local freeframe mapfile=""
    freeframe=$(grep -m1 "run: player free" "$run/spiel-i686.log" 2> /dev/null | sed -n 's/.* at frame \([0-9]*\).*/\1/p')
    if [ -n "$freeframe" ]; then
        for d in 0 1 2 3 4 5; do
            f=$(printf '%s/gx-%06u.bin' "$dump" $((freeframe + map_dump + d)))
            if [ -f "$f" ]; then mapfile="$f"; break; fi
        done
        if [ -n "$mapfile" ]; then
            say "Karte offen (Z, Bild $((freeframe + map_dump))): $(basename "$mapfile" .bin).png"
        else
            say "Karte offen: kein GX-Bild aufgezeichnet"
        fi
    fi
    # and frames asked for by number (RE4_GX_KEEP, default: 901, the opening
    # event's view of the car from above with its shadow on the ground)
    local wanted=""
    for n in ${RE4_GX_KEEP:-901}; do
        f=$(printf '%s/gx-%06u.bin' "$dump" "$n")
        [ -f "$f" ] && wanted="$wanted $f"
    done
    keep=$( (ls "$dump"/gx-*.bin | head -3; ls "$dump"/gx-*.bin | sed -n '4,99p' |
        while read -r f; do [ "$(stat -c %s "$f")" -gt 2000000 ] && echo "$f"; done | head -1; echo "$mapfile";
        for f in $wanted; do echo "$f"; done) |
        grep . | sort -u)
    python3 - "$run/gx-daten.zip" $keep <<'PY'
import sys, zipfile
from pathlib import Path
with zipfile.ZipFile(sys.argv[1], "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
    for f in sys.argv[2:]:
        z.write(f, Path(f).name)
PY
    rm -rf "$dump"  # large; the pictures and gx-daten.zip stay
    say "GX-Bilder: $(ls "$pics"/*.png 2> /dev/null | wc -l) PNG-Dateien im Ordner bilder-gx (bitte mitschicken)"
    say "GX-Daten: gx-daten.zip ($(du -h "$run/gx-daten.zip" | cut -f1), $(echo "$keep" | wc -w) Bilder als Daten; bitte mitschicken)"
}

# ---- the movies of the disc through the CRI middleware (M9) --------------------------------
# Every .sfd file of both discs is played to its end the way game/sofdec.cpp
# plays it (re4_host --movie-test, tests/movie_probe.cpp), in fast mode
# (RE4_VI_FAST: the fields at once, the CRI clock counts them; at most
# RE4_CHECK_MOVIE_MINUTES minutes of movie, default 15): the player's status
# at the end (3 = played to its end), the frames, frames the player skipped,
# CRI errors, the time one step takes; a still every 30 s as PNG
# (bilder-film/) and the sound of the opening movie as WAV (film-*.wav).
run_movies() {
    local dir="$src/port/build/whole-i686$opt" mins="${RE4_CHECK_MOVIE_MINUTES:-15}"
    section "6. Filme der Disc über die CRI-Middleware (ganz, im Schnelllauf)"
    if [ ! -x "$dir/re4_host" ]; then
        say "FEHLER: $dir/re4_host fehlt (Build fehlgeschlagen?)"
        overall=1
        return
    fi
    local stills="$run/bilder-film" bad=0 n=0 disc f name rel log line
    rm -rf "$stills"
    mkdir -p "$stills"
    for disc in 0 1; do
        [ -d "$layout/disc$disc/files" ] || continue
        while IFS= read -r f; do
            rel="${f#"$layout/disc$disc/files/"}"
            name="d$((disc + 1))-$(basename "$f" | sed 's/\.[sS][fF][dD]$//')"
            log="$run/film-$name.log"
            n=$((n + 1))
            (cd "$dir" && exec env RE4_MEMCARD=none RE4_MOVIE_DISC="$disc" RE4_VI_FAST=1 RE4_MOVIE_FAST=1 RE4_MOVIE_EVERY=1800 \
                timeout 1200 ./re4_host --data "$layout" --movie-test "$rel" "$run/film.yuv" \
                "$run/film-$name.wav" $((mins * 3600))) > "$log" 2>&1
            local code=$?
            line=$(grep -m1 "\[movie\] end:" "$log" | sed 's/^.*\[movie\] end: //')
            say "$name ($rel): exit $code, ${line:-kein Ende gemeldet}"
            grep -m1 "\[movie\] step" "$log" | sed 's/^.*\[movie\] /  /' | tee -a "$report"
            grep "CRI error\|last CRI error\|fatal signal" "$log" | head -3 | sed 's/^.*\[movie\] /  /;s/^\[re4\] /  /' | tee -a "$report"
            grep -o "Stub aufgerufen: .*\|stub called: .*" "$log" | sort -u | head -5 | sed 's/^/  /' | tee -a "$report"
            if [ -f "$run/film.yuv" ]; then
                python3 "$src/port/tools/movie_report.py" --sparse "$run/film.yuv" "$stills" "$name" 2>&1 | sed 's/^/  /' | tee -a "$report"
                rm -f "$run/film.yuv"
            fi
            if [ "$code" -ne 0 ]; then
                bad=1
            fi
            # the sound of the opening movie is kept (with its levels), the others not (size)
            if [ "$(basename "$f" | tr 'A-Z' 'a-z')" = "opening.sfd" ]; then
                python3 "$src/port/tools/audio_wav_report.py" "$run/film-$name.wav" 2>&1 | head -8 | sed 's/^/  /' | tee -a "$report"
            else
                rm -f "$run/film-$name.wav"
            fi
        done < <(find "$layout/disc$disc/files" -iname '*.sfd' | sort)
    done
    if [ "$n" -eq 0 ]; then
        say "Keine .sfd-Dateien gefunden."
        bad=1
    fi
    rm -f "$run/bilder-film.zip"
    (cd "$run" && python3 -m zipfile -c bilder-film.zip bilder-film)
    say "Standbilder: $(ls "$stills"/*.png 2> /dev/null | wc -l) PNG-Dateien im Ordner bilder-film, gepackt als bilder-film.zip ($(du -h "$run/bilder-film.zip" 2> /dev/null | cut -f1); bitte mitschicken)"
    [ "$bad" -ne 0 ] && overall=1
}

# ---- the memory card (M10): system file, save, a new start, load ---------------------------
# Slot A holds a card file of this run (runtime/memcard.cpp), formatted first
# by the SDK's card library (re4_host --memcard-format). Run A: the boot
# check finds no system file and asks to make one (A: yes, the cursor's
# default); new game into r100 (debug start), once the player is free the
# save screen of a typewriter opens (RE4_TEST_SAVE, after setting the
# brightness option to 72; the default is 64), A picks file 1, A confirms,
# START closes "saved"; the run ends when the screen has closed. Run B, a new
# start with the same card: the boot check reads the system file (brightness
# 72 again), the title menu stands on "load" (no debug start), A loads file
# 1, the game continues in r100. The card is listed afterwards
# (port/tools/memcard_list.py: files, banner/icon, the game's CRCs).
run_card_game() {
    local name="$1" limit="$2" dir="$3"
    shift 3
    (cd "$dir" && exec env RE4_VI_FAST=1 RE4_MEMCARD="$card" RE4_PAD_UNTIL_ROOM=1 "$@" \
        timeout "$limit" ./re4_host --data "$layout") > "$run/$name.log" 2>&1
    local code=$?
    grep -E "memcard: (files|.* saved)|card: |run: (memory card check|save |main loop|player free|stopped)|fatal signal|stub called: (OSResetSystem|CARD|__CARD)" \
        "$run/$name.log" | sed 's/^\[re4\] /  /' | head -n 80 | tee -a "$report"
    return $code
}
run_memcard() {
    local dir="$src/port/build/whole-i686$opt" card="$run/speicherkarte-a.bin" bad=0 code
    section "7. Speicherkarte: Systemdatei anlegen, im Raum speichern, Neustart, im Titel laden"
    if [ ! -x "$dir/re4_host" ]; then
        say "FEHLER: $dir/re4_host fehlt (Build fehlgeschlagen?)"
        overall=1
        return
    fi
    rm -f "$card"
    if ! (cd "$dir" && ./re4_host --memcard-format "$card") > "$run/karte-formatieren.log" 2>&1; then
        say "FEHLER: die Karte ließ sich nicht formatieren (karte-formatieren.log)"
        tail -n 5 "$run/karte-formatieren.log" | sed 's/^/  /' | tee -a "$report"
        overall=1
        return
    fi
    say "Karte: speicherkarte-a.bin (2 MB, formatiert, leer)"
    say "Lauf A: Systemdatei anlegen, neues Spiel in r100, dort speichern (Datei 1)"
    run_card_game karte-speichern "${RE4_CHECK_CARD_LIMIT:-1800}" "$dir" RE4_START_ROOM=0x100 RE4_ROOM_MENU=1 \
        RE4_PAD_SCRIPT="A@100-60000/60 START@130-60000/60" RE4_TEST_SAVE="free+60,72,stop" \
        RE4_PAD_FREE_SCRIPT="A@90-600/30 START@900-3000/120"
    code=$?
    if [ "$code" -ne 0 ] || ! grep -q "run: stopped after the save" "$run/karte-speichern.log"; then
        say "Ergebnis A: FEHLER (Exit-Code $code; Log: karte-speichern.log)"
        bad=1
    elif ! grep -q "card: boot check: making the system file" "$run/karte-speichern.log"; then
        say "Ergebnis A: FEHLER (die Startprüfung hat keine Systemdatei angelegt)"
        bad=1
    else
        say "Ergebnis A: OK (Systemdatei angelegt, gespeichert, Bildschirm geschlossen)"
    fi
    say "Lauf B: Neustart mit derselben Karte, im Titel laden"
    run_card_game karte-laden "${RE4_CHECK_CARD_LIMIT:-1800}" "$dir" RE4_STOP_AFTER=600 \
        RE4_PAD_SCRIPT="A@100-60000/60 START@130-60000/60"
    code=$?
    local check loaded
    check=$(grep -m1 "run: memory card check done" "$run/karte-laden.log")
    loaded=$(grep -m1 "run: save loaded from the memory card" "$run/karte-laden.log")
    if [ "$code" -ne 0 ] || ! grep -q "run: stopped after" "$run/karte-laden.log"; then
        say "Ergebnis B: FEHLER (Exit-Code $code; Log: karte-laden.log)"
        bad=1
    elif [ -z "$loaded" ]; then
        say "Ergebnis B: FEHLER (kein Spielstand geladen)"
        bad=1
    elif ! echo "$check" | grep -q "brightness 72,"; then
        say "Ergebnis B: FEHLER (Optionen nicht aus der Systemdatei: ${check#*options })"
        bad=1
    elif ! grep -q "run: main loop reached .* room r100" "$run/karte-laden.log"; then
        say "Ergebnis B: FEHLER (das geladene Spiel lief nicht in r100 weiter)"
        bad=1
    else
        say "Ergebnis B: OK (Optionen aus der Systemdatei, Spielstand geladen, r100 läuft)"
    fi
    say "Inhalt der Karte (port/tools/memcard_list.py):"
    python3 "$src/port/tools/memcard_list.py" "$card" --expect-system --expect-save 0 2>&1 | sed 's/^/  /' | tee -a "$report"
    [ "${PIPESTATUS[0]}" -ne 0 ] && bad=1
    [ "$bad" -ne 0 ] && overall=1
}

if [ "$game_ok" -eq 1 ]; then
    # limits in seconds: overall, until the main loop, without a new frame
    run_game i686 "${RE4_CHECK_LIMIT:-3600}" "${RE4_CHECK_MAIN_LIMIT:-600}" "${RE4_CHECK_STALL:-180}"
    render_pictures
    render_gx_pictures
    run_movies
    run_memcard
    [ "$arm" -eq 1 ] && run_game armhf 14400 5400 600
fi

section "Zusammenfassung"
if [ "$overall" -eq 0 ]; then
    say "ALLES OK"
else
    say "NICHT BESTANDEN (Einzelheiten oben)"
fi
say "Logs und Trace: $run"
if [ -n "$out" ]; then
    mkdir -p "$out"
    cp -r "$run"/. "$out"/
    echo "Bericht: $out/bericht.txt"
fi
exit $overall
