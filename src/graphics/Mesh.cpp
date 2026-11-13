#include "Mesh.hpp"

#include <stdexcept>

namespace
{
	MeshData const& validated(MeshData const& data)
	{
		// Zero-sized buffers are invalid in Vulkan.
		if (data.vertices.empty() || data.indices.empty())
		{
			throw std::runtime_error("cannot create a mesh without vertices or indices");
		}
		return data;
	}
}

Mesh::Mesh(VulkanContext const& context, vk::raii::CommandPool const& uploadCommandPool, MeshData const& data)
	: vertexBuffer(Buffer::forVertices(context, uploadCommandPool, validated(data).vertices))
	, indexBuffer(Buffer::forIndices(context, uploadCommandPool, data.indices))
	, indexCount(static_cast<uint32_t>(data.indices.size()))
{
}

void Mesh::bind(vk::raii::CommandBuffer const& commandBuffer) const
{
	commandBuffer.bindVertexBuffers(0, *vertexBuffer.getHandle(), { 0 });
	commandBuffer.bindIndexBuffer(*indexBuffer.getHandle(), 0, indexType);
}

void Mesh::draw(vk::raii::CommandBuffer const& commandBuffer, uint32_t instanceCount) const
{
	commandBuffer.drawIndexed(indexCount, instanceCount, 0, 0, 0);
}
