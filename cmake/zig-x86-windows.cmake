# Cross-compile 32-bit Windows binaries with zig (bundles clang, lld and mingw-w64 headers).
# Works from Linux (bash wrappers) and Windows (.cmd wrappers) hosts.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86)

get_filename_component(_tools "${CMAKE_CURRENT_LIST_DIR}/../tools" ABSOLUTE)
if(CMAKE_HOST_WIN32)
  set(_suffix ".cmd")
else()
  set(_suffix "")
endif()
set(CMAKE_C_COMPILER "${_tools}/zig-cc${_suffix}")
set(CMAKE_CXX_COMPILER "${_tools}/zig-c++${_suffix}")
set(CMAKE_AR "${_tools}/zig-ar${_suffix}" CACHE FILEPATH "")
set(CMAKE_RANLIB "${_tools}/zig-ranlib${_suffix}" CACHE FILEPATH "")
