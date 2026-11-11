#pragma once

#include "graphics/Vulkan.hpp"
#include "map/MapData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

struct TerrainVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;   // Into the albedo texture.

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(TerrainVertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return
		{
			{
				{ .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(TerrainVertex, position) },
				{ .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(TerrainVertex, normal) },
				{ .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(TerrainVertex, uv) }
			}
		};
	}
};

// A square block of grid cells whose indices are contiguous, so it can be culled and drawn on its own.
struct TerrainChunk
{
	uint32_t  firstIndex = 0;
	uint32_t  indexCount = 0;
	glm::vec3 boundsMin{ 0.0f };
	glm::vec3 boundsMax{ 0.0f };
};

// One regular grid over the whole map, displaced by the heightmap. Triangles are counter-clockwise
// seen from above.
struct TerrainMesh
{
	std::vector<TerrainVertex> vertices;
	std::vector<uint32_t>      indices;
	std::vector<TerrainChunk>  chunks;
};

// maxVerticesPerSide caps the grid resolution (the heightmap is resampled if it is larger).
// chunkCells is the chunk edge length in grid cells.
TerrainMesh buildTerrainMesh(MapData const& map, uint32_t maxVerticesPerSide = 513, uint32_t chunkCells = 32);
