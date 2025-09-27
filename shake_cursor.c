// gcc -O2 -Wall -Wextra -o shake-cursor shake_cursor.c $(pkg-config --cflags --libs libevdev gio-2.0 glib-2.0)
// Требует доступ на чтение /dev/input/event* (обычно группа input).
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

#include <libevdev/libevdev.h>
#include <glib.h>
#include <gio/gio.h>

// ===================== Настройки =====================
static const int    BIG_CURSOR_SIZE     = 256;   // размер после мотания
static const double ENLARGE_DURATION_S  = 1.5;   // сколько секунд держать крупный
static const int    MIN_DX               = 20;   // порог рывка по X
static const double MAX_INTERVAL_S       = 0.12; // макс. интервал между сменами направления
static const int    REQUIRED_FLIPS       = 3;    // сколько «туда-сюда» до срабатывания
static const double WINDOW_S             = 0.7;  // окно времени счёта смен
static const int    TICK_MS              = 100;  // частота проверки таймера
static const double ABS_IDLE_RESET_S     = 0.25; // пауза, после которой сбрасываем базу ABS_X
// ====================================================

typedef struct {
    struct libevdev *dev;
    int fd;

    bool use_rel;            // true: EV_REL/REL_X; false: EV_ABS/ABS_X
    char name[128];          // для логов/отладки

    // Для ABS (тачпад):
    int    last_abs_x;
    bool   have_last_abs_x;
    double last_abs_seen;    // время последнего ABS_X
} Dev;

typedef struct {
    int    last_sign;
    double last_flip_time;
    double flips[16]; // скользящее окно (до 16 меток)
    int    flips_count;
} ShakeDetector;

typedef struct {
    GSettings *iface;      // org.gnome.desktop.interface
    int    original_size;
    int    last_applied;
    double enlarged_until; // монотонное время, когда вернуть размер
} CursorCtl;

// ---- время ----
static double now_monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

// ---- GSettings cursor ----
static void cursor_force_reload_theme(GSettings *iface) {
    // Коротко переключаем тему курсора на альтернативную и обратно — иногда это помогает Force-Apply.
    gchar *current = g_settings_get_string(iface, "cursor-theme");
    if (!current) return;

    const char *alts[] = {"Adwaita", "Yaru", "DMZ-White", "DMZ-Black"};
    const char *alt = NULL;
    for (size_t i = 0; i < G_N_ELEMENTS(alts); i++) {
        if (g_strcmp0(current, alts[i]) != 0) { alt = alts[i]; break; }
    }
    if (alt) {
        g_settings_set_string(iface, "cursor-theme", alt);
        g_usleep(30 * 1000); // 30 ms
        g_settings_set_string(iface, "cursor-theme", current);
    }
    g_free(current);
}

static void cursor_set_size(CursorCtl *c, int size) {
    if (size == c->last_applied) return;
    g_settings_set_int(c->iface, "cursor-size", size);
    c->last_applied = size;
    cursor_force_reload_theme(c->iface);
}

static void cursor_enlarge(CursorCtl *c, double duration_s) {
    c->enlarged_until = now_monotonic_sec() + duration_s;
    cursor_set_size(c, BIG_CURSOR_SIZE);
}

static void cursor_tick(CursorCtl *c) {
    if (c->enlarged_until > 0 && now_monotonic_sec() >= c->enlarged_until) {
        cursor_set_size(c, c->original_size);
        c->enlarged_until = 0.0;
    }
}

// ---- Shake detector ----
static void sd_init(ShakeDetector *sd) {
    sd->last_sign = 0;
    sd->last_flip_time = 0.0;
    sd->flips_count = 0;
}

static void sd_prune_window(ShakeDetector *sd, double now) {
    int w = 0;
    for (int i = 0; i < sd->flips_count; i++) {
        if (now - sd->flips[i] <= WINDOW_S) sd->flips[w++] = sd->flips[i];
    }
    sd->flips_count = w;
}

static bool sd_feed_dx(ShakeDetector *sd, int dx) {
    if (dx > -MIN_DX && dx < MIN_DX) return false;
    int    sign = dx > 0 ? 1 : -1;
    double t    = now_monotonic_sec();

    if (sd->last_sign != 0 && sign != sd->last_sign) {
        if (t - sd->last_flip_time <= MAX_INTERVAL_S) {
            if (sd->flips_count < (int)(sizeof(sd->flips) / sizeof(sd->flips[0])))
                sd->flips[sd->flips_count++] = t;
            sd_prune_window(sd, t);
            if (sd->flips_count >= REQUIRED_FLIPS) {
                sd->flips_count   = 0;
                sd->last_sign     = sign;
                sd->last_flip_time= t;
                return true; // SHAKE!
            }
        }
    }
    sd->last_sign = sign;
    sd->last_flip_time = t;
    return false;
}

// ---- input discovery ----
static GPtrArray* discover_devices(void) {
    GPtrArray *arr = g_ptr_array_new_with_free_func(g_free);
    DIR *d = opendir("/dev/input");
    if (!d) return arr;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "event", 5) != 0) continue;
        char *path = g_strdup_printf("/dev/input/%s", e->d_name);
        g_ptr_array_add(arr, path);
    }
    closedir(d);
    return arr;
}

static bool is_pointer_like(struct libevdev *dev) {
    // Принимаем устройство, если:
    // - есть REL_X (типичная мышь), ИЛИ
    // - есть ABS_X и есть "finger" tool (тачпад), ИЛИ
    // - по имени похоже на Touchpad/Mouse (помогает при странных проперти)
    const char *name = libevdev_get_name(dev);
    bool has_rel = libevdev_has_event_type(dev, EV_REL) && libevdev_has_event_code(dev, EV_REL, REL_X);
    bool has_abs = libevdev_has_event_type(dev, EV_ABS) && libevdev_has_event_code(dev, EV_ABS, ABS_X);
    bool has_finger = libevdev_has_event_type(dev, EV_KEY) &&
        (libevdev_has_event_code(dev, EV_KEY, BTN_TOOL_FINGER) ||
         libevdev_has_event_code(dev, EV_KEY, BTN_TOOL_DOUBLETAP) ||
         libevdev_has_event_code(dev, EV_KEY, BTN_TOOL_TRIPLETAP));
    bool name_hint = name && (strstr(name, "Touchpad") || strstr(name, "Mouse"));
    return has_rel || (has_abs && (has_finger || name_hint)) || (name_hint && has_abs);
}

static Dev* open_one_device(const char *path) {
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return NULL;

    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) { close(fd); return NULL; }

    Dev *d = g_malloc0(sizeof(Dev));
    d->fd = fd;
    d->dev = dev;
    d->use_rel = libevdev_has_event_type(dev, EV_REL) && libevdev_has_event_code(dev, EV_REL, REL_X);
    const char *nm = libevdev_get_name(dev);
    if (nm) { strncpy(d->name, nm, sizeof(d->name)-1); d->name[sizeof(d->name)-1] = 0; }
    d->have_last_abs_x = false;
    d->last_abs_seen = 0.0;
    return d;
}

static GPtrArray* open_mouse_like_devices(void) {
    GPtrArray *devs = g_ptr_array_new_with_free_func(NULL);
    GPtrArray *paths = discover_devices();
    for (guint i = 0; i < paths->len; i++) {
        const char *path = paths->pdata[i];
        Dev *d = open_one_device(path);
        if (!d) continue;

        if (is_pointer_like(d->dev)) {
            g_ptr_array_add(devs, d);
        } else {
            libevdev_free(d->dev);
            close(d->fd);
            g_free(d);
        }
    }
    g_ptr_array_free(paths, TRUE);
    return devs;
}

static void print_usage(const char *argv0) {
    fprintf(stderr,
        "Usage: %s [--device /dev/input/eventX]\n"
        "  Listens to mouse (REL_X) and touchpad (ABS_X) to detect shakes and enlarge cursor.\n"
        "  If --device is set, only that device is used.\n", argv0);
}

// ---- Main ----
int main(int argc, char **argv) {
    const char *force_device = NULL;
    if (argc >= 2) {
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (argc >= 3 && strcmp(argv[1], "--device") == 0) {
            force_device = argv[2];
        }
    }

    // GSettings
    g_autoptr(GSettingsSchemaSource) src = NULL; // просто чтобы GLib не ворчал
    GSettings *iface = g_settings_new("org.gnome.desktop.interface");
    if (!iface) {
        fprintf(stderr, "failed to open GSettings org.gnome.desktop.interface\n");
        return 1;
    }

    CursorCtl cursor = {0};
    cursor.iface = iface;
    cursor.original_size = g_settings_get_int(iface, "cursor-size");
    if (cursor.original_size <= 0) cursor.original_size = 24;
    cursor.last_applied = cursor.original_size;
    cursor.enlarged_until = 0.0;

    // input
    GPtrArray *devs = NULL;
    if (force_device) {
        devs = g_ptr_array_new_with_free_func(NULL);
        Dev *d = open_one_device(force_device);
        if (!d) {
            fprintf(stderr, "failed to open %s: %s\n", force_device, strerror(errno));
            return 1;
        }
        g_ptr_array_add(devs, d);
        fprintf(stderr, "using device %s (%s) %s\n", force_device, d->name,
                d->use_rel ? "[REL]" : "[ABS]");
    } else {
        devs = open_mouse_like_devices();
        if (devs->len == 0) {
            fprintf(stderr, "no pointer-like devices with REL_X or ABS_X found\n");
            return 1;
        }
        for (guint i = 0; i < devs->len; i++) {
            Dev *d = devs->pdata[i];
            fprintf(stderr, "found device idx=%u %s %s\n",
                    i, d->name[0] ? d->name : "(noname)", d->use_rel ? "[REL]" : "[ABS]");
        }
    }

    // epoll
    int ep = epoll_create1(EPOLL_CLOEXEC);
    if (ep < 0) { perror("epoll_create1"); return 1; }

    for (guint i = 0; i < devs->len; i++) {
        Dev *d = devs->pdata[i];
        struct epoll_event ev = {0};
        ev.events = EPOLLIN;
        ev.data.u32 = i;
        if (epoll_ctl(ep, EPOLL_CTL_ADD, d->fd, &ev) < 0) {
            perror("epoll_ctl add");
        }
    }

    ShakeDetector sd; sd_init(&sd);

    // таймер проверки
    double next_tick = now_monotonic_sec() + (TICK_MS/1000.0);

    // цикл
    while (1) {
        int timeout_ms = 10;
        double now = now_monotonic_sec();
        double to_tick = next_tick - now;
        if (to_tick > 0) timeout_ms = (int)(to_tick * 1000.0);
        if (timeout_ms < 0) timeout_ms = 0;

        struct epoll_event evs[16];
        int n = epoll_wait(ep, evs, 16, timeout_ms);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        // читаем события
        for (int k = 0; k < n; k++) {
            guint idx = evs[k].data.u32;
            if (idx >= devs->len) continue;
            Dev *d = devs->pdata[idx];

            struct input_event e;
            for (;;) {
                int rc = libevdev_next_event(d->dev, LIBEVDEV_READ_FLAG_NORMAL, &e);
                if (rc == -EAGAIN) break;
                if (rc < 0) break;

                if (d->use_rel) {
                    if (e.type == EV_REL && e.code == REL_X) {
                        int dx = (int)e.value;
                        if (sd_feed_dx(&sd, dx)) {
                            cursor_enlarge(&cursor, ENLARGE_DURATION_S);
                        }
                    }
                } else {
                    // ABS-тачпад: считаем dx как разницу ABS_X
                    if (e.type == EV_ABS && e.code == ABS_X) {
                        int current_x = (int)e.value;
                        double t = now_monotonic_sec();

                        // Сброс базы после длинной паузы
                        if (d->have_last_abs_x && (t - d->last_abs_seen) > ABS_IDLE_RESET_S) {
                            d->have_last_abs_x = false;
                        }
                        d->last_abs_seen = t;

                        if (!d->have_last_abs_x) {
                            d->last_abs_x = current_x;
                            d->have_last_abs_x = true;
                        } else {
                            int dx = current_x - d->last_abs_x;
                            d->last_abs_x = current_x;
                            if (sd_feed_dx(&sd, dx)) {
                                cursor_enlarge(&cursor, ENLARGE_DURATION_S);
                            }
                        }
                    }
                    // При желании можно сбрасывать базу при отпускании пальца:
                    // if (e.type == EV_KEY && e.code == BTN_TOOL_FINGER && e.value == 0) d->have_last_abs_x = false;
                }
            }
        }

        // тик
        now = now_monotonic_sec();
        if (now >= next_tick) {
            cursor_tick(&cursor);
            next_tick = now + (TICK_MS/1000.0);
        }
    }

    // cleanup (обычно не дойдём)
    for (guint i = 0; i < devs->len; i++) {
        Dev *d = devs->pdata[i];
        if (d) {
            libevdev_free(d->dev);
            close(d->fd);
            g_free(d);
        }
    }
    g_ptr_array_free(devs, TRUE);

    // вернём размер курсора, если меняли
    if (cursor.last_applied != cursor.original_size) {
        g_settings_set_int(cursor.iface, "cursor-size", cursor.original_size);
    }
    return 0;
}
