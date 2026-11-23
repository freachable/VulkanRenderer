# Data flow (proposal)

> Sketch by P1: what data enters the project, who turns it into what, and what is handed to the next
> person, up to the final frame. Names marked *(planned)* do not exist yet and are proposals.
> P2 types are from `src/splat/SplatTypes.h` on `P2/Gaussian-Splatting-Compute-Core`.
> The per-frame GPU order is in [render-flow.md](render-flow.md); the map files are specified in
> [map-data-format.md](map-data-format.md).

## Overview

Boxes are functions or shaders (owner first), cylinders are files or downloaded data, arrows carry
the data named on them. Dashed boxes are planned, not written yet.

```mermaid
flowchart TD
    subgraph SRC["1. Sources (downloaded)"]
        OSM[("OpenStreetMap, Overpass API<br/>vector polygons/lines, WGS84")]
        DEM[("DEM GeoTIFF<br/>Copernicus GLO-30 / SRTM")]
        GS[("3DGS scenes<br/>.ply")]
    end

    subgraph OFF["2. Offline export (Python)"]
        EXP["P1: osm_export.py<br/>(or generate_sample_map.py)"]
    end

    subgraph DISK["Files on disk"]
        MAPDIR[("assets/maps/&lt;name&gt;/<br/>map.json, heightmap.png, albedo.png,<br/>buildings.json, foliage.json")]
        PLY[("assets/splats/*.ply")]
    end

    subgraph CPU["3-5. Load and build (CPU, once)"]
        LOAD["P1: loadMapData()"]
        TER["P1: buildTerrainMesh()"]
        BLD["P1: buildBuildingMesh()"]
        EXT["P1: extrudeBuildings()"]
        TMD["P1: terrainToMapMesh(), toMeshData()"]
        FOL["P3: foliage placement"]
        LSP["P3: loadSplats()"]
        GPUM["P4: Mesh, Texture"]
        UPL["P2: SplatComputePipeline (upload)"]
    end

    subgraph FRAME["6. Every frame"]
        CAM["P4: Camera"]
        CULL["P1: frustum culling (CPU, Frustum.hpp)"]
        PROJ["P2: project.slang (compute)"]
        SORT["P2: radix_sort.slang (compute)"]
        MESH["P1: terrain.slang, building.slang"]
        BB["P3: billboard shader"]
        PRES["P4: Renderer, present"]
    end

    OUT(["Final frame in the window"])

    OSM -->|"buildings, green, water, roads"| EXP
    DEM -->|"heights (optional)"| EXP
    EXP -->|"map format v1"| MAPDIR
    GS --> PLY

    MAPDIR -->|"map.json, then the 4 files it names"| LOAD
    LOAD -->|"MapData"| TER
    LOAD -->|"MapData"| BLD
    LOAD -->|"MapData.foliageAreas"| FOL
    LOAD -->|"MapData.albedoPath"| GPUM
    BLD -->|"BuildingMesh (footprints + heights)"| EXT
    TER -->|"TerrainMesh"| TMD
    EXT -->|"MapMesh"| TMD
    TMD -->|"MeshData"| GPUM

    PLY -->|".ply"| LSP
    FOL -->|"foliage positions"| LSP
    LSP -->|"Gaussian3D[] (world, Z-up)"| UPL

    CAM -->|"FrameUniforms (set 0)"| CULL
    CAM -->|"FrameUniforms"| PROJ
    CAM -->|"FrameUniforms"| MESH
    TER -.->|"chunk bounds"| CULL
    BLD -.->|"building bounds"| CULL
    CULL -->|"visible draws"| MESH
    GPUM -->|"vertex/index buffers, albedo image"| MESH
    UPL -->|"SSBO Gaussian3D"| PROJ
    PROJ -->|"depth keys"| SORT
    PROJ -->|"SSBO ProjectedSplat2D, DrawIndirectCommand"| BB
    SORT -->|"sorted indices"| BB
    MESH -->|"color + depth (opaque)"| BB
    BB -->|"color (alpha blended)"| PRES
    PRES --> OUT

    classDef planned stroke-dasharray: 5 5
    class FOL,LSP,BB,PLY planned
```

## Stage 1: sources

| Data | From | Format | Used for |
|---|---|---|---|
| Buildings, green areas, water, roads | OpenStreetMap via the Overpass API (`osmnx`) | Vector polygons / lines, WGS84 lon/lat, OSM tags (`height`, `building:levels`, `building:colour`) | Building footprints + heights, foliage areas, albedo |
| Terrain elevation | Copernicus GLO-30 or SRTM, e.g. from OpenTopography | GeoTIFF raster, meters | Heightmap (optional; flat without it) |
| Landmarks, foliage splats | 3DGS scenes, trained by us or public datasets *(to decide, P3)* | `.ply` with position, scale, rotation, opacity, SH colors | Splats |

## Stage 2: offline export (P1, Python)

| Step | Function (`tools/`) | Input | Output |
|---|---|---|---|
| Download | `osm_export.features()`, `ox.graph_from_bbox()`, `with_retries()` | `--bbox W S E N` | GeoDataFrames (WGS84) |
| Local frame | `ox.projection.project_gdf()`, `to_local()` | WGS84 | UTM meters, centered on the bbox: x east, y north |
| Buildings | `parse_height()`, `parse_color()`, `mapformat.make_building()` (`clean_polygon`, `triangulate`) | OSM polygons + tags | Footprint (CCW), triangles, height, color |
| Foliage | `mapformat.make_foliage_area()` | Parks, forests, grass | Polygon + kind |
| Albedo | PIL `ImageDraw` | Green, water, roads, buildings | `albedo.png`, row 0 = north |
| Heightmap | `rasterio.warp.reproject()`, `mapformat.write_heightmap_png()` | DEM GeoTIFF | `heightmap.png`, 16-bit, with `heightRange` |
| Write | `mapformat.write_map()` | All of the above | `map.json`, `buildings.json`, `foliage.json` |

`generate_sample_map.py` writes the same files from a synthetic city, without downloads.

**Hand-over:** the folder `assets/maps/<name>/` (format version 1). Readers: P1 (terrain, buildings),
P3 (`foliage.json` to place foliage splats), P4 (`map.size` for the camera range).

## Stage 3: load and build on the CPU (P1, C++)

| Function (`src/map/`) | Input | Output | Notes |
|---|---|---|---|
| `loadMapData()` | Map folder | `MapData` (size, `HeightField`, buildings, foliage, albedo path) | Rejects other format versions |
| `mapWorldToUv()`, `sampleTerrainHeight()` | `MapData`, world xy | uv, height | Shared helpers |
| `buildTerrainMesh()` | `MapData` | `TerrainMesh`: vertices (pos, normal, uv), indices, chunks with bounds | Grid up to 513², 32-cell chunks for culling |
| `buildBuildingMesh()` | `MapData` | `BuildingMesh`: footprint triangles + heights + wall mask, one draw per building | Input of the geometry shader |
| `extrudeBuildings()` | `BuildingMesh` | `MapMesh`: roofs + walls as triangles | CPU path, same rules as the geometry shader |
| `toMeshData()` | `MapMesh` | P4's `MeshData` (`Vertex{pos, color, texCoord}`, uint32 indices) | Hand-over to P4's `Mesh` |
| `writeMapObj()` | `MapMesh` | `.obj` + `.mtl` + albedo | Debug view in Blender / 3dviewer.net |

## Stage 4: splats on the CPU (P3, then P2) *(planned)*

| Function | Owner | Input | Output |
|---|---|---|---|
| `loadSplats()` *(planned)* | P3 | `.ply` | `std::vector<Gaussian3D>`: `posOpacity` (world, Z-up; opacity after sigmoid), `scale` (after exp), `rot` (unit quaternion), `color` (SH0) |
| Transform into the map *(planned)* | P3 | Splat positions + landmark placement (`landmarks.json`?) / foliage areas | Gaussians in world space |
| Upload | P2 | `Gaussian3D[]` | `SplatComputePipeline::inputGaussians` (SSBO, set 1 binding 0) |

## Stage 5: GPU resources (P4 framework)

| Class | Input | Output |
|---|---|---|
| `Mesh` | `MeshData` | Vertex + index buffer |
| Texture *(P4)* | `albedo.png` | Sampled image for the terrain shader |
| `Camera` | Window input | `FrameUniforms` / `UniformBufferObject`: model, view, proj, cameraPos, viewportParams (set 0) |

## Stage 6: every frame

| Step | Owner | Shader / function | Input | Output |
|---|---|---|---|---|
| Frustum culling | P1 | `Frustum.hpp` (CPU) | Chunk / building bounds, view-proj | Visible draws |
| Splat projection | P2 | `project.slang` (compute) | `Gaussian3D` SSBO, `FrameUniforms` | `ProjectedSplat2D` SSBO (NDC center + depth, axes, conic + opacity, color), `DrawIndirectCommand` |
| Sort | P2 | `radix_sort.slang` (compute) | View-space depth keys | Sorted indices (back to front) |
| Terrain | P1 | `mesh/terrain.slang` | Terrain buffers, albedo | Color + depth |
| Buildings | P1 | `mesh/building.slang` (vert, geom, frag) or CPU mesh | Building buffers | Color + depth |
| Splat billboards | P3 | billboard shader *(planned)* | `ProjectedSplat2D`, sorted indices, indirect args, depth buffer | Color, alpha-blended over the meshes |
| Present | P4 | `Renderer` | Swapchain image | **Final frame in the window** |

## Open points

1. Where the splat `.ply` files come from (own training or public scenes) and who converts them (P3?).
2. Landmark placement: a `landmarks.json` in the map folder (see map-data-format.md, open question 1)?
3. Splat orientation: trained scenes are usually Y-down or Y-up; they need a transform into Z-up world space.
