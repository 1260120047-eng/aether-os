/* aether-ayarlar — Aether Ayarlar uygulaması */
#include "aether.h"
#include "ag.h"

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

    /* grafik hızlandırma durumu */
    char *d = NULL, *cizici = NULL, *sanal = NULL; int hiz = 0;
    if (g_spawn_command_line_sync("env LANG=C /usr/libexec/aether/grafik-ayarla durum", &d, NULL, NULL, NULL) && d) {
        char **s = g_strsplit(d, "\n", -1);
        for (int i = 0; s[i]; i++) {
            char *iki = strstr(s[i], ": "); if (!iki) continue;
            if (g_str_has_prefix(s[i], "OpenGL")) cizici = g_strdup(iki + 2);
            else if (g_str_has_prefix(s[i], "sanal")) sanal = g_strdup(iki + 2);
            else if (g_str_has_prefix(s[i], "3D")) hiz = strstr(iki, "açık") != NULL;
        }
        g_strfreev(s); g_free(d);
    }
    GtkWidget *gh = ae_etiket(hiz ? T("Açık", "On") : T("Kapalı (yazılımla çizim)", "Off (software rendering)"), NULL);
    satir(k, T("Grafik hızlandırma", "Graphics acceleration"), gh);
    if (cizici) satir(k, T("OpenGL çizici", "OpenGL renderer"), ae_etiket(cizici, "ae-alt"));
    if (!hiz && sanal && strcmp(sanal, "none")) {
        const char *ip = !strcmp(sanal, "oracle")
            ? T("VirtualBox'ta açmak için makineyi kapat, Ayarlar › Ekran'da Grafik denetleyiciyi \"VMSVGA\" yap, \"3D hızlandırmayı etkinleştir\"i işaretle ve Video belleğini 128 MB'a çıkar.",
                "To enable it in VirtualBox, shut the VM down, set Settings › Display › Graphics Controller to \"VMSVGA\", tick \"Enable 3D Acceleration\" and raise Video Memory to 128 MB.")
            : !strcmp(sanal, "vmware")
            ? T("VMware'de makineyi kapat, Ayarlar › Display'de \"Accelerate 3D graphics\" seçeneğini aç.", "In VMware, shut the VM down and turn on Settings › Display › \"Accelerate 3D graphics\".")
            : T("QEMU/virt-manager'da ekran kartını \"virtio\" yap ve \"3D acceleration\" (OpenGL) seçeneğini aç.", "In QEMU/virt-manager, use the \"virtio\" video model with \"3D acceleration\" (OpenGL) enabled.");
        gtk_box_pack_start(GTK_BOX(k), ae_etiket(ip, "ae-alt"), FALSE, FALSE, 0);
    }
    g_free(cizici); g_free(sanal);
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
static void surucu_yenile(void);
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
    if (is->e) { ek_yenile(is->e); uygula(); } else surucu_yenile();
}
static void komut_penceresi(const char *baslik, char **argv, Eklenti *e);
static void ek_tikla(GtkButton *b, gpointer d) {
    Eklenti *e = d; int k = kurulu_mu(e->id);
    gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);
    char *argv[] = {"doas", "/usr/bin/aether-eklenti", k ? "kaldir" : "kur", (char *)e->id, NULL};
    komut_penceresi(ae_en() ? e->ad_en : e->ad_tr, argv, e);
}
static void komut_penceresi(const char *baslik, char **argv, Eklenti *e) {
    Is *is = g_new0(Is, 1); is->e = e;
    is->dlg = gtk_dialog_new_with_buttons(baslik, GTK_WINDOW(pencere), GTK_DIALOG_MODAL, T("Kapat", "Close"), GTK_RESPONSE_CLOSE, NULL);
    gtk_window_set_default_size(GTK_WINDOW(is->dlg), 560, 320);
    GtkWidget *tv = gtk_text_view_new(); gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE); gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    is->tb = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_container_add(GTK_CONTAINER(sc), tv);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(is->dlg))), sc, TRUE, TRUE, 0);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(is->dlg), GTK_RESPONSE_CLOSE, FALSE);
    g_signal_connect(is->dlg, "response", G_CALLBACK(gtk_widget_destroy), NULL);
    gtk_widget_show_all(is->dlg);
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

/* ---------- Sürücüler ---------- */
static GtkWidget *surucu_liste, *surucu_ozet;

static void surucu_kur_tik(GtkButton *b, gpointer v) {
    (void)v;
    const char *paket = g_object_get_data(G_OBJECT(b), "paket");
    char **p = g_strsplit(paket, " ", -1);
    int n = g_strv_length(p);
    char **argv = g_new0(char *, n + 4);
    argv[0] = "doas"; argv[1] = "/usr/bin/aether-surucu"; argv[2] = "kur";
    for (int i = 0; i < n; i++) argv[3 + i] = p[i];
    gtk_widget_set_sensitive(GTK_WIDGET(b), FALSE);
    komut_penceresi(T("Sürücü kuruluyor", "Installing driver"), argv, NULL);
    g_free(argv); g_strfreev(p);
}

static void surucu_yenile(void) {
    if (!surucu_liste) return;
    GList *c = gtk_container_get_children(GTK_CONTAINER(surucu_liste));
    for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(c);
    char *cikti = NULL;
    g_spawn_command_line_sync("/usr/bin/aether-surucu tara", &cikti, NULL, NULL, NULL);
    char **satir = g_strsplit(cikti ? cikti : "", "\n", -1);
    int toplam = 0, sorun = 0;
    for (int i = 0; satir[i]; i++) {
        char **a = g_strsplit(satir[i], "|", 5);
        if (g_strv_length(a) < 5) { g_strfreev(a); continue; }
        const char *tur = a[0], *ur = a[1], *mod = a[2], *dur = a[3], *paket = a[4];
        const char *ad, *simge;
        if (!strcmp(tur, "gpu")) { ad = T("Ekran kartı", "Graphics card"); simge = "video-display-symbolic"; }
        else if (!strcmp(tur, "wifi")) { ad = T("Kablosuz ağ kartı", "Wireless card"); simge = "network-wireless-symbolic"; }
        else if (!strcmp(tur, "ethernet")) { ad = T("Kablolu ağ kartı", "Ethernet card"); simge = "network-wired-symbolic"; }
        else { ad = T("Ses kartı", "Sound card"); simge = "audio-card-symbolic"; }
        const char *durum;
        if (!strcmp(dur, "tamam")) durum = T("çalışıyor", "working");
        else if (!strcmp(dur, "oneri")) durum = T("çalışıyor, ek firmware önerilir", "working, extra firmware recommended");
        else if (!strcmp(dur, "eksik")) durum = T("firmware gerekli", "firmware needed");
        else if (!strcmp(dur, "yuklenemedi")) durum = T("sürücü başlatılamadı", "driver failed to start");
        else durum = T("sürücü bulunamadı", "no driver found");
        toplam++;
        if (strcmp(dur, "tamam")) sorun++;
        GtkWidget *s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_container_set_border_width(GTK_CONTAINER(s), 10);
        GtkWidget *im = gtk_image_new_from_icon_name(simge, GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_box_pack_start(GTK_BOX(s), im, FALSE, FALSE, 0);
        GtkWidget *y = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        char *m1 = g_markup_printf_escaped("<b>%s</b> · %s", ad, ur);
        GtkWidget *l1 = ae_etiket("", NULL); gtk_label_set_markup(GTK_LABEL(l1), m1); g_free(m1);
        char *m2 = g_strdup_printf("%s%s%s", strcmp(mod, "-") ? mod : "", strcmp(mod, "-") ? " — " : "", durum);
        GtkWidget *l2 = ae_etiket(m2, strcmp(dur, "tamam") ? NULL : "ae-alt"); g_free(m2);
        if (!strcmp(dur, "eksik") || !strcmp(dur, "yok") || !strcmp(dur, "yuklenemedi"))
            gtk_style_context_add_class(gtk_widget_get_style_context(l2), "ae-uyari");
        gtk_box_pack_start(GTK_BOX(y), l1, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(y), l2, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(s), y, TRUE, TRUE, 0);
        if (*paket && (!strcmp(dur, "eksik") || !strcmp(dur, "oneri"))) {
            GtkWidget *b = gtk_button_new_with_label(T("Kur", "Install"));
            gtk_widget_set_valign(b, GTK_ALIGN_CENTER);
            gtk_widget_set_tooltip_text(b, paket);
            g_object_set_data_full(G_OBJECT(b), "paket", g_strdup(paket), g_free);
            g_signal_connect(b, "clicked", G_CALLBACK(surucu_kur_tik), NULL);
            gtk_box_pack_end(GTK_BOX(s), b, FALSE, FALSE, 0);
        }
        gtk_container_add(GTK_CONTAINER(surucu_liste), s);
        g_strfreev(a);
    }
    g_strfreev(satir); g_free(cikti);
    if (!toplam) gtk_container_add(GTK_CONTAINER(surucu_liste), ae_etiket(T("Aygıt bulunamadı.", "No devices found."), "ae-alt"));
    gtk_label_set_text(GTK_LABEL(surucu_ozet), !toplam ? "" : sorun ? T("Bazı aygıtlar için ek sürücü dosyası gerekiyor. Kurmak için internet bağlantısı lazım.",
                                                                       "Some devices need extra driver files. Installing them requires an internet connection.")
                                                              : T("Tüm aygıtların sürücüleri çalışıyor.", "All devices have working drivers."));
    gtk_widget_show_all(surucu_liste);
}

static void surucu_gosterildi(GtkWidget *w, gpointer v) { (void)w; (void)v; surucu_yenile(); }

static GtkWidget *surucu_sayfasi(void) {
    GtkWidget *k = sayfa(T("Sürücüler", "Drivers"));
    surucu_ozet = ae_etiket("", "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), surucu_ozet, FALSE, FALSE, 0);
    surucu_liste = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(surucu_liste), "ae-kart");
    gtk_box_pack_start(GTK_BOX(k), surucu_liste, FALSE, FALSE, 0);
    GtkWidget *y = gtk_button_new_with_label(T("Yeniden tara", "Rescan"));
    gtk_widget_set_halign(y, GTK_ALIGN_START);
    g_signal_connect_swapped(y, "clicked", G_CALLBACK(surucu_yenile), NULL);
    gtk_box_pack_start(GTK_BOX(k), y, FALSE, FALSE, 0);
    g_signal_connect(k, "map", G_CALLBACK(surucu_gosterildi), NULL);
    return k;
}

/* ---------- Ağ ---------- */
static GtkWidget *ag_kablolu, *ag_kablosuz_durum, *ag_liste, *ag_kes_dugme, *ag_mesaj, *ag_tara_dugme;
static int ag_baglaniyor;

static void ag_yenile(void);

static const char *ag_guc_simgesi(int g) {
    return g > 75 ? "network-wireless-signal-excellent-symbolic" : g > 50 ? "network-wireless-signal-good-symbolic" :
           g > 25 ? "network-wireless-signal-ok-symbolic" : "network-wireless-signal-weak-symbolic";
}

static void ag_sonuc(const char *hata, gpointer v) {
    char *ad = v;
    ag_baglaniyor = 0;
    char m[300];
    if (!hata) snprintf(m, sizeof m, T("%s ağına bağlanıldı.", "Connected to %s."), ad);
    else if (!strcmp(hata, "parola")) snprintf(m, sizeof m, T("%s ağına bağlanılamadı. Parola yanlış olabilir.", "Could not connect to %s. The password may be wrong."), ad);
    else snprintf(m, sizeof m, T("%s ağına bağlanılamadı: %s", "Could not connect to %s: %s"), ad, hata);
    gtk_label_set_text(GTK_LABEL(ag_mesaj), m);
    g_free(ad);
    ag_yenile();
}

static char *parola_sor(const char *ad) {
    GtkWidget *d = gtk_dialog_new_with_buttons(T("Wi-Fi parolası", "Wi-Fi password"), GTK_WINDOW(pencere), GTK_DIALOG_MODAL,
                                               T("Vazgeç", "Cancel"), GTK_RESPONSE_CANCEL, T("Bağlan", "Connect"), GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
    GtkWidget *k = gtk_dialog_get_content_area(GTK_DIALOG(d));
    gtk_container_set_border_width(GTK_CONTAINER(k), 16); gtk_box_set_spacing(GTK_BOX(k), 10);
    char m[256]; snprintf(m, sizeof m, T("\"%s\" ağının parolasını yaz:", "Enter the password for \"%s\":"), ad);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(m, NULL), FALSE, FALSE, 0);
    GtkWidget *e = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(e), FALSE);
    gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_PASSWORD);
    gtk_entry_set_activates_default(GTK_ENTRY(e), TRUE);
    gtk_widget_set_size_request(e, 300, -1);
    gtk_box_pack_start(GTK_BOX(k), e, FALSE, FALSE, 0);
    GtkWidget *g = gtk_check_button_new_with_label(T("Parolayı göster", "Show password"));
    g_object_bind_property(g, "active", e, "visibility", G_BINDING_DEFAULT);
    gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
    gtk_widget_show_all(d);
    char *sonuc = NULL;
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) sonuc = g_strdup(gtk_entry_get_text(GTK_ENTRY(e)));
    gtk_widget_destroy(d);
    return sonuc;
}

static void ag_satir_tik(GtkListBox *l, GtkListBoxRow *r, gpointer v) {
    (void)l; (void)v;
    if (ag_baglaniyor || !r) return;
    const char *yol = g_object_get_data(G_OBJECT(r), "yol");
    const char *ad = g_object_get_data(G_OBJECT(r), "ad");
    const char *tur = g_object_get_data(G_OBJECT(r), "tur");
    int bilinen = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(r), "bilinen"));
    int bagli = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(r), "bagli"));
    if (bagli) return;
    char *parola = NULL;
    if (!bilinen && strcmp(tur, "open")) {
        if (!strcmp(tur, "8021x")) { gtk_label_set_text(GTK_LABEL(ag_mesaj), T("Kurumsal (802.1X) ağlar henüz desteklenmiyor.", "Enterprise (802.1X) networks are not supported yet.")); return; }
        parola = parola_sor(ad);
        if (!parola) return;
        if (strlen(parola) < 8) { gtk_label_set_text(GTK_LABEL(ag_mesaj), T("Wi-Fi parolası en az 8 karakter olmalı.", "A Wi-Fi password must be at least 8 characters.")); g_free(parola); return; }
    }
    ag_baglaniyor = 1;
    char m[256]; snprintf(m, sizeof m, T("%s ağına bağlanılıyor…", "Connecting to %s…"), ad);
    gtk_label_set_text(GTK_LABEL(ag_mesaj), m);
    ag_baglan(yol, parola, ag_sonuc, g_strdup(ad));
    g_free(parola);
}

static void ag_unut_tik(GtkButton *b, gpointer v) {
    (void)v;
    ag_unut(g_object_get_data(G_OBJECT(b), "yol"));
    gtk_label_set_text(GTK_LABEL(ag_mesaj), T("Ağ unutuldu.", "Network forgotten."));
    ag_yenile();
}

static void ag_kes_tik(GtkButton *b, gpointer v) { (void)b; (void)v; ag_kes(); ag_yenile(); }
static void ag_tara_tik(GtkButton *b, gpointer v) {
    (void)b; (void)v; ag_tara();
    gtk_label_set_text(GTK_LABEL(ag_mesaj), T("Ağlar taranıyor…", "Scanning for networks…"));
}

static void ag_yenile(void) {
    /* kablolu */
    AgArayuz a[8]; int n = ag_arayuzler(a, 8), kablosuz_kart = 0;
    GString *k = g_string_new(NULL);
    for (int i = 0; i < n; i++) {
        if (a[i].kablosuz) { kablosuz_kart = 1; continue; }
        if (k->len) g_string_append(k, "\n");
        if (a[i].bagli) g_string_append_printf(k, T("%s — bağlı%s%s", "%s — connected%s%s"), a[i].arayuz, a[i].ip[0] ? " · " : "", a[i].ip);
        else g_string_append_printf(k, T("%s — kablo takılı değil", "%s — cable unplugged"), a[i].arayuz);
    }
    gtk_label_set_text(GTK_LABEL(ag_kablolu), k->len ? k->str : T("Kablolu ağ kartı yok.", "No wired network card."));
    g_string_free(k, TRUE);

    /* kablosuz */
    GList *c = gtk_container_get_children(GTK_CONTAINER(ag_liste));
    for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(c);
    gtk_widget_hide(ag_kes_dugme);
    gtk_widget_hide(ag_liste);
    char ist[256], durum[32] = "";
    if (!kablosuz_kart) {
        gtk_label_set_text(GTK_LABEL(ag_kablosuz_durum), T("Kablosuz ağ kartı bulunamadı.", "No wireless network card found."));
        gtk_widget_set_sensitive(ag_tara_dugme, FALSE);
        return;
    }
    if (!ag_istasyon(ist, sizeof ist, durum, sizeof durum)) {
        gtk_label_set_text(GTK_LABEL(ag_kablosuz_durum), T("Kablosuz ağ hizmeti (iwd) çalışmıyor.", "The wireless service (iwd) is not running."));
        gtk_widget_set_sensitive(ag_tara_dugme, FALSE);
        return;
    }
    gtk_widget_set_sensitive(ag_tara_dugme, TRUE);
    AgKablosuz l[64]; int m = ag_aglar(l, 64);
    const char *bagli = NULL;
    for (int i = 0; i < m; i++) {
        if (l[i].bagli) bagli = l[i].ad;
        GtkWidget *r = gtk_list_box_row_new();
        GtkWidget *s = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(s), 8);
        gtk_box_pack_start(GTK_BOX(s), gtk_image_new_from_icon_name(ag_guc_simgesi(l[i].guc), GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
        GtkWidget *ad = gtk_label_new(NULL);
        char *esc = g_markup_escape_text(l[i].ad, -1);
        char *mk = l[i].bagli ? g_strdup_printf("<b>%s</b>", esc) : g_strdup(esc);
        gtk_label_set_markup(GTK_LABEL(ad), mk); g_free(mk); g_free(esc);
        gtk_label_set_xalign(GTK_LABEL(ad), 0);
        gtk_box_pack_start(GTK_BOX(s), ad, TRUE, TRUE, 0);
        if (l[i].bilinen) {
            GtkWidget *u = gtk_button_new_with_label(T("Unut", "Forget"));
            gtk_button_set_relief(GTK_BUTTON(u), GTK_RELIEF_NONE);
            g_object_set_data_full(G_OBJECT(u), "yol", g_strdup(l[i].yol), g_free);
            g_signal_connect(u, "clicked", G_CALLBACK(ag_unut_tik), NULL);
            gtk_box_pack_end(GTK_BOX(s), u, FALSE, FALSE, 0);
        }
        const char *etiket = l[i].bagli ? T("Bağlı", "Connected") : l[i].bilinen ? T("Kayıtlı", "Saved") : NULL;
        if (etiket) gtk_box_pack_end(GTK_BOX(s), ae_etiket(etiket, "ae-alt"), FALSE, FALSE, 0);
        if (strcmp(l[i].tur, "open")) gtk_box_pack_end(GTK_BOX(s), gtk_image_new_from_icon_name("channel-secure-symbolic", GTK_ICON_SIZE_MENU), FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(r), s);
        g_object_set_data_full(G_OBJECT(r), "yol", g_strdup(l[i].yol), g_free);
        g_object_set_data_full(G_OBJECT(r), "ad", g_strdup(l[i].ad), g_free);
        g_object_set_data_full(G_OBJECT(r), "tur", g_strdup(l[i].tur), g_free);
        g_object_set_data(G_OBJECT(r), "bilinen", GINT_TO_POINTER(l[i].bilinen));
        g_object_set_data(G_OBJECT(r), "bagli", GINT_TO_POINTER(l[i].bagli));
        gtk_container_add(GTK_CONTAINER(ag_liste), r);
    }
    if (m) gtk_widget_show_all(ag_liste);
    char mesaj[200];
    if (bagli) { snprintf(mesaj, sizeof mesaj, T("Bağlı: %s", "Connected: %s"), bagli); gtk_widget_show(ag_kes_dugme); }
    else if (!strcmp(durum, "connecting")) snprintf(mesaj, sizeof mesaj, "%s", T("Bağlanılıyor…", "Connecting…"));
    else snprintf(mesaj, sizeof mesaj, "%s", m ? T("Bağlanmak için bir ağa tıkla.", "Click a network to connect.") : T("Yakında ağ bulunamadı. Taramayı dene.", "No networks found nearby. Try scanning."));
    gtk_label_set_text(GTK_LABEL(ag_kablosuz_durum), mesaj);
}

static gboolean ag_zamanlayici(gpointer v) {
    GtkWidget *k = v;
    if (gtk_widget_get_mapped(k) && !ag_baglaniyor) ag_yenile();
    return G_SOURCE_CONTINUE;
}

static void ag_gosterildi(GtkWidget *w, gpointer v) { (void)w; (void)v; ag_tara(); ag_yenile(); }

static GtkWidget *ag_sayfasi(void) {
    GtkWidget *k = sayfa(T("Ağ", "Network"));
    GtkWidget *b1 = ae_etiket("", NULL); gtk_label_set_markup(GTK_LABEL(b1), T("<b>Kablolu</b>", "<b>Wired</b>"));
    gtk_box_pack_start(GTK_BOX(k), b1, FALSE, FALSE, 0);
    ag_kablolu = ae_etiket("", "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), ag_kablolu, FALSE, FALSE, 0);
    GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *b2 = ae_etiket("", NULL); gtk_label_set_markup(GTK_LABEL(b2), T("<b>Kablosuz (Wi-Fi)</b>", "<b>Wireless (Wi-Fi)</b>"));
    gtk_box_pack_start(GTK_BOX(ust), b2, TRUE, TRUE, 0);
    ag_tara_dugme = gtk_button_new_with_label(T("Tara", "Scan"));
    g_signal_connect(ag_tara_dugme, "clicked", G_CALLBACK(ag_tara_tik), NULL);
    ag_kes_dugme = gtk_button_new_with_label(T("Bağlantıyı kes", "Disconnect"));
    g_signal_connect(ag_kes_dugme, "clicked", G_CALLBACK(ag_kes_tik), NULL);
    gtk_box_pack_end(GTK_BOX(ust), ag_tara_dugme, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(ust), ag_kes_dugme, FALSE, FALSE, 0);
    gtk_widget_set_margin_top(ust, 10);
    gtk_box_pack_start(GTK_BOX(k), ust, FALSE, FALSE, 0);
    ag_kablosuz_durum = ae_etiket("", "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), ag_kablosuz_durum, FALSE, FALSE, 0);
    ag_liste = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(ag_liste), GTK_SELECTION_NONE);
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(ag_liste), TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(ag_liste), "ae-kart");
    g_signal_connect(ag_liste, "row-activated", G_CALLBACK(ag_satir_tik), NULL);
    gtk_box_pack_start(GTK_BOX(k), ag_liste, FALSE, FALSE, 0);
    ag_mesaj = ae_etiket("", NULL);
    gtk_box_pack_start(GTK_BOX(k), ag_mesaj, FALSE, FALSE, 0);
    g_signal_connect(k, "map", G_CALLBACK(ag_gosterildi), NULL);
    g_timeout_add_seconds(5, ag_zamanlayici, k);
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
        {gorunum, "gorunum", "Görünüm", "Appearance"}, {ag_sayfasi, "ag", "Ağ", "Network"}, {dil, "dil", "Dil ve Klavye", "Language & Keyboard"},
        {ekran, "ekran", "Ekran", "Display"}, {kilit, "kilit", "Kilit ve Koruyucu", "Lock & Screensaver"},
        {saat, "saat", "Tarih ve Saat", "Date & Time"}, {surucu_sayfasi, "surucu", "Sürücüler", "Drivers"}, {eklenti_sayfasi, "eklenti", "Eklentiler", "Add-ons"}, {NULL}};
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
    for (int i = 1; i < argc; i++) {
        const char *ad = argv[i];
        if (!strcmp(ad, "--sayfa") && i + 1 < argc) ad = argv[++i];
        if (ad[0] != '-') gtk_stack_set_visible_child_name(GTK_STACK(yigin), ad);
    }
    gtk_main();
    return 0;
}
