#include "Mesh.hpp"

#include <span>
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

Mesh::Mesh(VulkanContext const& context, MeshData const& data)
	: vertexBuffer(Buffer::forDeviceLocal(context, std::as_bytes(std::span(validated(data).vertices)), vk::BufferUsageFlagBits::eVertexBuffer))
	, indexBuffer(Buffer::forDeviceLocal(context, std::as_bytes(std::span(data.indices)), vk::BufferUsageFlagBits::eIndexBuffer))
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
