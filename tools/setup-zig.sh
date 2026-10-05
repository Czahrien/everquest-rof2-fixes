#!/usr/bin/env bash
# Installs zig into .tools/venv from PyPI (the "ziglang" package). No root needed.
set -e
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if command -v uv >/dev/null 2>&1; then
  uv venv -q "$root/.tools/venv"
  uv pip install -q -p "$root/.tools/venv" ziglang
else
  python3 -m venv "$root/.tools/venv"
  "$root/.tools/venv/bin/pip" install -q ziglang
fi
"$root/tools/zig" version
