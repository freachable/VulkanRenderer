#include "Buffer.hpp"

#include <cstring>
#include <stdexcept>
#include <utility>
#include "Command.hpp"

Buffer Buffer::forVertices(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::span<Vertex const> vertices)
{
	return createDeviceLocal(context, commandPool, vertices.data(), vertices.size_bytes(), vk::BufferUsageFlagBits::eVertexBuffer);
}

Buffer Buffer::forIndices(VulkanContext const& context, vk::raii::CommandPool const& commandPool, std::span<uint16_t const> indices)
{
	return createDeviceLocal(context, commandPool, indices.data(), indices.size_bytes(), vk::BufferUsageFlagBits::eIndexBuffer);
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

Buffer Buffer::forStaging(VulkanContext const& context, void const* data, vk::DeviceSize size)
{
	Buffer stagingBuffer(
		context,
		size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	void* dataStaging = stagingBuffer.memory.mapMemory(0, size);
	std::memcpy(dataStaging, data, static_cast<size_t>(size));
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
		.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties)
	};

	memory = vk::raii::DeviceMemory(context.getDevice(), allocInfo);
	buffer.bindMemory(*memory, 0);
}

Buffer Buffer::createDeviceLocal(VulkanContext const& context, vk::raii::CommandPool const& commandPool,
                                 void const* data, vk::DeviceSize size, vk::BufferUsageFlags usage)
{
	Buffer stagingBuffer(
		context,
		size,
		vk::BufferUsageFlagBits::eTransferSrc,
		vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
	);

	void* dataStaging = stagingBuffer.memory.mapMemory(0, size);
	std::memcpy(dataStaging, data, static_cast<size_t>(size));
	stagingBuffer.memory.unmapMemory();

	Buffer deviceBuffer(context, size, usage | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

	copyBuffer(context, commandPool, stagingBuffer, deviceBuffer, size);
	return deviceBuffer;
}

void Buffer::copyBuffer(VulkanContext const& context, vk::raii::CommandPool const& commandPool,
                        Buffer const& srcBuffer, Buffer const& dstBuffer, vk::DeviceSize size)
{
	Command command(context, commandPool);
	vk::raii::CommandBuffer commandBuffer = command.beginSingleTimeCommands();

	commandBuffer.copyBuffer(*srcBuffer.buffer, *dstBuffer.buffer, vk::BufferCopy(0, 0, size));
	command.endSingleTimeCommands(std::move(commandBuffer));
}

uint32_t Buffer::findMemoryType(VulkanContext const& context, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
	vk::PhysicalDeviceMemoryProperties memProperties = context.getPhysicalDevice().getMemoryProperties();

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("failed to find suitable memory type!");
}
