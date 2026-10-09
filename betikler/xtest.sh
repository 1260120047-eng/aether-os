#!/bin/bash
# xtest.sh AD KOMUT... — Xvfb'de bir uygulamayı açıp ekran görüntüsü alır (/work/xt-AD.png)
AD=$1; shift
pkill -x Xvfb 2>/dev/null; sleep 0.5
Xvfb :5 -screen 0 1280x800x24 >/dev/null 2>&1 &
for i in $(seq 50); do [ -e /tmp/.X11-unix/X5 ] && break; sleep 0.1; done
export DISPLAY=:5 LANG=${LANG:-tr_TR.UTF-8} GTK_THEME=${GTK_THEME:-Adwaita}
"$@" >/tmp/xt.log 2>&1 &
sleep ${BEKLE:-6}
if [ -n "$TIK" ]; then xdotool mousemove $TIK click ${TIKB:-1}; sleep ${BEKLE2:-3}; fi
xwd -root -silent | magick xwd:- /work/xt-$AD.png
pkill -x "$(basename "$1" | cut -c1-15)" 2>/dev/null; pkill -x Xvfb
tail -5 /tmp/xt.log
