// Fixes horizontal stretching at aspect ratios of 2.0 and wider.
//
// eqgame.exe hands the camera (hfov, vfov) and the graphics DLL derives the
// projection's aspect ratio as hfov / vfov. Below 2.0 the game passes
// (fov, fov / aspect), which is exact. At 2.0 and above it switches to a Hor+ scheme
// that passes (2*atan(tan(0.375*fov) * aspect), 0.75*fov): the ratio of two angles,
// not the ratio of the screen's sides. At 21:9 with a 90 degree FOV that is ~1.70
// instead of 2.33, so everything is stretched ~37% horizontally.
//
// "Correct" mode NOPs that branch so the exact path is always used. "HorPlus" mode
// additionally hooks the camera's SetFov to widen the horizontal FOV past
// HorPlusBaseAspect while keeping the vertical FOV of the base aspect.

#include <cmath>
#include <cstdint>
#include <initializer_list>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace aspect_ratio {
namespace {

// test ah, 41h; jne +18h. Only the jne is replaced.
const uint8_t kBranchSig[] = {0xF6, 0xC4, 0x41, 0x75, 0x18};
const uint8_t kNop2[] = {0x90, 0x90};

// fld dword ptr [esp+4]; fld dword ptr [...]  (the operand address is relocated)
const uint8_t kSetFovPrologue[] = {0xD9, 0x44, 0x24, 0x04, 0xD9, 0x05};

constexpr double kPi = 3.14159265358979323846;
constexpr double kEqUnitsToRadians = 2.0 * kPi / 512.0;
constexpr double kMaxHalfFovRadians = 80.0 * kPi / 180.0;

using SetFovFn = void(__fastcall*)(void* camera, void* edx, float hfov, float vfov);
SetFovFn g_original_set_fov = nullptr;

// thiscall emulated via fastcall: ecx = camera, edx unused, two stack args, callee pops 8.
void __fastcall SetFovHook(void* camera, void* edx, float hfov, float vfov) {
  if (vfov > 0.0f && hfov > 0.0f) {
    double aspect = static_cast<double>(hfov) / vfov;
    double base = g_config.hor_plus_base_aspect;
    if (aspect > base + 1e-3) {
      double half = 0.5 * hfov * kEqUnitsToRadians;
      double wide_half = std::atan(std::tan(half) * aspect / base);
      if (wide_half > kMaxHalfFovRadians) wide_half = kMaxHalfFovRadians;
      float wide_hfov = static_cast<float>(2.0 * wide_half / kEqUnitsToRadians);
      static int logged = 0;
      if (logged < 8) {
        ++logged;
        Log("aspect_ratio: camera %p aspect %.3f, hfov %.1f -> %.1f units", camera, aspect,
            hfov, wide_hfov);
      }
      hfov = wide_hfov;
      vfov = static_cast<float>(wide_hfov / aspect);
    }
  }
  g_original_set_fov(camera, edx, hfov, vfov);
}

}  // namespace

void ApplyEqgame() {
  if (g_config.aspect_mode == AspectMode::Off) return;
  for (uint32_t branch : {addr::kFovUltrawideBranch1, addr::kFovUltrawideBranch2}) {
    uintptr_t sig = mem::Rebase(branch) - 3;
    if (!mem::Verify(sig, kBranchSig, sizeof(kBranchSig), "aspect_ratio (ultrawide branch)")) return;
  }
  for (uint32_t branch : {addr::kFovUltrawideBranch1, addr::kFovUltrawideBranch2})
    mem::Write(mem::Rebase(branch), kNop2, sizeof(kNop2));
  Log("aspect_ratio: ultrawide FOV branch disabled");
}

void ApplyGfx(uintptr_t gfx_base) {
  if (g_config.aspect_mode != AspectMode::HorPlus) return;
  uintptr_t slot = gfx_base + addr::kGfxCameraVtbl + addr::kGfxCameraSetFovSlot;
  uintptr_t set_fov = gfx_base + addr::kGfxCameraSetFov;
  if (*reinterpret_cast<uintptr_t*>(slot) != set_fov) {
    Log("aspect_ratio: camera vtable slot holds %08X, expected %08X; Hor+ not installed",
        static_cast<unsigned>(*reinterpret_cast<uintptr_t*>(slot)), static_cast<unsigned>(set_fov));
    return;
  }
  if (!mem::Verify(set_fov, kSetFovPrologue, sizeof(kSetFovPrologue), "aspect_ratio (SetFov)"))
    return;
  g_original_set_fov = reinterpret_cast<SetFovFn>(set_fov);
  mem::WriteValue(slot, reinterpret_cast<uintptr_t>(&SetFovHook));
  Log("aspect_ratio: Hor+ installed (base aspect %.4f)", g_config.hor_plus_base_aspect);
}

}  // namespace aspect_ratio
