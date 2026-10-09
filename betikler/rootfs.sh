#!/bin/bash
# Aether 2.0 "Orion" — Arch Linux kök sistemini /work/rootfs içine sıfırdan kurar
set -e
R=/work/rootfs
umount -R $R 2>/dev/null || true
rm -rf $R; mkdir -p $R

PKGS=(
  # taban
  base linux mkinitcpio opendoas bash-completion less nano sudo
  grub efibootmgr dosfstools e2fsprogs squashfs-tools rsync ntfs-3g ntfsprogs archlinux-appstream-data gvfs udisks2 flatpak xdg-desktop-portal-gtk virtualbox-guest-utils spice-vdagent open-vm-tools
  # firmware (tam): AMD ekran kartları ve Realtek. Diğerleri build.sh'de sürücüye göre seçilir.
  linux-firmware-amdgpu linux-firmware-radeon linux-firmware-realtek
  # masaüstü
  xorg-server xorg-xinit xorg-xrandr xorg-setxkbmap xorg-xset xorg-xinput xorg-xprop xprintidle xdotool
  xf86-input-libinput xf86-video-vesa xf86-video-fbdev mesa mesa-utils sdl2-compat
  lightdm lightdm-gtk-greeter plymouth xsettingsd feh i3lock scrot
  # uygulamalar ve kütüphaneler
  mousepad ristretto galculator webkit2gtk-4.1 gst-plugins-good gst-plugins-base
  vte3 libwnck3 libnotify libxft libxcursor gdk-pixbuf2 librsvg
  # ağ ve ses
  iwd dhcpcd alsa-utils alsa-ucm-conf
  # yazı tipleri ve simgeler
  ttf-jetbrains-mono ttf-dejavu adwaita-icon-theme
  # geliştirici (oyun motorları için)
  gcc make pkgconf
)
pacstrap -c -K $R "${PKGS[@]}" 2>&1 | grep -E "error|warning: .*conflict|installing linux |Total Installed" || true

# dil: Türkçe ve İngilizce
sed -i 's/^#tr_TR.UTF-8/tr_TR.UTF-8/; s/^#en_US.UTF-8/en_US.UTF-8/' $R/etc/locale.gen
arch-chroot $R locale-gen >/dev/null
echo 'LANG=tr_TR.UTF-8' > $R/etc/locale.conf
printf 'KEYMAP=trq\nFONT=\n' > $R/etc/vconsole.conf
ln -sf /usr/share/zoneinfo/Europe/Istanbul $R/etc/localtime
echo aether > $R/etc/hostname
du -sh $R
