#!/bin/bash
# Aether 2.0 "Orion" — ISO derleme betiği (Arch derleme ortamında çalışır; önce s/rootfs.sh)
set -e
R=/work/rootfs; O=/work/overlay; I=/work/iso; F=/work/initramfs
KV=$(ls $R/usr/lib/modules | head -1)
MD=$R/usr/lib/modules/$KV
FWD=$R/usr/lib/firmware
echo "== çekirdek: $KV"

# derleme ortamının araçları
pacman -S --needed --noconfirm -q mtools libisoburn grub squashfs-tools cpio zstd kmod busybox \
  python-pillow python-numpy ttf-jetbrains-mono imagemagick libjxl curl \
  base-devel gtk3 webkit2gtk-4.1 vte3 libwnck3 libxft libxcursor libxrandr sdl2-compat json-glib libsoup3 librsvg >/dev/null

echo "== görseller"
python3 /work/src/art/art.py

echo "== firmware dilimleri (yalnızca sürücülerin istediği dosyalar)"
# paket_al PAKET → /tmp/fw-PAKET/usr/lib/firmware
paket_al() {
  local d=/tmp/fw-$1; rm -rf $d; mkdir -p $d/cache
  pacman -Sw --noconfirm -q --cachedir $d/cache "$1" >/dev/null || return 1
  tar --zstd -xf $d/cache/$1-[0-9]*.pkg.tar.zst -C $d usr/lib/firmware 2>/dev/null || true
}
isaretle() { mkdir -p $R/usr/share/aether/firmware; touch $R/usr/share/aether/firmware/$1; }
# fw_kopyala PAKET MODÜL...: modinfo'daki her firmware isteği için dosyayı (sıkıştırılmış hâliyle) kopyala
fw_kopyala() {
  local pk=$1 d=/tmp/fw-$1/usr/lib/firmware adet=0; shift
  for m in "$@"; do
    ko=$(find $MD/kernel -name "$m.ko*" | head -1); [ -n "$ko" ] || continue
    for istek in $(modinfo -F firmware "$ko" 2>/dev/null); do
      for f in $d/$istek $d/$istek.zst $d/$istek.xz; do
        [ -e "$f" ] || continue
        h=$FWD/${f#$d/}; mkdir -p "$(dirname "$h")"; cp -rL "$f" "$h"; adet=$((adet + 1))
      done
    done
  done
  echo "  $pk ($*): $adet dosya"
  [ $adet -gt 0 ]
}

# Intel: ekran kartı (i915, xe) + Wi-Fi. iwlwifi için her çipin, çekirdeğin desteklediği en yüksek API sürümü
paket_al linux-firmware-intel
fw_kopyala linux-firmware-intel i915 xe || true
D=/tmp/fw-linux-firmware-intel/usr/lib/firmware
IWL=$(find $MD/kernel -name 'iwlwifi.ko*' | head -1)
for istek in $(modinfo -F firmware "$IWL" 2>/dev/null); do
  case "$istek" in
    *.pnvm) cp -L $D/$istek* $FWD/ 2>/dev/null || true ;;
    *.ucode)
      on=${istek%-*}; enbuyuk=${istek##*-}; enbuyuk=${enbuyuk%.ucode}
      sec=""; secn=0
      for f in $D/$on-[0-9]*.ucode*; do
        [ -e "$f" ] || continue
        n=${f##*/$on-}; n=${n%%.ucode*}
        [ "$n" -le "$enbuyuk" ] 2>/dev/null || continue
        if [ -z "$sec" ] || [ "$n" -gt "$secn" ]; then sec=$f; secn=$n; fi
      done
      [ -n "$sec" ] && cp -L "$sec" $FWD/ ;;
  esac
done
echo "  iwlwifi: $(ls $FWD/iwlwifi-* | wc -l) dosya"
isaretle linux-firmware-intel

paket_al linux-firmware-atheros
fw_kopyala linux-firmware-atheros ath9k_htc ath10k_pci ath10k_usb ath10k_core ath11k_pci ath11k ath12k ath6kl_usb && isaretle linux-firmware-atheros
paket_al linux-firmware-mediatek
fw_kopyala linux-firmware-mediatek mt7921e mt7921u mt7925e mt7925u mt7615e mt7663e mt7663u mt76x0e mt76x0u mt76x2e mt76x2u mt7601u mt7996e mt7915e && isaretle linux-firmware-mediatek
paket_al linux-firmware-broadcom
fw_kopyala linux-firmware-broadcom brcmfmac && isaretle linux-firmware-broadcom
rm -f $FWD/brcm/*sdio* $FWD/cypress/*sdio*   # x86 bilgisayarlarda SDIO kartlar yok
rm -rf /tmp/fw-linux-firmware-*
echo "  firmware toplam: $(du -sh $FWD | cut -f1)"

echo "== uygulama mağazası kataloğu"
# AppStream: uygulama adları, açıklamaları, kategoriler ve simgeler (Arch depoları)
# ntfs-3g + ntfsprogs: Windows bölümlerini okuma/yazma ve Windows'un yanına kurarken küçültme
pacman -r $R -S --needed --noconfirm archlinux-appstream-data ntfs-3g ntfsprogs gvfs udisks2 flatpak xdg-desktop-portal-gtk virtualbox-guest-utils spice-vdagent open-vm-tools >/dev/null 2>&1 || pacman -r $R -S --needed --noconfirm archlinux-appstream-data ntfs-3g ntfsprogs gvfs udisks2 flatpak xdg-desktop-portal-gtk virtualbox-guest-utils spice-vdagent open-vm-tools
# Flathub: sistem genelinde tanımlı olsun; Mağaza kullanıcıya özel kurar (parola gerekmez)
mkdir -p $R/etc/flatpak/remotes.d
curl -fsSL --retry 3 https://dl.flathub.org/repo/flathub.flatpakrepo -o $R/etc/flatpak/remotes.d/flathub.flatpakrepo || echo "  (Flathub tanımı indirilemedi)"
# kendi dosya yöneticimiz (aether-dosyalar) geldiği için PCManFM'e gerek yok
pacman -r $R -Rns --noconfirm pcmanfm >/dev/null 2>&1 || true
rm -rf $R/etc/xdg/libfm
# simgeler JPEG XL; mağaza hızlı açılsın diye PNG'ye çevir, JXL'leri ISO'ya koyma
# önceki derlemede JXL'ler silindiyse kataloğu yeniden aç
[ -d $R/usr/share/swcatalog/icons ] || pacman -r $R -S --noconfirm archlinux-appstream-data >/dev/null 2>&1
SM=$R/usr/share/aether/magaza/simgeler; rm -rf $SM; mkdir -p $SM
# her simge için en uygun boyut: önce 64, yoksa 128, yoksa 48
for b in 64x64 128x128 48x48; do
  for f in $R/usr/share/swcatalog/icons/archlinux-arch-*/$b/*.jxl; do
    [ -e "$f" ] || continue
    h="$SM/$(basename "${f%.jxl}").png"; [ -e "$h" ] && continue
    if [ $b = 128x128 ]; then djxl "$f" /tmp/s.png >/dev/null 2>&1 && magick /tmp/s.png -resize 64x64 "$h"; else djxl "$f" "$h" >/dev/null 2>&1; fi
  done
done
rm -rf $R/usr/share/swcatalog/icons
echo "  simge: $(ls $SM | wc -l)"

echo "== uygulamalar"
make -s -C /work/src/apps clean all
make -s -C /work/src/apps install DESTDIR=$R

echo "== overlay"
cp -a $O/. $R/
chmod 600 $R/etc/doas.conf; chown root:root $R/etc/doas.conf
chown -R root:root $R/etc/sudoers.d; chmod 440 $R/etc/sudoers.d/aether
chmod 755 $R/usr/libexec/aether/* $R/usr/bin/aether-* 2>/dev/null || true
rm -f $R/usr/share/xsessions/openbox.desktop

echo "== servisler"
systemctl --root=$R enable lightdm.service iwd.service dhcpcd.service systemd-timesyncd.service \
  aether-canli.service aether-pacman-anahtar.service aether-grafik.service \
  vboxservice.service spice-vdagentd.socket vmtoolsd.service 2>&1 | grep -v '^Created symlink' || true
systemctl --root=$R set-default graphical.target >/dev/null 2>&1

echo "== sistem ayarları"
sed -i 's/^root:[^:]*:/root:!*:/' $R/etc/shadow
: > $R/etc/machine-id
# her sistem kendi pacman anahtarını üretsin (aether-pacman-anahtar.service)
rm -rf $R/etc/pacman.d/gnupg
# Aether kimliği güncellemelerde ezilmesin
grep -q '^NoUpgrade.*os-release' $R/etc/pacman.conf || \
  sed -i 's|^#\?NoUpgrade *=.*|NoUpgrade   = usr/lib/os-release etc/issue|' $R/etc/pacman.conf
echo 'Aether 2.0 "Orion" \r (\l)' > $R/etc/issue; echo >> $R/etc/issue
# kurulu sistemde açılış ekranı: mkinitcpio'ya plymouth kancası
grep -q '^HOOKS=.*plymouth' $R/etc/mkinitcpio.conf || sed -i 's/^\(HOOKS=(base systemd\)/\1 plymouth/' $R/etc/mkinitcpio.conf
grep '^HOOKS' $R/etc/mkinitcpio.conf
mkdir -p $R/etc/systemd/logind.conf.d
printf '[Login]\nHandlePowerKey=poweroff\n' > $R/etc/systemd/logind.conf.d/aether.conf
# günlük RAM'de şişmesin
mkdir -p $R/etc/systemd/journald.conf.d
printf '[Journal]\nSystemMaxUse=50M\n' > $R/etc/systemd/journald.conf.d/aether.conf
chroot $R fc-cache -f >/dev/null 2>&1 || true
for t in hicolor Adwaita; do chroot $R gtk-update-icon-cache -q -f /usr/share/icons/$t 2>/dev/null || true; done
chroot $R glib-compile-schemas /usr/share/glib-2.0/schemas 2>/dev/null || true

echo "== budama"
rm -rf $MD/kernel/drivers/infiniband $MD/kernel/drivers/isdn $MD/kernel/drivers/atm $MD/kernel/drivers/nfc
depmod -b $R $KV
# yalnızca Türkçe ve İngilizce çeviriler; GObject introspection XML'leri (yalnızca geliştirme)
find $R/usr/share/locale -mindepth 1 -maxdepth 1 ! -name 'tr*' ! -name 'en*' ! -name locale.alias -exec rm -rf {} +
rm -rf $R/usr/share/gir-1.0
grep -q '^NoExtract' $R/etc/pacman.conf || sed -i '/^#NoExtract/a NoExtract   = usr/share/locale/* !usr/share/locale/tr* !usr/share/locale/en* !usr/share/locale/locale.alias usr/share/gir-1.0/* usr/share/man/* usr/share/doc/* usr/share/info/* usr/share/gtk-doc/*' $R/etc/pacman.conf
grep -q 'swcatalog' $R/etc/pacman.conf || sed -i 's|^NoExtract .*|& usr/share/swcatalog/icons/*|' $R/etc/pacman.conf
grep '^NoExtract' $R/etc/pacman.conf
# Mağaza bunları kaldırtmaz (Aether'in kendi parçaları)
pacman -r $R -Qqe > $R/usr/share/aether/temel-paketler
# OpenCL için Mesa sürücü kopyaları; masaüstü ve OpenGL kullanmıyor
rm -rf $R/usr/lib/gallium-pipe
rm -rf $R/usr/share/man $R/usr/share/doc $R/usr/share/info $R/usr/share/gtk-doc \
  $R/var/cache/pacman/pkg/* $R/boot/initramfs-linux*.img $R/var/log/pacman.log
# boş ama gerçek bir dosya olsun; yoksa systemd onu çalışmayan systemd-resolved'e bağlıyor ve dhcpcd DNS yazamıyor
: > $R/etc/resolv.conf

# ilk açılışta "/usr değişti" diye ldconfig, hwdb, kullanıcılar ve günlük kataloğu yeniden
# kurulmasın (TCG'de bir dakikadan fazla sürüyordu): hepsini burada yap, zaman damgalarını eşitle
chroot $R ldconfig
chroot $R systemd-hwdb update 2>/dev/null || true
chroot $R journalctl --update-catalog 2>/dev/null || true
chroot $R systemd-sysusers >/dev/null 2>&1 || true
for f in $R/etc/.updated $R/var/.updated; do
  printf 'TIMESTAMP_NSEC=%s\n' "$(stat -c %Y $R/usr)000000000" > $f
  touch -r $R/usr $f
done

echo "== squashfs"
rm -rf $I; mkdir -p $I/aether $I/boot/grub
mksquashfs $R $I/aether/rootfs.sfs -comp zstd -Xcompression-level 19 -b 1M -noappend -no-progress -quiet \
  -wildcards -e 'proc/*' 'sys/*' 'dev/*' 'run/*' 'tmp/*' 'mnt/*'
ls -la $I/aether/rootfs.sfs

echo "== initramfs"
rm -rf $F; mkdir -p $F/bin $F/sbin $F/proc $F/sys $F/dev $F/run $F/usr/lib/modules/$KV
ln -s usr/lib $F/lib
cp /usr/bin/busybox $F/bin/busybox
cp /work/src/initramfs/init $F/init; chmod 755 $F/init
# arka planda RAM'e kopyalayan yardımcı (statik)
gcc -static -Os -s /work/src/initramfs/toram.c -o $F/bin/aether-toram
GEREK="isofs squashfs overlay loop cdrom sr_mod sd_mod usb_storage uas ahci ata_piix ata_generic pata_acpi
 virtio_blk virtio_scsi virtio_pci virtio_mmio nvme xhci_pci xhci_hcd ehci_pci ehci_hcd ohci_pci ohci_hcd uhci_hcd
 vfat nls_cp437 nls_utf8 nls_iso8859_1 ext4 sdhci_pci sdhci_acpi mmc_block hid_generic usbhid atkbd i8042
 vmw_pvscsi mptspi BusLogic hv_storvsc hv_vmbus pata_oldpiix pata_mpiix mpt3sas"
DEP=$MD/modules.dep
yol() { grep -E "(^|/)$(echo $1 | sed 's/[_-]/[_-]/g')\.ko(\.gz|\.xz|\.zst)?:" $DEP | head -1; }
: > /tmp/modlist
for m in $GEREK; do
  s_=$(yol $m) || true; [ -n "$s_" ] || continue
  echo "$s_" | tr -d ':' | tr ' ' '\n' | grep . >> /tmp/modlist
done
sort -u /tmp/modlist | while read -r f; do
  mkdir -p $F/usr/lib/modules/$KV/$(dirname $f); cp $MD/$f $F/usr/lib/modules/$KV/$f
done
echo "  modül: $(sort -u /tmp/modlist | wc -l)"
# busybox modprobe sıkıştırılmış modül açamaz
find $F/usr/lib/modules -name '*.ko.zst' -exec zstd -q -d --rm {} +
find $F/usr/lib/modules -name '*.ko.xz' -exec xz -d {} +
cp $MD/modules.builtin* $MD/modules.order $F/usr/lib/modules/$KV/ 2>/dev/null || true
depmod -b $F $KV
(cd $F && find . | cpio -o -H newc --quiet | zstd -19 -q -T0) > $I/boot/initramfs.img
ls -la $I/boot/initramfs.img
cp $MD/vmlinuz $I/boot/vmlinuz

echo "== grub"
cp -a $O/boot/grub/themes $I/boot/grub/
cat > $I/boot/grub/grub.cfg <<'EOF'
# Aether 2.0 "Orion" — canlı ortam
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
set ortak="aether.canli aether.etiket=AETHER_2_0"
set sessiz="quiet loglevel=3 rd.udev.log_level=3 systemd.show_status=auto splash vt.global_cursor_default=0 plymouth.ignore-serial-consoles"
menuentry "Aether 2.0 Orion  —  Türkçe" {
  linux /boot/vmlinuz $ortak $sessiz
  initrd /boot/initramfs.img
}
menuentry "Aether 2.0 Orion  —  English" {
  linux /boot/vmlinuz $ortak aether.dil=en systemd.setenv=LANG=en_US.UTF-8 vconsole.keymap=us $sessiz
  initrd /boot/initramfs.img
}
menuentry "RAM'e yüklemeden  /  No RAM copy" {
  linux /boot/vmlinuz $ortak aether.toram=0 $sessiz
  initrd /boot/initramfs.img
}
menuentry "Güvenli grafik  /  Safe graphics" {
  linux /boot/vmlinuz $ortak nomodeset
  initrd /boot/initramfs.img
}
EOF

echo "== iso"
mkdir -p /work/cikti
grub-mkrescue -o /work/cikti/aether-2.0-orion.iso $I -- -volid AETHER_2_0 2>&1 | tail -2
ls -la /work/cikti/
