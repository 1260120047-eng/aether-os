#!/bin/sh
# Windows derleme araçları + WebView2 SDK
apk add -q mingw-w64-gcc mingw-w64-binutils mingw-w64-headers mingw-w64-crt mingw-w64-winpthreads unzip 2>&1 | tail -2
x86_64-w64-mingw32-g++ --version | head -1
mkdir -p /work/win/sdk && cd /work/win/sdk
V=$(curl -sS https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/index.json | tr ',' '\n' | tr -d '"[] ' | grep -E '^[0-9]+\.[0-9]+\.[0-9]+(\.[0-9]+)?$' | tail -1)
echo "WebView2 SDK: $V"
curl -sS -o wv2.nupkg https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$V/microsoft.web.webview2.$V.nupkg
unzip -qo wv2.nupkg
ls build/native/include build/native/x64
