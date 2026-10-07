# Aether

[![ISO](https://github.com/1260120047-eng/aether-os/actions/workflows/iso.yml/badge.svg)](https://github.com/1260120047-eng/aether-os/actions/workflows/iso.yml)

Aether, merak ettiğim için yaptığım küçük bir Linux dağıtımı. Alpine Linux üzerine kurulu, ISO dosyası 370 MB civarında, açılışta RAM'e yükleniyor ve beğenirsen diske kurulabiliyor. Arayüz Türkçe (İngilizce de var).

Bu depo, **Aether 1.0 "Nebula"** sürümünün kaynak kodları. Hazır ISO'yu indirmek için: **[aether-os.dev](https://aether-os.dev)**

## İçindekiler

```
betikler/     ISO'yu derleyen kabuk betikleri
overlay/      Kök sisteme olduğu gibi kopyalanan dosyalar
              (yapılandırmalar, temalar, simgeler, aether-* betikleri, açılış ekranı, GRUB teması)
src/apps/     Aether'e özel uygulamalar (C + GTK 3)
src/initramfs/init   Canlı açılış: ISO'yu bulur, sistemi RAM'e kopyalar, overlayfs kurar
src/art/      Duvar kağıdı, simgeler ve sesleri üreten Python betikleri
windows/      Eclipse tarayıcının Windows sürümü (Win32 + WebView2)
```

`src/apps` içindeki uygulamalar:

| Dosya | Program |
|---|---|
| `kurulum.c` | Diske kurulum sihirbazı (dil, klavye, hesap, disk) |
| `ayarlar.c` | Ayarlar: tema, dil, klavye, duvar kağıdı, eklentiler |
| `hosgeldin.c` | Karşılama penceresi |
| `hakkinda.c` | Aether Hakkında |
| `eclipse.c` | Eclipse web tarayıcı (WebKitGTK) |
| `oyunlar.c`, `mayin.c` | Oyun başlatıcı ve Mayın Tarlası |
| `yildizlar.c` | Yıldız alanı ekran koruyucusu (SDL2) |

Disk kurulumunun asıl işi `overlay/usr/libexec/aether/kur.sh` içinde.

## Derleme

Derleme bir **Alpine Linux 3.22** ortamında (chroot, konteyner ya da sanal makine) yapılır. Betikler şu yolları bekler:

| Depodaki klasör | Derleme ortamındaki yeri |
|---|---|
| `betikler/` | `/s` |
| `overlay/` | `/work/overlay` |
| `src/` | `/work/src` |

Sonra sırayla:

```sh
sh /s/tools.sh      # xorriso, grub, squashfs-tools, qemu...
sh /s/devtools.sh   # gcc, GTK 3 ve SDL2 geliştirme dosyaları
sh /s/rootfs.sh     # /work/rootfs içine Aether kök sistemini kurar
sh /s/webkit.sh     # Eclipse için WebKitGTK
sh /s/build.sh      # uygulamaları derler ve ISO'yu üretir
```

ISO `/work/cikti/aether-1.0-nebula.iso` olarak çıkar. QEMU'da denemek için:

```sh
sh /s/vm.sh basla cd
```

### Otomatik derleme

Bu depoya yapılan her değişiklikte GitHub Actions ISO'yu sıfırdan derler ([.github/workflows/iso.yml](.github/workflows/iso.yml)). Derlenen ISO, [Actions](https://github.com/1260120047-eng/aether-os/actions) sayfasında ilgili işin altındaki **Artifacts** bölümünden 14 gün boyunca indirilebilir. Bir sürüm etiketi (ör. `1.1`) gönderildiğinde ISO, o sürümün Releases sayfasına da eklenir.

### Eclipse (Windows)

Linux üzerinde MinGW ile çapraz derlenir:

```sh
sh windows/win.sh        # mingw-w64 ve WebView2 SDK
sh windows/winbuild.sh   # Eclipse.exe
```

`winbuild.sh` kaynakları `/work/win/src` altında bekler.

## Katkı ve geri bildirim

Hata bulduysan ya da bir fikrin varsa [Issues](https://github.com/1260120047-eng/aether-os/issues) bölümüne yazabilirsin. Aether hakkındaki genel yorumlar için [aether-os.dev](https://aether-os.dev/#yorum).

## Lisans

Aether'in kendi kodları **GNU GPL 3.0** ile lisanslıdır, ayrıntılar [LICENSE](LICENSE) dosyasında. Kısaca: kullanabilir, değiştirebilir ve dağıtabilirsin; değiştirdiğin sürümü dağıtırsan onun kaynak kodunu da aynı lisansla paylaşman gerekir.

ISO'nun içindeki Alpine paketleri (Linux çekirdeği, Openbox, GTK, WebKit, Mesa ve diğerleri) kendi lisanslarıyla gelir. Yazı tipi JetBrains Mono, SIL Open Font License ile lisanslıdır.

---

Aether, Hot Zot tarafından yapıldı.
