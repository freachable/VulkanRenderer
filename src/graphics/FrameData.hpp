#pragma once

#include "Vulkan.hpp"
#include "VulkanContext.hpp"
#include "Buffer.hpp"

// The application's resources for one frame in flight: one of these per RenderContext frame slot,
// indexed by Frame::frameIndex. The command buffer and sync objects live in RenderContext.
//
// The descriptor set comes from a pool owned elsewhere, so a FrameData must be destroyed before that pool.
struct FrameData
{
	// The descriptor set layout must have a uniform buffer at binding 0 (pointed at uniformBuffer)
	// and a combined image sampler at binding 1 (pointed at textureImageView and textureSampler).
	FrameData(VulkanContext const& context,
	          vk::raii::DescriptorPool const& descriptorPool,
	          vk::raii::DescriptorSetLayout const& descriptorSetLayout,
	          vk::DeviceSize uniformBufferSize,
	          vk::raii::ImageView const& textureImageView,
	          vk::raii::Sampler const& textureSampler);

	FrameData(FrameData&&) noexcept = default;
	FrameData& operator=(FrameData&&) noexcept = default;
	FrameData(FrameData const&) = delete;
	FrameData& operator=(FrameData const&) = delete;

	Buffer                  uniformBuffer;
	vk::raii::DescriptorSet descriptorSet = nullptr;
};
