#!/bin/sh
# derleme ortamına webkit geliştirme dosyaları
apk add -q webkit2gtk-4.1-dev 2>&1 | tail -2
pkg-config --modversion webkit2gtk-4.1
# kök sistemde ne kadar yer tutacağını ölç
R=/work/rootfs
before=$(du -sm $R | cut -f1)
apk add --root $R -q --no-progress webkit2gtk-4.1 gst-plugins-good gst-plugins-base 2>&1 | grep -v "^Executing\|grub-probe\|trigger" | tail -3
apk del --root $R -q --no-progress netsurf 2>&1 | tail -1
after=$(du -sm $R | cut -f1)
echo "ek: $((after - before)) MB"
apk --root $R info -e netsurf && echo "netsurf hala var" || echo "netsurf kaldirildi"
