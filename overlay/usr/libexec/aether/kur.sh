#!/bin/sh
# Aether kurulum betiği (root olarak çalışır)
#   kur.sh DISK KULLANICI "AD SOYAD" BILGISAYAR DIL KLAVYE [KIP]   (şifre stdin'den)
#   KIP: tum (varsayılan) — bütün disk Aether'e
#        yanina:GB        — Windows'un yanına: boş alana kur, yetmezse Windows bölümünü küçült
# İlerleme satırları: "@@ yüzde Türkçe mesaj|English message"
set -e
exec 2>&1
DISK="$1"; KUL="$2"; AD="$3"; BIL="$4"; DIL="$5"; KLV="$6"; KIP="${7:-tum}"
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
UEFI=0; [ -d /sys/firmware/efi ] && UEFI=1

p 2 "Disk hazırlanıyor|Preparing disk"
umount -R "$H" 2>/dev/null || true
for b in "$DISK"*; do umount "$b" 2>/dev/null || true; done
bolumler() { lsblk -nrpo NAME,TYPE "$DISK" | awk '$2 == "part" { print $1 }'; }
yenile() { partx -u "$DISK" 2>/dev/null || true; sync; sleep 1; udevadm settle 2>/dev/null || true; }

case "$KIP" in
tum)
  wipefs -a -q "$DISK" 2>/dev/null || true
  sfdisk -q --wipe always "$DISK" <<EOF
label: gpt
size=1MiB, type=21686148-6449-6E6F-744E-656564454649, name="bios"
size=300MiB, type=C12A7328-F81F-11D2-BA4B-00A0C93EC93B, name="EFI"
type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name="aether"
EOF
  yenile
  [ -b "${P}3" ] || { sleep 2; [ -b "${P}3" ] || hata "bölümler oluşmadı"; }
  KOKB="${P}3"; ESPB="${P}2"
  p 5 "Dosya sistemleri oluşturuluyor|Creating file systems"
  mkfs.vfat -F 32 -n AETHEREFI "$ESPB" >/dev/null
  ;;
yanina:*)
  GB=${KIP#yanina:}
  case "$GB" in ''|*[!0-9]*) hata "geçersiz boyut: $GB" ;; esac
  GEREK=$((GB * 1024 * 1024 * 1024))
  oku() { /usr/libexec/aether/disk-bilgi "$DISK" > /tmp/disk-bilgi; . /tmp/disk-bilgi; }
  oku
  [ "$TABLO" = gpt ] || [ "$TABLO" = dos ] || hata "Diskte bölüm tablosu yok; 'bütün diski kullan' seçeneğini seç."
  if [ "$TABLO" = gpt ] && [ $UEFI = 0 ]; then
    hata "Bu diskteki Windows UEFI ile kurulu ama Aether şu an eski BIOS kipinde açıldı. Bilgisayarın açılış menüsünden USB'yi 'UEFI' seçeneğiyle başlat."
  fi
  if [ "$TABLO" = gpt ] && [ -z "$ESP" ]; then hata "EFI sistem bölümü bulunamadı."; fi
  if [ "$TABLO" = dos ] && [ "$BOLUM_SAYI" -ge 4 ]; then hata "Diskte 4 birincil bölüm var; yeni bölüm açılamıyor."; fi
  ALT=$((16 * 1024 * 1024))
  if [ "${BOS:-0}" -lt $((GEREK + ALT)) ]; then
    # yer yok: Windows bölümünü sondan küçült
    [ -n "$WINDOWS" ] || hata "Yeterli boş alan yok ve küçültülecek bir Windows bölümü bulunamadı."
    case "$WIN_DURUM" in
      tamam) ;;
      uyku) hata "Windows hazırda bekletmede ya da 'Hızlı Başlatma' açık. Windows'ta Denetim Masası > Güç Seçenekleri'nden Hızlı Başlatma'yı kapat, Windows'u 'Yeniden Başlat' ile kapat ve tekrar dene." ;;
      kirli) hata "Windows bölümünde düzeltilmemiş hatalar var. Windows'u açıp 'chkdsk /f' çalıştır, sonra tekrar dene." ;;
      bitlocker) hata "Windows bölümü BitLocker ile şifreli; küçültülemez. Önce Windows'ta BitLocker'ı kapat." ;;
      *) hata "Windows bölümü okunamadı." ;;
    esac
    EKSIK=$((GEREK + ALT - BOS))
    YENI=$(( (WIN_BOYUT - EKSIK) / 1048576 * 1048576 ))
    PAY=$((8 * 1024 * 1024 * 1024))   # Windows'a en az 8 GB boş yer kalsın
    [ $YENI -ge $((WIN_MIN + PAY)) ] || hata "Windows bölümünde yeterli boş yer yok. Aether için daha az yer ayır ya da Windows'ta yer aç."
    p 3 "Windows bölümü küçültülüyor ($((WIN_BOYUT / 1073741824)) GB → $((YENI / 1073741824)) GB)|Shrinking the Windows partition ($((WIN_BOYUT / 1073741824)) GB → $((YENI / 1073741824)) GB)"
    ntfsresize --info --force --no-action "$WINDOWS" >/dev/null || hata "Windows bölümü denetlenemedi."
    echo y | ntfsresize --force --size "$YENI" "$WINDOWS" || hata "Windows bölümü küçültülemedi (Windows dosyalarına dokunulmadı)."
    # bölümü de yeni boyuta indir (başlangıç aynı kalır)
    echo ",$((YENI / SEKTOR))" | sfdisk -q -N "$WIN_NO" --no-reread "$DISK" || hata "bölüm tablosu güncellenemedi"
    yenile
    oku
  fi
  [ "${BOS:-0}" -ge $((GEREK / 2)) ] || hata "Boş alan oluşturulamadı."
  # boş alanın başına, 1 MiB hizalı yeni bölüm
  BAS=$(( (BOS_BAS + 2047) / 2048 * 2048 ))
  SEK=$(( BOS_SEK - (BAS - BOS_BAS) ))
  [ $((GEREK / SEKTOR)) -lt $SEK ] && SEK=$((GEREK / SEKTOR))
  ONCE=$(bolumler)
  if [ "$TABLO" = gpt ]; then TUR=0FC63DAF-8483-4772-8E79-3D69D8477DE4; else TUR=83; fi
  echo "$BAS,$SEK,$TUR" | sfdisk -q --append --no-reread "$DISK" || hata "yeni bölüm oluşturulamadı"
  yenile
  KOKB=""
  for b in $(bolumler); do echo "$ONCE" | grep -qx "$b" || KOKB=$b; done
  [ -n "$KOKB" ] && [ -b "$KOKB" ] || hata "yeni bölüm bulunamadı"
  ESPB="$ESP"
  [ "$TABLO" = dos ] && ESPB=""
  p 5 "Dosya sistemi oluşturuluyor|Creating file system"
  ;;
*) hata "bilinmeyen kurulum türü: $KIP" ;;
esac

mkfs.ext4 -q -F -L aether "$KOKB"
mkdir -p "$H"
mount "$KOKB" "$H"
if [ -n "$ESPB" ]; then
  mkdir -p "$H/boot/efi"
  mount "$ESPB" "$H/boot/efi"
fi

p 8 "Aether dosyaları kopyalanıyor|Copying Aether files"
unsquashfs -f -d "$H" -percentage "$SFS" 2>/dev/null | while read -r y; do
  case "$y" in ''|*[!0-9]*) ;; *) echo "@@ $((8 + y * 70 / 100)) Aether dosyaları kopyalanıyor ($y%)|Copying Aether files ($y%)" ;; esac
done
[ -x "$H/usr/lib/systemd/systemd" ] || hata "kopyalama başarısız"

p 80 "Sistem ayarlanıyor|Configuring system"
uuid() { blkid -o value -s UUID "$1"; }
KOK=$(uuid "$KOKB"); EFI=""; [ -n "$ESPB" ] && EFI=$(uuid "$ESPB")
[ -n "$KOK" ] || hata "disk kimlikleri okunamadı"
{
  echo "# Aether"
  echo "UUID=$KOK  /          ext4  rw,relatime   0 1"
  [ -n "$EFI" ] && echo "UUID=$EFI  /boot/efi  vfat  rw,umask=0077 0 2"
  echo "tmpfs      /tmp       tmpfs nosuid,nodev  0 0"
} > "$H/etc/fstab"
echo "$BIL" > "$H/etc/hostname"
printf '127.0.0.1\tlocalhost %s\n::1\t\tlocalhost %s\n' "$BIL" "$BIL" > "$H/etc/hosts"

# Dil ve klavye (sistem geneli)
sed -i "s/^DIL=.*/DIL=$DIL/; s/^KLAVYE=.*/KLAVYE=$KLV/" "$H/etc/aether/ayarlar"
case "$KLV" in
  trf) KM=trf; XL=tr; XV=f ;;
  us)  KM=us;  XL=us; XV= ;;
  *)   KM=trq; XL=tr; XV= ;;
esac
printf 'KEYMAP=%s\nFONT=\n' "$KM" > "$H/etc/vconsole.conf"
mkdir -p "$H/etc/X11/xorg.conf.d"
cat > "$H/etc/X11/xorg.conf.d/00-klavye.conf" <<EOF
Section "InputClass"
    Identifier "Aether klavye"
    MatchIsKeyboard "on"
    Option "XkbLayout" "$XL"
    $( [ -n "$XV" ] && echo "Option \"XkbVariant\" \"$XV\"" )
EndSection
EOF
LNG=tr_TR.UTF-8; [ "$DIL" = en ] && LNG=en_US.UTF-8
echo "LANG=$LNG" > "$H/etc/locale.conf"

p 84 "Kullanıcı oluşturuluyor|Creating user"
for d in dev proc sys run; do mkdir -p "$H/$d"; mount --bind /$d "$H/$d"; done
mkdir -p "$H/tmp" "$H/mnt"; chmod 1777 "$H/tmp"
cp /etc/resolv.conf "$H/etc/resolv.conf" 2>/dev/null || true
chroot "$H" useradd -m -s /bin/bash -c "$AD" -G wheel,audio,video,input,optical,storage,users "$KUL"
printf '%s:%s\n' "$KUL" "$SIFRE" | chroot "$H" chpasswd
chroot "$H" passwd -l root >/dev/null 2>&1 || true
mkdir -p "$H/home/$KUL/.config/aether"
cp "$H/etc/aether/ayarlar" "$H/home/$KUL/.config/aether/ayarlar"
chroot "$H" chown -R "$KUL:$KUL" "/home/$KUL"

p 88 "Önyükleme hazırlanıyor|Preparing boot"
# yalnızca bu bilgisayarın donanımına göre küçük bir initramfs (mkinitcpio)
chroot "$H" mkinitcpio -p linux >/dev/null 2>&1 || chroot "$H" mkinitcpio -p linux
p 93 "Önyükleyici kuruluyor|Installing boot loader"
if [ "$KIP" = tum ]; then
  # bütün disk: hem BIOS hem UEFI ile açılabilsin
  chroot "$H" grub-install --target=i386-pc --boot-directory=/boot "$DISK" >/dev/null 2>&1 || echo "(BIOS grub-install uyarı verdi)"
  chroot "$H" grub-install --target=x86_64-efi --efi-directory=/boot/efi --boot-directory=/boot --removable --no-nvram >/dev/null 2>&1 || echo "(UEFI grub-install uyarı verdi)"
elif [ -n "$ESPB" ]; then
  # Windows'un yanına (UEFI): Windows'un önyükleyicisine dokunma; kendi girdimizi ekle ve ilk sıraya koy
  mkdir -p "$H/sys/firmware/efi/efivars"; mount -t efivarfs efivarfs "$H/sys/firmware/efi/efivars" 2>/dev/null || true
  chroot "$H" grub-install --target=x86_64-efi --efi-directory=/boot/efi --boot-directory=/boot --bootloader-id=Aether || hata "önyükleyici kurulamadı"
else
  # Windows'un yanına (BIOS/MBR): GRUB diskin başına, Windows menüden açılır
  chroot "$H" grub-install --target=i386-pc --boot-directory=/boot "$DISK" || hata "önyükleyici kurulamadı"
fi
# Windows menü girdisi
WIN_GIRDI=""
if [ "$KIP" != tum ]; then
  if [ -n "$ESPB" ] && [ -f "$H/boot/efi/EFI/Microsoft/Boot/bootmgfw.efi" ]; then
    WIN_GIRDI="menuentry \"Windows\" --class windows {
  insmod part_gpt
  insmod fat
  search --no-floppy --fs-uuid --set=root $EFI
  chainloader /EFI/Microsoft/Boot/bootmgfw.efi
}"
  elif [ -n "${WIN_BOOT:-}" ]; then
    WU=$(uuid "$WIN_BOOT")
    WIN_GIRDI="menuentry \"Windows\" --class windows {
  insmod part_msdos
  insmod ntfs
  insmod ntldr
  search --no-floppy --fs-uuid --set=root $WU
  ntldr /bootmgr
}"
  fi
fi
SURE=3; [ -n "$WIN_GIRDI" ] && SURE=10
cat > "$H/boot/grub/grub.cfg" <<EOF
# Aether 2.0 "Orion"
set timeout=$SURE
set default=0
insmod all_video
insmod gfxterm
insmod png
insmod part_gpt
insmod part_msdos
insmod ext2
set gfxpayload=keep
search --no-floppy --fs-uuid --set=root $KOK
loadfont /boot/grub/themes/aether/jetbrains-12.pf2
if loadfont /boot/grub/themes/aether/jetbrains-16.pf2; then
  set gfxmode=1280x800,1366x768,1280x720,1024x768,auto
  terminal_output gfxterm
  set theme=/boot/grub/themes/aether/theme.txt
fi
menuentry "Aether 2.0 Orion" --class aether {
  linux /boot/vmlinuz-linux root=UUID=$KOK rw rootfstype=ext4 quiet loglevel=3 splash vt.global_cursor_default=0 console=tty3 plymouth.ignore-serial-consoles
  initrd /boot/initramfs-linux.img
}
$WIN_GIRDI
menuentry "Aether 2.0 Orion ($( [ "$DIL" = en ] && echo 'safe graphics' || echo 'güvenli grafik' ))" --class aether {
  linux /boot/vmlinuz-linux root=UUID=$KOK rw rootfstype=ext4 nomodeset
  initrd /boot/initramfs-linux.img
}
EOF

p 97 "Temizleniyor|Cleaning up"
rm -f "$H/etc/lightdm/lightdm.conf.d/50-canli.conf"
sed -i '/^permit nopass canli$/d' "$H/etc/doas.conf"
rm -f "$H/etc/sudoers.d/canli"
sync
umount "$H/sys/firmware/efi/efivars" 2>/dev/null || true
for d in run sys proc dev; do umount -l "$H/$d" 2>/dev/null || true; done
[ -n "$ESPB" ] && umount "$H/boot/efi"
umount "$H"
p 100 "Kurulum tamamlandı|Installation complete"
