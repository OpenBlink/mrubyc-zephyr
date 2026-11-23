#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

archive=${1:?Usage: fetch-mrubyc.sh ARCHIVE_PATH}
release=release3.4.1
release_sha=6e039f3e4ad046c588fbb635cfbe6627ebff8f26cd39ffb32574ca6ef5b6ac46

if [[ ! -f "$archive" ]]; then
  curl --fail --silent --show-error --location --retry 3 \
    "https://github.com/mrubyc/mrubyc/releases/download/$release/mrubyc-$release.tar.gz" \
    -o "$archive"
fi
printf '%s  %s\n' "$release_sha" "$archive" | sha256sum --check
