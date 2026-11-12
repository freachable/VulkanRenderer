#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/glm.hpp>

// The preprocessed map, as written by tools/generate_sample_map.py or tools/osm_export.py.
// See docs/map-data-format.md.
//
// p1_001 (GitHub #5): world space as defined by the map data format.
// x points east, y up, z south, in meters. The map is centered on the origin and spans
// [-size.x / 2, size.x / 2] along x and [-size.y / 2, size.y / 2] along z. 2D points are (x, z).

struct HeightField
{
	uint32_t           width  = 0;
	uint32_t           height = 0;
	std::vector<float> heights;   // Meters, row-major, row 0 at the northern edge (z = -size.y / 2).
};

struct BuildingData
{
	std::vector<glm::vec2> footprint;  // Outline in (x, z), any orientation.
	std::vector<uint32_t>  triangles;  // Indices into footprint; empty means "convex, fan-triangulate".
	float                  height = 0.0f;
	glm::vec3              color{ 0.7f };  // Linear RGB (converted from the file's sRGB).
};

struct FoliageArea
{
	std::string            type;     // e.g. "park", "forest"
	std::vector<glm::vec2> polygon;  // Outline in (x, z)
	glm::vec2              boundsMin{ 0.0f };
	glm::vec2              boundsMax{ 0.0f };
};

struct MapData
{
	std::string           name;
	glm::vec2             size{ 0.0f };   // Extent in meters along x and z.
	HeightField           heightField;
	std::filesystem::path albedoPath;
	std::vector<BuildingData> buildings;
	std::vector<FoliageArea>  foliageAreas;
};

// Loads <directory>/map.json and the files it points to. Throws std::runtime_error on bad input.
MapData loadMapData(std::filesystem::path const& directory);

// p1_001: texture coordinates as defined in docs/map-data-format.md.
// Maps a world (x, z) position to [0, 1]^2 texture coordinates of the heightmap and albedo.
glm::vec2 mapWorldToUv(MapData const& map, glm::vec2 xz);

// Bilinearly interpolated terrain height in meters; positions outside the map are clamped to its edge.
float sampleTerrainHeight(MapData const& map, glm::vec2 xz);
