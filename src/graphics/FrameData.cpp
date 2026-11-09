#include "FrameData.hpp"

#include <array>
#include <utility>

namespace
{
	vk::raii::DescriptorSet allocateDescriptorSet(VulkanContext const& context,
	                                              vk::raii::DescriptorPool const& descriptorPool,
	                                              vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	                                              Buffer const& uniformBuffer,
	                                              vk::DeviceSize uniformBufferSize,
	                                              vk::raii::ImageView const& textureImageView,
	                                              vk::raii::Sampler const& textureSampler)
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

		vk::DescriptorImageInfo imageInfo
		{ 
			.sampler = textureSampler, 
			.imageView = textureImageView, 
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		std::array<vk::WriteDescriptorSet, 2> descriptorWrites{ 
			{ 
				{ 
					.dstSet = descriptorSet,
					.dstBinding = 0,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &bufferInfo 
				},
				{ 
					.dstSet = descriptorSet,
					.dstBinding = 1,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eCombinedImageSampler,
					.pImageInfo = &imageInfo 
				} 
			}
		};

		context.getDevice().updateDescriptorSets(descriptorWrites, {});
		return descriptorSet;
	}
}

FrameData::FrameData(VulkanContext const& context,
                     vk::raii::DescriptorPool const& descriptorPool,
                     vk::raii::DescriptorSetLayout const& descriptorSetLayout,
                     vk::DeviceSize uniformBufferSize,
                     vk::raii::ImageView const& textureImageView,
                     vk::raii::Sampler const& textureSampler)
	: uniformBuffer(Buffer::forUniform(context, uniformBufferSize))
	, descriptorSet(allocateDescriptorSet(context, descriptorPool, descriptorSetLayout, uniformBuffer, uniformBufferSize,
	                                      textureImageView, textureSampler))
{
}
