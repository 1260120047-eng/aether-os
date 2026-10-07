/* aether-hosgeldin — Canlı oturumda "Canlı dene / Diske kur", kurulu sistemde ilk karşılama */
#include "aether.h"

static GtkWidget *pencere;
static void kur(GtkButton *b, gpointer d) { ae_calistir("aether-kurulum"); gtk_main_quit(); }
static void gosterme(GtkToggleButton *t, gpointer d) { ae_ayar_yaz("KARSILAMA", gtk_toggle_button_get_active(t) ? "0" : "1"); }

static GtkWidget *kisayol(GtkWidget *g, int satir, const char *tus, const char *ne) {
    GtkWidget *a = gtk_label_new(NULL), *b = gtk_label_new(ne);
    char m[128]; snprintf(m, sizeof m, "<tt><b>%s</b></tt>", tus); gtk_label_set_markup(GTK_LABEL(a), m);
    gtk_label_set_xalign(GTK_LABEL(a), 0); gtk_label_set_xalign(GTK_LABEL(b), 0);
    gtk_grid_attach(GTK_GRID(g), a, 0, satir, 1, 1); gtk_grid_attach(GTK_GRID(g), b, 1, satir, 1, 1);
    return g;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv); ae_css();
    int canli = ae_canli();
    pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pencere), T("Aether'e hoş geldin", "Welcome to Aether"));
    gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-hosgeldin");
    gtk_window_set_position(GTK_WINDOW(pencere), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(pencere), 560, -1);
    g_signal_connect(pencere, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(k), 24);
    GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_pack_start(GTK_BOX(ust), ae_logo(72), FALSE, FALSE, 0);
    GtkWidget *bk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_pack_start(GTK_BOX(bk), ae_etiket("A E T H E R", "ae-baslik"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bk), ae_etiket("1.0 · Nebula", "ae-alt"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ust), bk, FALSE, FALSE, 0);
    gtk_box_set_center_widget(GTK_BOX(ust), NULL);
    gtk_box_pack_start(GTK_BOX(k), ust, FALSE, FALSE, 0);

    char selam[256];
    if (canli) snprintf(selam, sizeof selam, "%s", T("Aether'i şu an kurmadan, doğrudan RAM üzerinden çalıştırıyorsun. "
        "Önce gönül rahatlığıyla deneyebilir, beğenirsen diske kurabilirsin.",
        "You are running Aether straight from RAM without installing it. "
        "Try it freely first, and install it to disk when you're ready."));
    else snprintf(selam, sizeof selam, T("Hoş geldin, %s! Aether kurulumun hazır.", "Welcome, %s! Your Aether is ready."), g_get_real_name());
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(selam, NULL), FALSE, FALSE, 0);

    GtkWidget *kart = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(kart), "ae-kart");
    gtk_box_pack_start(GTK_BOX(kart), ae_etiket(T("Başlarken", "Getting started"), NULL), FALSE, FALSE, 0);
    GtkWidget *g = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(g), 18); gtk_grid_set_row_spacing(GTK_GRID(g), 4);
    kisayol(g, 0, T("Sağ tık", "Right click"), T("Masaüstü menüsü", "Desktop menu"));
    kisayol(g, 1, "Super", T("Menüyü aç", "Open menu"));
    kisayol(g, 2, "Ctrl+Alt+T", "Terminal");
    kisayol(g, 3, "Super+E", T("Dosyalar", "Files"));
    kisayol(g, 4, "Super+L", T("Ekranı kilitle", "Lock screen"));
    kisayol(g, 5, "Alt+Tab · Alt+F4", T("Pencere değiştir · kapat", "Switch · close window"));
    kisayol(g, 6, "Print", T("Ekran görüntüsü (~/Resimler)", "Screenshot (~/Pictures)"));
    kisayol(g, 7, "aether-info", T("Terminalde sistem bilgisi", "System info in terminal"));
    gtk_box_pack_start(GTK_BOX(kart), g, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), kart, FALSE, FALSE, 0);

    GtkWidget *alt = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    if (canli) {
        GtkWidget *dene = gtk_button_new_with_label(T("Canlı Dene", "Try Live"));
        GtkWidget *kurb = gtk_button_new_with_label(T("Diske Kur", "Install to Disk"));
        gtk_style_context_add_class(gtk_widget_get_style_context(kurb), "suggested-action");
        gtk_style_context_add_class(gtk_widget_get_style_context(kurb), "ae-buyuk");
        gtk_style_context_add_class(gtk_widget_get_style_context(dene), "ae-buyuk");
        g_signal_connect(dene, "clicked", G_CALLBACK(gtk_main_quit), NULL);
        g_signal_connect(kurb, "clicked", G_CALLBACK(kur), NULL);
        gtk_box_pack_end(GTK_BOX(alt), kurb, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(alt), dene, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(alt), ae_etiket(T("Kurulum menüde de var.", "Installer is also in the menu."), "ae-alt"), FALSE, FALSE, 0);
    } else {
        GtkWidget *cb = gtk_check_button_new_with_label(T("Bir daha gösterme", "Don't show again"));
        g_signal_connect(cb, "toggled", G_CALLBACK(gosterme), NULL);
        GtkWidget *tamam = gtk_button_new_with_label(T("Başla", "Let's go"));
        gtk_style_context_add_class(gtk_widget_get_style_context(tamam), "suggested-action");
        g_signal_connect(tamam, "clicked", G_CALLBACK(gtk_main_quit), NULL);
        gtk_box_pack_start(GTK_BOX(alt), cb, FALSE, FALSE, 0);
        gtk_box_pack_end(GTK_BOX(alt), tamam, FALSE, FALSE, 0);
    }
    gtk_box_pack_end(GTK_BOX(k), alt, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(k), ae_etiket(T("Yapımcı: " AETHER_YAPIMCI, "Made by " AETHER_YAPIMCI), "ae-alt"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pencere), k);
    gtk_widget_show_all(pencere);
    gtk_main();
    return 0;
}
