#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
"$package_dir/build-package.sh"
package_path=$(cd "$package_dir" && makepkg --packagelist)
[[ -f $package_path ]] || { echo "Package not found: $package_path" >&2; exit 1; }

if (( EUID == 0 )); then
    pacman -U --noconfirm "$package_path"
elif command -v pkexec >/dev/null; then
    pkexec --disable-internal-agent /usr/bin/pacman -U --noconfirm "$package_path"
else
    sudo pacman -U --noconfirm "$package_path"
fi
