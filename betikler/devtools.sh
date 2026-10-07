#!/bin/sh
apk add -q python3 py3-pillow py3-numpy gtk+3.0-dev sdl2-dev gcc musl-dev pkgconf make busybox-static librsvg-dev rsvg-convert 2>&1 | tail -2
apk search -q xsettingsd lightdm-openrc dbus-openrc elogind-openrc | sort
ls /work/rootfs/etc/init.d/ | tr '\n' ' '; echo
ls /work/rootfs/lib/modules/*/kernel/fs/squashfs/
ls /work/rootfs/usr/share/plymouth/themes/
ls /work/rootfs/usr/share/xsessions/ 2>/dev/null
