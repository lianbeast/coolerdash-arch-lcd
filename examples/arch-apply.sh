#!/usr/bin/env bash
set -euo pipefail

bundle_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
plugin_dir=/var/lib/coolercontrol/plugins/coolerdash
binary_path=/usr/libexec/coolerdash/coolerdash
config_path="$plugin_dir/config.json"
ui_path="$plugin_dir/ui/index.html"

if [[ $EUID -ne 0 ]]; then
    echo "Run with sudo: sudo $0" >&2
    exit 1
fi

for path in "$config_path" "$ui_path" "$binary_path"; do
    [[ -e $path ]] || { echo "Missing installed CoolerDash file: $path" >&2; exit 1; }
done
for path in "$bundle_dir/coolerdash" "$bundle_dir/index.html" "$bundle_dir/arch-config.json"; do
    [[ -f $path ]] || { echo "Missing bundle file: $path" >&2; exit 1; }
done
command -v jq >/dev/null || { echo "jq is required" >&2; exit 1; }
jq -e '.display.mode == "arch"' "$bundle_dir/arch-config.json" >/dev/null

cp -an -- "$binary_path" "$binary_path.arch-layout.bak"
cp -an -- "$config_path" "$config_path.arch-layout.bak"
cp -an -- "$ui_path" "$ui_path.arch-layout.bak"

config_tmp=$(mktemp "$plugin_dir/.arch-config.XXXXXX")
trap 'rm -f -- "$config_tmp"' EXIT

jq --slurpfile theme "$bundle_dir/arch-config.json" '
  .paths = (.paths // {}) |
  .paths.image_background = "" |
  .display = (($theme[0].display | with_entries(select(.key | startswith("arch_")))) * (.display // {})) |
  .display.mode = "arch" |
  .display.refresh_interval = $theme[0].display.refresh_interval |
  .display.background_overlay_opacity = 0 |
  .display.content_scale_factor = 0.94 |
  .display.sensor_slot_1 = (.display.sensor_slot_1 // "cpu") |
  .display.sensor_slot_2 = (.display.sensor_slot_2 // "gpu") |
  .display.sensor_slot_3 = (.display.sensor_slot_3 // "liquid") |
  .display.split_ring_by_sensor = false |
  .display.sensor_ring_enabled = false |
  .layout = ((.layout // {}) * $theme[0].layout) |
  .colors = $theme[0].colors |
  .font = ((.font // {}) * $theme[0].font) |
  .sensors = (.sensors // {}) |
  .sensors.cpu = ((.sensors.cpu // {}) * $theme[0].sensors.cpu) |
  .sensors.gpu = ((.sensors.gpu // {}) * $theme[0].sensors.gpu) |
  .sensors.liquid = ((.sensors.liquid // {}) * $theme[0].sensors.liquid)
' "$config_path" > "$config_tmp"

jq -e '.display.mode == "arch"' "$config_tmp" >/dev/null

# Keep the immediately previous working installation as well as stock originals.
upgrade_backup=$(mktemp -d "$plugin_dir/arch-upgrade.XXXXXX")
cp -a -- "$binary_path" "$upgrade_backup/coolerdash"
cp -a -- "$config_path" "$upgrade_backup/config.json"
cp -a -- "$ui_path" "$upgrade_backup/index.html"
rollback() {
    trap - ERR
    echo "Upgrade failed; restoring previous installation." >&2
    systemctl stop cc-plugin-coolerdash.service || true
    cp -a -- "$upgrade_backup/coolerdash" "$binary_path"
    cp -a -- "$upgrade_backup/config.json" "$config_path"
    cp -a -- "$upgrade_backup/index.html" "$ui_path"
    systemctl start cc-plugin-coolerdash.service || true
    exit 1
}
trap rollback ERR
systemctl stop cc-plugin-coolerdash.service
install -m755 -- "$bundle_dir/coolerdash" "$binary_path"
install -m644 -- "$bundle_dir/index.html" "$ui_path"
install -o cc-plugin-user -g cc-plugin-user -m600 -- "$config_tmp" "$config_path"
systemctl restart cc-plugin-coolerdash.service
systemctl is-active --quiet cc-plugin-coolerdash.service

echo "CoolerDash Arch LCD suite installed. Previous installation: $upgrade_backup"
echo "Stock originals retained as *.arch-layout.bak. Credentials and sensor slots preserved."
