CC ?= cc
PKG_CONFIG ?= pkg-config
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
APPDIR ?= $(DATADIR)/applications
ICONDIR ?= $(DATADIR)/icons/hicolor

CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0 vte-2.91 fontconfig)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0 vte-2.91 fontconfig)

SOURCES := src/main.c src/config.c
OBJECTS := $(SOURCES:src/%.c=build/%.o)

.PHONY: all check clean install uninstall

all: check build/g-larpterm

check:
	@$(PKG_CONFIG) --exists gtk+-3.0 vte-2.91 fontconfig || { echo "gtk+-3.0, vte-2.91 and fontconfig development packages are required"; exit 1; }

build/g-larpterm: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) $(GTK_LIBS)

build/%.o: src/%.c src/config.h
	@mkdir -p build
	$(CC) $(CFLAGS) $(GTK_CFLAGS) -c $< -o $@

clean:
	rm -rf build

install: build/g-larpterm
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 build/g-larpterm $(DESTDIR)$(BINDIR)/g-larpterm
	install -d $(DESTDIR)$(DATADIR)/g-larpterm
	install -m 644 data/g-larpterm-pixel.ttf $(DESTDIR)$(DATADIR)/g-larpterm/g-larpterm-pixel.ttf
	install -d $(DESTDIR)$(APPDIR)
	install -m 644 data/g-larpterm.desktop $(DESTDIR)$(APPDIR)/g-larpterm.desktop
	install -d $(DESTDIR)$(ICONDIR)/scalable/apps
	install -m 644 data/g-larpterm.svg $(DESTDIR)$(ICONDIR)/scalable/apps/g-larpterm.svg
	install -d $(DESTDIR)$(ICONDIR)/128x128/apps
	install -m 644 data/g-larpterm-128.png $(DESTDIR)$(ICONDIR)/128x128/apps/g-larpterm.png
	install -d $(DESTDIR)$(ICONDIR)/256x256/apps
	install -m 644 data/g-larpterm-256.png $(DESTDIR)$(ICONDIR)/256x256/apps/g-larpterm.png

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/g-larpterm
	rm -f $(DESTDIR)$(DATADIR)/g-larpterm/g-larpterm-pixel.ttf
	rm -f $(DESTDIR)$(APPDIR)/g-larpterm.desktop
	rm -f $(DESTDIR)$(ICONDIR)/scalable/apps/g-larpterm.svg
	rm -f $(DESTDIR)$(ICONDIR)/128x128/apps/g-larpterm.png
	rm -f $(DESTDIR)$(ICONDIR)/256x256/apps/g-larpterm.png
