// Makes right-click mouse-look turning independent of the frame rate.
//
// The client turns the mouse's per-frame X delta into a turn amount and stores it as
// the controlled spawn's *turn speed* (SpeedHeading). Physics then integrates
// heading += SpeedHeading * (elapsed_ms * 0.02) every frame. Since the per-frame mouse
// delta already shrinks with frame time, heading changes by delta * dt, so the total
// turn for a given mouse movement falls off as 1 / fps: 10x the frame rate turns 10x
// slower. Pitch is applied directly and does not have this problem.
//
// We replace each store of the mouse turn speed with a call that applies the turn to
// Heading immediately, scaled to match the original feel at 30 fps, and leaves only a
// token SpeedHeading so the client still treats the spawn as turning.

#include <cmath>
#include <cstdint>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace mouse_look {
namespace {

// fstp dword ptr [eax+0x8C]
const uint8_t kStoreSpeedHeading[] = {0xD9, 0x98, 0x8C, 0x00, 0x00, 0x00};

// Physics time factor for one frame at 30 fps: (1000 / 30) ms * 0.02.
constexpr float kReferenceTimeFactor = (1000.0f / 30.0f) * 0.02f;
constexpr float kHeadingUnits = 512.0f;

}  // namespace

// cdecl, called from MouseTurnStoreStub with the spawn from eax and the value the
// client was about to store as SpeedHeading.
extern "C" void __cdecl MouseLookApplyTurn(uint8_t* spawn, float speed) {
  float& heading = *reinterpret_cast<float*>(spawn + addr::spawn::kHeading);
  float& speed_heading = *reinterpret_cast<float*>(spawn + addr::spawn::kSpeedHeading);
  float turned = heading + speed * kReferenceTimeFactor * g_config.mouse_look_scale;
  turned = std::fmod(turned, kHeadingUnits);
  if (turned < 0.0f) turned += kHeadingUnits;
  heading = turned;
  speed_heading = speed / 1000.0f;
}

// Replaces `fstp dword ptr [eax+0x8C]` (via call + nop). On entry st(0) holds the turn
// speed and eax the controlled spawn; eax/ecx/edx are dead after every patched site.
extern "C" __attribute__((naked)) void MouseTurnStoreStub() {
  asm volatile(
      "subl $4, %esp\n\t"
      "fstps (%esp)\n\t"
      "pushl %eax\n\t"
      "call _MouseLookApplyTurn\n\t"
      "addl $8, %esp\n\t"
      "ret\n\t");
}

void ApplyEqgame() {
  if (!g_config.mouse_look_fix) return;
  for (uint32_t site : addr::kMouseTurnSpeedStores) {
    if (!mem::Verify(mem::Rebase(site), kStoreSpeedHeading, sizeof(kStoreSpeedHeading),
                     "mouse_look (SpeedHeading store)"))
      return;
  }
  for (uint32_t site : addr::kMouseTurnSpeedStores) {
    uintptr_t at = mem::Rebase(site);
    mem::WriteCall(at, reinterpret_cast<void*>(&MouseTurnStoreStub));
    mem::Write(at + 5, "\x90", 1);  // the replaced store was 6 bytes
  }
  Log("mouse_look: installed (scale %.2f)", g_config.mouse_look_scale);
}

}  // namespace mouse_look
