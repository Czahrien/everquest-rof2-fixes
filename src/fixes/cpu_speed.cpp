// Replaces EQG_GetCpuSpeed2/3 in EQGraphicsDX9.dll.
//
// eqgame.exe calls both at startup and stores the result as its TSC ticks per
// millisecond, which drives all of its frame timing. The originals subtract two
// RDTSC readings taken ~1 s apart but keep only the low 32 bits, so any TSC faster
// than 2^32 ticks/s (~4.29 GHz) wraps and the game is told the CPU is slower than
// it is. Ryzen parts commonly have TSCs in that range, hence "runs too fast".

#include <windows.h>
#include <x86intrin.h>

#include <cstdint>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace cpu_speed {
namespace {

uint32_t MeasureTscTicksPerMs() {
  static uint32_t cached = 0;
  if (cached) return cached;

  // Keep both TSC reads on one core in case the TSC is not synchronized across cores.
  HANDLE thread = GetCurrentThread();
  DWORD_PTR old_affinity = SetThreadAffinityMask(thread, 1);

  LARGE_INTEGER freq, qpc0, qpc1;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&qpc0);
  uint64_t tsc0 = __rdtsc();
  Sleep(250);
  uint64_t tsc1 = __rdtsc();
  QueryPerformanceCounter(&qpc1);

  if (old_affinity) SetThreadAffinityMask(thread, old_affinity);

  double elapsed_ms = (qpc1.QuadPart - qpc0.QuadPart) * 1000.0 / freq.QuadPart;
  double ticks_per_ms = (tsc1 - tsc0) / elapsed_ms;
  cached = static_cast<uint32_t>(ticks_per_ms + 0.5);
  Log("cpu_speed: measured TSC at %u ticks/ms (%.3f GHz) over %.1f ms", cached,
      ticks_per_ms / 1e6, elapsed_ms);
  return cached;
}

extern "C" uint32_t __cdecl GetCpuSpeedReplacement() { return MeasureTscTicksPerMs(); }

// Function prologues; neither contains relocated bytes.
const uint8_t kSpeed2Prologue[] = {0x83, 0xEC, 0x48, 0x53, 0x56};  // sub esp,48h; push ebx; push esi
const uint8_t kSpeed3Prologue[] = {0x83, 0xEC, 0x48, 0x56, 0x57};  // sub esp,48h; push esi; push edi

}  // namespace

void ApplyGfx(uintptr_t gfx_base) {
  if (!g_config.cpu_speed_fix) return;
  uintptr_t speed2 = gfx_base + addr::kGfxGetCpuSpeed2;
  uintptr_t speed3 = gfx_base + addr::kGfxGetCpuSpeed3;
  if (!mem::Verify(speed2, kSpeed2Prologue, sizeof(kSpeed2Prologue), "cpu_speed (EQG_GetCpuSpeed2)") ||
      !mem::Verify(speed3, kSpeed3Prologue, sizeof(kSpeed3Prologue), "cpu_speed (EQG_GetCpuSpeed3)"))
    return;
  mem::WriteJump(speed2, reinterpret_cast<void*>(&GetCpuSpeedReplacement));
  mem::WriteJump(speed3, reinterpret_cast<void*>(&GetCpuSpeedReplacement));
  Log("cpu_speed: installed");
}

}  // namespace cpu_speed
