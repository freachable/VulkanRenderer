# VulkanRenderer

3D maps rendered with meshes and 3D Gaussian splats combined, in Vulkan, C++20 and Slang.
Team project for the TUM GPU-Pro lab (WiSe 2026/27).

## Build

Dependencies come from vcpkg (`vcpkg.json`) and are built on the first configure. Shaders are
compiled with `slangc` during the build.

**Windows:** Visual Studio 2022 or 2026 with the "Desktop development with C++" workload
(it includes CMake, Ninja and vcpkg). In an "x64 Native Tools Command Prompt":

```
set VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg
cmake --preset windows-debug
cmake --build --preset windows-debug
build\windows-debug\bin\VulkanRenderer.exe
```

`VCPKG_ROOT` is the vcpkg that comes with Visual Studio (adjust the path for VS 2022), or your own clone.

**Linux:** run `devenv shell`, then `build` and `run` (see `devenv.nix`).

## Repository layout

```
assets/       runtime data: shaders/, textures/, meshes/, maps/<name>/
src/          C++: platform/, scene/, assets/, graphics/{vulkan,gpu,renderer}/, map/
tests/        CPU checks (MapCheck)
tools/        offline Python scripts that produce map data
docs/         map-data-format.md
```

## World space

Z-up and right-handed: **x east, y north, z up**, in meters (ENU, as in GIS). The camera and the
map data both use it; splat data needs a transform into it.

## Map data

Maps live in `assets/maps/<name>/`. The format is described in [docs/map-data-format.md](docs/map-data-format.md).

```
python tools/generate_sample_map.py          # synthetic map, writes assets/maps/sample

pip install -r tools/requirements.txt        # real map from OpenStreetMap
python tools/osm_export.py --bbox W S E N --out assets/maps/<name> [--dem dem.tif]
```

`--bbox` is west south east north in degrees. Without `--dem` the terrain is flat. The public
Overpass server is sometimes busy; the script retries.

## Tests

`MapCheck` loads a map and checks the loader, the terrain mesh and the building mesh on the CPU
(no window or GPU):

```
ctest --test-dir build/windows-debug
build\windows-debug\bin\MapCheck.exe assets\maps\<name>
```

## P1: data pipeline and mesh engine

P1 code is tagged with comments such as `p1_001`: the number is the P1 task number from the
GitHub issue title (GitHub issue number = task number + 4). The first tag in a file also names
the GitHub issue, e.g. `p1_001 (GitHub #5)`.

| Task | Issue | What | Status |
|---|---|---|---|
| p1_001 | #5 | Map data format | Done (`docs/map-data-format.md`), Z-up |
| p1_002 | #6 | Sample map generator | Done (`tools/generate_sample_map.py`) |
| p1_003 | #7 | OSM export script | Done, tested on Munich's old town; DEM input not tested yet |
| p1_004 | #8 | One real map dataset | Area to be chosen with the team |
| p1_005 | #9 | Integrate with the framework | Waiting for geometry shader feature and multi-pipeline drawing |
| p1_006 | #10 | Map loader | Done, checked by MapCheck |
| p1_007 | #11 | Terrain mesh | Mesh and shader done, checked by MapCheck; not drawn yet (p1_005) |
| p1_008 | #12 | Building extrusion (geometry shader) | Mesh and shader done, checked by MapCheck; not drawn yet (p1_005) |
| p1_009 | #13 | Backface + frustum culling (CPU) | `src/map/Frustum.hpp` done; culling loop needs p1_005 |
| p1_010 to p1_014 | #14 to #18 | GPU culling, shadows, Hi-Z, mesh shaders, terrain LOD | Not started |
