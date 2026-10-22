#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(git -C "$package_dir" rev-parse --show-toplevel)

[[ -z $(git -C "$repo_dir" status --porcelain) ]] || {
    echo "Working tree has changes; commit or stash them before updating." >&2
    exit 1
}
git -C "$repo_dir" pull --ff-only
"$package_dir/install-package.sh"
