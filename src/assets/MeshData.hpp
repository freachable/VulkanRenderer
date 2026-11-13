#pragma once

#include "graphics/Vertex.hpp"

#include <cstdint>
#include <vector>

// Mesh geometry in CPU memory: vertices plus a triangle list indexing into them.
// Holds no GPU resources; upload it with graphics/Mesh.
struct MeshData
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;
};
