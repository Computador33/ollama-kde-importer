#!/usr/bin/env bash
# Build and run the headless core tests. No display, no live Ollama, no shell.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"   # the test binary reads importwizard.cpp relative to the project root
cmake -S "$root" -B "$root/build" -DBUILD_TESTING=ON >/dev/null
cmake --build "$root/build" --target test_oli_core -j"$(nproc)" >/dev/null
"$root/build/test_oli_core"

# GUI selfcheck: the wizard must open and close itself within 15s (headless).
cmake --build "$root/build" --target ollama-kde-importer -j"$(nproc)" >/dev/null
timeout 15 "$root/build/ollama-kde-importer" --ui-selfcheck -platform offscreen

# User-level install path: binary copy + desktop Exec substitution.
"$root/tests/test_install.sh"