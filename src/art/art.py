#!/usr/bin/env python3
# Aether 1.0 "Nebula" görsellerini üretir
import os, random, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = "/work/overlay"
FONT = "/usr/share/fonts/jetbrains-mono/JetBrainsMono-%s.ttf"
def font(w, s): return ImageFont.truetype(FONT % w, s)
def mk(p): os.makedirs(os.path.dirname(p), exist_ok=True); return p

def sky(W, H, seed=7, nebula=True, stars=1.0):
    rng = np.random.default_rng(seed)
    y = np.linspace(0, 1, H)[:, None]; x = np.linspace(0, 1, W)[None, :]
    top = np.array([5, 8, 26]); bot = np.array([30, 18, 66])
    img = top + (bot - top) * (y ** 1.3)[..., None]
    img = np.broadcast_to(img, (H, W, 3)).copy().astype(np.float32)
    if nebula:
        for cx, cy, r, col, a in [(0.72, 0.30, 0.38, (120, 70, 210), 0.55),
                                  (0.25, 0.70, 0.45, (40, 90, 200), 0.45),
                                  (0.55, 0.55, 0.20, (190, 110, 230), 0.30),
                                  (0.15, 0.20, 0.25, (70, 50, 160), 0.30)]:
            d = ((x - cx) * W / H) ** 2 + (y - cy) ** 2
            g = np.exp(-d / (2 * (r / 2.2) ** 2)) * a
            img += g[..., None] * (np.array(col) - img) * 0.9
        # ince doku
        n = rng.normal(0, 1, (H // 8 + 1, W // 8 + 1)).astype(np.float32)
        n = np.array(Image.fromarray(((n + 3) * 40).clip(0, 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), dtype=np.float32) / 255
        img *= (0.92 + 0.16 * n)[..., None]
    im = Image.fromarray(img.clip(0, 255).astype(np.uint8))
    # yıldızlar
    glow = Image.new("RGB", (W, H)); gd = ImageDraw.Draw(glow)
    d = ImageDraw.Draw(im)
    N = int(W * H / 900 * stars)
    for _ in range(N):
        px, py = rng.integers(0, W), rng.integers(0, H)
        b = rng.random() ** 3
        c = int(110 + 145 * b)
        tint = rng.choice([(1, 1, 1), (0.85, 0.9, 1), (1, 0.92, 0.85), (0.9, 0.85, 1)])
        col = tuple(int(c * t) for t in tint)
        if b > 0.92:
            d.ellipse([px - 1, py - 1, px + 1, py + 1], fill=col)
            gd.ellipse([px - 6, py - 6, px + 6, py + 6], fill=tuple(int(v * 0.5) for v in col))
        else:
            d.point((px, py), fill=col)
    glow = glow.filter(ImageFilter.GaussianBlur(4))
    im = Image.fromarray(np.minimum(255, np.array(im, dtype=np.int16) + np.array(glow, dtype=np.int16)).astype(np.uint8))
    return im

def spaced(t): return " ".join(t)

def draw_logo_text(im, cy, size, sub=True, alpha=255):
    W, H = im.size
    layer = Image.new("RGBA", im.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    f = font("Light", size); t = spaced("AETHER")
    w = d.textlength(t, font=f)
    d.text(((W - w) / 2, cy - size * 0.6), t, font=f, fill=(255, 255, 255, alpha))
    glow = layer.filter(ImageFilter.GaussianBlur(size / 6))
    out = Image.alpha_composite(im.convert("RGBA"), glow)
    out = Image.alpha_composite(out, layer)
    if sub:
        d2 = ImageDraw.Draw(out)
        f2 = font("Regular", max(12, size // 4)); t2 = "1.0  ·  N E B U L A"
        w2 = d2.textlength(t2, font=f2)
        d2.text(((W - w2) / 2, cy + size * 0.65), t2, font=f2, fill=(190, 170, 255, 220))
    return out.convert("RGB")

# --- duvar kağıtları
wp = OUT + "/usr/share/aether/wallpapers/"
base = sky(1920, 1080)
draw_logo_text(base, 470, 96).save(mk(wp + "nebula.png"), optimize=True)
base.save(mk(wp + "yildizlar.png"), optimize=True)
# açık "şafak" duvar kağıdı
W, H = 1920, 1080
y = np.linspace(0, 1, H)[:, None, None]
a = np.array([236, 238, 248]); b = np.array([206, 200, 236])
dawn = np.broadcast_to(a + (b - a) * y, (H, W, 3)).astype(np.uint8)
dim = Image.fromarray(dawn.copy()); dd = ImageDraw.Draw(dim)
rng = random.Random(3)
for i in range(14):  # ince geometrik çizgiler
    x0 = rng.randint(-400, 1920); dd.line([(x0, 1080), (x0 + 700, 0)], fill=(196, 190, 230), width=1)
f = font("Light", 90); t = spaced("AETHER"); w = dd.textlength(t, font=f)
dd.text(((W - w) / 2, 410), t, font=f, fill=(70, 60, 140))
f2 = font("Regular", 22); t2 = "1.0  ·  N E B U L A"; w2 = dd.textlength(t2, font=f2)
dd.text(((W - w2) / 2, 530), t2, font=f2, fill=(110, 100, 170))
dim.save(mk(wp + "safak.png"), optimize=True)

# --- GRUB arka planı
g = sky(1024, 768, seed=11)
g = draw_logo_text(g, 170, 64)
g.save(mk(OUT + "/boot/grub/themes/aether/arkaplan.png"))
# GRUB seçim çubuğu (9 parça: sadece merkez)
sel = Image.new("RGBA", (8, 8), (140, 110, 255, 90)); sel.save(mk(OUT + "/boot/grub/themes/aether/select_c.png"))
for n in ["n", "s", "e", "w", "ne", "nw", "se", "sw"]:
    Image.new("RGBA", (2, 2), (170, 150, 255, 200) if n in ("w",) else (0, 0, 0, 0)).save(OUT + "/boot/grub/themes/aether/select_%s.png" % n)

# --- Plymouth
pt = OUT + "/usr/share/plymouth/themes/aether/"
sky(1280, 720, seed=7).save(mk(pt + "arkaplan.png"), optimize=True)
lg = Image.new("RGBA", (900, 200), (0, 0, 0, 0)); ld = ImageDraw.Draw(lg)
f = font("Light", 84); t = spaced("AETHER"); w = ld.textlength(t, font=f)
ld.text(((900 - w) / 2, 30), t, font=f, fill=(255, 255, 255, 255))
f2 = font("Regular", 22); t2 = "1.0  ·  N E B U L A"; w2 = ld.textlength(t2, font=f2)
ld.text(((900 - w2) / 2, 150), t2, font=f2, fill=(190, 170, 255, 230))
glow = lg.filter(ImageFilter.GaussianBlur(10))
Image.alpha_composite(glow, lg).save(pt + "logo.png")
Image.new("RGBA", (360, 4), (255, 255, 255, 40)).save(pt + "cubuk_arka.png")
Image.new("RGBA", (360, 4), (180, 160, 255, 255)).save(pt + "cubuk.png")

# --- kilit ekranları (i3lock ölçeklemediği için birkaç çözünürlük)
for (W, H) in [(1024, 768), (1280, 720), (1280, 800), (1366, 768), (1440, 900), (1600, 900), (1920, 1080), (2560, 1440)]:
    im = Image.new("RGB", (W, H), (8, 10, 30)); d = ImageDraw.Draw(im)
    r = random.Random(W)
    for _ in range(W * H // 2500):
        x, y = r.randrange(W), r.randrange(H); c = r.randint(90, 230); d.point((x, y), fill=(c, c, min(255, c + 20)))
    f = font("Light", 54); t = spaced("AETHER"); w = d.textlength(t, font=f)
    d.text(((W - w) / 2, H / 2 - 190), t, font=f, fill=(235, 235, 255))
    f2 = font("Regular", 16); t2 = "kilitli  ·  locked"; w2 = d.textlength(t2, font=f2)
    d.text(((W - w2) / 2, H / 2 + 150), t2, font=f2, fill=(160, 150, 220))
    im.save(mk(OUT + "/usr/share/aether/kilit/%dx%d.png" % (W, H)), optimize=True)

print("tamam")
