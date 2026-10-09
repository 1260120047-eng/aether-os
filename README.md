# Aether

[![ISO](https://github.com/1260120047-eng/aether-os/actions/workflows/iso.yml/badge.svg)](https://github.com/1260120047-eng/aether-os/actions/workflows/iso.yml)

Aether, merak ettiğim için yaptığım küçük bir Linux dağıtımı. **2.0 "Orion"** sürümünden beri Arch Linux üzerine kurulu; ISO dosyası yaklaşık 800 MB, açılışta RAM'e yükleniyor ve beğenirsen diske (istersen Windows'un yanına) kurulabiliyor. Arayüz Türkçe (İngilizce de var).

Bu depo, **Aether 2.0 "Orion"** sürümünün kaynak kodları. Hazır ISO'yu indirmek için: **[aether-os.dev](https://aether-os.dev)**

Alpine tabanlı **1.1 "Nebula"** sürümünün kaynakları [`1.1` dalında](https://github.com/1260120047-eng/aether-os/tree/1.1).

## İçindekiler

```
betikler/     ISO'yu derleyen kabuk betikleri
overlay/      Kök sisteme olduğu gibi kopyalanan dosyalar
              (yapılandırmalar, systemd servisleri, temalar, simgeler, aether-* betikleri, GRUB teması)
src/apps/     Aether'e özel uygulamalar (C; GTK 3 ve Xlib)
src/initramfs/       Canlı açılış: ISO'yu bulur, overlayfs kurar; toram.c sistemi arka planda RAM'e kopyalar
src/art/      Duvar kağıdı, simgeler ve sesleri üreten Python betikleri
windows/      Eclipse tarayıcının Windows sürümü (Win32 + WebView2)
```

`src/apps` içindeki uygulamalar:

| Dosya | Program |
|---|---|
| `yorunge.c` | Yörünge, Aether'in pencere yöneticisi (Xlib, EWMH, XRandR) |
| `panel.c` | Görev çubuğu: başlat menüsü, pencereler, ağ, ses, saat (GTK 3 + libwnck) |
| `masaustu.c` | Masaüstü: simgeler, sağ tık menüleri, sürükle bırak |
| `uygulamalar.c` | Yüklü uygulamalardan başlat menüsünü oluşturur |
| `dosyalar.c` | Aether Dosyalar, dosya yöneticisi |
| `gorev.c` | Görev Yöneticisi: işlemler, CPU / bellek / disk / ağ grafikleri |
| `magaza.c`, `yetki.h` | Uygulama Mağazası: Arch paketleri (AppStream) ve Flathub |
| `oyun.c` | Oyun Modu: Steam, ekran kartı sürücüsü, performans modu |
| `bildirim.c` | Bildirim sunucusu (org.freedesktop.Notifications) |
| `terminal.c` | Aether Terminal, sekmeli (VTE) |
| `ag.h` | Wi-Fi için iwd D-Bus yardımcıları |
| `kurulum.c` | Diske kurulum sihirbazı (dil, klavye, hesap, disk; Windows'un yanına kurma) |
| `ayarlar.c` | Ayarlar: tema, dil, klavye, duvar kağıdı, ekran, Wi-Fi, sürücüler, eklentiler |
| `hosgeldin.c` | Karşılama penceresi |
| `hakkinda.c` | Aether Hakkında |
| `eclipse.c` | Eclipse web tarayıcı (WebKitGTK), reklam engelleyicili |
| `oyunlar.c`, `mayin.c` | Oyun başlatıcı ve Mayın Tarlası |
| `yildizlar.c` | Yıldız alanı ekran koruyucusu (SDL2) |

Disk kurulumunun asıl işi `overlay/usr/libexec/aether/kur.sh` içinde.

## Derleme

Derleme bir **Arch Linux** ortamında (chroot, konteyner ya da sanal makine; root olarak) yapılır. Betikler şu yolları bekler:

| Depodaki klasör | Derleme ortamındaki yeri |
|---|---|
| `betikler/` | `/work/s` |
| `overlay/` | `/work/overlay` |
| `src/` | `/work/src` |

Sonra sırayla:

```sh
pacman -S --needed arch-install-scripts python
bash /work/s/rootfs.sh   # /work/rootfs içine Aether kök sistemini kurar (pacstrap)
bash /work/s/build.sh    # derleme araçlarını kurar, uygulamaları derler ve ISO'yu üretir
```

Konteyner imajlarında `/etc/pacman.conf` içindeki `NoExtract` satırlarını silmeyi unutma; yoksa dil dosyaları kök sisteme kurulmaz.

ISO `/work/cikti/aether-2.0-orion.iso` olarak çıkar.

### Otomatik derleme

Bu depoya yapılan her değişiklikte GitHub Actions ISO'yu bir Arch Linux konteynerinde sıfırdan derler ([.github/workflows/iso.yml](.github/workflows/iso.yml)). Derlenen ISO, [Actions](https://github.com/1260120047-eng/aether-os/actions) sayfasında ilgili işin altındaki **Artifacts** bölümünden 14 gün boyunca indirilebilir. Bir sürüm etiketi (ör. `2.0`) gönderildiğinde ISO, o sürümün Releases sayfasına da eklenir.

### Eclipse (Windows)

Linux üzerinde MinGW ile çapraz derlenir:

```sh
sh windows/win.sh        # mingw-w64 ve WebView2 SDK
sh windows/winbuild.sh   # Eclipse.exe
```

`winbuild.sh` kaynakları `/work/win/src` altında bekler. Otomatik derleme, Eclipse-Windows.zip paketini de ISO ile birlikte üretir.

## Katkı ve geri bildirim

Hata bulduysan ya da bir fikrin varsa [Issues](https://github.com/1260120047-eng/aether-os/issues) bölümüne yazabilirsin. Aether hakkındaki genel yorumlar için [aether-os.dev](https://aether-os.dev/#yorum).

## Lisans

Aether'in kendi kodları **GNU GPL 3.0** ile lisanslıdır, ayrıntılar [LICENSE](LICENSE) dosyasında. Kısaca: kullanabilir, değiştirebilir ve dağıtabilirsin; değiştirdiğin sürümü dağıtırsan onun kaynak kodunu da aynı lisansla paylaşman gerekir.

ISO'nun içindeki Arch Linux paketleri (Linux çekirdeği, GTK, WebKit, VTE, Mesa ve diğerleri) kendi lisanslarıyla gelir. Yazı tipi JetBrains Mono, SIL Open Font License ile lisanslıdır. Eclipse'in reklam engelleyici listesi [AdAway](https://adaway.org) hosts listesinden alındı (CC BY 3.0).

---

Aether, Hot Zot tarafından yapıldı.
