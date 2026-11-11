#pragma once

#include "graphics/Vulkan.hpp"
#include "map/MapData.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// Input of the building geometry shader: the footprint triangles, flat on the ground. The geometry
// shader emits each triangle at roof height plus a wall quad for every edge on the outline.
// Triangles are not indexed, since the wall flag belongs to a triangle's edge, not to a shared vertex.
struct BuildingVertex
{
	glm::vec2 footprint;   // (x, z)
	float     baseHeight;  // World y of the wall bottoms (slightly below the terrain).
	float     roofHeight;  // World y of the roof.
	glm::vec3 color;       // Linear RGB
	float     wallMask;    // 1 if the edge from this vertex to the next one of its triangle is on the outline.

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return { .binding = 0, .stride = sizeof(BuildingVertex), .inputRate = vk::VertexInputRate::eVertex };
	}

	static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions()
	{
		return
		{
			{
				{ .location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(BuildingVertex, footprint) },
				{ .location = 1, .binding = 0, .format = vk::Format::eR32Sfloat,       .offset = offsetof(BuildingVertex, baseHeight) },
				{ .location = 2, .binding = 0, .format = vk::Format::eR32Sfloat,       .offset = offsetof(BuildingVertex, roofHeight) },
				{ .location = 3, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(BuildingVertex, color) },
				{ .location = 4, .binding = 0, .format = vk::Format::eR32Sfloat,       .offset = offsetof(BuildingVertex, wallMask) }
			}
		};
	}
};

// One building's vertex range and its extruded bounding box, for culling.
struct BuildingDraw
{
	uint32_t  firstVertex = 0;
	uint32_t  vertexCount = 0;
	glm::vec3 boundsMin{ 0.0f };
	glm::vec3 boundsMax{ 0.0f };
};

struct BuildingMesh
{
	std::vector<BuildingVertex> vertices;
	std::vector<BuildingDraw>   buildings;
};

// Places every building on the terrain and builds the geometry shader input.
// Buildings with fewer than 3 points, no valid triangles or a non-positive height are skipped.
BuildingMesh buildBuildingMesh(MapData const& map);
