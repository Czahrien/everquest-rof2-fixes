#!/usr/bin/env bash
# Builds build/dinput8.dll. Uses mingw-w64 if installed, otherwise zig
# (run tools/setup-zig.sh once if zig is not on PATH).
set -e
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
toolchain="${TOOLCHAIN:-}"
if [[ -z "$toolchain" ]]; then
  if command -v i686-w64-mingw32-g++ >/dev/null 2>&1; then toolchain=mingw-i686; else toolchain=zig-x86-windows; fi
fi
cmake -S "$root" -B "$root/build" -DCMAKE_TOOLCHAIN_FILE="$root/cmake/$toolchain.cmake" "$@"
cmake --build "$root/build"
echo "built $root/build/dinput8.dll ($toolchain)"
