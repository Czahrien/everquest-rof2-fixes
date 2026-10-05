// Extends the Max FPS slider (Options > Advanced Display) from 10-99 + Unlimited to
// 10-199 + Unlimited, and replaces the frame limiter with one that can hit those rates.
//
// The client stores the slider as MaxFPS (10..100) with 100 meaning unlimited, and its
// limiter (0x517EE0) sleeps 1000/fps whole milliseconds per frame, which cannot
// represent rates like 144 (6 ms = 166 fps). We move "unlimited" to 200, widen the
// slider, and pace frames against QueryPerformanceCounter instead.

#include <windows.h>

#include <cstdint>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace frame_limiter {
namespace {

constexpr int kUnlimitedFps = 200;
constexpr int kOldUnlimitedFps = 100;   // MaxBGFPS keeps the client's original meaning
constexpr int kBackgroundPausedFps = 9;  // MaxBGFPS slider's lowest setting
constexpr int kSliderSteps = kUnlimitedFps - 10 + 1;

template <typename T>
T& Global(uint32_t address) {
  return *reinterpret_cast<T*>(mem::Rebase(address));
}

int64_t Now() {
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  return t.QuadPart;
}

void WaitForNextFrame(int fps) {
  static int64_t frequency = [] {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return f.QuadPart;
  }();
  static int64_t next_frame = 0;

  int64_t period = frequency / fps;
  int64_t now = Now();
  next_frame += period;
  // More than a frame behind (load, hitch, or first call): restart the schedule.
  if (next_frame < now - period || next_frame > now + period) next_frame = now;

  // Sleep in 1 ms slices while there is comfortable margin, then spin for precision.
  int64_t spin_threshold = frequency / 500;  // 2 ms
  for (int64_t remaining = next_frame - Now(); remaining > 0; remaining = next_frame - Now()) {
    if (remaining > spin_threshold)
      Sleep(1);
    else
      YieldProcessor();
  }
}

// Replaces the client's per-frame limiter (cdecl, no args).
extern "C" void __cdecl FrameLimiterReplacement() {
  bool in_background = Global<uint8_t>(addr::kInBackground) != 0;
  int background_fps = Global<int>(addr::kMaxBackgroundFps);
  if (in_background && background_fps != kOldUnlimitedFps) {
    if (background_fps == kBackgroundPausedFps) {
      Sleep(100);
      return;
    }
    if (background_fps > 0) WaitForNextFrame(background_fps);
    return;
  }
  int fps = Global<int>(addr::kMaxFps);
  if (fps <= 0 || fps >= kUnlimitedFps) {
    Sleep(1);  // what the client does when unlimited
    return;
  }
  WaitForNextFrame(fps);
}

// Replaces the slider's SetRange(91) call.
using SetRangeFn = void(__fastcall*)(void* slider, void* edx, int steps);
void __fastcall SetMaxFpsSliderRange(void* slider, void* edx, int /*steps*/) {
  reinterpret_cast<SetRangeFn>(mem::Rebase(addr::kSliderSetRange))(slider, edx, kSliderSteps);
}

}  // namespace

// Replaces `cmp edi, 100; jne +0Ch` in the slider handler (edi = fps). Returns to the
// "Unlimited" label path for 200 and skips 0x0C bytes ahead to the numeric path otherwise.
extern "C" __attribute__((naked)) void MaxFpsLabelCheck() {
  asm volatile(
      "cmpl $200, %edi\n\t"
      "jne 1f\n\t"
      "ret\n\t"
      "1:\n\t"
      "addl $0x0C, (%esp)\n\t"
      "ret\n\t");
}

void ApplyEqgame() {
  if (!g_config.extended_fps_slider) return;

  // cmp byte ptr [InBackground], 0
  uintptr_t limiter = mem::Rebase(addr::kFrameLimiter);
  auto* code = reinterpret_cast<const uint8_t*>(limiter);
  uint32_t operand;
  __builtin_memcpy(&operand, code + 2, sizeof(operand));
  const uint8_t kLabelCheck[] = {0x83, 0xFF, 0x64, 0x75, 0x0C};
  uintptr_t range_call = mem::Rebase(addr::kMaxFpsSliderSetRangeCall);
  int32_t range_rel;
  __builtin_memcpy(&range_rel, reinterpret_cast<const uint8_t*>(range_call) + 1, sizeof(range_rel));
  if (code[0] != 0x80 || code[1] != 0x3D || operand != mem::Rebase(addr::kInBackground) ||
      !mem::Verify(mem::Rebase(addr::kMaxFpsLabelCheck), kLabelCheck, sizeof(kLabelCheck),
                   "frame_limiter (label check)") ||
      *reinterpret_cast<const uint8_t*>(range_call) != 0xE8 ||
      range_call + 5 + range_rel != mem::Rebase(addr::kSliderSetRange)) {
    Log("frame_limiter: code does not match, not installing");
    return;
  }

  mem::WriteJump(limiter, reinterpret_cast<void*>(&FrameLimiterReplacement));
  int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(&SetMaxFpsSliderRange) - (range_call + 5));
  mem::Write(range_call + 1, &rel, sizeof(rel));
  mem::WriteCall(mem::Rebase(addr::kMaxFpsLabelCheck), reinterpret_cast<void*>(&MaxFpsLabelCheck));
  Log("frame_limiter: installed (slider 10-%d, %d = unlimited)", kUnlimitedFps - 1, kUnlimitedFps);
}

}  // namespace frame_limiter
