/* Eclipse — Aether'in web tarayıcısı (WebKitGTK motoru üzerinde)
   Aether 1.0 "Nebula" · Yapımcı: Hot Zot
   Sekmeler, Aether ana sayfası, yer imleri, geçmiş, indirmeler, gizli pencere. */
#include "aether.h"
#include <webkit2/webkit2.h>
#include <time.h>

#define ANA_SAYFA "aether://ev"
#define ARAMA "https://duckduckgo.com/?q=%s"

typedef struct {
    GtkWidget *pencere, *defter, *adres, *geri, *ileri, *yenile, *yildiz, *indirme_listesi, *indirme_dugmesi;
    WebKitWebContext *baglam;
    gboolean gizli;
} Pencere;

static WebKitWebContext *normal_baglam;
static GList *pencereler;
static char *yerimi_yolu, *gecmis_yolu, *indirme_dizini;

static Pencere *pencere_ac(gboolean gizli, const char *url);
static WebKitWebView *sekme_ac(Pencere *p, const char *url, gboolean one_al);

/* ------------------------------------------------ kayıtlar */
static char *html_kacis(const char *s) { return g_markup_escape_text(s ? s : "", -1); }
static gboolean yerimi_var(const char *url) {
    char *c = NULL; gboolean r = FALSE;
    if (url && g_file_get_contents(yerimi_yolu, &c, NULL, NULL)) {
        char **s = g_strsplit(c, "\n", -1);
        for (int i = 0; s[i] && !r; i++) { char *t = strchr(s[i], '\t'); if (t && !strcmp(t + 1, url)) r = TRUE; }
        g_strfreev(s); g_free(c);
    }
    return r;
}
static void yerimi_degistir(const char *baslik, const char *url) {
    char *c = NULL; GString *y = g_string_new(NULL); gboolean silindi = FALSE;
    if (g_file_get_contents(yerimi_yolu, &c, NULL, NULL)) {
        char **s = g_strsplit(c, "\n", -1);
        for (int i = 0; s[i]; i++) {
            if (!*s[i]) continue;
            char *t = strchr(s[i], '\t');
            if (t && !strcmp(t + 1, url)) { silindi = TRUE; continue; }
            g_string_append_printf(y, "%s\n", s[i]);
        }
        g_strfreev(s); g_free(c);
    }
    if (!silindi) {
        char *b = g_strdup(baslik && *baslik ? baslik : url); g_strdelimit(b, "\t\n", ' ');
        g_string_append_printf(y, "%s\t%s\n", b, url); g_free(b);
    }
    g_file_set_contents(yerimi_yolu, y->str, -1, NULL);
    g_string_free(y, TRUE);
}
static void gecmise_ekle(const char *baslik, const char *url) {
    if (!url || g_str_has_prefix(url, "aether:") || g_str_has_prefix(url, "about:")) return;
    FILE *f = fopen(gecmis_yolu, "a"); if (!f) return;
    char *b = g_strdup(baslik && *baslik ? baslik : url); g_strdelimit(b, "\t\n", ' ');
    fprintf(f, "%ld\t%s\t%s\n", (long)time(NULL), url, b); fclose(f); g_free(b);
}

/* ------------------------------------------------ aether:// sayfaları */
static const char *CSS =
    "<style>"
    ":root{color-scheme:light dark}"
    "body{margin:0;font-family:'JetBrains Mono',monospace;background:#070a1f url(aether://kaynak/arkaplan.png) center/cover fixed;color:#e8e6f5;min-height:100vh}"
    ".kap{max-width:880px;margin:0 auto;padding:6vh 24px 40px}"
    "h1{font-weight:300;letter-spacing:.6em;text-align:center;font-size:44px;margin:0 0 6px;text-shadow:0 0 24px #a08cff88}"
    ".alt{text-align:center;color:#b6a8ff;letter-spacing:.3em;font-size:13px;margin-bottom:36px}"
    "form{display:flex;border:1px solid #8f7cf0;background:#ffffffee}"
    "input{flex:1;border:0;padding:14px 16px;font:inherit;font-size:16px;background:transparent;color:#1e1a3a;outline:none}"
    "button{border:0;background:#5b4bd6;color:#fff;font:inherit;padding:0 22px;cursor:pointer}"
    "button:hover{background:#6d5ce8}"
    ".kutular{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:12px;margin-top:34px}"
    ".kutu{display:block;padding:16px 14px;background:#ffffff14;border:1px solid #ffffff2a;color:#fff;text-decoration:none;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;backdrop-filter:blur(4px)}"
    ".kutu:hover{background:#5b4bd6aa;border-color:#b6a8ff}"
    ".kutu small{display:block;color:#b8b2d8;font-size:11px;margin-top:6px;overflow:hidden;text-overflow:ellipsis}"
    "h2{font-weight:400;letter-spacing:.2em;font-size:15px;color:#c9bcff;margin:34px 0 10px}"
    ".liste a{color:#fff}.liste div{padding:6px 0;border-bottom:1px solid #ffffff18;font-size:13px}"
    ".liste span{color:#a9a3c9;margin-right:12px}"
    ".alt-not{margin-top:40px;text-align:center;font-size:11px;color:#8f88b8}"
    "</style>";

static GString *sayfa_basla(const char *baslik) {
    GString *h = g_string_new("<!doctype html><html><head><meta charset=utf-8><title>");
    g_string_append_printf(h, "%s</title>%s</head><body><div class=kap>", baslik, CSS);
    return h;
}
static void kutu_ekle(GString *h, const char *ad, const char *url) {
    char *a = html_kacis(ad), *u = html_kacis(url);
    g_string_append_printf(h, "<a class=kutu href=\"%s\">%s<small>%s</small></a>", u, a, u);
    g_free(a); g_free(u);
}
static char *ev_sayfasi(gboolean gizli) {
    GString *h = sayfa_basla(T("Yeni sekme", "New tab"));
    g_string_append(h, "<div id=saat style=\"text-align:center;font-size:56px;font-weight:300;letter-spacing:.08em;margin-bottom:4px\"></div>"
        "<div id=tarih style=\"text-align:center;color:#b6a8ff;font-size:13px;letter-spacing:.2em;margin-bottom:30px\"></div>");
    if (ae_en()) g_string_append(h, "<script>var AY=['JANUARY','FEBRUARY','MARCH','APRIL','MAY','JUNE','JULY','AUGUST','SEPTEMBER','OCTOBER','NOVEMBER','DECEMBER'],"
        "GUN=['SUNDAY','MONDAY','TUESDAY','WEDNESDAY','THURSDAY','FRIDAY','SATURDAY'];</script>");
    else g_string_append(h, "<script>var AY=['OCAK','ŞUBAT','MART','NİSAN','MAYIS','HAZİRAN','TEMMUZ','AĞUSTOS','EYLÜL','EKİM','KASIM','ARALIK'],"
        "GUN=['PAZAR','PAZARTESİ','SALI','ÇARŞAMBA','PERŞEMBE','CUMA','CUMARTESİ'];</script>");
    g_string_append(h, "<script>(function(){function iki(n){return (n<10?'0':'')+n}function t(){var d=new Date();"
        "document.getElementById('saat').textContent=iki(d.getHours())+':'+iki(d.getMinutes());"
        "document.getElementById('tarih').textContent=d.getDate()+' '+AY[d.getMonth()]+' '+d.getFullYear()+' · '+GUN[d.getDay()];}"
        "t();setInterval(t,1000);})();</script>");
    g_string_append(h, "<h1>E C L I P S E</h1>");
    g_string_append_printf(h, "<div class=alt>%s</div>", gizli ? T("GİZLİ PENCERE · GEÇMİŞ KAYDEDİLMEZ", "PRIVATE WINDOW · NO HISTORY")
                                                              : "AETHER 1.0 · NEBULA");
    g_string_append_printf(h, "<form onsubmit=\"var q=document.getElementById('q').value.trim();if(q)location.href='https://duckduckgo.com/?q='+encodeURIComponent(q);return false\">"
        "<input id=q autofocus placeholder=\"%s\"><button>%s</button></form>",
        T("Web'de ara veya adres yaz…", "Search the web or type an address…"), T("Ara", "Search"));
    g_string_append(h, "<div class=kutular>");
    kutu_ekle(h, "Google", "https://www.google.com");
    kutu_ekle(h, "YouTube", "https://www.youtube.com");
    kutu_ekle(h, T("Vikipedi", "Wikipedia"), ae_en() ? "https://en.wikipedia.org" : "https://tr.wikipedia.org");
    kutu_ekle(h, "GitHub", "https://github.com");
    kutu_ekle(h, "DuckDuckGo", "https://duckduckgo.com");
    kutu_ekle(h, "Alpine Linux", "https://alpinelinux.org");
    g_string_append(h, "</div>");
    char *c = NULL;
    if (g_file_get_contents(yerimi_yolu, &c, NULL, NULL) && *c) {
        g_string_append_printf(h, "<h2>%s</h2><div class=kutular>", T("YER İMLERİ", "BOOKMARKS"));
        char **s = g_strsplit(c, "\n", -1);
        for (int i = 0; s[i]; i++) { char *t = strchr(s[i], '\t'); if (!t) continue; *t = 0; kutu_ekle(h, s[i], t + 1); }
        g_strfreev(s); g_string_append(h, "</div>");
    }
    g_free(c);
    g_string_append_printf(h, "<div class=alt-not>%s · <a style=color:#b6a8ff href=aether://gecmis>%s</a> · Hot Zot</div>",
        T("Ctrl+T yeni sekme · Ctrl+D yer imi · Ctrl+Shift+N gizli pencere", "Ctrl+T new tab · Ctrl+D bookmark · Ctrl+Shift+N private window"),
        T("Geçmiş", "History"));
    g_string_append(h, "</div></body></html>");
    return g_string_free(h, FALSE);
}
static char *gecmis_sayfasi(void) {
    GString *h = sayfa_basla(T("Geçmiş", "History"));
    g_string_append_printf(h, "<h1 style=font-size:30px>%s</h1><div class=alt><a style=color:#b6a8ff href=aether://gecmis/temizle>%s</a></div><div class=liste>",
        T("G E Ç M İ Ş", "H I S T O R Y"), T("Geçmişi temizle", "Clear history"));
    char *c = NULL; int n = 0;
    if (g_file_get_contents(gecmis_yolu, &c, NULL, NULL)) {
        char **s = g_strsplit(c, "\n", -1); int say = g_strv_length(s);
        for (int i = say - 1; i >= 0 && n < 300; i--) {
            char **p = g_strsplit(s[i], "\t", 3);
            if (g_strv_length(p) == 3) {
                time_t z = atol(p[0]); char tarih[32]; strftime(tarih, sizeof tarih, "%d.%m.%Y %H:%M", localtime(&z));
                char *u = html_kacis(p[1]), *b = html_kacis(p[2]);
                g_string_append_printf(h, "<div><span>%s</span><a href=\"%s\">%s</a></div>", tarih, u, b); g_free(u); g_free(b); n++;
            }
            g_strfreev(p);
        }
        g_strfreev(s); g_free(c);
    }
    if (!n) g_string_append_printf(h, "<div>%s</div>", T("Geçmiş boş.", "History is empty."));
    g_string_append(h, "</div></div></body></html>");
    return g_string_free(h, FALSE);
}
static void aether_semasi(WebKitURISchemeRequest *r, gpointer gizli_mi) {
    /* aether://ev, aether://gecmis, aether://kaynak/x.png → "ev", "gecmis", "kaynak/x.png" */
    const char *uri = webkit_uri_scheme_request_get_uri(r);
    const char *yol = g_str_has_prefix(uri, "aether://") ? uri + 9 : uri;
    char *html = NULL;
    if (g_str_has_prefix(yol, "kaynak/")) {           /* yerel görseller */
        char *d = g_build_filename("/usr/share/aether/eclipse", yol + 7, NULL), *icerik; gsize n;
        if (!strstr(yol, "..") && g_file_get_contents(d, &icerik, &n, NULL)) {
            GInputStream *s = g_memory_input_stream_new_from_data(icerik, n, g_free);
            webkit_uri_scheme_request_finish(r, s, n, "image/png"); g_object_unref(s);
        } else { GError *e = g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "yok"); webkit_uri_scheme_request_finish_error(r, e); g_error_free(e); }
        g_free(d); return;
    }
    if (!strcmp(yol, "gecmis/temizle")) { g_file_set_contents(gecmis_yolu, "", 0, NULL); html = gecmis_sayfasi(); }
    else if (!strcmp(yol, "gecmis")) html = gecmis_sayfasi();
    else html = ev_sayfasi(GPOINTER_TO_INT(gizli_mi));
    gsize n = strlen(html);
    GInputStream *s = g_memory_input_stream_new_from_data(html, n, g_free);
    webkit_uri_scheme_request_finish(r, s, n, "text/html; charset=utf-8");
    g_object_unref(s);
}

/* ------------------------------------------------ yardımcılar */
static WebKitWebView *etkin(Pencere *p) {
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->defter));
    return i < 0 ? NULL : WEBKIT_WEB_VIEW(gtk_notebook_get_nth_page(GTK_NOTEBOOK(p->defter), i));
}
static char *adres_coz(const char *g) {
    while (*g == ' ') g++;
    if (!*g) return g_strdup(ANA_SAYFA);
    if (strstr(g, "://") || g_str_has_prefix(g, "about:") || g_str_has_prefix(g, "aether:")) return g_strdup(g);
    if (g[0] == '/') return g_strdup_printf("file://%s", g);
    if (!strchr(g, ' ') && (strchr(g, '.') || g_str_has_prefix(g, "localhost"))) return g_strdup_printf("https://%s", g);
    char *q = g_uri_escape_string(g, NULL, TRUE), *u = g_strdup_printf(ARAMA, q); g_free(q); return u;
}
static void arayuzu_guncelle(Pencere *p) {
    WebKitWebView *w = etkin(p); if (!w) return;
    const char *u = webkit_web_view_get_uri(w);
    if (!gtk_widget_has_focus(p->adres))
        gtk_entry_set_text(GTK_ENTRY(p->adres), u && !g_str_has_prefix(u, "aether://ev") ? u : "");
    gtk_widget_set_sensitive(p->geri, webkit_web_view_can_go_back(w));
    gtk_widget_set_sensitive(p->ileri, webkit_web_view_can_go_forward(w));
    gboolean yukleniyor = webkit_web_view_is_loading(w);
    gtk_button_set_image(GTK_BUTTON(p->yenile), gtk_image_new_from_icon_name(yukleniyor ? "process-stop-symbolic" : "view-refresh-symbolic", GTK_ICON_SIZE_BUTTON));
    gtk_entry_set_progress_fraction(GTK_ENTRY(p->adres), yukleniyor ? webkit_web_view_get_estimated_load_progress(w) : 0);
    gtk_button_set_image(GTK_BUTTON(p->yildiz), gtk_image_new_from_icon_name(yerimi_var(u) ? "starred-symbolic" : "non-starred-symbolic", GTK_ICON_SIZE_BUTTON));
    const char *b = webkit_web_view_get_title(w);
    char *t = g_strdup_printf("%s%s — Eclipse", p->gizli ? T("[Gizli] ", "[Private] ") : "", b && *b ? b : "Eclipse");
    gtk_window_set_title(GTK_WINDOW(p->pencere), t); g_free(t);
}

/* ------------------------------------------------ sekmeler */
static void sekme_kapat(Pencere *p, GtkWidget *w) {
    int i = gtk_notebook_page_num(GTK_NOTEBOOK(p->defter), w);
    if (i >= 0) gtk_notebook_remove_page(GTK_NOTEBOOK(p->defter), i);
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter)) == 0) gtk_widget_destroy(p->pencere);
}
static void kapat_tik(GtkButton *b, gpointer w) { sekme_kapat(g_object_get_data(G_OBJECT(w), "pencere"), w); }
static void baslik_degisti(WebKitWebView *w, GParamSpec *ps, gpointer etiket) {
    const char *b = webkit_web_view_get_title(w);
    gtk_label_set_text(GTK_LABEL(etiket), b && *b ? b : T("Yeni sekme", "New tab"));
    Pencere *p = g_object_get_data(G_OBJECT(w), "pencere");
    if (etkin(p) == w) arayuzu_guncelle(p);
}
static void degisti(WebKitWebView *w, GParamSpec *ps, gpointer d) {
    Pencere *p = g_object_get_data(G_OBJECT(w), "pencere"); if (etkin(p) == w) arayuzu_guncelle(p);
}
static void yukleme(WebKitWebView *w, WebKitLoadEvent e, gpointer d) {
    Pencere *p = g_object_get_data(G_OBJECT(w), "pencere");
    if (e == WEBKIT_LOAD_FINISHED && !p->gizli) gecmise_ekle(webkit_web_view_get_title(w), webkit_web_view_get_uri(w));
    if (etkin(p) == w) arayuzu_guncelle(p);
}
static gboolean yukleme_hatasi(WebKitWebView *w, WebKitLoadEvent e, char *uri, GError *h, gpointer d) {
    if (g_error_matches(h, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED) ||
        g_error_matches(h, WEBKIT_POLICY_ERROR, WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE)) return FALSE;
    char *u = html_kacis(uri), *m = html_kacis(h->message);
    GString *s = sayfa_basla(T("Sayfa açılamadı", "Page failed to load"));
    g_string_append_printf(s, "<h1 style=font-size:28px>%s</h1><div class=alt>%s</div><p style=text-align:center>%s</p>"
        "<p style=text-align:center><a style=color:#b6a8ff href=\"%s\">%s</a></p></div></body></html>",
        T("SAYFA AÇILAMADI", "PAGE FAILED TO LOAD"), u, m, u, T("Tekrar dene", "Try again"));
    webkit_web_view_load_alternate_html(w, s->str, uri, NULL);
    g_string_free(s, TRUE); g_free(u); g_free(m);
    return TRUE;
}
static GtkWidget *yeni_sekme_istegi(WebKitWebView *w, WebKitNavigationAction *a, gpointer d) {
    Pencere *p = g_object_get_data(G_OBJECT(w), "pencere");
    WebKitWebView *y = sekme_ac(p, NULL, TRUE);
    return GTK_WIDGET(y);
}
static void sayfa_kapandi(WebKitWebView *w, gpointer d) { sekme_kapat(g_object_get_data(G_OBJECT(w), "pencere"), GTK_WIDGET(w)); }
static void tam_ekran(WebKitWebView *w, gpointer gir) {
    Pencere *p = g_object_get_data(G_OBJECT(w), "pencere");
    if (GPOINTER_TO_INT(gir)) gtk_window_fullscreen(GTK_WINDOW(p->pencere)); else gtk_window_unfullscreen(GTK_WINDOW(p->pencere));
}

static WebKitWebView *sekme_ac(Pencere *p, const char *url, gboolean one_al) {
    WebKitWebView *w = WEBKIT_WEB_VIEW(webkit_web_view_new_with_context(p->baglam));
    WebKitSettings *s = webkit_web_view_get_settings(w);
    webkit_settings_set_enable_javascript(s, TRUE);
    webkit_settings_set_enable_developer_extras(s, TRUE);
    webkit_settings_set_enable_smooth_scrolling(s, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(s, FALSE);
    webkit_settings_set_hardware_acceleration_policy(s, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    webkit_settings_set_default_charset(s, "utf-8");
    g_object_set_data(G_OBJECT(w), "pencere", p);

    GtkWidget *kafa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget *etiket = gtk_label_new(T("Yeni sekme", "New tab"));
    gtk_label_set_ellipsize(GTK_LABEL(etiket), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(etiket), 18); gtk_label_set_max_width_chars(GTK_LABEL(etiket), 18);
    GtkWidget *kapat = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(kapat), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(kapat, T("Sekmeyi kapat (Ctrl+W)", "Close tab (Ctrl+W)"));
    gtk_box_pack_start(GTK_BOX(kafa), etiket, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(kafa), kapat, FALSE, FALSE, 0);
    gtk_widget_show_all(kafa);
    g_signal_connect(kapat, "clicked", G_CALLBACK(kapat_tik), w);

    g_signal_connect(w, "notify::title", G_CALLBACK(baslik_degisti), etiket);
    g_signal_connect(w, "notify::uri", G_CALLBACK(degisti), NULL);
    g_signal_connect(w, "notify::estimated-load-progress", G_CALLBACK(degisti), NULL);
    g_signal_connect(w, "load-changed", G_CALLBACK(yukleme), NULL);
    g_signal_connect(w, "load-failed", G_CALLBACK(yukleme_hatasi), NULL);
    g_signal_connect(w, "create", G_CALLBACK(yeni_sekme_istegi), NULL);
    g_signal_connect(w, "close", G_CALLBACK(sayfa_kapandi), NULL);
    g_signal_connect(w, "enter-fullscreen", G_CALLBACK(tam_ekran), GINT_TO_POINTER(1));
    g_signal_connect(w, "leave-fullscreen", G_CALLBACK(tam_ekran), GINT_TO_POINTER(0));

    int i = gtk_notebook_append_page(GTK_NOTEBOOK(p->defter), GTK_WIDGET(w), kafa);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(p->defter), GTK_WIDGET(w), TRUE);
    gtk_widget_show(GTK_WIDGET(w));
    if (one_al) gtk_notebook_set_current_page(GTK_NOTEBOOK(p->defter), i);
    if (url) webkit_web_view_load_uri(w, url);
    if (one_al && (!url || !strcmp(url, ANA_SAYFA))) gtk_widget_grab_focus(p->adres);
    return w;
}

/* ------------------------------------------------ indirmeler */
static void indirme_ilerledi(WebKitDownload *d, GParamSpec *ps, gpointer cubuk) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cubuk), webkit_download_get_estimated_progress(d));
}
static void indirme_bitti(WebKitDownload *d, gpointer cubuk) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(cubuk), 1);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(cubuk), T("Tamamlandı", "Done"));
    ae_calistir("aether-ses bildirim");
}
static void indirme_hata(WebKitDownload *d, GError *e, gpointer cubuk) {
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(cubuk), T("Başarısız", "Failed"));
}
static gboolean hedef_belirle(WebKitDownload *d, char *onerilen, gpointer p_) {
    Pencere *p = p_;
    g_mkdir_with_parents(indirme_dizini, 0755);
    char *ad = g_path_get_basename(onerilen && *onerilen ? onerilen : "indirilen"), *yol = g_build_filename(indirme_dizini, ad, NULL);
    for (int i = 1; g_file_test(yol, G_FILE_TEST_EXISTS) && i < 1000; i++) { g_free(yol); char *a2 = g_strdup_printf("%d-%s", i, ad); yol = g_build_filename(indirme_dizini, a2, NULL); g_free(a2); }
    char *uri = g_filename_to_uri(yol, NULL, NULL);
    webkit_download_set_destination(d, uri);
    GtkWidget *satir = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    char *temel = g_path_get_basename(yol);
    GtkWidget *l = gtk_label_new(temel); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_ellipsize(GTK_LABEL(l), PANGO_ELLIPSIZE_MIDDLE);
    GtkWidget *c = gtk_progress_bar_new(); gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(c), TRUE);
    gtk_box_pack_start(GTK_BOX(satir), l, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(satir), c, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(p->indirme_listesi), satir, FALSE, FALSE, 6);
    gtk_widget_show_all(satir);
    g_signal_connect(d, "notify::estimated-progress", G_CALLBACK(indirme_ilerledi), c);
    g_signal_connect(d, "finished", G_CALLBACK(indirme_bitti), c);
    g_signal_connect(d, "failed", G_CALLBACK(indirme_hata), c);
    gtk_widget_show(p->indirme_dugmesi);
    g_free(ad); g_free(yol); g_free(uri); g_free(temel);
    return TRUE;
}
static void indirme_basladi(WebKitWebContext *b, WebKitDownload *d, gpointer x) {
    /* indirme hangi pencereden başladıysa onun listesine eklensin */
    WebKitWebView *w = webkit_download_get_web_view(d);
    Pencere *p = w ? g_object_get_data(G_OBJECT(w), "pencere") : (pencereler ? pencereler->data : NULL);
    if (p) g_signal_connect(d, "decide-destination", G_CALLBACK(hedef_belirle), p);
}

/* ------------------------------------------------ eylemler */
static void adres_girildi(GtkEntry *e, Pencere *p) {
    WebKitWebView *w = etkin(p); if (!w) return;
    char *u = adres_coz(gtk_entry_get_text(e)); webkit_web_view_load_uri(w, u); g_free(u);
    gtk_widget_grab_focus(GTK_WIDGET(w));
}
static void geri_tik(GtkButton *b, Pencere *p) { WebKitWebView *w = etkin(p); if (w) webkit_web_view_go_back(w); }
static void ileri_tik(GtkButton *b, Pencere *p) { WebKitWebView *w = etkin(p); if (w) webkit_web_view_go_forward(w); }
static void yenile_tik(GtkButton *b, Pencere *p) {
    WebKitWebView *w = etkin(p); if (!w) return;
    if (webkit_web_view_is_loading(w)) webkit_web_view_stop_loading(w); else webkit_web_view_reload(w);
}
static void ev_tik(GtkButton *b, Pencere *p) { WebKitWebView *w = etkin(p); if (w) webkit_web_view_load_uri(w, ANA_SAYFA); }
static void yeni_sekme_tik(GtkButton *b, Pencere *p) { sekme_ac(p, ANA_SAYFA, TRUE); }
static void yildiz_tik(GtkButton *b, Pencere *p) {
    WebKitWebView *w = etkin(p); if (!w) return;
    const char *u = webkit_web_view_get_uri(w);
    if (!u || g_str_has_prefix(u, "aether:")) return;
    yerimi_degistir(webkit_web_view_get_title(w), u); arayuzu_guncelle(p);
}
static gboolean bosta_guncelle(gpointer p) { arayuzu_guncelle(p); return G_SOURCE_REMOVE; }
static void sayfa_degisti(GtkNotebook *n, GtkWidget *s, guint i, Pencere *p) { g_idle_add(bosta_guncelle, p); }
static void gizli_ac(GtkWidget *x, Pencere *p) { pencere_ac(TRUE, ANA_SAYFA); }
static void gecmis_ac(GtkWidget *x, Pencere *p) { sekme_ac(p, "aether://gecmis", TRUE); }
static void yakinlastir(Pencere *p, double k) {
    WebKitWebView *w = etkin(p); if (!w) return;
    double z = k == 0 ? 1.0 : webkit_web_view_get_zoom_level(w) * k;
    if (z < 0.3) z = 0.3;
    if (z > 4) z = 4;
    webkit_web_view_set_zoom_level(w, z);
}
static void hakkinda(GtkWidget *x, Pencere *p) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(p->pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Eclipse");
    char m[512];
    snprintf(m, sizeof m, T("Aether'in web tarayıcısı.\nYapımcı: %s\n\nSayfa motoru: WebKitGTK %u.%u.%u (JavaScript destekli)",
                            "Aether's web browser.\nMade by %s\n\nPage engine: WebKitGTK %u.%u.%u (with JavaScript)"),
             AETHER_YAPIMCI, webkit_get_major_version(), webkit_get_minor_version(), webkit_get_micro_version());
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d), "%s", m);
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}
static void yerimi_menuden(GtkMenuItem *m, Pencere *p) { sekme_ac(p, g_object_get_data(G_OBJECT(m), "url"), TRUE); }
static void menu_goster(GtkButton *b, Pencere *p) {
    GtkWidget *m = gtk_menu_new();
#define OGE(ad, f) { GtkWidget *o = gtk_menu_item_new_with_label(ad); g_signal_connect(o, "activate", G_CALLBACK(f), p); gtk_menu_shell_append(GTK_MENU_SHELL(m), o); }
    OGE(T("Yeni sekme              Ctrl+T", "New tab                 Ctrl+T"), yeni_sekme_tik);
    OGE(T("Yeni gizli pencere      Ctrl+Shift+N", "New private window      Ctrl+Shift+N"), gizli_ac);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    GtkWidget *ym = gtk_menu_item_new_with_label(T("Yer imleri", "Bookmarks")), *alt = gtk_menu_new();
    char *c = NULL; int n = 0;
    if (g_file_get_contents(yerimi_yolu, &c, NULL, NULL)) {
        char **s = g_strsplit(c, "\n", -1);
        for (int i = 0; s[i]; i++) {
            char *t = strchr(s[i], '\t'); if (!t) continue; *t = 0;
            GtkWidget *o = gtk_menu_item_new_with_label(s[i]);
            g_object_set_data_full(G_OBJECT(o), "url", g_strdup(t + 1), g_free);
            g_signal_connect(o, "activate", G_CALLBACK(yerimi_menuden), p);
            gtk_menu_shell_append(GTK_MENU_SHELL(alt), o); n++;
        }
        g_strfreev(s); g_free(c);
    }
    if (!n) { GtkWidget *o = gtk_menu_item_new_with_label(T("(Ctrl+D ile ekle)", "(add with Ctrl+D)")); gtk_widget_set_sensitive(o, FALSE); gtk_menu_shell_append(GTK_MENU_SHELL(alt), o); }
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(ym), alt); gtk_menu_shell_append(GTK_MENU_SHELL(m), ym);
    OGE(T("Geçmiş                  Ctrl+H", "History                 Ctrl+H"), gecmis_ac);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    OGE(T("Eclipse hakkında", "About Eclipse"), hakkinda);
    gtk_widget_show_all(m);
    gtk_menu_popup_at_widget(GTK_MENU(m), GTK_WIDGET(b), GDK_GRAVITY_SOUTH_EAST, GDK_GRAVITY_NORTH_EAST, NULL);
}
static void indirmeleri_goster(GtkButton *b, GtkWidget *pop) { gtk_widget_show_all(pop); gtk_popover_popup(GTK_POPOVER(pop)); }

static gboolean tus(GtkWidget *win, GdkEventKey *e, Pencere *p) {
    guint k = gdk_keyval_to_lower(e->keyval);
    gboolean ctrl = e->state & GDK_CONTROL_MASK, shift = e->state & GDK_SHIFT_MASK, alt = e->state & GDK_MOD1_MASK;
    WebKitWebView *w = etkin(p);
    if (ctrl && shift && k == GDK_KEY_n) { pencere_ac(TRUE, ANA_SAYFA); return TRUE; }
    if (ctrl && !shift) switch (k) {
        case GDK_KEY_t: sekme_ac(p, ANA_SAYFA, TRUE); return TRUE;
        case GDK_KEY_n: pencere_ac(FALSE, ANA_SAYFA); return TRUE;
        case GDK_KEY_w: if (w) sekme_kapat(p, GTK_WIDGET(w)); return TRUE;
        case GDK_KEY_l: gtk_widget_grab_focus(p->adres); gtk_editable_select_region(GTK_EDITABLE(p->adres), 0, -1); return TRUE;
        case GDK_KEY_r: if (w) webkit_web_view_reload(w); return TRUE;
        case GDK_KEY_d: yildiz_tik(NULL, p); return TRUE;
        case GDK_KEY_h: gecmis_ac(NULL, p); return TRUE;
        case GDK_KEY_j: if (gtk_widget_get_visible(p->indirme_dugmesi)) gtk_button_clicked(GTK_BUTTON(p->indirme_dugmesi)); return TRUE;
        case GDK_KEY_plus: case GDK_KEY_equal: case GDK_KEY_KP_Add: yakinlastir(p, 1.1); return TRUE;
        case GDK_KEY_minus: case GDK_KEY_KP_Subtract: yakinlastir(p, 1 / 1.1); return TRUE;
        case GDK_KEY_0: yakinlastir(p, 0); return TRUE;
        case GDK_KEY_Tab: case GDK_KEY_Page_Down: {
            int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter)), i = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->defter));
            gtk_notebook_set_current_page(GTK_NOTEBOOK(p->defter), (i + 1) % n); return TRUE; }
    }
    if (ctrl && shift && (k == GDK_KEY_ISO_Left_Tab || k == GDK_KEY_Tab)) {
        int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter)), i = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->defter));
        gtk_notebook_set_current_page(GTK_NOTEBOOK(p->defter), (i + n - 1) % n); return TRUE;
    }
    if (alt && k == GDK_KEY_Left && w) { webkit_web_view_go_back(w); return TRUE; }
    if (alt && k == GDK_KEY_Right && w) { webkit_web_view_go_forward(w); return TRUE; }
    if (alt && k == GDK_KEY_Home && w) { webkit_web_view_load_uri(w, ANA_SAYFA); return TRUE; }
    if (k == GDK_KEY_F5 && w) { webkit_web_view_reload(w); return TRUE; }
    if (k == GDK_KEY_F11) {
        GdkWindow *gw = gtk_widget_get_window(win);
        if (gdk_window_get_state(gw) & GDK_WINDOW_STATE_FULLSCREEN) gtk_window_unfullscreen(GTK_WINDOW(win)); else gtk_window_fullscreen(GTK_WINDOW(win));
        return TRUE;
    }
    if (k == GDK_KEY_F12 && w) { webkit_web_inspector_show(webkit_web_view_get_inspector(w)); return TRUE; }
    return FALSE;
}

static void pencere_kapandi(GtkWidget *w, Pencere *p) {
    pencereler = g_list_remove(pencereler, p);
    if (p->gizli) g_object_unref(p->baglam);
    g_free(p);
    if (!pencereler) gtk_main_quit();
}

static GtkWidget *simge_dugme(const char *simge, const char *ipucu) {
    GtkWidget *b = gtk_button_new_from_icon_name(simge, GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_tooltip_text(b, ipucu); gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
    return b;
}

static WebKitWebContext *baglam_kur(gboolean gizli) {
    WebKitWebContext *b;
    if (gizli) b = webkit_web_context_new_ephemeral();
    else {
        char *veri = g_build_filename(g_get_user_data_dir(), "aether", "eclipse", NULL);
        char *onbellek = g_build_filename(g_get_user_cache_dir(), "aether", "eclipse", NULL);
        WebKitWebsiteDataManager *v = webkit_website_data_manager_new("base-data-directory", veri, "base-cache-directory", onbellek, NULL);
        b = webkit_web_context_new_with_website_data_manager(v);
        WebKitCookieManager *cm = webkit_website_data_manager_get_cookie_manager(v);
        char *cerez = g_build_filename(veri, "cerezler.sqlite", NULL);
        webkit_cookie_manager_set_persistent_storage(cm, cerez, WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE);
        g_free(veri); g_free(onbellek); g_free(cerez); g_object_unref(v);
    }
    webkit_web_context_register_uri_scheme(b, "aether", aether_semasi, GINT_TO_POINTER(gizli), NULL);
    webkit_security_manager_register_uri_scheme_as_local(webkit_web_context_get_security_manager(b), "aether");
    webkit_web_context_set_preferred_languages(b, (const char *const[]){ ae_en() ? "en-US" : "tr-TR", ae_en() ? "tr-TR" : "en-US", NULL });
    g_signal_connect(b, "download-started", G_CALLBACK(indirme_basladi), NULL);
    return b;
}

static Pencere *pencere_ac(gboolean gizli, const char *url) {
    Pencere *p = g_new0(Pencere, 1);
    p->gizli = gizli;
    p->baglam = gizli ? baglam_kur(TRUE) : normal_baglam;
    p->pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(p->pencere), 1100, 720);
    gtk_window_set_icon_name(GTK_WINDOW(p->pencere), "aether-eclipse");
    gtk_window_set_title(GTK_WINDOW(p->pencere), "Eclipse");
    g_signal_connect(p->pencere, "destroy", G_CALLBACK(pencere_kapandi), p);
    g_signal_connect(p->pencere, "key-press-event", G_CALLBACK(tus), p);

    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *cubuk = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_container_set_border_width(GTK_CONTAINER(cubuk), 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(cubuk), "eclipse-cubuk");
    if (gizli) gtk_style_context_add_class(gtk_widget_get_style_context(cubuk), "gizli");
    p->geri = simge_dugme("go-previous-symbolic", T("Geri (Alt+Sol)", "Back (Alt+Left)"));
    p->ileri = simge_dugme("go-next-symbolic", T("İleri (Alt+Sağ)", "Forward (Alt+Right)"));
    p->yenile = simge_dugme("view-refresh-symbolic", T("Yenile (F5)", "Reload (F5)"));
    GtkWidget *ev = simge_dugme("go-home-symbolic", T("Ana sayfa", "Home"));
    p->adres = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(p->adres), T("Ara veya adres yaz", "Search or enter address"));
    gtk_entry_set_input_purpose(GTK_ENTRY(p->adres), GTK_INPUT_PURPOSE_URL);
    gtk_widget_set_hexpand(p->adres, TRUE);
    if (gizli) gtk_entry_set_icon_from_icon_name(GTK_ENTRY(p->adres), GTK_ENTRY_ICON_PRIMARY, "security-high-symbolic");
    p->yildiz = simge_dugme("non-starred-symbolic", T("Yer imi ekle/kaldır (Ctrl+D)", "Bookmark (Ctrl+D)"));
    GtkWidget *yeni = simge_dugme("tab-new-symbolic", T("Yeni sekme (Ctrl+T)", "New tab (Ctrl+T)"));
    p->indirme_dugmesi = simge_dugme("folder-download-symbolic", T("İndirmeler (Ctrl+J)", "Downloads (Ctrl+J)"));
    GtkWidget *menu = simge_dugme("open-menu-symbolic", T("Menü", "Menu"));
    GtkWidget *pop = gtk_popover_new(p->indirme_dugmesi);
    GtkWidget *pk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4); gtk_container_set_border_width(GTK_CONTAINER(pk), 10);
    gtk_widget_set_size_request(pk, 320, -1);
    char baslik[256]; snprintf(baslik, sizeof baslik, "<b>%s</b>", T("İndirmeler", "Downloads"));
    GtkWidget *bl = gtk_label_new(NULL); gtk_label_set_markup(GTK_LABEL(bl), baslik); gtk_label_set_xalign(GTK_LABEL(bl), 0);
    gtk_box_pack_start(GTK_BOX(pk), bl, FALSE, FALSE, 0);
    p->indirme_listesi = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(pk), p->indirme_listesi, FALSE, FALSE, 0);
    GtkWidget *klasor = gtk_button_new_with_label(T("İndirilenler klasörünü aç", "Open downloads folder"));
    char *kk = g_strdup_printf("pcmanfm \"%s\"", indirme_dizini);
    g_signal_connect_swapped(klasor, "clicked", G_CALLBACK(ae_calistir), kk);
    gtk_box_pack_end(GTK_BOX(pk), klasor, FALSE, FALSE, 4);
    gtk_container_add(GTK_CONTAINER(pop), pk);

    gtk_box_pack_start(GTK_BOX(cubuk), p->geri, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), p->ileri, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), p->yenile, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), ev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), p->adres, TRUE, TRUE, 4);
    gtk_box_pack_start(GTK_BOX(cubuk), p->yildiz, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), p->indirme_dugmesi, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), yeni, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(cubuk), menu, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), cubuk, FALSE, FALSE, 0);

    p->defter = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(p->defter), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(p->defter), FALSE);
    gtk_box_pack_start(GTK_BOX(k), p->defter, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(p->pencere), k);

    g_signal_connect(p->adres, "activate", G_CALLBACK(adres_girildi), p);
    g_signal_connect(p->geri, "clicked", G_CALLBACK(geri_tik), p);
    g_signal_connect(p->ileri, "clicked", G_CALLBACK(ileri_tik), p);
    g_signal_connect(p->yenile, "clicked", G_CALLBACK(yenile_tik), p);
    g_signal_connect(ev, "clicked", G_CALLBACK(ev_tik), p);
    g_signal_connect(yeni, "clicked", G_CALLBACK(yeni_sekme_tik), p);
    g_signal_connect(p->yildiz, "clicked", G_CALLBACK(yildiz_tik), p);
    g_signal_connect(menu, "clicked", G_CALLBACK(menu_goster), p);
    g_signal_connect(p->indirme_dugmesi, "clicked", G_CALLBACK(indirmeleri_goster), pop);
    g_signal_connect(p->defter, "switch-page", G_CALLBACK(sayfa_degisti), p);

    pencereler = g_list_append(pencereler, p);
    gtk_widget_show_all(p->pencere);
    gtk_widget_hide(p->indirme_dugmesi);
    sekme_ac(p, url ? url : ANA_SAYFA, TRUE);
    return p;
}

int main(int argc, char **argv) {
    /* sanal makinelerde siyah sayfa sorunlarını önle */
    g_setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", FALSE);
    g_setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", FALSE);
    gtk_init(&argc, &argv); ae_css();
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        ".eclipse-cubuk { border-bottom: 1px solid alpha(currentColor,0.15); }"
        ".eclipse-cubuk.gizli { background: #2a2350; color: #fff; }"
        ".eclipse-cubuk.gizli entry { background: #3a3270; color: #fff; }"
        "notebook tab { padding: 2px 6px; } notebook tab button { padding: 0; min-height: 16px; min-width: 16px; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), 900);

    char *d = g_build_filename(g_get_user_config_dir(), "aether", "eclipse", NULL); g_mkdir_with_parents(d, 0755);
    yerimi_yolu = g_build_filename(d, "yerimleri", NULL); g_free(d);
    d = g_build_filename(g_get_user_data_dir(), "aether", "eclipse", NULL); g_mkdir_with_parents(d, 0755);
    gecmis_yolu = g_build_filename(d, "gecmis", NULL); g_free(d);
    const char *ind = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    indirme_dizini = g_strdup(ind ? ind : g_get_home_dir());

    normal_baglam = baglam_kur(FALSE);
    gboolean gizli = FALSE; const char *url = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--gizli") || !strcmp(argv[i], "--private")) gizli = TRUE;
        else url = argv[i];
    }
    char *u = url ? adres_coz(url) : NULL;
    pencere_ac(gizli, u);
    gtk_main();
    return 0;
}
