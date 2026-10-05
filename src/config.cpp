#include "config.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "log.h"

Config g_config;

static const char kDefaultIni[] =
    "; rof2-fixes settings. Delete this file to regenerate defaults.\n"
    "[Fixes]\n"
    "; Correct the CPU speed calibration that overflows on CPUs with a TSC above ~4.29 GHz\n"
    "; (most Ryzen and recent Intel), which makes the game run too fast.\n"
    "CpuSpeedFix=1\n"
    "\n"
    "; Ultrawide aspect ratio fix:\n"
    ";   Off     - original behavior (stretched at aspect ratios of 2.0 and wider)\n"
    ";   Correct - correct proportions, horizontal FOV unchanged (less visible vertically)\n"
    ";   HorPlus - correct proportions, extra horizontal FOV beyond HorPlusBaseAspect\n"
    "AspectMode=HorPlus\n"
    "; Aspect ratio up to which the FOV setting is used as-is (1.7778 = 16:9).\n"
    "HorPlusBaseAspect=1.7778\n"
    "\n"
    "; Re-apply the render resolution when the window is resized (e.g. by the Wine\n"
    "; window manager), instead of stretching the old backbuffer.\n"
    "ResizeFix=1\n"
    "; Allow the window to be resized larger than its current resolution.\n"
    "UnlockMaxWindowSize=1\n";

static bool ReadBool(const char* ini, const char* key, bool fallback) {
  return GetPrivateProfileIntA("Fixes", key, fallback ? 1 : 0, ini) != 0;
}

void LoadConfig(const char* ini_path) {
  if (GetFileAttributesA(ini_path) == INVALID_FILE_ATTRIBUTES) {
    if (FILE* f = std::fopen(ini_path, "w")) {
      std::fputs(kDefaultIni, f);
      std::fclose(f);
    }
  }

  g_config.cpu_speed_fix = ReadBool(ini_path, "CpuSpeedFix", g_config.cpu_speed_fix);
  g_config.resize_fix = ReadBool(ini_path, "ResizeFix", g_config.resize_fix);
  g_config.unlock_max_window_size =
      ReadBool(ini_path, "UnlockMaxWindowSize", g_config.unlock_max_window_size);

  char buf[64];
  GetPrivateProfileStringA("Fixes", "AspectMode", "HorPlus", buf, sizeof(buf), ini_path);
  if (_stricmp(buf, "Off") == 0)
    g_config.aspect_mode = AspectMode::Off;
  else if (_stricmp(buf, "Correct") == 0)
    g_config.aspect_mode = AspectMode::Correct;
  else
    g_config.aspect_mode = AspectMode::HorPlus;

  GetPrivateProfileStringA("Fixes", "HorPlusBaseAspect", "1.7778", buf, sizeof(buf), ini_path);
  float base = static_cast<float>(std::atof(buf));
  if (base >= 1.0f && base <= 4.0f) g_config.hor_plus_base_aspect = base;

  Log("config: CpuSpeedFix=%d AspectMode=%d HorPlusBaseAspect=%.4f ResizeFix=%d "
      "UnlockMaxWindowSize=%d",
      g_config.cpu_speed_fix, static_cast<int>(g_config.aspect_mode),
      g_config.hor_plus_base_aspect, g_config.resize_fix, g_config.unlock_max_window_size);
}
