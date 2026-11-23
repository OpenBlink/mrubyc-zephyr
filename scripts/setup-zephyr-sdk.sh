#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
set -euo pipefail

downloads=$(realpath -m "${1:?Usage: setup-zephyr-sdk.sh DOWNLOAD_DIR INSTALL_DIR}")
install_dir=$(realpath -m "${2:?Usage: setup-zephyr-sdk.sh DOWNLOAD_DIR INSTALL_DIR}")
version=0.16.9
base="https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v$version"
sdk="zephyr-sdk-${version}_linux-x86_64_minimal.tar.xz"
arm=toolchain_linux-x86_64_arm-zephyr-eabi.tar.xz

download() {
  local name=$1 sha=$2
  if ! (cd "$downloads" && printf '%s  %s\n' "$sha" "$name" | sha256sum --check --status); then
    curl --fail --silent --show-error --location --retry 3 "$base/$name" -o "$downloads/$name"
  fi
  printf '%s  %s\n' "$sha" "$downloads/$name" | sha256sum --check
}

mkdir -p "$downloads" "$install_dir"
download "$sdk" b433f19b334cd15f4d7696a958dda214a7737cf41432900d076adce4253782a5
download "$arm" 81162a8326e3cb82fcc53f20528aa7abec29a0bf8376fa009e0c887ba864aaf8
tar -xJf "$downloads/$sdk" -C "$install_dir" --strip-components=1
tar -xJf "$downloads/$arm" -C "$install_dir"
