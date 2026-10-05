// rof2-fixes: a dinput8.dll proxy that patches the RoF2 EverQuest client at load time.
//
// eqgame.exe statically imports dinput8.dll, so placing this DLL next to it gets us
// loaded before the game's own code runs. We forward DirectInput8Create to the system
// dinput8.dll and use DllMain to patch eqgame.exe, plus a LoadLibraryA hook to patch
// EQGraphicsDX9.dll the moment the game loads it.

#include <windows.h>
#include <unknwn.h>

#include <cstring>
#include <string>

#include "addresses.h"
#include "config.h"
#include "fixes.h"
#include "log.h"
#include "memory.h"

namespace {

using LoadLibraryAFn = HMODULE(WINAPI*)(LPCSTR);
LoadLibraryAFn g_original_load_library = nullptr;
bool g_gfx_patched = false;

void OnGraphicsDllLoaded(HMODULE module) {
  if (g_gfx_patched) return;
  g_gfx_patched = true;
  auto base = reinterpret_cast<uintptr_t>(module);
  uint32_t timestamp = mem::PeTimestamp(base);
  Log("EQGraphicsDX9.dll loaded at %08X (timestamp %08X)", static_cast<unsigned>(base),
      static_cast<unsigned>(timestamp));
  if (timestamp != addr::kGfxTimestamp) {
    Log("EQGraphicsDX9.dll is not the expected RoF2 build; graphics fixes skipped");
    return;
  }
  cpu_speed::ApplyGfx(base);
  aspect_ratio::ApplyGfx(base);
  vsync::ApplyGfx(base);
}

HMODULE WINAPI LoadLibraryAHook(LPCSTR name) {
  HMODULE module = g_original_load_library(name);
  if (module && !g_gfx_patched) {
    if (HMODULE gfx = GetModuleHandleA("EQGraphicsDX9.dll"); gfx == module)
      OnGraphicsDllLoaded(module);
  }
  return module;
}

std::string GameDirectory() {
  char path[MAX_PATH];
  DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
  std::string dir(path, len);
  size_t slash = dir.find_last_of("\\/");
  return slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
}

void Initialize() {
  std::string dir = GameDirectory();
  LogOpen((dir + "\\rof2fixes.log").c_str());
  Log("rof2-fixes starting in %s", dir.c_str());
  LoadConfig((dir + "\\rof2fixes.ini").c_str());

  uint32_t timestamp = mem::PeTimestamp(mem::EqgameBase());
  Log("eqgame.exe at %08X (timestamp %08X)", static_cast<unsigned>(mem::EqgameBase()),
      static_cast<unsigned>(timestamp));
  if (timestamp != addr::kEqgameTimestamp) {
    Log("this is not the supported RoF2 eqgame.exe; all fixes disabled");
    return;
  }

  aspect_ratio::ApplyEqgame();
  frame_limiter::ApplyEqgame();
  mouse_look::ApplyEqgame();
  mouse_confine::ApplyEqgame();
  physics_rate::ApplyEqgame();
  window_resize::ApplyEqgame();

  uintptr_t slot = mem::Rebase(addr::kIatLoadLibraryA);
  g_original_load_library = *reinterpret_cast<LoadLibraryAFn*>(slot);
  mem::WriteValue(slot, reinterpret_cast<uintptr_t>(&LoadLibraryAHook));
  if (HMODULE gfx = GetModuleHandleA("EQGraphicsDX9.dll")) OnGraphicsDllLoaded(gfx);
}

// ---- dinput8 forwarding ------------------------------------------------------

using DirectInput8CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, IUnknown*);

DirectInput8CreateFn RealDirectInput8Create() {
  static DirectInput8CreateFn real = [] {
    char path[MAX_PATH];
    UINT len = GetSystemDirectoryA(path, MAX_PATH);
    std::strncpy(path + len, "\\dinput8.dll", MAX_PATH - len);
    HMODULE module = LoadLibraryA(path);
    if (!module) {
      Log("failed to load system dinput8.dll from %s", path);
      return static_cast<DirectInput8CreateFn>(nullptr);
    }
    return reinterpret_cast<DirectInput8CreateFn>(GetProcAddress(module, "DirectInput8Create"));
  }();
  return real;
}

}  // namespace

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE instance, DWORD version, REFIID iid,
                                             LPVOID* out, IUnknown* outer) {
  DirectInput8CreateFn real = RealDirectInput8Create();
  if (!real) return E_FAIL;
  return real(instance, version, iid, out, outer);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    Initialize();
  } else if (reason == DLL_PROCESS_DETACH) {
    mouse_confine::Shutdown();  // never leave the cursor clipped after the game exits
  }
  return TRUE;
}
