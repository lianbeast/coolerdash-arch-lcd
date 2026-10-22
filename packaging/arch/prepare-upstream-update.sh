#!/usr/bin/env bash
set -euo pipefail

package_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(git -C "$package_dir" rev-parse --show-toplevel)
upstream_ref=refs/remotes/upstream-check/master

[[ -z $(git -C "$repo_dir" status --porcelain) ]] || {
    echo "Working tree has changes; commit or stash them first." >&2
    exit 1
}
git -C "$repo_dir" fetch --quiet --tags \
    https://github.com/damachine/coolerdash.git "+master:${upstream_ref}"
upstream_short=$(git -C "$repo_dir" rev-parse --short "$upstream_ref")
branch="upstream-update/${upstream_short}"
git -C "$repo_dir" switch -c "$branch"

if ! git -C "$repo_dir" rebase "$upstream_ref"; then
    echo "Upstream conflicts need review. Resolve them, then run git rebase --continue." >&2
    echo "To abandon this update branch: git rebase --abort; git switch main" >&2
    exit 1
fi

make -C "$repo_dir" -j"$(nproc)" preview-check
echo "Review ${branch}; merge it into main only after checking all five previews."
