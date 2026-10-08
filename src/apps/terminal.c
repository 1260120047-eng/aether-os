/* Aether Terminal — sekmeli, Aether temalı terminal (VTE)
 *
 *   aether-terminal                    yeni pencere
 *   aether-terminal -e KOMUT [ARG...]  komutu çalıştır
 *   aether-terminal --dizin KLASÖR     klasörde aç
 *
 * Kısayollar: Ctrl+Shift+T yeni sekme · Ctrl+Shift+W sekmeyi kapat · Ctrl+Shift+N yeni pencere
 *             Ctrl+Shift+C/V kopyala/yapıştır · Ctrl+PgUp/PgDn sekmeler arası
 *             Ctrl + / Ctrl - / Ctrl 0 yazı boyutu · Ctrl+tık bağlantıyı aç
 */
#include "aether.h"
#include <vte/vte.h>
#define PCRE2_CODE_UNIT_WIDTH 0
#include <pcre2.h>

typedef struct { GtkWidget *pencere, *defter; double olcek; } Pencere;

static GList *pencereler;
static char **ilk_komut;      /* -e ile verilen komut (yalnızca ilk sekme) */
static char *ilk_dizin;
static int koyu;
static const char *URL_DESEN = "(https?|ftp)://[-[:alnum:]._~:/?#@!$&'()*+,;=%]+[-[:alnum:]_~/#@$&*+=%]";

/* aether-uygula ile aynı renkler */
static const char *PAL_ACIK[16] = { "#26223f", "#c62828", "#2e7d32", "#a66b00", "#1e5bd6", "#5b4bd6", "#00838f", "#8a879c",
                                    "#56526e", "#e53935", "#43a047", "#c68a00", "#3d7bff", "#7c6ad6", "#0097a7", "#d8d6e4" };
static const char *PAL_KOYU[16] = { "#1d1b29", "#e06c75", "#98c379", "#e5c07b", "#7aa2f7", "#b39dff", "#56b6c2", "#c8c6d6",
                                    "#4a4760", "#ef8891", "#b5e08f", "#f0d197", "#9bb8ff", "#c9b8ff", "#7fd1db", "#ffffff" };

static void tema_oku(void) {
    char *t = ae_ayar("TEMA", "acik");
    koyu = !strcmp(t, "koyu");
    g_free(t);
}

static void renk_uygula(VteTerminal *v) {
    GdkRGBA on, arka, imlec, pal[16];
    const char **p = koyu ? PAL_KOYU : PAL_ACIK;
    gdk_rgba_parse(&arka, koyu ? "#14131c" : "#fbfbfd");
    gdk_rgba_parse(&on, koyu ? "#e4e2f0" : "#26223f");
    gdk_rgba_parse(&imlec, koyu ? "#b39dff" : "#5b4bd6");
    for (int i = 0; i < 16; i++) gdk_rgba_parse(&pal[i], p[i]);
    vte_terminal_set_colors(v, &on, &arka, pal, 16);
    vte_terminal_set_color_cursor(v, &imlec);
    vte_terminal_set_color_cursor_foreground(v, &arka);
}

static void hepsine_renk(void) {
    for (GList *l = pencereler; l; l = l->next) {
        Pencere *p = l->data;
        int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter));
        for (int i = 0; i < n; i++) renk_uygula(VTE_TERMINAL(gtk_notebook_get_nth_page(GTK_NOTEBOOK(p->defter), i)));
    }
}

static void ayar_degisti(GFileMonitor *m, GFile *f, GFile *o, GFileMonitorEvent e, gpointer d) {
    (void)m; (void)f; (void)o; (void)d;
    if (e != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT && e != G_FILE_MONITOR_EVENT_CREATED) return;
    int eski = koyu; tema_oku();
    if (eski != koyu) hepsine_renk();
}

static Pencere *pencere_yeni(void);
static GtkWidget *sekme_ekle(Pencere *p, char **komut, const char *dizin);

static VteTerminal *etkin(Pencere *p) {
    int i = gtk_notebook_get_current_page(GTK_NOTEBOOK(p->defter));
    return i < 0 ? NULL : VTE_TERMINAL(gtk_notebook_get_nth_page(GTK_NOTEBOOK(p->defter), i));
}

static char *calisma_dizini(VteTerminal *v) {
    if (!v) return NULL;
    const char *uri = vte_terminal_get_current_directory_uri(v);
    return uri ? g_filename_from_uri(uri, NULL, NULL) : NULL;
}

static void baslik_guncelle(Pencere *p) {
    VteTerminal *v = etkin(p);
    const char *t = v ? vte_terminal_get_window_title(v) : NULL;
    gtk_window_set_title(GTK_WINDOW(p->pencere), t && *t ? t : "Terminal");
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(p->defter), gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter)) > 1);
}

static void sekme_etiketi(VteTerminal *v, Pencere *p) {
    const char *t = vte_terminal_get_window_title(v);
    GtkWidget *kutu = gtk_notebook_get_tab_label(GTK_NOTEBOOK(p->defter), GTK_WIDGET(v));
    if (kutu) {
        GtkWidget *etiket = g_object_get_data(G_OBJECT(kutu), "etiket");
        gtk_label_set_text(GTK_LABEL(etiket), t && *t ? t : "Terminal");
        gtk_widget_set_tooltip_text(kutu, t);
    }
    if (v == etkin(p)) baslik_guncelle(p);
}

static void baslik_degisti(VteTerminal *v, gpointer d) { sekme_etiketi(v, d); }

static void sekme_kapat(Pencere *p, GtkWidget *v) {
    int i = gtk_notebook_page_num(GTK_NOTEBOOK(p->defter), v);
    if (i >= 0) gtk_notebook_remove_page(GTK_NOTEBOOK(p->defter), i);
    if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter)) == 0) gtk_widget_destroy(p->pencere);
    else baslik_guncelle(p);
}

static void cocuk_bitti(VteTerminal *v, int durum, gpointer d) { (void)durum; sekme_kapat(d, GTK_WIDGET(v)); }

static void kapat_tik(GtkButton *b, gpointer d) { sekme_kapat(d, g_object_get_data(G_OBJECT(b), "terminal")); }

static void baslatildi(VteTerminal *v, GPid pid, GError *e, gpointer d) {
    (void)pid; (void)d;
    if (e) {
        char *m = g_strdup_printf("\r\n%s: %s\r\n", T("Komut çalıştırılamadı", "Could not run command"), e->message);
        vte_terminal_feed(v, m, -1); g_free(m);
    }
}

static void yazi_boyutu(Pencere *p, double carpan) {
    p->olcek = carpan == 0 ? 1.0 : CLAMP(p->olcek * carpan, 0.5, 3.0);
    int n = gtk_notebook_get_n_pages(GTK_NOTEBOOK(p->defter));
    for (int i = 0; i < n; i++) vte_terminal_set_font_scale(VTE_TERMINAL(gtk_notebook_get_nth_page(GTK_NOTEBOOK(p->defter), i)), p->olcek);
}

/* --- sağ tık menüsü --- */
static void m_kopyala(GtkMenuItem *i, gpointer d) { (void)i; vte_terminal_copy_clipboard_format(d, VTE_FORMAT_TEXT); }
static void m_yapistir(GtkMenuItem *i, gpointer d) { (void)i; vte_terminal_paste_clipboard(d); }
static void m_tumu(GtkMenuItem *i, gpointer d) { (void)i; vte_terminal_select_all(d); }
static void m_sekme(GtkMenuItem *i, gpointer d) {
    (void)i; Pencere *p = d; char *dz = calisma_dizini(etkin(p));
    sekme_ekle(p, NULL, dz); g_free(dz);
}
static void m_pencere(GtkMenuItem *i, gpointer d) {
    (void)i; (void)d; Pencere *p = pencere_yeni(); sekme_ekle(p, NULL, NULL); gtk_widget_show_all(p->pencere);
}
static void m_baglanti(GtkMenuItem *i, gpointer d) {
    (void)i; gtk_show_uri_on_window(NULL, d, GDK_CURRENT_TIME, NULL);
}
static void m_baglanti_kopyala(GtkMenuItem *i, gpointer d) {
    (void)i; gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), d, -1);
}

static GtkWidget *menu_ogesi(GtkWidget *m, const char *etiket, const char *kisayol, GCallback cb, gpointer d) {
    GtkWidget *o = gtk_menu_item_new();
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 24);
    gtk_box_pack_start(GTK_BOX(k), gtk_label_new(etiket), FALSE, FALSE, 0);
    if (kisayol) { GtkWidget *s = gtk_label_new(kisayol); gtk_style_context_add_class(gtk_widget_get_style_context(s), "dim-label"); gtk_box_pack_end(GTK_BOX(k), s, FALSE, FALSE, 0); }
    gtk_container_add(GTK_CONTAINER(o), k);
    g_signal_connect(o, "activate", cb, d);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), o);
    return o;
}

static char *imlecteki_baglanti(VteTerminal *v, GdkEvent *e) {
    int etiket;
    return vte_terminal_match_check_event(v, e, &etiket);
}

static gboolean tik(GtkWidget *w, GdkEventButton *e, gpointer d) {
    VteTerminal *v = VTE_TERMINAL(w);
    Pencere *p = d;
    if (e->type != GDK_BUTTON_PRESS) return FALSE;
    if (e->button == 1 && (e->state & GDK_CONTROL_MASK)) {
        char *u = imlecteki_baglanti(v, (GdkEvent *)e);
        if (u) { gtk_show_uri_on_window(GTK_WINDOW(p->pencere), u, e->time, NULL); g_free(u); return TRUE; }
        return FALSE;
    }
    if (e->button != 3) return FALSE;
    GtkWidget *m = gtk_menu_new();
    char *u = imlecteki_baglanti(v, (GdkEvent *)e);
    if (u) {
        g_object_set_data_full(G_OBJECT(m), "u", u, g_free);
        menu_ogesi(m, T("Bağlantıyı Aç", "Open Link"), NULL, G_CALLBACK(m_baglanti), u);
        menu_ogesi(m, T("Bağlantıyı Kopyala", "Copy Link"), NULL, G_CALLBACK(m_baglanti_kopyala), u);
        gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    }
    GtkWidget *k = menu_ogesi(m, T("Kopyala", "Copy"), "Ctrl+Shift+C", G_CALLBACK(m_kopyala), v);
    gtk_widget_set_sensitive(k, vte_terminal_get_has_selection(v));
    menu_ogesi(m, T("Yapıştır", "Paste"), "Ctrl+Shift+V", G_CALLBACK(m_yapistir), v);
    menu_ogesi(m, T("Tümünü Seç", "Select All"), NULL, G_CALLBACK(m_tumu), v);
    gtk_menu_shell_append(GTK_MENU_SHELL(m), gtk_separator_menu_item_new());
    menu_ogesi(m, T("Yeni Sekme", "New Tab"), "Ctrl+Shift+T", G_CALLBACK(m_sekme), p);
    menu_ogesi(m, T("Yeni Pencere", "New Window"), "Ctrl+Shift+N", G_CALLBACK(m_pencere), p);
    gtk_widget_show_all(m);
    g_signal_connect(m, "deactivate", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_menu_popup_at_pointer(GTK_MENU(m), (GdkEvent *)e);
    return TRUE;
}

static gboolean tus(GtkWidget *w, GdkEventKey *e, gpointer d) {
    (void)w;
    Pencere *p = d;
    VteTerminal *v = etkin(p);
    guint m = e->state & gtk_accelerator_get_default_mod_mask();
    guint k = gdk_keyval_to_lower(e->keyval);
    if (m == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        switch (k) {
            case GDK_KEY_t: m_sekme(NULL, p); return TRUE;
            case GDK_KEY_w: if (v) sekme_kapat(p, GTK_WIDGET(v)); return TRUE;
            case GDK_KEY_n: m_pencere(NULL, p); return TRUE;
            case GDK_KEY_c: if (v) vte_terminal_copy_clipboard_format(v, VTE_FORMAT_TEXT); return TRUE;
            case GDK_KEY_v: if (v) vte_terminal_paste_clipboard(v); return TRUE;
        }
    }
    if (m == GDK_CONTROL_MASK) {
        switch (e->keyval) {
            case GDK_KEY_Page_Down: gtk_notebook_next_page(GTK_NOTEBOOK(p->defter)); return TRUE;
            case GDK_KEY_Page_Up: gtk_notebook_prev_page(GTK_NOTEBOOK(p->defter)); return TRUE;
            case GDK_KEY_plus: case GDK_KEY_equal: case GDK_KEY_KP_Add: yazi_boyutu(p, 1.1); return TRUE;
            case GDK_KEY_minus: case GDK_KEY_KP_Subtract: yazi_boyutu(p, 1 / 1.1); return TRUE;
            case GDK_KEY_0: case GDK_KEY_KP_0: yazi_boyutu(p, 0); return TRUE;
        }
    }
    if (m == (GDK_CONTROL_MASK | GDK_SHIFT_MASK) && (e->keyval == GDK_KEY_plus || e->keyval == GDK_KEY_asterisk)) { yazi_boyutu(p, 1.1); return TRUE; }
    return FALSE;
}

static GtkWidget *sekme_ekle(Pencere *p, char **komut, const char *dizin) {
    GtkWidget *w = vte_terminal_new();
    VteTerminal *v = VTE_TERMINAL(w);
    PangoFontDescription *f = pango_font_description_from_string("JetBrains Mono 11");
    vte_terminal_set_font(v, f); pango_font_description_free(f);
    vte_terminal_set_font_scale(v, p->olcek);
    vte_terminal_set_scrollback_lines(v, 10000);
    vte_terminal_set_cursor_blink_mode(v, VTE_CURSOR_BLINK_ON);
    vte_terminal_set_audible_bell(v, FALSE);
    vte_terminal_set_mouse_autohide(v, TRUE);
    vte_terminal_set_allow_hyperlink(v, TRUE);
    renk_uygula(v);
    VteRegex *rx = vte_regex_new_for_match(URL_DESEN, -1, PCRE2_CASELESS | PCRE2_MULTILINE, NULL);
    if (rx) { int t = vte_terminal_match_add_regex(v, rx, 0); vte_terminal_match_set_cursor_name(v, t, "pointer"); vte_regex_unref(rx); }

    /* sekme başlığı: etiket + kapat */
    GtkWidget *kutu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *etiket = gtk_label_new("Terminal");
    gtk_label_set_ellipsize(GTK_LABEL(etiket), PANGO_ELLIPSIZE_END);
    gtk_label_set_width_chars(GTK_LABEL(etiket), 16);
    gtk_label_set_max_width_chars(GTK_LABEL(etiket), 24);
    GtkWidget *kapat = gtk_button_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
    gtk_button_set_relief(GTK_BUTTON(kapat), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(kapat, FALSE);
    g_object_set_data(G_OBJECT(kapat), "terminal", w);
    g_signal_connect(kapat, "clicked", G_CALLBACK(kapat_tik), p);
    gtk_box_pack_start(GTK_BOX(kutu), etiket, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(kutu), kapat, FALSE, FALSE, 0);
    g_object_set_data(G_OBJECT(kutu), "etiket", etiket);
    gtk_widget_show_all(kutu);

    int i = gtk_notebook_append_page(GTK_NOTEBOOK(p->defter), w, kutu);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(p->defter), w, TRUE);
    gtk_container_child_set(GTK_CONTAINER(p->defter), w, "tab-expand", TRUE, NULL);
    g_signal_connect(w, "window-title-changed", G_CALLBACK(baslik_degisti), p);
    g_signal_connect(w, "child-exited", G_CALLBACK(cocuk_bitti), p);
    g_signal_connect(w, "button-press-event", G_CALLBACK(tik), p);

    const char *kabuk = g_getenv("SHELL");
    char *varsayilan[] = { (char *)(kabuk && *kabuk ? kabuk : "/bin/bash"), NULL };
    char **argv = komut ? komut : varsayilan;
    char **ortam = g_get_environ();
    ortam = g_environ_setenv(ortam, "TERM", "xterm-256color", TRUE);
    ortam = g_environ_setenv(ortam, "COLORTERM", "truecolor", TRUE);
    ortam = g_environ_setenv(ortam, "TERM_PROGRAM", "aether-terminal", TRUE);
    vte_terminal_spawn_async(v, VTE_PTY_DEFAULT, dizin ? dizin : g_get_home_dir(), argv, ortam,
                             G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, -1, NULL, baslatildi, NULL);
    g_strfreev(ortam);
    gtk_widget_show(w);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(p->defter), i);
    gtk_widget_grab_focus(w);
    baslik_guncelle(p);
    return w;
}

static void sayfa_degisti(GtkNotebook *n, GtkWidget *s, guint i, gpointer d) {
    (void)n; (void)i;
    Pencere *p = d;
    const char *t = vte_terminal_get_window_title(VTE_TERMINAL(s));
    gtk_window_set_title(GTK_WINDOW(p->pencere), t && *t ? t : "Terminal");
}

static void pencere_gitti(GtkWidget *w, gpointer d) {
    (void)w;
    pencereler = g_list_remove(pencereler, d);
    g_free(d);
    if (!pencereler) gtk_main_quit();
}

static Pencere *pencere_yeni(void) {
    Pencere *p = g_new0(Pencere, 1);
    p->olcek = 1.0;
    p->pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(p->pencere), "Terminal");
    gtk_window_set_icon_name(GTK_WINDOW(p->pencere), "aether-terminal");
    gtk_window_set_default_size(GTK_WINDOW(p->pencere), 860, 520);
    p->defter = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(p->defter), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(p->defter), FALSE);
    gtk_container_add(GTK_CONTAINER(p->pencere), p->defter);
    g_signal_connect(p->defter, "switch-page", G_CALLBACK(sayfa_degisti), p);
    g_signal_connect(p->pencere, "key-press-event", G_CALLBACK(tus), p);
    g_signal_connect(p->pencere, "destroy", G_CALLBACK(pencere_gitti), p);
    pencereler = g_list_append(pencereler, p);
    return p;
}

int main(int argc, char **argv) {
    /* -e'den sonrası komut olarak alınır (GTK seçeneklerinden önce ayıkla) */
    for (int i = 1; i < argc; i++) {
        if ((!strcmp(argv[i], "-e") || !strcmp(argv[i], "-x") || !strcmp(argv[i], "--")) && i + 1 < argc) {
            if (i + 2 == argc && strchr(argv[i + 1], ' ')) {      /* tek parça "komut arg" → kabukla çalıştır */
                ilk_komut = g_new0(char *, 4);
                ilk_komut[0] = g_strdup("/bin/sh"); ilk_komut[1] = g_strdup("-c"); ilk_komut[2] = g_strdup(argv[i + 1]);
            } else ilk_komut = g_strdupv(&argv[i + 1]);
            argc = i; break;
        }
        if ((!strcmp(argv[i], "--dizin") || !strcmp(argv[i], "--working-directory")) && i + 1 < argc) ilk_dizin = g_strdup(argv[i + 1]);
        else if (!strncmp(argv[i], "--working-directory=", 20)) ilk_dizin = g_strdup(argv[i] + 20);
    }
    g_set_prgname("aether-terminal");
    gtk_init(&argc, &argv);
    g_set_application_name("Terminal");
    tema_oku();
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "notebook header.top tab { padding: 2px 8px; min-height: 0; }"
        "notebook header.top tab button { padding: 0; min-height: 16px; min-width: 16px; }"
        "vte-terminal { padding: 2px 4px; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    char *yol = ae_ayar_yolu();
    GFile *f = g_file_new_for_path(yol);
    GFileMonitor *mon = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, NULL);
    if (mon) g_signal_connect(mon, "changed", G_CALLBACK(ayar_degisti), NULL);
    g_free(yol);

    Pencere *p = pencere_yeni();
    char *dz = ilk_dizin ? ilk_dizin : g_get_current_dir();
    sekme_ekle(p, ilk_komut, dz);
    gtk_widget_show_all(p->pencere);
    gtk_main();
    return 0;
}
