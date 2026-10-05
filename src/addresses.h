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

// Mouse-look (CEverQuest mouse handler at 0x516D40). The four places it stores the
// mouse X turn into the controlled spawn's SpeedHeading, each `fstp [eax+0x8C]`.
// Physics (0x8D1E80) later does Heading += SpeedHeading * elapsed_ms * 0.02.
constexpr uint32_t kMouseTurnSpeedStores[] = {0x516E66, 0x516E7A, 0x516F71, 0x516F84};
namespace spawn {
constexpr uint32_t kPhysicsTimer = 0x30;  // game time (ms) of the last physics step
constexpr uint32_t kHeading = 0x80;       // 0..512
constexpr uint32_t kSpeedHeading = 0x8C;  // heading units per 50 ms tick
constexpr uint32_t kMount = 0x154;        // spawn being ridden, or null
}  // namespace spawn

// Movement physics. The object (vtable 0x9D7100) lives inside a larger game object;
// slot 0x14 is the per-spawn step: thiscall (spawn*), ret 4. It computes
// t = (now - spawn->PhysicsTimer) * 0.02, then applies friction/acceleration once
// (0x8D2160, not time scaled) and integrates position by t.
constexpr uint32_t kPhysicsVtbl = 0x9D7100;
constexpr uint32_t kPhysicsStepSlot = 0x14;
constexpr uint32_t kPhysicsStep = 0x8D11A0;
constexpr float kPhysicsMsToTicks = 0.02f;
// Per-spawn collision/ground pass run right after each physics step (thiscall on the
// spawn, no args, returns 1). It can zero vertical velocity, so it must not run on
// frames where the step was skipped. These are all of its call sites.
constexpr uint32_t kSpawnCollision = 0x509050;
constexpr uint32_t kSpawnCollisionCalls[] = {0x4878B0, 0x4878E1, 0x49CC96,
                                             0x49CCD2, 0x49CD02, 0x49D49D};
// The collision pass stamps the game clock into the display object every call.
constexpr uint32_t kDisplayPtr = 0xDD2660;
constexpr uint32_t kDisplayLastMoveTime = 0x154;
constexpr uint32_t kGameClockMs = 0x809810;  // cdecl, no args
// Timer object whose vtable slot 0 (thiscall, no args) returns game time in ms.
constexpr uint32_t kGameTimerPtr = 0x15D4418;
// Local player spawn, and the physics time factor stored for it each step (read by camera code).
constexpr uint32_t kLocalPlayerPtr = 0xDD2630;
constexpr uint32_t kLocalPhysicsTimeFactor = 0xDE0A68;

// Mouse-look. The flag is set while right-click mouse-look has the cursor hidden; the
// cursor position at engage is saved at [0x15D3D00]+0x130/0x134 and restored on release.
constexpr uint32_t kMouseLookActive = 0xDDF702;  // byte
constexpr uint32_t kCursorSaveHolderPtr = 0x15D3D00;  // object holding the saved position
constexpr uint32_t kCursorSaveX = 0x130;              // screen coordinates
constexpr uint32_t kCursorSaveY = 0x134;
constexpr uint32_t kMainWindow = 0xE67B08;       // HWND returned by CreateWindowExA
// Per-frame mouse input routine (DirectInput read; cdecl, no args) and its only call.
constexpr uint32_t kProcessMouse = 0x5F9E30;
constexpr uint32_t kProcessMouseCall = 0x539FDC;

// Frame limiter (cdecl, no args), called once per frame. Uses MaxFPS (100 = unlimited)
// or, while in the background, MaxBGFPS (100 = unlimited, 9 = paused).
constexpr uint32_t kFrameLimiter = 0x517EE0;
constexpr uint32_t kInBackground = 0xE67B42;  // byte
constexpr uint32_t kMaxFps = 0xDE0D1C;
constexpr uint32_t kMaxBackgroundFps = 0xDE0D20;
// Advanced Display options: MaxFPS slider setup and change handler.
constexpr uint32_t kSliderSetRange = 0x896930;            // CSliderWnd::SetRange, thiscall(int)
constexpr uint32_t kMaxFpsSliderSetRangeCall = 0x616DCE;  // call SetRange (after push 5Bh)
constexpr uint32_t kMaxFpsLabelCheck = 0x6156BD;          // cmp edi, 64h; jne +0Ch

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
