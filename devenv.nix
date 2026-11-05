{
  pkgs,
  lib,
  ...
}:
{
  packages = with pkgs; [
    cmake
    ninja
    pkg-config
    vcpkg

    # Compiler and base libraries
    stdenv.cc
    stdenv.cc.cc.lib

    # Build tools for vcpkg
    autoconf
    autoconf-archive
    automake
    libtool
    python3

    # Wayland dependencies
    wayland
    wayland-protocols
    wayland-scanner
    libxkbcommon
    libdecor

    # X11 dependencies for GLFW
    xorg.libxcb
    xorg.libXau
    xorg.libX11
    xorg.libXrandr
    xorg.libXinerama
    xorg.libXcursor
    xorg.libXi
    xorg.libXext
    xorg.xorgproto

    # Other system dependencies
    libffi
    dbus

    # Compiler (C++ standard library is selected in CMakePresets.json and
    # cmake/triplets/x64-linux-clang-libcxx.cmake)
    clang_21
    llvmPackages_21.libcxx
    llvmPackages_21.libcxx.dev
  ];

  env = {
    # vcpkg
    VCPKG_ROOT = "${pkgs.vcpkg}/share/vcpkg";

    # Picked up by vcpkg when building ports; the project itself gets the
    # compiler from the "linux" preset.
    CC = "${pkgs.clang_21}/bin/clang";
    CXX = "${pkgs.clang_21}/bin/clang++";

    # pkg-config search paths
    PKG_CONFIG_PATH = lib.makeSearchPath "lib/pkgconfig" [
      pkgs.wayland
      pkgs.wayland-protocols
      pkgs.libxkbcommon
      pkgs.libdecor
      pkgs.libffi

      pkgs.xorg.libxcb
      pkgs.xorg.libXau
      pkgs.xorg.libX11
      pkgs.xorg.libXrandr
      pkgs.xorg.libXinerama
      pkgs.xorg.libXcursor
      pkgs.xorg.libXi
      pkgs.xorg.libXext
      pkgs.xorg.xorgproto
    ];

    # Runtime library paths
    LD_LIBRARY_PATH = lib.makeLibraryPath [
      pkgs.stdenv.cc.cc.lib
      pkgs.vulkan-loader

      pkgs.wayland
      pkgs.libxkbcommon
      pkgs.libdecor
      pkgs.libffi

      pkgs.xorg.libxcb
      pkgs.xorg.libXau
      pkgs.xorg.libX11
      pkgs.xorg.libXrandr
      pkgs.xorg.libXinerama
      pkgs.xorg.libXcursor
      pkgs.xorg.libXi
      pkgs.xorg.libXext
    ];
  };

  scripts.build.exec = ''
    cmake --preset linux-debug
    cmake --build --preset linux-debug
  '';

  scripts.run.exec = ''
    ./build/linux-debug/bin/VulkanRenderer "$@"
  '';
}
