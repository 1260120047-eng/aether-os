/* aether-dosyalar — Aether Dosyalar (dosya yöneticisi)
 *
 * Masaüstüyle aynı alışkanlıklar: Windows Gezgini'ne benzer sağ tık menüleri, Kes/Kopyala/Yapıştır
 * (masaüstü ve diğer programlarla ortak pano), Çöp Kutusu, Yeniden adlandır, Yeni klasör/metin belgesi,
 * Birlikte aç, Özellikler. Solda yerler ve diskler (GtkPlacesSidebar; USB ve Windows bölümleri tek tıkla bağlanır).
 * Simge ve ayrıntı görünümü, arama, gizli dosyalar, geri/ileri/yukarı, resim önizlemeleri, sürükle-bırak,
 * ilerleme çubuklu kopyalama/taşıma.
 *
 *   aether-dosyalar [KLASÖR|URI]
 */
#include <gio/gdesktopappinfo.h>
#include <sys/stat.h>
#include "aether.h"

enum { C_DOSYA, C_AD, C_ANAHTAR, C_SIMGE, C_KUCUK, C_KLASOR, C_BOYUT, C_BOYUT_M, C_ZAMAN, C_ZAMAN_M, C_TUR, C_TUR_M, C_GIZLI, C_SAYI };
static const char *KOPYALANAN = "x-special/gnome-copied-files";

typedef struct {
  GtkWidget *pencere, *yan, *geri, *ileri, *yukari, *yol_yigin, *yol_kutu, *yol_giris, *arama, *gorunum_yigin, *ikon, *liste, *durum;
  GtkWidget *is_cubugu, *is_yazi, *is_ilerleme;
  GtkListStore *depo; GtkTreeModel *suzgec, *sirali;
  GFile *dizin; GList *geri_l, *ileri_l;
  GCancellable *yukleme;
  GFileMonitor *izleyici; guint yenile_id;
  int gizli, liste_mi; char sira[12]; int ters;
  GCancellable *is_iptal;
} Pen;

static GtkApplication *uyg;
static void git(Pen *p, GFile *f, int gecmis);
static Pen *pencere_ac(GFile *f);
static void yenile(Pen *p);

/* ---------- yardımcılar ---------- */
static char *boyut_metni(guint64 b) {
  if (b < 1024) return g_strdup_printf(T("%lu bayt", "%lu bytes"), (unsigned long)b);
  char *s = g_format_size(b);
  return s;
}
static char *zaman_metni(gint64 t) {
  if (!t) return g_strdup("");
  GDateTime *d = g_date_time_new_from_unix_local(t);
  char *s = g_date_time_format(d, "%d.%m.%Y %H:%M");
  g_date_time_unref(d);
  return s;
}
static void hata(Pen *p, const char *m) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", m);
  gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}
static int cop_mu(Pen *p) { return p->dizin && g_file_has_uri_scheme(p->dizin, "trash"); }

static GdkPixbuf *simge_yukle(GIcon *ic, int boy, int klasor) {
  GtkIconTheme *t = gtk_icon_theme_get_default();
  GtkIconInfo *b = ic ? gtk_icon_theme_lookup_by_gicon(t, ic, boy, GTK_ICON_LOOKUP_FORCE_SIZE) : NULL;
  if (!b) b = gtk_icon_theme_lookup_icon(t, klasor ? "folder" : "text-x-generic", boy, GTK_ICON_LOOKUP_FORCE_SIZE);
  GdkPixbuf *pb = b ? gtk_icon_info_load_icon(b, NULL) : NULL;
  if (b) g_object_unref(b);
  return pb;
}

/* ---------- seçim ---------- */
static GList *secili_yollar(Pen *p) {   /* sıralı modeldeki GtkTreePath listesi */
  if (p->liste_mi) return gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)), NULL);
  return gtk_icon_view_get_selected_items(GTK_ICON_VIEW(p->ikon));
}
static GList *secili_dosyalar(Pen *p) {
  GList *y = secili_yollar(p), *d = NULL;
  for (GList *l = y; l; l = l->next) {
    GtkTreeIter it; GFile *f = NULL;
    if (gtk_tree_model_get_iter(p->sirali, &it, l->data)) { gtk_tree_model_get(p->sirali, &it, C_DOSYA, &f, -1); d = g_list_append(d, f); }
  }
  g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free);
  return d;
}
static int secili_sayisi(Pen *p) { GList *y = secili_yollar(p); int n = g_list_length(y); g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free); return n; }

static void durum_yaz(Pen *p) {
  int n = gtk_tree_model_iter_n_children(p->sirali, NULL);
  GList *y = secili_yollar(p); int s = 0; guint64 top = 0; int klasor_var = 0;
  for (GList *l = y; l; l = l->next) {
    GtkTreeIter it; gboolean k; gint64 b;
    if (gtk_tree_model_get_iter(p->sirali, &it, l->data)) { gtk_tree_model_get(p->sirali, &it, C_KLASOR, &k, C_BOYUT, &b, -1); s++; if (k) klasor_var = 1; else top += b; }
  }
  g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free);
  char *m;
  if (s) {
    char *b = boyut_metni(top);
    m = g_strdup_printf(T("%d öğe  ·  %d öğe seçili%s%s", "%d items  ·  %d selected%s%s"), n, s, klasor_var && !top ? "" : "  ·  ", klasor_var && !top ? "" : b);
    g_free(b);
  } else m = g_strdup_printf(T("%d öğe", "%d items"), n);
  gtk_label_set_text(GTK_LABEL(p->durum), m); g_free(m);
}

/* ---------- yükleme ---------- */
static void satir_ekle(Pen *p, GFileInfo *bi) {
  GFile *f = g_file_get_child(p->dizin, g_file_info_get_name(bi));
  /* çöp kutusunda dosyalar kendi adresleriyle gelir */
  const char *hedef = g_file_info_get_attribute_string(bi, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
  if (hedef && !cop_mu(p)) { g_object_unref(f); f = g_file_new_for_uri(hedef); }
  const char *ad = g_file_info_get_display_name(bi);
  int klasor = g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY || g_file_info_get_file_type(bi) == G_FILE_TYPE_MOUNTABLE;
  const char *tur = g_file_info_get_content_type(bi);
  char *kat = g_utf8_casefold(ad, -1); char *anahtar = g_utf8_collate_key_for_filename(kat, -1); g_free(kat);
  GIcon *ic = g_file_info_get_icon(bi);
  GdkPixbuf *buyuk = simge_yukle(ic, 48, klasor), *kucuk = simge_yukle(ic, 16, klasor);
  guint64 boy = g_file_info_get_size(bi);
  GDateTime *dt = g_file_info_get_modification_date_time(bi);
  gint64 zaman = dt ? g_date_time_to_unix(dt) : 0; if (dt) g_date_time_unref(dt);
  char *bm = klasor ? g_strdup("") : boyut_metni(boy), *zm = zaman_metni(zaman);
  char *tm = klasor ? g_strdup(T("Dosya klasörü", "File folder")) : g_content_type_get_description(tur ? tur : "application/octet-stream");
  GtkTreeIter it;
  gtk_list_store_insert_with_values(p->depo, &it, -1, C_DOSYA, f, C_AD, ad, C_ANAHTAR, anahtar, C_SIMGE, buyuk, C_KUCUK, kucuk, C_KLASOR, klasor,
                                    C_BOYUT, (gint64)boy, C_BOYUT_M, bm, C_ZAMAN, zaman, C_ZAMAN_M, zm, C_TUR, tur, C_TUR_M, tm,
                                    C_GIZLI, g_file_info_get_is_hidden(bi) || g_file_info_get_is_backup(bi), -1);
  g_object_unref(f); g_free(anahtar); g_free(bm); g_free(zm); g_free(tm);
  if (buyuk) g_object_unref(buyuk);
  if (kucuk) g_object_unref(kucuk);
}

/* resim önizlemeleri: arka planda */
typedef struct { Pen *p; GFile *dizin; GPtrArray *dosyalar; GPtrArray *resimler; } Onizleme;
static void onizleme_is(GTask *t, gpointer k, gpointer v, GCancellable *c) {
  (void)k; (void)c;
  Onizleme *o = v;
  o->resimler = g_ptr_array_new();
  for (unsigned i = 0; i < o->dosyalar->len; i++) {
    if (g_cancellable_is_cancelled(c)) break;
    char *yol = g_file_get_path(o->dosyalar->pdata[i]);
    GdkPixbuf *pb = yol ? gdk_pixbuf_new_from_file_at_scale(yol, 48, 48, TRUE, NULL) : NULL;
    g_ptr_array_add(o->resimler, pb);
    g_free(yol);
  }
  g_task_return_boolean(t, TRUE);
}
static void onizleme_bitti(GObject *k, GAsyncResult *r, gpointer v) {
  (void)k;
  Onizleme *o = v;
  if (g_task_propagate_boolean(G_TASK(r), NULL) && o->p->dizin && g_file_equal(o->p->dizin, o->dizin)) {
    GtkTreeIter it; gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(o->p->depo), &it);
    while (ok) {
      GFile *f; gtk_tree_model_get(GTK_TREE_MODEL(o->p->depo), &it, C_DOSYA, &f, -1);
      for (unsigned i = 0; i < o->resimler->len; i++)
        if (o->resimler->pdata[i] && g_file_equal(f, o->dosyalar->pdata[i])) gtk_list_store_set(o->p->depo, &it, C_SIMGE, o->resimler->pdata[i], -1);
      g_object_unref(f);
      ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(o->p->depo), &it);
    }
  }
  for (unsigned i = 0; o->resimler && i < o->resimler->len; i++) if (o->resimler->pdata[i]) g_object_unref(o->resimler->pdata[i]);
  if (o->resimler) g_ptr_array_free(o->resimler, TRUE);
  g_ptr_array_free(o->dosyalar, TRUE); g_object_unref(o->dizin); g_free(o);
}
static void onizlemeleri_baslat(Pen *p) {
  Onizleme *o = g_new0(Onizleme, 1); o->p = p; o->dizin = g_object_ref(p->dizin);
  o->dosyalar = g_ptr_array_new_with_free_func(g_object_unref);
  GtkTreeIter it; gboolean ok = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(p->depo), &it);
  while (ok) {
    char *tur; GFile *f; gint64 b;
    gtk_tree_model_get(GTK_TREE_MODEL(p->depo), &it, C_TUR, &tur, C_DOSYA, &f, C_BOYUT, &b, -1);
    if (tur && g_str_has_prefix(tur, "image/") && b < 25 * 1024 * 1024 && g_file_is_native(f)) g_ptr_array_add(o->dosyalar, f); else g_object_unref(f);
    g_free(tur);
    ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(p->depo), &it);
  }
  if (!o->dosyalar->len) { g_ptr_array_free(o->dosyalar, TRUE); g_object_unref(o->dizin); g_free(o); return; }
  GTask *t = g_task_new(NULL, p->yukleme, onizleme_bitti, o);
  g_task_set_task_data(t, o, NULL);
  g_task_run_in_thread(t, onizleme_is);
  g_object_unref(t);
}

static const char *NITELIKLER = "standard::*,time::modified,access::can-execute,trash::orig-path";

static void sonraki(GObject *k, GAsyncResult *r, gpointer v) {
  Pen *p = v;
  GError *e = NULL;
  GList *l = g_file_enumerator_next_files_finish(G_FILE_ENUMERATOR(k), r, &e);
  if (e) { g_error_free(e); g_object_unref(k); return; }
  if (!l) { g_object_unref(k); durum_yaz(p); onizlemeleri_baslat(p); return; }
  for (GList *i = l; i; i = i->next) satir_ekle(p, i->data);
  g_list_free_full(l, g_object_unref);
  g_file_enumerator_next_files_async(G_FILE_ENUMERATOR(k), 200, G_PRIORITY_DEFAULT, p->yukleme, sonraki, p);
}
static void numaralandi(GObject *k, GAsyncResult *r, gpointer v) {
  Pen *p = v;
  GError *e = NULL;
  GFileEnumerator *en = g_file_enumerate_children_finish(G_FILE(k), r, &e);
  if (!en) {
    if (!g_error_matches(e, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
      char *m = g_strdup_printf(T("Klasör açılamadı: %s", "Could not open folder: %s"), e->message);
      gtk_label_set_text(GTK_LABEL(p->durum), m); g_free(m);
    }
    g_error_free(e); return;
  }
  g_file_enumerator_next_files_async(en, 200, G_PRIORITY_DEFAULT, p->yukleme, sonraki, p);
}

static gboolean yenile_gecikmeli(gpointer v) { Pen *p = v; p->yenile_id = 0; yenile(p); return G_SOURCE_REMOVE; }
static void degisti(GFileMonitor *m, GFile *a, GFile *b, GFileMonitorEvent e, gpointer v) {
  (void)m; (void)a; (void)b; (void)e;
  Pen *p = v;
  if (p->yenile_id) g_source_remove(p->yenile_id);
  p->yenile_id = g_timeout_add(300, yenile_gecikmeli, p);
}

static void yenile(Pen *p) {
  /* seçimi ad üzerinden koru */
  GList *sec = secili_dosyalar(p);
  if (p->yukleme) { g_cancellable_cancel(p->yukleme); g_object_unref(p->yukleme); }
  p->yukleme = g_cancellable_new();
  gtk_list_store_clear(p->depo);
  g_file_enumerate_children_async(p->dizin, NITELIKLER, G_FILE_QUERY_INFO_NONE, G_PRIORITY_DEFAULT, p->yukleme, numaralandi, p);
  g_list_free_full(sec, g_object_unref);
}

/* ---------- yol çubuğu (Windows gibi parça parça) ---------- */
static void parca_tik(GtkButton *b, gpointer v) { Pen *p = v; GFile *f = g_object_get_data(G_OBJECT(b), "dosya"); git(p, f, 1); }
static void yol_cubugu(Pen *p) {
  GList *c = gtk_container_get_children(GTK_CONTAINER(p->yol_kutu));
  for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(c);
  GList *zincir = NULL;
  for (GFile *f = g_object_ref(p->dizin); f; ) {
    zincir = g_list_prepend(zincir, f);
    if (g_file_equal(f, g_file_new_for_path(g_get_home_dir()))) break;
    f = g_file_get_parent(f);
  }
  for (GList *l = zincir; l; l = l->next) {
    GFile *f = l->data;
    char *ad;
    char *yol = g_file_get_path(f);
    if (yol && !strcmp(yol, g_get_home_dir())) ad = g_strdup(T("Ev", "Home"));
    else if (yol && !strcmp(yol, "/")) ad = g_strdup(T("Bilgisayar", "Computer"));
    else if (cop_mu(p) && !g_file_get_parent(f)) ad = g_strdup(T("Çöp Kutusu", "Trash"));
    else { GFileInfo *bi = g_file_query_info(f, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL); ad = bi ? g_strdup(g_file_info_get_display_name(bi)) : g_file_get_basename(f); if (bi) g_object_unref(bi); }
    g_free(yol);
    GtkWidget *b = gtk_button_new_with_label(ad); g_free(ad);
    gtk_button_set_relief(GTK_BUTTON(b), GTK_RELIEF_NONE);
    g_object_set_data_full(G_OBJECT(b), "dosya", g_object_ref(f), g_object_unref);
    g_signal_connect(b, "clicked", G_CALLBACK(parca_tik), p);
    if (l != zincir) gtk_box_pack_start(GTK_BOX(p->yol_kutu), gtk_label_new("›"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p->yol_kutu), b, FALSE, FALSE, 0);
  }
  g_list_free_full(zincir, g_object_unref);
  gtk_widget_show_all(p->yol_kutu);
  char *ad = g_file_get_parse_name(p->dizin);
  gtk_entry_set_text(GTK_ENTRY(p->yol_giris), ad);
  char *bas = g_path_get_basename(ad);
  gtk_window_set_title(GTK_WINDOW(p->pencere), cop_mu(p) ? T("Çöp Kutusu", "Trash") : !strcmp(ad, g_get_home_dir()) ? T("Ev", "Home") : bas);
  g_free(bas); g_free(ad);
}

static void dugmeleri_ayarla(Pen *p) {
  gtk_widget_set_sensitive(p->geri, p->geri_l != NULL);
  gtk_widget_set_sensitive(p->ileri, p->ileri_l != NULL);
  GFile *u = g_file_get_parent(p->dizin);
  gtk_widget_set_sensitive(p->yukari, u != NULL);
  if (u) g_object_unref(u);
}

static void git(Pen *p, GFile *f, int gecmis) {
  if (!f) return;
  if (p->dizin && g_file_equal(p->dizin, f)) { yenile(p); return; }
  if (p->dizin) {
    if (gecmis == 1) { p->geri_l = g_list_prepend(p->geri_l, p->dizin); g_list_free_full(p->ileri_l, g_object_unref); p->ileri_l = NULL; }
    else if (gecmis == 0) g_object_unref(p->dizin);
    else if (gecmis == 2) p->ileri_l = g_list_prepend(p->ileri_l, p->dizin);   /* geri gidildi */
    else if (gecmis == 3) p->geri_l = g_list_prepend(p->geri_l, p->dizin);    /* ileri gidildi */
  }
  p->dizin = g_object_ref(f);
  if (p->izleyici) { g_file_monitor_cancel(p->izleyici); g_object_unref(p->izleyici); }
  p->izleyici = g_file_monitor_directory(f, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
  if (p->izleyici) g_signal_connect(p->izleyici, "changed", G_CALLBACK(degisti), p);
  gtk_entry_set_text(GTK_ENTRY(p->arama), "");
  gtk_places_sidebar_set_location(GTK_PLACES_SIDEBAR(p->yan), f);
  yol_cubugu(p);
  dugmeleri_ayarla(p);
  yenile(p);
}

static void geri_tik(GtkButton *b, gpointer v) {
  (void)b; Pen *p = v; if (!p->geri_l) return;
  GFile *f = p->geri_l->data; p->geri_l = g_list_delete_link(p->geri_l, p->geri_l);
  git(p, f, 2); g_object_unref(f);
}
static void ileri_tik(GtkButton *b, gpointer v) {
  (void)b; Pen *p = v; if (!p->ileri_l) return;
  GFile *f = p->ileri_l->data; p->ileri_l = g_list_delete_link(p->ileri_l, p->ileri_l);
  git(p, f, 3); g_object_unref(f);
}
static void yukari_tik(GtkButton *b, gpointer v) { (void)b; Pen *p = v; GFile *u = g_file_get_parent(p->dizin); if (u) { git(p, u, 1); g_object_unref(u); } }

/* ---------- açma ---------- */
static void dosya_ac(Pen *p, GFile *f, int klasor) {
  if (klasor) { git(p, f, 1); return; }
  GError *e = NULL;
  char *yol = g_file_get_path(f);
  if (yol && g_str_has_suffix(yol, ".desktop") && access(yol, X_OK) == 0) {
    GDesktopAppInfo *a = g_desktop_app_info_new_from_filename(yol);
    if (a) { g_app_info_launch(G_APP_INFO(a), NULL, NULL, &e); g_object_unref(a); g_free(yol); if (e) { hata(p, e->message); g_error_free(e); } return; }
  }
  g_free(yol);
  char *uri = g_file_get_uri(f);
  if (!g_app_info_launch_default_for_uri(uri, NULL, &e)) {
    g_clear_error(&e);
    GtkWidget *d = gtk_app_chooser_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, f);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
      GAppInfo *a = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(d));
      if (a) { GList *l = g_list_append(NULL, f); g_app_info_launch(a, l, NULL, NULL); g_list_free(l); g_object_unref(a); }
    }
    gtk_widget_destroy(d);
  }
  g_free(uri);
}
static void secilileri_ac(Pen *p) {
  GList *y = secili_yollar(p);
  for (GList *l = y; l; l = l->next) {
    GtkTreeIter it; GFile *f; gboolean k;
    if (!gtk_tree_model_get_iter(p->sirali, &it, l->data)) continue;
    gtk_tree_model_get(p->sirali, &it, C_DOSYA, &f, C_KLASOR, &k, -1);
    dosya_ac(p, f, k); g_object_unref(f);
    if (k) break;   /* klasöre gidildiyse model değişti */
  }
  g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free);
}
static void ikon_etkin(GtkIconView *v, GtkTreePath *y, gpointer d) { (void)v; (void)y; secilileri_ac(d); }
static void liste_etkin(GtkTreeView *v, GtkTreePath *y, GtkTreeViewColumn *c, gpointer d) { (void)v; (void)y; (void)c; secilileri_ac(d); }

/* ---------- kopyalama / taşıma (ilerlemeli) ---------- */
typedef struct { Pen *p; GList *kaynak; GFile *hedef; int tasi; guint64 toplam, biten, dosya_biten; int adet, i; char *su_an; GMutex kilit; char *hata; } Is;

static guint64 boyut_hesapla(GFile *f, int *adet, GCancellable *c) {
  GFileInfo *bi = g_file_query_info(f, "standard::type,standard::size", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, c, NULL);
  if (!bi) return 0;
  guint64 b = 0;
  if (g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY) {
    GFileEnumerator *en = g_file_enumerate_children(f, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, c, NULL);
    GFileInfo *ci;
    while (en && (ci = g_file_enumerator_next_file(en, c, NULL))) { GFile *cf = g_file_get_child(f, g_file_info_get_name(ci)); b += boyut_hesapla(cf, adet, c); g_object_unref(cf); g_object_unref(ci); }
    if (en) g_object_unref(en);
  } else { b = g_file_info_get_size(bi); (*adet)++; }
  g_object_unref(bi);
  return b;
}
static void ilerleme_cb(goffset su, goffset top, gpointer v) { (void)top; Is *is = v; g_mutex_lock(&is->kilit); is->dosya_biten = su; g_mutex_unlock(&is->kilit); }

static int kopyala_ozyineli(Is *is, GFile *k, GFile *h, GCancellable *c, GError **e) {
  GFileInfo *bi = g_file_query_info(k, "standard::type,standard::size", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, c, e);
  if (!bi) return 0;
  int ok = 1;
  if (g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY) {
    if (!g_file_make_directory(h, c, e) && !g_error_matches(*e, G_IO_ERROR, G_IO_ERROR_EXISTS)) ok = 0;
    g_clear_error(e);
    GFileEnumerator *en = ok ? g_file_enumerate_children(k, "standard::name", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, c, e) : NULL;
    GFileInfo *ci;
    while (ok && en && (ci = g_file_enumerator_next_file(en, c, NULL))) {
      GFile *kc = g_file_get_child(k, g_file_info_get_name(ci)), *hc = g_file_get_child(h, g_file_info_get_name(ci));
      ok = kopyala_ozyineli(is, kc, hc, c, e);
      g_object_unref(kc); g_object_unref(hc); g_object_unref(ci);
    }
    if (en) g_object_unref(en);
  } else {
    g_mutex_lock(&is->kilit); g_free(is->su_an); is->su_an = g_file_get_basename(k); is->i++; g_mutex_unlock(&is->kilit);
    ok = g_file_copy(k, h, G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA, c, ilerleme_cb, is, e);
    g_mutex_lock(&is->kilit); is->biten += g_file_info_get_size(bi); is->dosya_biten = 0; g_mutex_unlock(&is->kilit);
  }
  g_object_unref(bi);
  return ok;
}
static void sil_ozyineli(GFile *f, GCancellable *c) {
  GFileEnumerator *en = g_file_enumerate_children(f, "standard::name,standard::type", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, c, NULL);
  GFileInfo *ci;
  while (en && (ci = g_file_enumerator_next_file(en, c, NULL))) { GFile *cf = g_file_get_child(f, g_file_info_get_name(ci)); sil_ozyineli(cf, c); g_object_unref(cf); g_object_unref(ci); }
  if (en) g_object_unref(en);
  g_file_delete(f, c, NULL);
}

/* hedefte aynı ad varsa "ad (2)" */
static GFile *bos_hedef(GFile *dizin, const char *ad, int klasor) {
  GFile *f = g_file_get_child(dizin, ad);
  if (!g_file_query_exists(f, NULL)) return f;
  g_object_unref(f);
  const char *nokta = klasor ? NULL : strrchr(ad, '.');
  char *taban = nokta && nokta != ad ? g_strndup(ad, nokta - ad) : g_strdup(ad);
  for (int i = 2; i < 10000; i++) {
    char *y = g_strdup_printf("%s (%d)%s", taban, i, nokta && nokta != ad ? nokta : "");
    f = g_file_get_child(dizin, y); g_free(y);
    if (!g_file_query_exists(f, NULL)) break;
    g_object_unref(f); f = NULL;
  }
  g_free(taban);
  return f;
}

static void is_calis(GTask *t, gpointer k, gpointer v, GCancellable *c) {
  (void)k;
  Is *is = v;
  for (GList *l = is->kaynak; l; l = l->next) is->toplam += boyut_hesapla(l->data, &is->adet, c);
  for (GList *l = is->kaynak; l && !g_cancellable_is_cancelled(c); l = l->next) {
    GFile *kf = l->data;
    char *ad = g_file_get_basename(kf);
    GFileInfo *bi = g_file_query_info(kf, "standard::type", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
    int klasor = bi && g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY; if (bi) g_object_unref(bi);
    GFile *ust = g_file_get_parent(kf);
    int ayni = ust && g_file_equal(ust, is->hedef); if (ust) g_object_unref(ust);
    if (is->tasi && ayni) { g_free(ad); continue; }
    /* klasörü kendi içine kopyalama */
    if (g_file_has_prefix(is->hedef, kf) || g_file_equal(is->hedef, kf)) { g_free(is->hata); is->hata = g_strdup(T("Bir klasör kendi içine kopyalanamaz.", "A folder cannot be copied into itself.")); g_free(ad); continue; }
    GFile *hf;
    if (!is->tasi && ayni) { char *kopya = g_strdup_printf("%s - %s", ad, T("Kopya", "Copy")); hf = bos_hedef(is->hedef, kopya, klasor); g_free(kopya); }
    else hf = bos_hedef(is->hedef, ad, klasor);
    GError *e = NULL; int ok = 0;
    if (is->tasi) ok = g_file_move(kf, hf, G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_NO_FALLBACK_FOR_MOVE, c, NULL, NULL, &e);
    if (!ok) {
      g_clear_error(&e);
      ok = kopyala_ozyineli(is, kf, hf, c, &e);
      if (ok && is->tasi) sil_ozyineli(kf, c);
    } else { g_mutex_lock(&is->kilit); is->i++; g_mutex_unlock(&is->kilit); }
    if (!ok && e && !g_error_matches(e, G_IO_ERROR, G_IO_ERROR_CANCELLED)) { g_free(is->hata); is->hata = g_strdup_printf("%s: %s", ad, e->message); }
    g_clear_error(&e);
    g_object_unref(hf); g_free(ad);
  }
  g_task_return_boolean(t, TRUE);
}
static gboolean is_goster(gpointer v) {
  Is *is = v;
  if (!is->p->is_iptal) return G_SOURCE_REMOVE;
  g_mutex_lock(&is->kilit);
  double oran = is->toplam ? (double)(is->biten + is->dosya_biten) / is->toplam : 0;
  char *m = g_strdup_printf(is->tasi ? T("Taşınıyor: %s  (%d/%d)", "Moving: %s  (%d/%d)") : T("Kopyalanıyor: %s  (%d/%d)", "Copying: %s  (%d/%d)"),
                            is->su_an ? is->su_an : "…", MIN(is->i, MAX(is->adet, 1)), MAX(is->adet, 1));
  g_mutex_unlock(&is->kilit);
  gtk_label_set_text(GTK_LABEL(is->p->is_yazi), m); g_free(m);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(is->p->is_ilerleme), MIN(oran, 1.0));
  return G_SOURCE_CONTINUE;
}
static void is_bitti(GObject *k, GAsyncResult *r, gpointer v) {
  (void)k; (void)r;
  Is *is = v; Pen *p = is->p;
  g_clear_object(&p->is_iptal);
  gtk_revealer_set_reveal_child(GTK_REVEALER(p->is_cubugu), FALSE);
  if (is->hata) hata(p, is->hata);
  g_list_free_full(is->kaynak, g_object_unref); g_object_unref(is->hedef); g_free(is->su_an); g_free(is->hata);
  g_mutex_clear(&is->kilit); g_free(is);
}
static void dosyalari_aktar(Pen *p, GList *kaynak, GFile *hedef, int tasi) {
  if (!kaynak) return;
  if (p->is_iptal) { hata(p, T("Önceki işlem bitmeden yenisi başlatılamaz.", "Wait for the current operation to finish.")); g_list_free_full(kaynak, g_object_unref); return; }
  Is *is = g_new0(Is, 1); is->p = p; is->kaynak = kaynak; is->hedef = g_object_ref(hedef); is->tasi = tasi; g_mutex_init(&is->kilit);
  p->is_iptal = g_cancellable_new();
  gtk_revealer_set_reveal_child(GTK_REVEALER(p->is_cubugu), TRUE);
  GTask *t = g_task_new(NULL, p->is_iptal, is_bitti, is);
  g_task_set_task_data(t, is, NULL);
  g_task_run_in_thread(t, is_calis);
  g_object_unref(t);
  g_timeout_add(150, is_goster, is);
}
static void is_iptal_tik(GtkButton *b, gpointer v) { (void)b; Pen *p = v; if (p->is_iptal) g_cancellable_cancel(p->is_iptal); }

/* ---------- pano ---------- */
static char **pano_uri; static int pano_kes;
static void pano_ver(GtkClipboard *c, GtkSelectionData *sd, guint bilgi, gpointer v) {
  (void)c; (void)v;
  GString *s = g_string_new(NULL);
  if (bilgi == 0) {
    g_string_append(s, pano_kes ? "cut" : "copy");
    for (int i = 0; pano_uri && pano_uri[i]; i++) g_string_append_printf(s, "\n%s", pano_uri[i]);
    gtk_selection_data_set(sd, gdk_atom_intern(KOPYALANAN, FALSE), 8, (guchar *)s->str, s->len);
  } else if (bilgi == 1) gtk_selection_data_set_uris(sd, pano_uri);
  else {
    for (int i = 0; pano_uri && pano_uri[i]; i++) { char *y = g_filename_from_uri(pano_uri[i], NULL, NULL); g_string_append_printf(s, "%s%s", i ? "\n" : "", y ? y : pano_uri[i]); g_free(y); }
    gtk_selection_data_set_text(sd, s->str, -1);
  }
  g_string_free(s, TRUE);
}
static void pano_bos(GtkClipboard *c, gpointer v) { (void)c; (void)v; }
static void panoya(Pen *p, int kes) {
  GList *d = secili_dosyalar(p);
  if (!d) return;
  GPtrArray *u = g_ptr_array_new();
  for (GList *l = d; l; l = l->next) g_ptr_array_add(u, g_file_get_uri(l->data));
  g_ptr_array_add(u, NULL);
  g_list_free_full(d, g_object_unref);
  g_strfreev(pano_uri); pano_uri = (char **)g_ptr_array_free(u, FALSE); pano_kes = kes;
  GtkTargetEntry h[] = { { (char *)KOPYALANAN, 0, 0 }, { "text/uri-list", 0, 1 }, { "UTF8_STRING", 0, 2 }, { "text/plain", 0, 2 } };
  gtk_clipboard_set_with_data(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), h, G_N_ELEMENTS(h), pano_ver, pano_bos, NULL);
}
static void yapistir(Pen *p, GFile *hedef) {
  GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GtkSelectionData *sd = gtk_clipboard_wait_for_contents(c, gdk_atom_intern(KOPYALANAN, FALSE));
  GList *l = NULL; int kes = 0;
  if (sd && gtk_selection_data_get_length(sd) > 0) {
    char *s = g_strndup((const char *)gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd));
    char **sat = g_strsplit(s, "\n", -1);
    kes = sat[0] && !strcmp(sat[0], "cut");
    for (int i = 1; sat[0] && sat[i]; i++) if (*sat[i]) l = g_list_append(l, g_file_new_for_uri(sat[i]));
    g_strfreev(sat); g_free(s);
    if (kes) gtk_clipboard_clear(c);
  } else {
    char **u = gtk_clipboard_wait_for_uris(c);
    for (int i = 0; u && u[i]; i++) l = g_list_append(l, g_file_new_for_uri(u[i]));
    g_strfreev(u);
  }
  if (sd) gtk_selection_data_free(sd);
  dosyalari_aktar(p, l, hedef ? hedef : p->dizin, kes);
}
static gboolean panoda_dosya(void) {
  GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  return gtk_clipboard_wait_is_target_available(c, gdk_atom_intern(KOPYALANAN, FALSE)) || gtk_clipboard_wait_is_uris_available(c);
}

/* ---------- silme, yeniden adlandırma, yeni ---------- */
static void cope_at(Pen *p, int kalici) {
  GList *d = secili_dosyalar(p);
  if (!d) return;
  if (kalici || cop_mu(p)) {
    int n = g_list_length(d);
    char *m = n == 1 ? g_strdup_printf(T("\"%s\" kalıcı olarak silinsin mi? Bu geri alınamaz.", "Permanently delete \"%s\"? This cannot be undone."), g_file_get_basename(d->data))
                     : g_strdup_printf(T("%d öğe kalıcı olarak silinsin mi? Bu geri alınamaz.", "Permanently delete %d items? This cannot be undone."), n);
    GtkWidget *dl = gtk_message_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s", m);
    g_free(m);
    gtk_dialog_add_buttons(GTK_DIALOG(dl), T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("Sil", "Delete"), GTK_RESPONSE_OK, NULL);
    int r = gtk_dialog_run(GTK_DIALOG(dl)); gtk_widget_destroy(dl);
    if (r == GTK_RESPONSE_OK) for (GList *l = d; l; l = l->next) { if (!g_file_delete(l->data, NULL, NULL)) sil_ozyineli(l->data, NULL); }
  } else {
    for (GList *l = d; l; l = l->next) {
      GError *e = NULL;
      if (!g_file_trash(l->data, NULL, &e)) {
        char *m = g_strdup_printf(T("Çöp kutusuna taşınamadı (%s). Kalıcı olarak silmek için Shift+Del.", "Could not move to the trash (%s). Use Shift+Del to delete permanently."), e->message);
        hata(p, m); g_free(m); g_error_free(e); break;
      }
    }
  }
  g_list_free_full(d, g_object_unref);
}

static char *ad_sor(Pen *p, const char *baslik, const char *eski) {
  GtkWidget *d = gtk_dialog_new_with_buttons(baslik, GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("Tamam", "OK"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
  GtkWidget *g = gtk_entry_new(); gtk_entry_set_text(GTK_ENTRY(g), eski); gtk_entry_set_activates_default(GTK_ENTRY(g), TRUE);
  gtk_widget_set_size_request(g, 340, -1);
  GtkWidget *k = gtk_dialog_get_content_area(GTK_DIALOG(d)); gtk_container_set_border_width(GTK_CONTAINER(k), 12);
  gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
  gtk_widget_show_all(d);
  const char *nokta = strrchr(eski, '.');
  gtk_editable_select_region(GTK_EDITABLE(g), 0, nokta && nokta != eski ? (int)g_utf8_pointer_to_offset(eski, nokta) : -1);
  char *s = NULL;
  if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) s = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(g))));
  gtk_widget_destroy(d);
  if (s && (!*s || strchr(s, '/'))) { g_free(s); s = NULL; }
  return s;
}
static void adlandir_dosya(Pen *p, GFile *f) {
  char *eski = g_file_get_basename(f), *yeni = ad_sor(p, T("Yeniden adlandır", "Rename"), eski);
  if (yeni && strcmp(eski, yeni)) { GError *e = NULL; GFile *n = g_file_set_display_name(f, yeni, NULL, &e); if (n) g_object_unref(n); else { hata(p, e->message); g_error_free(e); } }
  g_free(eski); g_free(yeni);
}
static void adlandir(Pen *p) { GList *d = secili_dosyalar(p); if (d) adlandir_dosya(p, d->data); g_list_free_full(d, g_object_unref); }

static GFile *yeni_ad(Pen *p, const char *taban, const char *uz) {
  for (int i = 1; i < 1000; i++) {
    char *ad = i == 1 ? g_strdup_printf("%s%s", taban, uz) : g_strdup_printf("%s (%d)%s", taban, i, uz);
    GFile *f = g_file_get_child(p->dizin, ad); g_free(ad);
    if (!g_file_query_exists(f, NULL)) return f;
    g_object_unref(f);
  }
  return NULL;
}
static void yeni_klasor(Pen *p) {
  GFile *f = yeni_ad(p, T("Yeni klasör", "New folder"), ""); GError *e = NULL;
  if (f && g_file_make_directory(f, NULL, &e)) adlandir_dosya(p, f); else if (e) { hata(p, e->message); g_error_free(e); }
  if (f) g_object_unref(f);
}
static void yeni_metin(Pen *p) {
  GFile *f = yeni_ad(p, T("Yeni metin belgesi", "New text document"), ".txt"); GError *e = NULL;
  GFileOutputStream *s = f ? g_file_create(f, G_FILE_CREATE_NONE, NULL, &e) : NULL;
  if (s) { g_object_unref(s); adlandir_dosya(p, f); } else if (e) { hata(p, e->message); g_error_free(e); }
  if (f) g_object_unref(f);
}
static void terminalde_ac(GFile *f) {
  char *yol = g_file_get_path(f);
  if (!yol) return;
  const char *argv[] = { "aether-terminal", "--working-directory", yol, NULL };
  g_spawn_async(yol, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  g_free(yol);
}

/* çöp kutusu: geri yükle ve boşalt */
static void geri_yukle(Pen *p) {
  GList *d = secili_dosyalar(p);
  for (GList *l = d; l; l = l->next) {
    GFileInfo *bi = g_file_query_info(l->data, "trash::orig-path", 0, NULL, NULL);
    const char *asil = bi ? g_file_info_get_attribute_byte_string(bi, "trash::orig-path") : NULL;
    if (asil) {
      GFile *h = g_file_new_for_path(asil); GError *e = NULL;
      if (!g_file_move(l->data, h, G_FILE_COPY_NONE, NULL, NULL, NULL, &e)) { hata(p, e->message); g_error_free(e); }
      g_object_unref(h);
    }
    if (bi) g_object_unref(bi);
  }
  g_list_free_full(d, g_object_unref);
}
static void copu_bosalt(Pen *p) {
  GtkWidget *dl = gtk_message_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_NONE, "%s",
                                         T("Çöp kutusundaki her şey kalıcı olarak silinsin mi?", "Permanently delete everything in the trash?"));
  gtk_dialog_add_buttons(GTK_DIALOG(dl), T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("Boşalt", "Empty"), GTK_RESPONSE_OK, NULL);
  int r = gtk_dialog_run(GTK_DIALOG(dl)); gtk_widget_destroy(dl);
  if (r != GTK_RESPONSE_OK) return;
  GFile *cop = g_file_new_for_uri("trash:///");
  GFileEnumerator *en = g_file_enumerate_children(cop, "standard::name", 0, NULL, NULL);
  GFileInfo *bi;
  while (en && (bi = g_file_enumerator_next_file(en, NULL, NULL))) { GFile *f = g_file_get_child(cop, g_file_info_get_name(bi)); g_file_delete(f, NULL, NULL); g_object_unref(f); g_object_unref(bi); }
  if (en) g_object_unref(en);
  g_object_unref(cop);
}

/* ---------- özellikler ---------- */
typedef struct { GtkWidget *etiket; GList *dosyalar; guint64 boyut; int adet; } Hesap;
static void hesap_is(GTask *t, gpointer k, gpointer v, GCancellable *c) {
  (void)k; Hesap *h = v;
  for (GList *l = h->dosyalar; l; l = l->next) h->boyut += boyut_hesapla(l->data, &h->adet, c);
  g_task_return_boolean(t, TRUE);
}
static void hesap_bitti(GObject *k, GAsyncResult *r, gpointer v) {
  (void)k; (void)r; Hesap *h = v;
  if (h->etiket) {
    char *b = boyut_metni(h->boyut);
    char *m = g_strdup_printf(T("%s  (%d dosya)", "%s  (%d files)"), b, h->adet);
    gtk_label_set_text(GTK_LABEL(h->etiket), m); g_free(m); g_free(b);
    g_object_remove_weak_pointer(G_OBJECT(h->etiket), (gpointer *)&h->etiket);
  }
  g_list_free_full(h->dosyalar, g_object_unref); g_free(h);
}
static void ozellikler(Pen *p, GList *d) {
  int tek = d && !d->next;
  GFile *f = d ? d->data : p->dizin;
  GFileInfo *bi = g_file_query_info(f, "standard::*,time::*,unix::mode,owner::user", 0, NULL, NULL);
  GtkWidget *dl = gtk_dialog_new_with_buttons(T("Özellikler", "Properties"), GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, T("Kapat", "Close"), GTK_RESPONSE_CLOSE, NULL);
  GtkWidget *g = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 16);
  gtk_container_set_border_width(GTK_CONTAINER(g), 16);
  int s = 0;
#define SATIR(ad, deger) do { GtkWidget *a_ = gtk_label_new(ad), *b_ = gtk_label_new(deger); gtk_label_set_xalign(GTK_LABEL(a_), 1); gtk_label_set_xalign(GTK_LABEL(b_), 0); \
    gtk_label_set_selectable(GTK_LABEL(b_), TRUE); gtk_label_set_line_wrap(GTK_LABEL(b_), TRUE); gtk_label_set_max_width_chars(GTK_LABEL(b_), 44); \
    gtk_style_context_add_class(gtk_widget_get_style_context(a_), "ae-alt"); gtk_grid_attach(GTK_GRID(g), a_, 0, s, 1, 1); gtk_grid_attach(GTK_GRID(g), b_, 1, s++, 1, 1); } while (0)
  GtkWidget *boyut_et = NULL;
  if (tek && bi) {
    GdkPixbuf *pb = simge_yukle(g_file_info_get_icon(bi), 64, g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY);
    if (pb) { gtk_grid_attach(GTK_GRID(g), gtk_image_new_from_pixbuf(pb), 0, s, 1, 1); g_object_unref(pb); }
    GtkWidget *ad = gtk_label_new(g_file_info_get_display_name(bi)); gtk_label_set_xalign(GTK_LABEL(ad), 0); gtk_label_set_selectable(GTK_LABEL(ad), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(ad), "ae-baslik");
    gtk_grid_attach(GTK_GRID(g), ad, 1, s++, 1, 1);
    int klasor = g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY;
    char *tur = klasor ? g_strdup(T("Dosya klasörü", "File folder")) : g_content_type_get_description(g_file_info_get_content_type(bi));
    SATIR(T("Tür:", "Type:"), tur); g_free(tur);
    GFile *u = g_file_get_parent(f); char *konum = u ? g_file_get_parse_name(u) : g_strdup("/"); if (u) g_object_unref(u);
    SATIR(T("Konum:", "Location:"), konum); g_free(konum);
    char *b = boyut_metni(g_file_info_get_size(bi));
    SATIR(T("Boyut:", "Size:"), klasor ? T("hesaplanıyor…", "calculating…") : b); g_free(b);
    boyut_et = klasor ? gtk_grid_get_child_at(GTK_GRID(g), 1, s - 1) : NULL;
    GDateTime *dt = g_file_info_get_modification_date_time(bi);
    if (dt) { char *z = zaman_metni(g_date_time_to_unix(dt)); SATIR(T("Değiştirilme:", "Modified:"), z); g_free(z); g_date_time_unref(dt); }
    const char *sahip = g_file_info_get_attribute_string(bi, "owner::user");
    if (sahip) SATIR(T("Sahibi:", "Owner:"), sahip);
  } else {
    char *m = g_strdup_printf(T("%d öğe", "%d items"), g_list_length(d));
    SATIR(T("Seçim:", "Selection:"), m); g_free(m);
    SATIR(T("Boyut:", "Size:"), T("hesaplanıyor…", "calculating…"));
    boyut_et = gtk_grid_get_child_at(GTK_GRID(g), 1, s - 1);
  }
#undef SATIR
  if (boyut_et) {
    Hesap *h = g_new0(Hesap, 1); h->etiket = boyut_et;
    g_object_add_weak_pointer(G_OBJECT(boyut_et), (gpointer *)&h->etiket);
    if (d) for (GList *l = d; l; l = l->next) h->dosyalar = g_list_append(h->dosyalar, g_object_ref(l->data));
    else h->dosyalar = g_list_append(NULL, g_object_ref(f));
    GTask *t = g_task_new(NULL, NULL, hesap_bitti, h); g_task_set_task_data(t, h, NULL); g_task_run_in_thread(t, hesap_is); g_object_unref(t);
  }
  gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dl))), g, TRUE, TRUE, 0);
  gtk_widget_show_all(dl);
  gtk_dialog_run(GTK_DIALOG(dl)); gtk_widget_destroy(dl);
  if (bi) g_object_unref(bi);
}

/* ---------- menüler ---------- */
typedef struct { Pen *p; GAppInfo *a; } Birlikte;
static void m_birlikte(GtkMenuItem *m, gpointer v) {
  (void)m; Birlikte *b = v;
  GList *d = secili_dosyalar(b->p);
  g_app_info_launch(b->a, d, NULL, NULL);
  g_list_free_full(d, g_object_unref);
}
static void birlikte_sil(gpointer v, GClosure *c) { (void)c; Birlikte *b = v; g_object_unref(b->a); g_free(b); }
static void m_baska(GtkMenuItem *m, gpointer v) {
  (void)m; Pen *p = v;
  GList *d = secili_dosyalar(p); if (!d) return;
  GtkWidget *dl = gtk_app_chooser_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, d->data);
  if (gtk_dialog_run(GTK_DIALOG(dl)) == GTK_RESPONSE_OK) {
    GAppInfo *a = gtk_app_chooser_get_app_info(GTK_APP_CHOOSER(dl));
    if (a) { g_app_info_launch(a, d, NULL, NULL); g_object_unref(a); }
  }
  gtk_widget_destroy(dl); g_list_free_full(d, g_object_unref);
}
static void m_ac(GtkMenuItem *m, gpointer v) { (void)m; secilileri_ac(v); }
static void m_yeni_pencere(GtkMenuItem *m, gpointer v) {
  (void)m; Pen *p = v;
  GList *y = secili_yollar(p);
  for (GList *l = y; l; l = l->next) { GtkTreeIter it; GFile *f; gboolean k; if (gtk_tree_model_get_iter(p->sirali, &it, l->data)) { gtk_tree_model_get(p->sirali, &it, C_DOSYA, &f, C_KLASOR, &k, -1); if (k) pencere_ac(f); g_object_unref(f); } }
  g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free);
}
static void m_kes(GtkMenuItem *m, gpointer v) { (void)m; panoya(v, 1); }
static void m_kopyala(GtkMenuItem *m, gpointer v) { (void)m; panoya(v, 0); }
static void m_yapistir(GtkMenuItem *m, gpointer v) { (void)m; yapistir(v, NULL); }
static void m_yapistir_icine(GtkMenuItem *m, gpointer v) { (void)m; Pen *p = v; GList *d = secili_dosyalar(p); if (d) yapistir(p, d->data); g_list_free_full(d, g_object_unref); }
static void m_sil(GtkMenuItem *m, gpointer v) { (void)m; cope_at(v, 0); }
static void m_kalici(GtkMenuItem *m, gpointer v) { (void)m; cope_at(v, 1); }
static void m_adlandir(GtkMenuItem *m, gpointer v) { (void)m; adlandir(v); }
static void m_ozellik(GtkMenuItem *m, gpointer v) { (void)m; Pen *p = v; GList *d = secili_dosyalar(p); ozellikler(p, d); g_list_free_full(d, g_object_unref); }
static void m_dizin_ozellik(GtkMenuItem *m, gpointer v) { (void)m; ozellikler(v, NULL); }
static void m_terminal(GtkMenuItem *m, gpointer v) { (void)m; Pen *p = v; GList *d = secili_dosyalar(p); terminalde_ac(d ? d->data : p->dizin); g_list_free_full(d, g_object_unref); }
static void m_dizin_terminal(GtkMenuItem *m, gpointer v) { (void)m; Pen *p = v; terminalde_ac(p->dizin); }
static void m_yenile(GtkMenuItem *m, gpointer v) { (void)m; yenile(v); }
static void m_yeni_klasor(GtkMenuItem *m, gpointer v) { (void)m; yeni_klasor(v); }
static void m_yeni_metin(GtkMenuItem *m, gpointer v) { (void)m; yeni_metin(v); }
static void m_geri_yukle(GtkMenuItem *m, gpointer v) { (void)m; geri_yukle(v); }
static void m_bosalt(GtkMenuItem *m, gpointer v) { (void)m; copu_bosalt(v); }
static void gorunum_ayarla(Pen *p, int liste);
static void m_gorunum(GtkCheckMenuItem *m, gpointer v) { if (gtk_check_menu_item_get_active(m)) { Pen *p = g_object_get_data(G_OBJECT(m), "pen"); gorunum_ayarla(p, GPOINTER_TO_INT(v)); } }
static void sirala_ayarla(Pen *p);
static void m_sira(GtkCheckMenuItem *m, gpointer v) { if (gtk_check_menu_item_get_active(m)) { Pen *p = g_object_get_data(G_OBJECT(m), "pen"); snprintf(p->sira, sizeof p->sira, "%s", (char *)v); ae_ayar_yaz("DOSYALAR_SIRA", v); sirala_ayarla(p); } }
static void m_gizli(GtkCheckMenuItem *m, gpointer v) { Pen *p = v; p->gizli = gtk_check_menu_item_get_active(m); gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(p->suzgec)); durum_yaz(p); }

static GtkWidget *oge(GtkWidget *menu, const char *ad, const char *kisa, GCallback cb, gpointer v) {
  GtkWidget *o = gtk_menu_item_new(), *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24), *l = gtk_label_new(ad);
  gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_box_pack_start(GTK_BOX(k), l, TRUE, TRUE, 0);
  if (kisa) { GtkWidget *s = gtk_label_new(kisa); gtk_style_context_add_class(gtk_widget_get_style_context(s), "ae-alt"); gtk_box_pack_end(GTK_BOX(k), s, FALSE, FALSE, 0); }
  gtk_container_add(GTK_CONTAINER(o), k);
  if (cb) g_signal_connect(o, "activate", cb, v);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), o);
  return o;
}
static void ayrac(GtkWidget *m) { gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new()); }
static GtkWidget *alt(GtkWidget *m, const char *ad) { GtkWidget *o = gtk_menu_item_new_with_label(ad), *a = gtk_menu_new(); gtk_menu_item_set_submenu(GTK_MENU_ITEM(o), a); gtk_menu_shell_append(GTK_MENU_SHELL(m), o); return a; }
static GSList *radyo(GtkWidget *m, GSList *g, const char *ad, int etkin, GCallback cb, gpointer v, Pen *p) {
  GtkWidget *o = gtk_radio_menu_item_new_with_label(g, ad);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(o), etkin);
  g_object_set_data(G_OBJECT(o), "pen", p);
  g_signal_connect(o, "toggled", cb, v);
  gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  return gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(o));
}
static gboolean menu_yok(gpointer m) { gtk_widget_destroy(m); return G_SOURCE_REMOVE; }
static void menu_kapandi(GtkMenuShell *m, gpointer v) { (void)v; g_idle_add(menu_yok, m); }
static void menu_goster(GtkWidget *m, GdkEvent *e) {
  gtk_widget_show_all(m);
  g_signal_connect(m, "deactivate", G_CALLBACK(menu_kapandi), NULL);
  gtk_menu_popup_at_pointer(GTK_MENU(m), e);
}

static void oge_menusu(Pen *p, GdkEvent *e) {
  GtkWidget *m = gtk_menu_new();
  GList *y = secili_yollar(p);
  int n = g_list_length(y), klasor = 0; char *tur = NULL;
  if (y) { GtkTreeIter it; if (gtk_tree_model_get_iter(p->sirali, &it, y->data)) gtk_tree_model_get(p->sirali, &it, C_KLASOR, &klasor, C_TUR, &tur, -1); }
  g_list_free_full(y, (GDestroyNotify)gtk_tree_path_free);
  if (cop_mu(p)) {
    oge(m, T("Geri yükle", "Restore"), NULL, G_CALLBACK(m_geri_yukle), p);
    ayrac(m);
    oge(m, T("Kalıcı olarak sil", "Delete permanently"), "Del", G_CALLBACK(m_kalici), p);
    menu_goster(m, e); g_free(tur); return;
  }
  GtkWidget *a = oge(m, T("Aç", "Open"), "Enter", G_CALLBACK(m_ac), p);
  gtk_style_context_add_class(gtk_widget_get_style_context(gtk_bin_get_child(GTK_BIN(a))), "kalin");
  if (klasor) oge(m, T("Yeni pencerede aç", "Open in new window"), NULL, G_CALLBACK(m_yeni_pencere), p);
  if (!klasor && tur) {
    GtkWidget *b = alt(m, T("Birlikte aç", "Open with"));
    GList *uyg_l = g_app_info_get_all_for_type(tur);
    for (GList *l = uyg_l; l; l = l->next) {
      Birlikte *bb = g_new0(Birlikte, 1); bb->p = p; bb->a = g_object_ref(l->data);
      GtkWidget *o = gtk_menu_item_new_with_label(g_app_info_get_display_name(l->data));
      g_signal_connect_data(o, "activate", G_CALLBACK(m_birlikte), bb, birlikte_sil, 0);
      gtk_menu_shell_append(GTK_MENU_SHELL(b), o);
    }
    g_list_free_full(uyg_l, g_object_unref);
    if (uyg_l) ayrac(b);
    oge(b, T("Başka bir uygulama seç…", "Choose another app…"), NULL, G_CALLBACK(m_baska), p);
  }
  if (klasor && n == 1) oge(m, T("Terminalde aç", "Open in Terminal"), NULL, G_CALLBACK(m_terminal), p);
  ayrac(m);
  oge(m, T("Kes", "Cut"), "Ctrl+X", G_CALLBACK(m_kes), p);
  oge(m, T("Kopyala", "Copy"), "Ctrl+C", G_CALLBACK(m_kopyala), p);
  if (klasor && n == 1) { GtkWidget *y2 = oge(m, T("Klasörün içine yapıştır", "Paste into folder"), NULL, G_CALLBACK(m_yapistir_icine), p); gtk_widget_set_sensitive(y2, panoda_dosya()); }
  ayrac(m);
  oge(m, T("Sil", "Delete"), "Del", G_CALLBACK(m_sil), p);
  GtkWidget *r = oge(m, T("Yeniden adlandır", "Rename"), "F2", G_CALLBACK(m_adlandir), p);
  gtk_widget_set_sensitive(r, n == 1);
  ayrac(m);
  oge(m, T("Özellikler", "Properties"), "Alt+Enter", G_CALLBACK(m_ozellik), p);
  g_free(tur);
  menu_goster(m, e);
}

static void arka_menusu(Pen *p, GdkEvent *e) {
  GtkWidget *m = gtk_menu_new();
  if (cop_mu(p)) { oge(m, T("Çöp kutusunu boşalt", "Empty trash"), NULL, G_CALLBACK(m_bosalt), p); menu_goster(m, e); return; }
  GtkWidget *g = alt(m, T("Görünüm", "View"));
  GSList *gr = NULL;
  gr = radyo(g, gr, T("Simgeler", "Icons"), !p->liste_mi, G_CALLBACK(m_gorunum), GINT_TO_POINTER(0), p);
  gr = radyo(g, gr, T("Ayrıntılar", "Details"), p->liste_mi, G_CALLBACK(m_gorunum), GINT_TO_POINTER(1), p);
  ayrac(g);
  GtkWidget *gz = gtk_check_menu_item_new_with_label(T("Gizli dosyaları göster", "Show hidden files"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gz), p->gizli);
  g_signal_connect(gz, "toggled", G_CALLBACK(m_gizli), p);
  gtk_menu_shell_append(GTK_MENU_SHELL(g), gz);
  GtkWidget *s = alt(m, T("Sıralama ölçütü", "Sort by"));
  gr = NULL;
  gr = radyo(s, gr, T("Ad", "Name"), !strcmp(p->sira, "ad"), G_CALLBACK(m_sira), "ad", p);
  gr = radyo(s, gr, T("Değiştirme tarihi", "Date modified"), !strcmp(p->sira, "tarih"), G_CALLBACK(m_sira), "tarih", p);
  gr = radyo(s, gr, T("Tür", "Type"), !strcmp(p->sira, "tur"), G_CALLBACK(m_sira), "tur", p);
  gr = radyo(s, gr, T("Boyut", "Size"), !strcmp(p->sira, "boyut"), G_CALLBACK(m_sira), "boyut", p);
  oge(m, T("Yenile", "Refresh"), "F5", G_CALLBACK(m_yenile), p);
  ayrac(m);
  GtkWidget *y = oge(m, T("Yapıştır", "Paste"), "Ctrl+V", G_CALLBACK(m_yapistir), p);
  gtk_widget_set_sensitive(y, panoda_dosya());
  ayrac(m);
  GtkWidget *yn = alt(m, T("Yeni", "New"));
  oge(yn, T("Klasör", "Folder"), "Ctrl+Shift+N", G_CALLBACK(m_yeni_klasor), p);
  oge(yn, T("Metin Belgesi", "Text Document"), NULL, G_CALLBACK(m_yeni_metin), p);
  ayrac(m);
  oge(m, T("Terminalde aç", "Open in Terminal"), NULL, G_CALLBACK(m_dizin_terminal), p);
  ayrac(m);
  oge(m, T("Özellikler", "Properties"), NULL, G_CALLBACK(m_dizin_ozellik), p);
  menu_goster(m, e);
}

/* ---------- fare / klavye ---------- */
static gboolean yol_altinda(Pen *p, double x, double y, GtkTreePath **yol) {
  if (p->liste_mi) {
    int bx, by; gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(p->liste), (int)x, (int)y, &bx, &by);
    return gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(p->liste), bx, by, yol, NULL, NULL, NULL);
  }
  *yol = gtk_icon_view_get_path_at_pos(GTK_ICON_VIEW(p->ikon), (int)x, (int)y);
  return *yol != NULL;
}
static gboolean basildi(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)w; Pen *p = v;
  if (e->type != GDK_BUTTON_PRESS) return FALSE;
  if (e->button == 8) { geri_tik(NULL, p); return TRUE; }
  if (e->button == 9) { ileri_tik(NULL, p); return TRUE; }
  if (e->button != 3) return FALSE;
  GtkTreePath *yol = NULL;
  if (yol_altinda(p, e->x, e->y, &yol)) {
    int secili = p->liste_mi ? gtk_tree_selection_path_is_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)), yol)
                             : gtk_icon_view_path_is_selected(GTK_ICON_VIEW(p->ikon), yol);
    if (!secili) {
      if (p->liste_mi) { gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste))); gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)), yol); }
      else { gtk_icon_view_unselect_all(GTK_ICON_VIEW(p->ikon)); gtk_icon_view_select_path(GTK_ICON_VIEW(p->ikon), yol); }
    }
    gtk_tree_path_free(yol);
    oge_menusu(p, (GdkEvent *)e);
  } else {
    if (p->liste_mi) gtk_tree_selection_unselect_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)));
    else gtk_icon_view_unselect_all(GTK_ICON_VIEW(p->ikon));
    arka_menusu(p, (GdkEvent *)e);
  }
  return TRUE;
}

static void yol_duzenle(Pen *p, int ac) {
  gtk_stack_set_visible_child_name(GTK_STACK(p->yol_yigin), ac ? "giris" : "parca");
  if (ac) { gtk_widget_grab_focus(p->yol_giris); gtk_editable_select_region(GTK_EDITABLE(p->yol_giris), 0, -1); }
}
static void yol_girildi(GtkEntry *g, gpointer v) {
  Pen *p = v;
  const char *s = gtk_entry_get_text(g);
  char *gen = g_str_has_prefix(s, "~") ? g_build_filename(g_get_home_dir(), s + 1, NULL) : g_strdup(s);
  GFile *f = g_file_parse_name(gen); g_free(gen);
  GFileType t = g_file_query_file_type(f, 0, NULL);
  if (t == G_FILE_TYPE_DIRECTORY || t == G_FILE_TYPE_MOUNTABLE) git(p, f, 1);
  else if (t != G_FILE_TYPE_UNKNOWN) dosya_ac(p, f, 0);
  else { char *m = g_strdup_printf(T("\"%s\" bulunamadı.", "\"%s\" was not found."), s); hata(p, m); g_free(m); }
  g_object_unref(f);
  yol_duzenle(p, 0);
}
static gboolean yol_odak_gitti(GtkWidget *w, GdkEvent *e, gpointer v) { (void)w; (void)e; yol_duzenle(v, 0); return FALSE; }

static gboolean tus(GtkWidget *w, GdkEventKey *e, gpointer v) {
  (void)w; Pen *p = v;
  int ctrl = e->state & GDK_CONTROL_MASK, alt_ = e->state & GDK_MOD1_MASK, shift = e->state & GDK_SHIFT_MASK;
  GtkWidget *odak = gtk_window_get_focus(GTK_WINDOW(p->pencere));
  int yazi = GTK_IS_ENTRY(odak);
  if (alt_ && e->keyval == GDK_KEY_Left) { geri_tik(NULL, p); return TRUE; }
  if (alt_ && e->keyval == GDK_KEY_Right) { ileri_tik(NULL, p); return TRUE; }
  if (alt_ && e->keyval == GDK_KEY_Up) { yukari_tik(NULL, p); return TRUE; }
  if (alt_ && (e->keyval == GDK_KEY_Return || e->keyval == GDK_KEY_KP_Enter)) { m_ozellik(NULL, p); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_l || e->keyval == GDK_KEY_L)) { yol_duzenle(p, 1); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_f || e->keyval == GDK_KEY_F)) { gtk_widget_grab_focus(p->arama); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_h || e->keyval == GDK_KEY_H)) { p->gizli = !p->gizli; gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(p->suzgec)); durum_yaz(p); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_n || e->keyval == GDK_KEY_N)) { if (shift) yeni_klasor(p); else pencere_ac(p->dizin); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_w || e->keyval == GDK_KEY_W)) { gtk_widget_destroy(p->pencere); return TRUE; }
  if (e->keyval == GDK_KEY_F5) { yenile(p); return TRUE; }
  if (yazi) return FALSE;
  if (e->keyval == GDK_KEY_BackSpace) { geri_tik(NULL, p); return TRUE; }
  if (e->keyval == GDK_KEY_Delete || e->keyval == GDK_KEY_KP_Delete) { cope_at(p, shift); return TRUE; }
  if (e->keyval == GDK_KEY_F2) { if (secili_sayisi(p) == 1) adlandir(p); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_c || e->keyval == GDK_KEY_C)) { panoya(p, 0); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_x || e->keyval == GDK_KEY_X)) { panoya(p, 1); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_v || e->keyval == GDK_KEY_V)) { yapistir(p, NULL); return TRUE; }
  if (ctrl && (e->keyval == GDK_KEY_a || e->keyval == GDK_KEY_A)) {
    if (p->liste_mi) gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste))); else gtk_icon_view_select_all(GTK_ICON_VIEW(p->ikon));
    return TRUE;
  }
  return FALSE;
}

/* ---------- sürükle-bırak ---------- */
static void surukle_veri(GtkWidget *w, GdkDragContext *c, GtkSelectionData *sd, guint b, guint t, gpointer v) {
  (void)w; (void)c; (void)b; (void)t;
  GList *d = secili_dosyalar(v);
  GPtrArray *u = g_ptr_array_new();
  for (GList *l = d; l; l = l->next) g_ptr_array_add(u, g_file_get_uri(l->data));
  g_ptr_array_add(u, NULL);
  gtk_selection_data_set_uris(sd, (char **)u->pdata);
  g_strfreev((char **)g_ptr_array_free(u, FALSE)); g_list_free_full(d, g_object_unref);
}
static void birakildi(GtkWidget *w, GdkDragContext *c, int x, int y, GtkSelectionData *sd, guint b, guint t, gpointer v) {
  (void)w; (void)b; Pen *p = v;
  char **u = gtk_selection_data_get_uris(sd);
  GFile *hedef = g_object_ref(p->dizin);
  GtkTreePath *yol = NULL;
  /* bir klasörün üzerine bırakıldıysa onun içine */
  if (yol_altinda(p, x, y, &yol)) {
    GtkTreeIter it; gboolean k; GFile *f;
    if (gtk_tree_model_get_iter(p->sirali, &it, yol)) { gtk_tree_model_get(p->sirali, &it, C_KLASOR, &k, C_DOSYA, &f, -1); if (k) { g_object_unref(hedef); hedef = f; } else g_object_unref(f); }
    gtk_tree_path_free(yol);
  }
  GList *l = NULL;
  for (int i = 0; u && u[i]; i++) { GFile *f = g_file_new_for_uri(u[i]); if (g_file_equal(f, hedef)) g_object_unref(f); else l = g_list_append(l, f); }
  /* aynı diskteyse taşı, değilse kopyala (Windows gibi); Ctrl kopyalar, Shift taşır */
  int tasi = gdk_drag_context_get_selected_action(c) == GDK_ACTION_MOVE;
  if (l && gdk_drag_context_get_selected_action(c) != GDK_ACTION_COPY) {
    char *a = g_file_get_path(l->data), *h = g_file_get_path(hedef);
    struct stat s1, s2;
    if (a && h && stat(a, &s1) == 0 && stat(h, &s2) == 0) tasi = s1.st_dev == s2.st_dev;
    g_free(a); g_free(h);
  }
  dosyalari_aktar(p, l, hedef, tasi);
  g_object_unref(hedef); g_strfreev(u);
  gtk_drag_finish(c, TRUE, FALSE, t);
}

/* ---------- görünüm ve sıralama ---------- */
static gboolean gorunur(GtkTreeModel *m, GtkTreeIter *it, gpointer v) {
  Pen *p = v;
  gboolean gizli; char *ad;
  gtk_tree_model_get(m, it, C_GIZLI, &gizli, C_AD, &ad, -1);
  int r = !gizli || p->gizli;
  const char *a = gtk_entry_get_text(GTK_ENTRY(p->arama));
  if (r && *a) { char *x = g_utf8_casefold(ad ? ad : "", -1), *y = g_utf8_casefold(a, -1); r = strstr(x, y) != NULL; g_free(x); g_free(y); }
  g_free(ad);
  return r;
}
static int karsilastir(GtkTreeModel *m, GtkTreeIter *a, GtkTreeIter *b, gpointer v) {
  Pen *p = v;
  gboolean ka, kb; char *aa, *ab, *ta = NULL, *tb = NULL; gint64 za, zb, ba, bb;
  gtk_tree_model_get(m, a, C_KLASOR, &ka, C_ANAHTAR, &aa, C_ZAMAN, &za, C_BOYUT, &ba, C_TUR_M, &ta, -1);
  gtk_tree_model_get(m, b, C_KLASOR, &kb, C_ANAHTAR, &ab, C_ZAMAN, &zb, C_BOYUT, &bb, C_TUR_M, &tb, -1);
  int r;
  if (ka != kb) r = ka ? -1 : 1;   /* klasörler her zaman önce */
  else {
    if (!strcmp(p->sira, "tarih")) r = za < zb ? 1 : za > zb ? -1 : 0;
    else if (!strcmp(p->sira, "boyut")) r = ba < bb ? 1 : ba > bb ? -1 : 0;
    else if (!strcmp(p->sira, "tur")) r = g_utf8_collate(ta ? ta : "", tb ? tb : "");
    else r = 0;
    if (!r) r = g_strcmp0(aa, ab);
  }
  g_free(aa); g_free(ab); g_free(ta); g_free(tb);
  return r;
}
static void sirala_ayarla(Pen *p) {
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(p->sirali), GTK_TREE_SORTABLE_UNSORTED_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(p->sirali), GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID, GTK_SORT_ASCENDING);
}
static void gorunum_ayarla(Pen *p, int liste) {
  p->liste_mi = liste;
  gtk_stack_set_visible_child_name(GTK_STACK(p->gorunum_yigin), liste ? "liste" : "ikon");
  ae_ayar_yaz("DOSYALAR_GORUNUM", liste ? "liste" : "ikon");
  durum_yaz(p);
}
static void arama_degisti(GtkSearchEntry *s, gpointer v) { (void)s; Pen *p = v; gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(p->suzgec)); durum_yaz(p); }
static void secim_ikon(GtkIconView *iv, gpointer v) { (void)iv; durum_yaz(v); }
static void secim_liste(GtkTreeSelection *s, gpointer v) { (void)s; durum_yaz(v); }
static void yer_secildi(GtkPlacesSidebar *s, GObject *konum, GtkPlacesOpenFlags b, gpointer v) {
  (void)s;
  if (b & (GTK_PLACES_OPEN_NEW_WINDOW | GTK_PLACES_OPEN_NEW_TAB)) pencere_ac(G_FILE(konum)); else git(v, G_FILE(konum), 1);
}
static void gorunum_dugme(GtkToggleButton *b, gpointer v) { gorunum_ayarla(v, gtk_toggle_button_get_active(b)); }
static void pencere_kapandi(GtkWidget *w, gpointer v) {
  (void)w; Pen *p = v;
  if (p->yukleme) g_cancellable_cancel(p->yukleme);
  if (p->izleyici) g_file_monitor_cancel(p->izleyici);
}

static GtkWidget *simge_dugme(const char *simge, const char *ipucu) {
  GtkWidget *b = gtk_button_new_from_icon_name(simge, GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_tooltip_text(b, ipucu);
  gtk_widget_set_focus_on_click(b, FALSE);
  return b;
}
static GtkTreeViewColumn *sutun(const char *ad, int veri, float x, int genis) {
  GtkCellRenderer *r = gtk_cell_renderer_text_new();
  g_object_set(r, "xalign", x, NULL);
  GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(ad, r, "text", veri, NULL);
  gtk_tree_view_column_set_resizable(c, TRUE);
  gtk_tree_view_column_set_min_width(c, genis);
  return c;
}

static Pen *pencere_ac(GFile *f) {
  Pen *p = g_new0(Pen, 1);
  char *s = ae_ayar("DOSYALAR_SIRA", "ad"); snprintf(p->sira, sizeof p->sira, "%s", s); g_free(s);
  s = ae_ayar("DOSYALAR_GORUNUM", "ikon"); p->liste_mi = !strcmp(s, "liste"); g_free(s);

  p->pencere = gtk_application_window_new(uyg);
  gtk_window_set_default_size(GTK_WINDOW(p->pencere), 1000, 640);
  gtk_window_set_icon_name(GTK_WINDOW(p->pencere), "aether-dosyalar");
  g_signal_connect(p->pencere, "key-press-event", G_CALLBACK(tus), p);
  g_signal_connect(p->pencere, "destroy", G_CALLBACK(pencere_kapandi), p);

  /* araç çubuğu */
  p->geri = simge_dugme("go-previous-symbolic", T("Geri (Alt+Sol)", "Back (Alt+Left)"));
  p->ileri = simge_dugme("go-next-symbolic", T("İleri (Alt+Sağ)", "Forward (Alt+Right)"));
  p->yukari = simge_dugme("go-up-symbolic", T("Üst klasör (Alt+Yukarı)", "Up (Alt+Up)"));
  g_signal_connect(p->geri, "clicked", G_CALLBACK(geri_tik), p);
  g_signal_connect(p->ileri, "clicked", G_CALLBACK(ileri_tik), p);
  g_signal_connect(p->yukari, "clicked", G_CALLBACK(yukari_tik), p);
  p->yol_kutu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *yk = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(yk), GTK_POLICY_EXTERNAL, GTK_POLICY_NEVER);
  gtk_container_add(GTK_CONTAINER(yk), p->yol_kutu);
  p->yol_giris = gtk_entry_new();
  g_signal_connect(p->yol_giris, "activate", G_CALLBACK(yol_girildi), p);
  g_signal_connect(p->yol_giris, "focus-out-event", G_CALLBACK(yol_odak_gitti), p);
  p->yol_yigin = gtk_stack_new();
  gtk_stack_add_named(GTK_STACK(p->yol_yigin), yk, "parca");
  gtk_stack_add_named(GTK_STACK(p->yol_yigin), p->yol_giris, "giris");
  GtkWidget *duzenle = simge_dugme("document-edit-symbolic", T("Yolu yaz (Ctrl+L)", "Type a path (Ctrl+L)"));
  g_signal_connect_swapped(duzenle, "clicked", G_CALLBACK(yol_duzenle), p);
  p->arama = gtk_search_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(p->arama), T("Bu klasörde ara", "Search this folder"));
  gtk_widget_set_size_request(p->arama, 220, -1);
  g_signal_connect(p->arama, "search-changed", G_CALLBACK(arama_degisti), p);
  GtkWidget *gd = gtk_toggle_button_new();
  gtk_button_set_image(GTK_BUTTON(gd), gtk_image_new_from_icon_name("view-list-symbolic", GTK_ICON_SIZE_BUTTON));
  gtk_widget_set_tooltip_text(gd, T("Ayrıntılar görünümü", "Details view"));
  GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_container_set_border_width(GTK_CONTAINER(ust), 6);
  gtk_box_pack_start(GTK_BOX(ust), p->geri, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ust), p->ileri, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ust), p->yukari, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ust), p->yol_yigin, TRUE, TRUE, 6);
  gtk_box_pack_start(GTK_BOX(ust), duzenle, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ust), p->arama, FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(ust), gd, FALSE, FALSE, 0);

  /* model */
  p->depo = gtk_list_store_new(C_SAYI, G_TYPE_FILE, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN,
                               G_TYPE_INT64, G_TYPE_STRING, G_TYPE_INT64, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
  p->suzgec = gtk_tree_model_filter_new(GTK_TREE_MODEL(p->depo), NULL);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(p->suzgec), gorunur, p, NULL);
  p->sirali = gtk_tree_model_sort_new_with_model(p->suzgec);
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(p->sirali), karsilastir, p, NULL);
  sirala_ayarla(p);

  /* simge görünümü */
  p->ikon = gtk_icon_view_new_with_model(p->sirali);
  gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(p->ikon), C_SIMGE);
  gtk_icon_view_set_text_column(GTK_ICON_VIEW(p->ikon), C_AD);
  gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(p->ikon), GTK_SELECTION_MULTIPLE);
  gtk_icon_view_set_item_width(GTK_ICON_VIEW(p->ikon), 96);
  gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(p->ikon), 6); gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(p->ikon), 6);
  gtk_icon_view_set_activate_on_single_click(GTK_ICON_VIEW(p->ikon), FALSE);
  g_signal_connect(p->ikon, "item-activated", G_CALLBACK(ikon_etkin), p);
  g_signal_connect(p->ikon, "selection-changed", G_CALLBACK(secim_ikon), p);
  g_signal_connect(p->ikon, "button-press-event", G_CALLBACK(basildi), p);

  /* ayrıntı görünümü */
  p->liste = gtk_tree_view_new_with_model(p->sirali);
  gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)), GTK_SELECTION_MULTIPLE);
  gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(p->liste), TRUE);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(p->liste), FALSE);
  GtkTreeViewColumn *c = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(c, T("Ad", "Name"));
  GtkCellRenderer *ri = gtk_cell_renderer_pixbuf_new(), *rt = gtk_cell_renderer_text_new();
  g_object_set(rt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_pack_start(c, ri, FALSE); gtk_tree_view_column_add_attribute(c, ri, "pixbuf", C_KUCUK);
  gtk_tree_view_column_pack_start(c, rt, TRUE); gtk_tree_view_column_add_attribute(c, rt, "text", C_AD);
  gtk_tree_view_column_set_expand(c, TRUE); gtk_tree_view_column_set_resizable(c, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(p->liste), c);
  gtk_tree_view_append_column(GTK_TREE_VIEW(p->liste), sutun(T("Değiştirme tarihi", "Date modified"), C_ZAMAN_M, 0, 140));
  gtk_tree_view_append_column(GTK_TREE_VIEW(p->liste), sutun(T("Tür", "Type"), C_TUR_M, 0, 140));
  gtk_tree_view_append_column(GTK_TREE_VIEW(p->liste), sutun(T("Boyut", "Size"), C_BOYUT_M, 1, 80));
  g_signal_connect(p->liste, "row-activated", G_CALLBACK(liste_etkin), p);
  g_signal_connect(gtk_tree_view_get_selection(GTK_TREE_VIEW(p->liste)), "changed", G_CALLBACK(secim_liste), p);
  g_signal_connect(p->liste, "button-press-event", G_CALLBACK(basildi), p);

  GtkTargetEntry h[] = { { "text/uri-list", 0, 0 } };
  GtkWidget *gorunumler[2] = { p->ikon, p->liste };
  for (int i = 0; i < 2; i++) {
    if (i == 0) gtk_icon_view_enable_model_drag_source(GTK_ICON_VIEW(p->ikon), GDK_BUTTON1_MASK, h, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    else gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(p->liste), GDK_BUTTON1_MASK, h, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(gorunumler[i], "drag-data-get", G_CALLBACK(surukle_veri), p);
    gtk_drag_dest_set(gorunumler[i], GTK_DEST_DEFAULT_ALL, h, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
    g_signal_connect(gorunumler[i], "drag-data-received", G_CALLBACK(birakildi), p);
  }

  p->gorunum_yigin = gtk_stack_new();
  GtkWidget *s1 = gtk_scrolled_window_new(NULL, NULL), *s2 = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(s1), p->ikon); gtk_container_add(GTK_CONTAINER(s2), p->liste);
  gtk_stack_add_named(GTK_STACK(p->gorunum_yigin), s1, "ikon");
  gtk_stack_add_named(GTK_STACK(p->gorunum_yigin), s2, "liste");

  /* yerler */
  p->yan = gtk_places_sidebar_new();
  gtk_places_sidebar_set_show_recent(GTK_PLACES_SIDEBAR(p->yan), FALSE);
  gtk_places_sidebar_set_show_desktop(GTK_PLACES_SIDEBAR(p->yan), TRUE);
  gtk_places_sidebar_set_show_trash(GTK_PLACES_SIDEBAR(p->yan), TRUE);
  gtk_places_sidebar_set_show_other_locations(GTK_PLACES_SIDEBAR(p->yan), FALSE);
  gtk_places_sidebar_set_show_starred_location(GTK_PLACES_SIDEBAR(p->yan), FALSE);
  gtk_places_sidebar_set_open_flags(GTK_PLACES_SIDEBAR(p->yan), GTK_PLACES_OPEN_NORMAL | GTK_PLACES_OPEN_NEW_WINDOW);
  gtk_widget_set_size_request(p->yan, 200, -1);
  g_signal_connect(p->yan, "open-location", G_CALLBACK(yer_secildi), p);

  /* durum ve işlem çubuğu */
  p->durum = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(p->durum), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(p->durum), "ae-alt");
  GtkWidget *dk = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width(GTK_CONTAINER(dk), 4);
  gtk_box_pack_start(GTK_BOX(dk), p->durum, TRUE, TRUE, 6);
  p->is_yazi = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(p->is_yazi), 0); gtk_label_set_ellipsize(GTK_LABEL(p->is_yazi), PANGO_ELLIPSIZE_MIDDLE);
  p->is_ilerleme = gtk_progress_bar_new();
  GtkWidget *iptal = gtk_button_new_with_label(T("İptal", "Cancel"));
  g_signal_connect(iptal, "clicked", G_CALLBACK(is_iptal_tik), p);
  GtkWidget *ik = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(ik), 8);
  GtkWidget *iy = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_pack_start(GTK_BOX(iy), p->is_yazi, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(iy), p->is_ilerleme, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ik), iy, TRUE, TRUE, 0); gtk_box_pack_start(GTK_BOX(ik), iptal, FALSE, FALSE, 0);
  p->is_cubugu = gtk_revealer_new(); gtk_container_add(GTK_CONTAINER(p->is_cubugu), ik);

  GtkWidget *sag = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(sag), p->gorunum_yigin, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(sag), p->is_cubugu, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), dk, FALSE, FALSE, 0);
  GtkWidget *orta = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_paned_pack1(GTK_PANED(orta), p->yan, FALSE, FALSE);
  gtk_paned_pack2(GTK_PANED(orta), sag, TRUE, FALSE);
  GtkWidget *ana = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(ana), ust, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ana), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ana), orta, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(p->pencere), ana);

  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gd), p->liste_mi);
  g_signal_connect(gd, "toggled", G_CALLBACK(gorunum_dugme), p);
  gtk_widget_show_all(p->pencere);
  gtk_stack_set_visible_child_name(GTK_STACK(p->gorunum_yigin), p->liste_mi ? "liste" : "ikon");
  git(p, f, 0);
  gtk_widget_grab_focus(p->liste_mi ? p->liste : p->ikon);
  return p;
}

static void ac_sinyali(GApplication *a, GFile **dosyalar, int n, const char *ipucu, gpointer v) {
  (void)a; (void)ipucu; (void)v;
  for (int i = 0; i < n; i++) {
    GFileType t = g_file_query_file_type(dosyalar[i], 0, NULL);
    if (t == G_FILE_TYPE_DIRECTORY || t == G_FILE_TYPE_MOUNTABLE || g_file_has_uri_scheme(dosyalar[i], "trash")) pencere_ac(dosyalar[i]);
    else { GFile *u = g_file_get_parent(dosyalar[i]); if (u) { pencere_ac(u); g_object_unref(u); } }
  }
}
static void etkin(GApplication *a, gpointer v) {
  (void)a; (void)v;
  GFile *ev = g_file_new_for_path(g_get_home_dir());
  pencere_ac(ev); g_object_unref(ev);
}
static void basla(GApplication *a, gpointer v) {
  (void)a; (void)v;
  ae_css();
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, ".kalin { font-weight: bold; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

int main(int argc, char **argv) {
  uyg = gtk_application_new("org.aether.Dosyalar", G_APPLICATION_HANDLES_OPEN);
  g_signal_connect(uyg, "startup", G_CALLBACK(basla), NULL);
  g_signal_connect(uyg, "activate", G_CALLBACK(etkin), NULL);
  g_signal_connect(uyg, "open", G_CALLBACK(ac_sinyali), NULL);
  int r = g_application_run(G_APPLICATION(uyg), argc, argv);
  g_object_unref(uyg);
  return r;
}
