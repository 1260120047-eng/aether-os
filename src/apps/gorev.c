/* aether-gorev — Aether Görev Yöneticisi
 *
 * İşlemler:   açık pencereli uygulamalar ve arka plan işlemleri; CPU ve bellek kullanımı,
 *             arama, sıralama, görevi sonlandırma (Del) ve zorla sonlandırma.
 * Performans: CPU, bellek, disk ve ağ için son 60 saniyenin grafiği ve ayrıntılar.
 *
 * Her şey /proc'tan saniyede bir okunur. Ctrl+Shift+Esc ile açılır.
 */
#include <gio/gdesktopappinfo.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include "aether.h"

#define GECMIS 60

/* ---------- ölçümler ---------- */
typedef struct { double v[GECMIS]; int n; } Seri;
static void seri_ekle(Seri *s, double x) {
  memmove(s->v, s->v + 1, (GECMIS - 1) * sizeof(double));
  s->v[GECMIS - 1] = x; if (s->n < GECMIS) s->n++;
}

static Seri s_cpu, s_bellek, s_disk, s_ag;
static double ag_tepe = 128 * 1024;          /* ağ grafiğinin üst sınırı (bayt/sn), görülen en yükseğe göre büyür */
static unsigned long long cpu_onceki_top, cpu_onceki_bos, cpu_delta_top;
static double cpu_yuzde, bellek_yuzde, disk_yuzde, disk_oku, disk_yaz, ag_al, ag_gon;
static unsigned long long m_toplam, m_kullanilabilir, m_onbellek, m_takas_top, m_takas_bos;
static int islem_sayisi, is_parcacigi;
static double calisma_sn;
static char cpu_model[128] = "CPU";
static int cekirdek;

static char *boyut_metni(double b) {
  const char *b_tr[] = { "B", "KB", "MB", "GB", "TB" };
  int i = 0; while (b >= 1024 && i < 4) { b /= 1024; i++; }
  char *s = g_strdup_printf(i == 0 ? "%.0f %s" : b < 10 ? "%.1f %s" : "%.0f %s", b, b_tr[i]);
  if (!ae_en()) for (char *q = s; *q; q++) if (*q == '.') *q = ',';
  return s;
}
static char *hiz_metni(double bps) {  /* bit/sn, ağ için */
  const char *b[] = { "bit/sn", "Kbit/sn", "Mbit/sn", "Gbit/sn" };
  const char *e[] = { "bps", "Kbps", "Mbps", "Gbps" };
  double x = bps * 8; int i = 0; while (x >= 1000 && i < 3) { x /= 1000; i++; }
  char *s = g_strdup_printf(x < 10 && i ? "%.1f %s" : "%.0f %s", x, ae_en() ? e[i] : b[i]);
  if (!ae_en()) for (char *q = s; *q; q++) if (*q == '.') *q = ',';
  return s;
}

static void cpu_bilgisi(void) {
  char *c = NULL;
  if (g_file_get_contents("/proc/cpuinfo", &c, NULL, NULL)) {
    char *p = strstr(c, "model name");
    if (p && (p = strchr(p, ':'))) { p++; while (*p == ' ') p++; size_t n = strcspn(p, "\n"); if (n > sizeof cpu_model - 1) n = sizeof cpu_model - 1; memcpy(cpu_model, p, n); cpu_model[n] = 0; }
    g_free(c);
  }
  cekirdek = (int)sysconf(_SC_NPROCESSORS_ONLN);
}

static double cpu_mhz(void) {
  char *c = NULL; double mhz = 0;
  if (g_file_get_contents("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &c, NULL, NULL)) { mhz = g_ascii_strtod(c, NULL) / 1000; g_free(c); }
  else if (g_file_get_contents("/proc/cpuinfo", &c, NULL, NULL)) {
    char *p = strstr(c, "cpu MHz"); if (p && (p = strchr(p, ':'))) mhz = g_ascii_strtod(p + 1, NULL);
    g_free(c);
  }
  return mhz;
}

static void sistem_oku(double gecen) {
  /* CPU */
  FILE *f = fopen("/proc/stat", "r");
  if (f) {
    unsigned long long a[8] = { 0 };
    if (fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &a[0], &a[1], &a[2], &a[3], &a[4], &a[5], &a[6], &a[7]) == 8) {
      unsigned long long top = 0; for (int i = 0; i < 8; i++) top += a[i];
      unsigned long long bos = a[3] + a[4];
      if (cpu_onceki_top && top > cpu_onceki_top) {
        cpu_delta_top = top - cpu_onceki_top;
        cpu_yuzde = 100.0 * (double)(cpu_delta_top - (bos - cpu_onceki_bos)) / cpu_delta_top;
      }
      cpu_onceki_top = top; cpu_onceki_bos = bos;
    }
    fclose(f);
  }
  /* bellek */
  f = fopen("/proc/meminfo", "r");
  if (f) {
    char ad[64]; unsigned long long v; unsigned long long tampon = 0, onbel = 0, sreclaim = 0;
    while (fscanf(f, "%63s %llu kB\n", ad, &v) == 2) {
      if (!strcmp(ad, "MemTotal:")) m_toplam = v * 1024;
      else if (!strcmp(ad, "MemAvailable:")) m_kullanilabilir = v * 1024;
      else if (!strcmp(ad, "Buffers:")) tampon = v * 1024;
      else if (!strcmp(ad, "Cached:")) onbel = v * 1024;
      else if (!strcmp(ad, "SReclaimable:")) sreclaim = v * 1024;
      else if (!strcmp(ad, "SwapTotal:")) m_takas_top = v * 1024;
      else if (!strcmp(ad, "SwapFree:")) m_takas_bos = v * 1024;
    }
    m_onbellek = tampon + onbel + sreclaim;
    fclose(f);
    if (m_toplam) bellek_yuzde = 100.0 * (double)(m_toplam - m_kullanilabilir) / m_toplam;
  }
  /* disk: bütün diskler (bölümler, loop, ram, zram hariç) */
  static unsigned long long d_onceki_r, d_onceki_w, d_onceki_t; static int d_ilk = 1;
  f = fopen("/proc/diskstats", "r");
  if (f) {
    char satir[512]; unsigned long long r = 0, w = 0, tmax = 0;
    while (fgets(satir, sizeof satir, f)) {
      unsigned maj, min; char ad[64]; unsigned long long x[11];
      if (sscanf(satir, "%u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu", &maj, &min, ad,
                 &x[0], &x[1], &x[2], &x[3], &x[4], &x[5], &x[6], &x[7], &x[8], &x[9], &x[10]) < 14) continue;
      if (!strncmp(ad, "loop", 4) || !strncmp(ad, "ram", 3) || !strncmp(ad, "zram", 4) || !strncmp(ad, "sr", 2) || !strncmp(ad, "dm-", 3)) continue;
      char yol[128]; snprintf(yol, sizeof yol, "/sys/block/%s", ad);
      if (!g_file_test(yol, G_FILE_TEST_IS_DIR)) continue;     /* bölüm değil, bütün disk */
      r += x[2] * 512; w += x[6] * 512;
      if (x[9] > tmax) tmax = x[9];                              /* en meşgul diskin etkin süresi (ms) */
    }
    fclose(f);
    if (!d_ilk && gecen > 0) {
      disk_oku = (r - d_onceki_r) / gecen; disk_yaz = (w - d_onceki_w) / gecen;
      disk_yuzde = MIN(100.0, (tmax - d_onceki_t) / (gecen * 10.0));
    }
    d_onceki_r = r; d_onceki_w = w; d_onceki_t = tmax; d_ilk = 0;
  }
  /* ağ (lo hariç) */
  static unsigned long long a_onceki_r, a_onceki_t; static int a_ilk = 1;
  f = fopen("/proc/net/dev", "r");
  if (f) {
    char satir[512]; unsigned long long rx = 0, tx = 0;
    while (fgets(satir, sizeof satir, f)) {
      char *iki = strchr(satir, ':'); if (!iki) continue;
      *iki = 0; char *ad = g_strstrip(satir);
      if (!strcmp(ad, "lo")) continue;
      unsigned long long v[16];
      if (sscanf(iki + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8]) == 9) { rx += v[0]; tx += v[8]; }
    }
    fclose(f);
    if (!a_ilk && gecen > 0) { ag_al = (rx - a_onceki_r) / gecen; ag_gon = (tx - a_onceki_t) / gecen; }
    a_onceki_r = rx; a_onceki_t = tx; a_ilk = 0;
  }
  f = fopen("/proc/uptime", "r");
  if (f) { if (fscanf(f, "%lf", &calisma_sn) != 1) calisma_sn = 0; fclose(f); }

  seri_ekle(&s_cpu, cpu_yuzde);
  seri_ekle(&s_bellek, bellek_yuzde);
  seri_ekle(&s_disk, disk_yuzde);
  double ag = ag_al + ag_gon;
  if (ag > ag_tepe) ag_tepe = ag * 1.2;
  seri_ekle(&s_ag, ag);
}

/* ---------- uygulama adları ve simgeleri ---------- */
typedef struct { char *ad; GIcon *simge; } Uyg;
static GHashTable *uyg_tablo;   /* çalıştırılabilir dosya adı → Uyg */

static void uygulamalari_tara(void) {
  uyg_tablo = g_hash_table_new(g_str_hash, g_str_equal);
  GList *hepsi = g_app_info_get_all();
  for (GList *l = hepsi; l; l = l->next) {
    GAppInfo *a = l->data;
    const char *exe = g_app_info_get_executable(a);
    if (!exe || !*exe) continue;
    char *taban = g_path_get_basename(exe);
    if (!strcmp(taban, "env") || !strcmp(taban, "sh") || !strcmp(taban, "bash") || !strcmp(taban, "gtk-launch") ||
        g_hash_table_contains(uyg_tablo, taban)) { g_free(taban); continue; }
    Uyg *u = g_new0(Uyg, 1);
    u->ad = g_strdup(g_app_info_get_display_name(a));
    GIcon *ic = g_app_info_get_icon(a); if (ic) u->simge = g_object_ref(ic);
    g_hash_table_insert(uyg_tablo, taban, u);
  }
  g_list_free_full(hepsi, g_object_unref);
}

/* pencere açan işlemler (_NET_CLIENT_LIST → _NET_WM_PID) */
static GHashTable *pencereli_pidler(void) {
  GHashTable *h = g_hash_table_new(g_direct_hash, g_direct_equal);
  GdkDisplay *gd = gdk_display_get_default();
  if (!GDK_IS_X11_DISPLAY(gd)) return h;
  Display *d = GDK_DISPLAY_XDISPLAY(gd);
  Atom liste = XInternAtom(d, "_NET_CLIENT_LIST", False), pid_a = XInternAtom(d, "_NET_WM_PID", False);
  Atom tur; int fmt; unsigned long n, kalan; unsigned char *veri = NULL;
  if (XGetWindowProperty(d, DefaultRootWindow(d), liste, 0, 4096, False, XA_WINDOW, &tur, &fmt, &n, &kalan, &veri) == Success && veri) {
    Window *w = (Window *)veri;
    Atom tur_a = XInternAtom(d, "_NET_WM_WINDOW_TYPE", False), durum_a = XInternAtom(d, "_NET_WM_STATE", False);
    Atom normal = XInternAtom(d, "_NET_WM_WINDOW_TYPE_NORMAL", False), diyalog = XInternAtom(d, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    Atom atla_a = XInternAtom(d, "_NET_WM_STATE_SKIP_TASKBAR", False);
    for (unsigned long i = 0; i < n; i++) {
      unsigned char *p = NULL; unsigned long pn;
      gdk_x11_display_error_trap_push(gd);
      /* yalnızca görev çubuğunda görünen pencereler (masaüstü, görev çubuğu, açılır pencereler değil) */
      int gecerli = 1;
      if (XGetWindowProperty(d, w[i], tur_a, 0, 8, False, XA_ATOM, &tur, &fmt, &pn, &kalan, &p) == Success && p && pn) {
        Atom t0 = ((Atom *)p)[0];
        gecerli = t0 == normal || t0 == diyalog;
      }
      if (p) { XFree(p); p = NULL; }
      if (gecerli && XGetWindowProperty(d, w[i], durum_a, 0, 32, False, XA_ATOM, &tur, &fmt, &pn, &kalan, &p) == Success && p)
        for (unsigned long k = 0; k < pn; k++) if (((Atom *)p)[k] == atla_a) gecerli = 0;
      if (p) { XFree(p); p = NULL; }
      if (!gecerli) { gdk_x11_display_error_trap_pop_ignored(gd); continue; }
      if (XGetWindowProperty(d, w[i], pid_a, 0, 1, False, XA_CARDINAL, &tur, &fmt, &pn, &kalan, &p) == Success && p && pn)
        g_hash_table_add(h, GINT_TO_POINTER((int)*(unsigned long *)p));
      if (p) XFree(p);
      gdk_x11_display_error_trap_pop_ignored(gd);
    }
    XFree(veri);
  }
  return h;
}

/* ---------- işlemler ---------- */
enum { K_PID, K_AD, K_SIMGE, K_KULLANICI, K_CPU, K_CPU_M, K_BELLEK, K_BELLEK_M, K_KALIN, K_SAYI };
#define GRUP_UYG (-1)
#define GRUP_ARKA (-2)

typedef struct { int pid; unsigned long long zaman; int goruldu; } Onceki;
static GHashTable *onceki_zaman;     /* pid → Onceki* */
static GtkTreeStore *depo;
static GtkTreeModel *suzgec, *sirali;
static GtkWidget *agac, *arama, *sonlandir_dugme;
static GtkTreeIter grup_uyg, grup_arka;
static GHashTable *kullanici_adlari;

static const char *kullanici_adi(uid_t u) {
  gpointer v = g_hash_table_lookup(kullanici_adlari, GUINT_TO_POINTER(u + 1));
  if (v) return v;
  struct passwd *pw = getpwuid(u);
  char *ad = pw ? g_strdup(pw->pw_name) : g_strdup_printf("%u", u);
  g_hash_table_insert(kullanici_adlari, GUINT_TO_POINTER(u + 1), ad);
  return ad;
}

typedef struct { int pid; char ad[256]; char exe[128]; uid_t uid; unsigned long long zaman; unsigned long long rss; int thr; int cekirdek; } Islem;

static int islem_oku(int pid, Islem *is) {
  char yol[64], tampon[1024];
  snprintf(yol, sizeof yol, "/proc/%d/stat", pid);
  FILE *f = fopen(yol, "r"); if (!f) return 0;
  size_t n = fread(tampon, 1, sizeof tampon - 1, f); fclose(f); tampon[n] = 0;
  char *ac = strchr(tampon, '('), *kap = strrchr(tampon, ')');
  if (!ac || !kap) return 0;
  size_t ln = MIN((size_t)(kap - ac - 1), sizeof is->ad - 1);
  memcpy(is->ad, ac + 1, ln); is->ad[ln] = 0;
  char durum; unsigned long long ut = 0, st = 0; long thr = 1; unsigned bayrak = 0;
  /* ) sonrası: durum ppid pgrp session tty tpgid flags minflt cminflt majflt cmajflt utime stime cutime cstime priority nice num_threads */
  if (sscanf(kap + 2, "%c %*d %*d %*d %*d %*d %u %*u %*u %*u %*u %llu %llu %*d %*d %*d %*d %ld", &durum, &bayrak, &ut, &st, &thr) < 5) return 0;
  is->zaman = ut + st; is->thr = (int)thr;
  is->cekirdek = (bayrak & 0x00200000) != 0;   /* PF_KTHREAD: çekirdek iş parçacığı */
  if (durum == 'Z') return 0;
  snprintf(yol, sizeof yol, "/proc/%d/statm", pid);
  f = fopen(yol, "r"); unsigned long long boyut = 0, rss = 0;
  if (f) { if (fscanf(f, "%llu %llu", &boyut, &rss) != 2) rss = 0; fclose(f); }
  is->rss = rss * (unsigned long long)sysconf(_SC_PAGESIZE);
  snprintf(yol, sizeof yol, "/proc/%d", pid);
  struct stat s; is->uid = stat(yol, &s) == 0 ? s.st_uid : 0;
  /* comm 15 karakterle kesilir; tam adı komut satırından al */
  is->exe[0] = 0;
  snprintf(yol, sizeof yol, "/proc/%d/cmdline", pid);
  f = fopen(yol, "r");
  if (f) {
    n = fread(tampon, 1, sizeof tampon - 1, f); fclose(f); tampon[n] = 0;
    if (n) { char *t = g_path_get_basename(tampon); snprintf(is->exe, sizeof is->exe, "%s", t); g_free(t); }
  }
  if (!is->exe[0]) snprintf(is->exe, sizeof is->exe, "%s", is->ad);
  else if (strlen(is->ad) >= 15 && !strncmp(is->exe, is->ad, 15)) snprintf(is->ad, sizeof is->ad, "%s", is->exe);
  /* çekirdek iş parçacıkları (kthreadd'nin çocukları) arka planda kalabalık etmesin */
  return 1;
}


static void satir_yaz(GtkTreeIter *it, Islem *is, double cpu) {
  Uyg *u = g_hash_table_lookup(uyg_tablo, is->exe);
  if (!u) u = g_hash_table_lookup(uyg_tablo, is->ad);
  char *bm = boyut_metni((double)is->rss);
  char cm[32]; snprintf(cm, sizeof cm, "%.1f %%", cpu);
  if (!ae_en()) for (char *q = cm; *q; q++) if (*q == '.') *q = ',';
  gtk_tree_store_set(depo, it, K_PID, is->pid, K_AD, u ? u->ad : is->ad, K_SIMGE, u && u->simge ? u->simge : NULL,
                     K_KULLANICI, kullanici_adi(is->uid), K_CPU, cpu, K_CPU_M, cm, K_BELLEK, (guint64)is->rss, K_BELLEK_M, bm, K_KALIN, 400, -1);
  g_free(bm);
}

static void grup_basligi(void) {
  int nu = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(depo), &grup_uyg);
  int na = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(depo), &grup_arka);
  char *a = g_strdup_printf("%s (%d)", T("Uygulamalar", "Apps"), nu);
  char *b = g_strdup_printf("%s (%d)", T("Arka plan işlemleri", "Background processes"), na);
  gtk_tree_store_set(depo, &grup_uyg, K_AD, a, -1);
  gtk_tree_store_set(depo, &grup_arka, K_AD, b, -1);
  g_free(a); g_free(b);
}

static void islemleri_guncelle(void) {
  GHashTable *pencereli = pencereli_pidler();
  GHashTable *var = g_hash_table_new(g_direct_hash, g_direct_equal);   /* pid → 1 (uyg) / 2 (arka) */
  GHashTable *bilgi = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  islem_sayisi = 0; is_parcacigi = 0;
  DIR *d = opendir("/proc");
  struct dirent *e;
  while (d && (e = readdir(d))) {
    if (!g_ascii_isdigit(e->d_name[0])) continue;
    int pid = atoi(e->d_name);
    Islem *is = g_new0(Islem, 1); is->pid = pid;
    if (!islem_oku(pid, is)) { g_free(is); continue; }
    islem_sayisi++; is_parcacigi += is->thr;
    if (is->cekirdek) { g_free(is); continue; }
    g_hash_table_insert(var, GINT_TO_POINTER(pid), GINT_TO_POINTER(g_hash_table_contains(pencereli, GINT_TO_POINTER(pid)) ? 1 : 2));
    g_hash_table_insert(bilgi, GINT_TO_POINTER(pid), is);
  }
  if (d) closedir(d);

  double bolen = cpu_delta_top ? (double)cpu_delta_top : 0;
  GHashTable *mevcut = g_hash_table_new(g_direct_hash, g_direct_equal);
  /* var olan satırları güncelle, ölenleri ya da grubu değişenleri sil */
  GtkTreeIter *gruplar[2] = { &grup_uyg, &grup_arka };
  for (int g = 0; g < 2; g++) {
    GtkTreeIter it;
    gboolean ok = gtk_tree_model_iter_children(GTK_TREE_MODEL(depo), &it, gruplar[g]);
    while (ok) {
      int pid; gtk_tree_model_get(GTK_TREE_MODEL(depo), &it, K_PID, &pid, -1);
      int grup = GPOINTER_TO_INT(g_hash_table_lookup(var, GINT_TO_POINTER(pid)));
      if (grup != g + 1) { ok = gtk_tree_store_remove(depo, &it); continue; }
      Islem *is = g_hash_table_lookup(bilgi, GINT_TO_POINTER(pid));
      Onceki *o = g_hash_table_lookup(onceki_zaman, GINT_TO_POINTER(pid));
      double cpu = (o && bolen > 0 && is->zaman >= o->zaman) ? 100.0 * (is->zaman - o->zaman) / bolen : 0;
      satir_yaz(&it, is, MIN(cpu, 100.0));
      g_hash_table_add(mevcut, GINT_TO_POINTER(pid));
      ok = gtk_tree_model_iter_next(GTK_TREE_MODEL(depo), &it);
    }
  }
  /* yeni işlemler */
  GHashTableIter hi; gpointer k, v;
  g_hash_table_iter_init(&hi, var);
  while (g_hash_table_iter_next(&hi, &k, &v)) {
    if (g_hash_table_contains(mevcut, k)) continue;
    GtkTreeIter it;
    gtk_tree_store_append(depo, &it, GPOINTER_TO_INT(v) == 1 ? &grup_uyg : &grup_arka);
    satir_yaz(&it, g_hash_table_lookup(bilgi, k), 0);
  }
  /* bir sonraki tur için CPU zamanlarını sakla */
  g_hash_table_remove_all(onceki_zaman);
  g_hash_table_iter_init(&hi, bilgi);
  while (g_hash_table_iter_next(&hi, &k, &v)) {
    Onceki *o = g_new0(Onceki, 1); o->pid = GPOINTER_TO_INT(k); o->zaman = ((Islem *)v)->zaman;
    g_hash_table_insert(onceki_zaman, k, o);
  }
  grup_basligi();
  g_hash_table_destroy(mevcut); g_hash_table_destroy(var); g_hash_table_destroy(bilgi); g_hash_table_destroy(pencereli);
}

/* gruplar her zaman aynı sırada (önce Uygulamalar), sıralama yönünden bağımsız */
static int sirala(GtkTreeModel *m, GtkTreeIter *a, GtkTreeIter *b, gpointer kolon) {
  int pa, pb; gtk_tree_model_get(m, a, K_PID, &pa, -1); gtk_tree_model_get(m, b, K_PID, &pb, -1);
  if (pa < 0 || pb < 0) {
    int sutun; GtkSortType yon;
    gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(sirali), &sutun, &yon);
    int r = (pa < 0 ? -pa : 99) - (pb < 0 ? -pb : 99);
    return yon == GTK_SORT_DESCENDING ? -r : r;
  }
  int c = GPOINTER_TO_INT(kolon);
  if (c == K_AD || c == K_KULLANICI) {
    char *x, *y; gtk_tree_model_get(m, a, c, &x, -1); gtk_tree_model_get(m, b, c, &y, -1);
    int r = g_utf8_collate(x ? x : "", y ? y : ""); g_free(x); g_free(y); return r;
  }
  if (c == K_CPU) { double x, y; gtk_tree_model_get(m, a, c, &x, -1); gtk_tree_model_get(m, b, c, &y, -1); return x < y ? -1 : x > y; }
  if (c == K_BELLEK) { guint64 x, y; gtk_tree_model_get(m, a, c, &x, -1); gtk_tree_model_get(m, b, c, &y, -1); return x < y ? -1 : x > y; }
  return pa - pb;
}

static gboolean gorunur(GtkTreeModel *m, GtkTreeIter *it, gpointer v) {
  (void)v;
  int pid; gtk_tree_model_get(m, it, K_PID, &pid, -1);
  if (pid < 0) return TRUE;
  const char *aranan = gtk_entry_get_text(GTK_ENTRY(arama));
  if (!*aranan) return TRUE;
  char *ad; gtk_tree_model_get(m, it, K_AD, &ad, -1);
  char *a1 = g_utf8_casefold(ad ? ad : "", -1), *a2 = g_utf8_casefold(aranan, -1);
  char p[16]; snprintf(p, sizeof p, "%d", pid);
  gboolean r = strstr(a1, a2) != NULL || strstr(p, aranan) != NULL;
  g_free(ad); g_free(a1); g_free(a2);
  return r;
}

static int secili_pid(char **ad) {
  GtkTreeModel *m; GtkTreeIter it;
  if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(agac)), &m, &it)) return -1;
  int pid; gtk_tree_model_get(m, &it, K_PID, &pid, -1);
  if (ad) gtk_tree_model_get(m, &it, K_AD, ad, -1);
  return pid;
}

static void uyari(const char *metin) {
  GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(gtk_widget_get_toplevel(agac)), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", metin);
  gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}

static void sonlandir(int sinyal) {
  char *ad = NULL; int pid = secili_pid(&ad);
  if (pid <= 0) { g_free(ad); return; }
  if (kill(pid, sinyal) != 0) {
    char *m = errno == EPERM
      ? g_strdup_printf(T("\"%s\" başka bir kullanıcıya ya da sisteme ait. Sonlandırmak için yönetici yetkisi gerekiyor:\n\ndoas kill %d",
                          "\"%s\" belongs to another user or the system. Ending it needs administrator rights:\n\ndoas kill %d"), ad, pid)
      : g_strdup_printf(T("\"%s\" sonlandırılamadı.", "Could not end \"%s\"."), ad);
    uyari(m); g_free(m);
  }
  g_free(ad);
}
static void sonlandir_tik(GtkWidget *w, gpointer v) { (void)w; (void)v; sonlandir(SIGTERM); }
static void zorla_tik(GtkWidget *w, gpointer v) { (void)w; (void)v; sonlandir(SIGKILL); }

static void secim_degisti(GtkTreeSelection *s, gpointer v) {
  (void)s; (void)v;
  gtk_widget_set_sensitive(sonlandir_dugme, secili_pid(NULL) > 0);
}

static gboolean agac_tik(GtkWidget *w, GdkEventButton *e, gpointer v) {
  (void)v;
  if (e->type != GDK_BUTTON_PRESS || e->button != 3) return FALSE;
  GtkTreePath *yol;
  if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(w), (int)e->x, (int)e->y, &yol, NULL, NULL, NULL)) return FALSE;
  gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(w)), yol);
  gtk_tree_path_free(yol);
  if (secili_pid(NULL) <= 0) return TRUE;
  GtkWidget *m = gtk_menu_new(), *o;
  o = gtk_menu_item_new_with_label(T("Görevi sonlandır", "End task"));
  g_signal_connect(o, "activate", G_CALLBACK(sonlandir_tik), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  o = gtk_menu_item_new_with_label(T("Zorla sonlandır", "Force end"));
  g_signal_connect(o, "activate", G_CALLBACK(zorla_tik), NULL); gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
  gtk_widget_show_all(m);
  gtk_menu_popup_at_pointer(GTK_MENU(m), (GdkEvent *)e);
  return TRUE;
}

static gboolean agac_tus(GtkWidget *w, GdkEventKey *e, gpointer v) {
  (void)w; (void)v;
  if (e->keyval == GDK_KEY_Delete) { sonlandir((e->state & GDK_SHIFT_MASK) ? SIGKILL : SIGTERM); return TRUE; }
  return FALSE;
}

static void arama_degisti(GtkEditable *e, gpointer v) {
  (void)e; (void)v;
  gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(suzgec));
  gtk_tree_view_expand_all(GTK_TREE_VIEW(agac));
}

static GtkTreeViewColumn *sutun(const char *baslik, GtkCellRenderer *r, const char *ozellik, int veri, int sira, float x) {
  GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(baslik, r, ozellik, veri, "weight", K_KALIN, NULL);
  gtk_tree_view_column_set_sort_column_id(c, sira);
  gtk_tree_view_column_set_resizable(c, TRUE);
  g_object_set(r, "xalign", x, NULL);
  gtk_tree_view_column_set_alignment(c, x);
  return c;
}

/* grup satırlarında PID görünmesin */
static void pid_hucresi(GtkTreeViewColumn *c, GtkCellRenderer *r, GtkTreeModel *m, GtkTreeIter *it, gpointer v) {
  (void)c; (void)v;
  int pid; gtk_tree_model_get(m, it, K_PID, &pid, -1);
  char s[16]; snprintf(s, sizeof s, "%d", pid);
  g_object_set(r, "text", pid < 0 ? "" : s, NULL);
}

static GtkWidget *islemler_sayfasi(void) {
  depo = gtk_tree_store_new(K_SAYI, G_TYPE_INT, G_TYPE_STRING, G_TYPE_ICON, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_INT);
  gtk_tree_store_append(depo, &grup_uyg, NULL);
  gtk_tree_store_set(depo, &grup_uyg, K_PID, GRUP_UYG, K_KALIN, 700, -1);
  gtk_tree_store_append(depo, &grup_arka, NULL);
  gtk_tree_store_set(depo, &grup_arka, K_PID, GRUP_ARKA, K_KALIN, 700, -1);
  suzgec = gtk_tree_model_filter_new(GTK_TREE_MODEL(depo), NULL);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(suzgec), gorunur, NULL, NULL);
  sirali = gtk_tree_model_sort_new_with_model(suzgec);
  int kolonlar[] = { K_PID, K_AD, K_KULLANICI, K_CPU, K_BELLEK };
  for (unsigned i = 0; i < G_N_ELEMENTS(kolonlar); i++)
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(sirali), kolonlar[i], sirala, GINT_TO_POINTER(kolonlar[i]), NULL);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(sirali), K_CPU, GTK_SORT_DESCENDING);

  agac = gtk_tree_view_new_with_model(sirali);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(agac), FALSE);
  /* ad: simge + metin */
  GtkTreeViewColumn *c = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(c, T("Ad", "Name"));
  GtkCellRenderer *ri = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(c, ri, FALSE);
  gtk_tree_view_column_add_attribute(c, ri, "gicon", K_SIMGE);
  GtkCellRenderer *rt = gtk_cell_renderer_text_new();
  g_object_set(rt, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  gtk_tree_view_column_pack_start(c, rt, TRUE);
  gtk_tree_view_column_add_attribute(c, rt, "text", K_AD);
  gtk_tree_view_column_add_attribute(c, rt, "weight", K_KALIN);
  gtk_tree_view_column_set_sort_column_id(c, K_AD);
  gtk_tree_view_column_set_expand(c, TRUE);
  gtk_tree_view_column_set_resizable(c, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(agac), c);
  GtkCellRenderer *rp = gtk_cell_renderer_text_new();
  GtkTreeViewColumn *cp = sutun("PID", rp, "text", K_PID, K_PID, 1);
  gtk_tree_view_column_set_cell_data_func(cp, rp, pid_hucresi, NULL, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(agac), cp);
  gtk_tree_view_append_column(GTK_TREE_VIEW(agac), sutun(T("Kullanıcı", "User"), gtk_cell_renderer_text_new(), "text", K_KULLANICI, K_KULLANICI, 0));
  gtk_tree_view_append_column(GTK_TREE_VIEW(agac), sutun("CPU", gtk_cell_renderer_text_new(), "text", K_CPU_M, K_CPU, 1));
  gtk_tree_view_append_column(GTK_TREE_VIEW(agac), sutun(T("Bellek", "Memory"), gtk_cell_renderer_text_new(), "text", K_BELLEK_M, K_BELLEK, 1));

  GtkTreeSelection *s = gtk_tree_view_get_selection(GTK_TREE_VIEW(agac));
  g_signal_connect(s, "changed", G_CALLBACK(secim_degisti), NULL);
  g_signal_connect(agac, "button-press-event", G_CALLBACK(agac_tik), NULL);
  g_signal_connect(agac, "key-press-event", G_CALLBACK(agac_tus), NULL);

  GtkWidget *kaydir = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(kaydir), agac);
  gtk_widget_set_vexpand(kaydir, TRUE);

  arama = gtk_search_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(arama), T("Ad ya da PID ile ara", "Search by name or PID"));
  g_signal_connect(arama, "search-changed", G_CALLBACK(arama_degisti), NULL);
  sonlandir_dugme = gtk_button_new_with_label(T("Görevi sonlandır", "End task"));
  gtk_widget_set_sensitive(sonlandir_dugme, FALSE);
  g_signal_connect(sonlandir_dugme, "clicked", G_CALLBACK(sonlandir_tik), NULL);

  GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_box_pack_start(GTK_BOX(ust), arama, TRUE, TRUE, 0);
  gtk_box_pack_end(GTK_BOX(ust), sonlandir_dugme, FALSE, FALSE, 0);
  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_set_border_width(GTK_CONTAINER(k), 10);
  gtk_box_pack_start(GTK_BOX(k), ust, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), kaydir, TRUE, TRUE, 0);
  return k;
}

/* ---------- performans ---------- */
enum { P_CPU, P_BELLEK, P_DISK, P_AG, P_SAYI };
typedef struct { Seri *seri; double r, g, b; GtkWidget *kucuk, *deger; } Kaynak;
static Kaynak kaynak[P_SAYI];
static int secili_kaynak = P_CPU;
static GtkWidget *buyuk_grafik, *p_baslik, *p_alt, *p_ust_sag, *ayrinti[8][2];

static double seri_olcek(int k) { return k == P_AG ? ag_tepe : 100.0; }

static void grafik_ciz(cairo_t *cr, int w, int h, int k, int izgara) {
  Kaynak *x = &kaynak[k];
  /* seçili satırın renkli arka planında da görünsün diye önce düz zemin */
  cairo_set_source_rgb(cr, 1, 1, 1); cairo_rectangle(cr, 0, 0, w, h); cairo_fill(cr);
  cairo_set_source_rgba(cr, x->r, x->g, x->b, 0.06); cairo_rectangle(cr, 0, 0, w, h); cairo_fill(cr);
  if (izgara) {
    cairo_set_source_rgba(cr, x->r, x->g, x->b, 0.18); cairo_set_line_width(cr, 1);
    for (int i = 1; i < 10; i++) { double yy = floor(h * i / 10.0) + 0.5; cairo_move_to(cr, 0, yy); cairo_line_to(cr, w, yy); }
    for (int i = 1; i < 20; i++) { double xx = floor(w * i / 20.0) + 0.5; cairo_move_to(cr, xx, 0); cairo_line_to(cr, xx, h); }
    cairo_stroke(cr);
  }
  Seri *s = x->seri; double olcek = seri_olcek(k);
  double adim = (double)w / (GECMIS - 1);
  int bas = GECMIS - s->n;
  if (s->n > 1) {
    cairo_move_to(cr, bas * adim, h);
    for (int i = bas; i < GECMIS; i++) cairo_line_to(cr, i * adim, h - MIN(1.0, s->v[i] / olcek) * (h - 1));
    cairo_line_to(cr, w, h); cairo_close_path(cr);
    cairo_set_source_rgba(cr, x->r, x->g, x->b, 0.22); cairo_fill(cr);
    for (int i = bas; i < GECMIS; i++) {
      double yy = h - MIN(1.0, s->v[i] / olcek) * (h - 1);
      if (i == bas) cairo_move_to(cr, i * adim, yy); else cairo_line_to(cr, i * adim, yy);
    }
    cairo_set_source_rgb(cr, x->r, x->g, x->b); cairo_set_line_width(cr, izgara ? 1.5 : 1.2); cairo_stroke(cr);
  }
  cairo_set_source_rgba(cr, x->r, x->g, x->b, 0.8); cairo_set_line_width(cr, 1);
  cairo_rectangle(cr, 0.5, 0.5, w - 1, h - 1); cairo_stroke(cr);
}

static gboolean kucuk_ciz(GtkWidget *w, cairo_t *cr, gpointer v) {
  grafik_ciz(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w), GPOINTER_TO_INT(v), 0);
  return FALSE;
}
static gboolean buyuk_ciz(GtkWidget *w, cairo_t *cr, gpointer v) {
  (void)v;
  grafik_ciz(cr, gtk_widget_get_allocated_width(w), gtk_widget_get_allocated_height(w), secili_kaynak, 1);
  return FALSE;
}

static void ayrinti_yaz(int i, const char *ad, char *deger) {
  gtk_label_set_text(GTK_LABEL(ayrinti[i][0]), ad ? ad : "");
  gtk_label_set_text(GTK_LABEL(ayrinti[i][1]), deger ? deger : "");
  g_free(deger);
}

static char *sure_metni(double sn) {
  long s = (long)sn; long g = s / 86400; s %= 86400;
  return g ? g_strdup_printf("%ld:%02ld:%02ld:%02ld", g, s / 3600, s / 60 % 60, s % 60)
           : g_strdup_printf("%ld:%02ld:%02ld", s / 3600, s / 60 % 60, s % 60);
}
static char *yuzde_metni(double y) {
  char *s = g_strdup_printf("%% %.0f", y);
  if (ae_en()) { g_free(s); s = g_strdup_printf("%.0f%%", y); }
  return s;
}

static void performans_guncelle(void) {
  /* sol listedeki özetler */
  char *t;
  t = yuzde_metni(cpu_yuzde); gtk_label_set_text(GTK_LABEL(kaynak[P_CPU].deger), t); g_free(t);
  char *k1 = boyut_metni((double)(m_toplam - m_kullanilabilir)), *k2 = boyut_metni((double)m_toplam);
  t = g_strdup_printf("%s / %s (%.0f%%)", k1, k2, bellek_yuzde); gtk_label_set_text(GTK_LABEL(kaynak[P_BELLEK].deger), t); g_free(t);
  t = yuzde_metni(disk_yuzde); gtk_label_set_text(GTK_LABEL(kaynak[P_DISK].deger), t); g_free(t);
  char *a1 = hiz_metni(ag_gon), *a2 = hiz_metni(ag_al);
  t = g_strdup_printf("%s %s  %s %s", T("G:", "S:"), a1, T("A:", "R:"), a2); gtk_label_set_text(GTK_LABEL(kaynak[P_AG].deger), t); g_free(t);
  for (int i = 0; i < P_SAYI; i++) gtk_widget_queue_draw(kaynak[i].kucuk);
  gtk_widget_queue_draw(buyuk_grafik);

  for (int i = 0; i < 8; i++) ayrinti_yaz(i, NULL, NULL);
  switch (secili_kaynak) {
    case P_CPU: {
      gtk_label_set_text(GTK_LABEL(p_baslik), "CPU");
      gtk_label_set_text(GTK_LABEL(p_alt), cpu_model);
      gtk_label_set_text(GTK_LABEL(p_ust_sag), T("% Kullanım, son 60 saniye", "% Utilization, last 60 seconds"));
      ayrinti_yaz(0, T("Kullanım", "Utilization"), yuzde_metni(cpu_yuzde));
      double mhz = cpu_mhz();
      ayrinti_yaz(1, T("Hız", "Speed"), mhz > 0 ? g_strdup_printf(mhz >= 1000 ? "%.2f GHz" : "%.0f MHz", mhz >= 1000 ? mhz / 1000 : mhz) : g_strdup("—"));
      ayrinti_yaz(2, T("İşlemler", "Processes"), g_strdup_printf("%d", islem_sayisi));
      ayrinti_yaz(3, T("İş parçacıkları", "Threads"), g_strdup_printf("%d", is_parcacigi));
      ayrinti_yaz(4, T("Mantıksal işlemciler", "Logical processors"), g_strdup_printf("%d", cekirdek));
      ayrinti_yaz(5, T("Çalışma süresi", "Up time"), sure_metni(calisma_sn));
      break;
    }
    case P_BELLEK: {
      gtk_label_set_text(GTK_LABEL(p_baslik), T("Bellek", "Memory"));
      t = boyut_metni((double)m_toplam); gtk_label_set_text(GTK_LABEL(p_alt), t); g_free(t);
      gtk_label_set_text(GTK_LABEL(p_ust_sag), T("Bellek kullanımı, son 60 saniye", "Memory usage, last 60 seconds"));
      ayrinti_yaz(0, T("Kullanımda", "In use"), boyut_metni((double)(m_toplam - m_kullanilabilir)));
      ayrinti_yaz(1, T("Kullanılabilir", "Available"), boyut_metni((double)m_kullanilabilir));
      ayrinti_yaz(2, T("Önbellek", "Cached"), boyut_metni((double)m_onbellek));
      ayrinti_yaz(3, T("Toplam", "Total"), boyut_metni((double)m_toplam));
      if (m_takas_top) {
        char *x = boyut_metni((double)(m_takas_top - m_takas_bos)), *y = boyut_metni((double)m_takas_top);
        ayrinti_yaz(4, T("Takas alanı", "Swap"), g_strdup_printf("%s / %s", x, y)); g_free(x); g_free(y);
      } else ayrinti_yaz(4, T("Takas alanı", "Swap"), g_strdup(T("yok", "none")));
      break;
    }
    case P_DISK:
      gtk_label_set_text(GTK_LABEL(p_baslik), "Disk");
      gtk_label_set_text(GTK_LABEL(p_alt), T("Tüm diskler", "All disks"));
      gtk_label_set_text(GTK_LABEL(p_ust_sag), T("Etkin süre, son 60 saniye", "Active time, last 60 seconds"));
      ayrinti_yaz(0, T("Etkin süre", "Active time"), yuzde_metni(disk_yuzde));
      t = boyut_metni(disk_oku); ayrinti_yaz(1, T("Okuma hızı", "Read speed"), g_strdup_printf("%s/%s", t, T("sn", "s"))); g_free(t);
      t = boyut_metni(disk_yaz); ayrinti_yaz(2, T("Yazma hızı", "Write speed"), g_strdup_printf("%s/%s", t, T("sn", "s"))); g_free(t);
      break;
    case P_AG: {
      gtk_label_set_text(GTK_LABEL(p_baslik), T("Ağ", "Network"));
      gtk_label_set_text(GTK_LABEL(p_alt), T("Tüm bağdaştırıcılar", "All adapters"));
      char *ust = hiz_metni(ag_tepe);
      t = g_strdup_printf("%s %s", T("Aktarım hızı, en çok", "Throughput, up to"), ust); gtk_label_set_text(GTK_LABEL(p_ust_sag), t); g_free(t); g_free(ust);
      ayrinti_yaz(0, T("Gönderme", "Send"), hiz_metni(ag_gon));
      ayrinti_yaz(1, T("Alma", "Receive"), hiz_metni(ag_al));
      break;
    }
  }
  g_free(k1); g_free(k2); g_free(a1); g_free(a2);
}

static void kaynak_secildi(GtkListBox *l, GtkListBoxRow *r, gpointer v) {
  (void)l; (void)v;
  if (!r) return;
  secili_kaynak = gtk_list_box_row_get_index(r);
  performans_guncelle();
}

static GtkWidget *performans_sayfasi(void) {
  const char *adlar[P_SAYI] = { "CPU", T("Bellek", "Memory"), "Disk", T("Ağ", "Network") };
  Seri *seriler[P_SAYI] = { &s_cpu, &s_bellek, &s_disk, &s_ag };
  const double renk[P_SAYI][3] = { { 0.42, 0.29, 0.85 }, { 0.55, 0.22, 0.62 }, { 0.30, 0.60, 0.25 }, { 0.75, 0.45, 0.10 } };

  GtkWidget *liste = gtk_list_box_new();
  gtk_widget_set_size_request(liste, 250, -1);
  for (int i = 0; i < P_SAYI; i++) {
    Kaynak *k = &kaynak[i]; k->seri = seriler[i]; k->r = renk[i][0]; k->g = renk[i][1]; k->b = renk[i][2];
    GtkWidget *satir = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(satir), 8);
    k->kucuk = gtk_drawing_area_new(); gtk_widget_set_size_request(k->kucuk, 72, 44);
    g_signal_connect(k->kucuk, "draw", G_CALLBACK(kucuk_ciz), GINT_TO_POINTER(i));
    GtkWidget *yazi = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    GtkWidget *ad = gtk_label_new(adlar[i]); gtk_label_set_xalign(GTK_LABEL(ad), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(ad), "gorev-kaynak");
    k->deger = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(k->deger), 0);
    gtk_label_set_ellipsize(GTK_LABEL(k->deger), PANGO_ELLIPSIZE_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(k->deger), "ae-alt");
    gtk_box_pack_start(GTK_BOX(yazi), ad, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(yazi), k->deger, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(satir), k->kucuk, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(satir), yazi, TRUE, TRUE, 0);
    gtk_list_box_insert(GTK_LIST_BOX(liste), satir, -1);
  }
  g_signal_connect(liste, "row-selected", G_CALLBACK(kaynak_secildi), NULL);
  gtk_list_box_select_row(GTK_LIST_BOX(liste), gtk_list_box_get_row_at_index(GTK_LIST_BOX(liste), 0));

  GtkWidget *sag = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width(GTK_CONTAINER(sag), 14);
  GtkWidget *bas = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  p_baslik = gtk_label_new("CPU"); gtk_style_context_add_class(gtk_widget_get_style_context(p_baslik), "gorev-baslik");
  p_alt = gtk_label_new(""); gtk_label_set_ellipsize(GTK_LABEL(p_alt), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign(GTK_LABEL(p_alt), 1);
  gtk_box_pack_start(GTK_BOX(bas), p_baslik, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(bas), p_alt, TRUE, TRUE, 0);
  p_ust_sag = gtk_label_new(""); gtk_label_set_xalign(GTK_LABEL(p_ust_sag), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(p_ust_sag), "ae-alt");
  buyuk_grafik = gtk_drawing_area_new(); gtk_widget_set_vexpand(buyuk_grafik, TRUE);
  gtk_widget_set_size_request(buyuk_grafik, 360, 200);
  g_signal_connect(buyuk_grafik, "draw", G_CALLBACK(buyuk_ciz), NULL);
  GtkWidget *izg = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(izg), 28); gtk_grid_set_row_spacing(GTK_GRID(izg), 2);
  for (int i = 0; i < 8; i++) {
    ayrinti[i][0] = gtk_label_new(""); ayrinti[i][1] = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(ayrinti[i][0]), 0); gtk_label_set_xalign(GTK_LABEL(ayrinti[i][1]), 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(ayrinti[i][0]), "ae-alt");
    gtk_style_context_add_class(gtk_widget_get_style_context(ayrinti[i][1]), "gorev-deger");
    /* iki sütun: 0-3 solda, 4-7 sağda; her biri ad üstte, değer altta */
    int sut = i / 4, sat = (i % 4) * 2;
    gtk_grid_attach(GTK_GRID(izg), ayrinti[i][0], sut, sat, 1, 1);
    gtk_grid_attach(GTK_GRID(izg), ayrinti[i][1], sut, sat + 1, 1, 1);
  }
  gtk_box_pack_start(GTK_BOX(sag), bas, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), p_ust_sag, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sag), buyuk_grafik, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(sag), izg, FALSE, FALSE, 8);

  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *kaydir = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(kaydir), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(kaydir), liste);
  gtk_box_pack_start(GTK_BOX(k), kaydir, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), sag, TRUE, TRUE, 0);
  return k;
}

/* ---------- ana döngü ---------- */
static gint64 son_olcum;
static gboolean tik(gpointer v) {
  (void)v;
  gint64 simdi = g_get_monotonic_time();
  double gecen = son_olcum ? (simdi - son_olcum) / 1e6 : 1.0;
  son_olcum = simdi;
  sistem_oku(gecen);
  islemleri_guncelle();
  performans_guncelle();
  return G_SOURCE_CONTINUE;
}

static void etkinlesti(GtkApplication *uyg, gpointer v) {
  (void)v;
  GList *w = gtk_application_get_windows(uyg);
  if (w) { gtk_window_present(GTK_WINDOW(w->data)); return; }

  ae_css();
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css,
    ".gorev-baslik { font-size: 18pt; font-weight: 300; }"
    ".gorev-kaynak { font-weight: bold; }"
    ".gorev-deger { font-size: 12pt; margin-bottom: 6px; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  GtkWidget *pen = gtk_application_window_new(uyg);
  gtk_window_set_title(GTK_WINDOW(pen), T("Görev Yöneticisi", "Task Manager"));
  gtk_window_set_icon_name(GTK_WINDOW(pen), "aether-gorev");
  gtk_window_set_default_size(GTK_WINDOW(pen), 860, 580);

  GtkWidget *yigin = gtk_stack_new();
  gtk_stack_set_transition_type(GTK_STACK(yigin), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
  gtk_stack_add_titled(GTK_STACK(yigin), islemler_sayfasi(), "islemler", T("İşlemler", "Processes"));
  gtk_stack_add_titled(GTK_STACK(yigin), performans_sayfasi(), "performans", T("Performans", "Performance"));
  GtkWidget *sec = gtk_stack_switcher_new();
  gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(sec), GTK_STACK(yigin));
  gtk_widget_set_halign(sec, GTK_ALIGN_START);
  gtk_widget_set_margin_start(sec, 10); gtk_widget_set_margin_top(sec, 10);

  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start(GTK_BOX(k), sec, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), yigin, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(pen), k);

  tik(NULL);
  gtk_widget_show_all(pen);
  gtk_tree_view_expand_all(GTK_TREE_VIEW(agac));
  g_timeout_add(1000, tik, NULL);
  /* ilk açılışta yeni işlemlerin görünmesi için grupları açık tut */
  g_signal_connect_swapped(gtk_tree_view_get_model(GTK_TREE_VIEW(agac)), "row-has-child-toggled", G_CALLBACK(gtk_tree_view_expand_all), agac);
}

int main(int argc, char **argv) {
  cpu_bilgisi();
  uygulamalari_tara();
  onceki_zaman = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  kullanici_adlari = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
  GtkApplication *uyg = gtk_application_new("org.aether.Gorev", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(uyg, "activate", G_CALLBACK(etkinlesti), NULL);
  int r = g_application_run(G_APPLICATION(uyg), argc, argv);
  g_object_unref(uyg);
  return r;
}
