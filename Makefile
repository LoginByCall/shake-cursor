# Makefile для shake-cursor

CC      := gcc
CFLAGS  := -O2 -Wall -Wextra
PKGS    := libevdev gio-2.0 glib-2.0
PKGCFG  := $(shell pkg-config --cflags --libs $(PKGS))

TARGET  := shake-cursor
SRC     := shake_cursor.c

PREFIX  := $(HOME)/.local
BINDIR  := $(PREFIX)/bin
SYSTEMD := $(HOME)/.config/systemd/user
SERVICE := $(SYSTEMD)/shake-cursor.service

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(PKGCFG)

install: $(TARGET)
	mkdir -p $(BINDIR)
	install -m755 $(TARGET) $(BINDIR)/
	mkdir -p $(SYSTEMD)
	@echo "[Unit]"                                    >  $(SERVICE)
	@echo "Description=Shake-to-Find cursor enlarger (C version)" >> $(SERVICE)
	@echo "After=graphical-session.target"            >> $(SERVICE)
	@echo "PartOf=graphical-session.target"           >> $(SERVICE)
	@echo ""                                          >> $(SERVICE)
	@echo "[Service]"                                 >> $(SERVICE)
	@echo "Type=simple"                               >> $(SERVICE)
	@echo "ExecStart=$(BINDIR)/$(TARGET)"             >> $(SERVICE)
	@echo "Restart=on-failure"                        >> $(SERVICE)
	@echo "Nice=5"                                    >> $(SERVICE)
	@echo "IOSchedulingClass=idle"                    >> $(SERVICE)
	@echo "CPUQuota=10%"                              >> $(SERVICE)
	@echo ""                                          >> $(SERVICE)
	@echo "[Install]"                                 >> $(SERVICE)
	@echo "WantedBy=default.target"                   >> $(SERVICE)
	systemctl --user daemon-reload
	systemctl --user enable --now shake-cursor.service

uninstall:
	systemctl --user disable --now shake-cursor.service || true
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(SERVICE)
	systemctl --user daemon-reload

clean:
	rm -f $(TARGET)
