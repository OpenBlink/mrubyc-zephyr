#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
hal_commit=32b4ae7d8598213e8d5990f174e1ad5abc1cafa1
archive=${1:-"$work/mrubyc.tar.gz"}

bash "$repo/scripts/fetch-mrubyc.sh" "$archive"
mkdir -p "$work/upstream" "$work/expected/src" "$work/hal"
tar -xzf "$archive" -C "$work/upstream" --strip-components=1
cp "$work"/upstream/LICENSE{,.unicode} "$work/expected/"
cp "$work"/upstream/src/*.{c,h} "$work/upstream/src/VERSION" "$work/expected/src/"
diff -r "$work/expected" "$repo/vendor/mrubyc"

upstream="https://raw.githubusercontent.com/mrubyc/mrubyc/$hal_commit"
for file in hal.c hal.h; do
  curl --fail --silent --show-error --location --retry 3 \
    "$upstream/hal/zephyr/$file" -o "$work/hal/$file"
done
curl --fail --silent --show-error --location --retry 3 \
  "$upstream/LICENSE" -o "$work/hal/LICENSE"
diff -r "$work/hal" "$repo/zephyr/hal"
