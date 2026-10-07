#!/bin/sh
# vm.sh basla [disk|cd] | ss AD | tus TUŞ... | yaz METİN | dur | mon KOMUT
W=/work/vm; mkdir -p $W
mon() { echo "$*" | socat - UNIX-CONNECT:$W/mon.sock >/dev/null 2>&1; }
case "$1" in
  basla)
    [ -f $W/disk.qcow2 ] || qemu-img create -f qcow2 $W/disk.qcow2 10G >/dev/null
    BOOT=d; [ "$2" = disk ] && BOOT=c
    CD="-cdrom /work/cikti/aether-1.0-nebula.iso"; [ "$2" = disk ] && CD=""
    EK=""; [ "$3" = uefi ] && EK="-bios /usr/share/OVMF/OVMF.fd"
    rm -f $W/seri.log
    qemu-system-x86_64 -m 2048 -smp 2 -cpu max -vga ${VGA:-std} $EK \
      -drive file=$W/disk.qcow2,if=virtio $CD -boot $BOOT \
      -nic user,model=virtio-net-pci -usb -device usb-tablet \
      -display none -monitor unix:$W/mon.sock,server,nowait -serial file:$W/seri.log \
      -daemonize -pidfile $W/qemu.pid
    echo "başladı" ;;
  ss) mon "screendump $W/ss.ppm"; sleep 1; magick $W/ss.ppm /work/ss-$2.png && echo /work/ss-$2.png ;;
  tus) shift; for k in "$@"; do mon "sendkey $k"; sleep 0.3; done ;;
  yaz) shift; echo "$*" | sed 's/./&\n/g' | while IFS= read -r c; do
         case "$c" in
           " ") k=spc ;; ".") k=slash ;; "-") k=equal ;; "/") k=shift-7 ;; "_") k=shift-equal ;; "i") k=apostrophe ;; ",") k=backslash ;; "|") k=alt_r-minus ;; ">") k=shift-less ;; "<") k=less ;; "&") k=shift-6 ;; "=") k=shift-0 ;; ";") k=shift-backslash ;; "") continue ;;
           [A-Z]) k="shift-$(echo $c | tr A-Z a-z)" ;; *) k="$c" ;;
         esac; mon "sendkey $k"; sleep 0.12; done ;;
  fare) mon "mouse_move $2 $3"; [ -n "$4" ] && mon "mouse_button $4" && sleep 0.1 && mon "mouse_button 0" ;;
  dur) [ -f $W/qemu.pid ] && kill $(cat $W/qemu.pid) 2>/dev/null; rm -f $W/qemu.pid; echo durdu ;;
  mon) shift; mon "$*" ;;
esac
