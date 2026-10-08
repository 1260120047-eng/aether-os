/* Aether bildirimleri — org.freedesktop.Notifications sunucusu
 * Sağ altta, görev çubuğunun üstünde küçük kutular gösterir.
 * D-Bus etkinleştirmesiyle ilk bildirimde kendiliğinden başlar.
 */
#include "aether.h"
#include <gio/gio.h>

#define GENISLIK 340
#define KENAR 12
#define PANEL 34
#define EN_FAZLA 5

typedef struct {
  guint32 id;
  GtkWidget *pencere;
  guint zaman;
  gboolean varsayilan_eylem;
} Bildirim;

static GDBusConnection *bag;
static GList *bildirimler;     /* en yeni başta */
static guint32 sonraki_id = 1;

static const char *XML =
  "<node><interface name='org.freedesktop.Notifications'>"
  "<method name='GetCapabilities'><arg type='as' direction='out'/></method>"
  "<method name='Notify'>"
  "<arg type='s' direction='in'/><arg type='u' direction='in'/><arg type='s' direction='in'/>"
  "<arg type='s' direction='in'/><arg type='s' direction='in'/><arg type='as' direction='in'/>"
  "<arg type='a{sv}' direction='in'/><arg type='i' direction='in'/><arg type='u' direction='out'/></method>"
  "<method name='CloseNotification'><arg type='u' direction='in'/></method>"
  "<method name='GetServerInformation'><arg type='s' direction='out'/><arg type='s' direction='out'/>"
  "<arg type='s' direction='out'/><arg type='s' direction='out'/></method>"
  "<signal name='NotificationClosed'><arg type='u'/><arg type='u'/></signal>"
  "<signal name='ActionInvoked'><arg type='u'/><arg type='s'/></signal>"
  "</interface></node>";

static void yerlestir(void) {
  GdkScreen *e = gdk_screen_get_default();
  int sw = gdk_screen_get_width(e), sh = gdk_screen_get_height(e);
  int y = sh - PANEL - KENAR;
  for (GList *l = bildirimler; l; l = l->next) {
    Bildirim *b = l->data;
    int w, h; gtk_window_get_size(GTK_WINDOW(b->pencere), &w, &h);
    y -= h;
    gtk_window_move(GTK_WINDOW(b->pencere), sw - GENISLIK - KENAR, y);
    y -= 8;
  }
}

static Bildirim *bul(guint32 id) {
  for (GList *l = bildirimler; l; l = l->next) if (((Bildirim *)l->data)->id == id) return l->data;
  return NULL;
}

/* neden: 1 süre doldu, 2 kullanıcı kapattı, 3 CloseNotification */
static void kapat(Bildirim *b, guint32 neden) {
  if (!g_list_find(bildirimler, b)) return;
  bildirimler = g_list_remove(bildirimler, b);
  if (b->zaman) g_source_remove(b->zaman);
  gtk_widget_destroy(b->pencere);
  g_dbus_connection_emit_signal(bag, NULL, "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
                                "NotificationClosed", g_variant_new("(uu)", b->id, neden), NULL);
  g_free(b);
  yerlestir();
}

static gboolean sure_doldu(gpointer v) { Bildirim *b = v; b->zaman = 0; kapat(b, 1); return G_SOURCE_REMOVE; }

static gboolean tiklandi(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)w; (void)e;
  Bildirim *b = v;
  if (b->varsayilan_eylem)
    g_dbus_connection_emit_signal(bag, NULL, "/org/freedesktop/Notifications", "org.freedesktop.Notifications",
                                  "ActionInvoked", g_variant_new("(us)", b->id, "default"), NULL);
  kapat(b, 2);
  return TRUE;
}

static void kapat_dugme(GtkButton *d, gpointer v) { (void)d; kapat(v, 2); }

/* üzerine gelince süre dursun */
static gboolean uzerinde(GtkWidget *w, GdkEventCrossing *e, gpointer v) {
  (void)w;
  Bildirim *b = v;
  if (e->detail == GDK_NOTIFY_INFERIOR) return FALSE;
  if (e->type == GDK_ENTER_NOTIFY) { if (b->zaman) { g_source_remove(b->zaman); b->zaman = 0; } }
  else if (!b->zaman) b->zaman = g_timeout_add(3000, sure_doldu, b);
  return FALSE;
}

static GtkWidget *simge_yap(const char *ad, GVariant *ipuclari) {
  GtkWidget *im = NULL;
  /* uygulama görüntü verisi (image-data / icon_data) */
  GVariant *v = g_variant_lookup_value(ipuclari, "image-data", NULL);
  if (!v) v = g_variant_lookup_value(ipuclari, "image_data", NULL);
  if (!v) v = g_variant_lookup_value(ipuclari, "icon_data", NULL);
  if (v && g_variant_is_of_type(v, G_VARIANT_TYPE("(iiibiiay)"))) {
    gint w, h, st, bps, kanal; gboolean alfa; GVariant *veri;
    g_variant_get(v, "(iiibii@ay)", &w, &h, &st, &alfa, &bps, &kanal, &veri);
    gsize n; const guchar *p = g_variant_get_fixed_array(veri, &n, 1);
    if (n >= (gsize)(st * (h - 1) + w * kanal)) {
      GdkPixbuf *pb = gdk_pixbuf_new_from_data(g_memdup2(p, n), GDK_COLORSPACE_RGB, alfa, bps, w, h, st, (GdkPixbufDestroyNotify)g_free, NULL);
      GdkPixbuf *k = gdk_pixbuf_scale_simple(pb, 40, 40, GDK_INTERP_BILINEAR);
      im = gtk_image_new_from_pixbuf(k);
      g_object_unref(k); g_object_unref(pb);
    }
    g_variant_unref(veri);
  }
  if (v) g_variant_unref(v);
  if (im) return im;
  if (ad && *ad) {
    if (g_str_has_prefix(ad, "file://")) ad += 7;
    if (ad[0] == '/') {
      GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(ad, 40, 40, NULL);
      if (pb) { im = gtk_image_new_from_pixbuf(pb); g_object_unref(pb); return im; }
    } else if (gtk_icon_theme_has_icon(gtk_icon_theme_get_default(), ad)) {
      im = gtk_image_new_from_icon_name(ad, GTK_ICON_SIZE_DIALOG);
      gtk_image_set_pixel_size(GTK_IMAGE(im), 40);
      return im;
    }
  }
  return ae_logo(40);
}

static guint32 goster(const char *uygulama, guint32 yerine, const char *simge, const char *baslik, const char *govde,
                      const char **eylemler, GVariant *ipuclari, gint sure) {
  Bildirim *b = yerine ? bul(yerine) : NULL;
  if (b) {      /* aynı bildirimi güncelle: eskisini sessizce kaldır */
    bildirimler = g_list_remove(bildirimler, b);
    if (b->zaman) g_source_remove(b->zaman);
    gtk_widget_destroy(b->pencere);
  } else {
    b = g_new0(Bildirim, 1);
    b->id = sonraki_id++;
  }
  b->varsayilan_eylem = FALSE;
  for (int i = 0; eylemler && eylemler[i] && eylemler[i + 1]; i += 2) if (!strcmp(eylemler[i], "default")) b->varsayilan_eylem = TRUE;

  guchar aciliyet = 1;
  GVariant *u = g_variant_lookup_value(ipuclari, "urgency", G_VARIANT_TYPE_BYTE);
  if (u) { aciliyet = g_variant_get_byte(u); g_variant_unref(u); }

  GtkWidget *p = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_type_hint(GTK_WINDOW(p), GDK_WINDOW_TYPE_HINT_NOTIFICATION);
  gtk_widget_set_size_request(p, GENISLIK, -1);
  gtk_style_context_add_class(gtk_widget_get_style_context(p), "bildirim");
  if (aciliyet == 2) gtk_style_context_add_class(gtk_widget_get_style_context(p), "acil");
  GtkWidget *olay = gtk_event_box_new();
  gtk_container_add(GTK_CONTAINER(p), olay);
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(k), 12);
  gtk_container_add(GTK_CONTAINER(olay), k);
  GtkWidget *im = simge_yap(simge, ipuclari);
  gtk_widget_set_valign(im, GTK_ALIGN_START);
  gtk_box_pack_start(GTK_BOX(k), im, FALSE, FALSE, 0);
  GtkWidget *yazi = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
  gtk_box_pack_start(GTK_BOX(k), yazi, TRUE, TRUE, 0);
  GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *bl = gtk_label_new(baslik && *baslik ? baslik : uygulama);
  gtk_label_set_xalign(GTK_LABEL(bl), 0);
  gtk_label_set_line_wrap(GTK_LABEL(bl), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(bl), PANGO_WRAP_WORD_CHAR);
  gtk_label_set_max_width_chars(GTK_LABEL(bl), 28);
  gtk_style_context_add_class(gtk_widget_get_style_context(bl), "baslik");
  GtkWidget *x = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief(GTK_BUTTON(x), GTK_RELIEF_NONE);
  gtk_widget_set_valign(x, GTK_ALIGN_START);
  g_signal_connect(x, "clicked", G_CALLBACK(kapat_dugme), b);
  gtk_box_pack_start(GTK_BOX(ust), bl, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(ust), x, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(yazi), ust, FALSE, FALSE, 0);
  if (govde && *govde) {
    GtkWidget *gl = gtk_label_new(NULL);
    /* izin verilen basit biçimlendirme (<b>, <i>...) geçerli değilse düz metin */
    if (pango_parse_markup(govde, -1, 0, NULL, NULL, NULL, NULL)) gtk_label_set_markup(GTK_LABEL(gl), govde);
    else gtk_label_set_text(GTK_LABEL(gl), govde);
    gtk_label_set_xalign(GTK_LABEL(gl), 0);
    gtk_label_set_line_wrap(GTK_LABEL(gl), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(gl), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_max_width_chars(GTK_LABEL(gl), 32);
    gtk_label_set_lines(GTK_LABEL(gl), 4);
    gtk_label_set_ellipsize(GTK_LABEL(gl), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(gl), "govde");
    gtk_box_pack_start(GTK_BOX(yazi), gl, FALSE, FALSE, 0);
  }
  gtk_widget_add_events(olay, GDK_BUTTON_PRESS_MASK | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
  g_signal_connect(olay, "button-press-event", G_CALLBACK(tiklandi), b);
  g_signal_connect(olay, "enter-notify-event", G_CALLBACK(uzerinde), b);
  g_signal_connect(olay, "leave-notify-event", G_CALLBACK(uzerinde), b);
  b->pencere = p;

  bildirimler = g_list_prepend(bildirimler, b);
  while (g_list_length(bildirimler) > EN_FAZLA) kapat(g_list_last(bildirimler)->data, 1);
  gtk_widget_show_all(p);
  yerlestir();
  if (aciliyet != 2) b->zaman = g_timeout_add(sure > 0 ? sure : 6000, sure_doldu, b);
  if (!yerine) g_spawn_command_line_async("aether-ses bildirim", NULL);
  return b->id;
}

static void cagri(GDBusConnection *c, const char *gonderen, const char *yol, const char *arayuz,
                  const char *yontem, GVariant *p, GDBusMethodInvocation *inv, gpointer d) {
  (void)c; (void)gonderen; (void)yol; (void)arayuz; (void)d;
  if (!strcmp(yontem, "GetCapabilities")) {
    const char *yetenek[] = { "body", "body-markup", "icon-static", "actions", "persistence", NULL };
    g_dbus_method_invocation_return_value(inv, g_variant_new("(^as)", yetenek));
  } else if (!strcmp(yontem, "GetServerInformation")) {
    g_dbus_method_invocation_return_value(inv, g_variant_new("(ssss)", "Aether Bildirimleri", "Aether", AETHER_SURUM, "1.2"));
  } else if (!strcmp(yontem, "CloseNotification")) {
    guint32 id; g_variant_get(p, "(u)", &id);
    Bildirim *b = bul(id); if (b) kapat(b, 3);
    g_dbus_method_invocation_return_value(inv, NULL);
  } else if (!strcmp(yontem, "Notify")) {
    const char *uyg, *simge, *baslik, *govde; guint32 yerine; const char **eylem; GVariant *ip; gint sure;
    g_variant_get(p, "(&su&s&s&s^a&s@a{sv}i)", &uyg, &yerine, &simge, &baslik, &govde, &eylem, &ip, &sure);
    guint32 id = goster(uyg, yerine, simge, baslik, govde, eylem, ip, sure);
    g_free(eylem); g_variant_unref(ip);
    g_dbus_method_invocation_return_value(inv, g_variant_new("(u)", id));
  }
}

static void ad_alindi(GDBusConnection *c, const char *ad, gpointer d) { (void)c; (void)ad; (void)d; }
static void ad_kaybedildi(GDBusConnection *c, const char *ad, gpointer d) {
  (void)c; (void)ad; (void)d;
  g_printerr("aether-bildirim: başka bir bildirim sunucusu çalışıyor\n");
  gtk_main_quit();
}

int main(int argc, char **argv) {
  gtk_init(&argc, &argv);
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,
    ".bildirim { background-color: @theme_base_color; border: 1px solid alpha(@theme_fg_color, 0.18); border-left: 3px solid #7c6ad6; }"
    ".bildirim.acil { border-left-color: #e53935; }"
    ".bildirim .baslik { font-weight: bold; }"
    ".bildirim .govde { opacity: 0.85; }"
    ".bildirim button { padding: 0; min-height: 18px; min-width: 18px; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  GError *e = NULL;
  bag = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &e);
  if (!bag) { g_printerr("aether-bildirim: oturum veriyoluna bağlanılamadı: %s\n", e->message); return 1; }
  GDBusNodeInfo *ni = g_dbus_node_info_new_for_xml(XML, NULL);
  static const GDBusInterfaceVTable vt = { cagri, NULL, NULL, { 0 } };
  g_dbus_connection_register_object(bag, "/org/freedesktop/Notifications", ni->interfaces[0], &vt, NULL, NULL, NULL);
  g_bus_own_name_on_connection(bag, "org.freedesktop.Notifications", G_BUS_NAME_OWNER_FLAGS_NONE, ad_alindi, ad_kaybedildi, NULL, NULL);
  gtk_main();
  return 0;
}
