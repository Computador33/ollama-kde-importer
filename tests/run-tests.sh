#!/usr/bin/env bash
# Build and run the headless core tests. No display, no live Ollama, no shell.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cmake -S "$root" -B "$root/build" -DBUILD_TESTING=ON >/dev/null
cmake --build "$root/build" --target test_oli_core -j"$(nproc)" >/dev/null
"$root/build/test_oli_core"