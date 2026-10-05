#pragma once
// Addresses for the RoF2 client: eqgame.exe built 2013-05-11 (PE timestamp 0x518DE58F)
// and its EQGraphicsDX9.dll (PE timestamp 0x518DE5B6).
//
// eqgame.exe values are absolute addresses at the preferred image base (0x400000);
// use Rebase() before touching them. EQGraphicsDX9.dll values are RVAs.

#include <cstdint>

namespace addr {

constexpr uint32_t kEqgameTimestamp = 0x518DE58F;
constexpr uint32_t kEqgamePreferredBase = 0x400000;
constexpr uint32_t kGfxTimestamp = 0x518DE5B6;

// ---- eqgame.exe ----------------------------------------------------------

// CDisplay::UpdateCameraFov (thiscall, no args) and a second inlined copy of the
// same math. Both pick between two ways of building (hfov, vfov) for the camera:
//   aspect <  2.0 : SetFov(fov, fov / aspect)                         (correct)
//   aspect >= 2.0 : SetFov(hfov', fov * 0.75) with hfov' from atan()  (broken: the
//                   graphics DLL derives aspect as hfov'/vfov, which is not w/h)
// The fix NOPs the "aspect >= 2.0" jne so the correct branch is always taken.
constexpr uint32_t kFovUltrawideBranch1 = 0x48ACF9;  // 75 18  jne +0x18
constexpr uint32_t kFovUltrawideBranch2 = 0x48F366;  // 75 18  jne +0x18

// CResolutionHandler* (window/device mode manager).
constexpr uint32_t kResolutionHandlerPtr = 0x15D46CC;
constexpr uint32_t kResolutionHandlerVtbl = 0x9DC96C;
constexpr uint32_t kApplyResolution = 0x5B92A0;  // vtbl+0x28: (int w, int h, bool flag), ret 0xC
namespace res_handler {
constexpr uint32_t kFullscreen = 0x04;  // bool
constexpr uint32_t kWidth = 0x18;       // windowed client width
constexpr uint32_t kHeight = 0x1C;      // windowed client height
constexpr uint32_t kPosX = 0x20;
constexpr uint32_t kPosY = 0x24;
constexpr uint32_t kVtblApplyResolution = 0x28;
}  // namespace res_handler

// CGraphicsEngine* created from EQGraphicsDX9.dll; null until the engine exists.
constexpr uint32_t kGraphicsEnginePtr = 0x15D46A4;

// CEverQuest* and its game state field. ApplyResolution itself branches on states 1/2.
constexpr uint32_t kEverQuestPtr = 0xE67CCC;
constexpr uint32_t kEverQuestGameState = 0x5C8;
constexpr int kGameStateCharSelect = 1;
constexpr int kGameStateCharCreate = 2;
constexpr int kGameStateInGame = 5;

// Window class registered for the main game window.
constexpr uint32_t kMainWindowClassName = 0x9DD6E0;  // "_EverQuestwndclass"

// Current resolution globals written by ApplyResolution.
constexpr uint32_t kCurrentWidth = 0xDDF658;
constexpr uint32_t kCurrentHeight = 0xDDF65C;
constexpr uint32_t kCurrentModeFlag = 0xDDF668;  // third arg of ApplyResolution

// IAT slots in eqgame.exe.
constexpr uint32_t kIatLoadLibraryA = 0x9C0220;
constexpr uint32_t kIatCreateWindowExA = 0x9C030C;

// ---- EQGraphicsDX9.dll (RVAs) ----------------------------------------------

// Exported EQG_GetCpuSpeed2 / EQG_GetCpuSpeed3. Both return TSC ticks per
// millisecond, measured over ~1 s using only the low 32 bits of RDTSC. That
// overflows once the TSC runs faster than ~4.29 GHz, so the game believes the
// CPU is slower than it is and runs too fast.
constexpr uint32_t kGfxGetCpuSpeed2 = 0x11CC0;
constexpr uint32_t kGfxGetCpuSpeed3 = 0x11D30;

// Camera vtable; slot 0x24 is SetFov(float hfov, float vfov) (thiscall, ret 8).
// It stores half of hfov and aspect = hfov / vfov, which the projection builder
// (RVA 0x6470) turns into D3DXMatrixPerspectiveRH(2*tan(hfov/2), 2*tan(hfov/2)/aspect).
// Angles are in EQ units: 512 per full circle.
constexpr uint32_t kGfxCameraVtbl = 0x131C3C;
constexpr uint32_t kGfxCameraSetFovSlot = 0x24;
constexpr uint32_t kGfxCameraSetFov = 0x6140;

}  // namespace addr
