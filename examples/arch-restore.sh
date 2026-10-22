#!/usr/bin/env bash
set -euo pipefail

plugin_dir=/var/lib/coolercontrol/plugins/coolerdash
binary_path=/usr/libexec/coolerdash/coolerdash
config_path="$plugin_dir/config.json"
ui_path="$plugin_dir/ui/index.html"

if [[ $EUID -ne 0 ]]; then
    echo "Run with sudo: sudo $0" >&2
    exit 1
fi

for path in "$binary_path" "$config_path" "$ui_path"; do
    backup="$path.arch-layout.bak"
    [[ -f $backup ]] || { echo "Missing backup: $backup" >&2; exit 1; }
done

systemctl stop cc-plugin-coolerdash.service
install -m755 -- "$binary_path.arch-layout.bak" "$binary_path"
install -o cc-plugin-user -g cc-plugin-user -m600 -- "$config_path.arch-layout.bak" "$config_path"
install -m644 -- "$ui_path.arch-layout.bak" "$ui_path"
systemctl restart cc-plugin-coolerdash.service
systemctl is-active --quiet cc-plugin-coolerdash.service

echo "Original CoolerDash binary, UI, and config restored."
