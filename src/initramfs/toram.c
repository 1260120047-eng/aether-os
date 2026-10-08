/* aether-toram — sistemi açılışı bekletmeden RAM'e kopyalar
 *
 *   aether-toram LOOP_AYGITI KAYNAK HEDEF_KLASÖR
 *
 * Açılış, ISO/USB üzerindeki rootfs.sfs'ten (LOOP_AYGITI) hemen devam eder.
 * Bu program arka planda dosyayı HEDEF_KLASÖR/rootfs.sfs.tmp'ye kopyalar,
 * bitince adını rootfs.sfs yapar ve loop aygıtının arkasındaki dosyayı
 * LOOP_CHANGE_FD ile RAM'deki kopyaya geçirir. Çalışan sistem bunu fark etmez.
 *
 * Tüm dosyalar switch_root'tan ÖNCE açılır; sonrasında yol gerekmez.
 */
#include <fcntl.h>
#include <linux/loop.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char **argv) {
  if (argc != 4) { fprintf(stderr, "kullanım: aether-toram LOOP KAYNAK HEDEF_KLASÖR\n"); return 2; }
  int loop = open(argv[1], O_RDONLY);
  int kaynak = open(argv[2], O_RDONLY);
  int klasor = open(argv[3], O_RDONLY | O_DIRECTORY);
  if (loop < 0 || kaynak < 0 || klasor < 0) { perror("aether-toram"); return 1; }
  int hedef = openat(klasor, "rootfs.sfs.tmp", O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (hedef < 0) { perror("aether-toram: hedef"); return 1; }

  pid_t p = fork();
  if (p < 0) return 1;
  if (p > 0) return 0;                 /* açılış hemen devam etsin */

  setsid();
  setpriority(PRIO_PROCESS, 0, 10);    /* açılışla yarışmasın */
  static char tampon[1 << 20];
  for (;;) {
    ssize_t n = read(kaynak, tampon, sizeof tampon);
    if (n == 0) break;
    if (n < 0) { unlinkat(klasor, "rootfs.sfs.tmp", 0); return 1; }
    for (ssize_t y = 0; y < n;) {
      ssize_t w = write(hedef, tampon + y, n - y);
      if (w <= 0) { unlinkat(klasor, "rootfs.sfs.tmp", 0); return 1; }   /* RAM doldu: ortamdan devam */
      y += w;
    }
  }
  close(hedef);
  int ram = openat(klasor, "rootfs.sfs.tmp", O_RDONLY);
  struct stat a, b;
  if (ram < 0 || fstat(ram, &a) || fstat(kaynak, &b) || a.st_size != b.st_size) { unlinkat(klasor, "rootfs.sfs.tmp", 0); return 1; }
  if (ioctl(loop, LOOP_CHANGE_FD, ram) != 0) { unlinkat(klasor, "rootfs.sfs.tmp", 0); return 1; }
  /* kurulum programı yalnızca tamamlanmış kopyayı görsün */
  renameat(klasor, "rootfs.sfs.tmp", klasor, "rootfs.sfs");
  close(kaynak);
  return 0;
}
