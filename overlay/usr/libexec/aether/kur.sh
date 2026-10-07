#!/bin/sh
# Aether kurulum betiği (root olarak çalışır)
#   kur.sh DISK KULLANICI "AD SOYAD" BILGISAYAR DIL KLAVYE   (şifre stdin'den)
# İlerleme satırları: "@@ yüzde Türkçe mesaj|English message"
set -e
exec 2>&1
DISK="$1"; KUL="$2"; AD="$3"; BIL="$4"; DIL="$5"; KLV="$6"
IFS= read -r SIFRE || true
H=/mnt/aether-kur
p() { echo "@@ $1 $2"; }
hata() { echo "HATA: $*"; exit 1; }

[ "$(id -u)" = 0 ] || hata "root gerekli"
[ -b "$DISK" ] || hata "disk bulunamadı: $DISK"
[ -n "$KUL" ] && [ -n "$SIFRE" ] || hata "kullanıcı adı/şifre eksik"
case "$DISK" in *nvme*|*mmcblk*|*loop*) P="${DISK}p" ;; *) P="$DISK" ;; esac

SFS=/run/aether/rootfs.sfs
[ -f "$SFS" ] || SFS=/run/aether/ortam/aether/rootfs.sfs
[ -f "$SFS" ] || hata "sistem görüntüsü bulunamadı"

p 2 "Disk hazırlanıyor|Preparing disk"
umount -R "$H" 2>/dev/null || true
for b in "$DISK"*; do umount "$b" 2>/dev/null || true; done
wipefs -a -q "$DISK" 2>/dev/null || true
sfdisk -q --wipe always "$DISK" <<EOF
label: gpt
size=1MiB, type=21686148-6449-6E6F-744E-656564454649, name="bios"
size=300MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="EFI"
type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="aether"
EOF
sync; sleep 1; command -v udevadm >/dev/null && udevadm settle || mdev -s
[ -b "${P}3" ] || { sleep 2; [ -b "${P}3" ] || hata "bölümler oluşmadı"; }

p 5 "Dosya sistemleri oluşturuluyor|Creating file systems"
mkfs.vfat -F 32 -n AETHEREFI "${P}2" >/dev/null
mkfs.ext4 -q -F -L aether "${P}3"
mkdir -p "$H"
mount "${P}3" "$H"
mkdir -p "$H/boot/efi"
mount "${P}2" "$H/boot/efi"

p 8 "Aether dosyaları kopyalanıyor|Copying Aether files"
unsquashfs -f -d "$H" -percentage "$SFS" 2>/dev/null | while read -r y; do
  case "$y" in ''|*[!0-9]*) ;; *) echo "@@ $((8 + y * 70 / 100)) Aether dosyaları kopyalanıyor ($y%)|Copying Aether files ($y%)" ;; esac
done
[ -x "$H/sbin/init" ] || hata "kopyalama başarısız"

p 80 "Sistem ayarlanıyor|Configuring system"
uuid() { blkid "$1" | sed -n 's/.* UUID="\([^"]*\)".*/\1/p'; }
KOK=$(uuid "${P}3"); EFI=$(uuid "${P}2")
[ -n "$KOK" ] && [ -n "$EFI" ] || hata "disk kimlikleri okunamadı"
cat > "$H/etc/fstab" <<EOF
# Aether
UUID=$KOK  /          ext4  rw,relatime   0 1
UUID=$EFI  /boot/efi  vfat  rw,umask=0077 0 2
tmpfs      /tmp       tmpfs nosuid,nodev  0 0
EOF
echo "$BIL" > "$H/etc/hostname"
printf '127.0.0.1\tlocalhost %s\n::1\t\tlocalhost %s\n' "$BIL" "$BIL" > "$H/etc/hosts"

# Dil ve klavye (sistem geneli)
sed -i "s/^DIL=.*/DIL=$DIL/; s/^KLAVYE=.*/KLAVYE=$KLV/" "$H/etc/aether/ayarlar"
case "$KLV" in
  trf) KM=tr/tr-f; XL=tr; XV=f ;;
  us)  KM=us/us;   XL=us; XV= ;;
  *)   KM=tr/tr;   XL=tr; XV= ;;
esac
mkdir -p "$H/etc/keymap"
cp "$H/usr/share/bkeymaps/$KM.bmap.gz" "$H/etc/keymap/$(basename $KM).bmap.gz" 2>/dev/null || true
echo "KEYMAP=/etc/keymap/$(basename $KM).bmap.gz" > "$H/etc/conf.d/loadkmap"
cat > "$H/etc/X11/xorg.conf.d/00-klavye.conf" <<EOF
Section "InputClass"
    Identifier "Aether klavye"
    MatchIsKeyboard "on"
    Option "XkbLayout" "$XL"
    $( [ -n "$XV" ] && echo "Option \"XkbVariant\" \"$XV\"" )
EndSection
EOF
LNG=tr_TR.UTF-8; [ "$DIL" = en ] && LNG=en_US.UTF-8
echo "LANG=$LNG" > "$H/etc/aether/dil"

p 84 "Kullanıcı oluşturuluyor|Creating user"
for d in dev proc sys run; do mkdir -p "$H/$d"; mount --bind /$d "$H/$d"; done
mkdir -p "$H/tmp" "$H/mnt"; chmod 1777 "$H/tmp"
cp /etc/resolv.conf "$H/etc/resolv.conf" 2>/dev/null || true
chroot "$H" adduser -D -s /bin/bash -g "$AD" "$KUL"
printf '%s:%s\n' "$KUL" "$SIFRE" | chroot "$H" chpasswd
for g in wheel audio video input netdev plugdev cdrom users; do chroot "$H" addgroup "$KUL" "$g" 2>/dev/null || true; done
chroot "$H" passwd -l root >/dev/null 2>&1 || true
mkdir -p "$H/home/$KUL/.config/aether"
cp "$H/etc/aether/ayarlar" "$H/home/$KUL/.config/aether/ayarlar"
chroot "$H" chown -R "$KUL:$KUL" "/home/$KUL"

p 88 "Önyükleme hazırlanıyor|Preparing boot"
KV=$(ls "$H/lib/modules" | head -1)
chroot "$H" mkinitfs -q "$KV" || chroot "$H" mkinitfs "$KV"
p 93 "Önyükleyici kuruluyor|Installing boot loader"
chroot "$H" grub-install --target=i386-pc --boot-directory=/boot "$DISK" >/dev/null 2>&1 || echo "(BIOS grub-install uyarı verdi)"
chroot "$H" grub-install --target=x86_64-efi --efi-directory=/boot/efi --boot-directory=/boot --removable --no-nvram >/dev/null 2>&1 || echo "(UEFI grub-install uyarı verdi)"
cat > "$H/boot/grub/grub.cfg" <<EOF
# Aether 1.0 "Nebula"
set timeout=3
set default=0
insmod all_video
insmod gfxterm
insmod png
insmod part_gpt
insmod ext2
set gfxpayload=keep
search --no-floppy --fs-uuid --set=root $KOK
loadfont /boot/grub/themes/aether/jetbrains-12.pf2
if loadfont /boot/grub/themes/aether/jetbrains-16.pf2; then
  set gfxmode=1280x800,1366x768,1280x720,1024x768,auto
  terminal_output gfxterm
  set theme=/boot/grub/themes/aether/theme.txt
fi
menuentry "Aether 1.0 Nebula" --class aether {
  linux /boot/vmlinuz-lts root=UUID=$KOK rootfstype=ext4 modules=sd-mod,usb-storage,ext4 quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs-lts
}
menuentry "Aether 1.0 Nebula ($( [ "$DIL" = en ] && echo 'safe graphics' || echo 'güvenli grafik' ))" --class aether {
  linux /boot/vmlinuz-lts root=UUID=$KOK rootfstype=ext4 modules=sd-mod,usb-storage,ext4 nomodeset
  initrd /boot/initramfs-lts
}
EOF

p 97 "Temizleniyor|Cleaning up"
rm -f "$H/etc/doas.d/canli.conf" "$H/etc/lightdm/lightdm.conf.d/50-canli.conf"
sync
for d in run sys proc dev; do umount -l "$H/$d" 2>/dev/null || true; done
umount "$H/boot/efi"; umount "$H"
p 100 "Kurulum tamamlandı|Installation complete"
