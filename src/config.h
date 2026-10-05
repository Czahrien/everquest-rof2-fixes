#pragma once

enum class AspectMode {
  Off,      // leave the client's behavior alone
  Correct,  // fix the stretching; horizontal FOV stays fixed (vertical shrinks on ultrawide)
  HorPlus,  // fix the stretching and widen horizontal FOV beyond hor_plus_base_aspect
};

struct Config {
  bool cpu_speed_fix = true;
  AspectMode aspect_mode = AspectMode::HorPlus;
  float hor_plus_base_aspect = 16.0f / 9.0f;
  bool extended_fps_slider = true;
  bool vsync = false;
  bool mouse_look_fix = true;
  float mouse_look_scale = 1.0f;
  bool mouse_look_confine = true;
  bool physics_rate_fix = true;
  int physics_rate = 60;
  bool resize_fix = true;
  bool unlock_max_window_size = true;
};

extern Config g_config;

// Reads rof2fixes.ini from the game directory, writing a commented default if it is missing.
void LoadConfig(const char* ini_path);
