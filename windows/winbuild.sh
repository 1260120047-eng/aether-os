#!/bin/sh
set -e
cd /work/win/src
S=/usr/share/icons/hicolor/scalable/apps/aether-eclipse.svg
[ -f $S ] || S=/work/overlay/usr/share/icons/hicolor/scalable/apps/aether-eclipse.svg
for n in 16 24 32 48 64 128 256; do rsvg-convert -w $n -h $n $S -o /tmp/ik$n.png; done
magick /tmp/ik16.png /tmp/ik24.png /tmp/ik32.png /tmp/ik48.png /tmp/ik64.png /tmp/ik128.png /tmp/ik256.png eclipse.ico
cp /work/overlay/usr/share/plymouth/themes/aether/arkaplan.png arkaplan.png
cp /work/overlay/usr/share/aether/eclipse/reklam-alanlari.txt reklam-alanlari.txt
SDK=/work/win/sdk/build/native
x86_64-w64-mingw32-windres eclipse.rc -O coff -o eclipse.res
x86_64-w64-mingw32-g++ -std=c++17 -O2 -municode -mwindows -Wall -Wno-unknown-pragmas -Wno-unused-function \
  -I$SDK/include -Iuyum eclipse.cpp eclipse.res -o Eclipse.exe \
  -L$SDK/x64 -l:WebView2Loader.dll.lib -lole32 -loleaut32 -luuid -lcomctl32 -lshlwapi -lshell32 -luser32 -lgdi32 \
  -static -static-libgcc -static-libstdc++
x86_64-w64-mingw32-strip Eclipse.exe
ls -la Eclipse.exe
x86_64-w64-mingw32-objdump -p Eclipse.exe | grep "DLL Name"
