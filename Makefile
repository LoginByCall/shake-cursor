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

# Можно переопределить при установке:
#   make install DEVICE=/dev/input/by-id/...-event-mouse
#   make install DEVICE=/dev/input/event5
DEVICE  ?=

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(PKGCFG)

install: $(TARGET)
	mkdir -p "$(BINDIR)"
	install -m755 "$(TARGET)" "$(BINDIR)/"
	mkdir -p "$(SYSTEMD)"
	@set -eu; \
	dev="$(DEVICE)"; \
	if [ -z "$$dev" ]; then \
	  for p in /dev/input/by-id/*Touchpad*-event-mouse; do \
	    if [ -e "$$p" ]; then dev="$$p"; break; fi; \
	  done; \
	fi; \
	if [ -z "$$dev" ]; then \
	  for p in /dev/input/by-id/*-event-mouse; do \
	    if [ -e "$$p" ]; then dev="$$p"; break; fi; \
	  done; \
	fi; \
	if [ -z "$$dev" ] && [ -r /proc/bus/input/devices ]; then \
	  dev="$$(sed -n '/^N: Name=.*Touchpad/,/^$$/p' /proc/bus/input/devices | sed -n 's/.*\(event[0-9]\+\).*/\/dev\/input\/\1/p' | head -n1)"; \
	  if [ -z "$$dev" ]; then \
	    dev="$$(sed -n '/^N: Name=.*Mouse/,/^$$/p' /proc/bus/input/devices | sed -n 's/.*\(event[0-9]\+\).*/\/dev\/input\/\1/p' | head -n1)"; \
	  fi; \
	fi; \
	if [ -z "$$dev" ]; then \
	  echo "ERROR: could not detect input device." >&2; \
	  echo "Hint: run  make install DEVICE=/dev/input/by-id/...-event-mouse  or  /dev/input/eventX" >&2; \
	  exit 1; \
	fi; \
	echo "Using device: $$dev"; \
	printf '%s\n' \
	  "[Unit]" \
	  "Description=Shake-to-Find cursor enlarger (C version)" \
	  "After=graphical-session.target" \
	  "PartOf=graphical-session.target" \
	  "" \
	  "[Service]" \
	  "Type=simple" \
	  "ExecStart=$(BINDIR)/$(TARGET) --device $$dev" \
	  "Restart=on-failure" \
	  "RestartSec=1s" \
	  "Nice=5" \
	  "IOSchedulingClass=idle" \
	  "CPUQuota=10%" \
	  "Environment=DBUS_SESSION_BUS_ADDRESS=unix:path=%t/bus" \
	  "Environment=HOME=%h" \
	  "Environment=XDG_RUNTIME_DIR=%t" \
	  "" \
	  "[Install]" \
	  "WantedBy=default.target" \
	  > "$(SERVICE)"; \
	systemctl --user daemon-reload; \
	systemctl --user enable --now shake-cursor.service

uninstall:
	systemctl --user disable --now shake-cursor.service || true
	rm -f "$(BINDIR)/$(TARGET)"
	rm -f "$(SERVICE)"
	systemctl --user daemon-reload

clean:
	rm -f "$(TARGET)"
