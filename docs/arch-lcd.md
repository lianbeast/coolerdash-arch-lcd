# Arch LCD suite / revision 3

Native Cairo layout for the locally verified NZXT Kraken Z (Z53/Z63/Z73),
320x320 circular LCD, CoolerControl 5.1.1, and CoolerDash 3.3.6.
This custom binary/UI is required; stock CoolerDash does not have Arch mode.

## Pages

| Page | Readings |
| --- | --- |
| Overview | CPU, GPU, coolant temperatures and threshold status |
| CPU | Temperature, average clock, usage, power, maximum core clock |
| GPU | Temperature, core clock, usage, power, fan RPM |
| Liquid | Temperature with decimal, pump RPM, fan RPM, pump/fan duty |
| Power | CPU + GPU draw, individual power readings and usage |

Power is a sum of two reported component channels, not whole-PC or wall power.
CPU maximum clock is the current maximum across cores, not an all-time peak.
Missing or invalid data shows `--`; Overview also shows `NO DATA`. Zero remains
a legitimate reading (idle load or stopped fan). No fabricated telemetry.

## Build and apply

Requires the existing CoolerDash build dependencies and JetBrainsMono Nerd Font
for the local Arch glyph. No new runtime libraries are introduced.

1. Run `make -j4 arch-bundle` in this source checkout.
2. Run `sudo bash build/arch-bundle/apply.sh` to upgrade the existing installation.
3. Open CoolerDash plugin settings, Display, and choose Arch Page. Save applies it.

For a prebuilt bundle, run `sudo bash ./apply.sh` inside its folder.
The installer preserves credentials and selected sensor slots, stops only the
CoolerDash plugin, installs the binary/UI/theme, and restarts it. It keeps stock
originals as `*.arch-layout.bak` and the preceding working revision in a private
`arch-upgrade.*` directory beside plugin config. Failure triggers rollback.
`sudo bash ./restore.sh` restores the original stock installation, not revision 2.
Package upgrades may replace the custom binary and UI; rebuild/reapply if needed.

## Controls

- `display.arch_page`: `overview`, `cpu`, `gpu`, `liquid`, `power`, or `rotate`.
- `display.arch_page_seconds`: 5..120 seconds per page; bundled default 12.
- `display.arch_motion_seconds`: 10..120 seconds per orbit; default 40.
- `display.arch_reduce_motion`: freezes decoration; rotation stays on Overview.
  Explicit fixed-page selections still work; readings and clock remain live.
- `display.refresh_interval`: bundled 0.5 seconds targets two frames/second.
  Actual physical refresh depends on CoolerControl and USB transfer time.
- `display.sensor_slot_1/2/3`: CPU/GPU/liquid temperature source devices.
- Advanced Arch metric mappings: exact channel names on those same devices.
  Missing names never fall back to another device or a different channel type.

Default mapping names were confirmed through the authenticated local API:
CPU `CPU Freq Avg Freq`, `CPU Load Duty`, `power0 Watts`, `CPU Freq Max Freq`;
GPU `freq1 Freq`, `GPU Load Duty`, `power1_average Watts`, `fan1 RPM`;
Kraken `pump RPM`, `fan RPM`, `pump Duty`, `fan Duty`.
Device identifiers and credentials are not bundled. Names may differ on other
hardware; use the UI mappings or the helper's `--sensors` diagnostic there.

## Design tokens and motion

Midnight background, white primary text, muted blue-gray secondary text, and
locally sourced Arch blue. Existing config color keys remain authoritative:
`colors.display_background`, `font_temp`, `font_label`, and `bar_border` (accent).
`sensors.*.threshold_*` and corresponding colors control thermal status; these
are display thresholds, not hardware protection settings.

Shared native type tokens live near the top of `src/mods/arch.c`: 21px brand,
28px clock, 15px date, 12px labels, 21px metrics, and 42px hero readings. Arch
glyph is 34px; overview icons are 24px. Uniform resolution scaling and bounded
text fitting preserve the circular safe area. `content_scale_factor` adjusts
the full composition's inset. Keep `- Arch Linux -`; clock/date have no labels.

Slow orbital light, gently glowing segments, and an overview connection trace
provide motion without moving or fading readings. Five bottom dots indicate
current page. Native image uploads preserve live readings; a pre-rendered GIF
cannot keep telemetry live. GIF backgrounds remain supported upstream but are
not required. The native reduced-motion switch works without browser CSS.

## Validation and previews

`make preview-check` checks all twelve metric mappings, selected-device isolation,
invalid readings, page boundaries/wraparound, and reduced motion. It renders
sample and missing-data images through production Cairo code.

`build/render_preview_test examples/arch-config.json /tmp/arch-cpu.png --page cpu`
renders one page; also accepts `--missing`, `--at 12.5`, and `--size 640`.
UI previews use sample readings, with Overview for rotation. They are not live
measurements. Authenticated `--live` renders current values without uploading;
it requires local permission to read installed config/credentials. Never copy
private configuration into a public bundle or repository.
