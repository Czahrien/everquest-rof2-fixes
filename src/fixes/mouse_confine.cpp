// Keeps the hidden cursor inside the game window during right-click mouse-look.
//
// Engaging mouse-look (0x518870) hides the system cursor and saves its position;
// releasing it (0x539F88) shows the cursor and moves it back. In between the camera is
// driven by DirectInput deltas, but nothing confines the hidden system cursor, so it
// keeps moving and can end up over another window or monitor. The client never calls
// ClipCursor.
//
// Once per frame, after the client reads mouse input, while mouse-look is active and the
// game is in the foreground we can:
//   Clip - ClipCursor to the client area, released as soon as either stops being true;
//   Pin  - move the cursor back to where mouse-look started, like exclusive DirectInput
//          does on Windows. Under Wine's X11 driver on XWayland, Clip only takes effect
//          for the most recently started client; XWayland locks the pointer when a client
//          warps a hidden cursor, which may cover the other clients.

#include <windows.h>

#include <cstdint>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace mouse_confine {
namespace {

using ProcessMouseFn = int(__cdecl*)();
ProcessMouseFn g_original_process_mouse = nullptr;
bool g_clipped = false;

template <typename T>
T& Global(uint32_t address) {
  return *reinterpret_cast<T*>(mem::Rebase(address));
}

void Release() {
  if (!g_clipped) return;
  Log("mouse_confine: released");
  ClipCursor(nullptr);
  g_clipped = false;
}

// Under Wine, cursor confinement depends on which display driver the process loaded.
const char* DisplayDriver() {
  if (GetModuleHandleA("winewayland.drv")) return "wine-wayland";
  if (GetModuleHandleA("winex11.drv")) return "wine-x11";
  if (GetModuleHandleA("winemac.drv")) return "wine-mac";
  return "native";
}

bool g_was_pinning = false;

void Pin(bool first_frame) {
  auto* holder = Global<uint8_t*>(addr::kCursorSaveHolderPtr);
  if (!holder) return;
  POINT target = {*reinterpret_cast<LONG*>(holder + addr::kCursorSaveX),
                  *reinterpret_cast<LONG*>(holder + addr::kCursorSaveY)};
  POINT current;
  if (!GetCursorPos(&current) || (current.x == target.x && current.y == target.y)) return;
  BOOL ok = SetCursorPos(target.x, target.y);
  static int logged_this_engage = 0;
  if (first_frame) logged_this_engage = 0;
  if (logged_this_engage < 2) {
    ++logged_this_engage;
    Log("mouse_confine: pinned cursor %ld,%ld -> %ld,%ld (%s)", current.x, current.y, target.x,
        target.y, ok ? "ok" : "refused");
  }
}

void Update() {
  HWND window = Global<HWND>(addr::kMainWindow);
  bool mouse_look = Global<uint8_t>(addr::kMouseLookActive) != 0;
  HWND foreground = GetForegroundWindow();

  static bool last_mouse_look = false;
  static HWND last_foreground = nullptr;
  if (mouse_look != last_mouse_look || (mouse_look && foreground != last_foreground)) {
    Log("mouse_confine: mouse-look %s, window %p, foreground %p, focus %p, active %p, driver %s",
        mouse_look ? "on" : "off", window, foreground, GetFocus(), GetActiveWindow(),
        DisplayDriver());
    last_mouse_look = mouse_look;
    last_foreground = foreground;
  }

  if (!mouse_look || !window || foreground != window || IsIconic(window)) {
    g_was_pinning = false;
    Release();
    return;
  }
  if (g_config.mouse_look_confine & kConfinePin) Pin(mouse_look != g_was_pinning);
  g_was_pinning = mouse_look;
  if (!(g_config.mouse_look_confine & kConfineClip)) return;

  RECT client;
  GetClientRect(window, &client);
  MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&client), 2);
  // Other programs (and Wine) can reset the clip at any time, so check every frame, but
  // only call ClipCursor when it actually differs.
  RECT current;
  if (!g_clipped || !GetClipCursor(&current) || !EqualRect(&current, &client)) {
    ClipCursor(&client);
    if (!g_clipped) Log("mouse_confine: clipped to %ld,%ld-%ld,%ld", client.left, client.top, client.right, client.bottom);
    g_clipped = true;
  }
}

// Replaces the frame loop's call to the client's mouse input routine (cdecl, no args).
int __cdecl ProcessMouseHook() {
  int result = g_original_process_mouse();
  Update();
  return result;
}

}  // namespace

void ApplyEqgame() {
  if (!g_config.mouse_look_confine) return;
  uintptr_t call = mem::Rebase(addr::kProcessMouseCall);
  auto* code = reinterpret_cast<const uint8_t*>(call);
  int32_t rel;
  __builtin_memcpy(&rel, code + 1, sizeof(rel));
  if (code[0] != 0xE8 || call + 5 + rel != mem::Rebase(addr::kProcessMouse)) {
    Log("mouse_confine: mouse input call does not match, not installing");
    return;
  }
  g_original_process_mouse = reinterpret_cast<ProcessMouseFn>(mem::Rebase(addr::kProcessMouse));
  mem::WriteCall(call, reinterpret_cast<void*>(&ProcessMouseHook));
  Log("mouse_confine: installed");
}

void Shutdown() { Release(); }

}  // namespace mouse_confine
