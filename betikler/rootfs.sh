#!/bin/sh
# Aether kök sistemini /work/rootfs içine sıfırdan kurar
set -e
R=/work/rootfs
umount -R $R/dev $R/proc $R/sys 2>/dev/null || true
[ -d $R ] && find $R -mindepth 1 -maxdepth 1 -exec rm -rf {} +
mkdir -p $R/etc/apk/keys
cp /etc/apk/repositories $R/etc/apk/
cp /etc/apk/keys/* $R/etc/apk/keys/

PKGS="
alpine-base openrc busybox-openrc bash bash-completion shadow doas tzdata
linux-lts linux-firmware-none linux-firmware-i915 linux-firmware-radeon linux-firmware-xe linux-firmware-rtl_nic mkinitfs
eudev udev-init-scripts udev-init-scripts-openrc
dbus dbus-x11 elogind polkit-elogind
xorg-server xf86-input-libinput xinit xrandr setxkbmap xset xinput xdotool xprop xprintidle
mesa-dri-gallium mesa-gl mesa-egl mesa-gles mesa-utils sdl2
feh
lightdm lightdm-gtk-greeter
vte3 libwnck3 libnotify iwd alsa-utils alsa-utils-openrc alsa-ucm-conf mousepad pcmanfm ristretto galculator webkit2gtk-4.1 gst-plugins-good gst-plugins-base
i3lock scrot
plymouth
font-jetbrains-mono font-dejavu adwaita-icon-theme
musl-locales musl-locales-lang
gcc g++ make cmake musl-dev pkgconf
e2fsprogs dosfstools sfdisk grub grub-bios grub-efi efibootmgr rsync squashfs-tools
dhcpcd iproute2 ca-certificates curl
kbd-bkeymaps
mousepad-lang lightdm-gtk-greeter-lang pcmanfm-lang ristretto-lang galculator-lang libwnck3-lang
"
apk add --root $R --initdb -U --no-progress $PKGS 2>&1 | grep -E "ERROR|^OK" || true
du -sh $R
