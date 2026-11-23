#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath "${1:-$(command -v mrbc)}")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

for source in "$repo"/samples/{hello,vm_tasks}/*.rb; do
  symbol="$(basename "$source" .rb)_mrb"
  cmake "-DCOMPILER=$compiler" "-DSOURCE=$source" "-DSYMBOL=$symbol" \
    "-DHEADER=$work/$symbol.h" -P "$repo/cmake/mrubyc.cmake"
  diff -u "$(dirname "$source")/$symbol.h" "$work/$symbol.h"
done
