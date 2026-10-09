/* aether-masaustu — Aether masaüstü: duvar kağıdı, masaüstü simgeleri ve Windows'taki gibi sağ tık menüsü
 *
 * ~/Masaüstü (İngilizcede ~/Desktop) klasöründeki dosyaları simge olarak gösterir.
 *   Boş alana sağ tık: Görünüm, Sıralama ölçütü, Yenile, Yapıştır, Yeni (klasör, metin belgesi),
 *                      Terminalde aç, Görüntü ayarları, Kişiselleştir
 *   Simgeye sağ tık:   Aç, Kes, Kopyala, Yeniden adlandır, Sil
 *   Klavye: Enter aç · F2 yeniden adlandır · Del çöpe at · F5 yenile · Ctrl+A/C/X/V
 * Pano Aether Dosyalar ile ortaktır (x-special/gnome-copied-files). SIGHUP ile duvar kağıdı ve ayarlar yenilenir.
 */
#include <gio/gdesktopappinfo.h>
#include <glib-unix.h>
#include <math.h>
#include <sys/stat.h>
#include "aether.h"

typedef struct {
  GFile *dosya; char *ad, *anahtar, *tur; GIcon *simge; GdkPixbuf *pb;
  gboolean klasor, uygulama; guint64 boyut, zaman;
  int x, y, secili;
} Oge;

static GtkWidget *pencere, *alan;
static GPtrArray *ogeler;
static GFile *dizin;
static GFileMonitor *izleyici;
static cairo_surface_t *duvar;
static int sw, sh;                 /* ekran */
static GdkRectangle calisma;      /* görev çubuğu hariç alan */
static int simge_boy = 48, hucre_w = 100, hucre_h = 96;
static char sira[16] = "ad";
static int goster = 1;
static int basilan = -1, surukle_x = -1, surukle_y = -1, kutu = 0, kx1, ky1, kx2, ky2, suruklendi = 0;
static guint yenile_kaynak;

static const char *KOPYALANAN = "x-special/gnome-copied-files";

/* ---------- ayarlar ---------- */
static void ayarlari_oku(void) {
  char *s = ae_ayar("MASAUSTU_SIMGE", "orta");
  if (!strcmp(s, "buyuk")) { simge_boy = 72; hucre_w = 120; hucre_h = 120; }
  else if (!strcmp(s, "kucuk")) { simge_boy = 32; hucre_w = 84; hucre_h = 76; }
  else { simge_boy = 48; hucre_w = 100; hucre_h = 96; }
  g_free(s);
  s = ae_ayar("MASAUSTU_SIRA", "ad"); snprintf(sira, sizeof sira, "%s", s); g_free(s);
  s = ae_ayar("MASAUSTU_GOSTER", "1"); goster = strcmp(s, "0") != 0; g_free(s);
}

/* ---------- duvar kağıdı ---------- */
static void duvar_yukle(void) {
  if (duvar) { cairo_surface_destroy(duvar); duvar = NULL; }
  char *yol = ae_ayar("DUVAR", "/usr/share/aether/wallpapers/nebula.png");
  GdkPixbuf *pb = gdk_pixbuf_new_from_file(yol, NULL);
  if (!pb) pb = gdk_pixbuf_new_from_file("/usr/share/aether/wallpapers/nebula.png", NULL);
  g_free(yol);
  duvar = cairo_image_surface_create(CAIRO_FORMAT_RGB24, sw, sh);
  cairo_t *cr = cairo_create(duvar);
  cairo_set_source_rgb(cr, 0.05, 0.04, 0.12); cairo_paint(cr);
  if (pb) {
    /* ekranı tamamen kapla (kırp) */
    double pw = gdk_pixbuf_get_width(pb), ph = gdk_pixbuf_get_height(pb);
    double o = MAX(sw / pw, sh / ph);
    cairo_translate(cr, (sw - pw * o) / 2, (sh - ph * o) / 2);
    cairo_scale(cr, o, o);
    gdk_cairo_set_source_pixbuf(cr, pb, 0, 0);
    cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
    cairo_paint(cr);
    g_object_unref(pb);
  }
  cairo_destroy(cr);
}

/* ---------- öğeler ---------- */
static void oge_sil(gpointer p) {
  Oge *o = p;
  g_object_unref(o->dosya); g_free(o->ad); g_free(o->anahtar); g_free(o->tur);
  if (o->simge) g_object_unref(o->simge);
  if (o->pb) g_object_unref(o->pb);
  g_free(o);
}

static GdkPixbuf *simge_yukle(Oge *o) {
  /* resimler için küçük önizleme */
  if (o->tur && g_str_has_prefix(o->tur, "image/") && o->boyut < 30 * 1024 * 1024) {
    char *yol = g_file_get_path(o->dosya);
    GdkPixbuf *pb = yol ? gdk_pixbuf_new_from_file_at_scale(yol, simge_boy, simge_boy, TRUE, NULL) : NULL;
    g_free(yol);
    if (pb) return pb;
  }
  GtkIconTheme *t = gtk_icon_theme_get_default();
  GtkIconInfo *bilgi = o->simge ? gtk_icon_theme_lookup_by_gicon(t, o->simge, simge_boy, GTK_ICON_LOOKUP_FORCE_SIZE) : NULL;
  if (!bilgi) bilgi = gtk_icon_theme_lookup_icon(t, o->klasor ? "folder" : "text-x-generic", simge_boy, GTK_ICON_LOOKUP_FORCE_SIZE);
  GdkPixbuf *pb = bilgi ? gtk_icon_info_load_icon(bilgi, NULL) : NULL;
  if (bilgi) g_object_unref(bilgi);
  return pb;
}

static int karsilastir(gconstpointer a, gconstpointer b) {
  Oge *x = *(Oge **)a, *y = *(Oge **)b;
  if (x->klasor != y->klasor) return x->klasor ? -1 : 1;            /* klasörler önce */
  int r = 0;
  if (!strcmp(sira, "boyut")) r = x->boyut < y->boyut ? 1 : x->boyut > y->boyut ? -1 : 0;
  else if (!strcmp(sira, "tur")) r = g_strcmp0(x->tur, y->tur);
  else if (!strcmp(sira, "tarih")) r = x->zaman < y->zaman ? 1 : x->zaman > y->zaman ? -1 : 0;
  return r ? r : strcmp(x->anahtar, y->anahtar);
}

static void yerlestir(void) {
  /* Windows gibi: sol üstten aşağı doğru sütun sütun */
  int x0 = calisma.x + 6, y0 = calisma.y + 6;
  int satir = MAX(1, (calisma.height - 12) / hucre_h);
  for (unsigned i = 0; i < ogeler->len; i++) {
    Oge *o = ogeler->pdata[i];
    o->x = x0 + (int)(i / satir) * hucre_w;
    o->y = y0 + (int)(i % satir) * hucre_h;
  }
}

static void yukle(void) {
  /* seçimi koru */
  GHashTable *secili = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  if (ogeler) for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili) g_hash_table_add(secili, g_file_get_basename(o->dosya)); }
  if (ogeler) g_ptr_array_free(ogeler, TRUE);
  ogeler = g_ptr_array_new_with_free_func(oge_sil);
  GFileEnumerator *en = g_file_enumerate_children(dizin, "standard::*,time::modified,access::can-execute", G_FILE_QUERY_INFO_NONE, NULL, NULL);
  GFileInfo *bi;
  while (en && (bi = g_file_enumerator_next_file(en, NULL, NULL))) {
    if (g_file_info_get_is_hidden(bi) || g_file_info_get_is_backup(bi)) { g_object_unref(bi); continue; }
    Oge *o = g_new0(Oge, 1);
    o->dosya = g_file_get_child(dizin, g_file_info_get_name(bi));
    o->ad = g_strdup(g_file_info_get_display_name(bi));
    o->klasor = g_file_info_get_file_type(bi) == G_FILE_TYPE_DIRECTORY;
    o->boyut = g_file_info_get_size(bi);
    GDateTime *dt = g_file_info_get_modification_date_time(bi);
    if (dt) { o->zaman = g_date_time_to_unix(dt); g_date_time_unref(dt); }
    o->tur = g_strdup(g_file_info_get_content_type(bi));
    GIcon *ic = g_file_info_get_icon(bi); if (ic) o->simge = g_object_ref(ic);
    if (g_str_has_suffix(g_file_info_get_name(bi), ".desktop")) {
      char *yol = g_file_get_path(o->dosya);
      GDesktopAppInfo *a = yol ? g_desktop_app_info_new_from_filename(yol) : NULL;
      if (a) {
        g_free(o->ad); o->ad = g_strdup(g_app_info_get_display_name(G_APP_INFO(a)));
        GIcon *ai = g_app_info_get_icon(G_APP_INFO(a));
        if (ai) { if (o->simge) g_object_unref(o->simge); o->simge = g_object_ref(ai); }
        o->uygulama = TRUE; g_object_unref(a);
      }
      g_free(yol);
    }
    char *kat = g_utf8_casefold(o->ad, -1); o->anahtar = g_utf8_collate_key_for_filename(kat, -1); g_free(kat);
    char *taban = g_file_get_basename(o->dosya);
    o->secili = g_hash_table_contains(secili, taban); g_free(taban);
    o->pb = simge_yukle(o);
    g_ptr_array_add(ogeler, o);
    g_object_unref(bi);
  }
  if (en) g_object_unref(en);
  g_hash_table_destroy(secili);
  g_ptr_array_sort(ogeler, karsilastir);
  yerlestir();
  if (alan) gtk_widget_queue_draw(alan);
}

static gboolean yenile_gecikmeli(gpointer v) { (void)v; yenile_kaynak = 0; yukle(); return G_SOURCE_REMOVE; }
static void dizin_degisti(GFileMonitor *m, GFile *a, GFile *b, GFileMonitorEvent e, gpointer v) {
  (void)m; (void)a; (void)b; (void)e; (void)v;
  if (yenile_kaynak) g_source_remove(yenile_kaynak);
  yenile_kaynak = g_timeout_add(250, yenile_gecikmeli, NULL);
}

/* ---------- çizim ---------- */
static PangoLayout *yazi_duzeni(GtkWidget *w, Oge *o, int tam) {
  PangoLayout *l = gtk_widget_create_pango_layout(w, o->ad);
  pango_layout_set_width(l, (hucre_w - 8) * PANGO_SCALE);
  pango_layout_set_alignment(l, PANGO_ALIGN_CENTER);
  pango_layout_set_wrap(l, PANGO_WRAP_WORD_CHAR);
  pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
  pango_layout_set_height(l, tam ? -6 : -2);
  return l;
}

static void oge_kutusu(Oge *o, GdkRectangle *r) {
  r->x = o->x + 2; r->y = o->y; r->width = hucre_w - 4; r->height = hucre_h - 4;
}

static void yuvarlak(cairo_t *cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -M_PI / 2, 0); cairo_arc(cr, x + w - r, y + h - r, r, 0, M_PI / 2);
  cairo_arc(cr, x + r, y + h - r, r, M_PI / 2, M_PI); cairo_arc(cr, x + r, y + r, r, M_PI, 3 * M_PI / 2);
  cairo_close_path(cr);
}

static gboolean ciz(GtkWidget *w, cairo_t *cr, gpointer v) {
  (void)v;
  if (duvar) { cairo_set_source_surface(cr, duvar, 0, 0); cairo_paint(cr); }
  if (!goster) return FALSE;
  for (unsigned i = 0; i < ogeler->len; i++) {
    Oge *o = ogeler->pdata[i];
    PangoLayout *l = yazi_duzeni(w, o, o->secili);
    int tw, th; pango_layout_get_pixel_size(l, &tw, &th);
    int ix = o->x + (hucre_w - simge_boy) / 2, iy = o->y + 4;
    if (o->secili) {
      int yuk = MAX(hucre_h - 4, simge_boy + 10 + th + 4);
      yuvarlak(cr, o->x + 2, o->y, hucre_w - 4, yuk, 4);
      cairo_set_source_rgba(cr, 0.49, 0.42, 0.84, 0.42); cairo_fill_preserve(cr);
      cairo_set_source_rgba(cr, 0.75, 0.69, 1.0, 0.85); cairo_set_line_width(cr, 1); cairo_stroke(cr);
    }
    if (o->pb) {
      int pw = gdk_pixbuf_get_width(o->pb), ph = gdk_pixbuf_get_height(o->pb);
      gdk_cairo_set_source_pixbuf(cr, o->pb, ix + (simge_boy - pw) / 2, iy + (simge_boy - ph) / 2);
      cairo_paint(cr);
    }
    /* beyaz yazı + gölge: her duvar kağıdında okunsun */
    int ty = iy + simge_boy + 6;
    cairo_set_source_rgba(cr, 0, 0, 0, 0.75);
    cairo_move_to(cr, o->x + 4 + 1, ty + 1); pango_cairo_show_layout(cr, l);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, o->x + 4, ty); pango_cairo_show_layout(cr, l);
    g_object_unref(l);
  }
  if (kutu) {
    double x = MIN(kx1, kx2), y = MIN(ky1, ky2), ww = abs(kx2 - kx1), hh = abs(ky2 - ky1);
    cairo_rectangle(cr, x + 0.5, y + 0.5, ww, hh);
    cairo_set_source_rgba(cr, 0.49, 0.42, 0.84, 0.25); cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.75, 0.69, 1.0, 0.9); cairo_set_line_width(cr, 1); cairo_stroke(cr);
  }
  return FALSE;
}

static int oge_bul(int x, int y) {
  if (!goster) return -1;
  for (unsigned i = 0; i < ogeler->len; i++) {
    GdkRectangle r; oge_kutusu(ogeler->pdata[i], &r);
    if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return (int)i;
  }
  return -1;
}
static void secimi_temizle(void) { for (unsigned i = 0; i < ogeler->len; i++) ((Oge *)ogeler->pdata[i])->secili = 0; }
static int secili_sayi(void) { int n = 0; for (unsigned i = 0; i < ogeler->len; i++) n += ((Oge *)ogeler->pdata[i])->secili; return n; }

/* ---------- eylemler ---------- */
static void hata(const char *m) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", m);
  gtk_window_set_title(GTK_WINDOW(d), T("Masaüstü", "Desktop"));
  gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);
  gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}

static void calistir_dizinde(const char *const *argv) {
  char *yol = g_file_get_path(dizin);
  g_spawn_async(yol, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);
  g_free(yol);
}

static void oge_ac(Oge *o) {
  GError *e = NULL;
  char *yol = g_file_get_path(o->dosya);
  if (o->klasor) {
    const char *a[] = { "aether-dosyalar", yol, NULL };
    calistir_dizinde(a);
  } else if (o->uygulama) {
    /* yalnızca çalıştırılabilir işaretli kısayollar çalışsın (indirilen .desktop dosyaları kendiliğinden çalışmasın) */
    if (access(yol, X_OK) == 0) {
      GDesktopAppInfo *a = g_desktop_app_info_new_from_filename(yol);
      if (a) { g_app_info_launch(G_APP_INFO(a), NULL, NULL, &e); g_object_unref(a); }
    } else {
      const char *a[] = { "mousepad", yol, NULL };
      calistir_dizinde(a);
    }
  } else {
    char *uri = g_file_get_uri(o->dosya);
    if (!g_app_info_launch_default_for_uri(uri, NULL, &e)) {
      g_clear_error(&e);
      char *m = g_strdup_printf(T("\"%s\" dosyasını açacak bir program bulunamadı.", "No program found to open \"%s\"."), o->ad);
      hata(m); g_free(m);
    }
    g_free(uri);
  }
  if (e) { hata(e->message); g_error_free(e); }
  g_free(yol);
}

static void secilileri_ac(void) {
  for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili) oge_ac(o); }
}

static void cope_at(void) {
  for (unsigned i = 0; i < ogeler->len; i++) {
    Oge *o = ogeler->pdata[i];
    if (!o->secili) continue;
    GError *e = NULL;
    if (!g_file_trash(o->dosya, NULL, &e)) {
      char *m = g_strdup_printf(T("\"%s\" çöp kutusuna taşınamadı: %s", "Could not move \"%s\" to the trash: %s"), o->ad, e->message);
      hata(m); g_free(m); g_error_free(e);
    }
  }
}

static char *ad_sor(const char *baslik, const char *eski) {
  GtkWidget *d = gtk_dialog_new_with_buttons(baslik, GTK_WINDOW(pencere), GTK_DIALOG_MODAL,
                                             T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("Tamam", "OK"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
  gtk_window_set_position(GTK_WINDOW(d), GTK_WIN_POS_CENTER);
  GtkWidget *g = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(g), eski);
  gtk_entry_set_activates_default(GTK_ENTRY(g), TRUE);
  gtk_widget_set_size_request(g, 320, -1);
  GtkWidget *k = gtk_dialog_get_content_area(GTK_DIALOG(d));
  gtk_container_set_border_width(GTK_CONTAINER(k), 12);
  gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
  gtk_widget_show_all(d);
  /* uzantıyı seçme (Windows gibi): "rapor.txt" → "rapor" seçili */
  const char *nokta = strrchr(eski, '.');
  int son = nokta && nokta != eski ? (int)g_utf8_pointer_to_offset(eski, nokta) : -1;
  gtk_editable_select_region(GTK_EDITABLE(g), 0, son);
  char *sonuc = NULL;
  if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) sonuc = g_strstrip(g_strdup(gtk_entry_get_text(GTK_ENTRY(g))));
  gtk_widget_destroy(d);
  if (sonuc && (!*sonuc || strchr(sonuc, '/'))) { g_free(sonuc); sonuc = NULL; }
  return sonuc;
}

static void yeniden_adlandir_dosya(GFile *f) {
  char *eski = g_file_get_basename(f);
  char *yeni = ad_sor(T("Yeniden adlandır", "Rename"), eski);
  if (yeni && strcmp(yeni, eski)) {
    GError *e = NULL;
    GFile *n = g_file_set_display_name(f, yeni, NULL, &e);
    if (n) g_object_unref(n); else { hata(e->message); g_error_free(e); }
  }
  g_free(eski); g_free(yeni);
}

static void yeniden_adlandir(void) {
  for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili) { yeniden_adlandir_dosya(o->dosya); return; } }
}

/* "Yeni klasör", "Yeni klasör (2)"... */
static GFile *bos_ad(const char *taban, const char *uzanti) {
  for (int i = 1; i < 1000; i++) {
    char *ad = i == 1 ? g_strdup_printf("%s%s", taban, uzanti) : g_strdup_printf("%s (%d)%s", taban, i, uzanti);
    GFile *f = g_file_get_child(dizin, ad); g_free(ad);
    if (!g_file_query_exists(f, NULL)) return f;
    g_object_unref(f);
  }
  return NULL;
}

static void yeni_klasor(GtkMenuItem *m, gpointer v) {
  (void)m; (void)v;
  GFile *f = bos_ad(T("Yeni klasör", "New folder"), "");
  GError *e = NULL;
  if (f && g_file_make_directory(f, NULL, &e)) yeniden_adlandir_dosya(f);
  else if (e) { hata(e->message); g_error_free(e); }
  if (f) g_object_unref(f);
}
static void yeni_metin(GtkMenuItem *m, gpointer v) {
  (void)m; (void)v;
  GFile *f = bos_ad(T("Yeni metin belgesi", "New text document"), ".txt");
  GError *e = NULL;
  GFileOutputStream *s = f ? g_file_create(f, G_FILE_CREATE_NONE, NULL, &e) : NULL;
  if (s) { g_object_unref(s); yeniden_adlandir_dosya(f); }
  else if (e) { hata(e->message); g_error_free(e); }
  if (f) g_object_unref(f);
}

/* ---------- pano ---------- */
static char **pano_urileri;
static int pano_kes;

static void pano_ver(GtkClipboard *c, GtkSelectionData *sd, guint bilgi, gpointer v) {
  (void)c; (void)v;
  GString *s = g_string_new(NULL);
  if (bilgi == 0) {   /* x-special/gnome-copied-files */
    g_string_append(s, pano_kes ? "cut" : "copy");
    for (int i = 0; pano_urileri && pano_urileri[i]; i++) g_string_append_printf(s, "\n%s", pano_urileri[i]);
    gtk_selection_data_set(sd, gdk_atom_intern(KOPYALANAN, FALSE), 8, (guchar *)s->str, s->len);
  } else if (bilgi == 1) {
    gtk_selection_data_set_uris(sd, pano_urileri);
  } else {
    for (int i = 0; pano_urileri && pano_urileri[i]; i++) {
      char *p = g_filename_from_uri(pano_urileri[i], NULL, NULL);
      g_string_append_printf(s, "%s%s", i ? "\n" : "", p ? p : pano_urileri[i]); g_free(p);
    }
    gtk_selection_data_set_text(sd, s->str, -1);
  }
  g_string_free(s, TRUE);
}
static void pano_bos(GtkClipboard *c, gpointer v) { (void)c; (void)v; }

static void panoya(int kes) {
  GPtrArray *u = g_ptr_array_new();
  for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili) g_ptr_array_add(u, g_file_get_uri(o->dosya)); }
  if (!u->len) { g_ptr_array_free(u, TRUE); return; }
  g_ptr_array_add(u, NULL);
  g_strfreev(pano_urileri); pano_urileri = (char **)g_ptr_array_free(u, FALSE); pano_kes = kes;
  GtkTargetEntry h[] = { { (char *)KOPYALANAN, 0, 0 }, { "text/uri-list", 0, 1 }, { "UTF8_STRING", 0, 2 }, { "text/plain", 0, 2 } };
  gtk_clipboard_set_with_data(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), h, G_N_ELEMENTS(h), pano_ver, pano_bos, NULL);
}

static void dosyalari_aktar(char **uriler, int tasi) {
  GPtrArray *a = g_ptr_array_new_with_free_func(g_free);
  g_ptr_array_add(a, g_strdup(tasi ? "mv" : "cp"));
  if (!tasi) g_ptr_array_add(a, g_strdup("-r"));
  g_ptr_array_add(a, g_strdup("--backup=numbered"));
  g_ptr_array_add(a, g_strdup("--"));
  char *hedef = g_file_get_path(dizin);
  int n = 0;
  for (int i = 0; uriler && uriler[i]; i++) {
    if (!*uriler[i]) continue;
    char *p = g_filename_from_uri(uriler[i], NULL, NULL);
    if (!p) continue;
    char *ust = g_path_get_dirname(p);
    if (tasi && !strcmp(ust, hedef)) { g_free(p); g_free(ust); continue; }   /* zaten burada */
    if (!tasi && !strcmp(ust, hedef)) {
      /* aynı klasöre yapıştır: "ad - Kopya" */
      char *ad = g_path_get_basename(p), *nokta = strrchr(ad, '.');
      char *taban = nokta && nokta != ad && !g_file_test(p, G_FILE_TEST_IS_DIR) ? g_strndup(ad, nokta - ad) : g_strdup(ad);
      const char *uz = nokta && nokta != ad && !g_file_test(p, G_FILE_TEST_IS_DIR) ? nokta : "";
      char *kt = g_strdup_printf("%s - %s", taban, T("Kopya", "Copy"));
      GFile *f = bos_ad(kt, uz);
      if (f) {
        char *hp = g_file_get_path(f);
        const char *argv[] = { "cp", "-r", "--", p, hp, NULL };
        calistir_dizinde(argv);
        g_free(hp); g_object_unref(f);
      }
      g_free(ad); g_free(taban); g_free(kt); g_free(p); g_free(ust);
      continue;
    }
    g_ptr_array_add(a, p); n++;
    g_free(ust);
  }
  g_ptr_array_add(a, g_strdup(hedef));
  g_ptr_array_add(a, NULL);
  if (n) calistir_dizinde((const char *const *)a->pdata);
  g_ptr_array_free(a, TRUE); g_free(hedef);
}

static void yapistir(void) {
  GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  GtkSelectionData *sd = gtk_clipboard_wait_for_contents(c, gdk_atom_intern(KOPYALANAN, FALSE));
  if (sd && gtk_selection_data_get_length(sd) > 0) {
    char *s = g_strndup((const char *)gtk_selection_data_get_data(sd), gtk_selection_data_get_length(sd));
    char **satir = g_strsplit(s, "\n", -1);
    int kes = satir[0] && !strcmp(satir[0], "cut");
    if (satir[0]) dosyalari_aktar(satir + 1, kes);
    if (kes) gtk_clipboard_clear(c);
    g_strfreev(satir); g_free(s);
  } else {
    char **u = gtk_clipboard_wait_for_uris(c);
    if (u) { dosyalari_aktar(u, 0); g_strfreev(u); }
  }
  if (sd) gtk_selection_data_free(sd);
}

static gboolean panoda_dosya_var(void) {
  GtkClipboard *c = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  return gtk_clipboard_wait_is_target_available(c, gdk_atom_intern(KOPYALANAN, FALSE)) || gtk_clipboard_wait_is_uris_available(c);
}

/* ---------- menüler ---------- */
static void m_ac(GtkMenuItem *m, gpointer v) { (void)m; (void)v; secilileri_ac(); }
static void m_kes(GtkMenuItem *m, gpointer v) { (void)m; (void)v; panoya(1); }
static void m_kopyala(GtkMenuItem *m, gpointer v) { (void)m; (void)v; panoya(0); }
static void m_sil(GtkMenuItem *m, gpointer v) { (void)m; (void)v; cope_at(); }
static void m_adlandir(GtkMenuItem *m, gpointer v) { (void)m; (void)v; yeniden_adlandir(); }
static void m_yapistir(GtkMenuItem *m, gpointer v) { (void)m; (void)v; yapistir(); }
static void m_yenile(GtkMenuItem *m, gpointer v) { (void)m; (void)v; yukle(); }
static void m_terminal(GtkMenuItem *m, gpointer v) {
  (void)m; (void)v;
  char *yol = g_file_get_path(dizin);
  const char *a[] = { "aether-terminal", "--working-directory", yol, NULL };
  calistir_dizinde(a); g_free(yol);
}
static void m_klasor_terminal(GtkMenuItem *m, gpointer v) {
  (void)m; (void)v;
  for (unsigned i = 0; i < ogeler->len; i++) {
    Oge *o = ogeler->pdata[i];
    if (!o->secili || !o->klasor) continue;
    char *yol = g_file_get_path(o->dosya);
    const char *a[] = { "aether-terminal", "--working-directory", yol, NULL };
    calistir_dizinde(a); g_free(yol); return;
  }
}
static void m_ayar(GtkMenuItem *m, gpointer v) {
  (void)m;
  const char *a[] = { "aether-ayarlar", "--sayfa", v, NULL };
  calistir_dizinde(a);
}
static void m_gorunum(GtkCheckMenuItem *m, gpointer v) {
  if (!gtk_check_menu_item_get_active(m)) return;
  ae_ayar_yaz("MASAUSTU_SIMGE", v); ayarlari_oku(); yukle();
}
static void m_sira(GtkCheckMenuItem *m, gpointer v) {
  if (!gtk_check_menu_item_get_active(m)) return;
  ae_ayar_yaz("MASAUSTU_SIRA", v); ayarlari_oku(); yukle();
}
static void m_goster(GtkCheckMenuItem *m, gpointer v) {
  (void)v;
  ae_ayar_yaz("MASAUSTU_GOSTER", gtk_check_menu_item_get_active(m) ? "1" : "0");
  ayarlari_oku(); gtk_widget_queue_draw(alan);
}

static GtkWidget *oge(GtkWidget *menu, const char *ad, const char *kisayol, GCallback cb, gpointer v) {
  GtkWidget *o = gtk_menu_item_new();
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
  GtkWidget *l = gtk_label_new(ad); gtk_label_set_xalign(GTK_LABEL(l), 0);
  gtk_box_pack_start(GTK_BOX(k), l, TRUE, TRUE, 0);
  if (kisayol) {
    GtkWidget *ks = gtk_label_new(kisayol);
    gtk_style_context_add_class(gtk_widget_get_style_context(ks), "ae-alt");
    gtk_box_pack_end(GTK_BOX(k), ks, FALSE, FALSE, 0);
  }
  gtk_container_add(GTK_CONTAINER(o), k);
  if (cb) g_signal_connect(o, "activate", cb, v);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), o);
  return o;
}
static void ayrac(GtkWidget *menu) { gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new()); }
static GtkWidget *alt_menu(GtkWidget *menu, const char *ad) {
  GtkWidget *o = gtk_menu_item_new_with_label(ad), *alt = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(o), alt);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), o);
  return alt;
}
static GSList *radyo(GtkWidget *menu, GSList *grup, const char *ad, int etkin, GCallback cb, const char *deger) {
  GtkWidget *o = gtk_radio_menu_item_new_with_label(grup, ad);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(o), etkin);
  g_signal_connect(o, "toggled", cb, (gpointer)deger);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), o);
  return gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(o));
}

static gboolean menu_yok_et(gpointer m) { gtk_widget_destroy(m); return G_SOURCE_REMOVE; }
static void menu_kapandi(GtkMenuShell *m, gpointer v) { (void)v; g_idle_add(menu_yok_et, m); }
static void menu_goster(GtkWidget *m, GdkEvent *e) {
  gtk_widget_show_all(m);
  /* kapanınca yok et; seçilen öğe önce çalışsın diye bir sonraki olay döngüsünde */
  g_signal_connect(m, "deactivate", G_CALLBACK(menu_kapandi), NULL);
  gtk_menu_popup_at_pointer(GTK_MENU(m), e);
}

static void arka_menu(GdkEvent *e) {
  GtkWidget *m = gtk_menu_new();
  GtkWidget *g = alt_menu(m, T("Görünüm", "View"));
  char *s = ae_ayar("MASAUSTU_SIMGE", "orta");
  GSList *gr = NULL;
  gr = radyo(g, gr, T("Büyük simgeler", "Large icons"), !strcmp(s, "buyuk"), G_CALLBACK(m_gorunum), "buyuk");
  gr = radyo(g, gr, T("Orta simgeler", "Medium icons"), !strcmp(s, "orta"), G_CALLBACK(m_gorunum), "orta");
  gr = radyo(g, gr, T("Küçük simgeler", "Small icons"), !strcmp(s, "kucuk"), G_CALLBACK(m_gorunum), "kucuk");
  g_free(s);
  ayrac(g);
  GtkWidget *gs = gtk_check_menu_item_new_with_label(T("Masaüstü simgelerini göster", "Show desktop icons"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(gs), goster);
  g_signal_connect(gs, "toggled", G_CALLBACK(m_goster), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(g), gs);

  GtkWidget *sr = alt_menu(m, T("Sıralama ölçütü", "Sort by"));
  gr = NULL;
  gr = radyo(sr, gr, T("Ad", "Name"), !strcmp(sira, "ad"), G_CALLBACK(m_sira), "ad");
  gr = radyo(sr, gr, T("Boyut", "Size"), !strcmp(sira, "boyut"), G_CALLBACK(m_sira), "boyut");
  gr = radyo(sr, gr, T("Öğe türü", "Item type"), !strcmp(sira, "tur"), G_CALLBACK(m_sira), "tur");
  gr = radyo(sr, gr, T("Değiştirme tarihi", "Date modified"), !strcmp(sira, "tarih"), G_CALLBACK(m_sira), "tarih");
  oge(m, T("Yenile", "Refresh"), "F5", G_CALLBACK(m_yenile), NULL);
  ayrac(m);
  GtkWidget *y = oge(m, T("Yapıştır", "Paste"), "Ctrl+V", G_CALLBACK(m_yapistir), NULL);
  gtk_widget_set_sensitive(y, panoda_dosya_var());
  ayrac(m);
  GtkWidget *yeni = alt_menu(m, T("Yeni", "New"));
  oge(yeni, T("Klasör", "Folder"), NULL, G_CALLBACK(yeni_klasor), NULL);
  oge(yeni, T("Metin Belgesi", "Text Document"), NULL, G_CALLBACK(yeni_metin), NULL);
  ayrac(m);
  oge(m, T("Terminalde aç", "Open in Terminal"), NULL, G_CALLBACK(m_terminal), NULL);
  ayrac(m);
  oge(m, T("Görüntü ayarları", "Display settings"), NULL, G_CALLBACK(m_ayar), "ekran");
  oge(m, T("Kişiselleştir", "Personalize"), NULL, G_CALLBACK(m_ayar), "gorunum");
  menu_goster(m, e);
}

static void oge_menu(GdkEvent *e) {
  GtkWidget *m = gtk_menu_new();
  int tek_klasor = 0;
  for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili && o->klasor) tek_klasor = secili_sayi() == 1; }
  GtkWidget *a = oge(m, T("Aç", "Open"), "Enter", G_CALLBACK(m_ac), NULL);
  gtk_style_context_add_class(gtk_widget_get_style_context(gtk_bin_get_child(GTK_BIN(a))), "kalin");
  if (tek_klasor) oge(m, T("Terminalde aç", "Open in Terminal"), NULL, G_CALLBACK(m_klasor_terminal), NULL);
  ayrac(m);
  oge(m, T("Kes", "Cut"), "Ctrl+X", G_CALLBACK(m_kes), NULL);
  oge(m, T("Kopyala", "Copy"), "Ctrl+C", G_CALLBACK(m_kopyala), NULL);
  ayrac(m);
  oge(m, T("Sil", "Delete"), "Del", G_CALLBACK(m_sil), NULL);
  GtkWidget *r = oge(m, T("Yeniden adlandır", "Rename"), "F2", G_CALLBACK(m_adlandir), NULL);
  gtk_widget_set_sensitive(r, secili_sayi() == 1);
  menu_goster(m, e);
}

/* ---------- fare ve klavye ---------- */
static void odak_al(guint32 t) { gtk_window_present_with_time(GTK_WINDOW(pencere), t); }

static gboolean bas(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)w; (void)v;
  odak_al(e->time);
  int i = oge_bul((int)e->x, (int)e->y);
  Oge *o = i >= 0 ? ogeler->pdata[i] : NULL;
  if (e->type == GDK_2BUTTON_PRESS && e->button == 1) { if (o) { secimi_temizle(); o->secili = 1; oge_ac(o); } return TRUE; }
  if (e->type != GDK_BUTTON_PRESS) return TRUE;
  if (e->button == 1) {
    if (o) {
      if (e->state & GDK_CONTROL_MASK) o->secili = !o->secili;
      else if (!o->secili) { secimi_temizle(); o->secili = 1; }
      basilan = i; surukle_x = (int)e->x; surukle_y = (int)e->y; suruklendi = 0;
    } else {
      if (!(e->state & GDK_CONTROL_MASK)) secimi_temizle();
      kutu = 1; kx1 = kx2 = (int)e->x; ky1 = ky2 = (int)e->y;
    }
    gtk_widget_queue_draw(alan);
  } else if (e->button == 3) {
    if (o) { if (!o->secili) { secimi_temizle(); o->secili = 1; } gtk_widget_queue_draw(alan); oge_menu((GdkEvent *)e); }
    else { secimi_temizle(); gtk_widget_queue_draw(alan); arka_menu((GdkEvent *)e); }
  }
  return TRUE;
}

static gboolean birak(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)w; (void)v;
  if (e->button == 1 && basilan >= 0 && !suruklendi && !(e->state & GDK_CONTROL_MASK)) {
    /* çoklu seçimdeyken tek tıklama yalnızca o öğeyi seçsin */
    Oge *o = ogeler->pdata[basilan];
    secimi_temizle(); o->secili = 1;
  }
  basilan = -1; kutu = 0;
  gtk_widget_queue_draw(alan);
  return TRUE;
}

static GtkTargetList *surukle_hedef;

static gboolean hareket(GtkWidget *w, GdkEventMotion *e, gpointer v) {
  (void)v;
  if (kutu) {
    kx2 = (int)e->x; ky2 = (int)e->y;
    GdkRectangle k = { MIN(kx1, kx2), MIN(ky1, ky2), abs(kx2 - kx1), abs(ky2 - ky1) };
    for (unsigned i = 0; i < ogeler->len; i++) {
      Oge *o = ogeler->pdata[i]; GdkRectangle r; oge_kutusu(o, &r);
      o->secili = gdk_rectangle_intersect(&k, &r, NULL);
    }
    gtk_widget_queue_draw(alan);
  } else if (basilan >= 0 && !suruklendi && gtk_drag_check_threshold(w, surukle_x, surukle_y, (int)e->x, (int)e->y)) {
    /* seçili dosyaları başka bir pencereye sürükle (dosya yöneticisi, tarayıcı...) */
    suruklendi = 1;
    GdkDragContext *c = gtk_drag_begin_with_coordinates(w, surukle_hedef, GDK_ACTION_COPY | GDK_ACTION_MOVE, 1, (GdkEvent *)e, surukle_x, surukle_y);
    Oge *o = ogeler->pdata[basilan];
    if (c && o->pb) gtk_drag_set_icon_pixbuf(c, o->pb, simge_boy / 2, simge_boy / 2);
  }
  return TRUE;
}

static void surukle_veri(GtkWidget *w, GdkDragContext *c, GtkSelectionData *sd, guint bilgi, guint t, gpointer v) {
  (void)w; (void)c; (void)bilgi; (void)t; (void)v;
  GPtrArray *u = g_ptr_array_new();
  for (unsigned i = 0; i < ogeler->len; i++) { Oge *o = ogeler->pdata[i]; if (o->secili) g_ptr_array_add(u, g_file_get_uri(o->dosya)); }
  g_ptr_array_add(u, NULL);
  gtk_selection_data_set_uris(sd, (char **)u->pdata);
  g_strfreev((char **)g_ptr_array_free(u, FALSE));
}

static void birakilan_veri(GtkWidget *w, GdkDragContext *c, int x, int y, GtkSelectionData *sd, guint bilgi, guint t, gpointer v) {
  (void)w; (void)x; (void)y; (void)bilgi; (void)v;
  char **u = gtk_selection_data_get_uris(sd);
  int tasi = gdk_drag_context_get_selected_action(c) == GDK_ACTION_MOVE;
  if (u && gtk_drag_get_source_widget(c) != alan) dosyalari_aktar(u, tasi);
  g_strfreev(u);
  gtk_drag_finish(c, TRUE, FALSE, t);
}

static gboolean tus(GtkWidget *w, GdkEventKey *e, gpointer v) {
  (void)w; (void)v;
  int ctrl = e->state & GDK_CONTROL_MASK;
  switch (e->keyval) {
    case GDK_KEY_Return: case GDK_KEY_KP_Enter: secilileri_ac(); return TRUE;
    case GDK_KEY_Delete: case GDK_KEY_KP_Delete: cope_at(); return TRUE;
    case GDK_KEY_F2: if (secili_sayi() == 1) yeniden_adlandir(); return TRUE;
    case GDK_KEY_F5: yukle(); return TRUE;
    case GDK_KEY_Escape: secimi_temizle(); gtk_widget_queue_draw(alan); return TRUE;
  }
  if (ctrl) {
    switch (gdk_keyval_to_lower(e->keyval)) {
      case GDK_KEY_a: for (unsigned i = 0; i < ogeler->len; i++) ((Oge *)ogeler->pdata[i])->secili = 1; gtk_widget_queue_draw(alan); return TRUE;
      case GDK_KEY_c: panoya(0); return TRUE;
      case GDK_KEY_x: panoya(1); return TRUE;
      case GDK_KEY_v: yapistir(); return TRUE;
    }
  }
  return FALSE;
}

/* ---------- ekran ---------- */
static void ekran_olc(void) {
  GdkScreen *s = gdk_screen_get_default();
  GdkDisplay *d = gdk_display_get_default();
  GdkMonitor *m = gdk_display_get_primary_monitor(d);
  if (!m) m = gdk_display_get_monitor(d, 0);
  sw = gdk_screen_get_width(s); sh = gdk_screen_get_height(s);
  if (m) gdk_monitor_get_workarea(m, &calisma); else calisma = (GdkRectangle){ 0, 0, sw, sh - 40 };
}

static void ekran_degisti(GdkScreen *s, gpointer v) {
  (void)s; (void)v;
  ekran_olc();
  gtk_window_move(GTK_WINDOW(pencere), 0, 0);
  gtk_window_resize(GTK_WINDOW(pencere), sw, sh);
  duvar_yukle(); yerlestir(); gtk_widget_queue_draw(alan);
}

/* görev çubuğu sonradan açılınca çalışma alanı küçülür; simgeleri yeniden yerleştir */
static gboolean calisma_denetle(gpointer v) {
  (void)v;
  GdkRectangle eski = calisma; ekran_olc();
  if (memcmp(&eski, &calisma, sizeof eski)) { yerlestir(); gtk_widget_queue_draw(alan); }
  return G_SOURCE_CONTINUE;
}

static gboolean hup(gpointer v) {
  (void)v;
  ayarlari_oku(); duvar_yukle(); yukle();
  return G_SOURCE_CONTINUE;
}

static GFile *masaustu_dizini(void) {
  const char *d = g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP);
  char *yol = (d && strcmp(d, g_get_home_dir())) ? g_strdup(d)
            : g_build_filename(g_get_home_dir(), ae_en() ? "Desktop" : "Masaüstü", NULL);
  g_mkdir_with_parents(yol, 0755);
  GFile *f = g_file_new_for_path(yol); g_free(yol);
  return f;
}

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);
  ae_css();
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, ".kalin { font-weight: bold; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  dizin = masaustu_dizini();
  ayarlari_oku();
  ekran_olc();
  duvar_yukle();
  yukle();

  pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(pencere), GDK_WINDOW_TYPE_HINT_DESKTOP);
  gtk_window_set_decorated(GTK_WINDOW(pencere), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(pencere), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(pencere), TRUE);
  gtk_window_set_title(GTK_WINDOW(pencere), T("Masaüstü", "Desktop"));
  gtk_window_move(GTK_WINDOW(pencere), 0, 0);
  gtk_window_set_default_size(GTK_WINDOW(pencere), sw, sh);
  gtk_window_stick(GTK_WINDOW(pencere));

  alan = gtk_drawing_area_new();
  gtk_widget_set_can_focus(alan, TRUE);
  gtk_widget_add_events(alan, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_KEY_PRESS_MASK);
  g_signal_connect(alan, "draw", G_CALLBACK(ciz), NULL);
  g_signal_connect(alan, "button-press-event", G_CALLBACK(bas), NULL);
  g_signal_connect(alan, "button-release-event", G_CALLBACK(birak), NULL);
  g_signal_connect(alan, "motion-notify-event", G_CALLBACK(hareket), NULL);
  g_signal_connect(alan, "key-press-event", G_CALLBACK(tus), NULL);

  GtkTargetEntry h[] = { { "text/uri-list", 0, 0 } };
  surukle_hedef = gtk_target_list_new(h, 1);
  g_signal_connect(alan, "drag-data-get", G_CALLBACK(surukle_veri), NULL);
  gtk_drag_dest_set(alan, GTK_DEST_DEFAULT_ALL, h, 1, GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect(alan, "drag-data-received", G_CALLBACK(birakilan_veri), NULL);

  gtk_container_add(GTK_CONTAINER(pencere), alan);
  g_signal_connect(gdk_screen_get_default(), "size-changed", G_CALLBACK(ekran_degisti), NULL);
  g_signal_connect(gdk_screen_get_default(), "monitors-changed", G_CALLBACK(ekran_degisti), NULL);
  g_signal_connect(gtk_icon_theme_get_default(), "changed", G_CALLBACK(yukle), NULL);

  izleyici = g_file_monitor_directory(dizin, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
  if (izleyici) g_signal_connect(izleyici, "changed", G_CALLBACK(dizin_degisti), NULL);
  g_unix_signal_add(SIGHUP, hup, NULL);
  g_timeout_add_seconds(2, calisma_denetle, NULL);

  gtk_widget_show_all(pencere);
  gtk_widget_grab_focus(alan);
  gtk_main();
  return 0;
}
