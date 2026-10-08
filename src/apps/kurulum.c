/* aether-kurulum — Aether kurulum sihirbazı
   Dil → Klavye → Hesap → Disk → Özet → Kurulum → Bitti
   Asıl işi root olarak /usr/libexec/aether/kur.sh yapar; şifre stdin üzerinden verilir. */
#include "aether.h"
#include <ctype.h>
#include <sys/stat.h>

static GtkWidget *pencere, *yigin, *geri, *ileri, *adim_etiketi;
static int adim = 0;
static const char *dil = "tr", *klavye = "tr";
static GtkWidget *e_ad, *e_kul, *e_bil, *e_s1, *e_s2, *hesap_uyari;
static GtkWidget *disk_liste, *disk_onay, *ozet_etiketi, *ilerleme, *durum, *gunluk_tb_w;
static char *secili_disk;
static gboolean kul_elle = FALSE;

enum { S_DIL, S_KLAVYE, S_HESAP, S_DISK, S_OZET, S_KUR, S_BITTI, S_SAYI };
static GtkWidget *sayfalar[S_SAYI];
static void sayfalari_kur(void);

static GtkWidget *sayfa(const char *baslik, const char *aciklama) {
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(k), 28);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(baslik, "ae-baslik"), FALSE, FALSE, 0);
    if (aciklama) gtk_box_pack_start(GTK_BOX(k), ae_etiket(aciklama, "ae-alt"), FALSE, FALSE, 0);
    return k;
}
static void dugmeleri_guncelle(void) {
    static const char *tr[] = {"Dil", "Klavye", "Hesap", "Disk", "Özet", "Kurulum", "Bitti"};
    static const char *en[] = {"Language", "Keyboard", "Account", "Disk", "Summary", "Installing", "Done"};
    GString *s = g_string_new(NULL);
    for (int i = 0; i < S_SAYI; i++) {
        if (i) g_string_append(s, "  ›  ");
        if (i == adim) g_string_append_printf(s, "<b>%s</b>", ae_en() ? en[i] : tr[i]);
        else g_string_append(s, ae_en() ? en[i] : tr[i]);
    }
    gtk_label_set_markup(GTK_LABEL(adim_etiketi), s->str); g_string_free(s, TRUE);
    gtk_widget_set_visible(geri, adim > 0 && adim < S_KUR);
    gtk_widget_set_visible(ileri, adim < S_KUR);
    gtk_button_set_label(GTK_BUTTON(geri), T("Geri", "Back"));
    gtk_button_set_label(GTK_BUTTON(ileri), adim == S_OZET ? T("Kur", "Install") : T("İleri", "Next"));
    GtkStyleContext *sc = gtk_widget_get_style_context(ileri);
    if (adim == S_OZET) gtk_style_context_add_class(sc, "destructive-action"); else gtk_style_context_remove_class(sc, "destructive-action");
}

/* ---------- Dil ---------- */
static void dil_sec(GtkToggleButton *b, gpointer d) {
    if (!gtk_toggle_button_get_active(b)) return;
    dil = d; ae_dil_zorla = !strcmp(dil, "en");
}
static GtkWidget *dil_sayfasi(void) {
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(k), 28);
    GtkWidget *ust = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_box_pack_start(GTK_BOX(ust), ae_logo(80), FALSE, FALSE, 0);
    GtkWidget *b = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_box_pack_start(GTK_BOX(b), ae_etiket("A E T H E R", "ae-baslik"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(b), ae_etiket("1.1 · Nebula — Kurulum / Setup", "ae-alt"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ust), b, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), ust, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket("Dilini seç  ·  Choose your language", NULL), FALSE, FALSE, 8);
    GtkWidget *r1 = gtk_radio_button_new_with_label(NULL, "Türkçe");
    GtkWidget *r2 = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(r1), "English");
    gtk_style_context_add_class(gtk_widget_get_style_context(r1), "ae-buyuk");
    gtk_style_context_add_class(gtk_widget_get_style_context(r2), "ae-buyuk");
    if (ae_en()) { gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r2), TRUE); dil = "en"; }
    g_signal_connect(r1, "toggled", G_CALLBACK(dil_sec), "tr");
    g_signal_connect(r2, "toggled", G_CALLBACK(dil_sec), "en");
    gtk_box_pack_start(GTK_BOX(k), r1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), r2, FALSE, FALSE, 0);
    return k;
}

/* ---------- Klavye ---------- */
static void klavye_sec(GtkToggleButton *b, gpointer d) {
    if (!gtk_toggle_button_get_active(b)) return;
    klavye = d;
    char *kmt = g_strdup_printf("setxkbmap %s", strcmp(klavye, "trf") ? klavye : "tr -variant f");
    g_spawn_command_line_async(kmt, NULL); g_free(kmt);
}
static GtkWidget *klavye_sayfasi(void) {
    GtkWidget *k = sayfa(T("Klavye", "Keyboard"), T("Klavye düzenini seç ve aşağıda dene.", "Pick your keyboard layout and test it below."));
    const char *id[] = {"tr", "trf", "us"};
    const char *ad[] = {T("Türkçe Q", "Turkish Q"), T("Türkçe F", "Turkish F"), "English (US)"};
    GtkWidget *ilk = NULL;
    for (int i = 0; i < 3; i++) {
        GtkWidget *r = ilk ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ilk), ad[i]) : gtk_radio_button_new_with_label(NULL, ad[i]);
        if (!ilk) ilk = r;
        if (!strcmp(klavye, id[i])) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(r), TRUE);
        g_signal_connect(r, "toggled", G_CALLBACK(klavye_sec), (gpointer)id[i]);
        gtk_box_pack_start(GTK_BOX(k), r, FALSE, FALSE, 0);
    }
    GtkWidget *e = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(e), T("Burada dene: ğüşıöç ĞÜŞİÖÇ", "Test here"));
    gtk_box_pack_start(GTK_BOX(k), e, FALSE, FALSE, 8);
    return k;
}

/* ---------- Hesap ---------- */
static void ad_degisti(GtkEditable *e, gpointer d) {
    if (kul_elle) return;
    const char *ad = gtk_entry_get_text(GTK_ENTRY(e_ad));
    GString *s = g_string_new(NULL);
    /* Türkçe karakterleri sadeleştir, ilk kelimeyi kullan */
    const char *p = ad;
    while (*p && *p != ' ' && s->len < 20) {
        gunichar c = g_utf8_get_char(p);
        switch (c) {
            case 0x11F: case 0x11E: g_string_append_c(s, 'g'); break;
            case 0xFC: case 0xDC: g_string_append_c(s, 'u'); break;
            case 0x15F: case 0x15E: g_string_append_c(s, 's'); break;
            case 0x131: case 0x130: g_string_append_c(s, 'i'); break;
            case 0xF6: case 0xD6: g_string_append_c(s, 'o'); break;
            case 0xE7: case 0xC7: g_string_append_c(s, 'c'); break;
            default: if (c < 128 && isalnum((int)c)) g_string_append_c(s, tolower((int)c));
        }
        p = g_utf8_next_char(p);
    }
    g_signal_handlers_block_by_func(e_kul, d, NULL);
    gtk_entry_set_text(GTK_ENTRY(e_kul), s->str);
    g_signal_handlers_unblock_by_func(e_kul, d, NULL);
    g_string_free(s, TRUE);
}
static void kul_degisti(GtkEditable *e, gpointer d) { kul_elle = TRUE; }
static GtkWidget *alan(GtkWidget *g, int i, const char *ad, gboolean gizli) {
    GtkWidget *l = gtk_label_new(ad); gtk_label_set_xalign(GTK_LABEL(l), 1);
    GtkWidget *e = gtk_entry_new(); gtk_widget_set_hexpand(e, TRUE);
    if (gizli) { gtk_entry_set_visibility(GTK_ENTRY(e), FALSE); gtk_entry_set_input_purpose(GTK_ENTRY(e), GTK_INPUT_PURPOSE_PASSWORD); }
    gtk_grid_attach(GTK_GRID(g), l, 0, i, 1, 1); gtk_grid_attach(GTK_GRID(g), e, 1, i, 1, 1);
    return e;
}
static GtkWidget *hesap_sayfasi(void) {
    GtkWidget *k = sayfa(T("Hesabın", "Your account"), T("Aether'e giriş yaparken bu kullanıcı adını ve şifreyi kullanacaksın.", "You will use this username and password to log in to Aether."));
    GtkWidget *g = gtk_grid_new(); gtk_grid_set_row_spacing(GTK_GRID(g), 8); gtk_grid_set_column_spacing(GTK_GRID(g), 12);
    e_ad = alan(g, 0, T("Adın", "Your name"), FALSE);
    e_kul = alan(g, 1, T("Kullanıcı adı", "Username"), FALSE);
    e_bil = alan(g, 2, T("Bilgisayar adı", "Computer name"), FALSE);
    e_s1 = alan(g, 3, T("Şifre", "Password"), TRUE);
    e_s2 = alan(g, 4, T("Şifre (tekrar)", "Password (again)"), TRUE);
    gtk_entry_set_text(GTK_ENTRY(e_bil), "aether");
    g_signal_connect(e_ad, "changed", G_CALLBACK(ad_degisti), kul_degisti);
    g_signal_connect(e_kul, "changed", G_CALLBACK(kul_degisti), NULL);
    gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
    hesap_uyari = ae_etiket("", "ae-uyari");
    gtk_box_pack_start(GTK_BOX(k), hesap_uyari, FALSE, FALSE, 0);
    return k;
}
static gboolean gecerli_ad(const char *s, int bas_rakam_olabilir) {
    if (!*s || strlen(s) > 32) return FALSE;
    if (!bas_rakam_olabilir && !islower((unsigned char)s[0]) && s[0] != '_') return FALSE;
    for (const char *p = s; *p; p++) if (!islower((unsigned char)*p) && !isdigit((unsigned char)*p) && *p != '-' && *p != '_') return FALSE;
    return TRUE;
}
static gboolean hesap_kontrol(void) {
    const char *kul = gtk_entry_get_text(GTK_ENTRY(e_kul)), *bil = gtk_entry_get_text(GTK_ENTRY(e_bil));
    const char *s1 = gtk_entry_get_text(GTK_ENTRY(e_s1)), *s2 = gtk_entry_get_text(GTK_ENTRY(e_s2));
    const char *yasak[] = {"root", "canli", "bin", "daemon", "adm", "lp", "sync", "shutdown", "halt", "mail", "news", "operator", "man", "nobody", "lightdm", "messagebus", NULL};
    const char *hata = NULL;
    if (!*gtk_entry_get_text(GTK_ENTRY(e_ad))) hata = T("Adını yaz.", "Enter your name.");
    else if (!gecerli_ad(kul, 0)) hata = T("Kullanıcı adı küçük harfle başlamalı; yalnızca a-z, 0-9, - ve _ içerebilir.", "Username must start with a lowercase letter and use only a-z, 0-9, - and _.");
    else if (!gecerli_ad(bil, 1)) hata = T("Bilgisayar adı yalnızca küçük harf, rakam ve - içerebilir.", "Computer name may only contain lowercase letters, digits and -.");
    else if (strlen(s1) < 4) hata = T("Şifre en az 4 karakter olmalı.", "Password must be at least 4 characters.");
    else if (strcmp(s1, s2)) hata = T("Şifreler aynı değil.", "Passwords do not match.");
    for (int i = 0; !hata && yasak[i]; i++) if (!strcmp(kul, yasak[i])) hata = T("Bu kullanıcı adı sisteme ayrılmış, başka bir ad seç.", "This username is reserved, pick another.");
    gtk_label_set_text(GTK_LABEL(hesap_uyari), hata ? hata : "");
    return hata == NULL;
}

/* ---------- Disk ---------- */
static char *oku(const char *yol) { char *c = NULL; if (g_file_get_contents(yol, &c, NULL, NULL)) return g_strstrip(c); return g_strdup(""); }
static char *kaynak_disk(void) {   /* canlı ortamın açıldığı disk kurulum listesinde gösterilmez */
    char *k = oku("/run/aether/kaynak-disk"); return k;
}
static void disk_secildi(GtkToggleButton *b, gpointer d) {
    if (gtk_toggle_button_get_active(b)) { g_free(secili_disk); secili_disk = g_strdup(d); }
}
static void diskleri_doldur(void) {
    GList *c = gtk_container_get_children(GTK_CONTAINER(disk_liste));
    for (GList *l = c; l; l = l->next) gtk_widget_destroy(l->data);
    g_list_free(c);
    g_free(secili_disk); secili_disk = NULL;
    char *kaynak = kaynak_disk();
    GDir *d = g_dir_open("/sys/block", 0, NULL); const char *n; GtkWidget *ilk = NULL;
    while (d && (n = g_dir_read_name(d))) {
        if (g_str_has_prefix(n, "loop") || g_str_has_prefix(n, "ram") || g_str_has_prefix(n, "sr") || g_str_has_prefix(n, "zram") || g_str_has_prefix(n, "fd")) continue;
        if (*kaynak && !strcmp(n, kaynak)) continue;
        char yol[256]; snprintf(yol, sizeof yol, "/sys/block/%s/size", n);
        char *s = oku(yol); double gb = g_ascii_strtod(s, NULL) * 512 / 1e9; g_free(s);
        if (gb < 4) continue;
        snprintf(yol, sizeof yol, "/sys/block/%s/device/model", n);
        char *m = oku(yol);
        char et[256]; snprintf(et, sizeof et, "/dev/%s  —  %.1f GB  %s", n, gb, *m ? m : T("Disk", "Disk")); g_free(m);
        GtkWidget *r = ilk ? gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(ilk), et) : gtk_radio_button_new_with_label(NULL, et);
        char *ad = g_strdup_printf("/dev/%s", n);
        g_object_set_data_full(G_OBJECT(r), "ad", ad, g_free);
        g_signal_connect(r, "toggled", G_CALLBACK(disk_secildi), ad);
        if (!ilk) { ilk = r; secili_disk = g_strdup(ad); }
        gtk_box_pack_start(GTK_BOX(disk_liste), r, FALSE, FALSE, 0);
    }
    if (d) g_dir_close(d);
    g_free(kaynak);
    if (!ilk) gtk_box_pack_start(GTK_BOX(disk_liste), ae_etiket(T("Uygun disk bulunamadı (en az 4 GB gerekli). Sanal makineye bir sanal disk ekle.",
        "No suitable disk found (at least 4 GB needed). Add a virtual disk to the VM."), "ae-uyari"), FALSE, FALSE, 0);
    gtk_widget_show_all(disk_liste);
}
static GtkWidget *disk_sayfasi(void) {
    GtkWidget *k = sayfa(T("Disk", "Disk"), T("Aether'in kurulacağı diski seç. Disk tamamen Aether'e ayrılacak.", "Choose the disk to install Aether on. The whole disk will be used."));
    disk_liste = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(k), disk_liste, FALSE, FALSE, 0);
    GtkWidget *yen = gtk_button_new_with_label(T("Listeyi yenile", "Refresh list"));
    gtk_widget_set_halign(yen, GTK_ALIGN_START);
    g_signal_connect(yen, "clicked", G_CALLBACK(diskleri_doldur), NULL);
    gtk_box_pack_start(GTK_BOX(k), yen, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("⚠ Seçilen diskteki TÜM veriler silinecek.", "⚠ ALL data on the selected disk will be erased."), "ae-uyari"), FALSE, FALSE, 8);
    disk_onay = gtk_check_button_new_with_label(T("Anladım, bu diskin silinmesini onaylıyorum", "I understand, erase this disk"));
    gtk_box_pack_start(GTK_BOX(k), disk_onay, FALSE, FALSE, 0);
    diskleri_doldur();
    return k;
}

/* ---------- Özet ---------- */
static void ozeti_yaz(void) {
    const char *ka = !strcmp(klavye, "tr") ? T("Türkçe Q", "Turkish Q") : !strcmp(klavye, "trf") ? T("Türkçe F", "Turkish F") : "English (US)";
    char *m = g_markup_printf_escaped(
        "<b>%s</b>  %s\n<b>%s</b>  %s\n<b>%s</b>  %s (%s)\n<b>%s</b>  %s\n<b>%s</b>  %s\n\n%s",
        T("Dil:", "Language:"), !strcmp(dil, "en") ? "English" : "Türkçe",
        T("Klavye:", "Keyboard:"), ka,
        T("Kullanıcı:", "User:"), gtk_entry_get_text(GTK_ENTRY(e_ad)), gtk_entry_get_text(GTK_ENTRY(e_kul)),
        T("Bilgisayar adı:", "Computer name:"), gtk_entry_get_text(GTK_ENTRY(e_bil)),
        T("Disk:", "Disk:"), secili_disk ? secili_disk : "-",
        T("\"Kur\"a bastığında disk silinecek ve Aether kurulacak. Bu birkaç dakika sürer.", "When you press \"Install\", the disk will be erased and Aether installed. This takes a few minutes."));
    gtk_label_set_markup(GTK_LABEL(ozet_etiketi), m); g_free(m);
}
static GtkWidget *ozet_sayfasi(void) {
    GtkWidget *k = sayfa(T("Özet", "Summary"), NULL);
    ozet_etiketi = ae_etiket("", NULL);
    gtk_box_pack_start(GTK_BOX(k), ozet_etiketi, FALSE, FALSE, 0);
    return k;
}

/* ---------- Kurulum ---------- */
static GtkTextBuffer *gunluk;
static void gunluge(const char *s) { GtkTextIter it; gtk_text_buffer_get_end_iter(gunluk, &it); gtk_text_buffer_insert(gunluk, &it, s, -1); }
static gboolean satir_geldi(GIOChannel *ch, GIOCondition c, gpointer d) {
    char *s = NULL; gsize n;
    if (!(c & G_IO_IN)) return FALSE;
    GIOStatus st = g_io_channel_read_line(ch, &s, &n, NULL, NULL);
    if (st != G_IO_STATUS_NORMAL) return st == G_IO_STATUS_AGAIN;
    if (!strncmp(s, "@@ ", 3)) {        /* "@@ yüzde mesaj_tr|mesaj_en" */
        int yuzde = atoi(s + 3); char *m = strchr(s + 3, ' ');
        if (m) { m = g_strstrip(m); char *ayrac = strchr(m, '|'); if (ayrac) { *ayrac = 0; if (ae_en()) m = ayrac + 1; } gtk_label_set_text(GTK_LABEL(durum), m); }
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ilerleme), yuzde / 100.0);
    } else gunluge(s);
    g_free(s); return TRUE;
}
static void kurulum_bitti(GPid pid, gint st, gpointer d) {
    g_spawn_close_pid(pid);
    if (g_spawn_check_exit_status(st, NULL)) { adim = S_BITTI; gtk_stack_set_visible_child(GTK_STACK(yigin), sayfalar[S_BITTI]); dugmeleri_guncelle(); }
    else {
        gtk_label_set_text(GTK_LABEL(durum), T("Kurulum başarısız oldu. Ayrıntılar aşağıda.", "Installation failed. See details below."));
        gtk_style_context_add_class(gtk_widget_get_style_context(durum), "ae-uyari");
        gtk_widget_set_visible(gunluk_tb_w, TRUE);
    }
}
static void kurulumu_baslat(void) {
    const char *argv[] = {"doas", "/usr/libexec/aether/kur.sh", secili_disk, gtk_entry_get_text(GTK_ENTRY(e_kul)),
        gtk_entry_get_text(GTK_ENTRY(e_ad)), gtk_entry_get_text(GTK_ENTRY(e_bil)), dil, klavye, NULL};
    GPid pid; int in, out; GError *e = NULL;
    if (!g_spawn_async_with_pipes(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL,
                                  NULL, NULL, &pid, &in, &out, NULL, &e)) {
        gunluge(e->message); g_error_free(e); return;
    }
    const char *s = gtk_entry_get_text(GTK_ENTRY(e_s1));
    if (write(in, s, strlen(s)) < 0 || write(in, "\n", 1) < 0) gunluge("stdin\n");
    close(in);
    GIOChannel *ch = g_io_channel_unix_new(out);
    g_io_add_watch(ch, G_IO_IN | G_IO_HUP, satir_geldi, NULL);
    g_child_watch_add(pid, kurulum_bitti, NULL);
}
static GtkWidget *kur_sayfasi(void) {
    GtkWidget *k = sayfa(T("Aether kuruluyor", "Installing Aether"), T("Arkana yaslan; bu birkaç dakika sürecek.", "Sit back; this will take a few minutes."));
    ilerleme = gtk_progress_bar_new();
    durum = ae_etiket(T("Hazırlanıyor…", "Preparing…"), NULL);
    gtk_box_pack_start(GTK_BOX(k), ilerleme, FALSE, FALSE, 8);
    gtk_box_pack_start(GTK_BOX(k), durum, FALSE, FALSE, 0);
    GtkWidget *exp = gtk_expander_new(T("Ayrıntılar", "Details"));
    GtkWidget *tv = gtk_text_view_new(); gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE); gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gunluk = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));
    GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(sc, -1, 180); gtk_container_add(GTK_CONTAINER(sc), tv);
    gtk_container_add(GTK_CONTAINER(exp), sc);
    gunluk_tb_w = exp;
    gtk_box_pack_start(GTK_BOX(k), exp, TRUE, TRUE, 0);
    return k;
}

/* ---------- Bitti ---------- */
static void yeniden_baslat(GtkButton *b, gpointer d) { ae_calistir("doas /sbin/reboot"); }
static GtkWidget *bitti_sayfasi(void) {
    GtkWidget *k = sayfa(T("Aether kuruldu!", "Aether is installed!"), NULL);
    gtk_box_pack_start(GTK_BOX(k), ae_logo(96), FALSE, FALSE, 12);
    gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Bilgisayarı yeniden başlat. Sanal makinenin ayarlarından ISO dosyasını çıkarmayı (ya da önyükleme sırasını diske almayı) unutma.",
        "Restart the computer. Don't forget to remove the ISO from the virtual machine (or set it to boot from disk)."), NULL), FALSE, FALSE, 0);
    GtkWidget *a = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *kal = gtk_button_new_with_label(T("Canlı oturumda kal", "Stay in live session"));
    GtkWidget *yb = gtk_button_new_with_label(T("Yeniden Başlat", "Restart Now"));
    gtk_style_context_add_class(gtk_widget_get_style_context(yb), "suggested-action");
    g_signal_connect(kal, "clicked", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(yb, "clicked", G_CALLBACK(yeniden_baslat), NULL);
    gtk_box_pack_end(GTK_BOX(a), yb, FALSE, FALSE, 0); gtk_box_pack_end(GTK_BOX(a), kal, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(k), a, FALSE, FALSE, 0);
    return k;
}

/* ---------- Gezinme ---------- */
static void git(int yeni) {
    adim = yeni;
    gtk_stack_set_visible_child(GTK_STACK(yigin), sayfalar[adim]);
    dugmeleri_guncelle();
}
static void ileri_tik(GtkButton *b, gpointer d) {
    switch (adim) {
    case S_DIL: sayfalari_kur(); git(S_KLAVYE); break;   /* dile göre sayfaları yeniden kur */
    case S_KLAVYE: git(S_HESAP); break;
    case S_HESAP: if (hesap_kontrol()) git(S_DISK); break;
    case S_DISK:
        if (!secili_disk) return;
        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(disk_onay))) {
            GtkWidget *m = gtk_message_dialog_new(GTK_WINDOW(pencere), GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s",
                T("Devam etmek için disk silme onay kutusunu işaretle.", "Tick the erase confirmation box to continue."));
            gtk_dialog_run(GTK_DIALOG(m)); gtk_widget_destroy(m); return;
        }
        ozeti_yaz(); git(S_OZET); break;
    case S_OZET: git(S_KUR); kurulumu_baslat(); break;
    }
}
static void geri_tik(GtkButton *b, gpointer d) { if (adim > 0) git(adim - 1); }

static void sayfalari_kur(void) {
    for (int i = S_KLAVYE; i < S_SAYI; i++) if (sayfalar[i]) gtk_widget_destroy(sayfalar[i]);
    GtkWidget *(*f[])(void) = {NULL, klavye_sayfasi, hesap_sayfasi, disk_sayfasi, ozet_sayfasi, kur_sayfasi, bitti_sayfasi};
    for (int i = S_KLAVYE; i < S_SAYI; i++) { sayfalar[i] = f[i](); gtk_stack_add_named(GTK_STACK(yigin), sayfalar[i], g_strdup_printf("s%d", i)); gtk_widget_show_all(sayfalar[i]); }
    gtk_window_set_title(GTK_WINDOW(pencere), T("Aether Kurulumu", "Aether Setup"));
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv); ae_css();
    if (geteuid() != 0 && !ae_canli()) {
        GtkWidget *m = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s",
            T("Aether zaten kurulu. Kurulum yalnızca canlı (ISO) oturumda çalışır.", "Aether is already installed. Setup only runs in the live (ISO) session."));
        gtk_dialog_run(GTK_DIALOG(m)); return 0;
    }
    pencere = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(pencere), T("Aether Kurulumu", "Aether Setup"));
    gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-kurulum");
    gtk_window_set_default_size(GTK_WINDOW(pencere), 720, 520);
    gtk_window_set_position(GTK_WINDOW(pencere), GTK_WIN_POS_CENTER);
    g_signal_connect(pencere, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    adim_etiketi = gtk_label_new(""); gtk_widget_set_margin_top(adim_etiketi, 10); gtk_widget_set_margin_bottom(adim_etiketi, 4);
    gtk_style_context_add_class(gtk_widget_get_style_context(adim_etiketi), "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), adim_etiketi, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    yigin = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(yigin), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    sayfalar[S_DIL] = dil_sayfasi();
    gtk_stack_add_named(GTK_STACK(yigin), sayfalar[S_DIL], "s0");
    gtk_box_pack_start(GTK_BOX(k), yigin, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(k), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    GtkWidget *alt = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(alt), 12);
    geri = gtk_button_new_with_label(""); ileri = gtk_button_new_with_label("");
    gtk_style_context_add_class(gtk_widget_get_style_context(ileri), "suggested-action");
    g_signal_connect(geri, "clicked", G_CALLBACK(geri_tik), NULL);
    g_signal_connect(ileri, "clicked", G_CALLBACK(ileri_tik), NULL);
    gtk_box_pack_end(GTK_BOX(alt), ileri, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(alt), geri, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(k), alt, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(pencere), k);
    gtk_widget_show_all(pencere);
    dugmeleri_guncelle();
    gtk_main();
    return 0;
}
