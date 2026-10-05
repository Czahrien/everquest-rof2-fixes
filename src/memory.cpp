#include "memory.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "addresses.h"
#include "log.h"

namespace mem {

uintptr_t EqgameBase() { return reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)); }

uintptr_t Rebase(uint32_t preferred_address) {
  return preferred_address - addr::kEqgamePreferredBase + EqgameBase();
}

uint32_t PeTimestamp(uintptr_t module_base) {
  auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
  auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module_base + dos->e_lfanew);
  return nt->FileHeader.TimeDateStamp;
}

static void HexDump(char* out, size_t out_size, const uint8_t* bytes, size_t size) {
  out[0] = '\0';
  for (size_t i = 0; i < size && (i + 1) * 3 < out_size; ++i)
    std::snprintf(out + i * 3, out_size - i * 3, "%02X ", bytes[i]);
}

bool Verify(uintptr_t address, const void* expected, size_t size, const char* what) {
  if (std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0) return true;
  char want[128], got[128];
  HexDump(want, sizeof(want), static_cast<const uint8_t*>(expected), size);
  HexDump(got, sizeof(got), reinterpret_cast<const uint8_t*>(address), size);
  Log("%s: unexpected bytes at %08X, not patching", what, static_cast<unsigned>(address));
  Log("  expected: %s", want);
  Log("  found:    %s", got);
  return false;
}

void Write(uintptr_t address, const void* bytes, size_t size) {
  void* target = reinterpret_cast<void*>(address);
  DWORD old_protect;
  VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &old_protect);
  std::memcpy(target, bytes, size);
  VirtualProtect(target, size, old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), target, size);
}

void WriteJump(uintptr_t from, const void* to) {
  uint8_t jmp[5] = {0xE9};
  int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (from + 5));
  std::memcpy(jmp + 1, &rel, sizeof(rel));
  Write(from, jmp, sizeof(jmp));
}

}  // namespace mem
