// P2
#pragma once

#include "graphics/vulkan/Vulkan.hpp"
#include "graphics/vulkan/VulkanContext.hpp"

#include <cstring>
#include <vector>

struct StorageBuffer
{
	vk::raii::DeviceMemory memory = nullptr;
	vk::raii::Buffer       buffer = nullptr;
	vk::DeviceSize         size   = 0;

	static StorageBuffer createDeviceLocal(VulkanContext const& context, vk::DeviceSize byteSize, vk::BufferUsageFlags extraUsage = {})
	{
		StorageBuffer out;
		out.size = byteSize;

		vk::BufferCreateInfo info
		{
			.size        = byteSize,
			.usage       = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc | extraUsage,
			.sharingMode = vk::SharingMode::eExclusive
		};
		out.buffer = vk::raii::Buffer(context.getDevice(), info);

		vk::MemoryRequirements req = out.buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo alloc
		{
			.allocationSize  = req.size,
			.memoryTypeIndex = context.findMemoryType(req.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal)
		};
		out.memory = vk::raii::DeviceMemory(context.getDevice(), alloc);
		out.buffer.bindMemory(*out.memory, 0);
		return out;
	}

	template <typename T>
	static std::vector<T> readback(VulkanContext const& context, StorageBuffer const& src, size_t count)
	{
		vk::DeviceSize const bytes = sizeof(T) * count;

		vk::BufferCreateInfo stagingInfo
		{
			.size        = bytes,
			.usage       = vk::BufferUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive
		};
		vk::raii::Buffer stagingBuf(context.getDevice(), stagingInfo);

		vk::MemoryRequirements req = stagingBuf.getMemoryRequirements();
		vk::MemoryAllocateInfo alloc
		{
			.allocationSize  = req.size,
			.memoryTypeIndex = context.findMemoryType(req.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)
		};
		vk::raii::DeviceMemory stagingMem(context.getDevice(), alloc);
		stagingBuf.bindMemory(*stagingMem, 0);

		context.immediateSubmit([&](vk::raii::CommandBuffer const& cmd)
		{
			cmd.copyBuffer(*src.buffer, *stagingBuf, vk::BufferCopy{ .srcOffset = 0, .dstOffset = 0, .size = bytes });
		});

		std::vector<T> result(count);
		void* mapped = stagingMem.mapMemory(0, bytes);
		std::memcpy(result.data(), mapped, bytes);
		stagingMem.unmapMemory();
		return result;
	}
};

inline void computeMemoryBarrier(
	vk::raii::CommandBuffer const& cmd,
	vk::PipelineStageFlags2 srcStage,
	vk::AccessFlags2        srcAccess,
	vk::PipelineStageFlags2 dstStage,
	vk::AccessFlags2        dstAccess)
{
	vk::MemoryBarrier2 barrier
	{
		.srcStageMask  = srcStage,
		.srcAccessMask = srcAccess,
		.dstStageMask  = dstStage,
		.dstAccessMask = dstAccess
	};

	cmd.pipelineBarrier2(vk::DependencyInfo
	{
		.memoryBarrierCount = 1,
		.pMemoryBarriers    = &barrier
	});
}
