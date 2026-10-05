#pragma once

#include <cstdint>

// Each fix has an eqgame.exe part (applied at startup) and/or an EQGraphicsDX9.dll part
// (applied right after the game loads that DLL). All of them verify the bytes they
// replace and skip themselves, with a log entry, if anything does not match.

namespace cpu_speed {
void ApplyGfx(uintptr_t gfx_base);
}

namespace aspect_ratio {
void ApplyEqgame();
void ApplyGfx(uintptr_t gfx_base);
}

namespace frame_limiter {
void ApplyEqgame();
}

namespace mouse_look {
void ApplyEqgame();
}

namespace physics_rate {
void ApplyEqgame();
}

namespace vsync {
void ApplyGfx(uintptr_t gfx_base);
}

namespace window_resize {
void ApplyEqgame();
}
