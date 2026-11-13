#include "FrameResources.hpp"

#include <array>
#include <utility>
#include <vector>

namespace
{
	constexpr uint32_t uniformBufferBinding = 0;
	constexpr uint32_t textureBinding       = 1;

	std::array<vk::DescriptorSetLayoutBinding, 2> getBindings()
	{
		return
		{
			{
				{
					.binding         = uniformBufferBinding,
					.descriptorType  = vk::DescriptorType::eUniformBuffer,
					.descriptorCount = 1,
					.stageFlags      = vk::ShaderStageFlagBits::eVertex
				},
				{
					.binding         = textureBinding,
					.descriptorType  = vk::DescriptorType::eCombinedImageSampler,
					.descriptorCount = 1,
					.stageFlags      = vk::ShaderStageFlagBits::eFragment
				}
			}
		};
	}

	vk::raii::DescriptorSet allocateDescriptorSet(VulkanContext const& context,
	                                              vk::raii::DescriptorPool const& descriptorPool,
	                                              vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	                                              Buffer const& uniformBuffer,
	                                              vk::raii::ImageView const& textureView,
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
			.range = sizeof(UniformBufferObject)
		};

		vk::DescriptorImageInfo imageInfo
		{
			.sampler = textureSampler,
			.imageView = textureView,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		std::array<vk::WriteDescriptorSet, 2> descriptorWrites
		{
			{
				{
					.dstSet = descriptorSet,
					.dstBinding = uniformBufferBinding,
					.dstArrayElement = 0,
					.descriptorCount = 1,
					.descriptorType = vk::DescriptorType::eUniformBuffer,
					.pBufferInfo = &bufferInfo
				},
				{
					.dstSet = descriptorSet,
					.dstBinding = textureBinding,
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

vk::raii::DescriptorSetLayout FrameResources::createDescriptorSetLayout(VulkanContext const& context)
{
	auto const bindings = getBindings();

	vk::DescriptorSetLayoutCreateInfo layoutInfo
	{
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	return vk::raii::DescriptorSetLayout(context.getDevice(), layoutInfo);
}

vk::raii::DescriptorPool FrameResources::createDescriptorPool(VulkanContext const& context, uint32_t frameCount)
{
	// One set per frame, each holding every binding once.
	std::vector<vk::DescriptorPoolSize> poolSizes;
	for (vk::DescriptorSetLayoutBinding const& binding : getBindings())
	{
		poolSizes.push_back({ .type = binding.descriptorType, .descriptorCount = binding.descriptorCount * frameCount });
	}

	vk::DescriptorPoolCreateInfo poolInfo
	{
		.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		.maxSets = frameCount,
		.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		.pPoolSizes = poolSizes.data()
	};

	return vk::raii::DescriptorPool(context.getDevice(), poolInfo);
}

FrameResources::FrameResources(VulkanContext const& context,
                               vk::raii::DescriptorPool const& descriptorPool,
                               vk::raii::DescriptorSetLayout const& descriptorSetLayout,
                               vk::raii::ImageView const& textureView,
                               vk::raii::Sampler const& textureSampler)
	: uniformBuffer(Buffer::forUniform(context, sizeof(UniformBufferObject)))
	, descriptorSet(allocateDescriptorSet(context, descriptorPool, descriptorSetLayout, uniformBuffer, textureView, textureSampler))
{
}
