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

uintptr_t FindImportSlot(uintptr_t module_base, const char* dll, const char* function) {
  auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_base);
  auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(module_base + dos->e_lfanew);
  const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if (!dir.VirtualAddress) return 0;
  for (auto* desc = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(module_base + dir.VirtualAddress);
       desc->Name; ++desc) {
    if (_stricmp(reinterpret_cast<const char*>(module_base + desc->Name), dll) != 0) continue;
    auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(module_base + desc->OriginalFirstThunk);
    auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(module_base + desc->FirstThunk);
    for (; names->u1.AddressOfData; ++names, ++slots) {
      if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
      auto* by_name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(module_base + names->u1.AddressOfData);
      if (std::strcmp(reinterpret_cast<const char*>(by_name->Name), function) == 0)
        return reinterpret_cast<uintptr_t>(&slots->u1.Function);
    }
  }
  return 0;
}

static void WriteRel32(uintptr_t from, const void* to, uint8_t opcode) {
  uint8_t insn[5] = {opcode};
  int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (from + 5));
  std::memcpy(insn + 1, &rel, sizeof(rel));
  Write(from, insn, sizeof(insn));
}

void WriteJump(uintptr_t from, const void* to) { WriteRel32(from, to, 0xE9); }
void WriteCall(uintptr_t from, const void* to) { WriteRel32(from, to, 0xE8); }

}  // namespace mem
