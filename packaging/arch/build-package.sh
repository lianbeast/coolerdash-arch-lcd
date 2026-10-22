#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
build_dir=$(mktemp -d)
trap 'rm -rf -- "$build_dir"' EXIT

install -m644 "$package_dir/PKGBUILD" "$build_dir/PKGBUILD"
install -m644 "$package_dir/coolerdash-arch-lcd.install" \
    "$build_dir/coolerdash-arch-lcd.install"

cd "$build_dir"
PKGDEST="$package_dir" makepkg --cleanbuild --clean --force >&2
PKGDEST="$package_dir" makepkg --packagelist
