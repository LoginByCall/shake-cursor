# ====== Build config ======
CC      := gcc
CFLAGS  := -O2 -Wall -Wextra
# добавьте свои LDFLAGS при желании, например: -Wl,-O1 -Wl,--as-needed
LDFLAGS :=

PKGS    := glib-2.0 gio-2.0 gio-unix-2.0 gtk+-3.0 libevdev ayatana-appindicator3-0.1
PKGCFG  := $(shell pkg-config --cflags --libs $(PKGS))

TARGET  := shake-cursor
SRC     := src/shake_cursor.c

# ====== User install (ручная установка в $HOME) ======
USER_PREFIX ?= $(HOME)/.local
USER_BINDIR := $(USER_PREFIX)/bin
USER_SYSTEMD_DIR := $(HOME)/.config/systemd/user
USER_SERVICE := $(USER_SYSTEMD_DIR)/shake-cursor.service

# ====== System/packaging install (для .deb через DESTDIR) ======
PREFIX ?= /usr
BINDIR := $(PREFIX)/bin
SYSTEMD_USER_UNITDIR := $(PREFIX)/lib/systemd/user
PKG_SERVICE := debian/shake-cursor.service

INSTALL ?= install
STRIP   ?= strip

# ====== Phony targets ======
.PHONY: all clean install uninstall install-user help

# ====== Build ======
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(PKGCFG) $(LDFLAGS)

clean:
	rm -f $(TARGET)

help:
	@echo "Targets:"
	@echo "  make            - build"
	@echo "  make install    - install (system if DESTDIR set, else user)"
	@echo "  make uninstall  - uninstall from user location (~/.local)"
	@echo "  make clean      - remove binary"
	@echo ""
	@echo "Packaging (.deb) tip:"
	@echo "  dpkg-buildpackage calls: make install DESTDIR=\$$PWD/debian/shake-cursor"

# ====== Install ======
# Если DESTDIR задан (dpkg-buildpackage/packaging) — кладём в образ пакета и НИЧЕГО не запускаем.
install: $(TARGET)
ifneq ($(DESTDIR),)
	@echo "[install] packaging mode -> DESTDIR=$(DESTDIR)"
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m755 $(TARGET) $(DESTDIR)$(BINDIR)/
	$(STRIP) $(DESTDIR)$(BINDIR)/$(TARGET) || true
	$(INSTALL) -d $(DESTDIR)$(SYSTEMD_USER_UNITDIR)
	# используем уже подготовленный unit
	$(INSTALL) -m644 $(PKG_SERVICE) $(DESTDIR)$(SYSTEMD_USER_UNITDIR)/
else
	@$(MAKE) install-user
endif

# Живая пользовательская установка в ~/.local + enable user service
install-user: $(TARGET)
	@echo "[install] user mode -> $(USER_BINDIR)"
	$(INSTALL) -d $(USER_BINDIR)
	$(INSTALL) -m755 $(TARGET) $(USER_BINDIR)/
	$(INSTALL) -d $(USER_SYSTEMD_DIR)
	@echo "[Unit]"                                    >  $(USER_SERVICE)
	@echo "Description=Shake-to-Find cursor enlarger (user)" >> $(USER_SERVICE)
	@echo "After=graphical-session.target"            >> $(USER_SERVICE)
	@echo "PartOf=graphical-session.target"           >> $(USER_SERVICE)
	@echo ""                                          >> $(USER_SERVICE)
	@echo "[Service]"                                 >> $(USER_SERVICE)
	@echo "Type=simple"                               >> $(USER_SERVICE)
	@echo "ExecStart=$(USER_BINDIR)/$(TARGET)"        >> $(USER_SERVICE)
	@echo "Restart=on-failure"                        >> $(USER_SERVICE)
	@echo ""                                          >> $(USER_SERVICE)
	@echo "[Install]"                                 >> $(USER_SERVICE)
	@echo "WantedBy=default.target"                   >> $(USER_SERVICE)
	@systemctl --user daemon-reload
	@systemctl --user enable --now shake-cursor.service || true
	@echo "Installed to $(USER_BINDIR) and enabled user service."

# Удаляем только пользовательскую установку (~/.local и user-unit)
uninstall:
	@systemctl --user disable --now shake-cursor.service 2>/dev/null || true
	@rm -f $(USER_BINDIR)/$(TARGET)
	@rm -f $(USER_SERVICE)
	@systemctl --user daemon-reload || true
	@echo "Uninstalled from $(USER_BINDIR)."

