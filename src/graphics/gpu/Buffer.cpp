#include "Buffer.hpp"

#include <cstring>
#include <utility>

Buffer Buffer::forDeviceLocal(VulkanContext const& context, std::span<std::byte const> data, vk::BufferUsageFlags usage)
{
	Buffer stagingBuffer = forStaging(context, data);
	Buffer deviceBuffer(context, data.size_bytes(), usage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

	context.immediateSubmit([&](vk::raii::CommandBuffer const& commandBuffer)
	{
		commandBuffer.copyBuffer(*stagingBuffer.buffer, *deviceBuffer.buffer, vk::BufferCopy(0, 0, data.size_bytes()));
	});

	return deviceBuffer;
}

Buffer Buffer::forUniform(VulkanContext const& context, vk::DeviceSize size)
{
	Buffer uniformBuffer(
		context,
		size,
		vk::BufferUsageFlagBits::eUniformBuffer,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	uniformBuffer.mapped = uniformBuffer.memory.mapMemory(0, size);
	return uniformBuffer;
}

Buffer Buffer::forStaging(VulkanContext const& context, std::span<std::byte const> data)
{
	Buffer stagingBuffer(
		context,
		data.size_bytes(),
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	void* destination = stagingBuffer.memory.mapMemory(0, data.size_bytes());
	std::memcpy(destination, data.data(), data.size_bytes());
	stagingBuffer.memory.unmapMemory();

	return stagingBuffer;
}

Buffer::Buffer(Buffer&& other) noexcept
	: memory(std::move(other.memory))
	, buffer(std::move(other.buffer))
	, mapped(std::exchange(other.mapped, nullptr))
{
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
	if (this != &other)
	{
		// Release the old buffer before the memory it is bound to.
		buffer = std::move(other.buffer);
		memory = std::move(other.memory);
		mapped = std::exchange(other.mapped, nullptr);
	}
	return *this;
}

Buffer::Buffer(VulkanContext const& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
{
	vk::BufferCreateInfo bufferInfo
	{
		.size = size,
		.usage = usage,
		.sharingMode = vk::SharingMode::eExclusive
	};

	buffer = vk::raii::Buffer(context.getDevice(), bufferInfo);
	vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();

	vk::MemoryAllocateInfo allocInfo
	{
		.allocationSize = memRequirements.size,
		.memoryTypeIndex = context.findMemoryType(memRequirements.memoryTypeBits, properties)
	};

	memory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
	buffer.bindMemory(*memory, 0);
}
