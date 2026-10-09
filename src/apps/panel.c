/* Yörünge görev çubuğu — Aether'in alt çubuğu
 * Menü düğmesi · açık pencereler · ağ · ses · saat
 */
#define WNCK_I_KNOW_THIS_IS_UNSTABLE
#include "aether.h"
#include "ag.h"
#include <libwnck/libwnck.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <time.h>

#define YUKSEKLIK 34

static GtkWidget *pencere, *gorevler, *ag_dugme, *ag_simge, *ses_dugme, *ses_simge, *saat_dugme, *saat_etiket;
static WnckScreen *ekran;
static GHashTable *dugmeler;     /* WnckWindow* -> GtkWidget* */
static int ses_var, ses_seviye = -1, ses_acik = 1;

static const char *AY_TR[] = { "Ocak", "Şubat", "Mart", "Nisan", "Mayıs", "Haziran", "Temmuz", "Ağustos", "Eylül", "Ekim", "Kasım", "Aralık" };
static const char *GUN_TR[] = { "Pazar", "Pazartesi", "Salı", "Çarşamba", "Perşembe", "Cuma", "Cumartesi" };
static const char *AY_EN[] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char *GUN_EN[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

/* ---------- yerleşim ---------- */
static void strut_ayarla(void) {
  GdkWindow *gw = gtk_widget_get_window(pencere);
  if (!gw) return;
  GdkScreen *s = gdk_screen_get_default();
  /* Xlib'in WidthOfScreen değeri çözünürlük değişince güncellenmiyor; GDK'nınki güncel */
  int sw = gdk_screen_get_width(s), sh = gdk_screen_get_height(s);
  gtk_window_move(GTK_WINDOW(pencere), 0, sh - YUKSEKLIK);
  gtk_window_resize(GTK_WINDOW(pencere), sw, YUKSEKLIK);
  gtk_widget_set_size_request(pencere, sw, YUKSEKLIK);
  long st[12] = { 0, 0, 0, YUKSEKLIK, 0, 0, 0, 0, 0, 0, 0, sw - 1 };
  Display *d = GDK_WINDOW_XDISPLAY(gw); Window w = GDK_WINDOW_XID(gw);
  XChangeProperty(d, w, XInternAtom(d, "_NET_WM_STRUT_PARTIAL", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)st, 12);
  XChangeProperty(d, w, XInternAtom(d, "_NET_WM_STRUT", False), XA_CARDINAL, 32, PropModeReplace, (unsigned char *)st, 4);
}

static gboolean strut_gecikmeli(gpointer d) { (void)d; strut_ayarla(); return G_SOURCE_REMOVE; }
static void boyut_degisti(GdkScreen *s, gpointer d) {
  (void)s; (void)d;
  strut_ayarla();
  /* bazı sürücüler boyutu iki adımda bildiriyor; kısa süre sonra bir kez daha yerleş */
  g_timeout_add(400, strut_gecikmeli, NULL);
}

/* ---------- Aether menü düğmesi: Yörünge'ye mesaj gönder ---------- */
static void yorunge_menu(int x, int y, int tur) {
  Display *d = gdk_x11_get_default_xdisplay();
  XEvent e = { 0 };
  e.xclient.type = ClientMessage;
  e.xclient.window = DefaultRootWindow(d);
  e.xclient.message_type = XInternAtom(d, "_YORUNGE_MENU", False);
  e.xclient.format = 32;
  e.xclient.data.l[0] = x; e.xclient.data.l[1] = y; e.xclient.data.l[2] = tur;
  XSendEvent(d, DefaultRootWindow(d), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
  XFlush(d);
}

static void menu_tik(GtkButton *b, gpointer v) {
  (void)v;
  GtkAllocation a; gtk_widget_get_allocation(GTK_WIDGET(b), &a);
  int ox, oy; gdk_window_get_origin(gtk_widget_get_window(pencere), &ox, &oy);
  yorunge_menu(ox + a.x, oy, 0);
}

/* ---------- görev düğmeleri ---------- */
static void gorev_guncelle(WnckWindow *w) {
  GtkWidget *b = g_hash_table_lookup(dugmeler, w);
  if (!b) return;
  GtkWidget *etiket = g_object_get_data(G_OBJECT(b), "etiket");
  GtkWidget *simge = g_object_get_data(G_OBJECT(b), "simge");
  const char *ad = wnck_window_get_name(w);
  gtk_label_set_text(GTK_LABEL(etiket), ad && *ad ? ad : "Aether");
  gtk_widget_set_tooltip_text(b, ad);
  GdkPixbuf *ik = wnck_window_get_mini_icon(w);
  if (ik) {
    int ol = gtk_widget_get_scale_factor(b);
    GdkPixbuf *k = gdk_pixbuf_scale_simple(ik, 16 * ol, 16 * ol, GDK_INTERP_BILINEAR);
    cairo_surface_t *y = gdk_cairo_surface_create_from_pixbuf(k, ol, NULL);
    gtk_image_set_from_surface(GTK_IMAGE(simge), y);
    cairo_surface_destroy(y); g_object_unref(k);
  }
  GtkStyleContext *sc = gtk_widget_get_style_context(b);
  if (wnck_window_is_minimized(w)) gtk_style_context_add_class(sc, "kucuk"); else gtk_style_context_remove_class(sc, "kucuk");
  if (wnck_window_is_active(w)) gtk_style_context_add_class(sc, "etkin"); else gtk_style_context_remove_class(sc, "etkin");
  if (wnck_window_needs_attention(w)) gtk_style_context_add_class(sc, "dikkat"); else gtk_style_context_remove_class(sc, "dikkat");
}

static void hepsini_guncelle(void) {
  GHashTableIter it; gpointer k, v;
  g_hash_table_iter_init(&it, dugmeler);
  while (g_hash_table_iter_next(&it, &k, &v)) gorev_guncelle(k);
}

static void gorev_tik(GtkButton *b, gpointer v) {
  (void)b;
  WnckWindow *w = v;
  guint32 t = gtk_get_current_event_time();
  if (wnck_window_is_active(w) && !wnck_window_is_minimized(w)) wnck_window_minimize(w);
  else wnck_window_activate(w, t);
}

static void m_kucult(GtkMenuItem *i, gpointer v) {
  (void)i; WnckWindow *w = v;
  if (wnck_window_is_minimized(w)) wnck_window_activate(w, gtk_get_current_event_time()); else wnck_window_minimize(w);
}
static void m_buyut(GtkMenuItem *i, gpointer v) {
  (void)i; WnckWindow *w = v;
  if (wnck_window_is_maximized(w)) wnck_window_unmaximize(w); else wnck_window_maximize(w);
}
static void m_kapat(GtkMenuItem *i, gpointer v) { (void)i; wnck_window_close(v, gtk_get_current_event_time()); }

static gboolean gorev_bas(GtkWidget *b, GdkEventButton *e, gpointer v) {
  (void)b;
  WnckWindow *w = v;
  if (e->type != GDK_BUTTON_PRESS) return FALSE;
  if (e->button == 2) { wnck_window_close(w, e->time); return TRUE; }
  if (e->button != 3) return FALSE;
  GtkWidget *m = gtk_menu_new(), *o;
  o = gtk_menu_item_new_with_label(wnck_window_is_minimized(w) ? T("Geri Getir", "Restore") : T("Küçült", "Minimize"));
  g_signal_connect(o, "activate", G_CALLBACK(m_kucult), w); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  o = gtk_menu_item_new_with_label(wnck_window_is_maximized(w) ? T("Önceki Boyut", "Restore Size") : T("Büyüt", "Maximize"));
  g_signal_connect(o, "activate", G_CALLBACK(m_buyut), w); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
  o = gtk_menu_item_new_with_label(T("Kapat", "Close"));
  g_signal_connect(o, "activate", G_CALLBACK(m_kapat), w); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  gtk_widget_show_all(m);
  g_signal_connect(m, "deactivate", G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_menu_popup_at_widget(GTK_MENU(m), GTK_WIDGET(b), GDK_GRAVITY_NORTH_WEST, GDK_GRAVITY_SOUTH_WEST, (GdkEvent *)e);
  return TRUE;
}

/* görev çubuğunun boş yerine sağ tık (Windows gibi) */
static void m_gorev_yon(GtkMenuItem *i, gpointer v) { (void)i; (void)v; g_spawn_command_line_async("aether-gorev", NULL); }
static void m_masaustu(GtkMenuItem *i, gpointer v) {
  (void)i; (void)v;
  WnckScreen *s = wnck_screen_get_default();
  wnck_screen_toggle_showing_desktop(s, !wnck_screen_get_showing_desktop(s));
}
static void m_ayarlar(GtkMenuItem *i, gpointer v) { (void)i; (void)v; g_spawn_command_line_async("aether-ayarlar", NULL); }
static gboolean panel_bas(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)w; (void)v;
  if (e->type != GDK_BUTTON_PRESS || e->button != 3) return FALSE;
  GtkWidget *m = gtk_menu_new(), *o;
  o = gtk_menu_item_new_with_label(T("Görev Yöneticisi", "Task Manager"));
  g_signal_connect(o, "activate", G_CALLBACK(m_gorev_yon), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
  o = gtk_menu_item_new_with_label(T("Masaüstünü göster", "Show the desktop"));
  g_signal_connect(o, "activate", G_CALLBACK(m_masaustu), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  o = gtk_menu_item_new_with_label(T("Ayarlar", "Settings"));
  g_signal_connect(o, "activate", G_CALLBACK(m_ayarlar), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  gtk_widget_show_all(m);
  gtk_menu_popup_at_pointer(GTK_MENU(m), (GdkEvent *)e);
  return TRUE;
}

static void w_degisti(WnckWindow *w, gpointer v) { (void)v; gorev_guncelle(w); }
static void w_durum(WnckWindow *w, WnckWindowState a, WnckWindowState b, gpointer v) { (void)a; (void)b; (void)v; gorev_guncelle(w); }

static gboolean listelenir(WnckWindow *w) {
  WnckWindowType t = wnck_window_get_window_type(w);
  return !wnck_window_is_skip_tasklist(w) && t != WNCK_WINDOW_DOCK && t != WNCK_WINDOW_DESKTOP &&
         t != WNCK_WINDOW_SPLASHSCREEN && t != WNCK_WINDOW_MENU && t != WNCK_WINDOW_TOOLBAR;
}

static void pencere_acildi(WnckScreen *s, WnckWindow *w, gpointer v) {
  (void)s; (void)v;
  if (!listelenir(w) || g_hash_table_lookup(dugmeler, w)) return;
  GtkWidget *b = gtk_button_new();
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 7);
  GtkWidget *simge = gtk_image_new();
  GtkWidget *etiket = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(etiket), PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars(GTK_LABEL(etiket), 22);
  gtk_label_set_xalign(GTK_LABEL(etiket), 0);
  gtk_box_pack_start(GTK_BOX(k), simge, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), etiket, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(b), k);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "gorev");
  gtk_widget_set_focus_on_click(b, FALSE);
  g_object_set_data(G_OBJECT(b), "etiket", etiket);
  g_object_set_data(G_OBJECT(b), "simge", simge);
  g_signal_connect(b, "clicked", G_CALLBACK(gorev_tik), w);
  g_signal_connect(b, "button-press-event", G_CALLBACK(gorev_bas), w);
  g_signal_connect_object(w, "name-changed", G_CALLBACK(w_degisti), NULL, 0);
  g_signal_connect_object(w, "icon-changed", G_CALLBACK(w_degisti), NULL, 0);
  g_signal_connect_object(w, "state-changed", G_CALLBACK(w_durum), NULL, 0);
  g_hash_table_insert(dugmeler, w, b);
  gtk_box_pack_start(GTK_BOX(gorevler), b, FALSE, FALSE, 0);
  gtk_widget_show_all(b);
  gorev_guncelle(w);
}

static void pencere_kapandi(WnckScreen *s, WnckWindow *w, gpointer v) {
  (void)s; (void)v;
  GtkWidget *b = g_hash_table_lookup(dugmeler, w);
  if (b) { g_hash_table_remove(dugmeler, w); gtk_widget_destroy(b); }
}

static void etkin_degisti(WnckScreen *s, WnckWindow *eski, gpointer v) { (void)s; (void)eski; (void)v; hepsini_guncelle(); }

/* ---------- açılır pencere (GTK3 açılır kutuları ince çubuğun dışına taşamıyor) ---------- */
static GtkWidget *acilir;

static void acilir_kapat(void) {
  if (!acilir) return;
  GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
  gdk_seat_ungrab(seat);
  gtk_widget_destroy(acilir);
  acilir = NULL;
}

static gboolean acilir_tik(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)v;
  GtkAllocation a; gtk_widget_get_allocation(w, &a);
  if (e->x < 0 || e->y < 0 || e->x >= a.width || e->y >= a.height) { acilir_kapat(); return TRUE; }
  return FALSE;
}

static gboolean acilir_tus(GtkWidget *w, GdkEventKey *e, gpointer v) {
  (void)w; (void)v;
  if (e->keyval == GDK_KEY_Escape) { acilir_kapat(); return TRUE; }
  return FALSE;
}

static void acilir_ac(GtkWidget *dugme, GtkWidget *icerik) {
  acilir_kapat();
  acilir = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_type_hint(GTK_WINDOW(acilir), GDK_WINDOW_TYPE_HINT_POPUP_MENU);
  gtk_style_context_add_class(gtk_widget_get_style_context(acilir), "acilir");
  gtk_container_add(GTK_CONTAINER(acilir), icerik);
  gtk_widget_show_all(icerik);
  GtkRequisition r; gtk_widget_get_preferred_size(acilir, NULL, &r);
  GtkAllocation a; gtk_widget_get_allocation(dugme, &a);
  int ox, oy; gdk_window_get_origin(gtk_widget_get_window(pencere), &ox, &oy);
  int sw = gdk_screen_get_width(gdk_screen_get_default());
  int x = ox + a.x + a.width - r.width;
  if (x + r.width > sw - 4) x = sw - 4 - r.width;
  if (x < 4) x = 4;
  gtk_window_move(GTK_WINDOW(acilir), x, oy - r.height - 4);
  gtk_widget_add_events(acilir, GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK);
  g_signal_connect(acilir, "button-press-event", G_CALLBACK(acilir_tik), NULL);
  g_signal_connect(acilir, "key-press-event", G_CALLBACK(acilir_tus), NULL);
  gtk_widget_show(acilir);
  GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
  gdk_seat_grab(seat, gtk_widget_get_window(acilir), GDK_SEAT_CAPABILITY_ALL, TRUE, NULL, NULL, NULL, NULL);
}

/* ---------- ağ ---------- */
static gboolean ag_guncelle(gpointer v) {
  (void)v;
  AgArayuz a[8];
  int n = ag_arayuzler(a, 8);
  const char *simge = "network-offline-symbolic";
  GString *ip = g_string_new(NULL);
  int kablolu = 0, kablosuz = 0;
  for (int i = 0; i < n; i++) {
    if (!a[i].bagli) continue;
    if (a[i].kablosuz) kablosuz = 1; else kablolu = 1;
    if (a[i].ip[0]) g_string_append_printf(ip, "\n%s: %s", a[i].arayuz, a[i].ip);
  }
  char ssid[128] = ""; int guc = 0;
  if (kablosuz) ag_bagli_kablosuz(ssid, sizeof ssid, &guc);
  char *ipucu;
  if (kablolu) { simge = "network-wired-symbolic"; ipucu = g_strdup_printf("%s%s", T("Kablolu ağa bağlı", "Connected (wired)"), ip->str); }
  else if (kablosuz) {
    simge = guc > 75 ? "network-wireless-signal-excellent-symbolic" : guc > 50 ? "network-wireless-signal-good-symbolic" :
            guc > 25 ? "network-wireless-signal-ok-symbolic" : "network-wireless-signal-weak-symbolic";
    ipucu = g_strdup_printf("%s %s (%%%d)%s", T("Bağlı:", "Connected:"), ssid[0] ? ssid : "Wi-Fi", guc, ip->str);
  } else ipucu = g_strdup(T("Bağlı değil — ağ ayarları için tıkla", "Not connected — click for network settings"));
  gtk_image_set_from_icon_name(GTK_IMAGE(ag_simge), simge, GTK_ICON_SIZE_MENU);
  gtk_widget_set_tooltip_text(ag_dugme, ipucu);
  g_free(ipucu); g_string_free(ip, TRUE);
  return G_SOURCE_CONTINUE;
}

static void ag_tik(GtkButton *b, gpointer v) { (void)b; (void)v; g_spawn_command_line_async("aether-ayarlar --sayfa ag", NULL); }

/* ---------- ses (amixer varsa) ---------- */
static void ses_oku(void) {
  char *cikti = NULL;
  if (!g_spawn_command_line_sync("amixer get Master", &cikti, NULL, NULL, NULL) || !cikti) { ses_seviye = -1; return; }
  char *p = strchr(cikti, '[');
  ses_seviye = -1;
  if (p) ses_seviye = atoi(p + 1);
  ses_acik = strstr(cikti, "[off]") == NULL;
  g_free(cikti);
}

static void ses_simgesi(void) {
  const char *s = ses_seviye < 0 ? "audio-volume-muted-symbolic" : !ses_acik || ses_seviye == 0 ? "audio-volume-muted-symbolic" :
                  ses_seviye < 34 ? "audio-volume-low-symbolic" : ses_seviye < 67 ? "audio-volume-medium-symbolic" : "audio-volume-high-symbolic";
  gtk_image_set_from_icon_name(GTK_IMAGE(ses_simge), s, GTK_ICON_SIZE_MENU);
  char *t = ses_seviye < 0 ? g_strdup(T("Ses aygıtı yok", "No sound device")) :
            g_strdup_printf("%s %%%d%s", T("Ses:", "Volume:"), ses_seviye, ses_acik ? "" : T(" (kapalı)", " (muted)"));
  gtk_widget_set_tooltip_text(ses_dugme, t); g_free(t);
}

static void ses_ayarla(const char *arg) {
  char *k = g_strdup_printf("amixer -q set Master %s", arg);
  g_spawn_command_line_sync(k, NULL, NULL, NULL, NULL); g_free(k);
  ses_oku(); ses_simgesi();
}

static gboolean ses_kaydir(GtkWidget *w, GdkEventScroll *e, gpointer v) {
  (void)w; (void)v;
  if (e->direction == GDK_SCROLL_UP) ses_ayarla("5%+ unmute");
  else if (e->direction == GDK_SCROLL_DOWN) ses_ayarla("5%-");
  return TRUE;
}

static void ses_kaydirac(GtkRange *r, gpointer v) {
  (void)v;
  char a[32]; snprintf(a, sizeof a, "%d%% unmute", (int)gtk_range_get_value(r)); ses_ayarla(a);
}
static void ses_sustur(GtkToggleButton *t, gpointer v) { (void)v; ses_ayarla(gtk_toggle_button_get_active(t) ? "mute" : "unmute"); }

static void ses_tik(GtkButton *b, gpointer v) {
  (void)v;
  if (acilir) { acilir_kapat(); return; }
  ses_oku(); ses_simgesi();
  if (ses_seviye < 0) return;
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(k), 14);
  GtkWidget *s = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 5);
  gtk_widget_set_size_request(s, 200, -1);
  gtk_range_set_value(GTK_RANGE(s), ses_seviye);
  gtk_scale_set_draw_value(GTK_SCALE(s), TRUE);
  gtk_scale_set_value_pos(GTK_SCALE(s), GTK_POS_RIGHT);
  g_signal_connect(s, "value-changed", G_CALLBACK(ses_kaydirac), NULL);
  GtkWidget *t = gtk_check_button_new_with_label(T("Sesi kapat", "Mute"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(t), !ses_acik);
  g_signal_connect(t, "toggled", G_CALLBACK(ses_sustur), NULL);
  gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Ses", "Volume"), NULL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), s, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), t, FALSE, FALSE, 0);
  acilir_ac(GTK_WIDGET(b), k);
}

/* ---------- saat ---------- */
static gboolean saat_guncelle(gpointer v) {
  (void)v;
  time_t t = time(NULL); struct tm *z = localtime(&t);
  char s[16]; strftime(s, sizeof s, "%H:%M", z);
  gtk_label_set_text(GTK_LABEL(saat_etiket), s);
  char *uzun = g_strdup_printf("%d %s %d, %s", z->tm_mday, ae_en() ? AY_EN[z->tm_mon] : AY_TR[z->tm_mon], z->tm_year + 1900,
                               ae_en() ? GUN_EN[z->tm_wday] : GUN_TR[z->tm_wday]);
  gtk_widget_set_tooltip_text(saat_dugme, uzun); g_free(uzun);
  g_timeout_add_seconds(60 - z->tm_sec, saat_guncelle, NULL);
  return G_SOURCE_REMOVE;
}

/* kendi küçük takvimimiz: GTK'nınki ay adlarını sistem diline göre yazamıyor */
static int takvim_ay, takvim_yil;
static GtkWidget *takvim_izgara, *takvim_baslik;

static void takvim_ciz(void) {
  GList *c = gtk_container_get_children(GTK_CONTAINER(takvim_izgara));
  for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
  g_list_free(c);
  char *b = g_strdup_printf("%s %d", ae_en() ? AY_EN[takvim_ay] : AY_TR[takvim_ay], takvim_yil);
  gtk_label_set_text(GTK_LABEL(takvim_baslik), b); g_free(b);
  const char *gk_tr[] = { "Pt", "Sa", "Ça", "Pe", "Cu", "Ct", "Pz" }, *gk_en[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
  for (int i = 0; i < 7; i++) {
    GtkWidget *e = gtk_label_new(ae_en() ? gk_en[i] : gk_tr[i]);
    gtk_style_context_add_class(gtk_widget_get_style_context(e), "dim-label");
    gtk_grid_attach(GTK_GRID(takvim_izgara), e, i, 0, 1, 1);
  }
  struct tm ilk = { 0 }; ilk.tm_year = takvim_yil - 1900; ilk.tm_mon = takvim_ay; ilk.tm_mday = 1; ilk.tm_hour = 12;
  mktime(&ilk);
  int bas = (ilk.tm_wday + 6) % 7;
  int gunler = g_date_get_days_in_month(takvim_ay + 1, takvim_yil);
  time_t t = time(NULL); struct tm *bugun = localtime(&t);
  for (int g = 1; g <= gunler; g++) {
    char s[4]; snprintf(s, sizeof s, "%d", g);
    GtkWidget *e = gtk_label_new(s);
    gtk_widget_set_size_request(e, 30, 24);
    if (g == bugun->tm_mday && takvim_ay == bugun->tm_mon && takvim_yil == bugun->tm_year + 1900)
      gtk_style_context_add_class(gtk_widget_get_style_context(e), "bugun");
    int k = bas + g - 1;
    gtk_grid_attach(GTK_GRID(takvim_izgara), e, k % 7, 1 + k / 7, 1, 1);
  }
  gtk_widget_show_all(takvim_izgara);
}

static void takvim_kaydir(GtkButton *b, gpointer v) {
  (void)b;
  takvim_ay += GPOINTER_TO_INT(v);
  if (takvim_ay < 0) { takvim_ay = 11; takvim_yil--; }
  if (takvim_ay > 11) { takvim_ay = 0; takvim_yil++; }
  takvim_ciz();
}

static void saat_tik(GtkButton *b, gpointer v) {
  (void)v;
  if (acilir) { acilir_kapat(); return; }
  time_t t = time(NULL); struct tm *z = localtime(&t);
  takvim_ay = z->tm_mon; takvim_yil = z->tm_year + 1900;
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(k), 14);
  char *uzun = g_strdup_printf("%s, %d %s", ae_en() ? GUN_EN[z->tm_wday] : GUN_TR[z->tm_wday], z->tm_mday, ae_en() ? AY_EN[z->tm_mon] : AY_TR[z->tm_mon]);
  GtkWidget *gun = gtk_label_new(uzun); g_free(uzun);
  gtk_style_context_add_class(gtk_widget_get_style_context(gun), "takvim-gun");
  gtk_label_set_xalign(GTK_LABEL(gun), 0);
  GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *geri = gtk_button_new_from_icon_name("go-previous-symbolic", GTK_ICON_SIZE_MENU);
  GtkWidget *ileri = gtk_button_new_from_icon_name("go-next-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief(GTK_BUTTON(geri), GTK_RELIEF_NONE); gtk_button_set_relief(GTK_BUTTON(ileri), GTK_RELIEF_NONE);
  takvim_baslik = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(ust), geri, FALSE, FALSE, 0);
  gtk_box_set_center_widget(GTK_BOX(ust), takvim_baslik);
  gtk_box_pack_end(GTK_BOX(ust), ileri, FALSE, FALSE, 0);
  g_signal_connect(geri, "clicked", G_CALLBACK(takvim_kaydir), GINT_TO_POINTER(-1));
  g_signal_connect(ileri, "clicked", G_CALLBACK(takvim_kaydir), GINT_TO_POINTER(1));
  takvim_izgara = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(takvim_izgara), TRUE);
  gtk_box_pack_start(GTK_BOX(k), gun, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), ust, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), takvim_izgara, FALSE, FALSE, 0);
  takvim_ciz();
  acilir_ac(GTK_WIDGET(b), k);
}

/* ---------- kurulum ---------- */
static GtkWidget *simge_dugme(const char *simge_adi, GtkWidget **simge) {
  GtkWidget *b = gtk_button_new();
  *simge = gtk_image_new_from_icon_name(simge_adi, GTK_ICON_SIZE_MENU);
  gtk_container_add(GTK_CONTAINER(b), *simge);
  gtk_widget_set_focus_on_click(b, FALSE);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "tepsi");
  return b;
}

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);
  g_set_prgname("yorunge-panel");
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,
    ".aether-panel { background-color: @theme_bg_color; border-top: 1px solid alpha(@theme_fg_color, 0.14); }"
    ".aether-panel button { border: none; border-radius: 0; background: none; box-shadow: none; padding: 0 9px; min-height: 0; margin: 0; }"
    ".aether-panel button:hover { background-color: alpha(@theme_fg_color, 0.07); }"
    ".aether-panel button:active { background-color: alpha(@theme_selected_bg_color, 0.25); }"
    ".aether-panel .menu-dugme { padding: 0 10px; }"
    ".aether-panel .gorev { padding: 0 10px; border-bottom: 2px solid transparent; }"
    ".aether-panel .gorev.etkin { background-color: alpha(@theme_selected_bg_color, 0.14); border-bottom: 2px solid @theme_selected_bg_color; }"
    ".aether-panel .gorev.kucuk label, .aether-panel .gorev.kucuk image { opacity: 0.5; }"
    ".aether-panel .gorev.dikkat { background-color: alpha(#e5a50a, 0.25); }"
    ".aether-panel .saat { font-weight: bold; padding: 0 12px; }"
    ".bugun { background-color: @theme_selected_bg_color; color: #ffffff; font-weight: bold; }"
    ".takvim-gun { font-weight: bold; font-size: 11pt; }"
    ".acilir { background-color: @theme_base_color; border: 1px solid #7c6ad6; }",
    -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_type_hint(GTK_WINDOW(pencere), GDK_WINDOW_TYPE_HINT_DOCK);
  gtk_window_set_decorated(GTK_WINDOW(pencere), FALSE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(pencere), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(pencere), TRUE);
  gtk_window_stick(GTK_WINDOW(pencere));
  gtk_window_set_accept_focus(GTK_WINDOW(pencere), FALSE);
  gtk_window_set_title(GTK_WINDOW(pencere), "Yörünge");
  gtk_style_context_add_class(gtk_widget_get_style_context(pencere), "aether-panel");

  GtkWidget *kutu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add(GTK_CONTAINER(pencere), kutu);

  /* menü düğmesi */
  GtkWidget *mb = gtk_button_new();
  GtkWidget *mk = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GdkPixbuf *logo = gdk_pixbuf_new_from_file_at_size("/usr/share/icons/hicolor/48x48/apps/aether.png", 22, 22, NULL);
  if (logo) { gtk_box_pack_start(GTK_BOX(mk), gtk_image_new_from_pixbuf(logo), FALSE, FALSE, 0); g_object_unref(logo); }
  gtk_container_add(GTK_CONTAINER(mb), mk);
  gtk_style_context_add_class(gtk_widget_get_style_context(mb), "menu-dugme");
  gtk_widget_set_focus_on_click(mb, FALSE);
  gtk_widget_set_tooltip_text(mb, T("Başlat (Super)", "Start (Super)"));
  g_signal_connect(mb, "clicked", G_CALLBACK(menu_tik), NULL);
  gtk_box_pack_start(GTK_BOX(kutu), mb, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(kutu), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

  /* pencereler */
  gorevler = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_box_pack_start(GTK_BOX(kutu), gorevler, TRUE, TRUE, 4);

  /* tepsi: ağ, ses, saat */
  ag_dugme = simge_dugme("network-offline-symbolic", &ag_simge);
  g_signal_connect(ag_dugme, "clicked", G_CALLBACK(ag_tik), NULL);
  ses_var = g_find_program_in_path("amixer") != NULL;
  ses_dugme = simge_dugme("audio-volume-medium-symbolic", &ses_simge);
  g_signal_connect(ses_dugme, "clicked", G_CALLBACK(ses_tik), NULL);
  gtk_widget_add_events(ses_dugme, GDK_SCROLL_MASK);
  g_signal_connect(ses_dugme, "scroll-event", G_CALLBACK(ses_kaydir), NULL);
  saat_dugme = gtk_button_new();
  saat_etiket = gtk_label_new("--:--");
  gtk_container_add(GTK_CONTAINER(saat_dugme), saat_etiket);
  gtk_style_context_add_class(gtk_widget_get_style_context(saat_dugme), "saat");
  gtk_widget_set_focus_on_click(saat_dugme, FALSE);
  g_signal_connect(saat_dugme, "clicked", G_CALLBACK(saat_tik), NULL);
  gtk_box_pack_end(GTK_BOX(kutu), saat_dugme, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(kutu), ses_dugme, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(kutu), ag_dugme, FALSE, FALSE, 0);

  gtk_widget_add_events(pencere, GDK_BUTTON_PRESS_MASK);
  g_signal_connect(pencere, "button-press-event", G_CALLBACK(panel_bas), NULL);
  gtk_widget_realize(pencere);
  strut_ayarla();
  g_signal_connect(gdk_screen_get_default(), "size-changed", G_CALLBACK(boyut_degisti), NULL);
  g_signal_connect(gdk_screen_get_default(), "monitors-changed", G_CALLBACK(boyut_degisti), NULL);
  gtk_widget_show_all(pencere);
  if (ses_var) { ses_oku(); ses_simgesi(); }
  /* amixer yoksa ya da ses kartı yoksa simgeyi gösterme */
  if (!ses_var || ses_seviye < 0) gtk_widget_hide(ses_dugme);

  dugmeler = g_hash_table_new(NULL, NULL);
  ekran = wnck_screen_get_default();
  wnck_screen_force_update(ekran);
  for (GList *l = wnck_screen_get_windows(ekran); l; l = l->next) pencere_acildi(ekran, l->data, NULL);
  g_signal_connect(ekran, "window-opened", G_CALLBACK(pencere_acildi), NULL);
  g_signal_connect(ekran, "window-closed", G_CALLBACK(pencere_kapandi), NULL);
  g_signal_connect(ekran, "active-window-changed", G_CALLBACK(etkin_degisti), NULL);

  saat_guncelle(NULL);
  ag_guncelle(NULL);
  g_timeout_add_seconds(4, ag_guncelle, NULL);
  gtk_main();
  return 0;
}
