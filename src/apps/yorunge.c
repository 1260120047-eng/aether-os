/*
 * Yörünge — Aether'in pencere yöneticisi
 * Aether 1.1
 *
 * Çerçeve + başlık çubuğu (simge, ad, küçült/büyüt/kapat), taşıma ve boyutlandırma,
 * tıklayarak odak, Alt+Tab, sağ tık / Super menüsü, EWMH, açık/koyu tema.
 * Tema renkleri /usr/share/themes/<Tema>/yorunge/themerc dosyasından okunur.
 *
 *   yorunge            oturumu başlatır
 *   pkill -HUP yorunge temayı yeniden yükler
 *   pkill yorunge      oturumu kapatır
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#include <X11/cursorfont.h>
#include <X11/Xproto.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <X11/Xcursor/Xcursor.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>

#define SURUM "1.0"
#define MAKS_I 512
#define YAKIN 12          /* kenara yapışma mesafesi */
#define TUT_K 6           /* boyutlandırma tutamağı kalınlığı */
#define TUT_C 18          /* köşe tutamağı uzunluğu */
#define IKON 16
#define CIFT_TIK 400

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* ---------- atomlar ---------- */
enum {
  WM_PROTOCOLS, WM_DELETE_WINDOW, WM_TAKE_FOCUS, WM_STATE, WM_CHANGE_STATE, UTF8_STRING,
  NET_SUPPORTED, NET_SUPPORTING_WM_CHECK, NET_WM_NAME, NET_CLIENT_LIST, NET_CLIENT_LIST_STACKING,
  NET_ACTIVE_WINDOW, NET_NUMBER_OF_DESKTOPS, NET_CURRENT_DESKTOP, NET_DESKTOP_GEOMETRY,
  NET_DESKTOP_VIEWPORT, NET_WORKAREA, NET_DESKTOP_NAMES, NET_WM_DESKTOP, NET_CLOSE_WINDOW,
  NET_MOVERESIZE_WINDOW, NET_WM_MOVERESIZE, NET_REQUEST_FRAME_EXTENTS, NET_FRAME_EXTENTS,
  NET_SHOWING_DESKTOP, NET_WM_STATE, NET_WM_STATE_FULLSCREEN, NET_WM_STATE_MAXIMIZED_VERT,
  NET_WM_STATE_MAXIMIZED_HORZ, NET_WM_STATE_HIDDEN, NET_WM_STATE_ABOVE, NET_WM_STATE_FOCUSED,
  NET_WM_STATE_DEMANDS_ATTENTION, NET_WM_WINDOW_TYPE, NET_WM_WINDOW_TYPE_NORMAL,
  NET_WM_WINDOW_TYPE_DIALOG, NET_WM_WINDOW_TYPE_DOCK, NET_WM_WINDOW_TYPE_DESKTOP,
  NET_WM_WINDOW_TYPE_SPLASH, NET_WM_WINDOW_TYPE_UTILITY, NET_WM_WINDOW_TYPE_TOOLBAR,
  NET_WM_WINDOW_TYPE_MENU, NET_WM_WINDOW_TYPE_NOTIFICATION, NET_WM_WINDOW_TYPE_TOOLTIP,
  NET_WM_WINDOW_TYPE_DROPDOWN_MENU, NET_WM_WINDOW_TYPE_POPUP_MENU, NET_WM_WINDOW_TYPE_COMBO,
  NET_WM_WINDOW_TYPE_DND, NET_WM_ICON, NET_WM_STRUT, NET_WM_STRUT_PARTIAL, NET_WM_ALLOWED_ACTIONS,
  NET_WM_ACTION_MOVE, NET_WM_ACTION_RESIZE, NET_WM_ACTION_MINIMIZE, NET_WM_ACTION_MAXIMIZE_HORZ,
  NET_WM_ACTION_MAXIMIZE_VERT, NET_WM_ACTION_FULLSCREEN, NET_WM_ACTION_CLOSE, NET_WM_ACTION_ABOVE,
  MOTIF_WM_HINTS, YORUNGE_MENU, ATOM_SAYI
};
static char *atom_adi[ATOM_SAYI] = {
  "WM_PROTOCOLS", "WM_DELETE_WINDOW", "WM_TAKE_FOCUS", "WM_STATE", "WM_CHANGE_STATE", "UTF8_STRING",
  "_NET_SUPPORTED", "_NET_SUPPORTING_WM_CHECK", "_NET_WM_NAME", "_NET_CLIENT_LIST", "_NET_CLIENT_LIST_STACKING",
  "_NET_ACTIVE_WINDOW", "_NET_NUMBER_OF_DESKTOPS", "_NET_CURRENT_DESKTOP", "_NET_DESKTOP_GEOMETRY",
  "_NET_DESKTOP_VIEWPORT", "_NET_WORKAREA", "_NET_DESKTOP_NAMES", "_NET_WM_DESKTOP", "_NET_CLOSE_WINDOW",
  "_NET_MOVERESIZE_WINDOW", "_NET_WM_MOVERESIZE", "_NET_REQUEST_FRAME_EXTENTS", "_NET_FRAME_EXTENTS",
  "_NET_SHOWING_DESKTOP", "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN", "_NET_WM_STATE_MAXIMIZED_VERT",
  "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_HIDDEN", "_NET_WM_STATE_ABOVE", "_NET_WM_STATE_FOCUSED",
  "_NET_WM_STATE_DEMANDS_ATTENTION", "_NET_WM_WINDOW_TYPE", "_NET_WM_WINDOW_TYPE_NORMAL",
  "_NET_WM_WINDOW_TYPE_DIALOG", "_NET_WM_WINDOW_TYPE_DOCK", "_NET_WM_WINDOW_TYPE_DESKTOP",
  "_NET_WM_WINDOW_TYPE_SPLASH", "_NET_WM_WINDOW_TYPE_UTILITY", "_NET_WM_WINDOW_TYPE_TOOLBAR",
  "_NET_WM_WINDOW_TYPE_MENU", "_NET_WM_WINDOW_TYPE_NOTIFICATION", "_NET_WM_WINDOW_TYPE_TOOLTIP",
  "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU", "_NET_WM_WINDOW_TYPE_POPUP_MENU", "_NET_WM_WINDOW_TYPE_COMBO",
  "_NET_WM_WINDOW_TYPE_DND", "_NET_WM_ICON", "_NET_WM_STRUT", "_NET_WM_STRUT_PARTIAL", "_NET_WM_ALLOWED_ACTIONS",
  "_NET_WM_ACTION_MOVE", "_NET_WM_ACTION_RESIZE", "_NET_WM_ACTION_MINIMIZE", "_NET_WM_ACTION_MAXIMIZE_HORZ",
  "_NET_WM_ACTION_MAXIMIZE_VERT", "_NET_WM_ACTION_FULLSCREEN", "_NET_WM_ACTION_CLOSE", "_NET_WM_ACTION_ABOVE",
  "_MOTIF_WM_HINTS", "_YORUNGE_MENU"
};
static Atom A[ATOM_SAYI];

/* ---------- tema ---------- */
enum {
  R_KENAR_A, R_KENAR_I, R_BASLIK_A, R_BASLIK_I, R_YAZI_A, R_YAZI_I, R_AYRAC_A, R_AYRAC_I,
  R_DUG_A, R_DUG_I, R_DUG_UST_BG, R_DUG_UST_FG, R_DUG_BAS_BG, R_DUG_BAS_FG, R_DUG_ACIK, R_DUG_PASIF,
  R_M_KENAR, R_M_BG, R_M_FG, R_M_SOLUK, R_M_SEC_BG, R_M_SEC_FG, R_M_BAS_BG, R_M_BAS_FG, R_M_AYRAC,
  R_SAYI
};
static const struct { const char *anahtar, *varsayilan; } renk_tanim[R_SAYI] = {
  {"window.active.border.color", "#7c6ad6"},  {"window.inactive.border.color", "#c9c8d4"},
  {"window.active.title.bg.color", "#f4f4f8"}, {"window.inactive.title.bg.color", "#ebebf0"},
  {"window.active.label.text.color", "#26223f"}, {"window.inactive.label.text.color", "#9a98aa"},
  {"window.active.title.separator.color", "#c9c8d4"}, {"window.inactive.title.separator.color", "#c9c8d4"},
  {"window.active.button.unpressed.image.color", "#26223f"}, {"window.inactive.button.unpressed.image.color", "#9a98aa"},
  {"window.active.button.hover.bg.color", "#e2ddfb"}, {"window.active.button.hover.image.color", "#26223f"},
  {"window.active.button.pressed.bg.color", "#5b4bd6"}, {"window.active.button.pressed.image.color", "#ffffff"},
  {"window.active.button.toggled.image.color", "#5b4bd6"}, {"window.active.button.disabled.image.color", "#9a98aa"},
  {"menu.border.color", "#7c6ad6"}, {"menu.items.bg.color", "#fbfbfd"}, {"menu.items.text.color", "#26223f"},
  {"menu.items.disabled.text.color", "#9a98aa"}, {"menu.items.active.bg.color", "#5b4bd6"},
  {"menu.items.active.text.color", "#ffffff"}, {"menu.title.bg.color", "#efedf8"},
  {"menu.title.text.color", "#5b4bd6"}, {"menu.separator.color", "#dddce6"},
};
static XftColor R[R_SAYI];
static int renk_var;
static int kenar_k = 1, pad_w = 8, pad_h = 5;
enum { BM_KAPAT, BM_KUCULT, BM_BUYUT, BM_BUYUT2, BM_SAYI };
static Pixmap bm[BM_SAYI]; static int bm_w[BM_SAYI], bm_h[BM_SAYI];
static XftFont *f_aktif, *f_pasif, *f_menu, *f_menu_bas;
static int TH;            /* başlık çubuğu yüksekliği (ayraç çizgisi dahil) */

/* ---------- genel ---------- */
static Display *dpy;
static int scr, sw, sh, derinlik;
static Window root, kontrol;
static Visual *vis;
static Colormap cmap;
static GC gc;
static XRenderPictFormat *fmt_ekran, *fmt_argb;
static int numlock;
static int EN;
static int bitir_istek, yenile_istek;
static int sinyal_boru[2];

enum { IM_OK, IM_TASI, IM_U, IM_A, IM_SOL, IM_SAG, IM_USOL, IM_USAG, IM_ASOL, IM_ASAG, IM_SAYI };
static Cursor imlec[IM_SAYI];

typedef struct { int x, y, w, h; } Alan;
static Alan calisma;

enum { SOL = 1, SAG = 2, UST = 4, ALT = 8 };

typedef struct Istemci Istemci;
struct Istemci {
  Window win, cerceve, tut[12];
  int x, y, w, h;               /* yüzen konum: istemci alanının kök koordinatı ve boyutu */
  int gx, gy, gw, gh, gb, gth;  /* şu anki gerçek yerleşim */
  int dekor, buyuk, tam, kucuk, ustte, odaksiz, ozel, masaustu_gizli;
  Atom tur;
  int minw, minh, maxw, maxh, incw, inch, basew, baseh;
  int girdi, takefocus, silinebilir, eski_kenar;
  Window gecici;
  int yoksay_unmap;
  int ust_dug, bas_dug;
  char ad[256];
  Picture ikon; Pixmap ikon_pm; int ikon_sahip;
  long strut[4];
  Atom ek_durum[8]; int nek;    /* bizim yönetmediğimiz durumlar (ör. SKIP_TASKBAR) korunur */
};
static Istemci *yigin[MAKS_I]; static int ns;   /* [0] en alttaki */
static Istemci *mru[MAKS_I];   static int nm;   /* [0] en son odaklanan */
static Istemci *sira[MAKS_I];  static int nsr;  /* açılış sırası (_NET_CLIENT_LIST) */
static Istemci *odak;
static int masaustu_gosteriliyor;

/* sürükleme */
enum { S_YOK, S_TASI, S_BOYUT, S_DUGME };
static struct { int tip, yon, px, py, x, y, w, h, basladi; Istemci *c; } sr;
static Time son_tik; static Istemci *son_tik_c; static int son_tik_x, son_tik_y;
static int super_yalniz;

/* menü */
enum { MO_OGE, MO_AYRAC, MO_BASLIK, MO_ALT };
enum { E_YOK, E_CALISTIR, E_MENU_KOK, E_MENU_GUC, E_MENU_PENCERE, E_KAPAT, E_SONRAKI, E_ONCEKI,
       E_MASAUSTU, E_BUYUT, E_KUCULT, E_SOL, E_SAG, E_PMENU, E_ETKINLESTIR, E_USTTE, E_TAM };
typedef struct Menu Menu;
typedef struct { int tur, eylem, pasif, y, h; char etiket[200], komut[400]; Picture ikon; Istemci *c; Menu *alt; } MenuOge;
struct Menu { MenuOge *o; int n, kap; Window win; int x, y, w, h, sec; Menu *ust, *acik; };
static Menu *menu_kok;
static Time menu_zaman; static int menu_px, menu_py, menu_klavye;

/* Alt+Tab */
static Istemci *osd_l[MAKS_I]; static int osd_n, osd_i, osd_acik;
static Window osd_win;

static const char *t(const char *tr, const char *en) { return EN ? en : tr; }

/* ---------- hata işleyiciler ---------- */
static int baska_wm(Display *d, XErrorEvent *e) {
  (void)d;
  if (e->error_code == BadAccess) { fprintf(stderr, "yorunge: başka bir pencere yöneticisi çalışıyor\n"); exit(1); }
  return 0;
}
static int hata(Display *d, XErrorEvent *e) {
  if (e->error_code == BadWindow || e->error_code == BadDrawable || e->error_code == BadPixmap ||
      (e->request_code == X_SetInputFocus && e->error_code == BadMatch) ||
      (e->request_code == X_ConfigureWindow && e->error_code == BadMatch) ||
      (e->request_code == X_GrabButton && e->error_code == BadAccess) ||
      (e->request_code == X_GrabKey && e->error_code == BadAccess))
    return 0;
  char m[256]; XGetErrorText(d, e->error_code, m, sizeof m);
  fprintf(stderr, "yorunge: X hatası: %s (istek %d)\n", m, e->request_code);
  return 0;
}

/* ---------- yardımcılar ---------- */
static void calistir(const char *k) {
  if (!k || !*k) return;
  if (fork() == 0) {
    if (dpy) close(ConnectionNumber(dpy));
    setsid();
    signal(SIGCHLD, SIG_DFL);
    execl("/bin/sh", "sh", "-c", k, (char *)NULL);
    _exit(127);
  }
}

static Istemci *bul_win(Window w) {
  for (int i = 0; i < ns; i++) if (yigin[i]->win == w) return yigin[i];
  return NULL;
}
static Istemci *bul(Window w) {
  for (int i = 0; i < ns; i++) if (yigin[i]->win == w || (yigin[i]->cerceve && yigin[i]->cerceve == w)) return yigin[i];
  return NULL;
}
static const int tut_yon[12] = { UST, ALT, SOL, SAG, UST|SOL, UST|SOL, UST|SAG, UST|SAG, ALT|SOL, ALT|SOL, ALT|SAG, ALT|SAG };
static Istemci *bul_tut(Window w, int *yon) {
  for (int i = 0; i < ns; i++) for (int k = 0; k < 12; k++)
    if (yigin[i]->tut[k] && yigin[i]->tut[k] == w) { *yon = tut_yon[k]; return yigin[i]; }
  return NULL;
}
static int var_mi(Istemci *c) { for (int i = 0; i < ns; i++) if (yigin[i] == c) return 1; return 0; }
static void dizi_cikar(Istemci **d, int *n, Istemci *c) {
  for (int i = 0; i < *n; i++) if (d[i] == c) { memmove(&d[i], &d[i + 1], (*n - i - 1) * sizeof *d); (*n)--; return; }
}
static int imlec_yon(int y) {
  switch (y) {
    case UST: return IM_U; case ALT: return IM_A; case SOL: return IM_SOL; case SAG: return IM_SAG;
    case UST|SOL: return IM_USOL; case UST|SAG: return IM_USAG; case ALT|SOL: return IM_ASOL; case ALT|SAG: return IM_ASAG;
  }
  return IM_OK;
}
static unsigned temiz(unsigned m) {
  return m & ~(numlock | LockMask) & (ShiftMask | ControlMask | Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask);
}
static long zaman_ms(void) { struct timeval tv; gettimeofday(&tv, NULL); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }

static int metin_gen(XftFont *f, const char *s, int n) {
  XGlyphInfo g; XftTextExtentsUtf8(dpy, f, (const FcChar8 *)s, n, &g); return g.xOff;
}
/* metni genişliğe sığdır, gerekirse … ekle */
static void sigdir(XftFont *f, const char *s, int maks, char *out, size_t boy) {
  int n = strlen(s);
  if (maks <= 0) { out[0] = 0; return; }
  if (metin_gen(f, s, n) <= maks) { snprintf(out, boy, "%s", s); return; }
  int nk = metin_gen(f, "…", strlen("…"));
  while (n > 0) {
    n--; while (n > 0 && (s[n] & 0xC0) == 0x80) n--;
    if (metin_gen(f, s, n) + nk <= maks) break;
  }
  snprintf(out, boy, "%.*s…", n, s);
}

/* ---------- ARGB simgeler ---------- */
static Picture argb_resim(GdkPixbuf *pb, Pixmap *pm_out) {
  int w = gdk_pixbuf_get_width(pb), h = gdk_pixbuf_get_height(pb);
  int st = gdk_pixbuf_get_rowstride(pb), nc = gdk_pixbuf_get_n_channels(pb);
  int alfa = gdk_pixbuf_get_has_alpha(pb);
  const guchar *p = gdk_pixbuf_get_pixels(pb);
  uint32_t *buf = malloc(w * h * 4);
  if (!buf) return None;
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    const guchar *q = p + y * st + x * nc;
    unsigned a = alfa ? q[3] : 255;
    unsigned r = q[0] * a / 255, g = q[1] * a / 255, b = q[2] * a / 255;
    buf[y * w + x] = (a << 24) | (r << 16) | (g << 8) | b;
  }
  XImage *im = XCreateImage(dpy, vis, 32, ZPixmap, 0, (char *)buf, w, h, 32, 0);
  Pixmap pm = XCreatePixmap(dpy, root, w, h, 32);
  GC g = XCreateGC(dpy, pm, 0, NULL);
  XPutImage(dpy, pm, g, im, 0, 0, 0, 0, w, h);
  XFreeGC(dpy, g);
  XDestroyImage(im);
  *pm_out = pm;
  return XRenderCreatePicture(dpy, pm, fmt_argb, 0, NULL);
}

static struct { char yol[256]; Picture p; Pixmap pm; } ikon_ob[160];
static int nik;
static Picture ikon_dosya(const char *yol) {
  if (!yol || !*yol) return None;
  for (int i = 0; i < nik; i++) if (!strcmp(ikon_ob[i].yol, yol)) return ikon_ob[i].p;
  if (nik >= (int)(sizeof ikon_ob / sizeof ikon_ob[0])) return None;
  GError *er = NULL;
  GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size(yol, IKON, IKON, &er);
  if (!pb) { if (er) g_error_free(er); return None; }
  Pixmap pm; Picture p = argb_resim(pb, &pm);
  g_object_unref(pb);
  snprintf(ikon_ob[nik].yol, sizeof ikon_ob[nik].yol, "%s", yol);
  ikon_ob[nik].p = p; ikon_ob[nik].pm = pm; nik++;
  return p;
}

static void ikon_bosalt(Istemci *c) {
  if (c->ikon && c->ikon_sahip) { XRenderFreePicture(dpy, c->ikon); XFreePixmap(dpy, c->ikon_pm); }
  c->ikon = None; c->ikon_pm = None; c->ikon_sahip = 0;
}

static void ikon_yukle(Istemci *c) {
  ikon_bosalt(c);
  Atom tip; int fmt; unsigned long n = 0, kalan; unsigned char *d = NULL;
  if (XGetWindowProperty(dpy, c->win, A[NET_WM_ICON], 0, 4 * 1024 * 1024, False, XA_CARDINAL,
                         &tip, &fmt, &n, &kalan, &d) == Success && d && n > 2 && fmt == 32) {
    long *v = (long *)d;
    unsigned long i = 0, en = (unsigned long)-1; long bw = 0, bh = 0;
    while (i + 2 < n) {
      long w = v[i], h = v[i + 1];
      if (w <= 0 || h <= 0 || (unsigned long)(w * h) > n - i - 2) break;
      int iyi;
      if (!bw) iyi = 1;
      else if (w >= IKON && (bw < IKON || w < bw)) iyi = 1;
      else if (bw < IKON && w > bw) iyi = 1;
      else iyi = 0;
      if (iyi) { en = i; bw = w; bh = h; }
      i += 2 + w * h;
    }
    if (en != (unsigned long)-1) {
      guchar *rgba = malloc(bw * bh * 4);
      if (rgba) {
        for (long k = 0; k < bw * bh; k++) {
          unsigned long px = (unsigned long)v[en + 2 + k];
          rgba[k * 4 + 0] = (px >> 16) & 255; rgba[k * 4 + 1] = (px >> 8) & 255;
          rgba[k * 4 + 2] = px & 255; rgba[k * 4 + 3] = (px >> 24) & 255;
        }
        GdkPixbuf *pb = gdk_pixbuf_new_from_data(rgba, GDK_COLORSPACE_RGB, TRUE, 8, bw, bh, bw * 4, NULL, NULL);
        GdkPixbuf *k = gdk_pixbuf_scale_simple(pb, IKON, IKON, GDK_INTERP_BILINEAR);
        if (k) { c->ikon = argb_resim(k, &c->ikon_pm); c->ikon_sahip = 1; g_object_unref(k); }
        g_object_unref(pb);
        free(rgba);
      }
    }
  }
  if (d) XFree(d);
  if (c->ikon) return;
  /* _NET_WM_ICON yoksa sınıf adına göre simge temasından bul */
  XClassHint ch = {0};
  if (XGetClassHint(dpy, c->win, &ch)) {
    const char *adlar[2] = { ch.res_name, ch.res_class };
    for (int a = 0; a < 2 && !c->ikon; a++) {
      if (!adlar[a]) continue;
      char ad[128]; snprintf(ad, sizeof ad, "%s", adlar[a]);
      for (char *p = ad; *p; p++) *p = (*p >= 'A' && *p <= 'Z') ? *p + 32 : *p;
      const char *dz[] = { "/usr/share/icons/hicolor/48x48/apps/%s.png", "/usr/share/icons/hicolor/scalable/apps/%s.svg",
                           "/usr/share/pixmaps/%s.png", "/usr/share/icons/Adwaita/48x48/legacy/%s.png" };
      for (unsigned k = 0; k < 4 && !c->ikon; k++) {
        char yol[256]; snprintf(yol, sizeof yol, dz[k], ad);
        if (access(yol, R_OK) == 0) c->ikon = ikon_dosya(yol);
      }
    }
    if (ch.res_name) XFree(ch.res_name);
    if (ch.res_class) XFree(ch.res_class);
  }
}

static void ikon_ciz(Picture ikon, Drawable d, int x, int y) {
  if (!ikon) return;
  Picture hedef = XRenderCreatePicture(dpy, d, fmt_ekran, 0, NULL);
  XRenderComposite(dpy, PictOpOver, ikon, None, hedef, 0, 0, 0, 0, x, y, IKON, IKON);
  XRenderFreePicture(dpy, hedef);
}

/* ---------- tema ---------- */
static void tema_adi_bul(char *ad, size_t boy) {
  char tema[32] = "acik", yol[512];
  const char *home = getenv("HOME");
  const char *dosyalar[2] = { "/etc/aether/ayarlar", NULL };
  if (home) { snprintf(yol, sizeof yol, "%s/.config/aether/ayarlar", home); dosyalar[1] = yol; }
  for (int i = 0; i < 2; i++) {
    if (!dosyalar[i]) continue;
    FILE *f = fopen(dosyalar[i], "r"); if (!f) continue;
    char s[256];
    while (fgets(s, sizeof s, f)) if (!strncmp(s, "TEMA=", 5)) { sscanf(s + 5, "%31[^\n \"]", tema); }
    fclose(f);
  }
  snprintf(ad, boy, "%s", !strcmp(tema, "koyu") ? "Aether-Koyu" : "Aether");
}

static Pixmap bitmap_ciz(int tur, int *w, int *h) {
  *w = *h = 10;
  Pixmap p = XCreatePixmap(dpy, root, 10, 10, 1);
  GC g = XCreateGC(dpy, p, 0, NULL);
  XSetForeground(dpy, g, 0); XFillRectangle(dpy, p, g, 0, 0, 10, 10);
  XSetForeground(dpy, g, 1);
  switch (tur) {
    case BM_KAPAT:
      XSetLineAttributes(dpy, g, 2, LineSolid, CapButt, JoinMiter);
      XDrawLine(dpy, p, g, 1, 1, 8, 8); XDrawLine(dpy, p, g, 8, 1, 1, 8); break;
    case BM_KUCULT: XFillRectangle(dpy, p, g, 1, 7, 8, 2); break;
    case BM_BUYUT: XDrawRectangle(dpy, p, g, 1, 1, 7, 7); XFillRectangle(dpy, p, g, 1, 1, 8, 2); break;
    default: XDrawRectangle(dpy, p, g, 1, 3, 5, 5); XDrawLine(dpy, p, g, 3, 1, 8, 1); XDrawLine(dpy, p, g, 8, 1, 8, 6); break;
  }
  XFreeGC(dpy, g);
  return p;
}

static XftFont *yazi_ac(const char *a, const char *b) {
  XftFont *f = XftFontOpenName(dpy, scr, a);
  return f ? f : XftFontOpenName(dpy, scr, b);
}

static void tema_yukle(void) {
  char ad[64], yol[512];
  tema_adi_bul(ad, sizeof ad);
  const char *deger[R_SAYI];
  static char tampon[R_SAYI][128];
  for (int i = 0; i < R_SAYI; i++) deger[i] = renk_tanim[i].varsayilan;
  kenar_k = 1; pad_w = 8; pad_h = 5;
  snprintf(yol, sizeof yol, "/usr/share/themes/%s/yorunge/themerc", ad);
  FILE *f = fopen(yol, "r");
  if (f) {
    char s[512];
    while (fgets(s, sizeof s, f)) {
      char k[128], v[128];
      if (s[0] == '#' || sscanf(s, " %127[^: \t] : %127s", k, v) != 2) continue;
      if (!strcmp(k, "border.width")) kenar_k = atoi(v);
      else if (!strcmp(k, "padding.width")) pad_w = atoi(v);
      else if (!strcmp(k, "padding.height")) pad_h = atoi(v);
      else for (int i = 0; i < R_SAYI; i++) if (!strcmp(k, renk_tanim[i].anahtar)) {
        snprintf(tampon[i], sizeof tampon[i], "%s", v); deger[i] = tampon[i];
      }
    }
    fclose(f);
  } else fprintf(stderr, "yorunge: tema bulunamadı: %s (varsayılan renkler)\n", yol);
  for (int i = 0; i < R_SAYI; i++) {
    if (renk_var) XftColorFree(dpy, vis, cmap, &R[i]);
    if (!XftColorAllocName(dpy, vis, cmap, deger[i], &R[i])) XftColorAllocName(dpy, vis, cmap, renk_tanim[i].varsayilan, &R[i]);
  }
  renk_var = 1;
  const char *bmad[BM_SAYI] = { "close", "iconify", "max", "max_toggled" };
  for (int i = 0; i < BM_SAYI; i++) {
    if (bm[i]) XFreePixmap(dpy, bm[i]);
    unsigned w, h; int xh, yh; Pixmap p;
    snprintf(yol, sizeof yol, "/usr/share/themes/%s/yorunge/%s.xbm", ad, bmad[i]);
    if (XReadBitmapFile(dpy, root, yol, &w, &h, &p, &xh, &yh) == BitmapSuccess) { bm[i] = p; bm_w[i] = w; bm_h[i] = h; }
    else bm[i] = bitmap_ciz(i, &bm_w[i], &bm_h[i]);
  }
  if (!f_aktif) {
    f_aktif = yazi_ac("JetBrains Mono:size=9:weight=bold", "monospace:size=9:bold");
    f_pasif = yazi_ac("JetBrains Mono:size=9", "monospace:size=9");
    f_menu = yazi_ac("JetBrains Mono:size=10", "monospace:size=10");
    f_menu_bas = yazi_ac("JetBrains Mono:size=9:weight=bold", "monospace:size=9:bold");
  }
  TH = MAX(f_aktif->ascent + f_aktif->descent, IKON) + 2 * pad_h + 1;
}

/* ---------- EWMH özellikleri ---------- */
static void ozellik_ayarla(Window w, int atom, Atom tip, int fmt, const void *veri, int n) {
  XChangeProperty(dpy, w, A[atom], tip, fmt, PropModeReplace, (const unsigned char *)veri, n);
}

static void liste_guncelle(void) {
  Window ws[MAKS_I]; int n = 0;
  for (int i = 0; i < nsr; i++) ws[n++] = sira[i]->win;
  ozellik_ayarla(root, NET_CLIENT_LIST, XA_WINDOW, 32, ws, n);
  n = 0;
  for (int i = 0; i < ns; i++) ws[n++] = yigin[i]->win;
  ozellik_ayarla(root, NET_CLIENT_LIST_STACKING, XA_WINDOW, 32, ws, n);
}

static void durum_yaz(Istemci *c) {
  Atom d[16]; int n = 0;
  for (int i = 0; i < c->nek; i++) d[n++] = c->ek_durum[i];
  if (c->tam) d[n++] = A[NET_WM_STATE_FULLSCREEN];
  if (c->buyuk) { d[n++] = A[NET_WM_STATE_MAXIMIZED_VERT]; d[n++] = A[NET_WM_STATE_MAXIMIZED_HORZ]; }
  if (c->kucuk) d[n++] = A[NET_WM_STATE_HIDDEN];
  if (c->ustte) d[n++] = A[NET_WM_STATE_ABOVE];
  if (c == odak) d[n++] = A[NET_WM_STATE_FOCUSED];
  ozellik_ayarla(c->win, NET_WM_STATE, XA_ATOM, 32, d, n);
}

static void wm_state(Istemci *c, long s) {
  long d[2] = { s, None };
  XChangeProperty(dpy, c->win, A[WM_STATE], A[WM_STATE], 32, PropModeReplace, (unsigned char *)d, 2);
}

static void kapsam_yaz(Window w, int b, int th) {
  long e[4] = { b, b, th + b, b };
  ozellik_ayarla(w, NET_FRAME_EXTENTS, XA_CARDINAL, 32, e, 4);
}

static void calisma_hesapla(void);

/* ---------- istemci bilgileri ---------- */
static void ad_guncelle(Istemci *c) {
  c->ad[0] = 0;
  Atom tip; int fmt; unsigned long n, kalan; unsigned char *d = NULL;
  if (XGetWindowProperty(dpy, c->win, A[NET_WM_NAME], 0, 1024, False, A[UTF8_STRING], &tip, &fmt, &n, &kalan, &d) == Success && d) {
    if (n) snprintf(c->ad, sizeof c->ad, "%s", (char *)d);
    XFree(d);
  }
  if (c->ad[0]) return;
  XTextProperty tp;
  if (XGetWMName(dpy, c->win, &tp) && tp.value) {
    char **l = NULL; int ln = 0;
    if (tp.encoding == XA_STRING) snprintf(c->ad, sizeof c->ad, "%s", (char *)tp.value);
    else if (Xutf8TextPropertyToTextList(dpy, &tp, &l, &ln) >= Success && ln > 0 && l) {
      snprintf(c->ad, sizeof c->ad, "%s", l[0]); XFreeStringList(l);
    }
    XFree(tp.value);
  }
}

static void ipucu_guncelle(Istemci *c) {
  XSizeHints h; long s;
  c->minw = c->minh = c->maxw = c->maxh = c->incw = c->inch = c->basew = c->baseh = 0;
  if (!XGetWMNormalHints(dpy, c->win, &h, &s)) return;
  if (h.flags & PBaseSize) { c->basew = h.base_width; c->baseh = h.base_height; }
  else if (h.flags & PMinSize) { c->basew = h.min_width; c->baseh = h.min_height; }
  if (h.flags & PResizeInc) { c->incw = h.width_inc; c->inch = h.height_inc; }
  if (h.flags & PMaxSize) { c->maxw = h.max_width; c->maxh = h.max_height; }
  if (h.flags & PMinSize) { c->minw = h.min_width; c->minh = h.min_height; }
  else if (h.flags & PBaseSize) { c->minw = h.base_width; c->minh = h.base_height; }
}

static int boyutlanabilir(Istemci *c) { return !(c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh); }

static void boyut_sinirla(Istemci *c, int *w, int *h) {
  int mw = MAX(c->minw, 40), mh = MAX(c->minh, 20);
  if (c->incw > 1) *w = c->basew + (*w - c->basew) / c->incw * c->incw;
  if (c->inch > 1) *h = c->baseh + (*h - c->baseh) / c->inch * c->inch;
  if (*w < mw) *w = mw;
  if (*h < mh) *h = mh;
  if (c->maxw > 0 && *w > c->maxw) *w = c->maxw;
  if (c->maxh > 0 && *h > c->maxh) *h = c->maxh;
}

static void wmhints_oku(Istemci *c) {
  c->girdi = 1;
  XWMHints *h = XGetWMHints(dpy, c->win);
  if (h) { if (h->flags & InputHint) c->girdi = h->input; XFree(h); }
}

static void protokol_oku(Istemci *c) {
  Atom *p; int n;
  c->silinebilir = c->takefocus = 0;
  if (XGetWMProtocols(dpy, c->win, &p, &n)) {
    for (int i = 0; i < n; i++) {
      if (p[i] == A[WM_DELETE_WINDOW]) c->silinebilir = 1;
      if (p[i] == A[WM_TAKE_FOCUS]) c->takefocus = 1;
    }
    XFree(p);
  }
}

static Atom tur_oku(Window w) {
  Atom tip, r = 0; int fmt; unsigned long n, kalan; unsigned char *d = NULL;
  if (XGetWindowProperty(dpy, w, A[NET_WM_WINDOW_TYPE], 0, 8, False, XA_ATOM, &tip, &fmt, &n, &kalan, &d) == Success && d) {
    if (n) r = ((Atom *)d)[0];
    XFree(d);
  }
  return r;
}

static int motif_dekorsuz(Window w) {
  Atom tip; int fmt; unsigned long n, kalan; unsigned char *d = NULL; int r = 0;
  if (XGetWindowProperty(dpy, w, A[MOTIF_WM_HINTS], 0, 5, False, AnyPropertyType, &tip, &fmt, &n, &kalan, &d) == Success && d) {
    long *v = (long *)d;
    if (n >= 3 && (v[0] & 2) && v[2] == 0) r = 1;
    XFree(d);
  }
  return r;
}

static void strut_oku(Istemci *c) {
  memset(c->strut, 0, sizeof c->strut);
  Atom tip; int fmt; unsigned long n, kalan; unsigned char *d = NULL;
  if ((XGetWindowProperty(dpy, c->win, A[NET_WM_STRUT_PARTIAL], 0, 12, False, XA_CARDINAL, &tip, &fmt, &n, &kalan, &d) == Success && d && n >= 4) ||
      (d && (XFree(d), d = NULL, 0)) ||
      (XGetWindowProperty(dpy, c->win, A[NET_WM_STRUT], 0, 4, False, XA_CARDINAL, &tip, &fmt, &n, &kalan, &d) == Success && d && n >= 4)) {
    for (int i = 0; i < 4; i++) c->strut[i] = ((long *)d)[i];
  }
  if (d) XFree(d);
}

static void durum_oku(Istemci *c) {
  Atom tip; int fmt; unsigned long n, kalan; unsigned char *d = NULL;
  if (XGetWindowProperty(dpy, c->win, A[NET_WM_STATE], 0, 32, False, XA_ATOM, &tip, &fmt, &n, &kalan, &d) == Success && d) {
    for (unsigned long i = 0; i < n; i++) {
      Atom a = ((Atom *)d)[i];
      if (a == A[NET_WM_STATE_FULLSCREEN]) c->tam = 1;
      else if (a == A[NET_WM_STATE_MAXIMIZED_VERT] || a == A[NET_WM_STATE_MAXIMIZED_HORZ]) c->buyuk = 1;
      else if (a == A[NET_WM_STATE_ABOVE]) c->ustte = 1;
      else if (a != A[NET_WM_STATE_HIDDEN] && a != A[NET_WM_STATE_FOCUSED] && c->nek < 8) c->ek_durum[c->nek++] = a;
    }
    XFree(d);
  }
}

static long wm_state_oku(Window w) {
  Atom tip; int fmt; unsigned long n, kalan; unsigned char *d = NULL; long r = -1;
  if (XGetWindowProperty(dpy, w, A[WM_STATE], 0, 2, False, A[WM_STATE], &tip, &fmt, &n, &kalan, &d) == Success && d) {
    if (n) r = ((long *)d)[0];
    XFree(d);
  }
  return r;
}

static void protokol_gonder(Istemci *c, Atom p) {
  XEvent e = {0};
  e.type = ClientMessage; e.xclient.window = c->win; e.xclient.message_type = A[WM_PROTOCOLS];
  e.xclient.format = 32; e.xclient.data.l[0] = p; e.xclient.data.l[1] = CurrentTime;
  XSendEvent(dpy, c->win, False, NoEventMask, &e);
}

/* ---------- çizim ---------- */
static int dugme_gen(Istemci *c) { return c->gth - 1; }

/* 0: yok, 1: küçült, 2: büyüt, 3: kapat, 4: simge */
static int dugme_bul(Istemci *c, int x, int y) {
  if (!c->dekor || c->gth == 0 || y < 0 || y >= c->gth - 1 || x < 0) return 0;
  int bw = dugme_gen(c), w = c->gw;
  if (x >= w - bw) return 3;
  if (x >= w - 2 * bw) return 2;
  if (x >= w - 3 * bw) return 1;
  if (c->ikon && x >= pad_w && x < pad_w + IKON) return 4;
  return 0;
}

static void cerceve_ciz(Istemci *c) {
  if (c->ozel || !c->cerceve) return;
  int akt = (c == odak);
  XSetWindowBorder(dpy, c->cerceve, R[akt ? R_KENAR_A : R_KENAR_I].pixel);
  if (c->gth == 0 || c->kucuk || c->gw <= 0) return;
  int w = c->gw, h = c->gth, bh = h - 1, bw = dugme_gen(c);
  Pixmap pm = XCreatePixmap(dpy, c->cerceve, w, h, derinlik);
  XSetForeground(dpy, gc, R[akt ? R_BASLIK_A : R_BASLIK_I].pixel);
  XFillRectangle(dpy, pm, gc, 0, 0, w, h);
  XSetForeground(dpy, gc, R[akt ? R_AYRAC_A : R_AYRAC_I].pixel);
  XDrawLine(dpy, pm, gc, 0, h - 1, w, h - 1);

  /* düğmeler: küçült, büyüt, kapat */
  for (int d = 1; d <= 3; d++) {
    int x = w - (4 - d) * bw;
    int b = d == 3 ? BM_KAPAT : d == 1 ? BM_KUCULT : (c->buyuk ? BM_BUYUT2 : BM_BUYUT);
    int pasif = (d == 2 && !boyutlanabilir(c));
    unsigned long on = R[akt ? R_DUG_A : R_DUG_I].pixel;
    if (d == 2 && c->buyuk && akt) on = R[R_DUG_ACIK].pixel;
    if (pasif) on = R[R_DUG_PASIF].pixel;
    else if (c->bas_dug == d && c->ust_dug == d) {
      XSetForeground(dpy, gc, R[R_DUG_BAS_BG].pixel); XFillRectangle(dpy, pm, gc, x + 2, 2, bw - 4, bh - 4);
      on = R[R_DUG_BAS_FG].pixel;
    } else if (c->ust_dug == d && !c->bas_dug) {
      XSetForeground(dpy, gc, R[R_DUG_UST_BG].pixel); XFillRectangle(dpy, pm, gc, x + 2, 2, bw - 4, bh - 4);
      on = R[R_DUG_UST_FG].pixel;
    }
    int bx = x + (bw - bm_w[b]) / 2, by = (bh - bm_h[b]) / 2;
    XSetForeground(dpy, gc, on);
    XSetClipMask(dpy, gc, bm[b]); XSetClipOrigin(dpy, gc, bx, by);
    XFillRectangle(dpy, pm, gc, bx, by, bm_w[b], bm_h[b]);
    XSetClipMask(dpy, gc, None);
  }

  int tx = pad_w;
  if (c->ikon) { ikon_ciz(c->ikon, pm, tx, (bh - IKON) / 2); tx += IKON + 6; }
  XftFont *f = akt ? f_aktif : f_pasif;
  char s[300];
  sigdir(f, c->ad[0] ? c->ad : "Aether", w - 3 * bw - tx - 6, s, sizeof s);
  XftDraw *xd = XftDrawCreate(dpy, pm, vis, cmap);
  XftDrawStringUtf8(xd, &R[akt ? R_YAZI_A : R_YAZI_I], f, tx, (bh - (f->ascent + f->descent)) / 2 + f->ascent,
                    (const FcChar8 *)s, strlen(s));
  XftDrawDestroy(xd);
  XCopyArea(dpy, pm, c->cerceve, gc, 0, 0, w, h, 0, 0);
  XFreePixmap(dpy, pm);
}

/* ---------- yerleşim ---------- */
static void bildir(Istemci *c) {
  XConfigureEvent ce = {0};
  ce.type = ConfigureNotify; ce.display = dpy; ce.event = c->win; ce.window = c->win;
  ce.x = c->gx; ce.y = c->gy; ce.width = c->gw; ce.height = c->gh;
  ce.border_width = 0; ce.above = None; ce.override_redirect = False;
  XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent *)&ce);
}

static void geometri_hesapla(Istemci *c) {
  if (c->tam) { c->gb = 0; c->gth = 0; c->gx = 0; c->gy = 0; c->gw = sw; c->gh = sh; return; }
  c->gb = c->dekor ? kenar_k : 0;
  c->gth = c->dekor ? TH : 0;
  if (c->buyuk) {
    c->gx = calisma.x + c->gb; c->gy = calisma.y + c->gb + c->gth;
    c->gw = calisma.w - 2 * c->gb; c->gh = calisma.h - c->gth - 2 * c->gb;
  } else { c->gx = c->x; c->gy = c->y; c->gw = c->w; c->gh = c->h; }
  if (c->gw < 1) c->gw = 1;
  if (c->gh < 1) c->gh = 1;
}

static void tutamak_yerlestir(Istemci *c) {
  if (c->ozel) return;
  int ac = c->dekor && !c->tam && !c->buyuk && !c->kucuk && boyutlanabilir(c);
  if (!ac) { for (int i = 0; i < 12; i++) XUnmapWindow(dpy, c->tut[i]); return; }
  int X = c->gx - c->gb, Y = c->gy - c->gth - c->gb, W = c->gw + 2 * c->gb, H = c->gh + c->gth + 2 * c->gb;
  int K = TUT_K, C = TUT_C;
  int r[12][4] = {
    { X + C, Y - K, W - 2 * C, K }, { X + C, Y + H, W - 2 * C, K },
    { X - K, Y + C, K, H - 2 * C }, { X + W, Y + C, K, H - 2 * C },
    { X - K, Y - K, C + K, K }, { X - K, Y, K, C },
    { X + W - C, Y - K, C + K, K }, { X + W, Y, K, C },
    { X - K, Y + H, C + K, K }, { X - K, Y + H - C, K, C },
    { X + W - C, Y + H, C + K, K }, { X + W, Y + H - C, K, C },
  };
  for (int i = 0; i < 12; i++) {
    XMoveResizeWindow(dpy, c->tut[i], r[i][0], r[i][1], MAX(1, r[i][2]), MAX(1, r[i][3]));
    XMapWindow(dpy, c->tut[i]);
  }
}

static void yerlestir(Istemci *c) {
  geometri_hesapla(c);
  if (c->ozel) return;
  XSetWindowBorderWidth(dpy, c->cerceve, c->gb);
  XMoveResizeWindow(dpy, c->cerceve, c->gx - c->gb, c->gy - c->gth - c->gb, c->gw, c->gh + c->gth);
  XMoveResizeWindow(dpy, c->win, 0, c->gth, c->gw, c->gh);
  tutamak_yerlestir(c);
  kapsam_yaz(c->win, c->gb, c->gth);
  bildir(c);
  cerceve_ciz(c);
}

/* ---------- yığın ---------- */
static int katman(Istemci *c) {
  if (c->tur == A[NET_WM_WINDOW_TYPE_DESKTOP]) return 0;
  if (c->tam && c == odak) return 4;
  if (c->ozel || c->ustte) return 3;
  return 2;
}

static void menu_one(void);
static int sec_uygun(MenuOge *o);

static void yigin_uygula(void) {
  static Window ws[MAKS_I * 13 + 4];
  int n = 0;
  for (int k = 4; k >= 0; k--)
    for (int i = ns - 1; i >= 0; i--) {
      Istemci *c = yigin[i];
      if (katman(c) != k) continue;
      if (!c->ozel) for (int t2 = 0; t2 < 12; t2++) ws[n++] = c->tut[t2];
      ws[n++] = c->ozel ? c->win : c->cerceve;
    }
  if (n) { XRaiseWindow(dpy, ws[0]); XRestackWindows(dpy, ws, n); }
  menu_one();
  if (osd_acik) XRaiseWindow(dpy, osd_win);
  liste_guncelle();
}

static void one_al(Istemci *c) {
  if (!c) return;
  dizi_cikar(yigin, &ns, c); yigin[ns++] = c;
  /* geçici (diyalog) pencereler sahibinin üstünde kalsın */
  for (int i = 0; i < ns - 1; i++) {
    Istemci *d = yigin[i];
    if (d->gecici == c->win) { dizi_cikar(yigin, &ns, d); yigin[ns++] = d; i--; }
  }
  yigin_uygula();
}

static void alta_al(Istemci *c) {
  dizi_cikar(yigin, &ns, c);
  memmove(&yigin[1], &yigin[0], ns * sizeof *yigin); yigin[0] = c; ns++;
  yigin_uygula();
}

/* ---------- odak ---------- */
static void dugme_yakala(Istemci *c, int odakli) {
  if (c->ozel) return;
  XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
  if (!odakli) XGrabButton(dpy, AnyButton, AnyModifier, c->win, False, ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);
}

static Istemci *sonraki_odak(Istemci *haric) {
  for (int i = 0; i < nm; i++) {
    Istemci *c = mru[i];
    if (c != haric && !c->kucuk && !c->odaksiz && !c->ozel) return c;
  }
  return NULL;
}

static void odakla(Istemci *c) {
  if (c && (c->kucuk || c->odaksiz)) c = NULL;
  Istemci *eski = odak;
  odak = c;
  if (eski && eski != c && var_mi(eski)) { dugme_yakala(eski, 0); cerceve_ciz(eski); durum_yaz(eski); }
  if (c) {
    dizi_cikar(mru, &nm, c);
    memmove(&mru[1], &mru[0], nm * sizeof *mru); mru[0] = c; nm++;
    dugme_yakala(c, 1);
    if (c->girdi) XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
    else if (!c->takefocus) XSetInputFocus(dpy, kontrol, RevertToPointerRoot, CurrentTime);
    if (c->takefocus) protokol_gonder(c, A[WM_TAKE_FOCUS]);
    ozellik_ayarla(root, NET_ACTIVE_WINDOW, XA_WINDOW, 32, &c->win, 1);
    cerceve_ciz(c); durum_yaz(c);
  } else {
    XSetInputFocus(dpy, kontrol, RevertToPointerRoot, CurrentTime);
    Window yok = None; ozellik_ayarla(root, NET_ACTIVE_WINDOW, XA_WINDOW, 32, &yok, 1);
  }
  if ((eski && eski != c && eski->tam) || (c && c->tam)) yigin_uygula();
}

/* ---------- durumlar ---------- */
static void buyut_ayarla(Istemci *c, int v) {
  if (c->ozel || c->tam || (v && !boyutlanabilir(c)) || c->buyuk == v) return;
  c->buyuk = v; yerlestir(c); durum_yaz(c);
}
static void tam_ayarla(Istemci *c, int v) {
  if (c->ozel || c->tam == v) return;
  c->tam = v; yerlestir(c); durum_yaz(c); yigin_uygula();
}
static void kucult(Istemci *c, int v) {
  if (c->ozel || c->kucuk == v) return;
  c->kucuk = v;
  if (v) {
    c->yoksay_unmap++;
    XUnmapWindow(dpy, c->win); XUnmapWindow(dpy, c->cerceve);
    tutamak_yerlestir(c);
    wm_state(c, IconicState);
    if (odak == c) odakla(sonraki_odak(c));
  } else {
    c->masaustu_gizli = 0;
    XMapWindow(dpy, c->win); XMapWindow(dpy, c->cerceve);
    tutamak_yerlestir(c);
    wm_state(c, NormalState);
    cerceve_ciz(c);
  }
  durum_yaz(c);
}
static void kapat(Istemci *c) {
  if (!c) return;
  if (c->silinebilir) protokol_gonder(c, A[WM_DELETE_WINDOW]);
  else XKillClient(dpy, c->win);
}
static void yari(Istemci *c, int sag) {
  if (!c || c->ozel || c->tam || !boyutlanabilir(c)) return;
  int b = c->dekor ? kenar_k : 0, th = c->dekor ? TH : 0, W = calisma.w / 2;
  c->buyuk = 0;
  c->x = calisma.x + (sag ? calisma.w - W : 0) + b; c->y = calisma.y + th + b;
  c->w = W - 2 * b; c->h = calisma.h - th - 2 * b;
  yerlestir(c); durum_yaz(c);
}
static void masaustu_goster(int v) {
  if (v) {
    int say = 0;
    for (int i = 0; i < ns; i++) { Istemci *c = yigin[i]; if (!c->ozel && !c->kucuk) { kucult(c, 1); c->masaustu_gizli = 1; say++; } }
    masaustu_gosteriliyor = say > 0;
  } else {
    for (int i = 0; i < ns; i++) { Istemci *c = yigin[i]; if (c->masaustu_gizli) kucult(c, 0); }
    Istemci *c = sonraki_odak(NULL); if (c) { odakla(c); one_al(c); }
    masaustu_gosteriliyor = 0;
  }
  long m = masaustu_gosteriliyor; ozellik_ayarla(root, NET_SHOWING_DESKTOP, XA_CARDINAL, 32, &m, 1);
}
static void etkinlestir(Istemci *c) {
  if (!c || !var_mi(c)) return;
  if (c->kucuk) kucult(c, 0);
  odakla(c); one_al(c);
}

/* çözünürlük ya da çalışma alanı değişince pencereler ekranda kalsın: gerekirse küçült, sonra içeri kaydır */
static void ekrana_sigdir(void) {
  for (int i = 0; i < ns; i++) {
    Istemci *c = yigin[i];
    if (c->ozel || c->buyuk || c->tam) continue;
    int b = c->dekor ? kenar_k : 0, th = c->dekor ? TH : 0;
    int enw = calisma.w - 2 * b, enh = calisma.h - th - 2 * b;
    int w = MIN(c->w, enw), h = MIN(c->h, enh);
    if (w != c->w || h != c->h) { boyut_sinirla(c, &w, &h); c->w = MIN(w, enw); c->h = MIN(h, enh); }
    int sol = calisma.x + b, ust = calisma.y + th + b;
    int sag = calisma.x + calisma.w - b, alt = calisma.y + calisma.h - b;
    if (c->x + c->w > sag) c->x = sag - c->w;
    if (c->y + c->h > alt) c->y = alt - c->h;
    if (c->x < sol) c->x = sol;
    if (c->y < ust) c->y = ust;
    yerlestir(c);
  }
}

static void calisma_hesapla(void) {
  long l = 0, r = 0, u = 0, a = 0;
  for (int i = 0; i < ns; i++) if (yigin[i]->ozel) {
    l = MAX(l, yigin[i]->strut[0]); r = MAX(r, yigin[i]->strut[1]);
    u = MAX(u, yigin[i]->strut[2]); a = MAX(a, yigin[i]->strut[3]);
  }
  calisma.x = l; calisma.y = u; calisma.w = sw - l - r; calisma.h = sh - u - a;
  long wa[4] = { calisma.x, calisma.y, calisma.w, calisma.h };
  ozellik_ayarla(root, NET_WORKAREA, XA_CARDINAL, 32, wa, 4);
  for (int i = 0; i < ns; i++) if (yigin[i]->buyuk || yigin[i]->tam) yerlestir(yigin[i]);
  ekrana_sigdir();
}

/* ---------- yönetim ---------- */
static void osd_cikar(Istemci *c);
static void menu_kapat(void);

static void yonet(Window w, XWindowAttributes *wa) {
  if (bul(w) || ns >= MAKS_I) return;
  Istemci *c = calloc(1, sizeof *c);
  c->win = w;
  c->tur = tur_oku(w);
  ad_guncelle(c); ipucu_guncelle(c); wmhints_oku(c); protokol_oku(c);
  Window g = None;
  if (XGetTransientForHint(dpy, w, &g) && g != None && g != root && g != w) c->gecici = g;
  c->dekor = 1;
  Atom tr = c->tur;
  if (tr == A[NET_WM_WINDOW_TYPE_DOCK]) { c->ozel = 1; c->dekor = 0; c->odaksiz = 1; }
  /* masaüstü (aether-masaustu): çerçevesiz, en altta, listelerde yok; ama tıklanınca klavye odağı alabilsin (F2, Del, Enter) */
  if (tr == A[NET_WM_WINDOW_TYPE_DESKTOP]) { c->ozel = 1; c->dekor = 0; }
  if (tr == A[NET_WM_WINDOW_TYPE_SPLASH] || tr == A[NET_WM_WINDOW_TYPE_NOTIFICATION] || tr == A[NET_WM_WINDOW_TYPE_TOOLTIP] ||
      tr == A[NET_WM_WINDOW_TYPE_MENU] || tr == A[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] || tr == A[NET_WM_WINDOW_TYPE_POPUP_MENU] ||
      tr == A[NET_WM_WINDOW_TYPE_COMBO] || tr == A[NET_WM_WINDOW_TYPE_DND]) c->dekor = 0;
  if (tr == A[NET_WM_WINDOW_TYPE_NOTIFICATION] || tr == A[NET_WM_WINDOW_TYPE_TOOLTIP] || tr == A[NET_WM_WINDOW_TYPE_DND]) c->odaksiz = 1;
  if (motif_dekorsuz(w)) c->dekor = 0;
  durum_oku(c);
  XClassHint ch = {0};
  if (XGetClassHint(dpy, w, &ch)) {
    if (ch.res_name && !strcmp(ch.res_name, "aether-yildizlar")) { c->dekor = 0; c->tam = 1; }
    if (ch.res_name) XFree(ch.res_name);
    if (ch.res_class) XFree(ch.res_class);
  }
  ikon_yukle(c);
  strut_oku(c);
  c->eski_kenar = wa->border_width;
  c->x = wa->x; c->y = wa->y; c->w = wa->width; c->h = wa->height;

  if (c->ozel) {
    XSelectInput(dpy, w, PropertyChangeMask);
    yigin[ns++] = c;
  sira[nsr++] = c;
    XMapWindow(dpy, w);
    wm_state(c, NormalState);
    calisma_hesapla(); yigin_uygula();
    return;
  }

  int b = c->dekor ? kenar_k : 0, th = c->dekor ? TH : 0;
  boyut_sinirla(c, &c->w, &c->h);
  if (c->w > calisma.w - 2 * b) c->w = calisma.w - 2 * b;
  if (c->h > calisma.h - th - 2 * b) c->h = calisma.h - th - 2 * b;
  XSizeHints sh2; long sup;
  int konumlu = XGetWMNormalHints(dpy, w, &sh2, &sup) && (sh2.flags & (USPosition | PPosition)) && (wa->x || wa->y);
  Istemci *p = c->gecici ? bul_win(c->gecici) : NULL;
  if (wa->map_state == IsViewable) {
    /* yöneticiden önce açılmış pencere (ör. Yörünge yeniden başladı): yerinde kalsın */
    c->x = wa->x; c->y = wa->y;
  } else if (p && !p->kucuk && !p->ozel) {   /* masaüstünün pencereleri ekranın ortasında açılsın */
    c->x = p->gx + (p->gw - c->w) / 2; c->y = p->gy + (p->gh - c->h) / 2;
  } else if (konumlu) {
    if (!((sh2.flags & PWinGravity) && sh2.win_gravity == StaticGravity)) { c->x = wa->x + b; c->y = wa->y + th + b; }
  } else {
    c->x = calisma.x + (calisma.w - c->w) / 2;
    c->y = calisma.y + (calisma.h - c->h - th) / 2 + th;
    for (int deneme = 0; deneme < 20; deneme++) {
      int cak = 0;
      for (int i = 0; i < ns; i++) if (abs(yigin[i]->x - c->x) < 4 && abs(yigin[i]->y - c->y) < 4) cak = 1;
      if (!cak) break;
      c->x += 24; c->y += 24;
    }
  }
  if (c->x + c->w + b > calisma.x + calisma.w) c->x = calisma.x + calisma.w - c->w - b;
  if (c->y + c->h + b > calisma.y + calisma.h) c->y = calisma.y + calisma.h - c->h - b;
  if (c->x - b < calisma.x) c->x = calisma.x + b;
  if (c->y - th - b < calisma.y) c->y = calisma.y + th + b;

  XSetWindowAttributes a;
  a.override_redirect = True;
  a.background_pixel = R[R_BASLIK_I].pixel;
  a.border_pixel = R[R_KENAR_I].pixel;
  a.cursor = imlec[IM_OK];
  a.event_mask = SubstructureRedirectMask | SubstructureNotifyMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | LeaveWindowMask | ExposureMask;
  c->cerceve = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, derinlik, InputOutput, vis,
                             CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWCursor | CWEventMask, &a);
  for (int i = 0; i < 12; i++) {
    XSetWindowAttributes ta;
    ta.override_redirect = True; ta.event_mask = ButtonPressMask; ta.cursor = imlec[imlec_yon(tut_yon[i])];
    c->tut[i] = XCreateWindow(dpy, root, 0, 0, 1, 1, 0, 0, InputOnly, CopyFromParent,
                              CWOverrideRedirect | CWEventMask | CWCursor, &ta);
  }
  XAddToSaveSet(dpy, w);
  XSetWindowBorderWidth(dpy, w, 0);
  XSelectInput(dpy, w, PropertyChangeMask);
  if (wa->map_state == IsViewable) c->yoksay_unmap++;
  XReparentWindow(dpy, w, c->cerceve, 0, th);
  unsigned mods[] = { 0, LockMask, numlock, numlock | LockMask };
  for (int i = 0; i < 4; i++) {
    XGrabButton(dpy, Button1, Mod1Mask | mods[i], c->cerceve, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button3, Mod1Mask | mods[i], c->cerceve, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
  }
  dugme_yakala(c, 0);
  long masa = 0; ozellik_ayarla(w, NET_WM_DESKTOP, XA_CARDINAL, 32, &masa, 1);
  Atom izin[] = { A[NET_WM_ACTION_MOVE], A[NET_WM_ACTION_RESIZE], A[NET_WM_ACTION_MINIMIZE], A[NET_WM_ACTION_MAXIMIZE_HORZ],
                  A[NET_WM_ACTION_MAXIMIZE_VERT], A[NET_WM_ACTION_FULLSCREEN], A[NET_WM_ACTION_CLOSE], A[NET_WM_ACTION_ABOVE] };
  ozellik_ayarla(w, NET_WM_ALLOWED_ACTIONS, XA_ATOM, 32, izin, 8);

  yigin[ns++] = c;
  sira[nsr++] = c;
  mru[nm++] = c;
  yerlestir(c);
  XMapWindow(dpy, w);
  XMapWindow(dpy, c->cerceve);
  wm_state(c, NormalState);
  durum_yaz(c);
  one_al(c);
  XWMHints *h = XGetWMHints(dpy, w);
  int ikonik = (h && (h->flags & StateHint) && h->initial_state == IconicState) || wm_state_oku(w) == IconicState;
  if (h) XFree(h);
  if (ikonik) kucult(c, 1);
  else if (!c->odaksiz) odakla(c);
}

static void birak(Istemci *c, int yok_edildi) {
  if (menu_kok) menu_kapat();
  osd_cikar(c);
  if (sr.c == c) { XUngrabPointer(dpy, CurrentTime); sr.tip = S_YOK; sr.c = NULL; }
  if (son_tik_c == c) son_tik_c = NULL;
  dizi_cikar(yigin, &ns, c);
  dizi_cikar(mru, &nm, c);
  dizi_cikar(sira, &nsr, c);
  if (!yok_edildi) {
    XGrabServer(dpy);
    if (!c->ozel) {
      XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
      XReparentWindow(dpy, c->win, root, c->gx, c->gy);
      XSetWindowBorderWidth(dpy, c->win, c->eski_kenar);
      XRemoveFromSaveSet(dpy, c->win);
    }
    wm_state(c, WithdrawnState);
    XSync(dpy, False);
    XUngrabServer(dpy);
  }
  if (!c->ozel) {
    XDestroyWindow(dpy, c->cerceve);
    for (int i = 0; i < 12; i++) XDestroyWindow(dpy, c->tut[i]);
  }
  ikon_bosalt(c);
  int ozel = c->ozel;
  if (odak == c) { odak = NULL; free(c); odakla(sonraki_odak(NULL)); }
  else free(c);
  if (ozel) calisma_hesapla();
  liste_guncelle();
}

/* ---------- menü ---------- */
static int oge_h(int tur) {
  if (tur == MO_AYRAC) return 7;
  if (tur == MO_BASLIK) return f_menu_bas->ascent + f_menu_bas->descent + 8;
  return MAX(f_menu->ascent + f_menu->descent, IKON) + 8;
}
static MenuOge *menu_ekle(Menu *m, int tur, const char *etiket) {
  if (m->n == m->kap) { m->kap = m->kap ? m->kap * 2 : 16; m->o = realloc(m->o, m->kap * sizeof *m->o); }
  MenuOge *o = &m->o[m->n++];
  memset(o, 0, sizeof *o);
  o->tur = tur;
  if (etiket) snprintf(o->etiket, sizeof o->etiket, "%s", etiket);
  return o;
}
static Menu *menu_yeni(void) { Menu *m = calloc(1, sizeof *m); m->sec = -1; return m; }
static void menu_sil(Menu *m) {
  if (!m) return;
  for (int i = 0; i < m->n; i++) if (m->o[i].alt) menu_sil(m->o[i].alt);
  if (m->win) XDestroyWindow(dpy, m->win);
  free(m->o); free(m);
}

static void pencere_listesi(Menu *m) {
  int say = 0;
  for (int i = 0; i < nm; i++) {
    Istemci *c = mru[i];
    if (c->ozel || c->odaksiz) continue;
    char e[300];
    snprintf(e, sizeof e, c->kucuk ? "(%s)" : "%s", c->ad[0] ? c->ad : "Aether");
    MenuOge *o = menu_ekle(m, MO_OGE, e);
    o->eylem = E_ETKINLESTIR; o->c = c; o->ikon = c->ikon;
    say++;
  }
  if (!say) { MenuOge *o = menu_ekle(m, MO_OGE, t("(açık pencere yok)", "(no windows)")); o->pasif = 1; }
}

/* aether-menu çıktısı: B<TAB>başlık · O<TAB>etiket<TAB>simge<TAB>komut · - · M<TAB>etiket<TAB>simge · E · P<TAB>etiket<TAB>simge */
static Menu *menu_komuttan(const char *komut) {
  Menu *st[8]; int d = 0;
  st[0] = menu_yeni();
  FILE *p = popen(komut, "r");
  if (p) {
    char s[1024];
    while (fgets(s, sizeof s, p)) {
      s[strcspn(s, "\n")] = 0;
      char *f[4] = { s, "", "", "" }; int nf = 1;
      for (char *q = s; *q && nf < 4; q++) if (*q == '\t') { *q = 0; f[nf++] = q + 1; }
      Menu *m = st[d];
      switch (s[0]) {
        case 'B': menu_ekle(m, MO_BASLIK, f[1]); break;
        case '-': if (m->n) menu_ekle(m, MO_AYRAC, NULL); break;
        case 'O': { MenuOge *o = menu_ekle(m, MO_OGE, f[1]); o->ikon = ikon_dosya(f[2]); snprintf(o->komut, sizeof o->komut, "%s", f[3]); o->eylem = E_CALISTIR; break; }
        case 'M': case 'P': {
          MenuOge *o = menu_ekle(m, MO_ALT, f[1]); o->ikon = ikon_dosya(f[2]);
          o->alt = menu_yeni(); o->alt->ust = m;
          if (s[0] == 'P') pencere_listesi(o->alt);
          else if (d < 7) st[++d] = o->alt;
          break;
        }
        case 'E': if (d > 0) d--; break;
      }
    }
    pclose(p);
  }
  if (!st[0]->n) {
    MenuOge *o = menu_ekle(st[0], MO_BASLIK, "A E T H E R"); (void)o;
    o = menu_ekle(st[0], MO_OGE, "Terminal"); o->eylem = E_CALISTIR; snprintf(o->komut, sizeof o->komut, "aether-terminal");
    o = menu_ekle(st[0], MO_OGE, t("Oturumu Kapat", "Log Out")); o->eylem = E_CALISTIR; snprintf(o->komut, sizeof o->komut, "pkill -x yorunge");
  }
  return st[0];
}

static void menu_olc(Menu *m) {
  int gen = 0, y = 1;
  for (int i = 0; i < m->n; i++) {
    MenuOge *o = &m->o[i];
    o->y = y; o->h = oge_h(o->tur); y += o->h;
    int w;
    if (o->tur == MO_BASLIK) w = pad_w + metin_gen(f_menu_bas, o->etiket, strlen(o->etiket)) + pad_w;
    else if (o->tur == MO_AYRAC) w = 0;
    else w = 8 + IKON + 8 + metin_gen(f_menu, o->etiket, strlen(o->etiket)) + 28;
    gen = MAX(gen, w);
  }
  m->w = MIN(MAX(gen, 180), sw / 2);
  m->h = y + 1;
}

static void menu_ciz(Menu *m) {
  if (!m->win) return;
  Pixmap pm = XCreatePixmap(dpy, m->win, m->w, m->h, derinlik);
  XSetForeground(dpy, gc, R[R_M_BG].pixel); XFillRectangle(dpy, pm, gc, 0, 0, m->w, m->h);
  XftDraw *xd = XftDrawCreate(dpy, pm, vis, cmap);
  for (int i = 0; i < m->n; i++) {
    MenuOge *o = &m->o[i];
    char s[300];
    if (o->tur == MO_AYRAC) {
      XSetForeground(dpy, gc, R[R_M_AYRAC].pixel);
      XDrawLine(dpy, pm, gc, 6, o->y + o->h / 2, m->w - 7, o->y + o->h / 2);
    } else if (o->tur == MO_BASLIK) {
      XSetForeground(dpy, gc, R[R_M_BAS_BG].pixel); XFillRectangle(dpy, pm, gc, 0, o->y, m->w, o->h);
      sigdir(f_menu_bas, o->etiket, m->w - 2 * pad_w, s, sizeof s);
      XftDrawStringUtf8(xd, &R[R_M_BAS_FG], f_menu_bas, pad_w, o->y + (o->h - f_menu_bas->ascent - f_menu_bas->descent) / 2 + f_menu_bas->ascent,
                        (const FcChar8 *)s, strlen(s));
    } else {
      int secili = (i == m->sec && !o->pasif);
      if (secili) { XSetForeground(dpy, gc, R[R_M_SEC_BG].pixel); XFillRectangle(dpy, pm, gc, 0, o->y, m->w, o->h); }
      if (o->ikon) ikon_ciz(o->ikon, pm, 8, o->y + (o->h - IKON) / 2);
      int tx = 8 + IKON + 8;
      sigdir(f_menu, o->etiket, m->w - tx - 24, s, sizeof s);
      XftColor *rk = o->pasif ? &R[R_M_SOLUK] : secili ? &R[R_M_SEC_FG] : &R[R_M_FG];
      XftDrawStringUtf8(xd, rk, f_menu, tx, o->y + (o->h - f_menu->ascent - f_menu->descent) / 2 + f_menu->ascent,
                        (const FcChar8 *)s, strlen(s));
      if (o->tur == MO_ALT) {
        int ax = m->w - 14, ay = o->y + o->h / 2;
        XPoint p[3] = { { ax, ay - 4 }, { ax + 4, ay }, { ax, ay + 4 } };
        XSetForeground(dpy, gc, rk->pixel);
        XFillPolygon(dpy, pm, gc, p, 3, Convex, CoordModeOrigin);
      }
    }
  }
  XftDrawDestroy(xd);
  XCopyArea(dpy, pm, m->win, gc, 0, 0, m->w, m->h, 0, 0);
  XFreePixmap(dpy, pm);
}

static void menu_pencere(Menu *m, int x, int y) {
  menu_olc(m);
  int alt = calisma.y + calisma.h;
  if (x + m->w + 2 > sw) x = sw - m->w - 2;
  if (y + m->h + 2 > alt) y = alt - m->h - 2;
  if (x < 0) x = 0;
  if (y < calisma.y) y = calisma.y;
  m->x = x; m->y = y;
  XSetWindowAttributes a;
  a.override_redirect = True; a.save_under = True; a.border_pixel = R[R_M_KENAR].pixel;
  a.background_pixel = R[R_M_BG].pixel; a.event_mask = ExposureMask; a.cursor = imlec[IM_OK];
  m->win = XCreateWindow(dpy, root, x, y, m->w, m->h, 1, derinlik, InputOutput, vis,
                         CWOverrideRedirect | CWSaveUnder | CWBorderPixel | CWBackPixel | CWEventMask | CWCursor, &a);
  XMapRaised(dpy, m->win);
  menu_ciz(m);
}

static void menu_one(void) { for (Menu *m = menu_kok; m; m = m->acik) if (m->win) XRaiseWindow(dpy, m->win); }

static void menu_kapat(void) {
  if (!menu_kok) return;
  menu_sil(menu_kok); menu_kok = NULL;
  XUngrabPointer(dpy, CurrentTime); XUngrabKeyboard(dpy, CurrentTime);
}

static int sec_uygun(MenuOge *o) { return (o->tur == MO_OGE || o->tur == MO_ALT) && !o->pasif; }

static void alt_kapat(Menu *m) {
  if (m->acik) {
    alt_kapat(m->acik);
    if (m->acik->win) { XDestroyWindow(dpy, m->acik->win); m->acik->win = None; }
    m->acik->sec = -1;
    m->acik = NULL;
  }
}

static void alt_ac(Menu *m, int i) {
  MenuOge *o = &m->o[i];
  if (o->tur != MO_ALT || !o->alt) return;
  if (m->acik == o->alt) return;
  alt_kapat(m);
  Menu *a = o->alt;
  menu_olc(a);
  int x = m->x + m->w + 1, y = m->y + o->y - 1;
  if (x + a->w + 2 > sw) x = m->x - a->w - 1;
  menu_pencere(a, x, y);
  m->acik = a;
}

static void menu_goster(Menu *m, int x, int y) {
  menu_kapat();
  menu_kok = m;
  menu_pencere(m, x, y);
  for (int i = 0; i < 200; i++) {
    if (XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, None, imlec[IM_OK], CurrentTime) == GrabSuccess) break;
    usleep(1000);
  }
  for (int i = 0; i < 200; i++) {
    if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess) break;
    usleep(1000);
  }
  menu_zaman = zaman_ms(); menu_px = x; menu_py = y;
  if (menu_klavye) {
    for (int i = 0; i < m->n; i++) if (sec_uygun(&m->o[i])) { m->sec = i; break; }
    menu_ciz(m);
  }
  menu_klavye = 0;
}

static Menu *menu_derin(void) { Menu *m = menu_kok; while (m && m->acik) m = m->acik; return m; }

static Menu *menu_nokta(int x, int y, int *idx) {
  Menu *bulunan = NULL;
  for (Menu *m = menu_kok; m; m = m->acik) {
    if (m->win && x >= m->x && x < m->x + m->w + 2 && y >= m->y && y < m->y + m->h + 2) {
      bulunan = m; *idx = -1;
      int yy = y - m->y - 1;
      for (int i = 0; i < m->n; i++) if (yy >= m->o[i].y && yy < m->o[i].y + m->o[i].h) *idx = i;
    }
  }
  return bulunan;
}

static void eylem_yap(int e, const char *komut, Istemci *c);

static void oge_calistir(Menu *m, int i) {
  MenuOge *o = &m->o[i];
  if (!sec_uygun(o)) return;
  if (o->tur == MO_ALT) {
    alt_ac(m, i);
    Menu *a = o->alt;
    for (int k = 0; k < a->n; k++) if (sec_uygun(&a->o[k])) { a->sec = k; break; }
    menu_ciz(a);
    return;
  }
  int e = o->eylem; char k[400]; snprintf(k, sizeof k, "%s", o->komut); Istemci *c = o->c;
  menu_kapat();
  eylem_yap(e, k, c);
}

static void menu_hareket(int x, int y) {
  int i; Menu *m = menu_nokta(x, y, &i);
  if (!m) return;
  int yeni = (i >= 0 && sec_uygun(&m->o[i])) ? i : -1;
  if (yeni != m->sec) {
    m->sec = yeni;
    if (m->acik && (yeni < 0 || m->o[yeni].alt != m->acik)) alt_kapat(m);
    if (yeni >= 0 && m->o[yeni].tur == MO_ALT) alt_ac(m, yeni);
    menu_ciz(m);
  }
  /* üst menülerde, açık alt menüye giden öğe seçili görünsün */
  for (Menu *u = m->ust; u; u = u->ust) {
    for (int k = 0; k < u->n; k++) if (u->o[k].alt == (u == m->ust ? m : u->acik) && u->sec != k) { u->sec = k; menu_ciz(u); }
  }
}

static void menu_tus(XKeyEvent *e) {
  KeySym ks = XkbKeycodeToKeysym(dpy, e->keycode, 0, 0);
  Menu *m = menu_derin();
  if (!m) return;
  if (ks == XK_Escape) {
    if (m->ust) { Menu *u = m->ust; alt_kapat(u); menu_ciz(u); }
    else menu_kapat();
  } else if (ks == XK_Down || ks == XK_Up || ks == XK_Tab) {
    int yon = (ks == XK_Up || (ks == XK_Tab && (e->state & ShiftMask))) ? -1 : 1;
    int i = m->sec;
    for (int k = 0; k < m->n; k++) {
      i = (i + yon + m->n) % m->n;
      if (i < 0) i = 0;
      if (sec_uygun(&m->o[i])) break;
    }
    m->sec = i;
    menu_ciz(m);
  } else if (ks == XK_Right) {
    if (m->sec >= 0 && m->o[m->sec].tur == MO_ALT) oge_calistir(m, m->sec);
  } else if (ks == XK_Left) {
    if (m->ust) { Menu *u = m->ust; alt_kapat(u); menu_ciz(u); }
  } else if (ks == XK_Return || ks == XK_KP_Enter || ks == XK_space) {
    if (m->sec >= 0) oge_calistir(m, m->sec);
  } else if (ks == XK_Super_L || ks == XK_Super_R) {
    menu_kapat();
  } else {
    /* harfle atla: tek eşleşme varsa doğrudan çalıştır */
    char ch[8]; KeySym k2; int n = XLookupString(e, ch, sizeof ch, &k2, NULL);
    if (n == 1 && (unsigned char)ch[0] > ' ' && (unsigned char)ch[0] < 127) {
      int c0 = tolower((unsigned char)ch[0]), say = 0, ilk = -1, sonraki = -1;
      for (int i = 0; i < m->n; i++) {
        if (!sec_uygun(&m->o[i]) || tolower((unsigned char)m->o[i].etiket[0]) != c0) continue;
        say++; if (ilk < 0) ilk = i; if (i > m->sec && sonraki < 0) sonraki = i;
      }
      if (say) {
        m->sec = sonraki >= 0 ? sonraki : ilk;
        if (m->acik) alt_kapat(m);
        menu_ciz(m);
        if (say == 1) oge_calistir(m, m->sec);
      }
    }
  }
}

static void menu_buton(XButtonEvent *e, int birakma) {
  int i; Menu *m = menu_nokta(e->x_root, e->y_root, &i);
  if (!birakma) {
    if (!m) menu_kapat();
    return;
  }
  if (e->button > 3) return;
  if (!m || i < 0) return;
  int hareketli = abs(e->x_root - menu_px) > 4 || abs(e->y_root - menu_py) > 4;
  if (zaman_ms() - menu_zaman < 250 && !hareketli) return;
  oge_calistir(m, i);
}

static void kok_menu(int x, int y) {
  Menu *m = menu_komuttan("aether-menu yorunge");
  menu_goster(m, x, y);
}
static void guc_menu(int x, int y) {
  Menu *m = menu_komuttan("aether-menu yorunge guc");
  menu_goster(m, x, y);
}
static void liste_menu(int x, int y) {
  Menu *m = menu_yeni();
  menu_ekle(m, MO_BASLIK, t("Pencereler", "Windows"));
  pencere_listesi(m);
  menu_goster(m, x, y);
}
static void istemci_menu(Istemci *c, int x, int y) {
  if (!c || c->ozel) return;
  Menu *m = menu_yeni();
  MenuOge *o;
  o = menu_ekle(m, MO_OGE, t("Küçült", "Minimize")); o->eylem = E_KUCULT; o->c = c;
  o = menu_ekle(m, MO_OGE, c->buyuk ? t("Önceki Boyut", "Restore") : t("Büyüt", "Maximize")); o->eylem = E_BUYUT; o->c = c;
  o->pasif = !boyutlanabilir(c) || c->tam;
  o = menu_ekle(m, MO_OGE, c->tam ? t("Tam Ekrandan Çık", "Leave Full Screen") : t("Tam Ekran", "Full Screen")); o->eylem = E_TAM; o->c = c;
  o = menu_ekle(m, MO_OGE, c->ustte ? t("✓ Her Zaman Üstte", "✓ Always on Top") : t("Her Zaman Üstte", "Always on Top")); o->eylem = E_USTTE; o->c = c;
  menu_ekle(m, MO_AYRAC, NULL);
  o = menu_ekle(m, MO_OGE, t("Kapat", "Close")); o->eylem = E_KAPAT; o->c = c;
  menu_goster(m, x, y);
}

/* ---------- Alt+Tab ---------- */
static int osd_satir(void) { return MAX(f_menu->ascent + f_menu->descent, IKON) + 10; }

static void osd_ciz(void) {
  int sh2 = osd_satir(), w = 0;
  for (int i = 0; i < osd_n; i++) w = MAX(w, metin_gen(f_menu, osd_l[i]->ad, strlen(osd_l[i]->ad)));
  w = MIN(MAX(w + IKON + 48, 320), sw * 6 / 10);
  int bas = f_menu_bas->ascent + f_menu_bas->descent + 12;
  int h = bas + osd_n * sh2 + 8;
  if (h > sh - 40) h = sh - 40;
  int x = (sw - w) / 2, y = (sh - h) / 2;
  if (!osd_win) {
    XSetWindowAttributes a;
    a.override_redirect = True; a.border_pixel = R[R_M_KENAR].pixel; a.background_pixel = R[R_M_BG].pixel;
    a.event_mask = ExposureMask;
    osd_win = XCreateWindow(dpy, root, x, y, w, h, 1, derinlik, InputOutput, vis,
                            CWOverrideRedirect | CWBorderPixel | CWBackPixel | CWEventMask, &a);
  }
  XSetWindowBorder(dpy, osd_win, R[R_M_KENAR].pixel);
  XMoveResizeWindow(dpy, osd_win, x, y, w, h);
  XMapRaised(dpy, osd_win);
  Pixmap pm = XCreatePixmap(dpy, osd_win, w, h, derinlik);
  XSetForeground(dpy, gc, R[R_M_BG].pixel); XFillRectangle(dpy, pm, gc, 0, 0, w, h);
  XSetForeground(dpy, gc, R[R_M_BAS_BG].pixel); XFillRectangle(dpy, pm, gc, 0, 0, w, bas);
  XftDraw *xd = XftDrawCreate(dpy, pm, vis, cmap);
  const char *b = "A E T H E R";
  XftDrawStringUtf8(xd, &R[R_M_BAS_FG], f_menu_bas, pad_w + 4, (bas - f_menu_bas->ascent - f_menu_bas->descent) / 2 + f_menu_bas->ascent,
                    (const FcChar8 *)b, strlen(b));
  int ilk = 0, sigan = (h - bas - 8) / sh2;
  if (osd_i >= sigan) ilk = osd_i - sigan + 1;
  for (int i = ilk; i < osd_n && i - ilk < sigan; i++) {
    Istemci *c = osd_l[i];
    int yy = bas + 4 + (i - ilk) * sh2;
    int sec = (i == osd_i);
    if (sec) { XSetForeground(dpy, gc, R[R_M_SEC_BG].pixel); XFillRectangle(dpy, pm, gc, 4, yy, w - 8, sh2); }
    if (c->ikon) ikon_ciz(c->ikon, pm, 12, yy + (sh2 - IKON) / 2);
    char e[300], s[300];
    snprintf(e, sizeof e, c->kucuk ? "(%s)" : "%s", c->ad[0] ? c->ad : "Aether");
    sigdir(f_menu, e, w - 12 - IKON - 10 - 12, s, sizeof s);
    XftDrawStringUtf8(xd, sec ? &R[R_M_SEC_FG] : (c->kucuk ? &R[R_M_SOLUK] : &R[R_M_FG]), f_menu,
                      12 + IKON + 10, yy + (sh2 - f_menu->ascent - f_menu->descent) / 2 + f_menu->ascent,
                      (const FcChar8 *)s, strlen(s));
  }
  XftDrawDestroy(xd);
  XCopyArea(dpy, pm, osd_win, gc, 0, 0, w, h, 0, 0);
  XFreePixmap(dpy, pm);
}

static void osd_bitir(int sec) {
  if (!osd_acik) return;
  osd_acik = 0;
  XUngrabKeyboard(dpy, CurrentTime);
  if (osd_win) XUnmapWindow(dpy, osd_win);
  if (sec && osd_i < osd_n) etkinlestir(osd_l[osd_i]);
}

static void osd_cikar(Istemci *c) {
  if (!osd_acik) return;
  for (int i = 0; i < osd_n; i++) if (osd_l[i] == c) {
    memmove(&osd_l[i], &osd_l[i + 1], (osd_n - i - 1) * sizeof *osd_l); osd_n--;
    if (osd_i >= i && osd_i > 0) osd_i--;
    break;
  }
  if (!osd_n) osd_bitir(0); else osd_ciz();
}

static void osd_basla(int yon) {
  osd_n = 0;
  for (int i = 0; i < nm; i++) if (!mru[i]->ozel && !mru[i]->odaksiz) osd_l[osd_n++] = mru[i];
  if (!osd_n) return;
  if (XGrabKeyboard(dpy, root, True, GrabModeAsync, GrabModeAsync, CurrentTime) != GrabSuccess) return;
  osd_acik = 1;
  osd_i = osd_n > 1 ? (yon > 0 ? 1 : osd_n - 1) : 0;
  osd_ciz();
  char km[32]; XQueryKeymap(dpy, km);
  KeyCode a1 = XKeysymToKeycode(dpy, XK_Alt_L), a2 = XKeysymToKeycode(dpy, XK_Alt_R);
  int basili = (a1 && (km[a1 / 8] & (1 << (a1 % 8)))) || (a2 && (km[a2 / 8] & (1 << (a2 % 8))));
  if (!basili) osd_bitir(1);
}

static void osd_tus(XKeyEvent *e) {
  KeySym ks = XkbKeycodeToKeysym(dpy, e->keycode, 0, 0);
  if (ks == XK_Tab || ks == XK_Down || ks == XK_Right) {
    int yon = (ks == XK_Tab && (e->state & ShiftMask)) ? -1 : 1;
    osd_i = (osd_i + yon + osd_n) % osd_n; osd_ciz();
  } else if (ks == XK_Up || ks == XK_Left) {
    osd_i = (osd_i - 1 + osd_n) % osd_n; osd_ciz();
  } else if (ks == XK_Escape) osd_bitir(0);
  else if (ks == XK_Return) osd_bitir(1);
}

/* ---------- kısayollar ---------- */
static const struct { unsigned mod; KeySym ks; int eylem; const char *komut; } kisayol[] = {
  { Mod4Mask, XK_space, E_MENU_KOK, NULL },
  { ControlMask | Mod1Mask, XK_t, E_CALISTIR, "aether-terminal" },
  { Mod4Mask, XK_Return, E_CALISTIR, "aether-terminal" },
  { Mod4Mask, XK_e, E_CALISTIR, "aether-dosyalar" },
  { Mod4Mask, XK_i, E_CALISTIR, "aether-ayarlar" },
  { Mod4Mask, XK_l, E_CALISTIR, "aether-guc kilitle" },
  { ControlMask | Mod1Mask, XK_Delete, E_MENU_GUC, NULL },
  { ControlMask | ShiftMask, XK_Escape, E_CALISTIR, "aether-gorev" },
  { 0, XK_Print, E_CALISTIR, "aether-ekran-goruntusu" },
  { ShiftMask, XK_Print, E_CALISTIR, "aether-ekran-goruntusu --alan" },
  { Mod1Mask, XK_F4, E_KAPAT, NULL },
  { Mod1Mask, XK_Tab, E_SONRAKI, NULL },
  { Mod1Mask | ShiftMask, XK_Tab, E_ONCEKI, NULL },
  { Mod4Mask, XK_d, E_MASAUSTU, NULL },
  { Mod4Mask, XK_Up, E_BUYUT, NULL },
  { Mod4Mask, XK_Down, E_KUCULT, NULL },
  { Mod4Mask, XK_Left, E_SOL, NULL },
  { Mod4Mask, XK_Right, E_SAG, NULL },
  { Mod1Mask, XK_space, E_PMENU, NULL },
  { 0, XF86XK_AudioRaiseVolume, E_CALISTIR, "command -v amixer >/dev/null && amixer -q set Master 5%+ unmute" },
  { 0, XF86XK_AudioLowerVolume, E_CALISTIR, "command -v amixer >/dev/null && amixer -q set Master 5%-" },
  { 0, XF86XK_AudioMute, E_CALISTIR, "command -v amixer >/dev/null && amixer -q set Master toggle" },
};

static void numlock_bul(void) {
  numlock = 0;
  XModifierKeymap *mm = XGetModifierMapping(dpy);
  KeyCode nl = XKeysymToKeycode(dpy, XK_Num_Lock);
  for (int i = 0; i < 8; i++) for (int k = 0; k < mm->max_keypermod; k++)
    if (nl && mm->modifiermap[i * mm->max_keypermod + k] == nl) numlock = 1 << i;
  XFreeModifiermap(mm);
}

static void tus_yakala(void) {
  numlock_bul();
  XUngrabKey(dpy, AnyKey, AnyModifier, root);
  unsigned mods[] = { 0, LockMask, numlock, numlock | LockMask };
  for (unsigned i = 0; i < sizeof kisayol / sizeof kisayol[0]; i++) {
    KeyCode kc = XKeysymToKeycode(dpy, kisayol[i].ks);
    if (!kc) continue;
    for (int m = 0; m < 4; m++) XGrabKey(dpy, kc, kisayol[i].mod | mods[m], root, True, GrabModeAsync, GrabModeAsync);
  }
  KeySym sup[2] = { XK_Super_L, XK_Super_R };
  for (int i = 0; i < 2; i++) {
    KeyCode kc = XKeysymToKeycode(dpy, sup[i]);
    if (kc) for (int m = 0; m < 4; m++) XGrabKey(dpy, kc, mods[m], root, True, GrabModeAsync, GrabModeAsync);
  }
}

static void imlec_konum(int *x, int *y) {
  Window r, c; int wx, wy; unsigned m;
  if (!XQueryPointer(dpy, root, &r, &c, x, y, &wx, &wy, &m)) { *x = sw / 2; *y = sh / 2; }
}

static void eylem_yap(int e, const char *komut, Istemci *c) {
  int x, y;
  switch (e) {
    case E_CALISTIR: calistir(komut); break;
    case E_MENU_KOK: imlec_konum(&x, &y); menu_klavye = 1; kok_menu(x, y); break;
    case E_MENU_GUC: menu_klavye = 1; guc_menu(sw / 2 - 90, sh / 3); break;
    case E_MENU_PENCERE: imlec_konum(&x, &y); liste_menu(x, y); break;
    case E_KAPAT: kapat(c ? c : odak); break;
    case E_SONRAKI: osd_basla(1); break;
    case E_ONCEKI: osd_basla(-1); break;
    case E_MASAUSTU: masaustu_goster(!masaustu_gosteriliyor); break;
    case E_BUYUT: if ((c = c ? c : odak)) buyut_ayarla(c, !c->buyuk); break;
    case E_KUCULT: if ((c = c ? c : odak)) kucult(c, 1); break;
    case E_SOL: yari(odak, 0); break;
    case E_SAG: yari(odak, 1); break;
    case E_PMENU: if (odak) { menu_klavye = 1; istemci_menu(odak, odak->gx - odak->gb, odak->gy - odak->gb); } break;
    case E_ETKINLESTIR: etkinlestir(c); break;
    case E_USTTE: if (c && var_mi(c)) { c->ustte = !c->ustte; durum_yaz(c); yigin_uygula(); } break;
    case E_TAM: if (c && var_mi(c)) tam_ayarla(c, !c->tam); break;
  }
}

/* ---------- sürükleme ---------- */
static void surukle_basla(Istemci *c, int tip, int yon, int px, int py) {
  if (!c || c->ozel || c->tam) return;
  if (tip == S_BOYUT && (!boyutlanabilir(c) || c->buyuk)) return;
  int im = tip == S_TASI ? IM_TASI : tip == S_BOYUT ? imlec_yon(yon) : IM_OK;
  if (XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                   GrabModeAsync, GrabModeAsync, None, imlec[im], CurrentTime) != GrabSuccess) return;
  sr.tip = tip; sr.c = c; sr.yon = yon; sr.px = px; sr.py = py;
  sr.x = c->x; sr.y = c->y; sr.w = c->w; sr.h = c->h; sr.basladi = 0;
}

static void surukle_hareket(int xr, int yr) {
  Istemci *c = sr.c;
  if (!c) return;
  if (sr.tip == S_DUGME) {
    int d = dugme_bul(c, xr - c->gx, yr - (c->gy - c->gth));
    int ust = (d == c->bas_dug) ? d : 0;
    if (ust != c->ust_dug) { c->ust_dug = ust; cerceve_ciz(c); }
    return;
  }
  int dx = xr - sr.px, dy = yr - sr.py;
  if (!sr.basladi) {
    if (abs(dx) < 3 && abs(dy) < 3) return;
    sr.basladi = 1;
    if (sr.tip == S_TASI && c->buyuk) {
      double oran = (double)(sr.px - (c->gx - c->gb)) / MAX(1, c->gw + 2 * c->gb);
      int ust_ofs = sr.py - (c->gy - c->gth - c->gb);
      c->buyuk = 0;
      c->x = xr - (int)(oran * c->w);
      c->y = yr - ust_ofs + c->gth + c->gb;
      sr.x = c->x; sr.y = c->y; sr.px = xr; sr.py = yr; dx = dy = 0;
      yerlestir(c); durum_yaz(c);
    }
  }
  int b = c->gb, th = c->gth;
  if (sr.tip == S_TASI) {
    int nx = sr.x + dx, ny = sr.y + dy;
    int fl = nx - b, ft = ny - th - b, fr = nx + c->w + b, fa = ny + c->h + b;
    if (abs(fl - calisma.x) < YAKIN) nx = calisma.x + b;
    else if (abs(fr - (calisma.x + calisma.w)) < YAKIN) nx = calisma.x + calisma.w - c->w - b;
    if (abs(ft - calisma.y) < YAKIN) ny = calisma.y + th + b;
    else if (abs(fa - (calisma.y + calisma.h)) < YAKIN) ny = calisma.y + calisma.h - c->h - b;
    if (ny - th - b < calisma.y) ny = calisma.y + th + b;        /* başlık ekranın üstünden kaçmasın */
    if (ny - th > calisma.y + calisma.h - 8) ny = calisma.y + calisma.h - 8 + th;
    c->x = nx; c->y = ny;
    c->gx = nx; c->gy = ny;
    XMoveWindow(dpy, c->cerceve, nx - b, ny - th - b);
    tutamak_yerlestir(c);
  } else if (sr.tip == S_BOYUT) {
    int w = sr.w, h = sr.h;
    if (sr.yon & SAG) w = sr.w + dx;
    if (sr.yon & SOL) w = sr.w - dx;
    if (sr.yon & ALT) h = sr.h + dy;
    if (sr.yon & UST) h = sr.h - dy;
    boyut_sinirla(c, &w, &h);
    c->x = (sr.yon & SOL) ? sr.x + sr.w - w : sr.x;
    c->y = (sr.yon & UST) ? sr.y + sr.h - h : sr.y;
    c->w = w; c->h = h;
    yerlestir(c);
  }
}

static void surukle_bitir(int xr, int yr) {
  Istemci *c = sr.c;
  int tip = sr.tip;
  sr.tip = S_YOK; sr.c = NULL;
  XUngrabPointer(dpy, CurrentTime);
  if (!c) return;
  if (tip == S_DUGME) {
    int d = dugme_bul(c, xr - c->gx, yr - (c->gy - c->gth));
    int b = c->bas_dug;
    c->bas_dug = 0; c->ust_dug = d;
    cerceve_ciz(c);
    if (d == b) {
      if (d == 1) kucult(c, 1);
      else if (d == 2) buyut_ayarla(c, !c->buyuk);
      else if (d == 3) kapat(c);
    }
  } else if (sr.basladi || tip == S_BOYUT) bildir(c);
}

/* ---------- olaylar ---------- */
static void ev_buton(XButtonEvent *e) {
  super_yalniz = 0;
  if (menu_kok) { menu_buton(e, 0); return; }
  if (osd_acik) return;
  if (sr.tip != S_YOK) return;
  int yon; Istemci *c;
  if (e->window == root) {
    if (e->button == Button3) kok_menu(e->x_root, e->y_root);
    else if (e->button == Button2) liste_menu(e->x_root, e->y_root);
    else if (e->button == Button1 && odak) odakla(NULL);
    return;
  }
  if ((c = bul_tut(e->window, &yon))) {
    odakla(c); one_al(c);
    if (e->button == Button1) surukle_basla(c, S_BOYUT, yon, e->x_root, e->y_root);
    return;
  }
  c = bul(e->window);
  if (!c) return;
  if (e->window == c->win) {
    odakla(c); one_al(c);
    XAllowEvents(dpy, ReplayPointer, e->time);
    return;
  }
  if ((e->state & Mod1Mask) && (e->button == Button1 || e->button == Button3)) {
    odakla(c); one_al(c);
    if (e->button == Button1) surukle_basla(c, S_TASI, 0, e->x_root, e->y_root);
    else {
      int cx = c->gx + c->gw / 2, cy = c->gy + c->gh / 2;
      int y = (e->x_root < cx ? SOL : SAG) | (e->y_root < cy ? UST : ALT);
      surukle_basla(c, S_BOYUT, y, e->x_root, e->y_root);
    }
    return;
  }
  if (e->y >= c->gth) return;
  int d = dugme_bul(c, e->x, e->y);
  if (e->button == Button1) {
    odakla(c); one_al(c);
    if (d >= 1 && d <= 3) {
      if (d == 2 && !boyutlanabilir(c)) return;
      if (XGrabPointer(dpy, root, False, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                       GrabModeAsync, GrabModeAsync, None, imlec[IM_OK], CurrentTime) == GrabSuccess) {
        sr.tip = S_DUGME; sr.c = c; c->bas_dug = d; c->ust_dug = d; cerceve_ciz(c);
      }
      return;
    }
    if (d == 4) { istemci_menu(c, c->gx - c->gb, c->gy - c->gb); return; }
    if (son_tik_c == c && e->time - son_tik < CIFT_TIK && abs(e->x_root - son_tik_x) < 6 && abs(e->y_root - son_tik_y) < 6) {
      son_tik_c = NULL;
      buyut_ayarla(c, !c->buyuk);
      return;
    }
    son_tik_c = c; son_tik = e->time; son_tik_x = e->x_root; son_tik_y = e->y_root;
    surukle_basla(c, S_TASI, 0, e->x_root, e->y_root);
  } else if (e->button == Button3) {
    odakla(c); one_al(c);
    istemci_menu(c, e->x_root, e->y_root);
  } else if (e->button == Button2) {
    alta_al(c);
  }
}

static void ev_birak(XButtonEvent *e) {
  if (menu_kok) { menu_buton(e, 1); return; }
  if (sr.tip != S_YOK) surukle_bitir(e->x_root, e->y_root);
}

static void ev_hareket(XMotionEvent *e) {
  XEvent ev;
  while (XCheckTypedEvent(dpy, MotionNotify, &ev)) e = &ev.xmotion;
  if (menu_kok) { menu_hareket(e->x_root, e->y_root); return; }
  if (sr.tip != S_YOK) { surukle_hareket(e->x_root, e->y_root); return; }
  Istemci *c = bul(e->window);
  if (c && e->window == c->cerceve) {
    int d = dugme_bul(c, e->x, e->y);
    if (d == 4) d = 0;
    if (d != c->ust_dug) { c->ust_dug = d; cerceve_ciz(c); }
  }
}

static void ev_tus(XKeyEvent *e) {
  if (menu_kok) { menu_tus(e); return; }
  if (osd_acik) { osd_tus(e); return; }
  KeySym ks = XkbKeycodeToKeysym(dpy, e->keycode, 0, 0);
  if (ks == XK_Super_L || ks == XK_Super_R) { super_yalniz = 1; return; }
  super_yalniz = 0;
  unsigned st = temiz(e->state);
  for (unsigned i = 0; i < sizeof kisayol / sizeof kisayol[0]; i++)
    if (kisayol[i].ks == ks && kisayol[i].mod == st) { eylem_yap(kisayol[i].eylem, kisayol[i].komut, NULL); return; }
}

static void ev_tus_birak(XKeyEvent *e) {
  KeySym ks = XkbKeycodeToKeysym(dpy, e->keycode, 0, 0);
  if (osd_acik) { if (ks == XK_Alt_L || ks == XK_Alt_R || ks == XK_Meta_L) osd_bitir(1); return; }
  if ((ks == XK_Super_L || ks == XK_Super_R) && super_yalniz && !menu_kok) {
    super_yalniz = 0;
    int x, y; imlec_konum(&x, &y); menu_klavye = 1; kok_menu(x, y);
  }
}

static void ev_istemci_mesaj(XClientMessageEvent *m) {
  Istemci *c = bul_win(m->window);
  if (m->message_type == A[NET_WM_STATE] && c) {
    long ey = m->data.l[0]; int max_yapildi = 0;
    for (int i = 1; i <= 2; i++) {
      Atom p = m->data.l[i];
#define YENI(v) (ey == 1 ? 1 : ey == 0 ? 0 : !(v))
      if (p == A[NET_WM_STATE_FULLSCREEN]) tam_ayarla(c, YENI(c->tam));
      else if ((p == A[NET_WM_STATE_MAXIMIZED_VERT] || p == A[NET_WM_STATE_MAXIMIZED_HORZ]) && !max_yapildi) {
        buyut_ayarla(c, YENI(c->buyuk)); max_yapildi = 1;
      } else if (p == A[NET_WM_STATE_HIDDEN]) kucult(c, YENI(c->kucuk));
      else if (p == A[NET_WM_STATE_ABOVE]) { c->ustte = YENI(c->ustte); durum_yaz(c); yigin_uygula(); }
      else if (p && p != A[NET_WM_STATE_FOCUSED]) {
        int var = -1;
        for (int k = 0; k < c->nek; k++) if (c->ek_durum[k] == p) var = k;
        int yeni = YENI(var >= 0);
        if (yeni && var < 0 && c->nek < 8) c->ek_durum[c->nek++] = p;
        else if (!yeni && var >= 0) c->ek_durum[var] = c->ek_durum[--c->nek];
        durum_yaz(c);
      }
#undef YENI
    }
  } else if (m->message_type == A[NET_ACTIVE_WINDOW] && c) {
    etkinlestir(c);
  } else if (m->message_type == A[NET_CLOSE_WINDOW] && c) {
    kapat(c);
  } else if (m->message_type == A[WM_CHANGE_STATE] && c && m->data.l[0] == IconicState) {
    kucult(c, 1);
  } else if (m->message_type == A[NET_WM_MOVERESIZE] && c) {
    long d = m->data.l[2];
    static const int yonler[8] = { UST|SOL, UST, UST|SAG, SAG, ALT|SAG, ALT, ALT|SOL, SOL };
    if (d == 11) { if (sr.tip != S_YOK) surukle_bitir(m->data.l[0], m->data.l[1]); }
    else if (d == 8) { odakla(c); one_al(c); surukle_basla(c, S_TASI, 0, m->data.l[0], m->data.l[1]); }
    else if (d >= 0 && d <= 7) { odakla(c); one_al(c); surukle_basla(c, S_BOYUT, yonler[d], m->data.l[0], m->data.l[1]); }
  } else if (m->message_type == A[NET_MOVERESIZE_WINDOW] && c && !c->ozel) {
    long f = m->data.l[0];
    if (!c->tam && !c->buyuk) {
      if (f & (1 << 8)) c->x = m->data.l[1] + c->gb;
      if (f & (1 << 9)) c->y = m->data.l[2] + c->gth + c->gb;
      if (f & (1 << 10)) c->w = m->data.l[3];
      if (f & (1 << 11)) c->h = m->data.l[4];
      boyut_sinirla(c, &c->w, &c->h);
      yerlestir(c);
    }
  } else if (m->message_type == A[NET_REQUEST_FRAME_EXTENTS]) {
    int dek = !motif_dekorsuz(m->window);
    kapsam_yaz(m->window, dek ? kenar_k : 0, dek ? TH : 0);
  } else if (m->message_type == A[YORUNGE_MENU]) {
    /* görev çubuğu: l[0]=x, l[1]=y, l[2]=0 ana menü / 1 pencere listesi */
    if (menu_kok) menu_kapat();
    else if (m->data.l[2] == 1) liste_menu(m->data.l[0], m->data.l[1]);
    else kok_menu(m->data.l[0], m->data.l[1]);
  } else if (m->message_type == A[NET_SHOWING_DESKTOP]) {
    masaustu_goster(m->data.l[0] != 0);
  }
}

static void ev_yapilandir_istek(XConfigureRequestEvent *r) {
  Istemci *c = bul_win(r->window);
  if (c && !c->ozel) {
    if (!c->tam && !c->buyuk) {
      int b = c->dekor ? kenar_k : 0, th = c->dekor ? TH : 0;
      XSizeHints h; long s; int statik = XGetWMNormalHints(dpy, c->win, &h, &s) && (h.flags & PWinGravity) && h.win_gravity == StaticGravity;
      if (r->value_mask & CWWidth) c->w = r->width;
      if (r->value_mask & CWHeight) c->h = r->height;
      if (r->value_mask & CWX) c->x = statik ? r->x : r->x + b;
      if (r->value_mask & CWY) c->y = statik ? r->y : r->y + th + b;
      boyut_sinirla(c, &c->w, &c->h);
      yerlestir(c);
    } else bildir(c);
    if ((r->value_mask & CWStackMode) && r->detail == Above && !c->kucuk) one_al(c);
  } else {
    XWindowChanges wc = { r->x, r->y, r->width, r->height, r->border_width, r->above, r->detail };
    unsigned mask = r->value_mask;
    if (c) mask &= ~(CWSibling | CWStackMode);
    XConfigureWindow(dpy, r->window, mask, &wc);
  }
}

static void ev_ozellik(XPropertyEvent *e) {
  if (e->window == root || e->state == PropertyDelete) return;
  Istemci *c = bul_win(e->window);
  if (!c) return;
  if (e->atom == XA_WM_NAME || e->atom == A[NET_WM_NAME]) { ad_guncelle(c); cerceve_ciz(c); if (osd_acik) osd_ciz(); }
  else if (e->atom == XA_WM_NORMAL_HINTS) { ipucu_guncelle(c); tutamak_yerlestir(c); }
  else if (e->atom == XA_WM_HINTS) wmhints_oku(c);
  else if (e->atom == A[NET_WM_ICON]) { ikon_yukle(c); cerceve_ciz(c); }
  else if (e->atom == A[NET_WM_STRUT] || e->atom == A[NET_WM_STRUT_PARTIAL]) { strut_oku(c); if (c->ozel) calisma_hesapla(); }
  else if (e->atom == A[WM_PROTOCOLS]) protokol_oku(c);
}

static void yenile(void) {
  tema_yukle();
  for (int i = 0; i < ns; i++) {
    Istemci *c = yigin[i];
    if (c->ozel) continue;
    XSetWindowBackground(dpy, c->cerceve, R[R_BASLIK_I].pixel);
    yerlestir(c);
  }
  if (osd_win) { XDestroyWindow(dpy, osd_win); osd_win = None; }
}

static void isle(XEvent *e) {
  switch (e->type) {
    case MapRequest: {
      Window w = e->xmaprequest.window;
      XWindowAttributes wa;
      if (!XGetWindowAttributes(dpy, w, &wa) || wa.override_redirect) break;
      Istemci *c = bul_win(w);
      if (c) { etkinlestir(c); break; }
      yonet(w, &wa);
      break;
    }
    case UnmapNotify: {
      Istemci *c = bul_win(e->xunmap.window);
      if (!c) break;
      if (e->xunmap.send_event) { birak(c, 0); break; }
      if (c->yoksay_unmap > 0) { c->yoksay_unmap--; break; }
      birak(c, 0);
      break;
    }
    case DestroyNotify: {
      Istemci *c = bul_win(e->xdestroywindow.window);
      if (c) birak(c, 1);
      break;
    }
    case ConfigureRequest: ev_yapilandir_istek(&e->xconfigurerequest); break;
    case ConfigureNotify:
      if (e->xconfigure.window == root && (e->xconfigure.width != sw || e->xconfigure.height != sh)) {
        sw = e->xconfigure.width; sh = e->xconfigure.height;
        long g[2] = { sw, sh }; ozellik_ayarla(root, NET_DESKTOP_GEOMETRY, XA_CARDINAL, 32, g, 2);
        XRRUpdateConfiguration((XEvent *)e);
        calisma_hesapla();
        ekrana_sigdir();
      }
      break;
    case ButtonPress: ev_buton(&e->xbutton); break;
    case ButtonRelease: ev_birak(&e->xbutton); break;
    case MotionNotify: ev_hareket(&e->xmotion); break;
    case KeyPress: ev_tus(&e->xkey); break;
    case KeyRelease: ev_tus_birak(&e->xkey); break;
    case ClientMessage: ev_istemci_mesaj(&e->xclient); break;
    case PropertyNotify: ev_ozellik(&e->xproperty); break;
    case LeaveNotify: {
      Istemci *c = bul(e->xcrossing.window);
      if (c && c->ust_dug && sr.tip == S_YOK) { c->ust_dug = 0; cerceve_ciz(c); }
      break;
    }
    case Expose:
      if (e->xexpose.count) break;
      if (osd_win && e->xexpose.window == osd_win) { if (osd_acik) osd_ciz(); break; }
      for (Menu *m = menu_kok; m; m = m->acik) if (m->win == e->xexpose.window) { menu_ciz(m); return; }
      { Istemci *c = bul(e->xexpose.window); if (c) cerceve_ciz(c); }
      break;
    case MappingNotify:
      XRefreshKeyboardMapping(&e->xmapping);
      if (e->xmapping.request == MappingKeyboard || e->xmapping.request == MappingModifier) tus_yakala();
      break;
  }
}

/* ---------- sinyaller ---------- */
static void sinyal(int s) {
  int kayit = errno;
  if (s == SIGHUP) yenile_istek = 1;
  else if (s == SIGCHLD) { while (waitpid(-1, NULL, WNOHANG) > 0); }
  else bitir_istek = 1;
  if (write(sinyal_boru[1], "x", 1) < 0) {}
  errno = kayit;
}

static void kur_ewmh(void) {
  XSetWindowAttributes a; a.override_redirect = True;
  kontrol = XCreateWindow(dpy, root, -100, -100, 1, 1, 0, CopyFromParent, InputOnly, CopyFromParent, CWOverrideRedirect, &a);
  XMapWindow(dpy, kontrol);
  ozellik_ayarla(root, NET_SUPPORTING_WM_CHECK, XA_WINDOW, 32, &kontrol, 1);
  ozellik_ayarla(kontrol, NET_SUPPORTING_WM_CHECK, XA_WINDOW, 32, &kontrol, 1);
  const char *ad = "Yörünge";
  ozellik_ayarla(kontrol, NET_WM_NAME, A[UTF8_STRING], 8, ad, strlen(ad));
  int destek[] = { NET_SUPPORTED, NET_SUPPORTING_WM_CHECK, NET_WM_NAME, NET_CLIENT_LIST, NET_CLIENT_LIST_STACKING,
    NET_ACTIVE_WINDOW, NET_NUMBER_OF_DESKTOPS, NET_CURRENT_DESKTOP, NET_DESKTOP_GEOMETRY, NET_DESKTOP_VIEWPORT,
    NET_WORKAREA, NET_DESKTOP_NAMES, NET_WM_DESKTOP, NET_CLOSE_WINDOW, NET_MOVERESIZE_WINDOW, NET_WM_MOVERESIZE,
    NET_REQUEST_FRAME_EXTENTS, NET_FRAME_EXTENTS, NET_SHOWING_DESKTOP, NET_WM_STATE, NET_WM_STATE_FULLSCREEN,
    NET_WM_STATE_MAXIMIZED_VERT, NET_WM_STATE_MAXIMIZED_HORZ, NET_WM_STATE_HIDDEN, NET_WM_STATE_ABOVE,
    NET_WM_STATE_FOCUSED, NET_WM_WINDOW_TYPE, NET_WM_WINDOW_TYPE_NORMAL, NET_WM_WINDOW_TYPE_DIALOG,
    NET_WM_WINDOW_TYPE_DOCK, NET_WM_WINDOW_TYPE_DESKTOP, NET_WM_WINDOW_TYPE_SPLASH, NET_WM_WINDOW_TYPE_UTILITY,
    NET_WM_WINDOW_TYPE_NOTIFICATION, NET_WM_ICON, NET_WM_STRUT, NET_WM_STRUT_PARTIAL, NET_WM_ALLOWED_ACTIONS };
  Atom d[64]; int n = 0;
  for (unsigned i = 0; i < sizeof destek / sizeof destek[0]; i++) d[n++] = A[destek[i]];
  ozellik_ayarla(root, NET_SUPPORTED, XA_ATOM, 32, d, n);
  long bir = 1, sifir = 0, vp[2] = { 0, 0 }, g[2] = { sw, sh };
  ozellik_ayarla(root, NET_NUMBER_OF_DESKTOPS, XA_CARDINAL, 32, &bir, 1);
  ozellik_ayarla(root, NET_CURRENT_DESKTOP, XA_CARDINAL, 32, &sifir, 1);
  ozellik_ayarla(root, NET_DESKTOP_VIEWPORT, XA_CARDINAL, 32, vp, 2);
  ozellik_ayarla(root, NET_DESKTOP_GEOMETRY, XA_CARDINAL, 32, g, 2);
  ozellik_ayarla(root, NET_SHOWING_DESKTOP, XA_CARDINAL, 32, &sifir, 1);
  ozellik_ayarla(root, NET_DESKTOP_NAMES, A[UTF8_STRING], 8, "Aether", 7);
  Window yok = None; ozellik_ayarla(root, NET_ACTIVE_WINDOW, XA_WINDOW, 32, &yok, 1);
}

int main(int argc, char **argv) {
  if (argc > 1 && (!strcmp(argv[1], "--surum") || !strcmp(argv[1], "--version") || !strcmp(argv[1], "-v"))) {
    printf("Yörünge %s — Aether pencere yöneticisi\n", SURUM); return 0;
  }
  setlocale(LC_ALL, "");
  const char *dil = getenv("LANG");
  EN = dil && !strncmp(dil, "en", 2);
  if (!(dpy = XOpenDisplay(NULL))) { fprintf(stderr, "yorunge: ekrana bağlanılamadı\n"); return 1; }
  scr = DefaultScreen(dpy); root = RootWindow(dpy, scr);
  sw = DisplayWidth(dpy, scr); sh = DisplayHeight(dpy, scr);
  vis = DefaultVisual(dpy, scr); cmap = DefaultColormap(dpy, scr); derinlik = DefaultDepth(dpy, scr);

  XSetErrorHandler(baska_wm);
  XSelectInput(dpy, root, SubstructureRedirectMask);
  XSync(dpy, False);
  XSetErrorHandler(hata);
  XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | StructureNotifyMask |
                          ButtonPressMask | PropertyChangeMask);
  XInternAtoms(dpy, atom_adi, ATOM_SAYI, False, A);
  Bool db; XkbSetDetectableAutoRepeat(dpy, True, &db);
  fmt_ekran = XRenderFindVisualFormat(dpy, vis);
  fmt_argb = XRenderFindStandardFormat(dpy, PictStandardARGB32);

  if (!getenv("XCURSOR_THEME")) XcursorSetTheme(dpy, "Adwaita");
  static const char *imad[IM_SAYI] = { "left_ptr", "fleur", "top_side", "bottom_side", "left_side", "right_side",
    "top_left_corner", "top_right_corner", "bottom_left_corner", "bottom_right_corner" };
  static const unsigned imyedek[IM_SAYI] = { XC_left_ptr, XC_fleur, XC_top_side, XC_bottom_side, XC_left_side, XC_right_side,
    XC_top_left_corner, XC_top_right_corner, XC_bottom_left_corner, XC_bottom_right_corner };
  for (int i = 0; i < IM_SAYI; i++) {
    imlec[i] = XcursorLibraryLoadCursor(dpy, imad[i]);
    if (!imlec[i]) imlec[i] = XCreateFontCursor(dpy, imyedek[i]);
  }
  XDefineCursor(dpy, root, imlec[IM_OK]);
  gc = XCreateGC(dpy, root, 0, NULL);
  tema_yukle();
  kur_ewmh();
  calisma_hesapla();

  if (pipe(sinyal_boru) == 0) {
    fcntl(sinyal_boru[0], F_SETFL, O_NONBLOCK); fcntl(sinyal_boru[1], F_SETFL, O_NONBLOCK);
    fcntl(sinyal_boru[0], F_SETFD, FD_CLOEXEC); fcntl(sinyal_boru[1], F_SETFD, FD_CLOEXEC);
  }
  struct sigaction sa = {0};
  sa.sa_handler = sinyal; sa.sa_flags = SA_RESTART; sigemptyset(&sa.sa_mask);
  sigaction(SIGHUP, &sa, NULL); sigaction(SIGTERM, &sa, NULL); sigaction(SIGINT, &sa, NULL); sigaction(SIGCHLD, &sa, NULL);
  fcntl(ConnectionNumber(dpy), F_SETFD, FD_CLOEXEC);

  tus_yakala();

  /* zaten açık pencereleri yönet */
  Window r, p, *ch = NULL; unsigned n = 0;
  if (XQueryTree(dpy, root, &r, &p, &ch, &n)) {
    for (unsigned i = 0; i < n; i++) {
      XWindowAttributes wa;
      if (!XGetWindowAttributes(dpy, ch[i], &wa) || wa.override_redirect || ch[i] == kontrol) continue;
      if (wa.map_state == IsViewable || wm_state_oku(ch[i]) == IconicState) yonet(ch[i], &wa);
    }
    if (ch) XFree(ch);
  }
  if (!odak) odakla(sonraki_odak(NULL));

  int xfd = ConnectionNumber(dpy);
  while (!bitir_istek) {
    if (yenile_istek) { yenile_istek = 0; yenile(); }
    while (XPending(dpy) && !bitir_istek) { XEvent e; XNextEvent(dpy, &e); isle(&e); }
    XFlush(dpy);
    fd_set fs; FD_ZERO(&fs); FD_SET(xfd, &fs); FD_SET(sinyal_boru[0], &fs);
    if (select(MAX(xfd, sinyal_boru[0]) + 1, &fs, NULL, NULL, NULL) > 0 && FD_ISSET(sinyal_boru[0], &fs)) {
      char b[32]; while (read(sinyal_boru[0], b, sizeof b) > 0);
    }
  }
  /* çıkış: pencereler kaydetme kümesi sayesinde köke geri döner */
  menu_kapat();
  for (int i = 0; i < ns; i++) if (yigin[i]->kucuk) { XMapWindow(dpy, yigin[i]->win); }
  XDeleteProperty(dpy, root, A[NET_SUPPORTING_WM_CHECK]);
  XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
  XCloseDisplay(dpy);
  return 0;
}
