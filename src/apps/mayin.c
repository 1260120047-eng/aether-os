/* Aether Mayın Tarlası — Aether 1.1 "Nebula" */
#include "aether.h"
#include <time.h>

typedef struct { int mayin, acik, bayrak, komsu; GtkWidget *b; } Hucre;
static int G = 9, Y = 9, M = 10;
static Hucre *h; static GtkWidget *pencere, *izgara, *sayac, *saat, *yuz, *kutu;
static int ilk, bitti, acilan, bayraklar, saniye; static guint zamanlayici;
static const char *renk[] = {"", "#1e63d6", "#2e8b3a", "#d32f2f", "#3a1fa8", "#8b1a1a", "#00838f", "#222", "#777"};
#define H(x,y) (&h[(y)*G+(x)])

static void yuz_yaz(const char *s) { gtk_button_set_label(GTK_BUTTON(yuz), s); }
static void sayac_yaz(void) { char b[32]; snprintf(b, sizeof b, "⚑ %03d", M - bayraklar); gtk_label_set_text(GTK_LABEL(sayac), b); }
static gboolean tik(gpointer d) { char b[32]; if (saniye < 999) saniye++; snprintf(b, sizeof b, "⏱ %03d", saniye); gtk_label_set_text(GTK_LABEL(saat), b); return TRUE; }

static void hucre_ciz(int x, int y) {
    Hucre *c = H(x, y); GtkWidget *b = c->b;
    GtkStyleContext *sc = gtk_widget_get_style_context(b);
    if (c->acik) {
        gtk_style_context_add_class(sc, "acik");
        if (c->mayin) gtk_button_set_label(GTK_BUTTON(b), "✹");
        else if (c->komsu) {
            char m[96]; snprintf(m, sizeof m, "<b><span foreground='%s'>%d</span></b>", renk[c->komsu], c->komsu);
            gtk_button_set_label(GTK_BUTTON(b), ""); GtkWidget *l = gtk_bin_get_child(GTK_BIN(b));
            gtk_label_set_markup(GTK_LABEL(l), m);
        } else gtk_button_set_label(GTK_BUTTON(b), "");
    } else gtk_button_set_label(GTK_BUTTON(b), c->bayrak ? "⚑" : "");
}

static void yerlestir(int ox, int oy) {
    int k = 0;
    while (k < M) {
        int x = rand() % G, y = rand() % Y;
        if (H(x, y)->mayin || (abs(x - ox) <= 1 && abs(y - oy) <= 1)) continue;
        H(x, y)->mayin = 1; k++;
    }
    for (int y = 0; y < Y; y++) for (int x = 0; x < G; x++) {
        int n = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy;
            if (a >= 0 && b >= 0 && a < G && b < Y && H(a, b)->mayin) n++;
        }
        H(x, y)->komsu = n;
    }
}

static void oyun_bitir(int kazandi) {
    bitti = 1; if (zamanlayici) { g_source_remove(zamanlayici); zamanlayici = 0; }
    for (int i = 0; i < G * Y; i++) if (h[i].mayin) { if (kazandi) h[i].bayrak = 1; else h[i].acik = 1; hucre_ciz(i % G, i / G); }
    yuz_yaz(kazandi ? "★" : "☹");
    char m[128];
    if (kazandi) snprintf(m, sizeof m, T("Tebrikler! %d saniyede kazandın.", "Congratulations! You won in %d seconds."), saniye);
    else snprintf(m, sizeof m, "%s", T("Mayına bastın! Tekrar dene.", "You hit a mine! Try again."));
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, kazandi ? GTK_MESSAGE_INFO : GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", m);
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}

static void ac(int x, int y) {
    if (x < 0 || y < 0 || x >= G || y >= Y) return;
    Hucre *c = H(x, y); if (c->acik || c->bayrak) return;
    c->acik = 1; acilan++; hucre_ciz(x, y);
    if (c->mayin) { oyun_bitir(0); return; }
    if (!c->komsu) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (dx || dy) ac(x + dx, y + dy);
}

static gboolean basildi(GtkWidget *w, GdkEventButton *e, gpointer d) {
    if (bitti || e->type != GDK_BUTTON_PRESS) return TRUE;
    int i = GPOINTER_TO_INT(d), x = i % G, y = i / G; Hucre *c = H(x, y);
    if (e->button == 3) {
        if (!c->acik) { c->bayrak = !c->bayrak; bayraklar += c->bayrak ? 1 : -1; hucre_ciz(x, y); sayac_yaz(); }
        return TRUE;
    }
    if (e->button != 1) return TRUE;
    if (ilk) { ilk = 0; yerlestir(x, y); zamanlayici = g_timeout_add_seconds(1, tik, NULL); }
    if (c->acik && c->komsu) {   /* akor: çevredeki bayrak sayısı eşitse komşuları aç */
        int f = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int a = x + dx, b = y + dy; if (a >= 0 && b >= 0 && a < G && b < Y && H(a, b)->bayrak) f++; }
        if (f == c->komsu) for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if (!bitti) ac(x + dx, y + dy);
    } else ac(x, y);
    if (!bitti && acilan == G * Y - M) oyun_bitir(1);
    return TRUE;
}

static void yeni_oyun(void) {
    if (zamanlayici) { g_source_remove(zamanlayici); zamanlayici = 0; }
    if (izgara) gtk_widget_destroy(izgara);
    g_free(h); h = g_new0(Hucre, G * Y);
    ilk = 1; bitti = 0; acilan = 0; bayraklar = 0; saniye = 0;
    izgara = gtk_grid_new();
    gtk_widget_set_halign(izgara, GTK_ALIGN_CENTER);
    for (int y = 0; y < Y; y++) for (int x = 0; x < G; x++) {
        GtkWidget *b = gtk_button_new_with_label("");
        gtk_widget_set_size_request(b, 30, 30);
        gtk_style_context_add_class(gtk_widget_get_style_context(b), "hucre");
        g_signal_connect(b, "button-press-event", G_CALLBACK(basildi), GINT_TO_POINTER(y * G + x));
        H(x, y)->b = b; gtk_grid_attach(GTK_GRID(izgara), b, x, y, 1, 1);
    }
    gtk_box_pack_start(GTK_BOX(kutu), izgara, TRUE, TRUE, 0);
    gtk_widget_show_all(izgara);
    yuz_yaz("☺"); sayac_yaz(); gtk_label_set_text(GTK_LABEL(saat), "⏱ 000");
    gtk_window_resize(GTK_WINDOW(pencere), 1, 1);
}
static void seviye(GtkComboBox *c, gpointer d) {
    switch (gtk_combo_box_get_active(c)) { case 0: G = 9; Y = 9; M = 10; break; case 1: G = 16; Y = 16; M = 40; break; default: G = 30; Y = 16; M = 99; }
    yeni_oyun();
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv); srand(time(NULL)); ae_css();
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p,
        "button.hucre { min-width: 28px; min-height: 28px; padding: 0; border-radius: 0; font-weight: bold; }"
        "button.hucre.acik { background: alpha(currentColor,0.06); border-color: alpha(currentColor,0.12); box-shadow: none; }"
        ".ust { font-family: monospace; font-size: 13pt; }", -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p), 800);
    pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pencere), T("Mayın Tarlası", "Minesweeper"));
    gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-mayin");
    gtk_window_set_resizable(GTK_WINDOW(pencere), FALSE);
    g_signal_connect(pencere, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    kutu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(kutu), 10);
    GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(ust), "ust");
    sayac = gtk_label_new(""); saat = gtk_label_new("");
    yuz = gtk_button_new_with_label("☺"); g_signal_connect_swapped(yuz, "clicked", G_CALLBACK(yeni_oyun), NULL);
    GtkWidget *sev = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sev), T("Kolay 9×9", "Easy 9×9"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sev), T("Orta 16×16", "Medium 16×16"));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(sev), T("Zor 30×16", "Hard 30×16"));
    gtk_combo_box_set_active(GTK_COMBO_BOX(sev), 0);
    g_signal_connect(sev, "changed", G_CALLBACK(seviye), NULL);
    gtk_box_pack_start(GTK_BOX(ust), sayac, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ust), sev, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ust), yuz, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ust), saat, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(kutu), ust, FALSE, FALSE, 0);
    GtkWidget *ipucu = gtk_label_new(T("Sol tık: aç  ·  Sağ tık: bayrak  ·  Sayıya tık: çevresini aç", "Left: open  ·  Right: flag  ·  Click number: chord"));
    gtk_style_context_add_class(gtk_widget_get_style_context(ipucu), "ae-alt");
    gtk_box_pack_end(GTK_BOX(kutu), ipucu, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pencere), kutu);
    yeni_oyun();
    gtk_widget_show_all(pencere);
    gtk_main();
    return 0;
}
