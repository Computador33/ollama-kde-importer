#!/usr/bin/env bash
# Verifies install.sh: copies the binary and substitutes the real Exec path.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

mkdir -p "$tmp/build"
printf '#!/bin/sh\nexit 0\n' > "$tmp/build/ollama-kde-importer"
chmod +x "$tmp/build/ollama-kde-importer"

OLLAMA_IMPORTER_BIN_DIR="$tmp/bin" \
OLLAMA_IMPORTER_DESKTOP_DIR="$tmp/apps" \
  "$root/install.sh" >/dev/null

[ -x "$tmp/bin/ollama-kde-importer" ] || { echo "binary not installed"; exit 1; }
grep -q "^Exec=$tmp/bin/ollama-kde-importer$" "$tmp/apps/ollama-kde-importer.desktop" \
  || { echo "Exec path not substituted absolutely"; exit 1; }
grep -q "^Icon=applications-science$" "$tmp/apps/ollama-kde-importer.desktop" || { echo "icon wrong"; exit 1; }
grep -q "^Terminal=false$" "$tmp/apps/ollama-kde-importer.desktop" || { echo "Terminal wrong"; exit 1; }
echo "install test PASS"