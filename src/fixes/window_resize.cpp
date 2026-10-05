// Makes resizing the game window change the render resolution.
//
// The client only changes resolution through its options window. Its WM_SIZE handler
// tracks minimize/maximize/restore and nothing else, so when a window manager (Wine's
// in particular) resizes the window, D3D keeps presenting the old backbuffer stretched
// into the new client area. Mouse input is read with ScreenToClient against the real
// client area, so the cursor and the game's idea of it drift apart.
//
// We subclass the main window and, once a resize settles, feed the new client size
// through the same CResolutionHandler::ApplyResolution the options window uses. That
// updates the resolution globals, viewport, UI and camera, and resets the D3D device.

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "../addresses.h"
#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace window_resize {
namespace {

constexpr UINT_PTR kApplyTimerId = 0x52F1;
constexpr UINT kApplyDelayMs = 300;
constexpr int kMinClientSize = 64;

using CreateWindowExAFn = HWND(WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND,
                                        HMENU, HINSTANCE, LPVOID);
CreateWindowExAFn g_original_create_window = nullptr;

HWND g_hwnd = nullptr;
WNDPROC g_original_wndproc = nullptr;
bool g_in_size_move = false;
bool g_applying = false;

template <typename T>
T& Field(void* object, uint32_t offset) {
  return *reinterpret_cast<T*>(static_cast<uint8_t*>(object) + offset);
}

template <typename T>
T& Global(uint32_t address) {
  return *reinterpret_cast<T*>(mem::Rebase(address));
}

void* ResolutionHandler() {
  void* handler = Global<void*>(addr::kResolutionHandlerPtr);
  if (!handler) return nullptr;
  auto vtbl = *static_cast<uintptr_t**>(handler);
  if (reinterpret_cast<uintptr_t>(vtbl) != mem::Rebase(addr::kResolutionHandlerVtbl) ||
      vtbl[addr::res_handler::kVtblApplyResolution / 4] != mem::Rebase(addr::kApplyResolution)) {
    static bool warned = false;
    if (!warned) {
      warned = true;
      Log("window_resize: resolution handler has unexpected vtable %p", vtbl);
    }
    return nullptr;
  }
  return handler;
}

void ScheduleApply(HWND hwnd);

int GameState() {
  void* everquest = Global<void*>(addr::kEverQuestPtr);
  return everquest ? Field<int>(everquest, addr::kEverQuestGameState) : -1;
}

void ApplyClientSize(HWND hwnd) {
  if (hwnd != g_hwnd || IsIconic(hwnd) || !Global<void*>(addr::kGraphicsEnginePtr)) return;
  int state = GameState();
  if (state != addr::kGameStateCharSelect && state != addr::kGameStateCharCreate &&
      state != addr::kGameStateInGame) {
    // Zoning or logging in: try again shortly rather than resetting the device mid-load.
    static int last_logged_state = 0x7FFFFFFF;
    if (state != last_logged_state) {
      last_logged_state = state;
      Log("window_resize: game state %d, deferring", state);
    }
    ScheduleApply(hwnd);
    return;
  }
  void* handler = ResolutionHandler();
  if (!handler || Field<bool>(handler, addr::res_handler::kFullscreen)) return;

  RECT client;
  GetClientRect(g_hwnd, &client);
  int width = client.right - client.left;
  int height = client.bottom - client.top;
  if (width < kMinClientSize || height < kMinClientSize) return;

  int& handler_width = Field<int>(handler, addr::res_handler::kWidth);
  int& handler_height = Field<int>(handler, addr::res_handler::kHeight);
  int current_width = Global<int>(addr::kCurrentWidth);
  int current_height = Global<int>(addr::kCurrentHeight);
  if (width == current_width && height == current_height && width == handler_width &&
      height == handler_height)
    return;

  RECT window;
  GetWindowRect(g_hwnd, &window);
  Log("window_resize: client %dx%d, game %dx%d (handler %dx%d), state %d; applying", width,
      height, current_width, current_height, handler_width, handler_height, state);

  // ApplyResolution sizes the window from these, so they must describe the window as it is now.
  handler_width = width;
  handler_height = height;
  Field<int>(handler, addr::res_handler::kPosX) = window.left;
  Field<int>(handler, addr::res_handler::kPosY) = window.top;

  using ApplyResolutionFn = void(__fastcall*)(void* self, void* edx, int width, int height,
                                              int mode_flag);
  auto apply = reinterpret_cast<ApplyResolutionFn>(mem::Rebase(addr::kApplyResolution));
  int mode_flag = Global<int>(addr::kCurrentModeFlag) == 1 ? 1 : 0;
  g_applying = true;
  apply(handler, nullptr, width, height, mode_flag);
  g_applying = false;
  Log("window_resize: game now %dx%d", Global<int>(addr::kCurrentWidth),
      Global<int>(addr::kCurrentHeight));
}

// Window managers driving the resize (as under Wine) send a stream of WM_SIZE messages
// without WM_ENTERSIZEMOVE/WM_EXITSIZEMOVE, so wait until the size stops changing.
void ScheduleApply(HWND hwnd) {
  if (g_applying) return;
  SetTimer(hwnd, kApplyTimerId, kApplyDelayMs, nullptr);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_TIMER:
      if (wparam != kApplyTimerId) break;
      KillTimer(hwnd, kApplyTimerId);
      if (!g_in_size_move) ApplyClientSize(hwnd);
      return 0;
    case WM_ENTERSIZEMOVE:
      g_in_size_move = true;
      break;
    case WM_EXITSIZEMOVE:
      g_in_size_move = false;
      ScheduleApply(hwnd);
      break;
    case WM_SIZE: {
      LRESULT result = CallWindowProcA(g_original_wndproc, hwnd, msg, wparam, lparam);
      if (wparam != SIZE_MINIMIZED) ScheduleApply(hwnd);
      return result;
    }
    case WM_GETMINMAXINFO: {
      // The client caps the max track size at the current resolution.
      LRESULT result = CallWindowProcA(g_original_wndproc, hwnd, msg, wparam, lparam);
      if (g_config.unlock_max_window_size) {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMaxTrackSize.x = GetSystemMetrics(SM_CXVIRTUALSCREEN) + 64;
        info->ptMaxTrackSize.y = GetSystemMetrics(SM_CYVIRTUALSCREEN) + 64;
      }
      return result;
    }
    case WM_DESTROY:
      if (hwnd == g_hwnd) g_hwnd = nullptr;
      break;
  }
  return CallWindowProcA(g_original_wndproc, hwnd, msg, wparam, lparam);
}

bool IsMainWindowClass(LPCSTR class_name) {
  if (IS_INTRESOURCE(class_name)) return false;
  auto expected = reinterpret_cast<const char*>(mem::Rebase(addr::kMainWindowClassName));
  return class_name == expected || std::strcmp(class_name, expected) == 0;
}

HWND WINAPI CreateWindowExAHook(DWORD ex_style, LPCSTR class_name, LPCSTR window_name, DWORD style,
                                int x, int y, int width, int height, HWND parent, HMENU menu,
                                HINSTANCE instance, LPVOID param) {
  HWND hwnd = g_original_create_window(ex_style, class_name, window_name, style, x, y, width,
                                       height, parent, menu, instance, param);
  if (hwnd && !g_hwnd && IsMainWindowClass(class_name)) {
    g_hwnd = hwnd;
    g_original_wndproc = reinterpret_cast<WNDPROC>(
        SetWindowLongA(hwnd, GWL_WNDPROC, reinterpret_cast<LONG>(&WndProc)));
    Log("window_resize: subclassed main window %p (style %08lX)", hwnd, style);
  }
  return hwnd;
}

}  // namespace

void ApplyEqgame() {
  if (!g_config.resize_fix) return;
  auto expected = reinterpret_cast<const char*>(mem::Rebase(addr::kMainWindowClassName));
  if (std::strcmp(expected, "_EverQuestwndclass") != 0) {
    Log("window_resize: window class name not found, not installing");
    return;
  }
  uintptr_t slot = mem::Rebase(addr::kIatCreateWindowExA);
  g_original_create_window = *reinterpret_cast<CreateWindowExAFn*>(slot);
  mem::WriteValue(slot, reinterpret_cast<uintptr_t>(&CreateWindowExAHook));
  Log("window_resize: installed");
}

}  // namespace window_resize
