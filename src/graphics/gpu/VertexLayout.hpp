#pragma once

#include "assets/MeshData.hpp"
#include "graphics/vulkan/Vulkan.hpp"

#include <array>
#include <cstddef>

// How Vertex is fed to the vertex shader: one interleaved buffer at binding 0,
// with pos, color and texCoord at locations 0, 1 and 2.

inline vk::VertexInputBindingDescription getVertexBindingDescription()
{
	return
	{
		.binding   = 0,
		.stride    = sizeof(Vertex),
		.inputRate = vk::VertexInputRate::eVertex
	};
}

inline std::array<vk::VertexInputAttributeDescription, 3> getVertexAttributeDescriptions()
{
	return
	{
		{
			{ .location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos) },
			{ .location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color) },
			{ .location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat,    .offset = offsetof(Vertex, texCoord) }
		}
	};
}
