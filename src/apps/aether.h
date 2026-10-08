/* Aether 1.1 "Nebula" — ortak yardımcılar */
#ifndef AETHER_H
#define AETHER_H
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define AETHER_SURUM   "1.1"
#define AETHER_KODADI  "Nebula"
#define AETHER_YAPIMCI "Hot Zot"   /* yalnızca Aether Hakkında penceresinde gösterilir */

/* Dil: kurulumda çalışma anında değişebilir */
static int ae_dil_zorla = -1;   /* -1: LANG'e bak, 0: tr, 1: en */
static inline int ae_en(void) {
    if (ae_dil_zorla >= 0) return ae_dil_zorla;
    const char *l = getenv("LANG");
    return l && strncmp(l, "en", 2) == 0;
}
#define T(tr, en) (ae_en() ? (en) : (tr))

/* ~/.config/aether/ayarlar  (ANAHTAR=değer satırları) */
static inline char *ae_ayar_yolu(void) {
    return g_build_filename(g_get_user_config_dir(), "aether", "ayarlar", NULL);
}
static inline char *ae_ayar(const char *anahtar, const char *varsayilan) {
    char *yol = ae_ayar_yolu(), *icerik = NULL, *sonuc = NULL;
    const char *dosyalar[] = { yol, "/etc/aether/ayarlar", NULL };
    for (int d = 0; dosyalar[d] && !sonuc; d++) {
        if (!g_file_get_contents(dosyalar[d], &icerik, NULL, NULL)) continue;
        char **satir = g_strsplit(icerik, "\n", -1);
        size_t n = strlen(anahtar);
        for (int i = 0; satir[i]; i++)
            if (!strncmp(satir[i], anahtar, n) && satir[i][n] == '=') { sonuc = g_strdup(satir[i] + n + 1); break; }
        g_strfreev(satir); g_free(icerik); icerik = NULL;
    }
    g_free(yol);
    return sonuc ? sonuc : g_strdup(varsayilan);
}
static inline void ae_ayar_yaz(const char *anahtar, const char *deger) {
    char *yol = ae_ayar_yolu(), *icerik = NULL;
    char *dizin = g_path_get_dirname(yol); g_mkdir_with_parents(dizin, 0755); g_free(dizin);
    GString *yeni = g_string_new(NULL);
    gboolean bulundu = FALSE; size_t n = strlen(anahtar);
    if (g_file_get_contents(yol, &icerik, NULL, NULL)) {
        char **satir = g_strsplit(icerik, "\n", -1);
        for (int i = 0; satir[i]; i++) {
            if (!*satir[i]) continue;
            if (!strncmp(satir[i], anahtar, n) && satir[i][n] == '=') {
                g_string_append_printf(yeni, "%s=%s\n", anahtar, deger); bulundu = TRUE;
            } else g_string_append_printf(yeni, "%s\n", satir[i]);
        }
        g_strfreev(satir); g_free(icerik);
    }
    if (!bulundu) g_string_append_printf(yeni, "%s=%s\n", anahtar, deger);
    g_file_set_contents(yol, yeni->str, -1, NULL);
    g_string_free(yeni, TRUE); g_free(yol);
}

static inline int ae_canli(void) {
    char *c = NULL; int r = 0;
    if (g_file_get_contents("/proc/cmdline", &c, NULL, NULL)) { r = strstr(c, "aether.canli") != NULL; g_free(c); }
    return r;
}

/* Ortak görünüm: başlık sınıfları */
static inline void ae_css(void) {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p,
        ".ae-baslik { font-size: 20pt; font-weight: 300; letter-spacing: 6px; }"
        ".ae-alt { opacity: 0.7; }"
        ".ae-uyari { color: #c62828; font-weight: bold; }"
        ".ae-kart { border: 1px solid alpha(currentColor,0.2); padding: 14px; }"
        ".ae-buyuk { font-size: 13pt; padding: 10px 18px; }"
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
static inline GtkWidget *ae_etiket(const char *metin, const char *sinif) {
    GtkWidget *l = gtk_label_new(metin);
    /* başlıklar tek satırda kalsın ("A E T H E R" bölünmesin) */
    gtk_label_set_line_wrap(GTK_LABEL(l), !(sinif && !strcmp(sinif, "ae-baslik")));
    gtk_label_set_xalign(GTK_LABEL(l), 0);
    if (sinif) gtk_style_context_add_class(gtk_widget_get_style_context(l), sinif);
    return l;
}
static inline GtkWidget *ae_logo(int boyut) {
    GdkPixbuf *pb = gdk_pixbuf_new_from_file_at_size("/usr/share/aether/logo.png", boyut, boyut, NULL);
    GtkWidget *im = pb ? gtk_image_new_from_pixbuf(pb) : gtk_image_new();
    if (pb) g_object_unref(pb);
    return im;
}
static inline void ae_calistir(const char *komut) { g_spawn_command_line_async(komut, NULL); }
#endif
