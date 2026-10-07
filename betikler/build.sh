#!/bin/sh
# Aether 1.0 "Nebula" — ISO derleme betiği (Alpine derleme ortamında çalışır)
set -e
R=/work/rootfs; O=/work/overlay; I=/work/iso; F=/work/initramfs
KV=$(ls $R/lib/modules | head -1)
echo "== çekirdek: $KV"

echo "== ek paketler"
apk add --root $R -q --no-progress xsettingsd plymouth-drm openbox-lang xf86-video-vesa xf86-video-fbdev webkit2gtk-4.1 gst-plugins-good gst-plugins-base 2>&1 | grep -v "^Executing\|grub-probe\|trigger" || true

apk del --root $R -q --no-progress netsurf 2>/dev/null || true
echo "== uygulamalar"
make -s -C /work/src/apps clean all
make -s -C /work/src/apps install DESTDIR=$R

echo "== overlay"
cp -a $O/. $R/
chmod 600 $R/etc/doas.d/*.conf
cp $R/usr/share/bkeymaps/tr/tr.bmap.gz $R/etc/keymap/tr.bmap.gz 2>/dev/null || { mkdir -p $R/etc/keymap; cp $R/usr/share/bkeymaps/tr/tr.bmap.gz $R/etc/keymap/; }
rm -f $R/usr/share/xsessions/openbox.desktop

echo "== servisler"
rl() { mkdir -p $R/etc/runlevels/$1; shift_=$1; shift; for s in "$@"; do [ -f $R/etc/init.d/$s ] && ln -sf /etc/init.d/$s $R/etc/runlevels/$shift_/$s || echo "  (yok: $s)"; done; }
rm -rf $R/etc/runlevels/*/*
rl sysinit devfs dmesg udev udev-trigger udev-settle aether-splash
rl boot modules sysctl hostname bootmisc syslog hwclock loadkmap localmount seedrng machine-id swap
rl default dbus elogind dhcpcd udev-postmount aether-canli aether-splash-bitir lightdm local
rl shutdown killprocs mount-ro savecache
sed -i 's/^#\?unicode=.*/unicode="YES"/' $R/etc/rc.conf
grep -q '^rc_need_lightdm' $R/etc/rc.conf || true

echo "== sistem ayarları"
sed -i 's/^root:[^:]*:/root:*:/' $R/etc/shadow
ln -sf /usr/share/zoneinfo/Europe/Istanbul $R/etc/localtime; echo Europe/Istanbul > $R/etc/timezone
printf '%s\n' "https://dl-cdn.alpinelinux.org/alpine/v3.22/main" "https://dl-cdn.alpinelinux.org/alpine/v3.22/community" > $R/etc/apk/repositories
# elogind: güç düğmesi kapatsın
mkdir -p $R/etc/elogind/logind.conf.d
printf '[Login]\nHandlePowerKey=poweroff\n' > $R/etc/elogind/logind.conf.d/aether.conf
chroot $R fc-cache -f >/dev/null 2>&1 || true
chroot $R gtk-update-icon-cache -q -f /usr/share/icons/hicolor 2>/dev/null || true
chroot $R gtk-update-icon-cache -q -f /usr/share/icons/Adwaita 2>/dev/null || true

echo "== budama"
apk del --root $R -q --no-progress cmake 2>/dev/null || true
M=$R/lib/modules/$KV/kernel
rm -rf $M/drivers/media $M/drivers/infiniband $M/drivers/isdn $M/drivers/atm $M/drivers/nfc $M/drivers/staging \
       $M/drivers/net/wireless $M/drivers/gpu/drm/nouveau $M/drivers/gpu/drm/radeon $M/drivers/gpu/drm/xe \
       $M/net/wireless $M/net/mac80211 $M/drivers/bluetooth $M/net/bluetooth
chroot $R depmod -a $KV 2>/dev/null || true
rm -rf $R/usr/share/man $R/usr/share/doc $R/usr/share/info $R/usr/share/gtk-doc $R/var/cache/apk/* $R/boot/initramfs-lts $R/boot/System.map*
rm -f $R/etc/resolv.conf

echo "== squashfs"
rm -rf $I; mkdir -p $I/aether $I/boot/grub
mksquashfs $R $I/aether/rootfs.sfs -comp xz -Xbcj x86 -b 1M -noappend -no-progress -quiet \
  -wildcards -e 'proc/*' 'sys/*' 'dev/*' 'run/*' 'tmp/*' 'mnt/*'
ls -la $I/aether/rootfs.sfs

echo "== initramfs"
rm -rf $F; mkdir -p $F/bin $F/sbin $F/proc $F/sys $F/dev $F/run $F/lib/modules/$KV/kernel/drivers
cp /bin/busybox.static $F/bin/busybox
cp /work/src/initramfs/init $F/init; chmod 755 $F/init
# yalnızca gerekli modüller (+ bağımlılıkları) — modules.dep üzerinden kapanış
GEREK="isofs squashfs overlay loop cdrom sr_mod sd_mod usb_storage uas ahci ata_piix ata_generic pata_acpi
 virtio_blk virtio_scsi virtio_pci virtio_mmio nvme xhci_pci xhci_hcd ehci_pci ehci_hcd ohci_pci ohci_hcd uhci_hcd
 vfat nls_cp437 nls_utf8 nls_iso8859_1 ext4 sdhci_pci sdhci_acpi mmc_block hid_generic usbhid atkbd i8042
 vmw_pvscsi mptspi BusLogic hv_storvsc hv_vmbus pata_oldpiix pata_mpiix mpt3sas"
DEP=$R/lib/modules/$KV/modules.dep
yol() { grep -E "(^|/)$(echo $1 | sed 's/[_-]/[_-]/g')\.ko(\.gz|\.xz|\.zst)?:" $DEP | head -1; }
: > /tmp/modlist
for m in $GEREK; do
  s_=$(yol $m) || true; [ -n "$s_" ] || continue
  echo "$s_" | tr -d ':' | tr ' ' '\n' | grep . >> /tmp/modlist
done
sort -u /tmp/modlist | while read -r f; do
  mkdir -p $F/lib/modules/$KV/$(dirname $f); cp $R/lib/modules/$KV/$f $F/lib/modules/$KV/$f
done
echo "  modül: $(sort -u /tmp/modlist | wc -l)"
find $F/lib/modules -name '*.ko.gz' -exec gunzip {} +
cp $R/lib/modules/$KV/modules.builtin* $R/lib/modules/$KV/modules.order $F/lib/modules/$KV/ 2>/dev/null || true
depmod -b $F $KV
(cd $F && find . | cpio -o -H newc --quiet | gzip -9) > $I/boot/initramfs.gz
ls -la $I/boot/initramfs.gz
cp $R/boot/vmlinuz-lts $I/boot/vmlinuz

echo "== grub"
cp -a $O/boot/grub/themes $I/boot/grub/
cat > $I/boot/grub/grub.cfg <<'EOF'
# Aether 1.0 "Nebula" — canlı ortam
set timeout=10
set default=0
insmod all_video
insmod gfxterm
insmod png
set gfxpayload=keep
loadfont /boot/grub/themes/aether/jetbrains-12.pf2
if loadfont /boot/grub/themes/aether/jetbrains-16.pf2; then
  set gfxmode=1280x800,1366x768,1280x720,1024x768,auto
  terminal_output gfxterm
  set theme=/boot/grub/themes/aether/theme.txt
fi
set ortak="aether.canli aether.etiket=AETHER_1_0"
menuentry "Aether 1.0 Nebula  —  Türkçe" {
  linux /boot/vmlinuz $ortak quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs.gz
}
menuentry "Aether 1.0 Nebula  —  English" {
  linux /boot/vmlinuz $ortak aether.dil=en quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs.gz
}
menuentry "RAM'e yüklemeden  /  No RAM copy" {
  linux /boot/vmlinuz $ortak aether.toram=0 quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs.gz
}
menuentry "Güvenli grafik  /  Safe graphics" {
  linux /boot/vmlinuz $ortak nomodeset
  initrd /boot/initramfs.gz
}
EOF

echo "== iso"
mkdir -p /work/cikti
grub-mkrescue -o /work/cikti/aether-1.0-nebula.iso $I -- -volid AETHER_1_0 2>&1 | tail -2
ls -la /work/cikti/
