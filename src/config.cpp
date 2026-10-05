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
    "; Options > Advanced Display Max FPS slider goes to 199, with 200 = Unlimited, and\n"
    "; frames are paced precisely (the original limiter only manages whole milliseconds).\n"
    "ExtendedFpsSlider=1\n"
    "; Sync presentation to the monitor refresh (the client has no vsync option).\n"
    "; Set the Max FPS slider to Unlimited when using this.\n"
    "VSync=0\n"
    "\n"
    "; Make right-click mouse-look turn speed independent of frame rate.\n"
    "MouseLookFix=1\n"
    "; Turn speed multiplier. 1.0 matches the original feel at 30 fps.\n"
    "MouseLookScale=1.0\n"
    "; Keep the hidden cursor from wandering during mouse-look:\n"
    ";   Off        - original behavior\n"
    ";   Clip       - confine it to the game window (ClipCursor)\n"
    ";   Pin        - hold it where mouse-look started (SetCursorPos every frame)\n"
    ";   ClipAndPin - both (needed with several clients under Wine on Wayland)\n"
    "MouseLookConfine=ClipAndPin\n"
    "\n"
    "; Run the player's movement physics at a fixed PhysicsRate steps per second.\n"
    "; Without this, jump height, falling, fall damage and levitation change with\n"
    "; frame rate. The rate picks which frame rate's behavior you get everywhere.\n"
    "PhysicsRateFix=1\n"
    "PhysicsRate=60\n"
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
  g_config.extended_fps_slider = ReadBool(ini_path, "ExtendedFpsSlider", g_config.extended_fps_slider);
  g_config.vsync = ReadBool(ini_path, "VSync", g_config.vsync);
  g_config.mouse_look_fix = ReadBool(ini_path, "MouseLookFix", g_config.mouse_look_fix);
  g_config.physics_rate_fix = ReadBool(ini_path, "PhysicsRateFix", g_config.physics_rate_fix);
  int rate = GetPrivateProfileIntA("Fixes", "PhysicsRate", g_config.physics_rate, ini_path);
  if (rate >= 10 && rate <= 200) g_config.physics_rate = rate;
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

  GetPrivateProfileStringA("Fixes", "MouseLookConfine", "ClipAndPin", buf, sizeof(buf), ini_path);
  if (_stricmp(buf, "Off") == 0 || std::strcmp(buf, "0") == 0)
    g_config.mouse_look_confine = kConfineOff;
  else if (_stricmp(buf, "Clip") == 0)
    g_config.mouse_look_confine = kConfineClip;
  else if (_stricmp(buf, "Pin") == 0)
    g_config.mouse_look_confine = kConfinePin;
  else
    g_config.mouse_look_confine = kConfineClip | kConfinePin;  // "ClipAndPin", "1", or unrecognized

  GetPrivateProfileStringA("Fixes", "MouseLookScale", "1.0", buf, sizeof(buf), ini_path);
  float scale = static_cast<float>(std::atof(buf));
  if (scale > 0.0f && scale <= 20.0f) g_config.mouse_look_scale = scale;

  Log("config: CpuSpeedFix=%d AspectMode=%d HorPlusBaseAspect=%.4f ExtendedFpsSlider=%d VSync=%d MouseLookFix=%d "
      "MouseLookScale=%.2f MouseLookConfine=%d PhysicsRateFix=%d PhysicsRate=%d ResizeFix=%d UnlockMaxWindowSize=%d",
      g_config.cpu_speed_fix, static_cast<int>(g_config.aspect_mode),
      g_config.hor_plus_base_aspect, g_config.extended_fps_slider, g_config.vsync, g_config.mouse_look_fix, g_config.mouse_look_scale, g_config.mouse_look_confine,
      g_config.physics_rate_fix, g_config.physics_rate, g_config.resize_fix, g_config.unlock_max_window_size);
}
