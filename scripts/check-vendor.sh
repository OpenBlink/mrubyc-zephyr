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

# The vendored set from src/ is *.c, *.h and VERSION. Every other file
# upstream ships in src/ must be listed here so a reviewer decides
# whether to vendor or skip it; a new file fails this check.
known_exclusions="Makefile
UnicodeData.txt
hal_selector.mk"
upstream_exclusions=$(find "$work/upstream/src" -mindepth 1 -maxdepth 1 \
  ! -name '*.c' ! -name '*.h' ! -name VERSION -printf '%f\n' | sort)
if [[ "$upstream_exclusions" != "$known_exclusions" ]]; then
  printf 'error: new files in upstream src/; vendor them or extend known_exclusions:\n' >&2
  diff <(printf '%s\n' "$known_exclusions") <(printf '%s\n' "$upstream_exclusions") >&2 || true
  exit 1
fi

# zephyr/CMakeLists.txt must compile the same translation units the
# upstream src/Makefile lists in SRCS (hal.c resolves to zephyr/hal here).
upstream_srcs=$(sed -n '/^SRCS =/,/[^\\]$/p' "$work/upstream/src/Makefile" \
  | sed 's/\\//g' | tr ' ' '\n' | grep -E '\.c$' | sort)
module_srcs=$(sed -n '/^zephyr_library_sources(/,/^)/p' "$repo/zephyr/CMakeLists.txt" \
  | sed -n 's|^[[:space:]]*"\${[a-z_]*_dir}/\([[:alnum:]_]*\.c\)".*|\1|p' | sort)
if [[ "$upstream_srcs" != "$module_srcs" ]]; then
  printf 'error: zephyr_library_sources is out of sync with upstream src/Makefile SRCS:\n' >&2
  diff <(printf '%s\n' "$upstream_srcs") <(printf '%s\n' "$module_srcs") >&2 || true
  exit 1
fi

upstream="https://raw.githubusercontent.com/mrubyc/mrubyc/$hal_commit"
for file in hal.c hal.h; do
  curl --fail --silent --show-error --location --retry 3 \
    "$upstream/hal/zephyr/$file" -o "$work/hal/$file"
done
curl --fail --silent --show-error --location --retry 3 \
  "$upstream/LICENSE" -o "$work/hal/LICENSE"
diff -r "$work/hal" "$repo/zephyr/hal"
