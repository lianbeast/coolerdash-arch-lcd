#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/device/config.h"
#include "../src/mods/arch.h"
#include "../src/mods/display.h"
#include "../src/srv/cc_conf.h"
#include "../src/srv/cc_main.h"

int verbose_logging = 0;

static void self_check(Config config)
{
    monitor_sensor_data_t data = {0};
    populate_display_preview_data(&config, &data);
    for (int i = 0; i < ARCH_CHANNEL_COUNT; i++)
        assert(arch_metric(&config, &data, i));
    assert(arch_metric(&config, &data, ARCH_PUMP_RPM)->value == 2097);
    assert(arch_metric(&config, &data, ARCH_FAN_RPM)->value == 1724);
    assert(arch_metric(&config, &data, ARCH_GPU_LOAD)->value == 76);
    sensor_entry_t *pump = (sensor_entry_t *)arch_metric(&config, &data, ARCH_PUMP_RPM);
    SAFE_STRCPY(pump->device_uid, "unrelated-cooler");
    assert(!arch_metric(&config, &data, ARCH_PUMP_RPM));
    sensor_entry_t *load = (sensor_entry_t *)arch_metric(&config, &data, ARCH_GPU_LOAD);
    load->value = NAN;
    assert(!arch_metric(&config, &data, ARCH_GPU_LOAD));
    load->value = 101;
    assert(!arch_metric(&config, &data, ARCH_GPU_LOAD));
    load->value = 0;
    assert(arch_metric(&config, &data, ARCH_GPU_LOAD));
    SAFE_STRCPY(config.arch_channels[ARCH_GPU_LOAD], "nonexistent");
    assert(!arch_metric(&config, &data, ARCH_GPU_LOAD));
    SAFE_STRCPY(config.arch_page, "rotate");
    config.arch_page_seconds = 12;
    config.arch_reduce_motion = 0;
    for (int i = 0; i < 5; i++)
    {
        assert(arch_page_at(&config, i * 12) == i);
        assert(arch_page_at(&config, i * 12 + 11.99) == i);
    }
    assert(arch_page_at(&config, 60) == 0);
    config.arch_reduce_motion = 1;
    assert(arch_page_at(&config, 24) == 0);
    SAFE_STRCPY(config.arch_page, "liquid");
    assert(arch_page_at(&config, 0) == 3);
    assert(arch_page_at(&config, 999) == 3);
    puts("PASS: all 12 channels, device isolation, invalid readings, carousel boundaries, reduced motion");
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s CONFIG OUTPUT [--missing] [--live] [--page NAME] [--at SECONDS] [--size PIXELS]\n", argv[0]);
        return 2;
    }

    Config config = {0};
    if (!load_plugin_config_read_only(&config, argv[1]))
        return 3;
    if (strcmp(argv[2], "--check") == 0)
    {
        self_check(config);
        return 0;
    }
    if (strcmp(argv[2], "--sensors") == 0)
    {
        monitor_sensor_data_t data = {0};
        if (!init_coolercontrol_session(&config) || !init_device_cache(&config) ||
            !get_sensor_monitor_data(&config, &data))
            return 4;
        for (int i = 0; i < data.sensor_count; i++)
        {
            const sensor_entry_t *s = &data.sensors[i];
            if (strcmp(s->device_type, "CPU") == 0 ||
                strcmp(s->device_type, "GPU") == 0 ||
                strstr(s->device_name, "Kraken"))
                printf("%s | %s | %s | category=%d | %.2f %s\n",
                       s->device_type, s->device_name, s->name,
                       s->category, s->value, s->unit);
        }
        return 0;
    }
    cc_safe_strcpy(config.paths_image_coolerdash,
                   sizeof(config.paths_image_coolerdash), argv[2]);
    config.preview_render = 1;
    monitor_sensor_data_t data = {0};
    double seconds = 0;
    int missing = 0, live = 0;
    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "--missing") == 0)
            missing = 1;
        else if (strcmp(argv[i], "--live") == 0)
            live = 1;
        else if (strcmp(argv[i], "--page") == 0 && i + 1 < argc)
            SAFE_STRCPY(config.arch_page, argv[++i]);
        else if (strcmp(argv[i], "--at") == 0 && i + 1 < argc)
        {
            char *end = NULL;
            seconds = strtod(argv[++i], &end);
            if (!isfinite(seconds) || seconds < 0 || *end || end == argv[i]) return 2;
        }
        else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc)
        {
            char *end = NULL;
            long size = strtol(argv[++i], &end, 10);
            if (*end || size < 160 || size > 1024) return 2;
            config.display_width = config.display_height = (uint16_t)size;
        }
        else
            return 2;
    }
    if (live)
    {
        if (!init_coolercontrol_session(&config) || !init_device_cache(&config) ||
            !get_sensor_monitor_data(&config, &data)) return 4;
        (void)update_config_from_device(&config);
        if (!config.display_width || !config.display_height) return 4;
        for (int i = 0; i < ARCH_CHANNEL_COUNT; i++)
        {
            const sensor_entry_t *s = arch_metric(&config, &data, i);
            printf("%s: %s\n", arch_channel_keys[i], s ? "MATCH" : "NO DATA");
            if (!s) return 5;
        }
    }
    else if (!missing)
        populate_display_preview_data(&config, &data);
    return render_arch_frame(&config, &data, "NZXT Kraken Z", seconds) ? 0 : 4;
}
