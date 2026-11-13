#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <cstddef>
#include <span>

// A VkBuffer together with its own memory. Created through the named factories.
class Buffer
{
public:
	// Device-local, filled with data through a staging buffer; blocks until the copy is done.
	// eTransferDst is added to usage.
	static Buffer forDeviceLocal(VulkanContext const& context, std::span<std::byte const> data, vk::BufferUsageFlags usage);

	// Host-visible and coherent, mapped for its whole lifetime (see getMapped()).
	static Buffer forUniform(VulkanContext const& context, vk::DeviceSize size);

	// Host-visible transfer source holding a copy of data.
	static Buffer forStaging(VulkanContext const& context, std::span<std::byte const> data);

	Buffer(Buffer&& other) noexcept;
	Buffer& operator=(Buffer&& other) noexcept;
	Buffer(Buffer const&) = delete;
	Buffer& operator=(Buffer const&) = delete;

	vk::raii::Buffer const& getHandle() const { return buffer; }

	// Mapped memory of a uniform buffer; nullptr for all other buffers.
	void* getMapped() const { return mapped; }

private:
	Buffer(VulkanContext const& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

	vk::raii::DeviceMemory memory = nullptr;
	vk::raii::Buffer       buffer = nullptr;
	void*                  mapped = nullptr;
};
