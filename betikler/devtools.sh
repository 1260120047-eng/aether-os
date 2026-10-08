#!/bin/sh
# Derleme ortamına uygulamaların geliştirme dosyaları (GTK 3, VTE, libwnck, Xlib, SDL2),
# initramfs için busybox-static ve firmware seçimi için kmod
set -e
apk add -q python3 py3-pillow py3-numpy gtk+3.0-dev sdl2-dev gcc musl-dev pkgconf make busybox-static librsvg-dev rsvg-convert \
  vte3-dev libwnck3-dev libnotify-dev libx11-dev libxft-dev libxrender-dev libxcursor-dev gdk-pixbuf-dev kmod zstd
