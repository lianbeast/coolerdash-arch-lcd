#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(git -C "$package_dir" rev-parse --show-toplevel)
upstream_ref=refs/remotes/upstream-check/master

git -C "$repo_dir" fetch --quiet --tags \
    https://github.com/damachine/coolerdash.git "+master:${upstream_ref}"
local_head=$(git -C "$repo_dir" rev-parse --short HEAD)
upstream_head=$(git -C "$repo_dir" rev-parse --short "$upstream_ref")
base=$(git -C "$repo_dir" merge-base HEAD "$upstream_ref")
behind=$(git -C "$repo_dir" rev-list --count "HEAD..${upstream_ref}")

printf 'Local: %s\nUpstream: %s\nNew upstream commits: %s\n' \
    "$local_head" "$upstream_head" "$behind"
if [[ $base == $(git -C "$repo_dir" rev-parse "$upstream_ref") ]]; then
    echo "Arch fork already contains current upstream."
elif (( behind > 0 )); then
    echo "Run prepare-upstream-update.sh to create a review branch."
fi
