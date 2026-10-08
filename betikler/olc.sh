#!/bin/sh
# olc.sh ISO — açılıştan karşılama penceresine kadar geçen süre (saniye)
sh /s/vm.sh dur >/dev/null 2>&1; rm -f /work/vm/disk.qcow2
ISO=$1 sh /s/vm.sh basla cd >/dev/null 2>&1
bas=$(date +%s)
while :; do
  sleep 3
  t=$(( $(date +%s) - bas ))
  echo "screendump /work/vm/o.ppm" | socat - UNIX-CONNECT:/work/vm/mon.sock >/dev/null 2>&1; sleep 0.5
  v=$(magick /work/vm/o.ppm -crop 40x40+620+380 -colorspace gray -format "%[fx:mean*255]" info: 2>/dev/null | cut -d. -f1)
  [ "${v:-0}" -gt 200 ] && { echo "$1: $t sn"; break; }
  [ $t -gt 600 ] && { echo "$1: zaman aşımı"; break; }
done
