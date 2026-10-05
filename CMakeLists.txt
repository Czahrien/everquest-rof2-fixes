cmake_minimum_required(VERSION 3.20)
project(rof2-fixes CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "eqgame.exe is 32-bit; configure with a 32-bit Windows toolchain "
                      "(see cmake/zig-x86-windows.cmake or cmake/mingw-i686.cmake)")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(dinput8 SHARED
  src/dllmain.cpp
  src/config.cpp
  src/log.cpp
  src/memory.cpp
  src/fixes/aspect_ratio.cpp
  src/fixes/cpu_speed.cpp
  src/fixes/frame_limiter.cpp
  src/fixes/mouse_confine.cpp
  src/fixes/mouse_look.cpp
  src/fixes/physics_rate.cpp
  src/fixes/vsync.cpp
  src/fixes/window_resize.cpp
  src/dinput8.def
)
set_target_properties(dinput8 PROPERTIES PREFIX "" OUTPUT_NAME dinput8)
target_compile_definitions(dinput8 PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(dinput8 PRIVATE -Wall -Wextra -fno-exceptions)
# zig emits CodeView debug info by default, full of absolute paths; nothing uses it.
target_compile_options(dinput8 PRIVATE -g0)
target_link_libraries(dinput8 PRIVATE user32)
# Don't depend on libgcc/libstdc++ DLLs being present next to eqgame.exe.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(dinput8 PRIVATE -static -static-libgcc -static-libstdc++)
endif()
