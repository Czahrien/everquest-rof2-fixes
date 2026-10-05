# rof2-fixes

Runtime fixes for the Rain of Fear 2 (RoF2) EverQuest client used with EQEmu servers,
in the spirit of [Zeal](https://github.com/CoastalRedwood/Zeal) for the older client.
It is a `dinput8.dll` proxy: drop it next to `eqgame.exe` and it patches the game in
memory at startup. No files are modified on disk.

| Fix | Symptom | Cause |
| --- | --- | --- |
| `CpuSpeedFix` | Game runs too fast on Ryzen / fast CPUs | `EQG_GetCpuSpeed2/3` in `EQGraphicsDX9.dll` measure TSC ticks over ~1 s but keep only 32 bits, which wraps above ~4.29 GHz. Replaced with a 64-bit, QPC-timed measurement. |
| `AspectMode` | Characters stretched horizontally on ultrawide (aspect ≥ 2.0) | `eqgame.exe` switches to a different FOV formula at aspect ≥ 2.0 and passes the camera a ratio of two angles instead of width/height (about 1.70 instead of 2.33 at 21:9). The bad branch is disabled; `HorPlus` also widens the horizontal FOV past 16:9. |
| `ExtendedFpsSlider` | The Max FPS slider stops at 99, and high caps are inaccurate | The slider is widened to 10–199 with 200 = Unlimited (was 100). The limiter, which slept whole milliseconds (144 → 6 ms → 166 fps), is replaced with a QueryPerformanceCounter-paced one. |
| `VSync` | The client has no vsync option | Off by default. When enabled, `PresentationInterval` is forced to `D3DPRESENT_INTERVAL_ONE` in `CreateDevice` and `Reset`. |
| `MouseLookFix` | Right-click mouse-look turns slower the higher the frame rate | The mouse delta is stored as a turn *speed* that physics then multiplies by frame time, so turning scales with 1/fps. The turn is applied to the heading directly, scaled to match 30 fps (`MouseLookScale`). |
| `MouseLookConfine` | During mouse-look the hidden cursor keeps moving and can leave the window or monitor | The client hides the cursor but never clips it. While mouse-look is active and the game has focus, the cursor is clipped to the client area; the client already restores its position on release. |
| `PhysicsRateFix` | Jumps barely leave the ground, falls do little or no damage, and levitation descends slower at high FPS | Movement physics applies friction once per step while gravity scales with step length, and a collision pass after each step decides whether you are grounded. The local player's physics step is run at a fixed `PhysicsRate` (60 steps/s) regardless of frame rate, with the collision pass after each step. |
| `ResizeFix` | Resizing the window (e.g. under Wine) stretches the image and the mouse no longer lines up | The client never changes resolution on `WM_SIZE`. The main window is subclassed and, once a resize settles, the new client size goes through the same `ApplyResolution` path as the options window. |

Supports only the RoF2 `eqgame.exe` with PE timestamp `0x518DE58F` (2013-05-11) and
its `EQGraphicsDX9.dll` (`0x518DE5B6`). On any other build it logs a message and does
nothing. Every patch also checks the bytes it replaces before writing.

xackery's [patched EQGraphicsDX9.dll](https://github.com/xackery/eq-core-dll/releases/tag/v0.0.1)
works alongside this, but you no longer need it.

## Building

Both platforms use the same pinned tools (`tools/requirements.txt`: zig, CMake, Ninja),
installed from PyPI into `.tools/venv`. Only Python 3 is required; nothing is installed
system-wide, and no Visual Studio or Windows SDK is needed.

### Linux

```bash
tools/setup-tools.sh   # once
./build.sh             # produces build/dinput8.dll
```

`build.sh` uses `i686-w64-mingw32-g++` if it is installed (e.g. `pacman -S mingw-w64-gcc`)
and zig otherwise. Force one with `TOOLCHAIN=zig-x86-windows` or `TOOLCHAIN=mingw-i686`.

### Windows (PowerShell)

```powershell
tools\setup-tools.ps1   # once
.\build.ps1             # produces build\dinput8.dll
```

If script execution is blocked, run them as
`powershell -ExecutionPolicy Bypass -File tools\setup-tools.ps1` (and likewise for `build.ps1`).
Any of the tools can come from elsewhere instead: `ZIG` may point at a `zig.exe`, and
`cmake`/`ninja` are picked up from `PATH` when the venv does not exist.

### Comparing builds

Rebuilding in the same directory gives a byte-identical DLL. Builds from different
directories or hosts differ only in fields that identify the build (the PE
`TimeDateStamp`, which is a content hash, and the debug directory's build ID). To check
that two builds are otherwise identical:

```bash
python3 tools/compare-dll.py linux/dinput8.dll windows/dinput8.dll
```

### Continuous builds and releases

GitHub Actions (`.github/workflows/build.yml`) builds the DLL on Linux and Windows for
every push to `main` and every pull request, checks the two builds are equivalent with
`tools/compare-dll.py`, and attaches `dinput8.dll` to the run as an artifact.

Pushing a tag starting with `v` also publishes a GitHub Release with the DLL and its
SHA-256 checksum:

```bash
git tag v0.1.0
git push origin v0.1.0
```

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

   **Wayland desktops:** Wine's default X11 driver runs through XWayland, which ignores
   cursor confinement and warping, so `MouseLookConfine` has no effect and the cursor can
   leave the window during mouse-look. Wine's native Wayland driver honors both. Enable it
   for the prefix with:
   ```bash
   WINEPREFIX=/path/to/prefix wine reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d wayland /f
   ```
   (undo with `wine reg delete 'HKCU\Software\Wine\Drivers' /v Graphics /f`). Tested with
   Wine 11.18 Staging on KDE Plasma: the Wayland driver plus `MouseLookConfine=1` keeps the
   cursor in the window, and the client restores its position when mouse-look ends.
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
