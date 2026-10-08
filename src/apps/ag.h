/* Aether ağ yardımcıları — kablolu durum (/sys) ve Wi-Fi (iwd, D-Bus)
 * Görev çubuğu ve Ayarlar ortak kullanır.
 */
#ifndef AETHER_AG_H
#define AETHER_AG_H
#include <gio/gio.h>
#include <string.h>
#include <stdio.h>

#define IWD "net.connman.iwd"

typedef struct {
  char yol[256];     /* D-Bus nesne yolu */
  char ad[128];      /* SSID */
  char tur[16];      /* open, psk, 8021x */
  int guc;           /* 0-100 */
  int bagli, bilinen;
} AgKablosuz;

typedef struct {
  char arayuz[32];
  int kablosuz, bagli;
  char ip[64];
} AgArayuz;

static int ag_dosya_oku(const char *yol, char *tampon, size_t boy) {
  FILE *f = fopen(yol, "r");
  if (!f) return 0;
  size_t n = fread(tampon, 1, boy - 1, f); fclose(f);
  tampon[n] = 0;
  while (n && (tampon[n - 1] == '\n' || tampon[n - 1] == ' ')) tampon[--n] = 0;
  return 1;
}

/* arayüzlerin durumu; döndürülen sayı kadar a[] doldurulur */
static int ag_arayuzler(AgArayuz *a, int maks) {
  GDir *d = g_dir_open("/sys/class/net", 0, NULL);
  if (!d) return 0;
  int n = 0; const char *ad;
  while ((ad = g_dir_read_name(d)) && n < maks) {
    if (!strcmp(ad, "lo")) continue;
    char yol[256], dur[32] = "", tasiyici[8] = "0";
    char kb[256];
    snprintf(yol, sizeof yol, "/sys/class/net/%s/device", ad);
    snprintf(kb, sizeof kb, "/sys/class/net/%s/wireless", ad);
    /* köprü, tünel gibi sanal arayüzleri atla; kablosuz arayüzler her zaman sayılır */
    if (!g_file_test(yol, G_FILE_TEST_EXISTS) && !g_file_test(kb, G_FILE_TEST_IS_DIR)) continue;
    AgArayuz *x = &a[n++];
    memset(x, 0, sizeof *x);
    snprintf(x->arayuz, sizeof x->arayuz, "%s", ad);
    snprintf(yol, sizeof yol, "/sys/class/net/%s/wireless", ad);
    x->kablosuz = g_file_test(yol, G_FILE_TEST_IS_DIR);
    snprintf(yol, sizeof yol, "/sys/class/net/%s/operstate", ad); ag_dosya_oku(yol, dur, sizeof dur);
    snprintf(yol, sizeof yol, "/sys/class/net/%s/carrier", ad); ag_dosya_oku(yol, tasiyici, sizeof tasiyici);
    x->bagli = !strcmp(dur, "up") || (!strcmp(dur, "unknown") && tasiyici[0] == '1');
  }
  g_dir_close(d);
  /* IP adresleri */
  FILE *p = popen("ip -4 -o addr show 2>/dev/null", "r");
  if (p) {
    char s[512];
    while (fgets(s, sizeof s, p)) {
      char ar[64], ip[64];
      if (sscanf(s, "%*d: %63s inet %63s", ar, ip) == 2) {
        char *t = strchr(ip, '/'); if (t) *t = 0;
        for (int i = 0; i < n; i++) if (!strcmp(a[i].arayuz, ar)) snprintf(a[i].ip, sizeof a[i].ip, "%s", ip);
      }
    }
    pclose(p);
  }
  return n;
}

static int ag_guc(gint16 rssi100) {
  int dbm = rssi100 / 100;
  int g = (dbm + 100) * 2;
  return g < 0 ? 0 : g > 100 ? 100 : g;
}

static GDBusConnection *ag_sistem(void) {
  static GDBusConnection *b;
  if (!b) b = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
  return b;
}

static GVariant *ag_nesneler(void) {
  GDBusConnection *b = ag_sistem();
  if (!b) return NULL;
  return g_dbus_connection_call_sync(b, IWD, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
                                     NULL, G_VARIANT_TYPE("(a{oa{sa{sv}}})"), G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL);
}

/* iwd çalışıyor mu ve bir kablosuz istasyon var mı? istasyon yolunu döndürür */
static gboolean ag_istasyon(char *yol, size_t boy, char *durum, size_t dboy) {
  GVariant *v = ag_nesneler();
  if (!v) return FALSE;
  gboolean bulundu = FALSE;
  GVariantIter *it; const char *nyol; GVariant *arayuzler;
  g_variant_get(v, "(a{oa{sa{sv}}})", &it);
  while (!bulundu && g_variant_iter_next(it, "{&o@a{sa{sv}}}", &nyol, &arayuzler)) {
    GVariant *st = g_variant_lookup_value(arayuzler, IWD ".Station", G_VARIANT_TYPE("a{sv}"));
    if (st) {
      snprintf(yol, boy, "%s", nyol); bulundu = TRUE;
      if (durum) { const char *d = "?"; g_variant_lookup(st, "State", "&s", &d); snprintf(durum, dboy, "%s", d); }
      g_variant_unref(st);
    }
    g_variant_unref(arayuzler);
  }
  g_variant_iter_free(it); g_variant_unref(v);
  return bulundu;
}

/* taranmış ağlar (güce göre sıralı); en fazla maks */
static int ag_aglar(AgKablosuz *a, int maks) {
  char ist[256];
  if (!ag_istasyon(ist, sizeof ist, NULL, 0)) return 0;
  GVariant *nes = ag_nesneler();
  GVariant *v = g_dbus_connection_call_sync(ag_sistem(), IWD, ist, IWD ".Station", "GetOrderedNetworks", NULL,
                                            G_VARIANT_TYPE("(a(on))"), G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL);
  int n = 0;
  if (v && nes) {
    GVariantIter *it; const char *yol; gint16 rssi;
    g_variant_get(v, "(a(on))", &it);
    while (n < maks && g_variant_iter_next(it, "(&on)", &yol, &rssi)) {
      AgKablosuz *x = &a[n];
      memset(x, 0, sizeof *x);
      snprintf(x->yol, sizeof x->yol, "%s", yol);
      x->guc = ag_guc(rssi);
      GVariant *ar = NULL;
      GVariant *nesler = g_variant_get_child_value(nes, 0);
      if (g_variant_lookup(nesler, yol, "@a{sa{sv}}", &ar)) {
        GVariant *net = g_variant_lookup_value(ar, IWD ".Network", G_VARIANT_TYPE("a{sv}"));
        if (net) {
          const char *s = ""; gboolean b = FALSE;
          if (g_variant_lookup(net, "Name", "&s", &s)) snprintf(x->ad, sizeof x->ad, "%s", s);
          if (g_variant_lookup(net, "Type", "&s", &s)) snprintf(x->tur, sizeof x->tur, "%s", s);
          if (g_variant_lookup(net, "Connected", "b", &b)) x->bagli = b;
          x->bilinen = g_variant_lookup_value(net, "KnownNetwork", NULL) != NULL;
          g_variant_unref(net);
        }
        g_variant_unref(ar);
      }
      g_variant_unref(nesler);
      if (x->ad[0]) n++;
    }
    g_variant_iter_free(it);
  }
  if (v) g_variant_unref(v);
  if (nes) g_variant_unref(nes);
  return n;
}

/* bağlı kablosuz ağın adı ve gücü (yoksa FALSE) */
static gboolean ag_bagli_kablosuz(char *ad, size_t boy, int *guc) {
  AgKablosuz l[64];
  int n = ag_aglar(l, 64);
  for (int i = 0; i < n; i++) if (l[i].bagli) { snprintf(ad, boy, "%s", l[i].ad); if (guc) *guc = l[i].guc; return TRUE; }
  return FALSE;
}

static void ag_tara(void) {
  char ist[256];
  if (!ag_istasyon(ist, sizeof ist, NULL, 0)) return;
  g_dbus_connection_call(ag_sistem(), IWD, ist, IWD ".Station", "Scan", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
}

static void ag_kes(void) {
  char ist[256];
  if (!ag_istasyon(ist, sizeof ist, NULL, 0)) return;
  g_dbus_connection_call_sync(ag_sistem(), IWD, ist, IWD ".Station", "Disconnect", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL);
}

/* kayıtlı ağı unut: Network nesnesinin KnownNetwork özelliği üzerinden */
static void ag_unut(const char *ag_yolu) {
  GVariant *v = g_dbus_connection_call_sync(ag_sistem(), IWD, ag_yolu, "org.freedesktop.DBus.Properties", "Get",
                                            g_variant_new("(ss)", IWD ".Network", "KnownNetwork"), G_VARIANT_TYPE("(v)"),
                                            G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL);
  if (!v) return;
  GVariant *ic; g_variant_get(v, "(v)", &ic);
  const char *bilinen = g_variant_get_string(ic, NULL);
  g_dbus_connection_call_sync(ag_sistem(), IWD, bilinen, IWD ".KnownNetwork", "Forget", NULL, NULL, G_DBUS_CALL_FLAGS_NONE, 5000, NULL, NULL);
  g_variant_unref(ic); g_variant_unref(v);
}

/* --- parola ajanı: iwd parolayı bize sorar, biz önceden alınmış parolayı veririz --- */
static char *ag_parola;
static guint ag_ajan_id;
#define AG_AJAN_YOLU "/org/aether/ajan"

static void ag_ajan_cagri(GDBusConnection *c, const char *gonderen, const char *yol, const char *arayuz,
                          const char *yontem, GVariant *p, GDBusMethodInvocation *inv, gpointer d) {
  (void)c; (void)gonderen; (void)yol; (void)arayuz; (void)p; (void)d;
  if (!strcmp(yontem, "RequestPassphrase") || !strcmp(yontem, "RequestPrivateKeyPassphrase")) {
    if (ag_parola) g_dbus_method_invocation_return_value(inv, g_variant_new("(s)", ag_parola));
    else g_dbus_method_invocation_return_dbus_error(inv, IWD ".Agent.Error.Canceled", "parola yok");
  } else if (!strcmp(yontem, "Release") || !strcmp(yontem, "Cancel")) {
    g_dbus_method_invocation_return_value(inv, NULL);
  } else {
    g_dbus_method_invocation_return_dbus_error(inv, IWD ".Agent.Error.Canceled", "desteklenmiyor");
  }
}

static void ag_ajan_kur(void) {
  if (ag_ajan_id || !ag_sistem()) return;
  static const char *xml =
    "<node><interface name='net.connman.iwd.Agent'>"
    "<method name='Release'/>"
    "<method name='RequestPassphrase'><arg type='o' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='RequestPrivateKeyPassphrase'><arg type='o' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='RequestUserNameAndPassword'><arg type='o' direction='in'/><arg type='s' direction='out'/><arg type='s' direction='out'/></method>"
    "<method name='RequestUserPassword'><arg type='o' direction='in'/><arg type='s' direction='in'/><arg type='s' direction='out'/></method>"
    "<method name='Cancel'><arg type='s' direction='in'/></method>"
    "</interface></node>";
  static const GDBusInterfaceVTable vt = { ag_ajan_cagri, NULL, NULL, { 0 } };
  GDBusNodeInfo *ni = g_dbus_node_info_new_for_xml(xml, NULL);
  ag_ajan_id = g_dbus_connection_register_object(ag_sistem(), AG_AJAN_YOLU, ni->interfaces[0], &vt, NULL, NULL, NULL);
  g_dbus_node_info_unref(ni);
  g_dbus_connection_call_sync(ag_sistem(), IWD, "/net/connman/iwd", IWD ".AgentManager", "RegisterAgent",
                              g_variant_new("(o)", AG_AJAN_YOLU), NULL, G_DBUS_CALL_FLAGS_NONE, 3000, NULL, NULL);
}

/* bağlan (eşzamansız); bitince cb(hata ya da NULL) */
typedef void (*AgSonuc)(const char *hata, gpointer veri);
typedef struct { AgSonuc cb; gpointer veri; } AgIs;

static void ag_baglandi(GObject *k, GAsyncResult *r, gpointer d) {
  AgIs *is = d; GError *e = NULL;
  GVariant *v = g_dbus_connection_call_finish(G_DBUS_CONNECTION(k), r, &e);
  if (v) g_variant_unref(v);
  g_free(ag_parola); ag_parola = NULL;
  if (is->cb) {
    const char *m = NULL;
    if (e) {
      char *ad = g_dbus_error_get_remote_error(e);
      m = ad && (strstr(ad, "Failed") || strstr(ad, "Aborted") || strstr(ad, "Canceled")) ? "parola" : e->message;
      if (ad && strstr(ad, "NotFound")) m = "bulunamadi";
      is->cb(m, is->veri);
      g_free(ad);
    } else is->cb(NULL, is->veri);
  }
  if (e) g_error_free(e);
  g_free(is);
}

static void ag_baglan(const char *ag_yolu, const char *parola, AgSonuc cb, gpointer veri) {
  ag_ajan_kur();
  g_free(ag_parola); ag_parola = parola && *parola ? g_strdup(parola) : NULL;
  AgIs *is = g_new0(AgIs, 1); is->cb = cb; is->veri = veri;
  g_dbus_connection_call(ag_sistem(), IWD, ag_yolu, IWD ".Network", "Connect", NULL, NULL,
                         G_DBUS_CALL_FLAGS_NONE, 60000, NULL, ag_baglandi, is);
}

#endif
