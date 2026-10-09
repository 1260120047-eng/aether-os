CFLAGS ?= -O2 -Wall -Wno-unused-function -Wno-deprecated-declarations
GTK = $(shell pkg-config --cflags --libs gtk+-3.0)
GIO = $(shell pkg-config --cflags --libs gio-unix-2.0)
SDL = $(shell pkg-config --cflags --libs sdl2)
WEB = $(shell pkg-config --cflags --libs webkit2gtk-4.1)
APPS = aether-kurulum aether-ayarlar aether-hosgeldin aether-hakkinda aether-oyunlar aether-mayin aether-yildizlar eclipse-tarayici yorunge yorunge-panel aether-terminal aether-bildirim aether-gorev aether-uygulamalar aether-masaustu aether-magaza aether-oyun-modu aether-dosyalar
all: $(APPS)
aether-kurulum: kurulum.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) -lm
aether-ayarlar: ayarlar.c aether.h ag.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK)
aether-hosgeldin: hosgeldin.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK)
aether-hakkinda: hakkinda.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK)
aether-oyunlar: oyunlar.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO)
aether-mayin: mayin.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK)
eclipse-tarayici: eclipse.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(WEB)
aether-yildizlar: yildizlar.c ; $(CC) $(CFLAGS) $< -o $@ $(SDL)
yorunge: yorunge.c ; $(CC) $(CFLAGS) $< -o $@ $(shell pkg-config --cflags --libs x11 xft xrender xcursor xrandr gdk-pixbuf-2.0)
VTE = $(shell pkg-config --cflags --libs vte-2.91)
WNCK = $(shell pkg-config --cflags --libs libwnck-3.0 x11)
yorunge-panel: panel.c aether.h ag.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(WNCK)
aether-terminal: terminal.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(VTE)
aether-bildirim: bildirim.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK)
aether-gorev: gorev.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO) $(shell pkg-config --cflags --libs x11) -lm
aether-uygulamalar: uygulamalar.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO)
aether-oyun-modu: oyun.c aether.h yetki.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO)
aether-dosyalar: dosyalar.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO)
aether-magaza: magaza.c aether.h yetki.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO) $(shell pkg-config --cflags --libs libsoup-3.0 json-glib-1.0)
aether-masaustu: masaustu.c aether.h ; $(CC) $(CFLAGS) $< -o $@ $(GTK) $(GIO) -lm
install: all ; install -Dm755 -t $(DESTDIR)/usr/bin $(APPS) && strip $(addprefix $(DESTDIR)/usr/bin/,$(APPS))
clean: ; rm -f $(APPS)
