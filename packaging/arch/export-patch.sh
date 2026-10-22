#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(git -C "$package_dir" rev-parse --show-toplevel)
output=${1:-"${repo_dir}/coolerdash-arch-lcd.patch"}
upstream_ref=refs/remotes/upstream-check/master

git -C "$repo_dir" fetch --quiet --tags \
    https://github.com/damachine/coolerdash.git "+master:${upstream_ref}"
base=$(git -C "$repo_dir" merge-base HEAD "$upstream_ref")
temp_patch=$(mktemp "${output}.XXXXXX")
trap 'rm -f -- "$temp_patch"' EXIT
git -C "$repo_dir" diff --binary "$base..HEAD" > "$temp_patch"
mv -- "$temp_patch" "$output"
trap - EXIT
echo "Patch written: $output"
