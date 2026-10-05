# How the patches were found

Notes on the methodology behind each fix in this repo: the tools, the search
strategies, the dead ends, and what runtime evidence settled each question. All
addresses are for the RoF2 `eqgame.exe` (PE timestamp `0x518DE58F`, image base
`0x400000`) and `EQGraphicsDX9.dll` (`0x518DE5B6`, base `0x10000000`).

## Toolkit

No interactive disassembler was used. Everything was done with a few lines of
Python on top of [capstone](https://www.capstone-engine.org/) and
[pefile](https://github.com/erocarrera/pefile), kept in `tools/re/xr.py`:

- **Cross-reference index.** One linear sweep over `.text`, recording every
  immediate operand and every absolute memory operand (`[disp32]` with no base
  register) that lands inside the image. The result is a map from address to the
  instructions that mention it, pickled so later queries are instant. This is
  enough to answer "who reads this global", "who calls this function" (`call rel32`
  targets are immediates) and "who uses this string or float constant".
- **Import map.** IAT slot to `dll!function`, so `call dword ptr [IAT]` lines can be
  annotated when printing disassembly.
- **Function start heuristic.** Walk backwards to the `int3` padding MSVC puts
  between functions. Crude, but reliable for this compiler.

```python
import xr
pe, base, img, imports, refs = xr.load('eqgame.exe')
print([hex(a) for a in refs[0xDE0BE4]])        # who touches MouseSensitivity
print(xr.dis(img, base, 0x516D40, 0x100, imports))
```

Limits worth knowing: a linear sweep desynchronizes on jump tables and padding, so
some listings start with garbage instructions (you can see that in several dumps
below), and it cannot see indirect calls. Virtual calls (`call [reg+N]`) were found
by *pattern* searches over the instruction stream instead of xrefs, for example
"`call [reg+0x24]` with two `fstp [esp]` float pushes in the preceding 14
instructions".

The other half of the toolkit was **logging from the DLL itself** and asking for
controlled in-game experiments (capped vs uncapped frame rate, one ini option
toggled at a time). Two of the fixes would have stayed wrong without that.

## General approach

1. **Turn the symptom into a hypothesis about the math.** "Turns slower at high
   FPS" and "jumps lower at high FPS" both say *a per-frame quantity is being
   treated as a per-second one*, but in different ways. Writing down what the
   frame-rate dependence should look like (1/fps, a cap, a threshold) tells you
   what code shape to look for.
2. **Find an anchor.** Strings (ini keys, UI element names), imports (`D3DX*`,
   `SetWindowPos`), exports (`EQG_GetCpuSpeed2`), distinctive float constants, or
   a known-good binary diff. Every fix started from one of these.
3. **Follow references outward** from the anchor until reaching the code that
   implements the behavior, and **decode the math** completely, including every
   constant.
4. **Check the model against the symptom numerically** before patching. The
   aspect bug predicted a ~37% stretch at 21:9; the CPU bug predicted a factor of
   ~11 at 4.7 GHz. When the numbers match the complaint, you have the right code.
5. **Patch the smallest thing that fixes the root cause**, verify the original
   bytes at runtime before writing, and log what was installed.
6. **Verify in the running game**, with instrumentation when the result is not
   obviously right.

## The fixes

### CPU speed (Ryzen "game runs too fast")

*Anchor: a known-good patched binary.*

xackery's eq-core-dll release ships a patched `EQGraphicsDX9.dll`. The user's install
had both that file and the original (`EQGraphicsDX9.dll.backup`), so the first step
was a byte diff: six small regions, all inside two exported functions,
`EQG_GetCpuSpeed2` (`0x10011CC0`) and `EQG_GetCpuSpeed3` (`0x10011D30`).

Disassembling original and patched side by side made the bug obvious. Both
functions take two `rdtsc` readings about one second apart, but only keep `eax`:

```
rdtsc
mov [esp+0xc], eax          ; low 32 bits only
...
mov ecx, [esp+0xc]
sub ecx, [esp+0x10]         ; 32-bit delta wraps above 2^32 ticks/s (~4.29 GHz)
mov eax, 0x10624DD3 ; mul ecx ; shr edx, 6     ; /1000 -> ticks per ms
```

The patch keeps `edx` too, does a 64-bit subtract (`sub`/`sbb`) and pre-shifts with
`shrd`. `eqgame.exe` turns the result into ticks per millisecond (`0x809820` stores
it as an `int64` at `0x15D3618`), which drives all frame timing.

Confirming it: a 20-line test program called the *original* export under Wine on the
user's Ryzen 9 7900X. True TSC rate: 4,699,946 ticks/ms. Original function: 409,712,
which is `(4.70e9 − 2^32) / 1000`. The game thought the CPU was ~11× slower than it is.

**Patch:** a 5-byte `jmp` over each function's prologue to one replacement that
measures the TSC against `QueryPerformanceCounter` over 250 ms with 64-bit math. It is
applied from a `LoadLibraryA` IAT hook the moment the game loads the graphics DLL
(found by following xrefs to the string `"EQGraphicsDX9.DLL"` to `0x8D908A`).

Worth noting: eq-core-dll's earlier attempt overwrote the ticks-per-ms global with
`QueryPerformanceFrequency`, which is a different clock (10 MHz on Wine), hence its
"side effects".

### Ultrawide aspect ratio

*Anchor: the projection matrix import.*

**Dead end first.** Scanning `.rdata` for aspect-like float constants found 1.6 (16:10)
in eqgame. All of its uses turned out to be unrelated gameplay values. Constant scans
are cheap, but here they only produced noise.

**What worked:** `EQGraphicsDX9.dll` imports `D3DXMatrixPerspectiveRH(w, h, zn, zf)`.
Its import thunk has exactly two callers. The main one (`0x10006470`) builds
`w = 2·tan(hfov/2)` and `h = w / camera[+8]`, with `hfov` read from `camera[+4]` and
converted by `0.703125 · π/180`. `0.703125` is 360/512, the giveaway that angles are in
EQ's 512-per-circle units.

To find what writes `camera[+8]`, I located the camera's vtable by searching `.rdata`
for runs of code pointers that include functions next to `0x10006470`. Slot `0x24`
(`0x10006140`) is the setter:

```
camera[+4] = 0.5 * hfov
camera[+8] = hfov / vfov       ; "aspect" is a ratio of two angles
```

Then I searched eqgame for virtual `call [reg+0x24]` sites with two float pushes and an
`fdiv` nearby. That found two copies of the same routine (`0x48AC70` and an inlined
copy at `0x48F300`). Decoding the x87 code, with constants
`0.7501875 ≈ 3/4`, `1.4222 = 512/360` and `200.0` (a 140.6° clamp), gave:

```
r = viewport width / height
if r < 2.0:  SetFov(F, F / r)                                   # exact
else:        vfov = 0.75·F
             hfov = min(2·atan(tan(vfov/2)·r), 200 units)
             SetFov(hfov, vfov)                                 # aspect = hfov/vfov ≠ r
```

At 21:9 with F = 90°, the ultrawide branch passes ~1.70 as the aspect instead of 2.33, a
~37% horizontal stretch, which matched the report.

**Patch:** NOP the two-byte `jne` that selects the ultrawide branch (`0x48ACF9`,
`0x48F366`), so the exact branch always runs. For Hor+, replace camera vtable slot `0x24`
with a function that widens `hfov` past 16:9 while keeping the true aspect.

#### Reading x87 comparisons

Most of the game math is x87, so decoding `fcom`/`fnstsw ax`/`test ah, …` quickly
matters. With `st0` compared to `src`: C0 (`ah & 0x01`) means `st0 < src`, C3
(`ah & 0x40`) means equal, and C2 (`ah & 0x04`) means unordered.

| Sequence | Jumps when |
| --- | --- |
| `test ah, 0x05` / `jp` | `st0 >= src` |
| `test ah, 0x05` / `jnp` | `st0 < src` |
| `test ah, 0x41` / `jne` | `st0 <= src` |
| `test ah, 0x41` / `je` | `st0 > src` |
| `test ah, 0x44` / `jnp` | `st0 == src` |

Track the stack depth on paper for every `fld`/`fstp`/`fxch`. The aspect routine reorders
its stack three times before the call, and which value ends up as which argument is the
whole bug.

### Window resize under Wine

*Anchor: window-management imports.*

- `RegisterClassA` has one caller. The `WNDCLASS` built on the stack just before it
  gives the window procedure (`0x5FCD80`) and the class name (`_EverQuestwndclass`).
- The window procedure dispatches through a byte-index table plus a jump table
  (`movzx ecx, byte [eax+0x5FD8B0]` / `jmp [ecx*4+0x5FD87C]`). Decoding both tables
  maps each `WM_*` message to its handler. `WM_SIZE` only tracks minimize, maximize
  and restore; nothing re-applies the resolution.
- The cluster of `AdjustWindowRect`, `SetWindowPos` and `GetClientRect` calls led to
  the resolution handler's apply routine (`0x8D9FD0`), which sizes the window, reads
  the client rect and resets the D3D device only when the size changed. The handler
  object lives at `0x15D46CC`. Its vtable (`0x9DC96C`, found from the constructor that
  stores it) has slot `0x28` = `ApplyResolution(w, h, windowed)` at `0x5B92A0`, the
  routine the options window uses. It updates the resolution globals, the viewport
  (`0x4EB860`), the UI and the camera FOV before calling apply.
- The mouse code (`0x5FA7F0`) uses `GetCursorPos` + `ScreenToClient` against the real
  client rect, which explained the cursor mismatch once backbuffer and window sizes
  diverge.

**Patch:** an IAT hook on `CreateWindowExA` subclasses the main window. The first
version applied the new size on every `WM_SIZE`. The log then showed about 20 device
resets per drag, because Wine's window manager sends no `WM_ENTERSIZEMOVE` or
`WM_EXITSIZEMOVE`. The final version debounces with a 300 ms timer and only applies at
character select, character create or in-game (game state at `[0xE67CCC]+0x5C8`). State
numbers 1 and 2 come from the game's own `ApplyResolution` branches; 5 = in-game is the
MacroQuest convention, confirmed in testing.

(A "missing models" problem during this work turned out to be the launch environment,
plain `wine` instead of Lutris, not the patch. Bisecting with ini toggles is the
cheapest way to rule a patch in or out.)

### Mouse-look speed

*Anchor: the `MouseSensitivity` ini key.*

- Xrefs to the string `MouseSensitivity` lead to the ini load, which stores the value
  in `0xDE0BE4`, and to the options slider's save path, which writes it there *and* to
  a copy at `0xDDF69C`. The copy is the one the look code reads.
- Readers of `0xDDF69C` include `0x516D40`, which reads the mouse deltas at
  `0xDDF690`/`0xDDF694`. Decoding its constants gives
  `turn = dx / viewport_width · 512 · ((sens − 1)/7 · 1.5 + 0.5)`. That is linear and
  has no frame-rate term, so the bug had to be downstream.
- The key line was the store: `fstp [controlled_spawn + 0x8C]`. The turn is written to
  a field next to the heading, not to the heading itself.
- To see what consumes `+0x8C`, I searched for `fld [reg+0x8C]` followed by an `fmul`
  and an `fadd` into a heading-like field. Nothing came up, because the physics code
  addresses the spawn through `spawn + 0x64`, so the same fields show up as `+0x28` and
  `+0x1C`. Counting which offsets appear near uses of the `512.0` constant (heading
  wraps at 512) exposed the base shift. Re-running the search with the shifted offsets
  found the integrator at `0x8D1E80`:
  `heading += speed_heading · t`, wrapped to [0, 512).

So a per-frame mouse displacement was stored as a velocity and then multiplied by frame
time: total turn ∝ 1/fps. Zeal's equivalent fix for the old client
(`camera_mods.cpp`) confirmed the diagnosis. It adds the delta to `Heading` directly and
leaves a tiny `SpeedHeading`.

**Patch:** each of the four `fstp dword ptr [eax+0x8C]` sites (6 bytes) becomes
`call stub` + `nop`. The stub pops the value off the x87 stack and calls a C function
that applies it to `Heading`, scaled to the 30 fps feel. Before writing the stub, I
checked that `eax`, `ecx` and `edx` are dead after every site, which made a plain cdecl
call safe.

### Jumping, falling and levitation

*Anchor: the heading integrator, plus runtime traces.*

This one took four iterations, and the runtime evidence corrected the static reading
twice.

1. **Model from the symptoms.** If gravity were per-frame, high FPS would fall
   *faster*. Lower jumps, no fall damage and lost levitation bob instead point to
   per-frame *damping*. The physics step (`0x8D11A0`, slot `0x14` of the vtable at
   `0x9D7100`) integrates position correctly (`pos += vel · t`, `t = ms · 0.02`), but
   its friction routine (`0x8D2160`) does `vel = vel · f + accel` once per call with no
   `t`. The coefficient routine `0x8D09D0` returns fixed constants per environment
   (0.99 air, 0.85/0.1 levitation, …).
2. **First fix: rate-limit the step to 60 Hz**, Zeal's approach for the old client.
   It didn't work. Rather than guess, I added counters: 3,717 calls per 10 s, 3,134
   skipped. The limiter worked as designed, so the model was incomplete.
3. **Per-frame trace** of the player's `z` and `vz` (`spawn+0x6C`, `+0x78`) at
   uncapped and at 30 fps. At 30 fps there was a clean ballistic arc. Uncapped, the jump
   impulse was zeroed on the next frame, and falling speed stayed fixed at −0.13 instead
   of accumulating. Something *outside* the step was resetting vertical velocity on the
   frames where the step was skipped. The step is reached through a virtual getter
   (slot `0x78` returns the physics object), and searching for "call `[reg+0x78]`, then
   call `[reg+0x14]` with a spawn argument" found the per-frame movement block
   (`0x49CCB7`, `0x49CCE7`). Right after each step it calls `0x509050`, a collision and
   ground pass that can zero `vz`. **Fix: skip the collision pass whenever the step was
   skipped.** Jumps and ledges were then correct.
4. **Fall damage still varied** (about 33 uncapped, 40 at 60 fps, 53 at 30 fps). Fitting the 30 fps trace gave
   `vz ← 0.98·vz − 0.54` per step, and the coefficient routine confirmed that gravity
   is scaled by `t` but friction is not. Terminal velocity is therefore
   `g·t/(1 − f)`, proportional to step length, so *any* variable step length changes
   it. **Final fix: fixed-timestep stepping.** The step runs a whole number of times per
   frame, each exactly 1000/60 ms (alternating 16 and 17 ms to average exactly), with
   the collision pass between steps and the remainder carried to the next frame. Damage
   became identical at 60 fps and uncapped. The leftover 8% gap at 30 fps or lower was
   accepted: it is probably per-frame fall tracking outside the physics.

The lesson: a confident static reading ("the damping is inside the step, so limiting
the step fixes it") was wrong twice. Ten minutes of tracing beat another hour of
reading.

### Max FPS slider and frame limiter

*Anchor: UI element names.*

The UI XML (`EQUI_AdvancedDisplayOptionsWnd.xml`) defines `ADOW_MaxFPSSlider` with no
range, so the range is set in code. Xrefs to the strings `ADOW_MaxFPSSlider`,
`Unlimited` and `MaxFPS` led to:

- the slider setup at `0x616DCC`: `push 0x5B; call 0x896930` (`SetRange(91)`, which is
  10–100);
- the change handler at `0x6156A8`: `fps = value + 10`, label "Unlimited" when
  `fps == 100`, `frame_ms = 1000 / fps`;
- the limiter at `0x517EE0`: `Sleep(frame_ms − elapsed)` in whole milliseconds, with
  `MaxFPS == 100` meaning unlimited (still `Sleep(1)`).

Two encoding details shaped the patch. `push imm8` and `cmp r32, imm8` sign-extend, so
`0xBF` (191) or `0xC8` (200) can't be written in place, and the 5-byte long forms don't
fit. `SetRange` is shared by 29 sliders, so hooking it was out. Instead:

- redirect only *this* `call SetRange` site to a wrapper that passes 191;
- replace the 5-byte `cmp edi, 0x64; jne +0x0C` with `call stub`, where the stub compares
  with 200 and either returns (the "Unlimited" path is the next instruction) or adds
  `0x0C` to its return address (the numeric path);
- replace the limiter with a `QueryPerformanceCounter`-paced one, since whole-millisecond
  sleeps can't represent 144 fps.

### VSync

A search of the graphics DLL for `mov …, 0x80000000` (`D3DPRESENT_INTERVAL_IMMEDIATE`)
found it hard-coded into the present parameters at `0x10098DFB`. **Patch:** hook the
graphics DLL's `Direct3DCreate9` import, then the COM vtables: `IDirect3D9::CreateDevice`
(slot 16) and `IDirect3DDevice9::Reset` (slot 16). Both rewrite
`PresentationInterval`.

## Patching techniques used

| Technique | Where | Notes |
| --- | --- | --- |
| 5-byte `jmp` over a prologue | CPU speed, frame limiter | Full replacement only; check the prologue has no relocated bytes |
| IAT slot swap | `LoadLibraryA`, `CreateWindowExA`, `Direct3DCreate9` | Cleanest hook when the game calls through its own IAT |
| Vtable slot swap | camera `SetFov`, physics step, D3D `CreateDevice`/`Reset` | Affects every instance of the class, which was always what was wanted |
| Call-site redirection | collision pass (6 sites), `SetRange` | Retarget one `call rel32` without touching a shared function |
| `call` + `nop` over an instruction | mouse-look stores | Needs a naked stub and dead scratch registers after the site |
| Return-address arithmetic in a stub | FPS label check | Replaces a compare-and-branch when the branch doesn't fit |
| Byte NOP | ultrawide branch | Smallest possible change when one branch is simply wrong |

Practical details:

- **`thiscall` from C++ without MSVC:** declare the replacement `__fastcall` with a
  dummy second parameter. `ecx` is `this`, `edx` is ignored, and stack arguments and
  callee cleanup match.
- **Check the `ret N` of every hook** in the compiled object (`llvm-objdump -d` on the
  `.obj`, where symbols survive). A wrong stack cleanup crashes the game far from the
  bug.
- **Verify before writing:** every site's original bytes are compared at runtime,
  skipping bytes that contain relocated absolute addresses (or comparing them against
  the rebased value). A mismatch logs both byte strings and skips that fix.
- **Gate on PE timestamps** of `eqgame.exe` and `EQGraphicsDX9.dll`, so a different build
  never gets patched by accident.

## Pitfalls hit along the way

- A helper script named `dis.py` shadowed Python's standard `dis` module and broke
  capstone's import. Name helpers something unique.
- Struct fields can appear at two offsets when code uses a pointer into the middle of
  the struct (`spawn + 0x64` here). If a search for a known field finds nothing, try
  likely interior bases.
- Linear-sweep listings that start mid-instruction look plausible but are garbage.
  Re-disassemble from a known function start before trusting them.
- Under Wine, a `dinput8.dll` proxy needs the `dinput8=n,b` override, and Lutris and a
  bare `wine` launch can behave differently. zig's compiler cache does not work under
  Wine at all, so the Windows build could only be smoke-tested there.
