/* aether-oyunlar — Aether oyun başlatıcı.
   /usr/share/aether/oyunlar ve ~/Oyunlar içindeki .desktop dosyalarını listeler. */
#include "aether.h"
#include <gio/gdesktopappinfo.h>

static GtkWidget *akis;
static void baslat(GtkButton *b, gpointer d) {
    GDesktopAppInfo *a = d; GError *e = NULL;
    if (!g_app_info_launch(G_APP_INFO(a), NULL, NULL, &e)) {
        GtkWidget *m = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", e ? e->message : "?");
        gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m); g_clear_error(&e);
    }
}
static void ekle(GDesktopAppInfo *a) {
    GtkWidget *b = gtk_button_new();
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GIcon *ic = g_app_info_get_icon(G_APP_INFO(a));
    GtkWidget *im = ic ? gtk_image_new_from_gicon(ic, GTK_ICON_SIZE_DIALOG) : gtk_image_new_from_icon_name("aether-oyunlar", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(im), 64);
    GtkWidget *l = gtk_label_new(g_app_info_get_name(G_APP_INFO(a)));
    gtk_label_set_max_width_chars(GTK_LABEL(l), 16); gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    gtk_label_set_justify(GTK_LABEL(l), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(k), im, FALSE, FALSE, 0); gtk_box_pack_start(GTK_BOX(k), l, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(b), k);
    const char *acik = g_app_info_get_description(G_APP_INFO(a));
    if (acik) gtk_widget_set_tooltip_text(b, acik);
    gtk_widget_set_size_request(b, 140, 130);
    g_signal_connect(b, "clicked", G_CALLBACK(baslat), a);
    gtk_flow_box_insert(GTK_FLOW_BOX(akis), b, -1);
}
static int tara(const char *dizin) {
    GDir *d = g_dir_open(dizin, 0, NULL); int n = 0; const char *f;
    if (!d) return 0;
    while ((f = g_dir_read_name(d))) {
        if (!g_str_has_suffix(f, ".desktop")) continue;
        char *yol = g_build_filename(dizin, f, NULL);
        GDesktopAppInfo *a = g_desktop_app_info_new_from_filename(yol);
        if (a) { ekle(a); n++; }
        g_free(yol);
    }
    g_dir_close(d); return n;
}
int main(int argc, char **argv) {
    gtk_init(&argc, &argv); ae_css();
    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), T("Oyunlar", "Games"));
    gtk_window_set_icon_name(GTK_WINDOW(w), "aether-oyunlar");
    gtk_window_set_default_size(GTK_WINDOW(w), 620, 400);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(k), 18);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("O Y U N L A R", "G A M E S"), "ae-baslik"), FALSE, FALSE, 0);
    char *ev = g_build_filename(g_get_home_dir(), T("Oyunlar", "Games"), NULL);
    char ipucu[300]; snprintf(ipucu, sizeof ipucu, T("Kendi oyunlarını eklemek için .desktop dosyasını %s klasörüne koy.",
        "To add your own games, put a .desktop file in %s."), ev);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(ipucu, "ae-alt"), FALSE, FALSE, 0);
    akis = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(akis), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(akis), TRUE);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(akis), 10); gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(akis), 10);
    int n = tara("/usr/share/aether/oyunlar") + tara(ev);
    if (!n) gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Henüz oyun yok.", "No games yet."), NULL), FALSE, FALSE, 0);
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_container_add(GTK_CONTAINER(sc), akis);
    gtk_box_pack_start(GTK_BOX(k), sc, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(w), k);
    gtk_widget_show_all(w);
    gtk_main();
    return 0;
}
