/* aether-ayarlar — Aether Ayarlar uygulaması */
#include "aether.h"

static GtkWidget *pencere;

static void uygula(void) { ae_calistir("aether-uygula"); }
static void bilgi(const char *m) {
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", m);
    gtk_dialog_run(GTK_DIALOG(d)); gtk_widget_destroy(d);
}
static GtkWidget *sayfa(const char *baslik) {
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(k), 24);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(baslik, "ae-baslik"), FALSE, FALSE, 0);
    return k;
}
static GtkWidget *satir(GtkWidget *sayfa_kutusu, const char *ad, GtkWidget *w) {
    GtkWidget *s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(s), ae_etiket(ad, NULL), TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(s), w, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(sayfa_kutusu), s, FALSE, FALSE, 0);
    return s;
}
/* anahtar=değer combosu: id'ler ayar değerleri */
static void combo_degisti(GtkComboBox *c, gpointer anahtar) {
    const char *id = gtk_combo_box_get_active_id(c); if (!id) return;
    ae_ayar_yaz(anahtar, id); uygula();
    if (!strcmp(anahtar, "DIL")) bilgi(T("Dil, oturumu kapatıp yeniden açtığında değişecek.", "The language will change after you log out and back in."));
}
static GtkWidget *combo(const char *anahtar, const char *varsayilan, const char **idler, const char **adlar) {
    GtkWidget *c = gtk_combo_box_text_new();
    for (int i = 0; idler[i]; i++) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c), idler[i], adlar[i]);
    char *v = ae_ayar(anahtar, varsayilan); gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), v); g_free(v);
    g_signal_connect(c, "changed", G_CALLBACK(combo_degisti), (gpointer)anahtar);
    return c;
}
static void anahtar_degisti(GObject *o, GParamSpec *p, gpointer anahtar) {
    ae_ayar_yaz(anahtar, gtk_switch_get_active(GTK_SWITCH(o)) ? "1" : "0"); uygula();
}
static GtkWidget *dugme(const char *anahtar, const char *varsayilan) {
    GtkWidget *s = gtk_switch_new(); char *v = ae_ayar(anahtar, varsayilan);
    gtk_switch_set_active(GTK_SWITCH(s), !strcmp(v, "1")); g_free(v);
    gtk_widget_set_valign(s, GTK_ALIGN_CENTER);
    g_signal_connect(s, "notify::active", G_CALLBACK(anahtar_degisti), (gpointer)anahtar);
    return s;
}

/* ---- Görünüm ---- */
static void duvar_sec(GtkFlowBox *f, GtkFlowBoxChild *c, gpointer d) {
    const char *yol = g_object_get_data(G_OBJECT(c), "yol");
    ae_ayar_yaz("DUVAR", yol); uygula();
}
static void duvar_ekle(GtkWidget *akis, const char *yol) {
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_scale(yol, 192, 108, FALSE, NULL); if (!pb) return;
    GtkWidget *im = gtk_image_new_from_pixbuf(pb); g_object_unref(pb);
    GtkWidget *c = gtk_flow_box_child_new(); gtk_container_add(GTK_CONTAINER(c), im);
    g_object_set_data_full(G_OBJECT(c), "yol", g_strdup(yol), g_free);
    gtk_flow_box_insert(GTK_FLOW_BOX(akis), c, -1);
}
static void dosyadan(GtkButton *b, gpointer akis) {
    GtkWidget *d = gtk_file_chooser_dialog_new(T("Duvar kağıdı seç", "Choose wallpaper"), GTK_WINDOW(pencere), GTK_FILE_CHOOSER_ACTION_OPEN,
        T("Vazgeç", "Cancel"), GTK_RESPONSE_CANCEL, T("Seç", "Choose"), GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *f = gtk_file_filter_new(); gtk_file_filter_add_pixbuf_formats(f); gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(d), f);
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_ACCEPT) {
        char *y = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(d));
        ae_ayar_yaz("DUVAR", y); duvar_ekle(akis, y); gtk_widget_show_all(akis); uygula(); g_free(y);
    }
    gtk_widget_destroy(d);
}
static GtkWidget *gorunum(void) {
    GtkWidget *k = sayfa(T("Görünüm", "Appearance"));
    const char *ti[] = {"acik", "koyu", NULL}, *ta[] = {T("Açık", "Light"), T("Koyu", "Dark"), NULL};
    satir(k, T("Tema", "Theme"), combo("TEMA", "acik", ti, ta));
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Duvar kağıdı", "Wallpaper"), NULL), FALSE, FALSE, 0);
    GtkWidget *akis = gtk_flow_box_new();
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(akis), 3);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(akis), 3);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(akis), TRUE);
    GDir *dz = g_dir_open("/usr/share/aether/wallpapers", 0, NULL); const char *n;
    if (dz) { while ((n = g_dir_read_name(dz))) { char *y = g_build_filename("/usr/share/aether/wallpapers", n, NULL); duvar_ekle(akis, y); g_free(y); } g_dir_close(dz); }
    g_signal_connect(akis, "child-activated", G_CALLBACK(duvar_sec), NULL);
    gtk_box_pack_start(GTK_BOX(k), akis, FALSE, FALSE, 0);
    GtkWidget *b = gtk_button_new_with_label(T("Kendi resmimi seç…", "Choose my own image…"));
    gtk_widget_set_halign(b, GTK_ALIGN_START);
    g_signal_connect(b, "clicked", G_CALLBACK(dosyadan), akis);
    gtk_box_pack_start(GTK_BOX(k), b, FALSE, FALSE, 0);
    return k;
}

/* ---- Dil ve klavye ---- */
static GtkWidget *dil(void) {
    GtkWidget *k = sayfa(T("Dil ve Klavye", "Language & Keyboard"));
    const char *di[] = {"tr", "en", NULL}, *da[] = {"Türkçe", "English", NULL};
    satir(k, T("Sistem dili", "System language"), combo("DIL", "tr", di, da));
    const char *ki[] = {"tr", "trf", "us", NULL}, *ka[] = {T("Türkçe Q", "Turkish Q"), T("Türkçe F", "Turkish F"), "English (US)", NULL};
    satir(k, T("Klavye düzeni", "Keyboard layout"), combo("KLAVYE", "tr", ki, ka));
    GtkWidget *e = gtk_entry_new(); gtk_entry_set_placeholder_text(GTK_ENTRY(e), T("Klavyeyi burada dene: ğüşıöç", "Test your keyboard here"));
    gtk_box_pack_start(GTK_BOX(k), e, FALSE, FALSE, 0);
    return k;
}

/* ---- Ekran ---- */
static char *cikis_adi;
static void coz_degisti(GtkComboBox *c, gpointer d) {
    const char *m = gtk_combo_box_get_active_id(c); if (!m || !cikis_adi) return;
    char *kmt = g_strdup_printf("xrandr --output %s --mode %s", cikis_adi, m);
    g_spawn_command_line_sync(kmt, NULL, NULL, NULL, NULL); g_free(kmt);
    ae_ayar_yaz("COZUNURLUK", m); uygula();
}
static GtkWidget *ekran(void) {
    GtkWidget *k = sayfa(T("Ekran", "Display"));
    GtkWidget *c = gtk_combo_box_text_new(); char *out = NULL;
    if (g_spawn_command_line_sync("xrandr", &out, NULL, NULL, NULL) && out) {
        char **s = g_strsplit(out, "\n", -1); int aktif = 0;
        for (int i = 0; s[i]; i++) {
            if (strstr(s[i], " connected")) { g_free(cikis_adi); cikis_adi = g_strndup(s[i], strcspn(s[i], " ")); aktif = 1; continue; }
            if (strstr(s[i], " disconnected")) { aktif = 0; continue; }
            if (aktif && s[i][0] == ' ') {
                char mod[32]; if (sscanf(s[i], " %31s", mod) == 1 && strchr(mod, 'x')) {
                    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c), mod, mod);
                    if (strchr(s[i], '*')) gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), mod);
                }
            }
        }
        g_strfreev(s); g_free(out);
    }
    g_signal_connect(c, "changed", G_CALLBACK(coz_degisti), NULL);
    satir(k, T("Çözünürlük", "Resolution"), c);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Sanal makinede daha fazla çözünürlük için pencereyi büyütüp sanal makinenin ekran ayarlarını değiştirebilirsin.",
        "In a virtual machine, more resolutions may appear after changing the VM's display settings."), "ae-alt"), FALSE, FALSE, 0);
    return k;
}

/* ---- Kilit ve ekran koruyucu ---- */
static void onizle(GtkButton *b, gpointer d) { ae_calistir("aether-yildizlar"); }
static GtkWidget *kilit(void) {
    GtkWidget *k = sayfa(T("Kilit ve Ekran Koruyucu", "Lock & Screensaver"));
    const char *si[] = {"0", "5", "10", "15", "30", NULL};
    const char *sa[] = {T("Asla", "Never"), T("5 dakika", "5 minutes"), T("10 dakika", "10 minutes"), T("15 dakika", "15 minutes"), T("30 dakika", "30 minutes"), NULL};
    satir(k, T("Boşta kalınca ekranı kilitle", "Lock screen when idle"), combo("KILIT_DK", "10", si, sa));
    satir(k, T("Ekran koruyucu (yıldız alanı)", "Screensaver (starfield)"), dugme("EKRAN_KORUYUCU", "0"));
    const char *ki[] = {"2", "5", "10", NULL}, *ka[] = {T("2 dakika", "2 minutes"), T("5 dakika", "5 minutes"), T("10 dakika", "10 minutes"), NULL};
    satir(k, T("Ekran koruyucu başlama süresi", "Screensaver delay"), combo("KORUYUCU_DK", "5", ki, ka));
    GtkWidget *b = gtk_button_new_with_label(T("Önizle", "Preview")); gtk_widget_set_halign(b, GTK_ALIGN_START);
    g_signal_connect(b, "clicked", G_CALLBACK(onizle), NULL);
    gtk_box_pack_start(GTK_BOX(k), b, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Kısayol: Super+L ekranı hemen kilitler.", "Shortcut: Super+L locks the screen immediately."), "ae-alt"), FALSE, FALSE, 0);
    return k;
}

/* ---- Saat ---- */
static void dilim_degisti(GtkComboBox *c, gpointer d) {
    const char *z = gtk_combo_box_get_active_id(c); if (!z) return;
    char *kmt = g_strdup_printf("doas /usr/libexec/aether/saat-dilimi %s", z);
    g_spawn_command_line_async(kmt, NULL); g_free(kmt);
}
static GtkWidget *saat(void) {
    GtkWidget *k = sayfa(T("Tarih ve Saat", "Date & Time"));
    const char *z[] = {"Europe/Istanbul", "Europe/London", "Europe/Berlin", "Europe/Moscow", "Asia/Baku", "Asia/Dubai", "Asia/Tokyo", "America/New_York", "America/Los_Angeles", "UTC", NULL};
    GtkWidget *c = gtk_combo_box_text_new();
    for (int i = 0; z[i]; i++) gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(c), z[i], z[i]);
    char *cur = g_file_read_link("/etc/localtime", NULL);
    const char *ad = cur && strstr(cur, "zoneinfo/") ? strstr(cur, "zoneinfo/") + 9 : "Europe/Istanbul";
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(c), ad); g_free(cur);
    g_signal_connect(c, "changed", G_CALLBACK(dilim_degisti), NULL);
    satir(k, T("Saat dilimi", "Time zone"), c);
    return k;
}

/* ---- Eklentiler ---- */
typedef struct { const char *id, *ad_tr, *ad_en, *ac_tr, *ac_en; GtkWidget *dugme, *durum; } Eklenti;
static Eklenti eklentiler[] = {
    {"muzik", "Müzik ve sesler", "Music & sounds", "Audacious müzik çalar ve Aether ses efektleri (açılış, bildirim, kapanış).", "Audacious music player and Aether sound effects."},
    {"animasyon", "Pencere animasyonları", "Window animations", "Pencereler açılıp kapanırken yumuşak geçişler ve gölgeler (picom).", "Smooth open/close transitions and shadows (picom)."},
    {"gelistirici", "Motor geliştirme", "Engine development", "SDL2, OpenGL başlık dosyaları ve CMake: oyun motorlarını Aether içinde derlemek için.", "SDL2/OpenGL headers and CMake for building game engines inside Aether."},
    {NULL}
};
static int kurulu_mu(const char *id) {
    char *kmt = g_strdup_printf("/usr/bin/aether-eklenti durum %s", id); int st = 1;
    g_spawn_command_line_sync(kmt, NULL, NULL, &st, NULL); g_free(kmt);
    return g_spawn_check_exit_status(st, NULL);
}
static void ek_yenile(Eklenti *e) {
    int k = kurulu_mu(e->id);
    gtk_label_set_text(GTK_LABEL(e->durum), k ? T("Kurulu", "Installed") : T("Kurulu değil", "Not installed"));
    gtk_button_set_label(GTK_BUTTON(e->dugme), k ? T("Kaldır", "Remove") : T("Kur", "Install"));
    gtk_widget_set_sensitive(e->dugme, TRUE);
}
typedef struct { Eklenti *e; GtkWidget *dlg; GtkTextBuffer *tb; } Is;
static gboolean okundu(GIOChannel *ch, GIOCondition c, gpointer d) {
    Is *is = d; char *s = NULL; gsize n;
    if (c & G_IO_IN && g_io_channel_read_line(ch, &s, &n, NULL, NULL) == G_IO_STATUS_NORMAL) {
        GtkTextIter it; gtk_text_buffer_get_end_iter(is->tb, &it); gtk_text_buffer_insert(is->tb, &it, s, -1); g_free(s); return TRUE;
    }
    return FALSE;
}
static void bitti(GPid pid, gint st, gpointer d) {
    Is *is = d; g_spawn_close_pid(pid);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(is->dlg), GTK_RESPONSE_CLOSE, TRUE);
    GtkTextIter it; gtk_text_buffer_get_end_iter(is->tb, &it);
    gtk_text_buffer_insert(is->tb, &it, g_spawn_check_exit_status(st, NULL) ? T("\n✔ Tamamlandı.\n", "\n✔ Done.\n") : T("\n✘ Hata oluştu. İnternet bağlantını kontrol et.\n", "\n✘ Failed. Check your internet connection.\n"), -1);
    ek_yenile(is->e); uygula();
}
static void ek_tikla(GtkButton *b, gpointer d) {
    Eklenti *e = d; int k = kurulu_mu(e->id);
    gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);
    Is *is = g_new0(Is, 1); is->e = e;
    is->dlg = gtk_dialog_new_with_buttons(ae_en() ? e->ad_en : e->ad_tr, GTK_WINDOW(pencere), GTK_DIALOG_MODAL, T("Kapat", "Close"), GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(is->dlg), 560, 320);
    GtkWidget *tv = gtk_text_view_new(); gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE); gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    is->tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_container_add(GTK_CONTAINER(sc), tv);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(is->dlg))), sc, TRUE, TRUE, 0);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(is->dlg), GTK_RESPONSE_CLOSE, FALSE);
    g_signal_connect(is->dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(is->dlg);
    char *argv[] = {"doas", "/usr/bin/aether-eklenti", k ? "kaldir" : "kur", (char *)e->id, NULL};
    GPid pid; int out;
    if (g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &pid, NULL, &out, NULL, NULL)) {
        GIOChannel *ch = g_io_channel_unix_new(out);
        g_io_add_watch(ch, G_IO_IN | G_IO_HUP, okundu, is);
        g_child_watch_add(pid, bitti, is);
    }
}
static GtkWidget *eklenti_sayfasi(void) {
    GtkWidget *k = sayfa(T("Eklentiler", "Add-ons"));
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Aether'i hafif tutmak için bu parçalar isteğe bağlı. Kurmak için internet gerekir.",
        "These parts are optional to keep Aether light. Installing needs internet."), "ae-alt"), FALSE, FALSE, 0);
    for (Eklenti *e = eklentiler; e->id; e++) {
        GtkWidget *kart = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_style_context_add_class(gtk_widget_get_style_context(kart), "ae-kart");
        GtkWidget *sol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        char m[256]; snprintf(m, sizeof m, "<b>%s</b>", ae_en() ? e->ad_en : e->ad_tr);
        GtkWidget *ad = ae_etiket("", NULL); gtk_label_set_markup(GTK_LABEL(ad), m);
        gtk_box_pack_start(GTK_BOX(sol), ad, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(sol), ae_etiket(ae_en() ? e->ac_en : e->ac_tr, "ae-alt"), FALSE, FALSE, 0);
        e->durum = ae_etiket("", "ae-alt"); gtk_box_pack_start(GTK_BOX(sol), e->durum, FALSE, FALSE, 0);
        e->dugme = gtk_button_new_with_label(""); gtk_widget_set_valign(e->dugme, GTK_ALIGN_CENTER);
        g_signal_connect(e->dugme, "clicked", G_CALLBACK(ek_tikla), e);
        gtk_box_pack_start(GTK_BOX(kart), sol, TRUE, TRUE, 0); gtk_box_pack_end(GTK_BOX(kart), e->dugme, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(k), kart, FALSE, FALSE, 0);
        ek_yenile(e);
    }
    satir(k, T("Ses efektleri (Müzik ve sesler eklentisiyle)", "Sound effects (with Music & sounds add-on)"), dugme("SESLER", "1"));
    satir(k, T("Pencere animasyonları (eklentiyle)", "Window animations (with add-on)"), dugme("ANIMASYON", "1"));
    return k;
}

static void hakkinda(GtkButton *b, gpointer d) { ae_calistir("aether-hakkinda"); }

int main(int argc, char **argv) {
    gtk_init(&argc, &argv); ae_css();
    pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pencere), T("Ayarlar", "Settings"));
    gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-ayarlar");
    gtk_window_set_default_size(GTK_WINDOW(pencere), 820, 540);
    g_signal_connect(pencere, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *yatay = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *yigin = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(yigin), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    struct { GtkWidget *(*f)(void); const char *id, *tr, *en; } s[] = {
        {gorunum, "gorunum", "Görünüm", "Appearance"}, {dil, "dil", "Dil ve Klavye", "Language & Keyboard"},
        {ekran, "ekran", "Ekran", "Display"}, {kilit, "kilit", "Kilit ve Koruyucu", "Lock & Screensaver"},
        {saat, "saat", "Tarih ve Saat", "Date & Time"}, {eklenti_sayfasi, "eklenti", "Eklentiler", "Add-ons"}, {NULL}};
    for (int i = 0; s[i].f; i++) {
        GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_add(GTK_CONTAINER(sc), s[i].f());
        gtk_stack_add_titled(GTK_STACK(yigin), sc, s[i].id, ae_en() ? s[i].en : s[i].tr);
    }
    GtkWidget *sol = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *kenar = gtk_stack_sidebar_new(); gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(kenar), GTK_STACK(yigin));
    gtk_widget_set_size_request(kenar, 200, -1);
    gtk_box_pack_start(GTK_BOX(sol), kenar, TRUE, TRUE, 0);
    GtkWidget *hb = gtk_button_new_with_label(T("Aether Hakkında", "About Aether"));
    g_signal_connect(hb, "clicked", G_CALLBACK(hakkinda), NULL);
    gtk_box_pack_end(GTK_BOX(sol), hb, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(yatay), sol, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(yatay), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(yatay), yigin, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(pencere), yatay);
    gtk_widget_show_all(pencere);
    if (argc > 1) gtk_stack_set_visible_child_name(GTK_STACK(yigin), argv[1]);
    gtk_main();
    return 0;
}
