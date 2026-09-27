#!/usr/bin/env bash
# Bootstrap: check packages → configure → build → user-level install.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

deps=(extra-cmake-modules qt6-base base-devel cmake)
missing=()
for p in "${deps[@]}"; do
  pacman -Q "$p" >/dev/null 2>&1 || missing+=("$p")
done
# KF6 CoreAddons is packaged as kcoreaddons on some CachyOS/Arch mirrors and
# kcoreaddons6 on others; CMake provides KF6CoreAddons either way.
for p in kcoreaddons kcoreaddons6; do
  if pacman -Q "$p" >/dev/null 2>&1; then found_kcoreaddons=1; break; fi
done
if [ -z "${found_kcoreaddons:-}" ]; then missing+=(kcoreaddons6); fi
# ki18n may be named ki18n or ki18n6 depending on mirror.
for p in ki18n ki18n6; do
  if pacman -Q "$p" >/dev/null 2>&1; then found_ki18n=1; break; fi
done
if [ -z "${found_ki18n:-}" ]; then missing+=(ki18n); fi

if (( ${#missing[@]} )); then
  echo "Missing build packages: ${missing[*]}" >&2
  echo "Install with: sudo pacman -S ${missing[*]}" >&2
  exit 1
fi

cmake -S "$root" -B "$root/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$root/build" -j"$(nproc)"
"$root/install.sh"
echo "Run via: ollama-kde-importer"