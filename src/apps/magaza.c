/* aether-magaza — Aether Uygulama Mağazası
 *
 * Arch Linux depolarındaki uygulamaları (AppStream kataloğu: ad, açıklama, simge, kategori)
 * gösterir; arama, kategoriler, öne çıkanlar, yüklü uygulamalar ve güncellemeler.
 * Kurma/kaldırma/güncelleme /usr/libexec/aether/magaza-yardimci ile sudo üzerinden yapılır;
 * gerekirse parola Windows'taki gibi bir pencereyle sorulur.
 *
 * Flathub: aramalarda ve "Flathub" bölümünde Flathub uygulamaları da çıkar (internetten, flathub.org API).
 * Flatpak uygulamaları kullanıcıya özel kurulur; parola gerekmez.
 */
#include <gio/gdesktopappinfo.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <sys/wait.h>
#include "aether.h"

#define KATALOG "/usr/share/swcatalog/xml"
#define SIMGELER "/usr/share/aether/magaza/simgeler"
#define YARDIMCI "/usr/libexec/aether/magaza-yardimci"
#define LISTE_SINIR 400
#define FLATHUB_API "https://flathub.org/api/v2"

typedef struct {
  char *id, *ad, *ozet, *aciklama, *paket, *simge, *stok, *masaustu, *depo;
  guint kat;          /* bit maskesi: K_* */
  int one_cikan;      /* öne çıkanlar sırası (0: değil) */
  int flathub;        /* 1: Flathub uygulaması (paket = uygulama kimliği, simge = adres) */
  char *gelistirici, *lisans;
} Uyg;

enum { K_OYUN = 1, K_INTERNET = 2, K_OFIS = 4, K_GRAFIK = 8, K_SES = 16, K_GELISTIRME = 32, K_EGITIM = 64, K_ARAC = 128 };
typedef struct { const char *id, *tr, *en, *simge; guint maske; } Bolum;
static const Bolum bolumler[] = {
  { "ana", "Öne çıkanlar", "Featured", "starred-symbolic", 0 },
  { "oyun", "Oyunlar", "Games", "input-gaming-symbolic", K_OYUN },
  { "internet", "İnternet", "Internet", "web-browser-symbolic", K_INTERNET },
  { "ofis", "Ofis", "Office", "x-office-document-symbolic", K_OFIS },
  { "grafik", "Grafik", "Graphics", "applications-graphics-symbolic", K_GRAFIK },
  { "ses", "Ses ve Video", "Sound & Video", "applications-multimedia-symbolic", K_SES },
  { "gelistirme", "Geliştirme", "Development", "applications-engineering-symbolic", K_GELISTIRME },
  { "egitim", "Eğitim ve Bilim", "Education & Science", "accessories-dictionary-symbolic", K_EGITIM },
  { "arac", "Araçlar", "Utilities", "applications-utilities-symbolic", K_ARAC },
  { "flathub", "Flathub", "Flathub", "system-software-install-symbolic", 0 },
  { "yuklu", "Yüklü", "Installed", "emblem-ok-symbolic", 0 },
  { "guncel", "Güncellemeler", "Updates", "software-update-available-symbolic", 0 },
};
#define BOLUM_SAYI G_N_ELEMENTS(bolumler)

/* öne çıkanlar (sırayla) */
static const char *one_cikanlar[] = {
  "firefox", "chromium", "discord", "telegram-desktop", "libreoffice-fresh", "vlc", "gimp", "obs-studio",
  "prismlauncher", "supertuxkart", "code", "spotify-launcher", "thunderbird", "inkscape", "krita", "blender",
  "audacity", "kdenlive", "godot", "luanti", "0ad", "retroarch", "qbittorrent", "mpv", "darktable", "xournalpp", NULL
};
/* birlikte kurulacaklar: Türkçe dil paketleri (yalnızca Türkçe arayüzde) ve gerekli parçalar */
static const struct { const char *paket, *ek; int yalniz_tr; } ek_paketler[] = {
  { "libreoffice-fresh", "libreoffice-fresh-tr", 1 }, { "firefox", "firefox-i18n-tr", 1 }, { "thunderbird", "thunderbird-i18n-tr", 1 },
  { "prismlauncher", "jre21-openjdk", 0 }, { NULL, NULL, 0 }
};

static GPtrArray *uygulamalar;
static GHashTable *yuklu, *temel, *guncellenecek;   /* paket adı kümeleri */
static GtkWidget *pencere, *yan, *arama, *yigin, *akis, *akis_baslik, *akis_alt, *ayrinti_kutu, *durum_cubugu, *durum_yazi, *ilerleme, *guncel_dugme;
static GtkWidget *fh_baslik, *fh_alt, *fh_akis;      /* Flathub sonuçları */
static GHashTable *fp_yuklu, *fp_onbellek;           /* kurulu flatpak kimlikleri · kimlik → Uyg */
static SoupSession *oturum;
static int fp_var;                                   /* flatpak kurulu mu */
static guint arama_zaman;
static int secili_bolum = 0;
static Uyg *acik_uyg;
static int mesgul;

/* ---------- katalog ---------- */
typedef struct { Uyg *u; GString *metin; char *eleman; int dil_uygun, aciklama_ici, simge_64, kategori; const char *depo; int derinlik, bilesen_derinlik, son_li; } Ayristirici;

static int dil_uygun(const char **an, const char **de) {
  for (int i = 0; an[i]; i++) if (!strcmp(an[i], "xml:lang")) return ae_en() ? 0 : !strcmp(de[i], "tr") ? 2 : 0;
  return 1;   /* dil belirtilmemiş: İngilizce/varsayılan */
}

static void ac(GMarkupParseContext *c, const char *el, const char **an, const char **de, gpointer v, GError **e) {
  (void)c; (void)e;
  Ayristirici *a = v;
  a->derinlik++;
  if (!strcmp(el, "component")) {
    a->bilesen_derinlik = a->derinlik;
    const char *tur = NULL;
    for (int i = 0; an[i]; i++) if (!strcmp(an[i], "type")) tur = de[i];
    a->u = (tur && !strcmp(tur, "desktop-application")) ? g_new0(Uyg, 1) : NULL;
    if (a->u) a->u->depo = g_strdup(a->depo);
    return;
  }
  if (!strcmp(el, "components")) {
    for (int i = 0; an[i]; i++) if (!strcmp(an[i], "origin")) a->depo = g_intern_string(de[i]);
    return;
  }
  if (!a->u) return;
  g_string_truncate(a->metin, 0);
  g_free(a->eleman); a->eleman = g_strdup(el);
  a->dil_uygun = dil_uygun(an, de);
  if (!strcmp(el, "description")) { a->aciklama_ici = 1; a->son_li = 0; if (a->dil_uygun) { g_free(a->u->aciklama); a->u->aciklama = NULL; } }
  if (!strcmp(el, "icon")) {
    const char *tur = "", *gen = "";
    for (int i = 0; an[i]; i++) { if (!strcmp(an[i], "type")) tur = de[i]; if (!strcmp(an[i], "width")) gen = de[i]; }
    a->simge_64 = !strcmp(tur, "cached") ? (!strcmp(gen, "64") ? 1 : 3) : !strcmp(tur, "stock") ? 2 : 0;
  }
  if (!strcmp(el, "launchable")) {
    a->kategori = 0;
    for (int i = 0; an[i]; i++) if (!strcmp(an[i], "type") && strcmp(de[i], "desktop-id")) a->kategori = -1;
  }
}

static void ekle_kat(Uyg *u, const char *k) {
  if (!strcmp(k, "Game")) u->kat |= K_OYUN;
  else if (!strcmp(k, "Network")) u->kat |= K_INTERNET;
  else if (!strcmp(k, "Office")) u->kat |= K_OFIS;
  else if (!strcmp(k, "Graphics")) u->kat |= K_GRAFIK;
  else if (!strcmp(k, "AudioVideo") || !strcmp(k, "Audio") || !strcmp(k, "Video")) u->kat |= K_SES;
  else if (!strcmp(k, "Development")) u->kat |= K_GELISTIRME;
  else if (!strcmp(k, "Education") || !strcmp(k, "Science")) u->kat |= K_EGITIM;
  else if (!strcmp(k, "Utility") || !strcmp(k, "System")) u->kat |= K_ARAC;
}

/* dil önceliği: Türkçe (2) > varsayılan (1) */
static void ata(char **hedef, int *oncelik, int yeni, const char *deger) {
  if (yeni >= *oncelik) { g_free(*hedef); *hedef = g_strdup(deger); *oncelik = yeni; }
}
static int on_ad, on_ozet;

static void kapa(GMarkupParseContext *c, const char *el, gpointer v, GError **e) {
  (void)c; (void)e;
  Ayristirici *a = v;
  int derin = a->derinlik--;
  if (!a->u) return;
  Uyg *u = a->u;
  int dogrudan = derin == a->bilesen_derinlik + 1;   /* <component>'in doğrudan çocuğu (geliştirici adı vb. değil) */
  /* XML'deki satır kırılımları ve girintiler tek boşluk olsun */
  GString *ns = g_string_new(NULL); int bos = 0;
  for (const char *q = a->metin->str; *q; q++) {
    if (g_ascii_isspace(*q)) { if (!bos) g_string_append_c(ns, ' '); bos = 1; }
    else { g_string_append_c(ns, *q); bos = 0; }
  }
  char *t = g_strstrip(g_string_free(ns, FALSE));
  if (!strcmp(el, "component")) {
    if (u->paket && u->ad) { g_ptr_array_add(uygulamalar, u); }
    else { g_free(u->id); g_free(u->ad); g_free(u); }
    a->u = NULL; on_ad = on_ozet = 0;
  } else if (!strcmp(el, "id")) { g_free(u->id); u->id = g_strdup(t); }
  else if (!strcmp(el, "name") && dogrudan && a->dil_uygun) ata(&u->ad, &on_ad, a->dil_uygun, t);
  else if (!strcmp(el, "summary") && dogrudan && a->dil_uygun) ata(&u->ozet, &on_ozet, a->dil_uygun, t);
  else if (!strcmp(el, "pkgname")) { g_free(u->paket); u->paket = g_strdup(t); }
  else if (!strcmp(el, "launchable") && a->kategori == 0 && !u->masaustu) u->masaustu = g_strdup(t);
  else if (!strcmp(el, "category")) ekle_kat(u, t);
  else if (!strcmp(el, "icon")) {
    if (a->simge_64 == 1 || (a->simge_64 == 3 && !u->simge)) { g_free(u->simge); u->simge = g_strdup(t); }
    else if (a->simge_64 == 2 && !u->stok) u->stok = g_strdup(t);
  } else if ((!strcmp(el, "p") || !strcmp(el, "li")) && a->aciklama_ici && a->dil_uygun) {
    char *eski = u->aciklama;
    int li = !strcmp(el, "li");
    u->aciklama = g_strdup_printf("%s%s%s%s", eski ? eski : "", eski ? (li && a->son_li ? "\n" : "\n\n") : "", li ? "•  " : "", t);
    a->son_li = li;
    g_free(eski);
  } else if (!strcmp(el, "description")) a->aciklama_ici = 0;
  g_free(t);
  g_string_truncate(a->metin, 0);
}

static void metin(GMarkupParseContext *c, const char *m, gsize n, gpointer v, GError **e) {
  (void)c; (void)e;
  Ayristirici *a = v;
  if (a->u) g_string_append_len(a->metin, m, n);
}

static void katalog_oku(const char *dosya) {
  GFile *f = g_file_new_for_path(dosya);
  GFileInputStream *gs = g_file_read(f, NULL, NULL);
  g_object_unref(f);
  if (!gs) return;
  GConverter *z = G_CONVERTER(g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP));
  GInputStream *s = g_converter_input_stream_new(G_INPUT_STREAM(gs), z);
  GMarkupParser p = { ac, kapa, metin, NULL, NULL };
  Ayristirici a = { 0 }; a.metin = g_string_new(NULL);
  GMarkupParseContext *ctx = g_markup_parse_context_new(&p, 0, &a, NULL);
  char tampon[65536]; gssize n;
  while ((n = g_input_stream_read(s, tampon, sizeof tampon, NULL, NULL)) > 0)
    if (!g_markup_parse_context_parse(ctx, tampon, n, NULL)) break;
  g_markup_parse_context_free(ctx);
  g_string_free(a.metin, TRUE); g_free(a.eleman);
  g_object_unref(s); g_object_unref(z); g_object_unref(gs);
}

static int multilib_acik(void) {
  char *c = NULL; int r = 0;
  if (g_file_get_contents("/etc/pacman.conf", &c, NULL, NULL)) {
    char **s = g_strsplit(c, "\n", -1);
    for (int i = 0; s[i]; i++) if (!strcmp(g_strstrip(s[i]), "[multilib]")) r = 1;
    g_strfreev(s); g_free(c);
  }
  return r;
}

static int uyg_sirala(gconstpointer a, gconstpointer b) {
  const Uyg *x = *(Uyg **)a, *y = *(Uyg **)b;
  return g_utf8_collate(x->ad, y->ad);
}

static void katalog_yukle(void) {
  uygulamalar = g_ptr_array_new();
  katalog_oku(KATALOG "/extra.xml.gz");
  katalog_oku(KATALOG "/core.xml.gz");
  if (multilib_acik()) katalog_oku(KATALOG "/multilib.xml.gz");
  /* aynı pakette birden çok bileşen olabilir; öne çıkanlarda paketin asıl uygulaması seçilsin */
  for (int i = 0; one_cikanlar[i]; i++) {
    Uyg *en_iyi = NULL;
    for (unsigned j = 0; j < uygulamalar->len; j++) {
      Uyg *u = uygulamalar->pdata[j];
      if (strcmp(u->paket, one_cikanlar[i])) continue;
      if (!en_iyi || (u->id && strstr(u->id, one_cikanlar[i]) && !(en_iyi->id && strstr(en_iyi->id, one_cikanlar[i])))) en_iyi = u;
      if (!strcmp(u->paket, "libreoffice-fresh") && u->id && strstr(u->id, "startcenter")) en_iyi = u;
    }
    if (en_iyi) en_iyi->one_cikan = i + 1;
  }
  g_ptr_array_sort(uygulamalar, uyg_sirala);
}

static GHashTable *komut_listesi(const char *komut) {
  GHashTable *h = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  char *cikti = NULL;
  if (g_spawn_command_line_sync(komut, &cikti, NULL, NULL, NULL) && cikti) {
    char **s = g_strsplit(cikti, "\n", -1);
    for (int i = 0; s[i]; i++) {
      char *ad = g_strstrip(s[i]); char *bosluk = strchr(ad, ' '); if (bosluk) *bosluk = 0;
      if (*ad) g_hash_table_add(h, g_strdup(ad));
    }
    g_strfreev(s);
  }
  g_free(cikti);
  return h;
}

static void durum_yenile(void) {
  if (yuklu) g_hash_table_destroy(yuklu);
  if (guncellenecek) g_hash_table_destroy(guncellenecek);
  yuklu = komut_listesi("pacman -Qq");
  guncellenecek = komut_listesi("pacman -Qu");
  if (fp_yuklu) g_hash_table_destroy(fp_yuklu);
  fp_yuklu = fp_var ? komut_listesi("flatpak list --app --columns=application") : g_hash_table_new(g_str_hash, g_str_equal);
}

/* ---------- Flathub ---------- */
static int gecerli_kimlik(const char *k) {
  if (!k || !*k || *k == '-') return 0;
  for (const char *q = k; *q; q++) if (!g_ascii_isalnum(*q) && *q != '.' && *q != '_' && *q != '-') return 0;
  return 1;
}
static Uyg *fh_uyg(JsonObject *o) {
  const char *id = json_object_has_member(o, "app_id") ? json_object_get_string_member_with_default(o, "app_id", NULL) : NULL;
  if (!gecerli_kimlik(id)) return NULL;
  Uyg *u = g_hash_table_lookup(fp_onbellek, id);
  if (u) return u;
  u = g_new0(Uyg, 1);
  u->flathub = 1;
  u->paket = g_strdup(id); u->id = g_strdup(id); u->stok = g_strdup(id); u->masaustu = g_strdup_printf("%s.desktop", id);
  u->ad = g_strdup(json_object_get_string_member_with_default(o, "name", id));
  u->ozet = g_strdup(json_object_get_string_member_with_default(o, "summary", ""));
  const char *ac = json_object_get_string_member_with_default(o, "description", NULL);
  if (ac) {   /* paragraflar arasındaki girintileri ayıkla */
    char **s = g_strsplit(ac, "\n", -1); GString *g = g_string_new(NULL);
    for (int i = 0; s[i]; i++) { char *t = g_strstrip(s[i]); if (*t) g_string_append_printf(g, "%s%s", g->len ? "\n\n" : "", t); }
    u->aciklama = g_string_free(g, FALSE); g_strfreev(s);
  }
  u->simge = g_strdup(json_object_get_string_member_with_default(o, "icon", NULL));
  u->gelistirici = g_strdup(json_object_get_string_member_with_default(o, "developer_name", NULL));
  u->lisans = g_strdup(json_object_get_string_member_with_default(o, "project_license", NULL));
  g_hash_table_insert(fp_onbellek, g_strdup(id), u);
  return u;
}
/* kurulu ama henüz Flathub'dan bilgisi gelmemiş flatpak'lar (Yüklü sayfası için) */
static void fp_yerel_doldur(void) {
  if (!fp_var) return;
  char *cikti = NULL;
  if (!g_spawn_command_line_sync("flatpak list --app --columns=application,name,description", &cikti, NULL, NULL, NULL) || !cikti) return;
  char **s = g_strsplit(cikti, "\n", -1);
  for (int i = 0; s[i]; i++) {
    char **a = g_strsplit(s[i], "\t", 3);
    if (a[0] && gecerli_kimlik(a[0]) && !g_hash_table_contains(fp_onbellek, a[0])) {
      Uyg *u = g_new0(Uyg, 1); u->flathub = 1;
      u->paket = g_strdup(a[0]); u->id = g_strdup(a[0]); u->stok = g_strdup(a[0]); u->masaustu = g_strdup_printf("%s.desktop", a[0]);
      u->ad = g_strdup(a[1] && *a[1] ? a[1] : a[0]); u->ozet = g_strdup(a[1] && a[2] ? a[2] : "");
      g_hash_table_insert(fp_onbellek, g_strdup(a[0]), u);
    }
    g_strfreev(a);
  }
  g_strfreev(s); g_free(cikti);
}

/* ---------- simgeler ---------- */
static char *fh_simge_yolu(Uyg *u) { return g_build_filename(g_get_user_cache_dir(), "aether-magaza", "simgeler", u->paket, NULL); }
static GdkPixbuf *simge(Uyg *u, int boy) {
  GdkPixbuf *pb = NULL;
  if (u->flathub) {
    char *y = fh_simge_yolu(u);
    pb = gdk_pixbuf_new_from_file_at_scale(y, boy, boy, TRUE, NULL);
    g_free(y);
  } else if (u->simge) {
    char *taban = g_str_has_suffix(u->simge, ".jxl") ? g_strndup(u->simge, strlen(u->simge) - 4) : g_strdup(u->simge);
    char *yol = g_strdup_printf(SIMGELER "/%s.png", taban);
    pb = gdk_pixbuf_new_from_file_at_scale(yol, boy, boy, TRUE, NULL);
    g_free(yol); g_free(taban);
  }
  if (!pb) {
    GtkIconTheme *t = gtk_icon_theme_get_default();
    pb = gtk_icon_theme_load_icon(t, u->stok ? u->stok : "application-x-executable", boy, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
    if (!pb) pb = gtk_icon_theme_load_icon(t, "application-x-executable", boy, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
  }
  return pb;
}

/* ---------- işlemler (sudo) ---------- */
#include "yetki.h"
static int yetki_al(const char *neden) { return ae_yetki_al(GTK_WINDOW(pencere), neden); }

static void sayfa_goster(void);
static void ayrinti_goster(Uyg *u);

typedef struct { GPid pid; char *ad; char *islem; GString *gunluk; } Is;

static gboolean is_satiri(GIOChannel *ch, GIOCondition c, gpointer v) {
  Is *is = v;
  if (c & G_IO_IN) {
    char *satir = NULL; gsize n;
    if (g_io_channel_read_line(ch, &satir, &n, NULL, NULL) == G_IO_STATUS_NORMAL && satir) {
      g_strchomp(satir);
      g_string_append_printf(is->gunluk, "%s\n", satir);
      /* "(3/12) ..." → ilerleme */
      int a, b;
      if (sscanf(satir, "(%d/%d)", &a, &b) == 2 && b > 0) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ilerleme), (double)a / b);
      else gtk_progress_bar_pulse(GTK_PROGRESS_BAR(ilerleme));
      g_free(satir);
      return TRUE;
    }
  }
  if (c & (G_IO_HUP | G_IO_ERR)) return FALSE;
  return TRUE;
}

static void is_bitti(GPid pid, int st, gpointer v) {
  Is *is = v;
  g_spawn_close_pid(pid);
  mesgul = 0;
  int ok = g_spawn_check_wait_status(st, NULL);
  gtk_revealer_set_reveal_child(GTK_REVEALER(durum_cubugu), FALSE);
  durum_yenile();
  fp_yerel_doldur();
  if (!ok) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s",
                                          T("İşlem tamamlanamadı.", "The operation could not be completed."));
    /* son satırlar ipucu olsun */
    char **s = g_strsplit(is->gunluk->str, "\n", -1); int n = g_strv_length(s);
    GString *son = g_string_new(NULL);
    for (int i = MAX(0, n - 8); i < n; i++) if (*s[i]) g_string_append_printf(son, "%s\n", s[i]);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", son->str);
    g_string_free(son, TRUE); g_strfreev(s);
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
  } else {
    char *m = !strcmp(is->islem, "kur") ? g_strdup_printf(T("%s yüklendi", "%s installed"), is->ad)
            : !strcmp(is->islem, "kaldir") ? g_strdup_printf(T("%s kaldırıldı", "%s removed"), is->ad)
            : !strcmp(is->islem, "fpguncelle") ? g_strdup(T("Flatpak uygulamaları güncel", "Flatpak apps are up to date"))
            : g_strdup(T("Sistem güncel", "System is up to date"));
    char *argv[] = { "notify-send", "-a", "Mağaza", "-i", "aether-magaza", m, NULL };
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
    g_free(m);
  }
  if (acik_uyg && gtk_stack_get_visible_child_name(GTK_STACK(yigin)) && !strcmp(gtk_stack_get_visible_child_name(GTK_STACK(yigin)), "ayrinti")) ayrinti_goster(acik_uyg);
  else sayfa_goster();
  g_string_free(is->gunluk, TRUE); g_free(is->ad); g_free(is->islem); g_free(is);
}

static void is_calistir(const char *islem, const char *ad, char **argv, const char *mesaj);
static void is_baslat(const char *islem, const char *ad, char **paketler) {
  if (mesgul) return;
  const char *neden = !strcmp(islem, "kur") ? T("Uygulama yüklemek yönetici izni gerektirir.", "Installing apps requires administrator permission.")
                    : !strcmp(islem, "kaldir") ? T("Uygulama kaldırmak yönetici izni gerektirir.", "Removing apps requires administrator permission.")
                    : T("Sistemi güncellemek yönetici izni gerektirir.", "Updating the system requires administrator permission.");
  if (!yetki_al(neden)) return;
  GPtrArray *a = g_ptr_array_new();
  g_ptr_array_add(a, "sudo"); g_ptr_array_add(a, "-n"); g_ptr_array_add(a, YARDIMCI); g_ptr_array_add(a, (char *)islem);
  for (int i = 0; paketler && paketler[i]; i++) g_ptr_array_add(a, paketler[i]);
  g_ptr_array_add(a, NULL);
  char *m = !strcmp(islem, "kur") ? g_strdup_printf(T("%s yükleniyor… (önce sistem de güncellenir)", "Installing %s… (the system is updated first)"), ad)
          : !strcmp(islem, "kaldir") ? g_strdup_printf(T("%s kaldırılıyor…", "Removing %s…"), ad)
          : !strcmp(islem, "yenile") ? g_strdup(T("Güncellemeler denetleniyor…", "Checking for updates…"))
          : g_strdup(T("Sistem güncelleniyor…", "Updating the system…"));
  is_calistir(islem, ad, (char **)a->pdata, m);
  g_free(m);
  g_ptr_array_free(a, TRUE);
}

static void is_calistir(const char *islem, const char *ad, char **argv, const char *mesaj) {
  Is *is = g_new0(Is, 1); is->ad = g_strdup(ad); is->islem = g_strdup(islem); is->gunluk = g_string_new(NULL);
  int cikis;
  GError *e = NULL;
  if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL,
                                NULL, NULL, &is->pid, NULL, &cikis, NULL, &e)) {
    g_free(is->ad); g_free(is->islem); g_string_free(is->gunluk, TRUE); g_free(is);
    if (e) g_error_free(e);
    return;
  }
  mesgul = 1;
  GIOChannel *ch = g_io_channel_unix_new(cikis);
  g_io_channel_set_close_on_unref(ch, TRUE);
  g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, is_satiri, is);
  g_io_channel_unref(ch);
  g_child_watch_add(is->pid, is_bitti, is);
  gtk_label_set_text(GTK_LABEL(durum_yazi), mesaj);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ilerleme), 0);
  gtk_revealer_set_reveal_child(GTK_REVEALER(durum_cubugu), TRUE);
  if (acik_uyg) ayrinti_goster(acik_uyg); else sayfa_goster();
}

/* Flatpak: kullanıcıya özel kurulum, parola gerekmez. Flathub tanımı yoksa önce eklenir. */
#define FH_UZAK "flatpak remote-add --user --if-not-exists flathub /etc/flatpak/remotes.d/flathub.flatpakrepo 2>/dev/null || " \
                "flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo"
static void fp_islem(const char *islem, Uyg *u) {
  char *k = NULL, *m = NULL;
  if (!strcmp(islem, "kur")) {
    k = g_strdup_printf(FH_UZAK " && exec flatpak install --user -y --noninteractive flathub %s 2>&1", u->paket);
    m = g_strdup_printf(T("%s Flathub'dan yükleniyor… (ilk Flathub uygulamasında ortak parçalar da iner, biraz sürebilir)",
                          "Installing %s from Flathub… (the first Flathub app also downloads shared runtimes; this can take a while)"), u->ad);
  } else if (!strcmp(islem, "kaldir")) {
    k = g_strdup_printf("flatpak uninstall --user -y --noninteractive %s 2>&1 || flatpak uninstall -y --noninteractive %s 2>&1", u->paket, u->paket);
    m = g_strdup_printf(T("%s kaldırılıyor…", "Removing %s…"), u->ad);
  }
  char *argv[] = { "sh", "-c", k, NULL };
  is_calistir(islem, u->ad, argv, m);
  g_free(k); g_free(m);
}

static void uyg_kur(Uyg *u) {
  if (mesgul) return;
  if (u->flathub) { fp_islem("kur", u); return; }
  char *p[3] = { u->paket, NULL, NULL };
  for (int i = 0; ek_paketler[i].paket; i++)
    if (!strcmp(ek_paketler[i].paket, u->paket) && (!ek_paketler[i].yalniz_tr || !ae_en())) p[1] = (char *)ek_paketler[i].ek;
  is_baslat("kur", u->ad, p);
}
static void uyg_kaldir(Uyg *u) {
  char *m = g_strdup_printf(T("%s kaldırılsın mı?", "Remove %s?"), u->ad);
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "%s", m);
  g_free(m);
  gtk_dialog_add_buttons(GTK_DIALOG(d), T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("Kaldır", "Remove"), GTK_RESPONSE_OK, NULL);
  int r = gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
  if (r != GTK_RESPONSE_OK) return;
  if (u->flathub) { fp_islem("kaldir", u); return; }
  char *p[2] = { u->paket, NULL };
  is_baslat("kaldir", u->ad, p);
}
static void uyg_ac(Uyg *u) {
  if (u->flathub) { char *argv[] = { "flatpak", "run", u->paket, NULL }; g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL); return; }
  GDesktopAppInfo *a = u->masaustu ? g_desktop_app_info_new(u->masaustu) : NULL;
  if (!a && u->id) a = g_desktop_app_info_new(u->id);
  if (a) { g_app_info_launch(G_APP_INFO(a), NULL, NULL, NULL); g_object_unref(a); }
}

/* ---------- arayüz ---------- */
static int yuklu_mu(Uyg *u) { return u->flathub ? fp_yuklu && g_hash_table_contains(fp_yuklu, u->paket) : yuklu && g_hash_table_contains(yuklu, u->paket); }
static int temel_mi(Uyg *u) { return !u->flathub && temel && g_hash_table_contains(temel, u->paket); }

static void d_kur(GtkButton *b, gpointer v) { (void)b; uyg_kur(v); }
static void d_kaldir(GtkButton *b, gpointer v) { (void)b; uyg_kaldir(v); }
static void d_ac(GtkButton *b, gpointer v) { (void)b; uyg_ac(v); }
static void d_ayrinti(GtkButton *b, gpointer v) { (void)b; ayrinti_goster(v); }

static GtkWidget *eylem_dugmesi(Uyg *u, int buyuk) {
  GtkWidget *b;
  if (yuklu_mu(u)) {
    b = gtk_button_new_with_label(T("Aç", "Open"));
    g_signal_connect(b, "clicked", G_CALLBACK(d_ac), u);
  } else {
    b = gtk_button_new_with_label(T("Yükle", "Install"));
    gtk_style_context_add_class(gtk_widget_get_style_context(b), "suggested-action");
    g_signal_connect(b, "clicked", G_CALLBACK(d_kur), u);
  }
  gtk_widget_set_sensitive(b, !mesgul);
  if (buyuk) gtk_widget_set_size_request(b, 110, -1);
  gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
  return b;
}

/* Flathub simgesini indir, önbelleğe yaz, görüntüyü güncelle */
typedef struct { Uyg *u; GtkWidget *im; int boy; } SimgeIstegi;
static void simge_geldi(GObject *k, GAsyncResult *r, gpointer v) {
  SimgeIstegi *si = v;
  GBytes *b = soup_session_send_and_read_finish(SOUP_SESSION(k), r, NULL);
  if (b && g_bytes_get_size(b) > 0) {
    char *y = fh_simge_yolu(si->u), *d = g_path_get_dirname(y);
    g_mkdir_with_parents(d, 0755);
    g_file_set_contents(y, g_bytes_get_data(b, NULL), g_bytes_get_size(b), NULL);
    if (si->im) {
      GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(y, si->boy, si->boy, TRUE, NULL);
      if (pb) { gtk_image_set_from_pixbuf(GTK_IMAGE(si->im), pb); g_object_unref(pb); }
    }
    g_free(y); g_free(d);
  }
  if (b) g_bytes_unref(b);
  if (si->im) g_object_remove_weak_pointer(G_OBJECT(si->im), (gpointer *)&si->im);
  g_free(si);
}
static void simge_iste(Uyg *u, GtkWidget *im, int boy) {
  char *y = fh_simge_yolu(u);
  int var = g_file_test(y, G_FILE_TEST_EXISTS); g_free(y);
  if (var) return;
  /* henüz yoksa yerel tema simgesi (kuruluysa) ya da genel simge */
  GdkPixbuf *pb = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "application-x-executable", boy, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
  if (pb) { gtk_image_set_from_pixbuf(GTK_IMAGE(im), pb); g_object_unref(pb); }
  if (!u->simge || !g_str_has_prefix(u->simge, "https://")) return;
  SoupMessage *msg = soup_message_new("GET", u->simge);
  if (!msg) return;
  SimgeIstegi *si = g_new0(SimgeIstegi, 1); si->u = u; si->im = im; si->boy = boy;
  g_object_add_weak_pointer(G_OBJECT(im), (gpointer *)&si->im);
  soup_session_send_and_read_async(oturum, msg, G_PRIORITY_LOW, NULL, simge_geldi, si);
  g_object_unref(msg);
}

static GtkWidget *kart(Uyg *u) {
  GtkWidget *d = gtk_button_new();
  gtk_style_context_add_class(gtk_widget_get_style_context(d), "magaza-kart");
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  GdkPixbuf *pb = simge(u, 48);
  GtkWidget *im = pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
  if (pb) g_object_unref(pb);
  if (u->flathub) simge_iste(u, im, 48);
  gtk_widget_set_valign(im, GTK_ALIGN_START);
  GtkWidget *y = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
  GtkWidget *ad = gtk_label_new(u->ad);
  gtk_label_set_xalign(GTK_LABEL(ad), 0); gtk_label_set_ellipsize(GTK_LABEL(ad), PANGO_ELLIPSIZE_END);
  gtk_style_context_add_class(gtk_widget_get_style_context(ad), "magaza-ad");
  GtkWidget *oz = gtk_label_new(u->ozet ? u->ozet : "");
  gtk_label_set_xalign(GTK_LABEL(oz), 0); gtk_label_set_yalign(GTK_LABEL(oz), 0);
  gtk_label_set_line_wrap(GTK_LABEL(oz), TRUE); gtk_label_set_lines(GTK_LABEL(oz), 2);
  gtk_label_set_ellipsize(GTK_LABEL(oz), PANGO_ELLIPSIZE_END); gtk_label_set_max_width_chars(GTK_LABEL(oz), 30);
  gtk_style_context_add_class(gtk_widget_get_style_context(oz), "ae-alt");
  gtk_box_pack_start(GTK_BOX(y), ad, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(y), oz, FALSE, FALSE, 0);
  if (u->flathub) {
    GtkWidget *fk = gtk_label_new("Flathub");
    gtk_label_set_xalign(GTK_LABEL(fk), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(fk), "magaza-kaynak");
    gtk_box_pack_start(GTK_BOX(y), fk, FALSE, FALSE, 0);
  }
  if (yuklu_mu(u)) {
    GtkWidget *ok = gtk_label_new(T("✓ Yüklü", "✓ Installed"));
    gtk_label_set_xalign(GTK_LABEL(ok), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(ok), "magaza-yuklu");
    gtk_box_pack_start(GTK_BOX(y), ok, FALSE, FALSE, 0);
  }
  gtk_box_pack_start(GTK_BOX(k), im, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), y, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(d), k);
  gtk_widget_set_size_request(d, 300, 96);
  g_signal_connect(d, "clicked", G_CALLBACK(d_ayrinti), u);
  return d;
}

static void akis_temizle(void) {
  GList *c = gtk_container_get_children(GTK_CONTAINER(akis));
  for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(c);
}

static int esles(Uyg *u, const char *a) {
  if (!*a) return 1;
  char *x = g_utf8_casefold(a, -1); int r = 0;
  /* "oyun", "ofis", "grafik"... yazılınca o kategorideki her şey */
  if (g_utf8_strlen(x, -1) >= 3)
    for (unsigned i = 1; i < BOLUM_SAYI && !r; i++) {
      if (!bolumler[i].maske || !(u->kat & bolumler[i].maske)) continue;
      char *tr = g_utf8_casefold(bolumler[i].tr, -1), *en = g_utf8_casefold(bolumler[i].en, -1);
      r = g_str_has_prefix(tr, x) || g_str_has_prefix(en, x);
      g_free(tr); g_free(en);
    }
  const char *alanlar[] = { u->ad, u->ozet, u->paket };
  for (int i = 0; i < 3 && !r; i++) if (alanlar[i]) { char *y = g_utf8_casefold(alanlar[i], -1); r = strstr(y, x) != NULL; g_free(y); }
  g_free(x);
  return r;
}

static int one_sirala(gconstpointer a, gconstpointer b) { return (*(Uyg **)a)->one_cikan - (*(Uyg **)b)->one_cikan; }

static void guncelle_tik(GtkButton *b, gpointer v) { (void)b; (void)v; is_baslat("guncelle", "", NULL); }
static void fp_guncelle_tik(GtkButton *b, gpointer v) {
  (void)b; (void)v;
  if (mesgul) return;
  char *argv[] = { "sh", "-c", "exec flatpak update --user -y --noninteractive 2>&1", NULL };
  is_calistir("fpguncelle", "", argv, T("Flatpak uygulamaları güncelleniyor…", "Updating Flatpak apps…"));
}
static void denetle_tik(GtkButton *b, gpointer v) { (void)b; (void)v; is_baslat("yenile", "", NULL); }

static guint fh_sayac;
typedef struct { guint sayac; int arama; } FhIstek;
static void fh_geldi(GObject *k, GAsyncResult *r, gpointer v) {
  FhIstek *fi = v;
  GError *e = NULL;
  GBytes *b = soup_session_send_and_read_finish(SOUP_SESSION(k), r, &e);
  if (e) g_printerr("aether-magaza: Flathub: %s\n", e->message);
  if (fi->sayac != fh_sayac) goto bitti;   /* kullanıcı bu arada başka bir şey aradı */
  GtkWidget *hedef = fi->arama ? fh_akis : akis, *alt = fi->arama ? fh_alt : akis_alt;
  JsonParser *jp = json_parser_new();
  if (!b || !json_parser_load_from_data(jp, g_bytes_get_data(b, NULL), g_bytes_get_size(b), NULL) || !JSON_NODE_HOLDS_OBJECT(json_parser_get_root(jp))) {
    gtk_label_set_text(GTK_LABEL(alt), T("Flathub'a ulaşılamadı. İnternet bağlantını kontrol et.", "Could not reach Flathub. Check your internet connection."));
    g_object_unref(jp); goto bitti;
  }
  JsonObject *kok = json_node_get_object(json_parser_get_root(jp));
  JsonArray *hits = json_object_has_member(kok, "hits") ? json_object_get_array_member(kok, "hits") : NULL;
  int n = 0;
  for (guint i = 0; hits && i < json_array_get_length(hits) && n < 60; i++) {
    JsonNode *h = json_array_get_element(hits, i);
    if (!JSON_NODE_HOLDS_OBJECT(h)) continue;
    JsonObject *o = json_node_get_object(h);
    const char *tur = json_object_get_string_member_with_default(o, "type", "desktop-application");
    if (strcmp(tur, "desktop-application") && strcmp(tur, "desktop")) continue;
    Uyg *u = fh_uyg(o);
    if (!u) continue;
    gtk_container_add(GTK_CONTAINER(hedef), kart(u)); n++;
  }
  gtk_widget_show_all(hedef);
  char *m = n ? g_strdup_printf(T("%d uygulama · Flathub", "%d apps · Flathub"), n) : g_strdup(T("Flathub'da sonuç yok.", "No results on Flathub."));
  if (!fi->arama && n) { g_free(m); m = g_strdup(T("Flathub'da bu ay en çok yüklenenler. Kullanıcıya özel kurulur, parola gerekmez.", "Most installed on Flathub this month. Installed for your user, no password needed.")); }
  gtk_label_set_text(GTK_LABEL(alt), m); g_free(m);
  g_object_unref(jp);
bitti:
  if (b) g_bytes_unref(b);
  if (e) g_error_free(e);
  g_free(fi);
}
static void fh_getir(const char *yol, const char *aranan, int arama) {
  SoupMessage *msg;
  if (aranan) {
    msg = soup_message_new("POST", FLATHUB_API "/search");
    JsonBuilder *jb = json_builder_new();
    json_builder_begin_object(jb); json_builder_set_member_name(jb, "query"); json_builder_add_string_value(jb, aranan); json_builder_end_object(jb);
    JsonGenerator *g = json_generator_new(); json_generator_set_root(g, json_builder_get_root(jb));
    char *govde = json_generator_to_data(g, NULL);
    GBytes *gb = g_bytes_new_take(govde, strlen(govde));
    soup_message_set_request_body_from_bytes(msg, "application/json", gb);
    g_bytes_unref(gb); g_object_unref(g); g_object_unref(jb);
  } else {
    char *url = g_strdup_printf(FLATHUB_API "%s", yol);
    msg = soup_message_new("GET", url); g_free(url);
  }
  if (!msg) return;
  FhIstek *fi = g_new0(FhIstek, 1); fi->sayac = ++fh_sayac; fi->arama = arama;
  soup_session_send_and_read_async(oturum, msg, G_PRIORITY_DEFAULT, NULL, fh_geldi, fi);
  g_object_unref(msg);
}
static gboolean fh_arama_zamani(gpointer v) {
  (void)v; arama_zaman = 0;
  fh_getir(NULL, gtk_entry_get_text(GTK_ENTRY(arama)), 1);
  return G_SOURCE_REMOVE;
}

static void sayfa_goster(void) {
  acik_uyg = NULL;
  /* Flathub bölümünü sıfırla */
  GList *fc = gtk_container_get_children(GTK_CONTAINER(fh_akis));
  for (GList *l = fc; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(fc);
  gtk_widget_hide(fh_baslik); gtk_widget_hide(fh_alt); gtk_widget_hide(fh_akis);
  fh_sayac++;
  gtk_stack_set_visible_child_name(GTK_STACK(yigin), "liste");
  akis_temizle();
  const char *aranan = gtk_entry_get_text(GTK_ENTRY(arama));
  const Bolum *b = &bolumler[secili_bolum];
  int arama_var = *aranan != 0;
  gtk_label_set_text(GTK_LABEL(akis_baslik), arama_var ? T("Arama sonuçları", "Search results") : (ae_en() ? b->en : b->tr));
  gtk_widget_hide(guncel_dugme);
  GPtrArray *liste = g_ptr_array_new();
  for (unsigned i = 0; i < uygulamalar->len; i++) {
    Uyg *u = uygulamalar->pdata[i];
    if (arama_var) { if (esles(u, aranan)) g_ptr_array_add(liste, u); continue; }
    if (!strcmp(b->id, "ana")) { if (u->one_cikan) g_ptr_array_add(liste, u); }
    else if (!strcmp(b->id, "yuklu")) { if (yuklu_mu(u) && !temel_mi(u)) g_ptr_array_add(liste, u); }
    else if (!strcmp(b->id, "guncel")) { if (guncellenecek && g_hash_table_contains(guncellenecek, u->paket)) g_ptr_array_add(liste, u); }
    else if (u->kat & b->maske) g_ptr_array_add(liste, u);
  }
  if (!arama_var && !strcmp(b->id, "ana")) g_ptr_array_sort(liste, one_sirala);
  if (!arama_var && !strcmp(b->id, "yuklu") && fp_onbellek) {
    GHashTableIter hi; gpointer k, v;
    g_hash_table_iter_init(&hi, fp_onbellek);
    while (g_hash_table_iter_next(&hi, &k, &v)) if (yuklu_mu(v)) g_ptr_array_add(liste, v);
  }
  char *alt;
  if (!arama_var && !strcmp(b->id, "guncel")) {
    int n = guncellenecek ? (int)g_hash_table_size(guncellenecek) : 0;
    alt = n ? g_strdup_printf(T("%d paket güncellenebilir. Liste en son denetlemeye göre.", "%d packages can be updated, as of the last check."), n)
            : g_strdup(T("Bilinen güncelleme yok. Yenilerini görmek için Denetle'ye bas.", "No known updates. Press Check to look for new ones."));
    gtk_button_set_label(GTK_BUTTON(guncel_dugme), n ? T("Hepsini güncelle", "Update all") : T("Denetle", "Check"));
    g_signal_handlers_disconnect_matched(guncel_dugme, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, guncelle_tik, NULL);
    g_signal_handlers_disconnect_matched(guncel_dugme, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, denetle_tik, NULL);
    g_signal_connect(guncel_dugme, "clicked", n ? G_CALLBACK(guncelle_tik) : G_CALLBACK(denetle_tik), NULL);
    gtk_widget_set_sensitive(guncel_dugme, !mesgul);
    gtk_widget_show(guncel_dugme);
  } else if (!arama_var && !strcmp(b->id, "yuklu"))
    alt = g_strdup_printf(T("Sonradan yüklediğin %u uygulama", "%u apps you installed"), liste->len);
  else if (!arama_var && !strcmp(b->id, "ana"))
    alt = g_strdup(T("Arch Linux depolarından, tek tıkla", "From the Arch Linux repositories, in one click"));
  else if (!arama_var && !strcmp(b->id, "flathub"))
    alt = g_strdup(fp_var ? T("Flathub'a bağlanılıyor…", "Connecting to Flathub…") : T("Flatpak kurulu değil.", "Flatpak is not installed."));
  else if (arama_var) alt = liste->len ? g_strdup_printf(T("%u uygulama · Arch Linux depoları", "%u apps · Arch Linux repositories"), liste->len)
                                       : g_strdup(T("Arch Linux depolarında sonuç yok.", "No results in the Arch Linux repositories."));
  else alt = g_strdup_printf(T("%u uygulama", "%u apps"), liste->len);
  gtk_label_set_text(GTK_LABEL(akis_alt), alt); g_free(alt);
  for (unsigned i = 0; i < liste->len && i < LISTE_SINIR; i++) gtk_container_add(GTK_CONTAINER(akis), kart(liste->pdata[i]));
  gtk_widget_show_all(akis);
  g_ptr_array_free(liste, TRUE);
  if (!arama_var && !strcmp(b->id, "flathub") && fp_var) fh_getir("/collection/popular", NULL, 0);
  if (!arama_var && !strcmp(b->id, "guncel") && fp_var) {
    gtk_label_set_text(GTK_LABEL(fh_baslik), "Flatpak");
    gtk_label_set_text(GTK_LABEL(fh_alt), T("Flathub'dan kurduğun uygulamalar ayrıca güncellenir.", "Apps installed from Flathub are updated separately."));
    gtk_widget_show(fh_baslik); gtk_widget_show(fh_alt);
    GtkWidget *d = gtk_button_new_with_label(T("Flatpak uygulamalarını güncelle", "Update Flatpak apps"));
    gtk_widget_set_sensitive(d, !mesgul);
    g_signal_connect(d, "clicked", G_CALLBACK(fp_guncelle_tik), NULL);
    gtk_container_add(GTK_CONTAINER(fh_akis), d);
    gtk_widget_show_all(fh_akis);
  }
  if (arama_var && fp_var) {
    gtk_label_set_text(GTK_LABEL(fh_baslik), T("Flathub'dan", "From Flathub"));
    gtk_label_set_text(GTK_LABEL(fh_alt), T("Flathub'da aranıyor…", "Searching Flathub…"));
    gtk_widget_show(fh_baslik); gtk_widget_show(fh_alt); gtk_widget_show(fh_akis);
    if (arama_zaman) g_source_remove(arama_zaman);
    arama_zaman = g_timeout_add(450, fh_arama_zamani, NULL);
  }
}

static void geri_tik(GtkButton *b, gpointer v) { (void)b; (void)v; sayfa_goster(); }

typedef struct { GtkWidget *etiket; char *paket; } BoyutIstegi;
static void boyut_geldi(GObject *k, GAsyncResult *r, gpointer v) {
  BoyutIstegi *bi = v;
  char *cikti = NULL;
  if (g_subprocess_communicate_utf8_finish(G_SUBPROCESS(k), r, &cikti, NULL, NULL) && cikti && GTK_IS_LABEL(bi->etiket)) {
    char *ind = NULL, *kur = NULL, *surum = NULL;
    char **s = g_strsplit(cikti, "\n", -1);
    for (int i = 0; s[i]; i++) {
      char *iki = strstr(s[i], " : "); if (!iki) continue;
      char *deger = g_strstrip(g_strdup(iki + 3));
      if (g_str_has_prefix(s[i], "Download Size")) { g_free(ind); ind = deger; }
      else if (g_str_has_prefix(s[i], "Installed Size")) { g_free(kur); kur = deger; }
      else if (g_str_has_prefix(s[i], "Version")) { g_free(surum); surum = deger; }
      else g_free(deger);
    }
    if (ind || kur) {
      char *m = g_strdup_printf("%s %s   ·   %s %s   ·   %s %s", T("Sürüm", "Version"), surum ? surum : "—",
                                T("İndirme", "Download"), ind ? ind : "—", T("Diskte", "On disk"), kur ? kur : "—");
      gtk_label_set_text(GTK_LABEL(bi->etiket), m); g_free(m);
    }
    g_strfreev(s); g_free(ind); g_free(kur); g_free(surum);
  }
  g_free(cikti);
  if (bi->etiket) g_object_remove_weak_pointer(G_OBJECT(bi->etiket), (gpointer *)&bi->etiket);
  g_free(bi->paket); g_free(bi);
}

static void ayrinti_goster(Uyg *u) {
  acik_uyg = u;
  GList *c = gtk_container_get_children(GTK_CONTAINER(ayrinti_kutu));
  for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(c);

  GtkWidget *geri = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(geri, T("Geri", "Back"));
  gtk_widget_set_halign(geri, GTK_ALIGN_START);
  g_signal_connect(geri, "clicked", G_CALLBACK(geri_tik), NULL);
  gtk_box_pack_start(GTK_BOX(ayrinti_kutu), geri, FALSE, FALSE, 0);

  GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
  GdkPixbuf *pb = simge(u, 96);
  GtkWidget *im = pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new(); if (pb) g_object_unref(pb);
  if (u->flathub) simge_iste(u, im, 96);
  GtkWidget *y = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *ad = gtk_label_new(u->ad); gtk_label_set_xalign(GTK_LABEL(ad), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(ad), "magaza-buyuk-ad");
  GtkWidget *oz = gtk_label_new(u->ozet ? u->ozet : ""); gtk_label_set_xalign(GTK_LABEL(oz), 0); gtk_label_set_line_wrap(GTK_LABEL(oz), TRUE);
  gtk_style_context_add_class(gtk_widget_get_style_context(oz), "ae-alt");
  GtkWidget *bilgi = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(bilgi), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(bilgi), "ae-alt");
  gtk_box_pack_start(GTK_BOX(y), ad, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(y), oz, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(y), bilgi, FALSE, FALSE, 4);
  GtkWidget *dugmeler = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start(GTK_BOX(dugmeler), eylem_dugmesi(u, 1), FALSE, FALSE, 0);
  if (yuklu_mu(u) && !temel_mi(u)) {
    GtkWidget *k = gtk_button_new_with_label(T("Kaldır", "Remove"));
    gtk_style_context_add_class(gtk_widget_get_style_context(k), "destructive-action");
    gtk_widget_set_sensitive(k, !mesgul);
    g_signal_connect(k, "clicked", G_CALLBACK(d_kaldir), u);
    gtk_box_pack_start(GTK_BOX(dugmeler), k, FALSE, FALSE, 0);
  }
  gtk_box_pack_start(GTK_BOX(y), dugmeler, FALSE, FALSE, 6);
  gtk_box_pack_start(GTK_BOX(ust), im, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ust), y, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(ayrinti_kutu), ust, FALSE, FALSE, 8);
  gtk_box_pack_start(GTK_BOX(ayrinti_kutu), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

  GtkWidget *ac = gtk_label_new(u->aciklama ? u->aciklama : T("Açıklama yok.", "No description."));
  gtk_label_set_xalign(GTK_LABEL(ac), 0); gtk_label_set_line_wrap(GTK_LABEL(ac), TRUE); gtk_label_set_selectable(GTK_LABEL(ac), TRUE);
  gtk_label_set_max_width_chars(GTK_LABEL(ac), 90);
  gtk_box_pack_start(GTK_BOX(ayrinti_kutu), ac, FALSE, FALSE, 4);
  char *pk = u->flathub
    ? g_strdup_printf(T("Kaynak: Flathub  ·  Uygulama kimliği: %s%s%s%s%s", "Source: Flathub  ·  App ID: %s%s%s%s%s"), u->paket,
                      u->gelistirici ? T("  ·  Geliştirici: ", "  ·  Developer: ") : "", u->gelistirici ? u->gelistirici : "",
                      u->lisans ? T("  ·  Lisans: ", "  ·  License: ") : "", u->lisans ? u->lisans : "")
    : g_strdup_printf(T("Paket: %s  ·  Depo: %s", "Package: %s  ·  Repository: %s"), u->paket,
                      u->depo ? (g_str_has_prefix(u->depo, "archlinux-arch-") ? u->depo + 15 : u->depo) : "—");
  GtkWidget *pl = gtk_label_new(pk); g_free(pk);
  gtk_label_set_xalign(GTK_LABEL(pl), 0); gtk_label_set_selectable(GTK_LABEL(pl), TRUE);
  gtk_style_context_add_class(gtk_widget_get_style_context(pl), "ae-alt");
  gtk_box_pack_start(GTK_BOX(ayrinti_kutu), pl, FALSE, FALSE, 8);
  gtk_widget_show_all(ayrinti_kutu);
  gtk_stack_set_visible_child_name(GTK_STACK(yigin), "ayrinti");

  if (u->flathub) {
    gtk_label_set_text(GTK_LABEL(bilgi), T("Flathub'dan kullanıcıya özel kurulur; parola gerekmez.", "Installed from Flathub for your user; no password needed."));
    return;
  }
  /* boyut ve sürüm bilgisini arka planda al */
  GSubprocessLauncher *sl = g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);
  g_subprocess_launcher_setenv(sl, "LANG", "C", TRUE);
  GSubprocess *sp = g_subprocess_launcher_spawn(sl, NULL, "pacman", "-Si", u->paket, NULL);
  g_object_unref(sl);
  if (sp) {
    BoyutIstegi *bi = g_new0(BoyutIstegi, 1); bi->etiket = bilgi; bi->paket = g_strdup(u->paket);
    g_object_add_weak_pointer(G_OBJECT(bilgi), (gpointer *)&bi->etiket);
    g_subprocess_communicate_utf8_async(sp, NULL, NULL, boyut_geldi, bi);
    g_object_unref(sp);
  }
}

static void bolum_secildi(GtkListBox *l, GtkListBoxRow *r, gpointer v) {
  (void)l; (void)v;
  if (!r) return;
  secili_bolum = gtk_list_box_row_get_index(r);
  g_signal_handlers_block_matched(arama, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)1);
  gtk_entry_set_text(GTK_ENTRY(arama), "");
  g_signal_handlers_unblock_matched(arama, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, (gpointer)1);
  sayfa_goster();
}
static void arama_degisti(GtkSearchEntry *e, gpointer v) { (void)e; (void)v; if (uygulamalar && yuklu) sayfa_goster(); }

static gboolean kapaniyor(GtkWidget *w, GdkEvent *e, gpointer v) {
  (void)w; (void)e; (void)v;
  if (!mesgul) return FALSE;
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
                                        T("Bir işlem sürüyor. Bitince kapatabilirsin.", "An operation is in progress. You can close the store when it finishes."));
  gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
  return TRUE;
}

static gboolean katalog_hazir(gpointer v) {
  (void)v;
  durum_yenile();
  fp_yerel_doldur();
  gtk_widget_set_sensitive(yan, TRUE); gtk_widget_set_sensitive(arama, TRUE);
  g_signal_connect(yan, "row-selected", G_CALLBACK(bolum_secildi), NULL);
  if (*gtk_entry_get_text(GTK_ENTRY(arama))) sayfa_goster();
  else gtk_list_box_select_row(GTK_LIST_BOX(yan), gtk_list_box_get_row_at_index(GTK_LIST_BOX(yan), 0));
  gtk_widget_grab_focus(arama);
  return G_SOURCE_REMOVE;
}
static gpointer katalog_is(gpointer v) { (void)v; katalog_yukle(); g_idle_add(katalog_hazir, NULL); return NULL; }

static void etkinlesti(GtkApplication *app, gpointer v) {
  (void)v;
  GList *w = gtk_application_get_windows(app);
  if (w) { gtk_window_present(GTK_WINDOW(w->data)); return; }
  ae_css();
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,
    ".magaza-kart { padding: 12px; border: 1px solid alpha(currentColor, 0.12); background: @theme_base_color; }"
    ".magaza-kart:hover { border-color: #7c6ad6; }"
    ".magaza-ad { font-weight: bold; }"
    ".magaza-yuklu { color: #2e7d32; font-size: 9pt; }"
    ".magaza-kaynak { color: #4a86cf; font-size: 9pt; }"
    ".magaza-baslik { font-size: 18pt; font-weight: 300; }"
    ".magaza-buyuk-ad { font-size: 20pt; font-weight: 300; }"
    ".magaza-yan row { padding: 8px 12px; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  pencere = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(pencere), T("Uygulama Mağazası", "App Store"));
  gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-magaza");
  gtk_window_set_default_size(GTK_WINDOW(pencere), 1000, 680);
  g_signal_connect(pencere, "delete-event", G_CALLBACK(kapaniyor), NULL);

  /* sol: bölümler */
  yan = gtk_list_box_new();
  gtk_style_context_add_class(gtk_widget_get_style_context(yan), "magaza-yan");
  for (unsigned i = 0; i < BOLUM_SAYI; i++) {
    GtkWidget *s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(s), gtk_image_new_from_icon_name(bolumler[i].simge, GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
    GtkWidget *l = gtk_label_new(ae_en() ? bolumler[i].en : bolumler[i].tr); gtk_label_set_xalign(GTK_LABEL(l), 0);
    gtk_box_pack_start(GTK_BOX(s), l, TRUE, TRUE, 0);
    gtk_list_box_insert(GTK_LIST_BOX(yan), s, -1);
  }
  gtk_widget_set_size_request(yan, 210, -1);

  /* üst: arama */
  arama = gtk_search_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(arama), T("Uygulama ara: Arch depoları ve Flathub (ör. prism, discord, spotify)", "Search apps in Arch and Flathub (e.g. prism, discord, spotify)"));
  g_signal_connect(arama, "search-changed", G_CALLBACK(arama_degisti), (gpointer)1);

  /* liste sayfası */
  akis_baslik = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(akis_baslik), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(akis_baslik), "magaza-baslik");
  akis_alt = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(akis_alt), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(akis_alt), "ae-alt");
  guncel_dugme = gtk_button_new_with_label("");
  gtk_style_context_add_class(gtk_widget_get_style_context(guncel_dugme), "suggested-action");
  gtk_widget_set_no_show_all(guncel_dugme, TRUE);
  akis = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(akis), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(akis), TRUE);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(akis), 10); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(akis), 10);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(akis), 4);
  gtk_widget_set_valign(akis, GTK_ALIGN_START);
  GtkWidget *lk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_container_set_border_width(GTK_CONTAINER(lk), 18);
  GtkWidget *bs = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  GtkWidget *bsy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_box_pack_start(GTK_BOX(bsy), akis_baslik, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bsy), akis_alt, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bs), bsy, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(bs), guncel_dugme, FALSE, FALSE, 0);
  gtk_widget_set_valign(guncel_dugme, GTK_ALIGN_CENTER);
  gtk_box_pack_start(GTK_BOX(lk), bs, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(lk), akis, FALSE, FALSE, 12);
  fh_baslik = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(fh_baslik), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(fh_baslik), "magaza-baslik");
  fh_alt = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(fh_alt), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(fh_alt), "ae-alt");
  fh_akis = gtk_flow_box_new();
  gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(fh_akis), GTK_SELECTION_NONE);
  gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(fh_akis), TRUE);
  gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(fh_akis), 10); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(fh_akis), 10);
  gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(fh_akis), 4);
  gtk_widget_set_valign(fh_akis, GTK_ALIGN_START);
  gtk_widget_set_no_show_all(fh_baslik, TRUE); gtk_widget_set_no_show_all(fh_alt, TRUE);
  gtk_box_pack_start(GTK_BOX(lk), fh_baslik, FALSE, FALSE, 8);
  gtk_box_pack_start(GTK_BOX(lk), fh_alt, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(lk), fh_akis, FALSE, FALSE, 12);
  GtkWidget *lks = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(lks), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(lks), lk);

  /* ayrıntı sayfası */
  ayrinti_kutu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width(GTK_CONTAINER(ayrinti_kutu), 18);
  GtkWidget *aks = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(aks), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(aks), ayrinti_kutu);

  yigin = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(yigin), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_add_named(GTK_STACK(yigin), lks, "liste");
  gtk_stack_add_named(GTK_STACK(yigin), aks, "ayrinti");

  /* alt: işlem durumu */
  durum_yazi = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(durum_yazi), 0);
  ilerleme = gtk_progress_bar_new(); gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(ilerleme), 0.05);
  GtkWidget *dk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width(GTK_CONTAINER(dk), 10);
  gtk_box_pack_start(GTK_BOX(dk), durum_yazi, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(dk), ilerleme, FALSE, FALSE, 0);
  durum_cubugu = gtk_revealer_new();
  gtk_container_add(GTK_CONTAINER(durum_cubugu), dk);

  GtkWidget *sag = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *ab = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width(GTK_CONTAINER(ab), 10);
  gtk_box_pack_start(GTK_BOX(ab), arama, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(sag), ab, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), yigin, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(sag), durum_cubugu, FALSE, FALSE, 0);

  GtkWidget *ana = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start(GTK_BOX(ana), yan, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ana), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ana), sag, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(pencere), ana);

  oturum = soup_session_new_with_options("user-agent", "Aether-Magaza/2.0", "timeout", 20, NULL);
  char *fp = g_find_program_in_path("flatpak"); fp_var = fp != NULL; g_free(fp);
  fp_onbellek = g_hash_table_new(g_str_hash, g_str_equal);
  temel = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  char *c = NULL;
  if (g_file_get_contents("/usr/share/aether/temel-paketler", &c, NULL, NULL)) {
    char **s = g_strsplit(c, "\n", -1); for (int i = 0; s[i]; i++) if (*s[i]) g_hash_table_add(temel, g_strdup(s[i]));
    g_strfreev(s); g_free(c);
  }
  /* katalog büyük (~30 MB XML); pencere hemen açılsın, okuma arka planda */
  gtk_label_set_text(GTK_LABEL(akis_baslik), T("Yükleniyor…", "Loading…"));
  gtk_widget_set_sensitive(yan, FALSE); gtk_widget_set_sensitive(arama, FALSE);
  gtk_widget_show_all(pencere);
  g_thread_unref(g_thread_new("katalog", katalog_is, NULL));
}

/* aether-magaza [arama] — ör. "aether-magaza prism" doğrudan aramayla açılır */
static void komut_satiri(GApplication *app, char **argv) {
  (void)app;
  if (argv && argv[1] && arama) gtk_entry_set_text(GTK_ENTRY(arama), argv[1]);
}
static int cmd(GApplication *app, GApplicationCommandLine *cl, gpointer v) {
  (void)v;
  g_application_activate(app);
  int argc; char **argv = g_application_command_line_get_arguments(cl, &argc);
  komut_satiri(app, argv);
  g_strfreev(argv);
  return 0;
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.aether.Magaza", G_APPLICATION_HANDLES_COMMAND_LINE);
  g_signal_connect(app, "activate", G_CALLBACK(etkinlesti), NULL);
  g_signal_connect(app, "command-line", G_CALLBACK(cmd), NULL);
  int r = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return r;
}
