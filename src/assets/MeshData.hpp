#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

// One mesh vertex. graphics/gpu/VertexLayout describes how it is fed to the vertex shader.
struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;
};

// Mesh geometry in CPU memory: vertices plus a triangle list indexing into them.
// Holds no GPU resources; upload it with graphics/gpu/Mesh.
struct MeshData
{
	std::vector<Vertex>   vertices;
	std::vector<uint32_t> indices;
};
