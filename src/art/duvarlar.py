#!/usr/bin/env python3
# Aether 1.1 duvar kağıdı paketi — Nebula ile aynı tarzda yıldız ve bulutsu görselleri
# Kullanım: python3 duvarlar.py [çıktı klasörü]
import os, sys, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = sys.argv[1] if len(sys.argv) > 1 else "/work/overlay/usr/share/aether/wallpapers"
W, H = 1920, 1080
os.makedirs(OUT, exist_ok=True)

Y = np.linspace(0, 1, H)[:, None]
X = np.linspace(0, 1, W)[None, :]


def gradyan(ust, alt, us=1.3):
    ust, alt = np.array(ust, np.float32), np.array(alt, np.float32)
    img = ust + (alt - ust) * (Y ** us)[..., None]
    return np.broadcast_to(img, (H, W, 3)).copy().astype(np.float32)


def bulut(img, lekeler, rng, doku=0.16, olcek=8):
    for cx, cy, r, renk, a in lekeler:
        d = ((X - cx) * W / H) ** 2 + (Y - cy) ** 2
        g = np.exp(-d / (2 * (r / 2.2) ** 2)) * a
        img += g[..., None] * (np.array(renk, np.float32) - img) * 0.9
    n = rng.normal(0, 1, (H // olcek + 1, W // olcek + 1)).astype(np.float32)
    n = np.array(Image.fromarray(((n + 3) * 40).clip(0, 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255
    img *= (1 - doku / 2 + doku * n)[..., None]
    return img


def yildizlar(im, rng, yogunluk=1.0, maske=None):
    glow = Image.new("RGB", (W, H)); gd = ImageDraw.Draw(glow)
    d = ImageDraw.Draw(im)
    for _ in range(int(W * H / 900 * yogunluk)):
        px, py = int(rng.integers(0, W)), int(rng.integers(0, H))
        if maske is not None and rng.random() > maske[py, px]:
            continue
        b = rng.random() ** 3
        c = int(110 + 145 * b)
        tint = [(1, 1, 1), (0.85, 0.9, 1), (1, 0.92, 0.85), (0.9, 0.85, 1)][int(rng.integers(0, 4))]
        col = tuple(int(c * t) for t in tint)
        if b > 0.92:
            d.ellipse([px - 1, py - 1, px + 1, py + 1], fill=col)
            gd.ellipse([px - 6, py - 6, px + 6, py + 6], fill=tuple(int(v * 0.5) for v in col))
        else:
            d.point((px, py), fill=col)
    glow = glow.filter(ImageFilter.GaussianBlur(4))
    return Image.fromarray(np.minimum(255, np.array(im, np.int16) + np.array(glow, np.int16)).astype(np.uint8))


def kaydet(img, ad):
    if isinstance(img, np.ndarray):
        img = Image.fromarray(img.clip(0, 255).astype(np.uint8))
    yol = os.path.join(OUT, ad + ".png")
    img.save(yol, optimize=True)
    print(yol, os.path.getsize(yol) // 1024, "KB")


# 1) Orion — camgöbeği ve mavi bulutsu
rng = np.random.default_rng(21)
img = gradyan((3, 10, 22), (8, 34, 58))
img = bulut(img, [(0.30, 0.35, 0.42, (30, 130, 170), 0.55), (0.75, 0.65, 0.40, (40, 80, 190), 0.45),
                  (0.55, 0.40, 0.18, (120, 210, 220), 0.30), (0.85, 0.20, 0.22, (60, 60, 160), 0.30)], rng)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng), "orion")

# 2) Gün batımı — eflatun ve turuncu bulutsu
rng = np.random.default_rng(33)
img = gradyan((14, 6, 24), (60, 18, 40), 1.1)
img = bulut(img, [(0.65, 0.62, 0.45, (210, 80, 110), 0.50), (0.35, 0.40, 0.35, (150, 60, 170), 0.45),
                  (0.80, 0.80, 0.25, (240, 150, 90), 0.35), (0.20, 0.75, 0.30, (90, 40, 130), 0.30)], rng)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng, 0.8), "gunbatimi")

# 3) Kuzey ışıkları — yeşil-mor kuşaklar
rng = np.random.default_rng(45)
img = gradyan((4, 8, 18), (10, 20, 36))
for i, (ty, renk, a) in enumerate([(0.42, (60, 220, 160), 0.55), (0.50, (90, 120, 230), 0.35), (0.36, (170, 90, 220), 0.25)]):
    faz = rng.random() * 6
    egri = ty + 0.07 * np.sin(X * 7 + faz) + 0.03 * np.sin(X * 17 + faz * 2)
    uzaklik = Y - egri
    kusak = np.exp(-(uzaklik ** 2) / (2 * 0.035 ** 2))
    perde = np.where(uzaklik < 0, np.exp(uzaklik / 0.18), 1.0)   # yukarı doğru sönen perde
    yogun = (kusak * 0.6 + perde * np.exp(-(uzaklik.clip(0) ** 2) / 0.002) * 0.4) * a
    ham = rng.random(W // 6 + 2)
    ham = np.convolve(ham, np.ones(5) / 5, mode="same")
    cizgi = 0.55 + 0.45 * np.interp(np.arange(W), np.linspace(0, W, ham.size), ham)[None, :]
    img += (yogun * cizgi)[..., None] * (np.array(renk, np.float32) - img) * 0.9
img = bulut(img, [], rng, doku=0.10)
maske = np.clip(1.2 - img.mean(axis=2) / 90, 0.15, 1)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng, 1.1, maske), "kuzey-isiklari")

# 4) Tutulma — Eclipse'in simgesi gibi parlayan bir halka
rng = np.random.default_rng(57)
img = gradyan((4, 4, 12), (16, 10, 34))
img = bulut(img, [(0.5, 0.5, 0.65, (40, 25, 90), 0.45)], rng, doku=0.12)
cx, cy, R = 0.5, 0.47, 0.17
d = np.sqrt(((X - cx) * W / H) ** 2 + (Y - cy) ** 2)
halka = np.exp(-((d - R) ** 2) / (2 * 0.010 ** 2))
korona = np.exp(-np.clip(d - R, 0, None) / 0.06) * (d > R)
aci = np.arctan2(Y - cy, (X - cx) * W / H)
ham = rng.random(181) ** 2
ham[-1] = ham[0]
isinlar = 0.45 + 0.55 * np.interp(aci, np.linspace(-math.pi, math.pi, ham.size), np.convolve(np.r_[ham[-3:], ham, ham[:3]], np.ones(3) / 3, mode="same")[3:-3])
img += (halka * 1.0)[..., None] * (np.array((235, 225, 255), np.float32) - img)
img += (korona * isinlar * 0.75)[..., None] * (np.array((150, 120, 255), np.float32) - img)
img *= np.where(d < R - 0.004, 0.08, 1.0)[..., None]   # ay diski karanlık
maske = np.where(d < R + 0.02, 0.0, 1.0)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng, 0.9, maske), "tutulma")

# 5) Gece — sade, koyu, az yıldızlı (çalışırken gözü yormayan)
rng = np.random.default_rng(69)
img = gradyan((10, 11, 20), (22, 20, 38), 1.6)
img = bulut(img, [(0.80, 0.85, 0.50, (42, 34, 78), 0.35)], rng, doku=0.08)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng, 0.35), "gece")

# 6) Galaksi — eğik sarmal gökada
rng = np.random.default_rng(81)
img = gradyan((4, 6, 16), (14, 12, 32))
cx, cy = 0.56, 0.50
u = (X - cx) * W / H; v = (Y - cy)
egik = 0.42
ru = u * math.cos(egik) + v * math.sin(egik); rv = (-u * math.sin(egik) + v * math.cos(egik)) / 0.38
r = np.sqrt(ru ** 2 + rv ** 2) + 1e-6
th = np.arctan2(rv, ru)
kollar = (np.cos(2 * th - 7.5 * np.log(r + 0.02)) * 0.5 + 0.5) ** 3
disk = np.exp(-r / 0.21)
gokada = disk * (0.35 + 0.65 * kollar)
cekirdek = np.exp(-(r ** 2) / (2 * 0.035 ** 2))
n = rng.normal(0, 1, (H // 4 + 1, W // 4 + 1)).astype(np.float32)
n = np.array(Image.fromarray(((n + 3) * 40).clip(0, 255).astype(np.uint8)).resize((W, H), Image.BICUBIC), np.float32) / 255
gokada *= 0.7 + 0.6 * n
img += (gokada * 0.9).clip(0, 1)[..., None] * (np.array((150, 140, 235), np.float32) - img)
img += (cekirdek * 0.95)[..., None] * (np.array((255, 235, 210), np.float32) - img)
img = bulut(img, [(0.15, 0.25, 0.30, (50, 40, 120), 0.25)], rng, doku=0.08)
kaydet(yildizlar(Image.fromarray(img.clip(0, 255).astype(np.uint8)), rng, 0.9), "galaksi")
