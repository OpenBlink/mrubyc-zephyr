#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

install_dir=$(realpath -m "${1:?Usage: build-mrbc.sh INSTALL_DIR}")
source_commit=831da26b9021de0369d17b71b5667e2941a1a32d # mruby 4.0.0
source_sha=e4fb1bbad8276878e84e3c3b6161ac35a4216853de404f8463675aa2d1920b4f
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
