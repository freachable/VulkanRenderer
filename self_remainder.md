# self_remainder.md: handoff notes for Claude on Linux

> **For Claude only. Delete this file right after reading (阅后直接删除).**
> 1. Read it fully and save the key points into your own memory (user, feedback, project status).
> 2. Delete it: `git rm self_remainder.md`, commit, and push, so it never reaches `master`.
>
> Written 2026-11-11 by Claude Code on the user's Windows install (same PC, dual boot). Teammates can see this file on GitHub until it is deleted.

## The user

- TUM master student, course GPU-Pro (Computer Games Laboratory IN7106 / IN2106), WiSe 2026/27. The course rewards advanced GPU techniques, so the optional items (#10-#14) matter.
- Team role: **P1, Data Pipeline & Mesh Engine**.
- Reply to the user in **Chinese**. Anything for teammates (issues, comments, chat): **short English**, P1 scope only.
- Answer the user's direct question first, before continuing other work.
- Commit or push only when the user asks. Team rule: never decide shared architecture alone; propose it to the group.

## Team project

"3D Maps with 3DGS and meshes combined": Vulkan, C++20 (Vulkan-Hpp RAII, dynamic rendering), Slang shaders, splats drawn as hardware billboards. Repo: `git@github.com:freachable/VulkanRenderer.git`, main branch `master`.

| Role | Work |
|---|---|
| P1 (the user) | OSM/DEM data pipeline; terrain mesh, geometry-shader building extrusion, culling |
| P2 | Splat projection compute, alpha/frustum culling, radix sort (remote branch `Gaussian-Splatting-Compute-Core`) |
| P3 | Billboard raster, mesh + splat compositing, splat file integration (landmarks, foliage) |
| P4 | Vulkan framework, window, render loop, camera. Promised textures, depth buffer, multiple objects, maybe compute, and an API for the others |

## Repo state

- **`feature/p1-mesh-terrain-buildings`** (pushed): all P1 work. It only adds files, plus 5 lines in `CMakeLists.txt` (`src/map` sources, nlohmann-json) and `nlohmann-json` in `vcpkg.json`. The app still runs master's demo; the map code is compiled but not drawn.
  - `tools/`: `mapformat.py` (stdlib writer, ear clipping), `generate_sample_map.py` (run it without `python -I`), `osm_export.py` (osmnx 2.x, untested), `requirements.txt`
  - `data/sample/`: generated 1 km sample map (289 buildings, 17 foliage areas)
  - `docs/map-data-format.md`: format v1 draft with open questions. `docs/structure.md`: folder structure proposal (shared `assets/`, one owner per folder), not agreed yet
  - `src/map/`: `MapData` (loader, terrain height sampling), `TerrainMesh` (chunked grid), `BuildingMesh` (footprint triangles + wall mask), `Frustum.hpp`
  - `shaders/terrain.slang`, `shaders/building.slang` (geometry shader extrudes roof and walls)
- **`draft/p1-framework-needs`**: local only, on the Windows disk (`D:\codeLibrary\VulkanRenderer`). Framework changes needed to actually draw the map: DepthBuffer, GraphicsPipelineConfig, Renderer with CPU culling, placeholder orbit camera, one SPIR-V module per shader in CMake. Reference for P4 only, never merge. **The user wants to leave it alone for now.** If it is needed later, fetch it from the mounted Windows partition: `git fetch <windows-repo-path> draft/p1-framework-needs:draft/p1-framework-needs`.
- **None of the C++ or Slang code has been compiled yet.** Expect compile errors to fix.

## GitHub issues (label P1)

Created by hand on GitHub; the real numbers may differ from this plan.

1. Define map data format (`discussion`): draft done, not agreed yet
2. Sample map generator: done
3. OSM export script: written, untested
4. Preprocess one real map dataset (blocked by 3)
5. Integrate with framework API (blocked by P4)
6. Map loader: written
7. Terrain mesh: written
8. Building extrusion (geometry shader): written
9. Backface + frustum culling (CPU): written
10. GPU-driven frustum culling (optional)
11. Sun shadows (optional)
12. Hi-Z occlusion culling (optional)
13. Mesh shader building extrusion (optional)
14. Terrain LOD (optional)

## Next steps

1. **Linux build setup:** check the NVIDIA driver with `vulkaninfo --summary`; install Nix and devenv (the repo has `devenv.nix`; preset `linux-debug` uses clang 21 + libc++); run `devenv shell`, then `build` and `run`. Fix compile errors on this branch first.
2. **Issue #1:** post the comment below on the issue, and the group message with the structure proposal as PDF (regenerate it from `docs/structure.md`; the old PDF is only on the Windows disk). Probably not posted yet; ask the user.
3. **#3:** run `tools/osm_export.py` on a small area (`pip install -r tools/requirements.txt` in a venv), fix bugs. Main work of this week.
4. **#4:** pick the real map area with the team (ideally one with a landmark that has free splat files, for P3), get a DEM, export, commit.
5. Rendering (#5, #7-#9) waits for P4's API. After that, optional #10 and #11 first.

## Ready-to-post texts

Issue #1 comment (replace P3/P4 with real @usernames; adjust "Friday" if posting later):

```
Draft: docs/map-data-format.md on feature/p1-mesh-terrain-buildings
https://github.com/freachable/VulkanRenderer/blob/feature/p1-mesh-terrain-buildings/docs/map-data-format.md

Short version:
- A map is a folder: map.json, heightmap.png (16-bit), albedo.png, buildings.json, foliage.json
- World space: meters, x east, y up, z south, centered on the origin (sample map: 1 km x 1 km)
- Buildings: footprint polygon + height + color, extruded to walls and roof on the GPU
- Foliage: park/forest polygons in world (x, z)

Questions:
1. P3: should the map also place the landmark splats? Idea: landmarks.json with the splat file
   and its world transform; mesh buildings under a landmark get removed.
2. P3: are foliage polygons enough, or do you need single tree positions?
3. P4: OK for the camera (meters, y up)?

If there are no objections by Friday, I'll go with this.
```

Group message:

```
Hi! Two things to review, feedback by Friday would be great:
1. Map data format: please comment on issue #1, especially P3 (landmarks, foliage).
2. Folder structure proposal: see the attached PDF. Nothing outside my folders moves until we agree.
```
