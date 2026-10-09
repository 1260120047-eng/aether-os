/* aether-uygulamalar — kurulu uygulamaları (.desktop) Yörünge menü biçiminde listeler
 *
 *   aether-uygulamalar menu      Uygulamalar alt menüsünün içi: kategori alt menüleri
 *   aether-uygulamalar oyunlar   Oyunlar kategorisindeki uygulamalar (tek tek öğe)
 *
 * Çıktı biçimi aether-menu ile aynı (sekmeyle ayrılmış):
 *   O etiket simge komut · M etiket simge · E
 * pacman ile kurulan her program (ör. Prism Launcher) kendi .desktop dosyasıyla gelir
 * ve menüye kendiliğinden düşer. Menü her açılışta yeniden üretildiği için ek bir
 * güncelleme adımı gerekmez.
 */
#include <gio/gdesktopappinfo.h>
#include <gtk/gtk.h>
#include <string.h>
#include "aether.h"

/* Aether'in kendi menüsünde zaten ayrıca duranlar ya da menüde gereksiz olanlar */
static const char *gizli[] = {
  "aether-hosgeldin.desktop", "aether-hakkinda.desktop", "aether-kurulum.desktop", "aether-ayarlar.desktop",
  "aether-oyunlar.desktop", "aether-mayin.desktop", "aether-terminal.desktop", "eclipse.desktop",
  "lightdm-gtk-greeter-settings.desktop", "xdg-desktop-portal-gtk.desktop", "org.gtk.IconBrowser.desktop",
  "gtk3-icon-browser.desktop", "gtk3-demo.desktop", "gtk3-widget-factory.desktop", "avahi-discover.desktop",
  "bssh.desktop", "bvnc.desktop", "qv4l2.desktop", "qvidcap.desktop", "nm-connection-editor.desktop",
  "pcmanfm-desktop-pref.desktop", "libfm-pref-apps.desktop", "lxshortcut.desktop", "xfce4-about.desktop",
  "org.freedesktop.IBus.Setup.desktop", "feh.desktop", "org.gnupg.pinentry-qt.desktop", "org.xfce.mousepad-settings.desktop", "aether-magaza.desktop", "aether-gorev.desktop", "aether-oyun-modu.desktop", "aether-dosyalar.desktop", "cmake-gui.desktop", "jshell-java-openjdk.desktop", "jconsole-java-openjdk.desktop",
  NULL
};

/* kategori: freedesktop ana kategorisi → Türkçe / İngilizce ad, simge */
typedef struct { const char *kat, *tr, *en, *simge; GPtrArray *ogeler; } Kategori;
static Kategori kategoriler[] = {
  { "Utility",     "Donatılar",          "Accessories",    "applications-utilities", NULL },
  { "Development", "Geliştirme",         "Development",    "applications-engineering", NULL },
  { "Education",   "Eğitim",             "Education",      "applications-science", NULL },
  { "Graphics",    "Grafik",             "Graphics",       "applications-graphics", NULL },
  { "Network",     "İnternet",           "Internet",       "applications-internet", NULL },
  { "Office",      "Ofis",               "Office",         "applications-office", NULL },
  { "AudioVideo",  "Ses ve Video",       "Sound & Video",  "applications-multimedia", NULL },
  { "System",      "Sistem",             "System",         "applications-system", NULL },
  { "Settings",    "Ayarlar",            "Settings",       "preferences-system", NULL },
  { "Game",        "Oyunlar",            "Games",          "applications-games", NULL },
  { NULL,          "Diğer",              "Other",          "applications-other", NULL },
};
#define KAT_SAYI (sizeof kategoriler / sizeof kategoriler[0])

static GtkIconTheme *tema;

/* simge adını/yolunu menünün okuyabileceği bir dosya yoluna çevir */
static char *simge_yolu(GIcon *ic, const char *yedek) {
  if (ic && G_IS_FILE_ICON(ic)) {
    GFile *f = g_file_icon_get_file(G_FILE_ICON(ic));
    char *p = g_file_get_path(f);
    if (p && g_file_test(p, G_FILE_TEST_EXISTS)) return p;
    g_free(p);
  }
  GtkIconInfo *bilgi = NULL;
  if (ic && tema) bilgi = gtk_icon_theme_lookup_by_gicon(tema, ic, 48, GTK_ICON_LOOKUP_FORCE_SIZE);
  if (!bilgi && tema && yedek) bilgi = gtk_icon_theme_lookup_icon(tema, yedek, 48, GTK_ICON_LOOKUP_FORCE_SIZE);
  if (!bilgi) return g_strdup("");
  char *p = g_strdup(gtk_icon_info_get_filename(bilgi));
  g_object_unref(bilgi);
  return p ? p : g_strdup("");
}

/* sekme ve satır sonları menü biçimini bozmasın */
static char *temizle(const char *s) {
  char *t = g_strdup(s ? s : "");
  for (char *q = t; *q; q++) if (*q == '\t' || *q == '\n' || *q == '\r') *q = ' ';
  return t;
}

static int karsilastir(gconstpointer a, gconstpointer b) {
  GAppInfo *x = *(GAppInfo **)a, *y = *(GAppInfo **)b;
  char *kx = g_utf8_collate_key(g_app_info_get_display_name(x), -1);
  char *ky = g_utf8_collate_key(g_app_info_get_display_name(y), -1);
  int r = strcmp(kx, ky);
  g_free(kx); g_free(ky);
  return r;
}

static Kategori *kategori_bul(GDesktopAppInfo *d) {
  const char *k = g_desktop_app_info_get_categories(d);
  if (k) {
    char **parca = g_strsplit(k, ";", -1);
    /* Oyun önce: oyunların çoğu ayrıca "Utility" ya da "Network" de der */
    for (char **p = parca; *p; p++) if (!strcmp(*p, "Game")) { g_strfreev(parca); return &kategoriler[KAT_SAYI - 2]; }
    for (char **p = parca; *p; p++)
      for (unsigned i = 0; kategoriler[i].kat; i++)
        if (!strcmp(*p, kategoriler[i].kat)) { g_strfreev(parca); return &kategoriler[i]; }
    g_strfreev(parca);
  }
  return &kategoriler[KAT_SAYI - 1];
}

static void oge_yaz(GAppInfo *a) {
  const char *id = g_app_info_get_id(a);
  char *ad = temizle(g_app_info_get_display_name(a));
  char *simge = simge_yolu(g_app_info_get_icon(a), "application-x-executable");
  /* gtk-launch: Exec satırındaki %f %u gibi alanları ve Terminal=true'yu doğru işler */
  char *kimlik = g_shell_quote(id);
  printf("O\t%s\t%s\tgtk-launch %s\n", ad, simge, kimlik);
  g_free(ad); g_free(simge); g_free(kimlik);
}

int main(int argc, char **argv) {
  const char *kip = argc > 1 ? argv[1] : "menu";
  if (gtk_init_check(&argc, &argv)) tema = gtk_icon_theme_get_default();

  for (unsigned i = 0; i < KAT_SAYI; i++) kategoriler[i].ogeler = g_ptr_array_new();
  GList *hepsi = g_app_info_get_all();
  for (GList *l = hepsi; l; l = l->next) {
    GAppInfo *a = l->data;
    if (!G_IS_DESKTOP_APP_INFO(a) || !g_app_info_should_show(a)) continue;
    const char *id = g_app_info_get_id(a);
    int atla = 0;
    for (int k = 0; gizli[k]; k++) if (id && !strcmp(id, gizli[k])) atla = 1;
    if (atla) continue;
    g_ptr_array_add(kategori_bul(G_DESKTOP_APP_INFO(a))->ogeler, a);
  }

  if (!strcmp(kip, "oyunlar")) {
    GPtrArray *o = kategoriler[KAT_SAYI - 2].ogeler;
    g_ptr_array_sort(o, karsilastir);
    for (unsigned j = 0; j < o->len; j++) oge_yaz(o->pdata[j]);
  } else {
    for (unsigned i = 0; i < KAT_SAYI; i++) {
      GPtrArray *o = kategoriler[i].ogeler;
      if (!o->len || i == KAT_SAYI - 2) continue;   /* oyunlar kendi menüsünde */
      g_ptr_array_sort(o, karsilastir);
      char *s = simge_yolu(NULL, kategoriler[i].simge);
      printf("M\t%s\t%s\n", ae_en() ? kategoriler[i].en : kategoriler[i].tr, s);
      g_free(s);
      for (unsigned j = 0; j < o->len; j++) oge_yaz(o->pdata[j]);
      printf("E\n");
    }
  }
  g_list_free_full(hepsi, g_object_unref);
  return 0;
}
