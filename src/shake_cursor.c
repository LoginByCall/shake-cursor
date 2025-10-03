// gcc -O2 -Wall -Wextra -o shake-cursor shake_cursor.c $(pkg-config --cflags --libs libevdev gio-2.0 glib-2.0 gtk+-3.0 libayatana-appindicator3-0.1)
// Ubuntu 24+: трей через Ayatana AppIndicator. Настройки - окно GTK.
// Доступ к /dev/input/event*: добавьте пользователя в группу input и перелогиньтесь.

#define _GNU_SOURCE
#include <linux/input.h>
#include <glib-unix.h>
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

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libevdev/libevdev.h>
#include <libayatana-appindicator/app-indicator.h>

// ===================== Параметры (дефолты) =====================
static int    P_BIG_CURSOR_SIZE   = 256;   // [1..256]
static double P_ENLARGE_DUR_S     = 1.5;   // [1..5]
static int    P_MIN_DX            = 20;    // [5..40]  (мышь, REL_X)
static int    P_MIN_DX_ABS        = 120;   // [20..600] (тачпад, ABS_X)
static double P_MAX_INTERVAL_S    = 0.12;  // [0.03..0.3]
static int    P_REQUIRED_FLIPS    = 3;     // [1..5]
static double P_WINDOW_S          = 0.7;   // окно учёта смен
static int    P_TICK_MS           = 100;   // [50..300]
// ===============================================================

#define APP_ID "shake-cursor"
#define CONF_DIR  "shake-cursor"
#define CONF_FILE "config.ini"

typedef struct {
    struct libevdev *dev;
    int fd;
    guint source_id; // GLib fd-source id
    bool has_rel;
    bool has_abs;
    int  last_abs_x; // для ABS_X
    bool have_last_abs;
} Dev;

typedef struct {
    int last_sign;
    double last_flip_time;
    double flips[16];
    int flips_count;
} ShakeDetector;

typedef struct {
    GSettings *iface;      // org.gnome.desktop.interface
    int original_size;
    int last_applied;
    double enlarged_until; // монотонное время, когда вернуть размер
} CursorCtl;

typedef struct {
    GtkWidget *win;        // окно настроек
    GtkWidget *sp_big;
    GtkWidget *sp_dur;
    GtkWidget *sp_mindx;
    GtkWidget *sp_mindx_abs;
    GtkWidget *sp_maxint;
    GtkWidget *sp_flips;
    GtkWidget *sp_tick;
} PrefsUI;

static GMainLoop *gloop = NULL;
static GPtrArray *g_devs = NULL;
static ShakeDetector g_sd;
static CursorCtl g_cursor;
static PrefsUI g_prefs = {0};
static AppIndicator *g_indicator = NULL;
static guint g_tick_id = 0;

// ---- время ----
static double now_monotonic_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

// ---- конфиг ----
static gchar* conf_path(void) {
    gchar *dir = g_build_filename(g_get_user_config_dir(), CONF_DIR, NULL);
    g_mkdir_with_parents(dir, 0700);
    gchar *path = g_build_filename(dir, CONF_FILE, NULL);
    g_free(dir);
    return path;
}

static void conf_load(void) {
    GKeyFile *kf = g_key_file_new();
    g_autofree gchar *path = conf_path();
    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        if (g_key_file_has_key(kf, "main", "big_cursor", NULL))
            P_BIG_CURSOR_SIZE = g_key_file_get_integer(kf, "main", "big_cursor", NULL);
        if (g_key_file_has_key(kf, "main", "enlarge_dur_s", NULL))
            P_ENLARGE_DUR_S   = g_key_file_get_double (kf, "main", "enlarge_dur_s", NULL);
        if (g_key_file_has_key(kf, "main", "min_dx", NULL))
            P_MIN_DX          = g_key_file_get_integer(kf, "main", "min_dx", NULL);
        if (g_key_file_has_key(kf, "main", "min_dx_abs", NULL))
            P_MIN_DX_ABS      = g_key_file_get_integer(kf, "main", "min_dx_abs", NULL);
        if (g_key_file_has_key(kf, "main", "max_interval_s", NULL))
            P_MAX_INTERVAL_S  = g_key_file_get_double (kf, "main", "max_interval_s", NULL);
        if (g_key_file_has_key(kf, "main", "required_flips", NULL))
            P_REQUIRED_FLIPS  = g_key_file_get_integer(kf, "main", "required_flips", NULL);
        if (g_key_file_has_key(kf, "main", "window_s", NULL))
            P_WINDOW_S        = g_key_file_get_double (kf, "main", "window_s", NULL);
        if (g_key_file_has_key(kf, "main", "tick_ms", NULL))
            P_TICK_MS         = g_key_file_get_integer(kf, "main", "tick_ms", NULL);
    }
    g_key_file_unref(kf);
}

static void conf_save(void) {
    GKeyFile *kf = g_key_file_new();
    g_key_file_set_integer(kf, "main", "big_cursor", P_BIG_CURSOR_SIZE);
    g_key_file_set_double (kf, "main", "enlarge_dur_s", P_ENLARGE_DUR_S);
    g_key_file_set_integer(kf, "main", "min_dx", P_MIN_DX);
    g_key_file_set_integer(kf, "main", "min_dx_abs", P_MIN_DX_ABS);
    g_key_file_set_double (kf, "main", "max_interval_s", P_MAX_INTERVAL_S);
    g_key_file_set_integer(kf, "main", "required_flips", P_REQUIRED_FLIPS);
    g_key_file_set_double (kf, "main", "window_s", P_WINDOW_S);
    g_key_file_set_integer(kf, "main", "tick_ms", P_TICK_MS);

    gsize len = 0;
    g_autofree gchar *data = g_key_file_to_data(kf, &len, NULL);
    g_autofree gchar *path = conf_path();
    g_file_set_contents(path, data, len, NULL);
    g_key_file_unref(kf);
}

// ---- GSettings cursor ----
static void cursor_force_reload_theme(GSettings *iface) {
    // Пересоздаём курсор: быстрое переключение темы на альтернативную и обратно
    gchar *current = g_settings_get_string(iface, "cursor-theme");
    if (!current) return;

    const char *alts[] = {"Adwaita", "Yaru", "DMZ-White", "DMZ-Black"};
    const char *alt = NULL;
    for (size_t i = 0; i < G_N_ELEMENTS(alts); i++) {
        if (g_strcmp0(current, alts[i]) != 0) { alt = alts[i]; break; }
    }
    if (alt) {
        g_settings_set_string(iface, "cursor-theme", alt);
        g_usleep(30 * 1000);
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
    cursor_set_size(c, P_BIG_CURSOR_SIZE);
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
        if (now - sd->flips[i] <= P_WINDOW_S) sd->flips[w++] = sd->flips[i];
    }
    sd->flips_count = w;
}

static bool sd_feed_dx_with_threshold(ShakeDetector *sd, int dx, int min_dx_threshold) {
    if (dx > -min_dx_threshold && dx < min_dx_threshold) return false;
    int sign = dx > 0 ? 1 : -1;
    double t = now_monotonic_sec();

    if (sd->last_sign != 0 && sign != sd->last_sign) {
        if (t - sd->last_flip_time <= P_MAX_INTERVAL_S) {
            if (sd->flips_count < (int)(sizeof(sd->flips)/sizeof(sd->flips[0])))
                sd->flips[sd->flips_count++] = t;
            sd_prune_window(sd, t);
            if (sd->flips_count >= P_REQUIRED_FLIPS) {
                sd->flips_count = 0;
                sd->last_sign = sign;
                sd->last_flip_time = t;
                return true; // SHAKE!
            }
        }
    }
    sd->last_sign = sign;
    sd->last_flip_time = t;
    return false;
}

// ---- input discovery & GLib IO watch ----
static gboolean on_fd_ready(gint fd, GIOCondition cond, gpointer user_data) {
    Dev *d = (Dev*)user_data;
    if (!(cond & G_IO_IN)) return TRUE;

    struct input_event e;
    int rc;
    while ((rc = libevdev_next_event(d->dev, LIBEVDEV_READ_FLAG_NORMAL, &e)) == 0) {
        if (e.type == EV_REL && e.code == REL_X && d->has_rel) {
            int dx = (int)e.value;
            if (sd_feed_dx_with_threshold(&g_sd, dx, P_MIN_DX)) {
                cursor_enlarge(&g_cursor, P_ENLARGE_DUR_S);
            }
        } else if (e.type == EV_ABS && e.code == ABS_X && d->has_abs) {
            int cur = (int)e.value;
            if (d->have_last_abs) {
                int dx = cur - d->last_abs_x;
                if (sd_feed_dx_with_threshold(&g_sd, dx, P_MIN_DX_ABS)) {
                    cursor_enlarge(&g_cursor, P_ENLARGE_DUR_S);
                }
            }
            d->last_abs_x = cur;
            d->have_last_abs = true;
        }
    }
    return TRUE; // продолжать вызовы
}

static void devices_open_all(void) {
    g_devs = g_ptr_array_new_with_free_func(g_free);
    DIR *dir = opendir("/dev/input");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (g_str_has_prefix(ent->d_name, "event")) {
            char path[256];
            g_snprintf(path, sizeof(path), "/dev/input/%s", ent->d_name);
            int fd = open(path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            struct libevdev *dev = NULL;
            if (libevdev_new_from_fd(fd, &dev) < 0) { close(fd); continue; }

            bool has_rel = libevdev_has_event_type(dev, EV_REL) && libevdev_has_event_code(dev, EV_REL, REL_X);
            bool has_abs = libevdev_has_event_type(dev, EV_ABS) && libevdev_has_event_code(dev, EV_ABS, ABS_X);

            if (has_rel || has_abs) {
                Dev *d = g_malloc0(sizeof(Dev));
                d->fd = fd;
                d->dev = dev;
                d->has_rel = has_rel;
                d->has_abs = has_abs;
                d->last_abs_x = 0;
                d->have_last_abs = false;
                d->source_id = g_unix_fd_add(fd, G_IO_IN, on_fd_ready, d);
                g_ptr_array_add(g_devs, d);
            } else {
                libevdev_free(dev);
                close(fd);
            }
        }
    }
    closedir(dir);
}

// ---- tick ----
static gboolean on_tick(gpointer user_data) {
    cursor_tick(&g_cursor);
    return TRUE; // keep
}

// ---- UI: preferences ----
static void prefs_apply_from_ui(void) {
    P_BIG_CURSOR_SIZE  = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_big));
    P_ENLARGE_DUR_S    = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_dur));
    P_MIN_DX           = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_mindx));
    P_MIN_DX_ABS       = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_mindx_abs));
    P_MAX_INTERVAL_S   = gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_maxint));
    P_REQUIRED_FLIPS   = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_flips));
    P_TICK_MS          = (int)gtk_spin_button_get_value(GTK_SPIN_BUTTON(g_prefs.sp_tick));

    if (g_tick_id) { g_source_remove(g_tick_id); }
    g_tick_id = g_timeout_add(P_TICK_MS, on_tick, NULL);

    conf_save();
}

static GtkWidget* make_row(const char *label, GtkWidget **out_spin, double val, double min, double max, double step, gboolean is_int) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *lab = gtk_label_new(label);
    gtk_widget_set_halign(lab, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), lab, TRUE, TRUE, 0);
    GtkAdjustment *adj = gtk_adjustment_new(val, min, max, step, step*2, 0);
    GtkWidget *spin = gtk_spin_button_new(adj, 1.0, is_int ? 0 : 2);
    gtk_widget_set_size_request(spin, 120, -1);
    gtk_box_pack_end(GTK_BOX(box), spin, FALSE, FALSE, 0);
    *out_spin = spin;
    return box;
}

static void prefs_show(GtkWidget *w, gpointer data) {
    if (g_prefs.win) { gtk_window_present(GTK_WINDOW(g_prefs.win)); return; }

    g_prefs.win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_prefs.win), "Shake Cursor — Настройки");
    gtk_window_set_default_size(GTK_WINDOW(g_prefs.win), 460, -1);

    GtkWidget *area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(area), 12);

    gtk_box_pack_start(GTK_BOX(area), make_row("Макс. размер курсора [1..256]", &g_prefs.sp_big, P_BIG_CURSOR_SIZE, 1, 256, 1, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Длительность удержания [1..5] (сек)", &g_prefs.sp_dur, P_ENLARGE_DUR_S, 1, 5, 0.1, FALSE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Порог мыши (REL) [5..40]", &g_prefs.sp_mindx, P_MIN_DX, 5, 40, 1, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Порог тачпада (ABS) [20..600]", &g_prefs.sp_mindx_abs, P_MIN_DX_ABS, 20, 600, 10, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Интервал смены направления [0.03..0.3] (сек)", &g_prefs.sp_maxint, P_MAX_INTERVAL_S, 0.03, 0.3, 0.01, FALSE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Число рывков [1..5]", &g_prefs.sp_flips, P_REQUIRED_FLIPS, 1, 5, 1, TRUE), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(area), make_row("Частота проверки [50..300] (мс)", &g_prefs.sp_tick, P_TICK_MS, 50, 300, 10, TRUE), FALSE, FALSE, 0);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *btn_apply = gtk_button_new_with_label("Применить");
    GtkWidget *btn_close = gtk_button_new_with_label("Закрыть");
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(prefs_apply_from_ui), NULL);
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_widget_destroy), g_prefs.win);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_close, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(btn_box), btn_apply, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(area), btn_box, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(g_prefs.win), area);
    g_signal_connect(g_prefs.win, "destroy", G_CALLBACK(gtk_widget_destroyed), &g_prefs.win);
    gtk_widget_show_all(g_prefs.win);
}

static void on_quit(GtkMenuItem *item, gpointer data) {
    if (gloop) g_main_loop_quit(gloop);
}

// ---- Indicator ----
static void indicator_init(void) {
    g_indicator = app_indicator_new(APP_ID, "input-mouse", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(g_indicator, APP_INDICATOR_STATUS_ACTIVE);

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *mi_prefs = gtk_menu_item_new_with_label("Настройки…");
    g_signal_connect(mi_prefs, "activate", G_CALLBACK(prefs_show), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_prefs);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *mi_quit = gtk_menu_item_new_with_label("Выход");
    g_signal_connect(mi_quit, "activate", G_CALLBACK(on_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mi_quit);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(g_indicator, GTK_MENU(menu));
}

// ---- main ----
int main(int argc, char **argv) {
    // Инициализация
    gtk_init(&argc, &argv);
    conf_load();

    // GSettings
    g_cursor.iface = g_settings_new("org.gnome.desktop.interface");
    if (!g_cursor.iface) {
        fprintf(stderr, "GSettings org.gnome.desktop.interface недоступен\n");
        return 1;
    }
    g_cursor.original_size = g_settings_get_int(g_cursor.iface, "cursor-size");
    if (g_cursor.original_size <= 0) g_cursor.original_size = 24;
    g_cursor.last_applied = g_cursor.original_size;
    g_cursor.enlarged_until = 0.0;

    // Устройства ввода
    devices_open_all();
    if (!g_devs || g_devs->len == 0) {
        fprintf(stderr, "Не найдено устройств с REL_X/ABS_X\n");
        return 1;
    }
    sd_init(&g_sd);

    // Индикатор и меню
    indicator_init();

    // Тикер
    g_tick_id = g_timeout_add(P_TICK_MS, on_tick, NULL);

    // Главный цикл
    gloop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(gloop);

    // Завершение
    if (g_tick_id) g_source_remove(g_tick_id);
    if (g_devs) {
        for (guint i = 0; i < g_devs->len; i++) {
            Dev *d = g_devs->pdata[i];
            if (!d) continue;
            if (d->source_id) g_source_remove(d->source_id);
            libevdev_free(d->dev);
            close(d->fd);
            g_free(d);
        }
        g_ptr_array_free(g_devs, TRUE);
    }
    // вернуть размер
    if (g_cursor.last_applied != g_cursor.original_size) {
        g_settings_set_int(g_cursor.iface, "cursor-size", g_cursor.original_size);
    }
    g_clear_object(&g_cursor.iface);
    return 0;
}

