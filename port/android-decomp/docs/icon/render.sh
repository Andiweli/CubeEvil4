#!/bin/sh
# render.sh in.svg out.png [size]
s=${3:-512}
/opt/pw-browsers/chromium_headless_shell-1194/chrome-linux/headless_shell --no-sandbox --disable-gpu --hide-scrollbars \
  --default-background-color=00000000 --window-size=$s,$s --screenshot="$2" "file://$(realpath "$1")" > /dev/null 2>&1
