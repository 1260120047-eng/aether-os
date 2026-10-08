#!/bin/sh
# Aether 1.1 "Nebula" — ISO derleme betiği (Alpine derleme ortamında çalışır)
set -e
R=/work/rootfs; O=/work/overlay; I=/work/iso; F=/work/initramfs
KV=$(ls $R/lib/modules | head -1)
echo "== çekirdek: $KV"

echo "== ek paketler"
apk add --root $R -q --no-progress xsettingsd plymouth-drm xf86-video-vesa xf86-video-fbdev webkit2gtk-4.1 gst-plugins-good gst-plugins-base \
  vte3 libwnck3 libwnck3-lang libnotify iwd \
  linux-firmware-rtlwifi linux-firmware-rtw88 linux-firmware-rtw89 linux-firmware-ath9k_htc \
  linux-firmware-i915 linux-firmware-radeon linux-firmware-xe linux-firmware-rtl_nic alsa-utils alsa-utils-openrc alsa-ucm-conf 2>&1 | grep -v "^Executing\|grub-probe\|trigger" || true

apk del --root $R -q --no-progress netsurf 2>/dev/null || true
# LXTerminal yerine kendi terminalimiz (aether-terminal)
apk del --root $R -q --no-progress lxterminal lxterminal-lang 2>/dev/null || true

echo "== Intel Wi-Fi firmware (her çipin yalnızca en yeni sürümü)"
# linux-firmware-other 79 MB; içinden sadece iwlwifi dosyalarını, çip başına en yeni API sürümüyle alıyoruz
FW=/tmp/fw-intel; rm -rf $FW; mkdir -p $FW
( cd $FW && apk fetch -q linux-firmware-other && tar xzf linux-firmware-other-*.apk lib/firmware 2>/dev/null ) || true
mkdir -p $R/lib/firmware
rm -f $R/lib/firmware/iwlwifi-*
# sürücünün istediği her dosya için (ör. iwlwifi-bz-a0-fm-c0-93.ucode) en yüksek uygun sürümü seç
IWL=$(find $R/lib/modules/$KV -name 'iwlwifi.ko*' | head -1)
for istek in $(modinfo -F firmware "$IWL" 2>/dev/null); do
  case "$istek" in
    *.pnvm) cp $FW/lib/firmware/$istek* $R/lib/firmware/ 2>/dev/null || true ;;
    *.ucode)
      on=${istek%-*}; enbuyuk=${istek##*-}; enbuyuk=${enbuyuk%.ucode}
      sec=""
      for f in $FW/lib/firmware/$on-[0-9]*.ucode*; do
        [ -e "$f" ] || continue
        n=${f##*/$on-}; n=${n%%.ucode*}
        [ "$n" -le "$enbuyuk" ] 2>/dev/null || continue
        [ -z "$sec" ] || [ "$n" -gt "$secn" ] && { sec=$f; secn=$n; }
      done
      [ -n "$sec" ] && cp "$sec" $R/lib/firmware/ ;;
  esac
done
echo "  iwlwifi: $(ls $R/lib/firmware/iwlwifi-* 2>/dev/null | wc -l) dosya, $(du -ch $R/lib/firmware/iwlwifi-* 2>/dev/null | tail -1 | cut -f1)"
rm -rf $FW

echo "== diğer Wi-Fi firmware'leri (yalnızca sürücülerin istediği dosyalar)"
# büyük paketlerin tamamı yerine, sürücünün modinfo'da istediği dosyaları al
fw_sec() {   # fw_sec PAKET MODÜL...
  pk=$1; shift
  d=/tmp/fw-$pk; rm -rf $d; mkdir -p $d; adet=0
  ( cd $d && apk fetch -q $pk && tar xzf $pk-*.apk lib/firmware 2>/dev/null ) || { rm -rf $d; return 0; }
  for m in "$@"; do
    ko=$(find $R/lib/modules/$KV -name "$m.ko*" | head -1); [ -n "$ko" ] || continue
    for istek in $(modinfo -F firmware "$ko" 2>/dev/null); do
      for f in $d/lib/firmware/$istek $d/lib/firmware/$istek.zst $d/lib/firmware/$istek.xz; do
        [ -e "$f" ] || continue
        h=$R/lib/firmware/${f#$d/lib/firmware/}; mkdir -p "$(dirname "$h")"; cp -L "$f" "$h"; adet=$((adet + 1))
      done
    done
  done
  rm -rf $d
  echo "  $pk: $adet dosya"
  # Sürücüler sayfası bu paketi "kurulu" saysın
  [ $adet -gt 0 ] && { mkdir -p $R/usr/share/aether/firmware; touch $R/usr/share/aether/firmware/$pk; }
  return 0
}
fw_sec linux-firmware-mediatek mt7921e mt7921s mt7921u mt7925e mt7925u mt7615e mt7663e mt7663u mt76x0e mt76x0u mt76x2e mt76x2u mt7601u
fw_sec linux-firmware-ath10k ath10k_pci ath10k_sdio ath10k_usb ath10k_core
fw_sec linux-firmware-ath11k ath11k_pci ath11k
fw_sec linux-firmware-brcm brcmfmac
fw_sec linux-firmware-cypress brcmfmac
rm -f $R/lib/firmware/brcm/*sdio* $R/lib/firmware/cypress/*sdio*   # x86 bilgisayarlarda SDIO kartlar yok
echo "  Wi-Fi firmware: $(du -sh $R/lib/firmware | cut -f1) toplam"
# Openbox yerine kendi pencere yöneticimiz Yörünge
apk del --root $R -q --no-progress openbox openbox-lang 2>/dev/null || true
rm -rf $R/etc/xdg/openbox $R/usr/share/themes/Aether/openbox-3 $R/usr/share/themes/Aether-Koyu/openbox-3
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
rl default dbus elogind iwd alsa dhcpcd udev-postmount aether-canli aether-splash-bitir lightdm local
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
# önceki derlemede silinmiş modülleri geri getir (kablosuz sürücüler artık gerekiyor)
apk fix --root $R -q --no-progress linux-lts 2>/dev/null || true
apk del --root $R -q --no-progress cmake 2>/dev/null || true
M=$R/lib/modules/$KV/kernel
# sürücü desteği için ekran kartı, kamera, kablosuz ve staging sürücüleri artık kalıyor
rm -rf $M/drivers/infiniband $M/drivers/isdn $M/drivers/atm $M/drivers/nfc
chroot $R depmod -a $KV 2>/dev/null || true
rm -rf $R/usr/share/man $R/usr/share/doc $R/usr/share/info $R/usr/share/gtk-doc $R/var/cache/apk/* $R/boot/initramfs-lts $R/boot/System.map*
rm -f $R/etc/resolv.conf

echo "== squashfs"
rm -rf $I; mkdir -p $I/aether $I/boot/grub
mksquashfs $R $I/aether/rootfs.sfs -comp zstd -Xcompression-level 19 -b 1M -noappend -no-progress -quiet \
  -wildcards -e 'proc/*' 'sys/*' 'dev/*' 'run/*' 'tmp/*' 'mnt/*'
ls -la $I/aether/rootfs.sfs

echo "== initramfs"
rm -rf $F; mkdir -p $F/bin $F/sbin $F/proc $F/sys $F/dev $F/run $F/lib/modules/$KV/kernel/drivers
cp /bin/busybox.static $F/bin/busybox
cp /work/src/initramfs/init $F/init; chmod 755 $F/init
# arka planda RAM'e kopyalayan yardımcı (statik)
gcc -static -Os -s /work/src/initramfs/toram.c -o $F/bin/aether-toram
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
# Aether 1.1 "Nebula" — canlı ortam
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
set ortak="aether.canli aether.etiket=AETHER_1_1"
menuentry "Aether 1.1 Nebula  —  Türkçe" {
  linux /boot/vmlinuz $ortak quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs.gz
}
menuentry "Aether 1.1 Nebula  —  English" {
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
grub-mkrescue -o /work/cikti/aether-1.1-nebula.iso $I -- -volid AETHER_1_1 2>&1 | tail -2
ls -la /work/cikti/
