# Display Modes

Four modes: **split** (default), **dual**, **circle**, and **arch**.

The three stock modes can add the optional **Sensor Ring** overlay. Split and Dual
can instead divide the ring between their two visible sensors. In Circle mode,
the normal ring follows the sensor currently shown on the display. In other
modes, `display.sensor_ring_sensor` selects its sensor and defaults to `liquid`.
The ring maps the current value to that sensor's configured maximum and uses
the same threshold colors as its bars and values. The overlay is off by default
for backwards-compatible output.

Mode selection: `config.json` → `"display": { "mode": "split" }` or CLI `--split`, `--dual`, `--circle`, or `--arch`.

## Arch Mode

Arch mode renders three columns from `sensor_slot_1`, `sensor_slot_2`, and
`sensor_slot_3` inside a uniformly scaled 320px composition. CPU, GPU and
coolant outline icons share a 24px grid and 1.5px stroke. Temperature readings
use whole degrees and status labels follow the configured sensor thresholds.
Missing/nonfinite readings show `--` and `NO DATA`.

`display.arch_page` selects `overview`, `cpu`, `gpu`, `liquid`, `power`, or
`rotate`. Rotation shows all five pages, with `arch_page_seconds` (5..120,
default 12) per page. Missing setting preserves the original fixed overview.
CPU shows average/max clock, usage, and power. GPU shows core clock, usage,
power, and fan RPM. Liquid shows temperature, separate pump/fan RPM, and duties.
Power sums only CPU and GPU readings, never whole-system or wall power.

Metric mappings are editable under Display > Arch metric mappings. The twelve
`display.arch_cpu_*`, `arch_gpu_*`, `arch_pump_*`, and `arch_fan_*` fields use
exact CoolerDash channel names. Each is matched by category and the device of
the corresponding temperature slot. Default names were verified against the
local Ryzen 5950X, Radeon GPU, and Kraken Z. No fallback to unrelated devices.
The `--sensors` preview-helper option lists names without device IDs or secrets.

The orbital light uses `arch_motion_seconds` (10..120, default 40). The supplied
theme targets 0.5-second uploads; no 60fps or GIF playback claim is made.
`arch_reduce_motion: true` freezes decoration and pins rotation to Overview;
fixed-page selections remain honored. Clock and telemetry stay live. Browser
CSS preferences cannot control a native LCD daemon.

Shared type tokens in `arch.c` map a primitive scale to brand (21), clock (28),
date (15), metric label (12), metric value (21), and hero (42) roles. Colors use
existing semantic config keys: `display_background`, `font_temp`, `font_label`,
and `bar_border` (accent). Brand glyph is 34px; telemetry icons remain 24px.
All dimensions are logical 320px units scaled uniformly for the detected LCD.
Text fits bounded boxes; the live clock/date have no TIME or DATE prefixes.
Clock and date appear only on Overview; detail pages use the lower area for
their second metric row and page indicator.

Run `make preview-check` for channel isolation, invalid data, carousel boundary,
reduced-motion assertions, and actual Cairo PNGs. The helper also accepts
`--page cpu`, `--at 12.5`, `--missing`, `--size 640`, and authenticated `--live`.
UI previews contain sample data and show Overview when rotation is selected.

## Display Elements

The UI shows only the checkboxes supported by the selected mode and saves them
independently in the `display` object:

- **Dual:** `dual_show_bars` (default `true`). Hiding both bars releases the
  middle area for the two sensor rows.
- **Split:** `split_show_load` and `split_show_watts` (default `true`), plus
  `split_show_rpm` (default `false`). Visible secondary rows share the available
  column height; hiding rows enlarges the temperature region. CPU RPM uses the
  cooler's RPM channel, as in Circle; other slots use their own RPM channel.
  Load uses 66% of the effective Power / Extra Info font size.
- **Circle:** individual load, RPM, power, frequency and bar controls, described
  below.

The primary preview sends unsaved settings to CoolerDash and uses the production
Cairo renderer with deterministic example values. If the plugin endpoint is not
available, the embedded SVG preview uses static example values and recalculates
its layout when elements are toggled. The device renderer uses its detected
geometry and configured font limits when allocating the freed space; large
values and long labels are fitted within their regions.

The optional sensor ring follows the detected display geometry. Circular
displays use a 270-degree arc. Rectangular and rounded-square displays use an
inset rounded frame whose stroke stays inside the rendered image.

Split and Dual modes can divide that ring into two equal halves. Each half uses
one visible layout sensor and the same temperature-threshold colors as its bar.
Split temperature numbers stay white so the ring alone carries the threshold
state. Split load, power and RPM values use the configured label color. This
option overrides the single-sensor ring and is enabled by default. Enabling
either ring disables the other, so only one ring type is saved. Ring thickness,
opacity and label color are configurable in the Layout tab.

## Files

```
src/mods/
├── display.c/h    # Mode dispatcher
├── dual.c/h       # Dual mode
├── circle.c/h     # Circle mode
└── split.c/h      # Split mode
```

Dispatcher (`display.c`):
```c
void draw_display_image(const struct Config *config) {
    if (strcmp(config->display_mode, "circle") == 0)
        draw_circle_image(config);
    else if (strcmp(config->display_mode, "split") == 0)
        draw_split_image(config);
    else
        draw_dual_image(config);
}
```

---

## Dual Mode

CPU and GPU temperatures side-by-side.

```
┌────────────────────────────┐
│     CPU: 45°    GPU: 52°   │
│     ┌─────┐     ┌─────┐   │
│     │█████│     │██████│   │
│     └─────┘     └─────┘   │
│      CPU         GPU       │
└────────────────────────────┘
```

### ScalingParams

```c
typedef struct {
    double scale_x, scale_y, scale_uniform;
    double corner_radius;
    double circle_center_x, circle_center_y;
    double circle_radius;
    int safe_bar_width;
    double safe_content_margin;
    int is_circular;
    DisplayShape shape;
    const char *profile_name;
} ScalingParams;
```

Base resolution: 240×240. Scales dynamically.
Circular displays use a position-dependent chord calculated from each rendered
region's Y position and height.

### Rendering Flow

1. `draw_dual_image()` — entry point
2. `get_cached_lcd_device_data()` — device info
3. `get_temperature_monitor_data()` — sensor data
4. Cairo: create surface → draw background → draw bars + labels → write PNG
5. `send_image_to_lcd()` — upload

---

## Circle Mode

Alternates between sensor slots, one at a time. Optimized for circular high-res displays.

```
┌────────────────┐
│      45°       │
│    ┌──────┐    │
│    │██████│    │
│    └──────┘    │
│      CPU       │
└────────────────┘
```

### Sensor Slots

Configured in `config.json`:
```json
"sensor_slot_1": "cpu",
"sensor_slot_2": "liquid",
"sensor_slot_3": "gpu"
```

Cycles through slots at `circle_switch_interval` (default: 8s, range: 1–60s).

### Circle Layout

Selecting Circle in the plugin UI reveals **Circle Layout**:

- **Mode 1 — Classic** (`"circle_layout": "classic"`): the existing layout,
  including CPU/GPU load beside the temperature when available.
- **Mode 2 — Centered** (`"circle_layout": "centered"`): temperature and degree
  symbol centered together above the bar, with centered label and extra info
  below. Load can optionally appear beside the temperature.

Set `circle_layout` inside the `display` object. Missing or unsupported values
use Classic, so existing configurations keep their layout. Both layouts share
sensor slots, switch interval, and threshold colors.

**Display Elements** provides separate checkboxes for Circle in both layouts:

| Element | Key in `display` |
| --- | --- |
| Load (%) | `circle_show_load` |
| Fan / pump speed (RPM) | `circle_show_rpm` |
| Power (W) | `circle_show_watts` |
| Frequency (GHz / MHz) | `circle_show_frequency` |
| Temperature bar | `circle_show_bar` |

These flags only affect Circle. Hidden elements release their space; the
temperature and remaining text are resized and repositioned within the LCD
geometry. Hiding load centers the temperature in either Circle layout.
The old `circle_show_extra_info` setting remains a fallback for RPM, power and
frequency when their individual keys are missing. Explicit individual settings
take precedence. Old configurations keep the bar enabled and load enabled in
Classic, disabled in Centered. The UI saves the individual settings.

Extra lines depend on the available sensor channels. The UI preview uses fixed
example values and switches with the selected layout.

### State

```c
static int current_slot = 0;      // 0→1→2→0
static time_t last_switch_time = 0;
```

### Centering

In Mode 2, temperature + degree symbol are measured and centered as a unit
within the safe display region. Conceptually:
```c
const double total_width = temp_ext.width + 5 + degree_ext.width;
double temp_x = (display_width - total_width) / 2.0;
```

### Rendering Flow

1. `draw_circle_image()` — entry point
2. `get_cached_lcd_device_data()` — device info
3. `get_temperature_monitor_data()` — sensor data
4. `update_sensor_mode()` — check switch interval
5. Cairo: create surface → draw single sensor → write PNG
6. `send_image_to_lcd()` — upload

---

## Adding a New Mode

1. Create `src/mods/newmode.c/h`
2. Implement `draw_newmode_image(const struct Config *config)`
3. Add dispatch in `display.c`
4. Add validation in `config.c` (`load_display_from_json`)
5. Add to Makefile `SRC_MODULES`
6. Add CLI flag in `main.c` (`parse_arguments`)
7. Update docs and `config.json`
