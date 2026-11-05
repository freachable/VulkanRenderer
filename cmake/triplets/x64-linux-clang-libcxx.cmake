# Linux x64 with clang + libc++.
# Keeps the C++ standard library of all vcpkg dependencies in sync with the
# project (see the "linux" preset in CMakePresets.json). The compiler itself
# is taken from CC/CXX, which devenv.nix points at clang.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)

set(VCPKG_CXX_FLAGS "-stdlib=libc++")
set(VCPKG_LINKER_FLAGS "-stdlib=libc++")
