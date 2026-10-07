#!/bin/sh
# Derleme ortamına uygulamaların geliştirme dosyaları (GTK 3, SDL2) ve initramfs için busybox-static
set -e
apk add -q python3 py3-pillow py3-numpy gtk+3.0-dev sdl2-dev gcc musl-dev pkgconf make busybox-static librsvg-dev rsvg-convert
