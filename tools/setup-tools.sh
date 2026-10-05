#!/usr/bin/env bash
# Installs the pinned build tools (zig, cmake, ninja) into .tools/venv. No root needed.
set -e
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
venv="$root/.tools/venv"
if command -v uv >/dev/null 2>&1; then
  uv venv -q --allow-existing "$venv"
  uv pip install -q -p "$venv" -r "$root/tools/requirements.txt"
else
  python3 -m venv "$venv"
  "$venv/bin/pip" install -q -r "$root/tools/requirements.txt"
fi
"$root/tools/zig" version
