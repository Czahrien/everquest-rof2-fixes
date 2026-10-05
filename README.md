# rof2-fixes

Runtime fixes for the Rain of Fear 2 (RoF2) EverQuest client used with EQEmu servers,
in the spirit of [Zeal](https://github.com/CoastalRedwood/Zeal) for the older client.
It is a `dinput8.dll` proxy: drop it next to `eqgame.exe` and it patches the game in
memory at startup. No files are modified on disk.

| Fix | Symptom | Cause |
| --- | --- | --- |
| `CpuSpeedFix` | Game runs too fast on Ryzen / fast CPUs | `EQG_GetCpuSpeed2/3` in `EQGraphicsDX9.dll` measure TSC ticks over ~1 s but keep only 32 bits, which wraps above ~4.29 GHz. Replaced with a 64-bit, QPC-timed measurement. |
| `AspectMode` | Characters stretched horizontally on ultrawide (aspect ≥ 2.0) | `eqgame.exe` switches to a different FOV formula at aspect ≥ 2.0 and passes the camera a ratio of two angles instead of width/height (about 1.70 instead of 2.33 at 21:9). The bad branch is disabled; `HorPlus` also widens the horizontal FOV past 16:9. |
| `ResizeFix` | Resizing the window (e.g. under Wine) stretches the image and the mouse no longer lines up | The client never changes resolution on `WM_SIZE`. The main window is subclassed and, once a resize settles, the new client size goes through the same `ApplyResolution` path as the options window. |

Supports only the RoF2 `eqgame.exe` with PE timestamp `0x518DE58F` (2013-05-11) and
its `EQGraphicsDX9.dll` (`0x518DE5B6`). On any other build it logs a message and does
nothing. Every patch also checks the bytes it replaces before writing.

xackery's [patched EQGraphicsDX9.dll](https://github.com/xackery/eq-core-dll/releases/tag/v0.0.1)
works alongside this, but you no longer need it.

## Building on Linux

```bash
tools/setup-zig.sh   # once: installs zig into .tools/venv (no root needed)
./build.sh           # produces build/dinput8.dll
```

`build.sh` uses `i686-w64-mingw32-g++` if it is installed (e.g. `pacman -S mingw-w64-gcc`)
and zig otherwise. Force one with `TOOLCHAIN=zig-x86-windows` or `TOOLCHAIN=mingw-i686`.

## Installing

1. Copy `build/dinput8.dll` into the folder that contains `eqgame.exe`.
2. **Wine only:** Wine prefers its own built-in `dinput8`, so tell it to use the native one,
   either per launch:
   ```bash
   WINEDLLOVERRIDES="dinput8=n,b" wine eqgame.exe patchme
   ```
   or permanently in `winecfg` → Libraries → add `dinput8` → "Native then Builtin".
   In Lutris: Configure → Runner options → DLL overrides → `dinput8` = `n,b`.
   Running through Lutris (DXVK) is recommended: launching with plain `wine` from a shell
   has been seen to render without character/NPC models, independent of this DLL.
3. Start the game. `rof2fixes.ini` (settings) and `rof2fixes.log` appear next to `eqgame.exe`.
   Check the log to see which fixes were installed.

## Settings (`rof2fixes.ini`)

```ini
[Fixes]
CpuSpeedFix=1
AspectMode=HorPlus        ; Off | Correct | HorPlus
HorPlusBaseAspect=1.7778  ; up to this aspect the in-game FOV is used as-is
ResizeFix=1
UnlockMaxWindowSize=1     ; let the window grow beyond its current resolution
```

## Layout

- `src/addresses.h` — every client address used, with notes on what it is
- `src/fixes/*.cpp` — one file per fix, each with a header comment explaining the bug
- `tools/re/xr.py` — capstone/pefile helpers used to find the addresses
  (`uv pip install capstone pefile`)
