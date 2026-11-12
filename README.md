# VulkanRenderer

3D maps rendered with meshes and 3D Gaussian splats combined, in Vulkan, C++20 and Slang.
Team project for the TUM GPU-Pro lab (WiSe 2026/27).

## Build

Dependencies come from vcpkg (`vcpkg.json`). Shaders are compiled with `slangc` during the build.

**Windows:** Visual Studio 2022 or 2026 with the "Desktop development with C++" workload
(it includes CMake, Ninja and vcpkg). In an "x64 Native Tools Command Prompt":

```
cmake --preset windows-debug
cmake --build --preset windows-debug
```

**Linux:** run `devenv shell`, then `build` and `run` (see `devenv.nix`).

## Repository layout

See [docs/structure.md](docs/structure.md).

## Map data

Maps live in `assets/maps/<name>/`. The format is described in [docs/map-data-format.md](docs/map-data-format.md).

```
python tools/generate_sample_map.py          # synthetic map, writes assets/maps/sample

pip install -r tools/requirements.txt        # real map from OpenStreetMap
python tools/osm_export.py --bbox W S E N --out assets/maps/<name> [--dem dem.tif]
```

P1 code is tagged with comments such as `p1_001`: the number is the P1 task number from the
GitHub issue title, and each file header names the matching GitHub issue.
