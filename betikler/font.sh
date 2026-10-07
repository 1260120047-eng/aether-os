#!/bin/sh
apk add -q grub-mkfont 2>/dev/null || apk add -q grub-dev 2>/dev/null
command -v grub-mkfont || { apk search grub | tr '\n' ' '; exit 1; }
cd /work/overlay/boot/grub/themes/aether
F=/usr/share/fonts/jetbrains-mono/JetBrainsMono-Regular.ttf
grub-mkfont -s 16 -o jetbrains-16.pf2 $F
grub-mkfont -s 12 -o jetbrains-12.pf2 $F
ls -la *.pf2
