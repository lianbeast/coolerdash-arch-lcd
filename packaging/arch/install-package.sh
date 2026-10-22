#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
"$package_dir/build-package.sh"
package_path=$(cd "$package_dir" && makepkg --packagelist)
[[ -f $package_path ]] || { echo "Package not found: $package_path" >&2; exit 1; }
package_path=$(realpath "$package_path")

if (( EUID == 0 )); then
    "$package_dir/install-root.sh" "$package_path"
elif command -v pkexec >/dev/null; then
    pkexec --disable-internal-agent /usr/bin/bash \
        "$package_dir/install-root.sh" "$package_path"
else
    sudo /usr/bin/bash "$package_dir/install-root.sh" "$package_path"
fi
