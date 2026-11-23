#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
archive=${1:-"$work/mrubyc.tar.gz"}
bash "$repo/scripts/fetch-mrubyc.sh" "$archive"
tar -xzf "$archive" -C "$work" --strip-components=3 --wildcards '*/hal/posix/hal.[ch]'

runtime="$repo/vendor/mrubyc/src"
"${CC:-cc}" -m32 -std=gnu11 -Os -DMRBC_SUPPORT_OP_EXT=1 \
  -I "$runtime" -I "$work" -I "$repo/samples/vm_tasks" \
  "$runtime"/*.c "$work/hal.c" "$repo/tests/posix/main.c" -o "$work/smoke"
timeout 10 "$work/smoke" | tee "$work/output"
printf '%s\n' a:start b:ran a:done | diff -u - "$work/output"
