# ollama-kde-importer

KDE Plasma 6 (Qt6/KF6) wizard for importing Safetensors models and LoRA
adapters into Ollama. A standalone tool for Ollama.

Tested on CachyOS (Plasma 6, Qt 6.11 / KF6, Ollama 0.34.3).

## Requirements

The wizard itself is a standard Qt6/KF6 app and runs on any Linux desktop
with those libraries — a Plasma session is the primary target, but it will
run under X11 or Wayland on other desktops too.

| Tool | Needed for | Arch family (pacman) | Debian/Ubuntu (apt) | Fedora (dnf) |
|------|-----------|---------------------|---------------------|--------------|
| Qt6 Widgets, KF6 (CoreAddons, I18n), ECM, CMake, C++20 compiler | Build | `extra-cmake-modules qt6-base kcoreaddons6 ki18n base-devel cmake` | `qt6-base-dev libkf6coreaddons-dev libkf6i18n-dev extra-cmake-modules cmake g++` | `qt6-qtbase-devel kf6-kcoreaddons-devel kf6-ki18n-devel extra-cmake-modules cmake gcc-c++` |
| `ollama` daemon (running) | All methods | `ollama` (or from ollama.com) | same | same |
| `python3` + `gguf` (pip) | Method 1 (convert) | system python3 + `pip install --user gguf` | same | same |
| llama.cpp checkout with `convert_hf_to_gguf.py` | Method 1 | from https://github.com/ggml-org/llama.cpp | same | same |
| `llama-quantize` on `PATH` | Method 1 | built from that checkout (`build/bin/llama-quantize`, symlinked into `~/.local/bin`) | same | same |

Package names differ slightly between distros (`kcoreaddons` vs `kcoreaddons6`);
`build.sh` accepts either. On non-Arch distros, install the equivalent dev
packages above and either export `SKIP_PACMAN_DEP_CHECK=1` — or nothing:
the check is skipped automatically when `pacman` is absent.

## Build & install

    ./build.sh

Installs user-level (no sudo): binary → `~/.local/bin/ollama-kde-importer`,
desktop entry → `~/.local/share/applications/ollama-kde-importer.desktop`.
Run it from the app launcher ("Ollama Safetensors Importer") or a terminal.

## Three import methods

1. **Full model** — `convert_hf_to_gguf.py` → `llama-quantize` → `ollama create`.
   Needs `python3`, a llama.cpp checkout with `convert_hf_to_gguf.py`
   (path entered in the UI), and `llama-quantize` on PATH. Newer llama.cpp
   scripts import the `gguf` Python package (`pip install --user gguf`).
2. **LoRA adapter** — `FROM <base-model>` + `ADAPTER <safetensors-dir>`.
   Requires a base model name.
3. **Native folder** — `FROM <safetensors-dir>` (Ollama native import).

The import is confirmed before it starts; staging lives in
`~/.cache/ollama-kde-importer/` and is removed after success or cancel.
Requires the `ollama` daemon to be running.

## Tests

    ./tests/run-tests.sh        # headless core + GUI selfcheck + install tests

## Troubleshooting

- **Launcher start: "llama-quantize was not found on PATH"** — a KDE menu
  app often starts without `~/.local/bin` on `PATH`. Add it (KDE Plasma:
  `~/.config/environment.d/10-local-bin.conf` with `PATH=$HOME/.local/bin:$PATH`)
  or launch from a terminal.
- **Wizard won't open under Wayland** — launch with `-platform xcb`.

## Manual click-through (verification)

1. Launch from the app launcher; page 1 offers the three methods.
2. Page 2: pick a folder with the Browse button; give the model a name.
   Method 2 requires a base model; Method 1 requires the llama.cpp checkout.
   Invalid input must block Next with a message.
3. Page 3: log streams per step; with `ollama` daemon stopped, it must stop at
   the preflight with a clear message — not spin. Afterwards `ollama list`
   shows the imported model.
4. Cancel mid-run must remove `~/.cache/ollama-kde-importer/<model>/`.