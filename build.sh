#!/usr/bin/env bash
# Builds build/dinput8.dll with Ninja. Uses mingw-w64 if installed, otherwise zig
# (run tools/setup-tools.sh once to get pinned zig/cmake/ninja in .tools/venv).
set -e
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[[ -d "$root/.tools/venv/bin" ]] && export PATH="$root/.tools/venv/bin:$PATH"
toolchain="${TOOLCHAIN:-}"
if [[ -z "$toolchain" ]]; then
  if command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then toolchain=mingw-i686; else toolchain=zig-x86-windows; fi
fi
# Older checkouts configured build/ with Makefiles; CMake refuses to switch generators.
if [[ -f "$root/build/CMakeCache.txt" ]] && ! grep -q "CMAKE_GENERATOR:INTERNAL=Ninja" "$root/build/CMakeCache.txt"; then
  rm -rf "$root/build"
fi
cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$root/cmake/$toolchain.cmake" "$@"
cmake --build "$root/build"
echo "built $root/build/dinput8.dll ($toolchain)"
