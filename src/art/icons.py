#!/usr/bin/env python3
# Aether uygulama simgeleri (SVG) -> hicolor PNG'ler
import os, subprocess
OUT = "/work/overlay/usr/share/icons/hicolor"
BG = '''<defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1">
<stop offset="0" stop-color="#2b1f6b"/><stop offset="1" stop-color="#6a4bd8"/></linearGradient></defs>
<rect x="4" y="4" width="120" height="120" fill="url(#g)"/>
<rect x="4" y="4" width="120" height="120" fill="none" stroke="#b9a8ff" stroke-width="2"/>'''
W = 'stroke="#fff" fill="none" stroke-width="7" stroke-linecap="square"'
ICONS = {
 # Aether logosu: A + yörünge + yıldız
 "aether": '''<path d="M40 98 L64 30 L88 98" %s/><path d="M50 74 H78" %s/>
  <ellipse cx="64" cy="66" rx="50" ry="16" transform="rotate(-18 64 66)" stroke="#c9b8ff" stroke-width="3" fill="none"/>
  <path d="M98 24 l3 7 7 3 -7 3 -3 7 -3 -7 -7 -3 7 -3z" fill="#fff"/>''' % (W, W),
 "aether-kurulum": '''<rect x="28" y="70" width="72" height="30" %s/><circle cx="86" cy="85" r="4" fill="#fff"/>
  <path d="M64 22 V58 M48 44 L64 60 L80 44" %s/>''' % (W, W),
 "aether-ayarlar": '''<circle cx="64" cy="64" r="16" %s/>
  <path d="M64 22V36 M64 92V106 M22 64H36 M92 64H106 M34 34L44 44 M84 84L94 94 M94 34L84 44 M44 84L34 94" %s/>''' % (W, W),
 "aether-mayin": '''<circle cx="64" cy="68" r="24" fill="#fff"/><path d="M64 32V104 M28 68H100 M39 43L89 93 M89 43L39 93" stroke="#fff" stroke-width="6"/>
  <circle cx="56" cy="60" r="6" fill="#6a4bd8"/>''',
 "aether-oyunlar": '''<rect x="22" y="44" width="84" height="44" %s/><path d="M40 66H56 M48 58V74" %s/>
  <circle cx="80" cy="62" r="4" fill="#fff"/><circle cx="90" cy="72" r="4" fill="#fff"/>''' % (W, W),
 "aether-hakkinda": '''<circle cx="64" cy="64" r="40" %s/><path d="M64 58V92" %s/><circle cx="64" cy="42" r="5" fill="#fff"/>''' % (W, W),
 "aether-hosgeldin": '''<path d="M98 24 l6 14 14 6 -14 6 -6 14 -6 -14 -14 -6 14 -6z" fill="#fff"/>
  <path d="M30 98 L56 40 L82 98" %s/><path d="M41 76 H71" %s/>''' % (W, W),
 "aether-terminal": '''<path d="M30 46 L48 62 L30 78" %s/><path d="M56 82 H96" %s/>''' % (W, W),
 "aether-dosyalar": '''<path d="M24 38 H54 L62 46 H104 V94 H24 Z" %s/>''' % W,
 "aether-metin": '''<path d="M34 24 H82 L96 38 V104 H34 Z" %s/><path d="M48 58 H82 M48 72 H82 M48 86 H70" stroke="#fff" stroke-width="5"/>''' % W,
 "aether-web": '''<circle cx="64" cy="64" r="38" %s/><ellipse cx="64" cy="64" rx="16" ry="38" stroke="#fff" stroke-width="5" fill="none"/><path d="M26 64 H102" stroke="#fff" stroke-width="5"/>''' % W,
 "aether-uygulamalar": '''<rect x="30" y="30" width="26" height="26" fill="#fff"/><rect x="72" y="30" width="26" height="26" fill="#fff"/><rect x="30" y="72" width="26" height="26" fill="#fff"/><rect x="72" y="72" width="26" height="26" fill="#c9b8ff"/>''',
 "aether-resim": '''<rect x="24" y="32" width="80" height="64" %s/><path d="M28 90 L54 62 L72 80 L84 68 L102 88" stroke="#fff" stroke-width="5" fill="none"/><circle cx="84" cy="50" r="7" fill="#fff"/>''' % W,
 "aether-hesap": '''<rect x="32" y="22" width="64" height="84" %s/><rect x="44" y="34" width="40" height="16" fill="#fff"/><path d="M46 66 h8 M60 66 h8 M74 66 h8 M46 80 h8 M60 80 h8 M74 80 h8 M46 94 h8 M60 94 h8 M74 94 h8" stroke="#fff" stroke-width="6"/>''' % W,
 "aether-muzik": '''<path d="M52 92 V34 L94 26 V84" %s/><circle cx="42" cy="92" r="11" fill="#fff"/><circle cx="84" cy="84" r="11" fill="#fff"/>''' % W,
 "aether-kamera": '''<rect x="22" y="42" width="84" height="56" %s/><path d="M48 42 L54 30 H74 L80 42" stroke="#fff" stroke-width="6" fill="none"/><circle cx="64" cy="70" r="14" stroke="#fff" stroke-width="6" fill="none"/>''' % W,
 "aether-kilit": '''<rect x="32" y="58" width="64" height="46" fill="#fff"/><path d="M44 58 V44 A20 20 0 0 1 84 44 V58" %s/>''' % W,
 "aether-cikis": '''<path d="M56 30 H30 V98 H56" %s/><path d="M54 64 H100 M84 48 L100 64 L84 80" %s/>''' % (W, W),
 "aether-yeniden": '''<path d="M96 64 A32 32 0 1 1 84 39" %s/><path d="M86 22 V42 H66" %s/>''' % (W, W),
 "aether-kapat": '''<path d="M44 40 A32 32 0 1 0 84 40" %s/><path d="M64 24 V62" %s/>''' % (W, W),
 "aether-eclipse": '''<circle cx="64" cy="64" r="36" fill="#fff"/><circle cx="80" cy="54" r="34" fill="#2b1f6b"/><circle cx="64" cy="64" r="36" fill="none" stroke="#fff" stroke-width="3"/><path d="M98 22 l3 7 7 3 -7 3 -3 7 -3 -7 -7 -3 7 -3z" fill="#fff"/>''',
 "aether-gorev": '''<rect x="22" y="28" width="84" height="72" %s/><path d="M30 74 H46 L54 52 L66 88 L76 62 L82 74 H98" stroke="#fff" stroke-width="6" fill="none" stroke-linejoin="round"/>''' % W,
 "aether-magaza": '''<path d="M30 46 H98 L92 104 H36 Z" %s/><path d="M48 54 V40 A16 16 0 0 1 80 40 V54" %s/>''' % (W, W),
 "aether-oyun": '''<rect x="18" y="46" width="92" height="46" %s/><path d="M36 69H54 M45 60V78" %s/><circle cx="80" cy="64" r="4" fill="#fff"/><circle cx="90" cy="74" r="4" fill="#fff"/><path d="M70 18 L58 40 H70 L62 56" stroke="#c9b8ff" stroke-width="5" fill="none"/>''' % (W, W),
 "aether-motor": '''<path d="M64 22 L100 43 V85 L64 106 L28 85 V43Z" %s/><path d="M28 43 L64 64 L100 43 M64 64V106" stroke="#c9b8ff" stroke-width="4" fill="none"/>''' % W,
}
for name, body in ICONS.items():
    svg = '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">%s%s</svg>' % (BG, body)
    d = OUT + "/scalable/apps"; os.makedirs(d, exist_ok=True)
    p = d + "/%s.svg" % name; open(p, "w").write(svg)
    for s in (32, 48, 64, 128):
        dd = OUT + "/%dx%d/apps" % (s, s); os.makedirs(dd, exist_ok=True)
        subprocess.run(["rsvg-convert", "-w", str(s), "-h", str(s), p, "-o", dd + "/%s.png" % name], check=True)
# Plymouth ve hakkında ekranı için büyük logo
subprocess.run(["rsvg-convert", "-w", "256", "-h", "256", OUT + "/scalable/apps/aether.svg", "-o", "/work/overlay/usr/share/aether/logo.png"], check=True)
print("simgeler tamam")
