# Cross-compile 32-bit Windows binaries with zig (bundles clang, lld and mingw-w64 headers).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

get_filename_component(_tools "${CMAKE_CURRENT_LIST_DIR}/../tools" ABSOLUTE)
set(CMAKE_C_COMPILER "${_tools}/zig-cc")
set(CMAKE_CXX_COMPILER "${_tools}/zig-c++")
set(CMAKE_AR "${_tools}/zig-ar" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_tools}/zig-ranlib" CACHE FILEPATH "")
