#ifndef ARCH_MODE_H
#define ARCH_MODE_H

#include "../device/config.h"
#include "../srv/cc_sensor.h"

int render_arch_preview(const struct Config *config,
                        const monitor_sensor_data_t *data,
                        const char *device_name);
void draw_arch_image(const struct Config *config);
int arch_page_at(const Config *config, double seconds);
const sensor_entry_t *arch_metric(const Config *config,
                                 const monitor_sensor_data_t *data, int metric);
int render_arch_frame(const Config *config, const monitor_sensor_data_t *data,
                      const char *device_name, double seconds);

#endif
