#define _POSIX_C_SOURCE 200809L

#include <cairo/cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../srv/cc_conf.h"
#include "../srv/cc_main.h"
#include "arch.h"
#include "display.h"

/* Primitive type scale -> semantic roles -> shared page components (320px units). */
enum { TYPE_SM = 12, TYPE_MD = 15, TYPE_LG = 21, TYPE_XL = 28, TYPE_HERO = 42 };
static const double BRAND_SIZE = TYPE_LG, CLOCK_SIZE = TYPE_XL;
static const double LABEL_SIZE = TYPE_SM, DATE_SIZE = TYPE_MD;
static const double METRIC_SIZE = TYPE_LG, HERO_SIZE = TYPE_HERO;
static const double PAGE_CENTER = 160, FRAME_RADIUS = 150;
static const double MOTION_OPACITY_MIN = 0.35, MOTION_OPACITY_MAX = 0.85;
static const char *const PAGES[] = {"overview", "cpu", "gpu", "liquid", "power"};

int arch_page_at(const Config *config, double seconds)
{
    for (int i = 0; i < 5; i++)
        if (strcmp(config->arch_page, PAGES[i]) == 0)
            return i;
    if (strcmp(config->arch_page, "rotate") != 0 || config->arch_reduce_motion ||
        !isfinite(seconds) || seconds < 0)
        return 0;
    double dwell = config->arch_page_seconds >= 5 ? config->arch_page_seconds : 12;
    return (int)fmod(floor(seconds / dwell), 5);
}

const sensor_entry_t *arch_metric(const Config *config,
                                 const monitor_sensor_data_t *data, int metric)
{
    static const sensor_category_t categories[] = {
        SENSOR_CATEGORY_FREQ, SENSOR_CATEGORY_DUTY, SENSOR_CATEGORY_WATTS, SENSOR_CATEGORY_FREQ,
        SENSOR_CATEGORY_FREQ, SENSOR_CATEGORY_DUTY, SENSOR_CATEGORY_WATTS, SENSOR_CATEGORY_RPM,
        SENSOR_CATEGORY_RPM, SENSOR_CATEGORY_RPM, SENSOR_CATEGORY_DUTY, SENSOR_CATEGORY_DUTY
    };
    if (!config || !data || metric < 0 || metric >= ARCH_CHANNEL_COUNT)
        return NULL;
    const char *slot = metric < ARCH_GPU_SPEED ? config->sensor_slot_1 :
                       metric < ARCH_PUMP_RPM ? config->sensor_slot_2 : config->sensor_slot_3;
    const sensor_entry_t *base = find_sensor_for_slot(data, slot);
    if (!base || base->category != SENSOR_CATEGORY_TEMP)
        return NULL;
    /* Exact name + category + selected device: never substitute fan duty for load. */
    for (int i = 0; i < data->sensor_count; i++)
    {
        const sensor_entry_t *s = &data->sensors[i];
        if (s->category == categories[metric] &&
            strcmp(s->device_uid, base->device_uid) == 0 &&
            strcmp(s->name, config->arch_channels[metric]) == 0 &&
            isfinite(s->value) && s->value >= 0 &&
            (s->category != SENSOR_CATEGORY_DUTY || s->value <= 100))
            return s;
    }
    return NULL;
}

static int valid_temperature(const sensor_entry_t *entry)
{
    return entry && entry->category == SENSOR_CATEGORY_TEMP &&
           isfinite(entry->value) && entry->value >= -50 && entry->value <= 200;
}

static void text(cairo_t *cr, const char *value, double x, double y,
                 double size, double width, double height, const Color *color)
{
    cairo_text_extents_t ext;
    cairo_set_font_size(cr, fit_text_font_size(cr, value, size, width, height, 1));
    cairo_text_extents(cr, value, &ext);
    set_cairo_color(cr, color);
    cairo_move_to(cr, x - ext.width / 2 - ext.x_bearing,
                  y - ext.height / 2 - ext.y_bearing);
    cairo_show_text(cr, value);
}

/* icon-device-{cpu,gpu,coolant}-outline: 24px grid, 1.5px round stroke. */
static void sensor_icon(cairo_t *cr, int kind, double x, double y, const Color *color)
{
    cairo_save(cr);
    cairo_translate(cr, x - 12, y - 12);
    set_cairo_color(cr, color);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    if (kind == 0)
    {
        cairo_rectangle(cr, 6, 6, 12, 12);
        cairo_rectangle(cr, 9, 9, 6, 6);
        for (int p = 8; p <= 16; p += 4)
        {
            cairo_move_to(cr, p, 3); cairo_line_to(cr, p, 6);
            cairo_move_to(cr, p, 18); cairo_line_to(cr, p, 21);
            cairo_move_to(cr, 3, p); cairo_line_to(cr, 6, p);
            cairo_move_to(cr, 18, p); cairo_line_to(cr, 21, p);
        }
    }
    else if (kind == 1)
    {
        cairo_rectangle(cr, 3, 6, 18, 12);
        cairo_move_to(cr, 6, 18); cairo_line_to(cr, 6, 21);
        cairo_move_to(cr, 9, 18); cairo_line_to(cr, 9, 20);
        cairo_move_to(cr, 12, 18); cairo_line_to(cr, 12, 20);
        cairo_move_to(cr, 16, 12);
        cairo_arc(cr, 12, 12, 4, 0, 2 * DISPLAY_M_PI);
        cairo_move_to(cr, 10, 10); cairo_line_to(cr, 14, 14);
        cairo_move_to(cr, 14, 10); cairo_line_to(cr, 10, 14);
    }
    else
    {
        cairo_move_to(cr, 12, 2);
        cairo_curve_to(cr, 10, 6, 5, 11, 5, 15);
        cairo_curve_to(cr, 5, 24, 19, 24, 19, 15);
        cairo_curve_to(cr, 19, 11, 14, 6, 12, 2);
        cairo_close_path(cr);
        cairo_move_to(cr, 9, 15);
        cairo_curve_to(cr, 9, 18, 10, 19, 12, 19);
    }
    cairo_stroke(cr);
    cairo_restore(cr);
}

static void metric_text(cairo_t *cr, const sensor_entry_t *entry, const char *label,
                        double x, double y, const Color *white, const Color *muted)
{
    char value[48] = "--";
    if (entry)
    {
        if (entry->category == SENSOR_CATEGORY_FREQ)
            snprintf(value, sizeof(value), "%.2f GHz", entry->value / 1000);
        else
            snprintf(value, sizeof(value), "%.0f %s", entry->value, entry->unit);
    }
    text(cr, label, x, y, LABEL_SIZE, 112, 14, muted);
    text(cr, value, x, y + 20, METRIC_SIZE, 112, 24, white);
}

static void detail_page(cairo_t *cr, const Config *config,
                        const monitor_sensor_data_t *data, int page,
                        const Color *white, const Color *muted, const Color *accent)
{
    const char *slots[] = {config->sensor_slot_1, config->sensor_slot_2, config->sensor_slot_3};
    const char *titles[] = {"CPU TEMPERATURE", "GPU TEMPERATURE", "LIQUID TEMPERATURE", "CPU + GPU POWER"};
    const char *labels[][4] = {
        {"AVG SPEED", "USAGE", "POWER DRAW", "MAX SPEED"},
        {"CORE SPEED", "USAGE", "POWER DRAW", "FAN SPEED"},
        {"PUMP RPM", "FAN RPM", "PUMP DUTY", "FAN DUTY"},
        {"CPU DRAW", "GPU DRAW", "CPU USAGE", "GPU USAGE"}
    };
    const int metrics[][4] = {
        {ARCH_CPU_SPEED, ARCH_CPU_LOAD, ARCH_CPU_POWER, ARCH_CPU_PEAK},
        {ARCH_GPU_SPEED, ARCH_GPU_LOAD, ARCH_GPU_POWER, ARCH_GPU_FAN},
        {ARCH_PUMP_RPM, ARCH_FAN_RPM, ARCH_PUMP_DUTY, ARCH_FAN_DUTY},
        {ARCH_CPU_POWER, ARCH_GPU_POWER, ARCH_CPU_LOAD, ARCH_GPU_LOAD}
    };
    char value[48] = "--";
    Color state = *white;
    if (page == 4)
    {
        const sensor_entry_t *cpu = arch_metric(config, data, ARCH_CPU_POWER);
        const sensor_entry_t *gpu = arch_metric(config, data, ARCH_GPU_POWER);
        if (cpu && gpu)
            snprintf(value, sizeof(value), "%.0f W", cpu->value + gpu->value);
    }
    else
    {
        const sensor_entry_t *entry = find_sensor_for_slot(data, slots[page - 1]);
        if (valid_temperature(entry))
        {
            snprintf(value, sizeof(value), page == 3 ? "%.1f\xC2\xB0" : "%.0f\xC2\xB0", entry->value);
            state = get_slot_bar_color(config, slots[page - 1], entry->value);
        }
    }
    text(cr, titles[page - 1], PAGE_CENTER, 104, LABEL_SIZE, 220, 14, muted);
    text(cr, value, PAGE_CENTER, 139, HERO_SIZE, 220, 44, white);
    set_cairo_color_alpha(cr, &state, 0.8);
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, 144, 166); cairo_line_to(cr, 176, 166); cairo_stroke(cr);
    for (int i = 0; i < 4; i++)
        metric_text(cr, arch_metric(config, data, metrics[page - 1][i]),
                    labels[page - 1][i], i % 2 ? 224 : 96, i < 2 ? 184 : 233,
                    white, muted);
    set_cairo_color_alpha(cr, accent, 0.22);
    cairo_set_line_width(cr, 1);
    cairo_move_to(cr, 160, 183); cairo_line_to(cr, 160, 258); cairo_stroke(cr);
    cairo_move_to(cr, 88, 272); cairo_line_to(cr, 232, 272); cairo_stroke(cr);
}

int render_arch_frame(const Config *config, const monitor_sensor_data_t *data,
                      const char *device_name, double seconds)
{
    if (!config || !data || !config->display_width || !config->display_height)
        return 0;
    cairo_surface_t *surface = NULL;
    cairo_t *cr = create_cairo_context(config, &surface);
    if (!cr)
        return 0;
    ScalingParams params = {0};
    calculate_scaling_params(config, &params, device_name);
    paint_display_background(cr, config);

    /* Uniform 320px composition keeps every text box inside the circular panel. */
    double safe_scale = config->display_content_scale_factor;
    if (safe_scale <= 0 || safe_scale > 1)
        safe_scale = 0.94;
    double scale = fmin(config->display_width, config->display_height) / 320.0 *
                   safe_scale / 0.94;
    cairo_translate(cr, config->display_width / 2.0, config->display_height / 2.0);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -160, -160);
    const Color *accent = &config->layout_bar_color_border;
    const Color *white = &config->font_color_temp;
    const Color *muted = &config->font_color_label;
    cairo_pattern_t *wash = cairo_pattern_create_radial(160, 80, 0, 160, 110, 170);
    cairo_pattern_add_color_stop_rgba(wash, 0, accent->r / 255.0,
                                      accent->g / 255.0, accent->b / 255.0, 0.09);
    cairo_pattern_add_color_stop_rgba(wash, 1, 0, 0, 0, 0);
    cairo_set_source(cr, wash);
    cairo_paint(cr);
    cairo_pattern_destroy(wash);

    double cycle = config->arch_motion_seconds >= 10 ? config->arch_motion_seconds : 40;
    double phase = config->arch_reduce_motion || !isfinite(seconds) || seconds < 0 ? 0 :
                   fmod(seconds, cycle) / cycle;
    int page = arch_page_at(config, seconds);
    double glow = config->arch_reduce_motion ? 0.7 :
        MOTION_OPACITY_MIN + (MOTION_OPACITY_MAX - MOTION_OPACITY_MIN) *
        (0.5 - 0.5 * cos(phase * 2 * DISPLAY_M_PI));
    cairo_set_line_width(cr, 1);
    set_cairo_color_alpha(cr, accent, 0.18);
    cairo_arc(cr, PAGE_CENTER, PAGE_CENTER, FRAME_RADIUS, 0, 2 * DISPLAY_M_PI);
    cairo_stroke(cr);
    for (int i = 0; i < 3; i++)
    {
        double start = (-150 + 120 * i) * DISPLAY_M_PI / 180;
        cairo_set_line_width(cr, 2);
        set_cairo_color_alpha(cr, accent, glow);
        cairo_arc(cr, 160, 160, 146, start, start + 28 * DISPLAY_M_PI / 180);
        cairo_stroke(cr);
    }
    /* Slow orbital light, not a progress gauge. Data never moves or fades. */
    for (int i = 0; i < 20; i++)
    {
        double angle = (phase * 360 - 90 - i * 2) * DISPLAY_M_PI / 180;
        set_cairo_color_alpha(cr, accent, 0.8 * (1.0 - i / 20.0));
        cairo_set_line_width(cr, 3);
        cairo_arc(cr, 160, 160, 150, angle, angle + 2.1 * DISPLAY_M_PI / 180);
        cairo_stroke(cr);
    }

    cairo_select_font_face(cr, "JetBrainsMono Nerd Font", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    text(cr, "\xF3\xB0\xA3\x87", 160, 37, 34, 40, 34, accent);
    cairo_select_font_face(cr, config->font_face, CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    text(cr, "- Arch Linux -", 160, 73, BRAND_SIZE, 228, 25, white);

    const char *slots[] = {config->sensor_slot_1, config->sensor_slot_2,
                           config->sensor_slot_3};
    const char *labels[] = {"CPU", "GPU", "COOLANT"};
    if (page != 0)
        detail_page(cr, config, data, page, white, muted, accent);
    else for (int i = 0; i < 3; i++)
    {
        double x = 76 + 84 * i;
        sensor_icon(cr, i, x, 105, accent);
        text(cr, labels[i], x, 130, 12, 76, 15, muted);
        const sensor_entry_t *entry = find_sensor_for_slot(data, slots[i]);
        int valid = valid_temperature(entry);
        const SensorConfig *sc = get_sensor_config(config, slots[i]);
        char value[32] = "--";
        const char *status = "NO DATA";
        Color state = *muted;
        if (valid)
        {
            snprintf(value, sizeof(value), "%.0f\xC2\xB0", entry->value);
            state = get_slot_bar_color(config, slots[i], entry->value);
            if (sc)
                status = entry->value < sc->threshold_1 ? "COOL" :
                         entry->value < sc->threshold_2 ? "WARM" :
                         entry->value < sc->threshold_3 ? "HOT" : "HIGH";
            else
                status = "LIVE";
        }
        text(cr, value, x, 163, 36, 76, 40, white);
        text(cr, status, x, 193, 10, 72, 13, &state);
        if (i < 2)
        {
            set_cairo_color_alpha(cr, accent, 0.22);
            cairo_set_line_width(cr, 1);
            cairo_move_to(cr, x + 42, 120);
            cairo_line_to(cr, x + 42, 198);
            cairo_stroke(cr);
        }
    }
    if (page == 0)
    {
        set_cairo_color_alpha(cr, accent, 0.35);
        cairo_set_line_width(cr, 1);
        cairo_move_to(cr, 64, 241); cairo_line_to(cr, 256, 241);
        cairo_stroke(cr);
        set_cairo_color_alpha(cr, accent, 0.25);
        cairo_move_to(cr, 76, 213); cairo_line_to(cr, 160, 225);
        cairo_line_to(cr, 244, 213); cairo_stroke(cr);
        for (int i = 0; i < 3; i++)
        {
            double pulse = config->arch_reduce_motion ? 0.6 :
                0.5 + 0.3 * cos(phase * 4 * DISPLAY_M_PI - i * 1.2);
            set_cairo_color_alpha(cr, accent, pulse);
            cairo_arc(cr, 76 + 84 * i, i == 1 ? 225 : 213, 2.5, 0, 2 * DISPLAY_M_PI);
            cairo_fill(cr);
        }
    }

    if (page == 0)
    {
        time_t now = time(NULL);
        struct tm local = {0};
        char time_text[40] = "--:--";
        char date_text[40] = "--";
        static const char *days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        static const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        if (localtime_r(&now, &local))
        {
            snprintf(time_text, sizeof(time_text), "%02d:%02d %s",
                     local.tm_hour % 12 ? local.tm_hour % 12 : 12, local.tm_min,
                     local.tm_hour < 12 ? "AM" : "PM");
            snprintf(date_text, sizeof(date_text), "%s: %s %02d",
                     days[local.tm_wday], months[local.tm_mon], local.tm_mday);
        }
        text(cr, time_text, 160, 262, CLOCK_SIZE, 220, 30, white);
        text(cr, date_text, 160, 288, DATE_SIZE, 174, 18, muted);
    }
    for (int i = 0; i < 5; i++)
    {
        set_cairo_color_alpha(cr, i == page ? white : accent, i == page ? 1 : 0.4);
        cairo_arc(cr, 140 + i * 10, page == 0 ? 306 : 291,
                  i == page ? 2.2 : 1.5, 0, 2 * DISPLAY_M_PI);
        cairo_fill(cr);
    }
    cairo_surface_flush(surface);
    int success = cairo_status(cr) == CAIRO_STATUS_SUCCESS &&
        write_display_png(surface, config, config->paths_image_coolerdash) ==
            CAIRO_STATUS_SUCCESS;
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return success;
}

int render_arch_preview(const Config *config, const monitor_sensor_data_t *data,
                        const char *device_name)
{
    return render_arch_frame(config, data, device_name, 0);
}

void draw_arch_image(const Config *config)
{
    if (!config)
        return;
    monitor_sensor_data_t data = {0};
    if (!get_sensor_monitor_data(config, &data))
        memset(&data, 0, sizeof(data));
    char uid[128] = {0};
    char name[256] = {0};
    int available = get_cached_lcd_device_data(
        config, uid, sizeof(uid), name, sizeof(name), NULL, NULL);
    static double start = -1;
    struct timespec tick = {0};
    clock_gettime(CLOCK_MONOTONIC, &tick);
    double now = tick.tv_sec + tick.tv_nsec / 1e9;
    if (start < 0) start = now;
    if (render_arch_frame(config, &data, name, now - start) &&
        is_session_initialized() && available && uid[0])
        send_image_to_lcd(config, config->paths_image_coolerdash, uid);
}
