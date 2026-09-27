#!/usr/bin/env bash
# User-level install: binary + desktop entry, no sudo.
# Override with OLLAMA_IMPORTER_BIN_DIR / OLLAMA_IMPORTER_DESKTOP_DIR (tests do).
set -euo pipefail
bin_dir="${OLLAMA_IMPORTER_BIN_DIR:-$HOME/.local/bin}"
desktop_dir="${OLLAMA_IMPORTER_DESKTOP_DIR:-$HOME/.local/share/applications}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

binary="$root/build/ollama-kde-importer"
[ -x "$binary" ] || { echo "binary missing: $binary — run ./build.sh first" >&2; exit 1; }

install -d "$bin_dir" "$desktop_dir"
resolved_bin="$(realpath -m "$bin_dir/ollama-kde-importer")"
install -m 0755 "$binary" "$resolved_bin"

desktop="$desktop_dir/ollama-kde-importer.desktop"
sed -e "s|@BINARY_PATH@|$resolved_bin|g" "$root/ollama-kde-importer.desktop" > "$desktop"
chmod 0644 "$desktop"

if command -v update-desktop-database >/dev/null 2>&1; then
  update-desktop-database "$desktop_dir" >/dev/null 2>&1 || true
fi

echo "Installed: $resolved_bin"
echo "Desktop entry: $desktop"