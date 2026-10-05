#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>

static FILE* g_log = nullptr;
static CRITICAL_SECTION g_lock;

void LogOpen(const char* path) {
  InitializeCriticalSection(&g_lock);
  g_log = std::fopen(path, "w");
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
