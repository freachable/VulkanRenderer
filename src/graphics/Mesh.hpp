#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "Buffer.hpp"
#include "assets/MeshData.hpp"

#include <cstdint>

// Indexed triangle geometry in device-local memory, laid out as Vertex at binding 0.
class Mesh
{
public:
	// Uploads data through a staging buffer; blocks until the copies are done.
	Mesh(VulkanContext const& context, vk::raii::CommandPool const& uploadCommandPool, MeshData const& data);

	Mesh(Mesh&&) noexcept = default;
	Mesh& operator=(Mesh&&) noexcept = default;
	Mesh(Mesh const&) = delete;
	Mesh& operator=(Mesh const&) = delete;

	// Binds the vertex buffer at binding 0 and the index buffer.
	void bind(vk::raii::CommandBuffer const& commandBuffer) const;

	// Records an indexed draw of the whole mesh. Call after bind(), with a pipeline and its descriptors bound.
	void draw(vk::raii::CommandBuffer const& commandBuffer, uint32_t instanceCount = 1) const;

	uint32_t getIndexCount() const { return indexCount; }

private:
	static constexpr vk::IndexType indexType = vk::IndexType::eUint32;

	Buffer   vertexBuffer;
	Buffer   indexBuffer;
	uint32_t indexCount;
};
