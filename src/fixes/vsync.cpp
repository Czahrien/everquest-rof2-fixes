// Optional vsync (VSync=1 in rof2fixes.ini).
//
// EQGraphicsDX9.dll always requests D3DPRESENT_INTERVAL_IMMEDIATE (no vsync) in its
// present parameters and has no setting for it. We wrap Direct3DCreate9 in the
// graphics DLL's imports and rewrite PresentationInterval in IDirect3D9::CreateDevice
// and IDirect3DDevice9::Reset, which every device creation and resolution change uses.

#include <windows.h>
#include <d3d9.h>

#include <cstdint>

#include "../config.h"
#include "../fixes.h"
#include "../log.h"
#include "../memory.h"

namespace vsync {
namespace {

// COM vtable slots.
constexpr int kD3D9CreateDeviceSlot = 16;
constexpr int kDeviceResetSlot = 16;

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);
using CreateDeviceFn = HRESULT(WINAPI*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                        D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
using ResetFn = HRESULT(WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

Direct3DCreate9Fn g_original_create9 = nullptr;
CreateDeviceFn g_original_create_device = nullptr;
ResetFn g_original_reset = nullptr;

void ApplyInterval(D3DPRESENT_PARAMETERS* params, const char* where) {
  if (!params) return;
  UINT wanted = D3DPRESENT_INTERVAL_ONE;
  if (params->PresentationInterval != wanted) {
    Log("vsync: %s present interval %08X -> %08X", where, params->PresentationInterval, wanted);
    params->PresentationInterval = wanted;
  }
}

// Replaces one entry of a COM object's vtable, returning the previous function.
template <typename Fn>
Fn PatchVtable(void* object, int slot, Fn replacement) {
  auto* vtable = *static_cast<uintptr_t**>(object);
  auto previous = reinterpret_cast<Fn>(vtable[slot]);
  if (previous != replacement)
    mem::WriteValue(reinterpret_cast<uintptr_t>(&vtable[slot]), reinterpret_cast<uintptr_t>(replacement));
  return previous;
}

HRESULT WINAPI ResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* params) {
  ApplyInterval(params, "Reset");
  return g_original_reset(device, params);
}

HRESULT WINAPI CreateDeviceHook(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, HWND window,
                                DWORD flags, D3DPRESENT_PARAMETERS* params,
                                IDirect3DDevice9** device) {
  ApplyInterval(params, "CreateDevice");
  HRESULT hr = g_original_create_device(d3d, adapter, type, window, flags, params, device);
  if (SUCCEEDED(hr) && device && *device) {
    // Devices share one vtable, so this only needs doing once.
    ResetFn previous = PatchVtable(*device, kDeviceResetSlot, &ResetHook);
    if (previous != &ResetHook) g_original_reset = previous;
  }
  return hr;
}

IDirect3D9* WINAPI Direct3DCreate9Hook(UINT sdk_version) {
  IDirect3D9* d3d = g_original_create9(sdk_version);
  if (d3d) {
    CreateDeviceFn previous = PatchVtable(d3d, kD3D9CreateDeviceSlot, &CreateDeviceHook);
    if (previous != &CreateDeviceHook) g_original_create_device = previous;
  }
  return d3d;
}

}  // namespace

void ApplyGfx(uintptr_t gfx_base) {
  if (!g_config.vsync) return;
  uintptr_t slot = mem::FindImportSlot(gfx_base, "d3d9.dll", "Direct3DCreate9");
  if (!slot) {
    Log("vsync: Direct3DCreate9 import not found, not installing");
    return;
  }
  g_original_create9 = *reinterpret_cast<Direct3DCreate9Fn*>(slot);
  mem::WriteValue(slot, reinterpret_cast<uintptr_t>(&Direct3DCreate9Hook));
  Log("vsync: installed");
}

}  // namespace vsync
