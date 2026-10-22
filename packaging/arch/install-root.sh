#!/usr/bin/env bash
set -euo pipefail

if (( EUID != 0 )) || (( $# != 1 )); then
    echo "Usage as root: $0 /absolute/path/to/coolerdash-arch-lcd package" >&2
    exit 2
fi

package_path=$1
[[ $package_path == /* && -f $package_path ]] || {
    echo "Package path must be an absolute regular file." >&2
    exit 2
}
package_name=$(pacman -Qp "$package_path" | awk '{print $1}')
[[ $package_name == coolerdash-arch-lcd-git ]] || {
    echo "Refusing unexpected package: $package_name" >&2
    exit 2
}

plugin_dir=/var/lib/coolercontrol/plugins/coolerdash
backup_dir=""
if [[ -d $plugin_dir ]]; then
    backup_dir=$(mktemp -d "$plugin_dir/package-backup.XXXXXX")
    chmod 700 "$backup_dir"
    for name in config.json credentials.json; do
        [[ ! -f $plugin_dir/$name ]] || cp -a -- "$plugin_dir/$name" "$backup_dir/$name"
    done
fi

restore_runtime() {
    [[ -n $backup_dir ]] || return 0
    for name in config.json credentials.json; do
        [[ ! -f $backup_dir/$name ]] || cp -a -- "$backup_dir/$name" "$plugin_dir/$name"
    done
}
trap restore_runtime EXIT

pacman -U --noconfirm "$package_path"
restore_runtime
trap - EXIT

if systemctl is-active --quiet cc-plugin-coolerdash.service; then
    systemctl restart cc-plugin-coolerdash.service
fi
echo "Runtime config backup retained at: ${backup_dir:-not needed (fresh install)}"
