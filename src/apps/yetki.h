/* yetki.h — yönetici izni: sudo parolasını Windows'taki gibi bir pencereyle sorar
 * ae_yetki_al(pencere, neden) 1 dönerse ardından "sudo -n KOMUT" parola sormadan çalışır. */
#ifndef AETHER_YETKI_H
#define AETHER_YETKI_H
#include <sys/wait.h>
#include "aether.h"
static char *ae_parola_sor(GtkWindow *ust, const char *neden) {
  GtkWidget *d = gtk_dialog_new_with_buttons(T("Yönetici izni", "Administrator permission"), ust, GTK_DIALOG_MODAL,
                                             T("İptal", "Cancel"), GTK_RESPONSE_CANCEL, T("İzin ver", "Allow"), GTK_RESPONSE_OK, NULL);
  gtk_dialog_set_default_response(GTK_DIALOG(d), GTK_RESPONSE_OK);
  GtkWidget *k = gtk_dialog_get_content_area(GTK_DIALOG(d));
  gtk_container_set_border_width(GTK_CONTAINER(k), 16);
  gtk_box_set_spacing(GTK_BOX(k), 10);
  GtkWidget *ustk = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_box_pack_start(GTK_BOX(ustk), gtk_image_new_from_icon_name("dialog-password", GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 0);
  char *m = g_strdup_printf("%s\n\n%s", neden, T("Devam etmek için parolanı gir.", "Enter your password to continue."));
  GtkWidget *l = gtk_label_new(m); g_free(m);
  gtk_label_set_line_wrap(GTK_LABEL(l), TRUE); gtk_label_set_xalign(GTK_LABEL(l), 0); gtk_label_set_max_width_chars(GTK_LABEL(l), 44);
  gtk_box_pack_start(GTK_BOX(ustk), l, TRUE, TRUE, 0);
  GtkWidget *g = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(g), FALSE);
  gtk_entry_set_input_purpose(GTK_ENTRY(g), GTK_INPUT_PURPOSE_PASSWORD);
  gtk_entry_set_activates_default(GTK_ENTRY(g), TRUE);
  gtk_entry_set_placeholder_text(GTK_ENTRY(g), T("Parola", "Password"));
  gtk_box_pack_start(GTK_BOX(k), ustk, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(k), g, FALSE, FALSE, 0);
  gtk_widget_show_all(d);
  char *p = NULL;
  if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) p = g_strdup(gtk_entry_get_text(GTK_ENTRY(g)));
  gtk_widget_destroy(d);
  return p;
}

/* sudo parolasını doğrula; zaten izin varsa sormaz */
static int ae_yetki_al(GtkWindow *ust, const char *neden) {
  int durum = 1;
  char *argv0[] = { "sudo", "-n", "-v", NULL };
  if (g_spawn_sync(NULL, argv0, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, NULL, NULL, NULL, NULL, &durum, NULL) && durum == 0)
    return 1;
  for (int deneme = 0; deneme < 3; deneme++) {
    char *p = ae_parola_sor(ust, deneme ? T("Parola yanlış, tekrar dene.", "Wrong password, try again.") : neden);
    if (!p) return 0;
    char *argv[] = { "sudo", "-S", "-p", "", "-v", NULL };
    GPid pid; int girdi;
    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                                  NULL, NULL, &pid, &girdi, NULL, NULL, NULL)) { g_free(p); return 0; }
    char *satir = g_strdup_printf("%s\n", p);
    if (write(girdi, satir, strlen(satir)) < 0) {}
    close(girdi);
    memset(satir, 0, strlen(satir)); memset(p, 0, strlen(p)); g_free(satir); g_free(p);
    int st = 0; waitpid(pid, &st, 0); g_spawn_close_pid(pid);
    if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return 1;
  }
  return 0;
}

#endif
