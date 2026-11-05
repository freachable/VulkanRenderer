#include "FrameData.hpp"

#include <utility>

namespace
{
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
                     vk::raii::DescriptorPool const& descriptorPool,
                     vk::raii::DescriptorSetLayout const& descriptorSetLayout,
                     vk::DeviceSize uniformBufferSize)
	: uniformBuffer(Buffer::forUniform(context, uniformBufferSize))
	, descriptorSet(allocateDescriptorSet(context, descriptorPool, descriptorSetLayout, uniformBuffer, uniformBufferSize))
{
}
