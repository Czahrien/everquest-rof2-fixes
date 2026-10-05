#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <share.h>

static FILE* g_log = nullptr;
static CRITICAL_SECTION g_lock;

void LogOpen(const char* directory) {
  InitializeCriticalSection(&g_lock);
  char path[MAX_PATH];
  for (int instance = 1; instance <= 16 && !g_log; ++instance) {
    if (instance == 1)
      std::snprintf(path, sizeof(path), "%s\\rof2fixes.log", directory);
    else
      std::snprintf(path, sizeof(path), "%s\\rof2fixes.%d.log", directory, instance);
    // Deny other writers: a second instance fails here instead of truncating our log.
    g_log = _fsopen(path, "w", _SH_DENYWR);
  }
}

void Log(const char* fmt, ...) {
  if (!g_log) return;
  EnterCriticalSection(&g_lock);
  SYSTEMTIME t;
  GetLocalTime(&t);
  std::fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
  va_list args;
  va_start(args, fmt);
  std::vfprintf(g_log, fmt, args);
  va_end(args);
  std::fputc('\n', g_log);
  std::fflush(g_log);
  LeaveCriticalSection(&g_lock);
}
