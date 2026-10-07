#!/bin/sh
apk add -q xorriso grub grub-bios grub-efi squashfs-tools mtools dosfstools \
  qemu-system-x86_64 qemu-img e2fsprogs cpio gzip xz imagemagick font-dejavu ovmf \
  font-jetbrains-mono 2>&1 | tail -3
qemu-system-x86_64 --version | head -1
grub-mkrescue --version
