#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

archive=${1:?Usage: fetch-mrubyc.sh ARCHIVE_PATH}
release=release4.0.0
release_sha=1af3b87e8f143df6cb08858e2c640f996e927485611d972bb5590b69d646bc85

if [[ ! -f "$archive" ]]; then
  curl --fail --silent --show-error --location --retry 3 \
    "https://github.com/mrubyc/mrubyc/releases/download/$release/mrubyc-$release.tar.gz" \
    -o "$archive"
fi
printf '%s  %s\n' "$release_sha" "$archive" | sha256sum --check
