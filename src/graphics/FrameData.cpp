#include "FrameData.hpp"

#include <utility>

namespace
{
	vk::raii::CommandBuffer allocateCommandBuffer(VulkanContext const& context, vk::raii::CommandPool const& commandPool)
	{
		vk::CommandBufferAllocateInfo allocInfo
		{
			.commandPool = commandPool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1
		};

		return std::move(vk::raii::CommandBuffers(context.getDevice(), allocInfo).front());
	}

	vk::raii::DescriptorSet allocateDescriptorSet(VulkanContext const& context,
	                                              vk::raii::DescriptorPool const& descriptorPool,
	                                              vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	                                              Buffer const& uniformBuffer,
	                                              vk::DeviceSize uniformBufferSize)
	{
		vk::DescriptorSetAllocateInfo allocInfo
		{
			.descriptorPool = descriptorPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &*descriptorSetLayout
		};

		vk::raii::DescriptorSet descriptorSet = std::move(context.getDevice().allocateDescriptorSets(allocInfo).front());

		vk::DescriptorBufferInfo bufferInfo
		{
			.buffer = uniformBuffer.getHandle(),
			.offset = 0,
			.range = uniformBufferSize
		};

		vk::WriteDescriptorSet descriptorWrite
		{
			.dstSet = descriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &bufferInfo
		};

		context.getDevice().updateDescriptorSets(descriptorWrite, {});
		return descriptorSet;
	}
}

FrameData::FrameData(VulkanContext const& context,
                     vk::raii::CommandPool const& commandPool,
                     vk::raii::DescriptorPool const& descriptorPool,
                     vk::raii::DescriptorSetLayout const& descriptorSetLayout,
                     vk::DeviceSize uniformBufferSize)
	: commandBuffer(allocateCommandBuffer(context, commandPool))
	, imageAvailable(context.getDevice(), vk::SemaphoreCreateInfo{})
	, inFlight(context.getDevice(), vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled })
	, uniformBuffer(Buffer::forUniform(context, uniformBufferSize))
	, descriptorSet(allocateDescriptorSet(context, descriptorPool, descriptorSetLayout, uniformBuffer, uniformBufferSize))
{
}
