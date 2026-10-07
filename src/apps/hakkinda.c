/* aether-hakkinda — Aether Hakkında / About Aether */
#include "aether.h"
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

static char *cpu_adi(void) {
    char *c = NULL, *r = NULL;
    if (g_file_get_contents("/proc/cpuinfo", &c, NULL, NULL)) {
        char *p = strstr(c, "model name");
        if (p && (p = strchr(p, ':'))) { p += 2; char *e = strchr(p, '\n'); r = g_strndup(p, e ? e - p : strlen(p)); }
        g_free(c);
    }
    return r ? r : g_strdup("?");
}
static char *alpine_surum(void) {
    char *c = NULL;
    if (g_file_get_contents("/etc/alpine-release", &c, NULL, NULL)) return g_strstrip(c);
    return g_strdup("?");
}
static void satir(GtkWidget *g, int i, const char *a, const char *b) {
    GtkWidget *x = gtk_label_new(a), *y = gtk_label_new(b);
    gtk_style_context_add_class(gtk_widget_get_style_context(x), "ae-alt");
    gtk_label_set_xalign(GTK_LABEL(x), 1); gtk_label_set_xalign(GTK_LABEL(y), 0);
    
    gtk_grid_attach(GTK_GRID(g), x, 0, i, 1, 1); gtk_grid_attach(GTK_GRID(g), y, 1, i, 1, 1);
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv); ae_css();
    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), T("Aether Hakkında", "About Aether"));
    gtk_window_set_icon_name(GTK_WINDOW(w), "aether-hakkinda");
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
    g_signal_connect(w, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    GtkWidget *k = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(k), 28);
    gtk_box_pack_start(GTK_BOX(k), ae_logo(112), FALSE, FALSE, 0);
    GtkWidget *b = gtk_label_new("A E T H E R"); gtk_style_context_add_class(gtk_widget_get_style_context(b), "ae-baslik");
    gtk_box_pack_start(GTK_BOX(k), b, FALSE, FALSE, 0);
    GtkWidget *s = gtk_label_new(AETHER_SURUM " \"" AETHER_KODADI "\""); gtk_style_context_add_class(gtk_widget_get_style_context(s), "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), s, FALSE, FALSE, 0);
    GtkWidget *y = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(y), ae_en() ? "Made by <b>" AETHER_YAPIMCI "</b>" : "Yapımcı: <b>" AETHER_YAPIMCI "</b>");
    gtk_box_pack_start(GTK_BOX(k), y, FALSE, FALSE, 6);

    struct utsname u; uname(&u);
    struct sysinfo si; sysinfo(&si);
    struct statvfs fs; statvfs("/", &fs);
    char ram[64], disk[64], sure[64], *cpu = cpu_adi(), *alp = alpine_surum(), taban[96];
    snprintf(ram, sizeof ram, "%.1f GB", si.totalram * (double)si.mem_unit / 1073741824.0);
    snprintf(disk, sizeof disk, T("%.1f GB boş / %.1f GB", "%.1f GB free / %.1f GB"),
        fs.f_bavail * (double)fs.f_frsize / 1073741824.0, fs.f_blocks * (double)fs.f_frsize / 1073741824.0);
    snprintf(sure, sizeof sure, T("%ld sa %ld dk", "%ldh %ldm"), si.uptime / 3600, si.uptime / 60 % 60);
    snprintf(taban, sizeof taban, "Alpine Linux %s", alp);
    GtkWidget *g = gtk_grid_new(); gtk_grid_set_column_spacing(GTK_GRID(g), 14); gtk_grid_set_row_spacing(GTK_GRID(g), 4);
    gtk_widget_set_halign(g, GTK_ALIGN_CENTER);
    int i = 0;
    satir(g, i++, T("Taban", "Base"), taban);
    satir(g, i++, T("Çekirdek", "Kernel"), u.release);
    satir(g, i++, T("İşlemci", "CPU"), cpu);
    satir(g, i++, T("Bellek", "Memory"), ram);
    satir(g, i++, "Disk", disk);
    satir(g, i++, T("Çalışma süresi", "Uptime"), sure);
    satir(g, i++, T("Masaüstü", "Desktop"), "Openbox · Aether");
    satir(g, i++, T("Mod", "Mode"), ae_canli() ? T("Canlı (RAM)", "Live (RAM)") : T("Kurulu", "Installed"));
    gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
    GtkWidget *n = gtk_label_new(T("Aether, özgür yazılımlar üzerine kurulmuştur.\nLinux, Alpine, Openbox, GTK ve Mesa topluluklarına teşekkürler.",
                                   "Aether is built on free software.\nThanks to the Linux, Alpine, Openbox, GTK and Mesa communities."));
    gtk_label_set_justify(GTK_LABEL(n), GTK_JUSTIFY_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(n), "ae-alt");
    gtk_box_pack_start(GTK_BOX(k), n, FALSE, FALSE, 10);
    gtk_container_add(GTK_CONTAINER(w), k);
    gtk_widget_show_all(w);
    gtk_main();
    g_free(cpu); g_free(alp);
    return 0;
}
