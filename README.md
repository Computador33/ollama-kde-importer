# ollama-kde-importer

KDE Plasma 6 (Qt6/KF6) wizard for importing Safetensors models and LoRA
adapters into Ollama. A standalone tool for Ollama, grouped under this repo's
`tools/` for now.

## Build & install (CachyOS / Arch)

    sudo pacman -S extra-cmake-modules qt6-base kcoreaddons6 ki18n base-devel cmake
    ./build.sh

Installs user-level (no sudo): binary → `~/.local/bin/ollama-kde-importer`,
desktop entry → `~/.local/share/applications/ollama-kde-importer.desktop`.
Run it from the app launcher (Ollama Safetensors Importer) or a terminal.

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

## Manual click-through (verification)

1. Launch from the app launcher; page 1 offers the three methods.
2. Page 2: pick a folder with the Browse button; give the model a name.
   Method 2 requires a base model; Method 1 requires the llama.cpp checkout.
   Invalid input must block Next with a message.
3. Page 3: log streams per step; with `ollama` daemon stopped, it must stop at
   the preflight with a clear message — not spin. Afterwards `ollama list`
   shows the imported model.
4. Cancel mid-run must remove `~/.cache/ollama-kde-importer/<model>/`.