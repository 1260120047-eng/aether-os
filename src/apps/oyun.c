/* aether-oyun-modu — Aether Oyun Modu
 *
 * Tek tıkla Steam (Proton ile Windows oyunları), ekran kartına uygun 32/64 bit Vulkan sürücüleri,
 * GameMode ve MangoHud; işlemciyi en yüksek hızda tutan performans modu; diğer oyun başlatıcıları.
 * Yönetici işlerini /usr/libexec/aether/oyun-yardimci yapar (sudo, parola pencereyle sorulur).
 */
#include <gio/gdesktopappinfo.h>
#include "aether.h"
#include "yetki.h"

#define YARDIMCI "/usr/libexec/aether/oyun-yardimci"

static GtkWidget *pencere, *steam_durum, *steam_dugme, *gpu_durum, *gpu_dugme, *perf_anahtar, *perf_durum;
static GtkWidget *is_cubugu, *is_yazi, *is_ilerleme, *gunluk;
static int mesgul, perf_guncelleniyor;

static int paket_var(const char *p) {
  char *argv[] = { "pacman", "-Qq", (char *)p, NULL }; int st = 1;
  g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, &st, NULL);
  return st == 0;
}

static char *oku(const char *yol) { char *c = NULL; if (g_file_get_contents(yol, &c, NULL, NULL)) return g_strstrip(c); return NULL; }

/* ekran kartı adları (lspci varsa tam ad, yoksa üretici) */
static char *ekran_kartlari(void) {
  GString *s = g_string_new(NULL);
  char *cikti = NULL;
  char *argv[] = { "lspci", "-mm", NULL };
  if (g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, &cikti, NULL, NULL, NULL) && cikti) {
    char **sat = g_strsplit(cikti, "\n", -1);
    for (int i = 0; sat[i]; i++) {
      if (!strstr(sat[i], "VGA") && !strstr(sat[i], "3D controller") && !strstr(sat[i], "Display controller")) continue;
      /* biçim: 00:02.0 "VGA compatible controller" "Intel Corporation" "UHD Graphics 620" ... */
      char **p = g_strsplit(sat[i], "\"", -1);
      if (g_strv_length(p) >= 6) g_string_append_printf(s, "%s%s %s", s->len ? "\n" : "", p[3], p[5]);
      g_strfreev(p);
    }
    g_strfreev(sat);
  }
  g_free(cikti);
  if (!s->len) {
    GDir *d = g_dir_open("/sys/bus/pci/devices", 0, NULL); const char *n;
    while (d && (n = g_dir_read_name(d))) {
      char yol[300]; snprintf(yol, sizeof yol, "/sys/bus/pci/devices/%s/class", n);
      char *c = oku(yol); if (!c || strncmp(c, "0x03", 4)) { g_free(c); continue; } g_free(c);
      snprintf(yol, sizeof yol, "/sys/bus/pci/devices/%s/vendor", n);
      char *v = oku(yol);
      const char *ad = !g_strcmp0(v, "0x1002") ? "AMD" : !g_strcmp0(v, "0x8086") ? "Intel" : !g_strcmp0(v, "0x10de") ? "NVIDIA" : T("Sanal ekran kartı", "Virtual display adapter");
      g_string_append_printf(s, "%s%s", s->len ? "\n" : "", ad);
      g_free(v);
    }
    if (d) g_dir_close(d);
  }
  return g_string_free(s, FALSE);
}

static int vulkan_surucusu_var(void) {
  return paket_var("vulkan-radeon") || paket_var("vulkan-intel") || paket_var("nvidia-utils") || paket_var("vulkan-nouveau") || paket_var("vulkan-swrast");
}

static int perf_acik(void) {
  char *g = oku("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
  int r = g && !strcmp(g, "performance");
  g_free(g);
  return r;
}

static void durumu_yenile(void) {
  int steam = paket_var("steam");
  gtk_label_set_text(GTK_LABEL(steam_durum), steam ? T("✓ Kurulu", "✓ Installed") : T("Kurulu değil", "Not installed"));
  gtk_button_set_label(GTK_BUTTON(steam_dugme), steam ? T("Steam'i aç", "Open Steam") : T("Steam'i kur", "Install Steam"));
  char *kart = ekran_kartlari();
  int vk = vulkan_surucusu_var();
  char *m = g_strdup_printf("%s\n%s", *kart ? kart : T("Ekran kartı bulunamadı", "No graphics card found"),
                            vk ? T("✓ Oyun sürücüleri (Vulkan, 32 bit dahil) kurulu", "✓ Gaming drivers (Vulkan, incl. 32-bit) installed")
                               : T("Oyun sürücüleri (Vulkan) kurulu değil", "Gaming drivers (Vulkan) not installed"));
  gtk_label_set_text(GTK_LABEL(gpu_durum), m); g_free(m); g_free(kart);
  gtk_button_set_label(GTK_BUTTON(gpu_dugme), vk ? T("Yeniden kur", "Reinstall") : T("Sürücüleri kur", "Install drivers"));
  int freq = g_file_test("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", G_FILE_TEST_EXISTS);
  perf_guncelleniyor = 1;
  gtk_switch_set_active(GTK_SWITCH(perf_anahtar), freq && perf_acik());
  perf_guncelleniyor = 0;
  gtk_widget_set_sensitive(perf_anahtar, freq && !mesgul);
  char *g = oku("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor");
  char *pm = freq ? g_strdup_printf(T("Şu anki işlemci profili: %s", "Current CPU profile: %s"), g ? g : "?")
                  : g_strdup(T("Bu bilgisayarda (ya da sanal makinede) işlemci hızı ayarlanamıyor.", "CPU speed cannot be adjusted on this computer (or virtual machine)."));
  gtk_label_set_text(GTK_LABEL(perf_durum), pm); g_free(pm); g_free(g);
  gtk_widget_set_sensitive(steam_dugme, !mesgul);
  gtk_widget_set_sensitive(gpu_dugme, !mesgul);
}

/* ---------- arka plan işi ---------- */
static gboolean satir(GIOChannel *ch, GIOCondition c, gpointer v) {
  (void)v;
  if (c & G_IO_IN) {
    char *s = NULL; gsize n;
    if (g_io_channel_read_line(ch, &s, &n, NULL, NULL) == G_IO_STATUS_NORMAL && s) {
      GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gunluk)); GtkTextIter it;
      gtk_text_buffer_get_end_iter(b, &it); gtk_text_buffer_insert(b, &it, s, -1);
      gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(gunluk), &it, 0, FALSE, 0, 0);
      int a, t;
      if (sscanf(s, "(%d/%d)", &a, &t) == 2 && t > 0) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(is_ilerleme), (double)a / t);
      else gtk_progress_bar_pulse(GTK_PROGRESS_BAR(is_ilerleme));
      g_free(s);
      return TRUE;
    }
  }
  return !(c & (G_IO_HUP | G_IO_ERR));
}

static void bitti(GPid pid, int st, gpointer v) {
  char *ad = v;
  g_spawn_close_pid(pid);
  mesgul = 0;
  int ok = g_spawn_check_wait_status(st, NULL);
  gtk_label_set_text(GTK_LABEL(is_yazi), ok ? T("Tamamlandı.", "Done.") : T("İşlem tamamlanamadı. Ayrıntılar aşağıda.", "The operation failed. See details below."));
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(is_ilerleme), ok ? 1 : 0);
  if (ok && ad) {
    char *argv[] = { "notify-send", "-a", "Oyun Modu", "-i", "aether-oyun", ad, NULL };
    g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL);
  }
  g_free(ad);
  durumu_yenile();
}

static int calistir(const char *neden, const char *bitis_mesaji, const char *a1, const char *a2) {
  if (mesgul) return 0;
  if (!ae_yetki_al(GTK_WINDOW(pencere), neden)) return 0;
  const char *argv[] = { "sudo", "-n", YARDIMCI, a1, a2, NULL };
  GPid pid; int cikis;
  if (!g_spawn_async_with_pipes(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                                NULL, NULL, &pid, NULL, &cikis, NULL, NULL)) return 0;
  mesgul = 1;
  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gunluk)), "", -1);
  gtk_label_set_text(GTK_LABEL(is_yazi), neden);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(is_ilerleme), 0);
  gtk_widget_show(is_cubugu);
  GIOChannel *ch = g_io_channel_unix_new(cikis);
  g_io_channel_set_close_on_unref(ch, TRUE);
  g_io_add_watch(ch, G_IO_IN | G_IO_HUP | G_IO_ERR, satir, NULL);
  g_io_channel_unref(ch);
  g_child_watch_add(pid, bitti, g_strdup(bitis_mesaji));
  durumu_yenile();
  return 1;
}

static void steam_tik(GtkButton *b, gpointer v) {
  (void)b; (void)v;
  if (paket_var("steam")) { g_spawn_command_line_async("steam", NULL); return; }
  calistir(T("Steam, oyun sürücüleri, GameMode ve MangoHud kuruluyor… (önce sistem güncellenir, birkaç dakika sürebilir)",
             "Installing Steam, gaming drivers, GameMode and MangoHud… (the system is updated first; this can take a few minutes)"),
           T("Steam kuruldu. İlk açılışta kendini güncelleyecek.", "Steam is installed. It will update itself on first launch."),
           "steam", g_get_user_name());
}
static void gpu_tik(GtkButton *b, gpointer v) {
  (void)b; (void)v;
  calistir(T("Ekran kartı sürücüleri kuruluyor…", "Installing graphics drivers…"),
           T("Ekran kartı sürücüleri kuruldu. Bilgisayarı yeniden başlatman önerilir.", "Graphics drivers installed. Restarting is recommended."), "surucu", NULL);
}
static gboolean perf_degisti(GtkSwitch *s, gboolean acik, gpointer v) {
  (void)v;
  if (perf_guncelleniyor) return FALSE;
  if (!calistir(acik ? T("Performans modu açılıyor…", "Turning on performance mode…") : T("Performans modu kapatılıyor…", "Turning off performance mode…"),
                NULL, "performans", acik ? "ac" : "kapat")) {
    perf_guncelleniyor = 1; gtk_switch_set_active(s, !acik); perf_guncelleniyor = 0;
  }
  return FALSE;
}
static void magaza_ac(GtkButton *b, gpointer v) { (void)b; const char *argv[] = { "aether-magaza", v, NULL }; g_spawn_async(NULL, (char **)argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL); }
static void kopyala(GtkButton *b, gpointer v) {
  (void)v;
  gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), "gamemoderun %command%", -1);
  gtk_button_set_label(b, T("Kopyalandı ✓", "Copied ✓"));
}

/* ---------- arayüz ---------- */
static GtkWidget *kart(const char *simge, const char *baslik, const char *aciklama, GtkWidget **durum, GtkWidget *sag) {
  GtkWidget *f = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_style_context_add_class(gtk_widget_get_style_context(f), "ae-kart");
  GtkWidget *im = gtk_image_new_from_icon_name(simge, GTK_ICON_SIZE_DIALOG);
  gtk_widget_set_valign(im, GTK_ALIGN_START);
  GtkWidget *y = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *b = gtk_label_new(baslik); gtk_label_set_xalign(GTK_LABEL(b), 0);
  gtk_style_context_add_class(gtk_widget_get_style_context(b), "oyun-baslik");
  GtkWidget *a = ae_etiket(aciklama, "ae-alt");
  gtk_box_pack_start(GTK_BOX(y), b, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(y), a, FALSE, FALSE, 0);
  if (durum) { *durum = ae_etiket("", NULL); gtk_box_pack_start(GTK_BOX(y), *durum, FALSE, FALSE, 4); }
  gtk_box_pack_start(GTK_BOX(f), im, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(f), y, TRUE, TRUE, 0);
  if (sag) { gtk_widget_set_valign(sag, GTK_ALIGN_CENTER); gtk_box_pack_end(GTK_BOX(f), sag, FALSE, FALSE, 0); }
  return f;
}

static void etkinlesti(GtkApplication *app, gpointer v) {
  (void)v;
  GList *w = gtk_application_get_windows(app);
  if (w) { gtk_window_present(GTK_WINDOW(w->data)); return; }
  ae_css();
  GtkCssProvider *css = gtk_css_provider_new();
  gtk_css_provider_load_from_data(css, ".oyun-baslik { font-weight: bold; font-size: 12pt; } .ae-kart { background: @theme_base_color; }", -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  pencere = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(pencere), T("Oyun Modu", "Game Mode"));
  gtk_window_set_icon_name(GTK_WINDOW(pencere), "aether-oyun");
  gtk_window_set_default_size(GTK_WINDOW(pencere), 760, 640);

  GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_container_set_border_width(GTK_CONTAINER(k), 22);
  gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("O Y U N   M O D U", "G A M E   M O D E"), "ae-baslik"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), ae_etiket(T("Aether'i oyun için hazırla: Steam, sürücüler ve performans tek yerde.", "Get Aether ready for gaming: Steam, drivers and performance in one place."), "ae-alt"), FALSE, FALSE, 0);

  steam_dugme = gtk_button_new_with_label("");
  gtk_style_context_add_class(gtk_widget_get_style_context(steam_dugme), "suggested-action");
  g_signal_connect(steam_dugme, "clicked", G_CALLBACK(steam_tik), NULL);
  gtk_box_pack_start(GTK_BOX(k), kart("input-gaming", "Steam",
    T("Binlerce oyun. Windows oyunları Proton sayesinde Aether'de çalışır. Kurulumla birlikte ekran kartına uygun 32 bit sürücüler, GameMode ve MangoHud (FPS göstergesi) da gelir.",
      "Thousands of games. Windows games run on Aether thanks to Proton. Installing also adds 32-bit drivers for your graphics card, GameMode and MangoHud (FPS overlay)."),
    &steam_durum, steam_dugme), FALSE, FALSE, 0);

  gpu_dugme = gtk_button_new_with_label("");
  g_signal_connect(gpu_dugme, "clicked", G_CALLBACK(gpu_tik), NULL);
  gtk_box_pack_start(GTK_BOX(k), kart("video-display", T("Ekran kartı", "Graphics card"),
    T("Oyunların ekran kartını tam güçle kullanabilmesi için Vulkan sürücüleri.", "Vulkan drivers so games can use your graphics card at full power."),
    &gpu_durum, gpu_dugme), FALSE, FALSE, 0);

  perf_anahtar = gtk_switch_new();
  g_signal_connect(perf_anahtar, "state-set", G_CALLBACK(perf_degisti), NULL);
  gtk_box_pack_start(GTK_BOX(k), kart("power-profile-performance-symbolic", T("Performans modu", "Performance mode"),
    T("İşlemciyi en yüksek hızda tutar; oyunlarda takılmayı azaltır. Dizüstünde pil daha çabuk biter. Yeniden başlatınca kapanır.",
      "Keeps the CPU at full speed to reduce stutter in games. Drains laptop batteries faster. Turns off after a restart."),
    &perf_durum, perf_anahtar), FALSE, FALSE, 0);

  GtkWidget *ipucu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *kop = gtk_button_new_with_label(T("Kopyala", "Copy"));
  g_signal_connect(kop, "clicked", G_CALLBACK(kopyala), NULL);
  gtk_box_pack_start(GTK_BOX(ipucu), kop, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), kart("dialog-information", T("İpuçları", "Tips"),
    T("• Windows oyunları için: Steam › Ayarlar › Uyumluluk › \"Steam Play'i diğer tüm oyunlar için etkinleştir\".\n"
      "• Bir oyunu GameMode ile açmak için: oyuna sağ tık › Özellikler › Başlatma seçenekleri:  gamemoderun %command%\n"
      "• FPS göstergesi için başlatma seçeneği:  mangohud %command%",
      "• For Windows games: Steam › Settings › Compatibility › \"Enable Steam Play for all other titles\".\n"
      "• To run a game with GameMode: right-click the game › Properties › Launch options:  gamemoderun %command%\n"
      "• For the FPS overlay use the launch option:  mangohud %command%"),
    NULL, ipucu), FALSE, FALSE, 0);

  GtkWidget *diger = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  const char *baslaticilar[][2] = { { "Prism Launcher", "prism" }, { "Lutris", "lutris" }, { "RetroArch", "retroarch" }, { "Luanti", "luanti" } };
  for (unsigned i = 0; i < G_N_ELEMENTS(baslaticilar); i++) {
    GtkWidget *b = gtk_button_new_with_label(baslaticilar[i][0]);
    g_signal_connect(b, "clicked", G_CALLBACK(magaza_ac), (gpointer)baslaticilar[i][1]);
    gtk_box_pack_start(GTK_BOX(diger), b, FALSE, FALSE, 0);
  }
  GtkWidget *dk = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  gtk_box_pack_start(GTK_BOX(dk), ae_etiket(T("Diğer oyun başlatıcıları (Mağaza'da açılır):", "Other game launchers (opens in the Store):"), "ae-alt"), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(dk), diger, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), dk, FALSE, FALSE, 4);

  /* işlem durumu */
  is_cubugu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
  is_yazi = ae_etiket("", NULL);
  is_ilerleme = gtk_progress_bar_new(); gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(is_ilerleme), 0.05);
  GtkWidget *exp = gtk_expander_new(T("Ayrıntılar", "Details"));
  gunluk = gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(gunluk), FALSE); gtk_text_view_set_monospace(GTK_TEXT_VIEW(gunluk), TRUE);
  GtkWidget *sc = gtk_scrolled_window_new(NULL, NULL); gtk_widget_set_size_request(sc, -1, 140);
  gtk_container_add(GTK_CONTAINER(sc), gunluk); gtk_container_add(GTK_CONTAINER(exp), sc);
  gtk_box_pack_start(GTK_BOX(is_cubugu), is_yazi, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(is_cubugu), is_ilerleme, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(is_cubugu), exp, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), is_cubugu, FALSE, FALSE, 0);

  GtkWidget *kay = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(kay), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(kay), k);
  gtk_container_add(GTK_CONTAINER(pencere), kay);
  gtk_widget_show_all(pencere);
  gtk_widget_hide(is_cubugu);
  durumu_yenile();
}

int main(int argc, char **argv) {
  GtkApplication *app = gtk_application_new("org.aether.OyunModu", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(etkinlesti), NULL);
  int r = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return r;
}
