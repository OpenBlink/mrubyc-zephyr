#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

install_dir=$(realpath -m "${1:?Usage: build-mrbc.sh INSTALL_DIR}")
source_commit=a309524d0bc90eef077a24634db2495a6f68e318 # mruby 3.4.0
source_sha=fe691f90225448c0bbb43deb304113d206138cf3f4a4690769962300c0995058
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

curl --fail --silent --show-error --location --retry 3 \
  "https://codeload.github.com/mruby/mruby/tar.gz/$source_commit" -o "$work/mruby.tar.gz"
printf '%s  %s\n' "$source_sha" "$work/mruby.tar.gz" | sha256sum --check
tar -xzf "$work/mruby.tar.gz" -C "$work"
compiler="$work/mruby-$source_commit/build/host/mrbc/bin/mrbc"
(
  cd "$work/mruby-$source_commit"
  MRUBY_CONFIG=default rake -j "${MRUBYC_BUILD_JOBS:-2}" "$compiler"
)
install -D -m 0755 "$compiler" "$install_dir/bin/mrbc"
