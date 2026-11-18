#pragma once

#include "assets/MeshData.hpp"
#include "map/BuildingMesh.hpp"
#include "map/MapData.hpp"
#include "map/TerrainMesh.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

// p1_008 (GitHub #12): CPU path for drawing the map as plain indexed triangle meshes, without a
// geometry shader. Buildings are extruded on the CPU by the same rules as the geometry shader
// (assets/shaders/mesh/building.slang): a roof per footprint triangle plus one wall per outline edge.
// The GPU extrusion stays the goal of p1_008; this path lets the map be drawn and viewed now.

struct MapMeshVertex
{
	glm::vec3 position;  // World space, Z-up.
	glm::vec3 normal;
	glm::vec3 color;     // Linear RGB; white for the terrain.
	glm::vec2 uv;        // Into the map's albedo texture.
};

struct MapMesh
{
	std::vector<MapMeshVertex> vertices;
	std::vector<uint32_t>      indices;   // Triangle list, counter-clockwise seen from outside.
};

MapMesh terrainToMapMesh(TerrainMesh const& terrain);
MapMesh extrudeBuildings(MapData const& map, BuildingMesh const& buildings);

// For graphics/gpu/Mesh: drops the normals, keeps position, color and the albedo uv.
MeshData toMeshData(MapMesh const& mesh);

// Writes terrain and buildings as one Wavefront OBJ (+ .mtl, albedo copied next to it) for
// viewers such as Blender or MeshLab. OBJ is Y-up, so world (x, y, z) is written as (x, z, -y).
void writeMapObj(std::filesystem::path const& objPath, MapData const& map, MapMesh const& terrain, MapMesh const& buildings);
