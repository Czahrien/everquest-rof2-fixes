#pragma once

#include <cstddef>
#include <cstdint>

namespace mem {

// Base address eqgame.exe was actually loaded at.
uintptr_t EqgameBase();

// Converts an eqgame.exe address at its preferred base (0x400000) to the live address.
uintptr_t Rebase(uint32_t preferred_address);

uint32_t PeTimestamp(uintptr_t module_base);

// Compares `size` bytes at `address` with `expected`. Logs a hex dump of both on mismatch.
bool Verify(uintptr_t address, const void* expected, size_t size, const char* what);

// Writes bytes into code or read-only data, restoring the page protection afterwards.
void Write(uintptr_t address, const void* bytes, size_t size);

template <typename T>
void WriteValue(uintptr_t address, T value) {
  Write(address, &value, sizeof(value));
}

// Address of the import address table slot for `function` imported from `dll` by the
// module at `module_base`, or 0 if not found.
uintptr_t FindImportSlot(uintptr_t module_base, const char* dll, const char* function);

// Overwrites the 5 bytes at `from` with `jmp to` / `call to`.
void WriteJump(uintptr_t from, const void* to);
void WriteCall(uintptr_t from, const void* to);

}  // namespace mem
