#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "Vertex.hpp"

#include <cstdint>
#include <span>

// A VkBuffer together with its own memory. Created through the named factories.
class Buffer
{
public:
	// Device-local buffers, filled through a staging buffer and a one-off copy on the graphics queue.
	static Buffer forVertices(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::span<Vertex const> vertices);
	static Buffer forIndices(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::span<uint16_t const> indices);

	// Host-visible and coherent, mapped for its whole lifetime (see getMapped()).
	static Buffer forUniform(VulkanContext const& context, vk::DeviceSize size);

	// Host-visible transfer source holding a copy of data.
	static Buffer forStaging(VulkanContext const& context, void const* data, vk::DeviceSize size);

	static uint32_t findMemoryType(VulkanContext const& context, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;
	Buffer(Buffer const&) = delete;
	Buffer& operator=(Buffer const&) = delete;

	vk::raii::Buffer const& getHandle() const { return buffer; }

	// Mapped memory of a uniform buffer; nullptr for device-local buffers.
	void* getMapped() const { return mapped; }

private:
	Buffer(VulkanContext const& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

	static Buffer createDeviceLocal(VulkanContext const& context, vk::raii::CommandPool const& commandPool,
	                                void const* data, vk::DeviceSize size, vk::BufferUsageFlags usage);

	static void copyBuffer(VulkanContext const& context, vk::raii::CommandPool const& commandPool,
	                       Buffer const& srcBuffer, Buffer const& dstBuffer, vk::DeviceSize size);

	vk::raii::DeviceMemory memory = nullptr;
	vk::raii::Buffer       buffer = nullptr;
	void*                  mapped = nullptr;
};
